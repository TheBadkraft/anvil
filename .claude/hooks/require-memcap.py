#!/usr/bin/env python3
"""PreToolUse(Bash) guard: heavy runtimes must run under a systemd memory cap.

Agents run inside VS Code's process group. Twice (a 28 GB fuzzer, a 26 GB node profile) one
uncapped command was OOM-killed and took VS Code and every agent with it. This hook refuses a
Bash command that starts node, python3, npm, npx, make or dotnet on a script or target unless
the command line carries `systemd-run ... -p MemoryMax=...` (agents/STANDARDS.md section 8).
Small inline checks (node -e/-p, python3 -c) and version queries stay allowed.
"""
import json, re, shlex, sys

HEAVY = {"node", "nodejs", "python", "python3", "npm", "npx", "make", "dotnet"}
WRAPPERS = {"taskset", "nice", "ionice", "timeout", "env", "exec", "time", "command", "stdbuf", "sudo", "nohup", "setsid", "builtin"}
SAFE_FLAGS = {"-", "-e", "-p", "-c", "--eval", "--print", "-v", "-V", "--version", "-h", "--help", "-n", "-q"}


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
HEAVY_NAME = re.compile(r"^(node|nodejs|python[0-9.]*|npm|npx|make|dotnet)$")
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


def substitutions(text):
    """Bodies of every `$( )` and backtick substitution that the shell will run: outside quotes and inside double
    quotes, but not inside single quotes or after a backslash."""
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
            out.append(text[i + 2:end])
            i = end
        elif c == "`":
            j = i + 1
            while j < len(text) and text[j] != "`":
                j += 2 if text[j] == "\\" else 1
            out.append(text[i + 1:j])
            i = j
        i += 1
    return out


def _words(seg):
    try:
        return shlex.split(seg, posix=True)
    except ValueError:
        return seg.split()


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
            rest = words[i + 1:]
            own = rest[:rest.index("--")] if "--" in rest else rest
            if any("MemoryMax" in x for x in own):
                return
            i += 1 + (len(own) + 1 if "--" in rest else len(rest))
        elif base in WRAPPERS:
            i += 1
            # skip the wrapper's own options and numeric args (taskset -c 6,7 / timeout 60)
            while i < len(words) and (words[i].startswith("-") or re.match(r"^[0-9][0-9,.:smh-]*$", words[i])):
                i += 1
        else:
            yield base, words[i + 1:]
            return


def commands(command, depth=0):
    """Yield (name, args) for every command the shell would run, including the ones inside substitutions."""
    if depth > 4:
        return
    for inner in substitutions(command):
        yield from commands(inner, depth + 1)
    for seg in split_segments(command):
        yield from simple_command(_words(seg.strip()), depth)


def heavy_use(command):
    for base, args in commands(command):
        if HEAVY_NAME.match(base) and args and args[0] not in SAFE_FLAGS:
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
