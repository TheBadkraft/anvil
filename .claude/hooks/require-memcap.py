#!/usr/bin/env python3
"""PreToolUse(Bash) guard: heavy runtimes must run under a systemd memory cap.

Agents run inside VS Code's process group. Twice (a 28 GB fuzzer, a 26 GB node profile) one
uncapped command was OOM-killed and took VS Code and every agent with it. This hook refuses a
Bash command that starts node, python3, npm, npx, make, dotnet or gradle (`gradle`, `gradlew`,
`./gradlew`) on a script or target, or runs a shell script (`bash x.sh`, `./x.sh`: heavy work hides in scripts), unless that very command is
wrapped by `systemd-run ... -p MemoryMax=...` (agents/STANDARDS.md section 8). Only a
`-p MemoryMax=` or `--property=MemoryMax=` counts, not any text that mentions it.
Small inline checks (node -e/-p, python3 -c, bash -n) and version queries stay allowed. A bare
`make` (no target, no option) is heavy: it runs the default target, which builds; `make -n`, `make -q`
and `make --version` stay allowed. A `MemoryMax=0`, `infinity`, `max` or empty value limits nothing and
is not a cap. `env -S "STRING"` and a command named by a `which`/`command -v` substitution are followed.
There is no allowlist of light scripts: run one under the cap, as the others.
It is a guardrail against a slip, not a security boundary: aliases, `eval "$x"` and `source`
are out of scope.
"""
import json, re, shlex, sys

HEAVY = {"node", "nodejs", "python", "python3", "npm", "npx", "make", "dotnet", "gradle", "gradlew"}
# gradle's -q, -p and -c are quiet, project dir and settings file, so only these leave it light.
GRADLE_SAFE_FLAGS = {"-v", "-V", "--version", "-h", "--help", "-?", "--stop", "--status"}
WRAPPERS = {"taskset", "nice", "ionice", "timeout", "env", "exec", "time", "command", "stdbuf", "sudo", "nohup", "setsid", "builtin", "chrt"}
SAFE_FLAGS = {"-", "-e", "-p", "-c", "--eval", "--print", "-v", "-V", "--version", "-h", "--help", "-n", "-q"}
# Options of a wrapper that take a value, and how many bare positionals it takes (timeout DURATION, taskset MASK,
# chrt PRIORITY): `sudo -u bob node x.js` runs node, not `bob`.
WRAPPER_VALUE_OPTIONS = {"sudo": {"-u", "-g", "-h", "-p", "-C", "-D", "-R", "-T", "-U", "-r", "-t"},
                         "timeout": {"-s", "-k", "--signal", "--kill-after"}, "env": {"-u", "-C", "-S", "--unset", "--chdir"},
                         "nice": {"-n"}, "ionice": {"-c", "-n", "-p", "-P", "-u"}, "taskset": {"-c"}, "stdbuf": {"-i", "-o", "-e"}}
WRAPPER_POSITIONALS = {"timeout": 1, "taskset": 1, "chrt": 1, "nice": 1}
POSITIONAL = re.compile(r"^(0x[0-9a-fA-F]+|[0-9][0-9,.:smhd-]*)$")
SYSTEMD_RUN_VALUE_OPTIONS = {"-p", "-E", "-u", "-G", "-M", "-H", "-t", "--property", "--setenv", "--unit", "--description",
                             "--slice", "--working-directory", "--uid", "--gid", "--nice", "--machine", "--host"}
SAFE_SHELL_FLAGS = {"-n", "--version", "--help", "-h", "-s"}
SHELL_VALUE_OPTIONS = {"-o", "-O", "+O", "--rcfile", "--init-file"}


SEPARATORS = r"\|\||&&|[;&|\n()]"


def split_segments(command):
    """Split into simple commands on shell separators that are outside quotes: a `|` in `grep -E "a|node x"` is a
    pattern, not a pipe. If the quotes do not balance (an apostrophe in a heredoc body, say), fall back to splitting
    everywhere, which can only make the hook stricter."""
    segs, cur, quote, i = [], [], None, 0
    while i < len(command):
        c = command[i]
        if quote:
            cur.append(c)
            if c == "\\" and quote == '"' and i + 1 < len(command):
                i += 1
                cur.append(command[i])
            elif c == quote:
                quote = None
        elif c in "'\"":
            quote = c
            cur.append(c)
        elif c == "\\" and i + 1 < len(command):
            cur.append(c)
            i += 1
            cur.append(command[i])
        else:
            m = re.match(SEPARATORS, command[i:])
            if m:
                segs.append("".join(cur))
                cur = []
                i += len(m.group(0))
                continue
            cur.append(c)
        i += 1
    if quote:
        return re.split(SEPARATORS, command)
    segs.append("".join(cur))
    return segs


ASSIGNMENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*=")
HEAVY_NAME = re.compile(r"^(node|nodejs|python[0-9.]*|npm|npx|make|dotnet|gradle|gradlew)$")
LOCATORS = {"which", "command", "type", "whereis", "readlink", "realpath"}
KEYWORDS = {"{", "}", "if", "then", "else", "elif", "fi", "do", "done", "while", "until", "!"}


def _close_paren(text, i):
    """Index of the `)` that closes a `$(` whose body starts at i (quotes and nested parens respected)."""
    depth, quote = 1, None
    while i < len(text):
        c = text[i]
        if quote:
            if c == "\\" and quote == '"':
                i += 1
            elif c == quote:
                quote = None
        elif c in "'\"":
            quote = c
        elif c == "\\":
            i += 1
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return len(text)


def substitution_spans(text):
    """[(start, end, body)] of every `$( )` and backtick substitution that the shell will run, `text[start:end]` being the
    whole substitution: outside quotes and inside double quotes, but not inside single quotes or after a backslash."""
    out, i, single, double = [], 0, False, False
    while i < len(text):
        c = text[i]
        if single:
            single = c != "'"
        elif c == "\\":
            i += 1
        elif c == "'" and not double:
            single = True
        elif c == '"':
            double = not double
        elif c == "$" and text.startswith("(", i + 1):
            end = _close_paren(text, i + 2)
            out.append((i, min(end + 1, len(text)), text[i + 2:end]))
            i = end
        elif c == "`":
            j = i + 1
            while j < len(text) and text[j] != "`":
                j += 2 if text[j] == "\\" else 1
            out.append((i, min(j + 1, len(text)), text[i + 1:j]))
            i = j
        i += 1
    return out


def substitutions(text):
    """Bodies of every `$( )` and backtick substitution that the shell will run."""
    return [body for _, _, body in substitution_spans(text)]


def locate_heavy(body):
    """The runtime a substitution body such as `which node`, `command -v python3` or `readlink -f $(which node)` locates,
    else None: `$(which node) x.js` runs node."""
    words = _words(name_located_commands(body).strip())
    if len(words) > 1 and words[0].rsplit("/", 1)[-1] in LOCATORS:
        for w in words[1:]:
            if not w.startswith("-") and HEAVY_NAME.match(w.rsplit("/", 1)[-1]):
                return w.rsplit("/", 1)[-1]
    return None


def name_located_commands(text):
    """The text with every substitution that locates a runtime replaced by that runtime's name, so that
    `$(which node) x.js` reads as `node x.js` (and `echo $(which node)` as `echo node`, which is harmless)."""
    out, last = [], 0
    for start, end, body in substitution_spans(text):
        name = locate_heavy(body)
        if name:
            out.append(text[last:start])
            out.append(name)
            last = end
    out.append(text[last:])
    return "".join(out)


def _words(seg):
    try:
        return shlex.split(seg, posix=True)
    except ValueError:
        return seg.split()


def skip_wrapper(words, i):
    """Index of the command after the wrapper at words[i] and its options (with their values)."""
    name = words[i].rsplit("/", 1)[-1]
    values, positional = WRAPPER_VALUE_OPTIONS.get(name, set()), WRAPPER_POSITIONALS.get(name, 0)
    i += 1
    while i < len(words):
        w = words[i]
        if w == "--":
            return i + 1
        if w.startswith("-") and len(w) > 1:
            i += 2 if w in values else 1
            if name == "taskset" and w == "-c":
                positional = 0  # the CPU list was the value; there is no mask
        elif positional and POSITIONAL.match(w):
            positional -= 1
            i += 1
        else:
            break
    return i


def systemd_run_command(words, i):
    """Index of the command a `systemd-run` at words[i] runs: after `--`, else after its options and their values."""
    j = i + 1
    while j < len(words) and words[j] != "--" and words[j].startswith("-"):
        j += 2 if words[j] in SYSTEMD_RUN_VALUE_OPTIONS else 1
    return j + 1 if j < len(words) and words[j] == "--" else j


def limits_memory(value):
    """A MemoryMax value that limits: not empty, `0`, `infinity` or `max`, which leave the memory unlimited (or nothing)."""
    value = value.strip().lower()
    if value in ("", "infinity", "max"):
        return False
    m = re.match(r"^([0-9]+(?:\.[0-9]+)?)[kmgtpe%]?b?$", value)
    return not (m and float(m.group(1)) == 0)


def caps_memory(options):
    """Only `-p MemoryMax=...`, `-pMemoryMax=...`, `--property MemoryMax=...` or `--property=MemoryMax=...` caps, and
    only with a value that limits (the last assignment wins, as in systemd): a description or any other text that
    mentions MemoryMax does not, nor does `MemoryMax=0` or `MemoryMax=infinity`."""
    last = None
    for k, w in enumerate(options):
        m = re.match(r"^(?:-p|--property=)MemoryMax=(.*)$", w)
        if m:
            last = m.group(1)
        elif w in ("-p", "--property") and k + 1 < len(options) and options[k + 1].startswith("MemoryMax="):
            last = options[k + 1][len("MemoryMax="):]
    return last is not None and limits_memory(last)


def runs_script(base, args):
    """A shell started on a script file (`bash x.sh`, `sh -e x.sh`) or a script run directly (`./x.sh`)."""
    if base in SHELLS:
        if SAFE_SHELL_FLAGS & set(args) or any(a.startswith("-") and not a.startswith("--") and "c" in a for a in args):
            return False
        k = 0
        while k < len(args):
            if args[k] in SHELL_VALUE_OPTIONS:
                k += 2
            elif args[k].startswith(("-", "+")):
                k += 1
            else:
                return True
        return False
    return base.endswith(".sh")


def env_split(words, i):
    """For `env [options] -S "STRING" [more]` at words[i]: the words of the command line that STRING and the rest make
    (assignments before -S are kept), else None."""
    j = i + 1
    while j < len(words) and (words[j].startswith("-") or ASSIGNMENT.match(words[j])):
        w = words[j]
        if w in ("-S", "--split-string") and j + 1 < len(words):
            return [x for x in words[i + 1:j] if ASSIGNMENT.match(x)] + _words(words[j + 1]) + words[j + 2:]
        if w.startswith("--split-string="):
            return [x for x in words[i + 1:j] if ASSIGNMENT.match(x)] + _words(w.split("=", 1)[1]) + words[j + 1:]
        if w.startswith("-S") and len(w) > 2:
            return [x for x in words[i + 1:j] if ASSIGNMENT.match(x)] + _words(w[2:]) + words[j + 1:]
        j += 2 if w in WRAPPER_VALUE_OPTIONS["env"] else 1
    return None


def simple_command(words, depth):
    """Yield (name, args) of the command a simple command runs, after assignments, keywords and wrappers.
    A `systemd-run ... -p MemoryMax=...` wrapper in this very command caps it: nothing is yielded."""
    i = 0
    while i < len(words):
        w = words[i]
        base = w.rsplit("/", 1)[-1]
        if ASSIGNMENT.match(w) or w in KEYWORDS:
            i += 1
        elif base == "systemd-run":
            end = systemd_run_command(words, i)
            if caps_memory(words[i + 1:end]):
                return
            i = end
        elif base == "env" and env_split(words, i) is not None:
            yield from simple_command(env_split(words, i), depth + 1)
            return
        elif base in WRAPPERS:
            i = skip_wrapper(words, i)
        else:
            args = words[i + 1:]
            yield base, args
            for inner in nested_commands(base, args):
                yield from commands(inner, depth + 1) if isinstance(inner, str) else simple_command(inner, depth + 1)
            return


XARGS_VALUE_OPTIONS = {"-n", "-I", "-L", "-P", "-d", "-E", "-s", "-a"}
SHELLS = {"bash", "sh", "zsh", "dash", "ksh"}
EXEC_OPTIONS = {"-exec", "-execdir", "-ok", "-okdir"}


def nested_commands(base, args):
    """Commands hidden in a command's arguments: a string for `bash -c`/`eval` (parsed as a command line) or a word
    list for what `xargs` and `find -exec` run."""
    if base in SHELLS:
        for k, a in enumerate(args[:-1]):
            if a.startswith("-") and not a.startswith("--") and "c" in a:
                return [args[k + 1]]
    elif base == "eval":
        return [" ".join(args)]
    elif base == "xargs":
        k = 0
        while k < len(args) and args[k].startswith("-") and args[k] != "--":
            k += 2 if args[k] in XARGS_VALUE_OPTIONS else 1
        sub = args[k + (1 if k < len(args) and args[k] == "--" else 0):]
        return [sub + ["<stdin>"]] if sub else []  # xargs adds its input as arguments
    elif base == "find":
        out = []
        for k, a in enumerate(args):
            if a in EXEC_OPTIONS:
                end = next((j for j in range(k + 1, len(args)) if args[j] in (";", "+")), len(args))
                out.append(args[k + 1:end])
        return out
    return []


def commands(command, depth=0):
    """Yield (name, args) for every command the shell would run, including the ones inside substitutions."""
    if depth > 4:
        return
    for inner in substitutions(command):
        yield from commands(inner, depth + 1)
    for seg in split_segments(name_located_commands(command)):
        yield from simple_command(_words(seg.strip()), depth)


def heavy_use(command):
    for base, args in commands(command):
        if HEAVY_NAME.match(base):
            safe = GRADLE_SAFE_FLAGS if base.startswith("gradle") else SAFE_FLAGS
            if (args and args[0] not in safe) or (not args and base == "make"):
                return base
        if runs_script(base, args):
            return base
    return None


def main():
    try:
        data = json.load(sys.stdin)
    except Exception:
        return 0
    command = (data.get("tool_input") or {}).get("command") or ""
    tool = heavy_use(command)
    if not tool:
        return 0
    reason = (
        f"Refused: `{tool}` runs without a memory cap. Agents share VS Code's process group, and an uncapped "
        "runaway kills VS Code and every agent (twice so far). Re-run it as: systemd-run --user --scope -q "
        "-p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=<s> -- <command>  (foreground; agents/STANDARDS.md "
        "section 8). Inline checks (node -e, python3 -c) and --version are allowed uncapped."
    )
    print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse",
                                             "permissionDecision": "deny",
                                             "permissionDecisionReason": reason}}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
