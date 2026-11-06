#!/usr/bin/env python3
"""PreToolUse(Bash) guard: a feature, fix, tools, perf or chore branch is merged only after check-tdd passed on it.

The Principal Engineer merges branches into main after the Verification Engineer passes them. check-tdd
(scripts/tdd/check-tdd.py) is the mechanical check of the strict-TDD protocol (agents/STANDARDS.md section 3). Its
complete, all-PASS run leaves <git-common-dir>/check-tdd/pass/<sha>.json. This hook refuses `git merge <ref>` when ref
is a guarded branch and no such record exists for the exact sha, made with the default options, whose base covers the
commits the merge brings in. See merge_decision() for the override.

This is a guardrail against a plausible slip by an agent, not a security boundary. It follows wrapper words, `bash -c`,
`sh -c` and `eval` strings, `$( )` and backticks, `git -C`, `--git-dir`, `cd dir &&`, every spelling of a branch, a sha
or tag that is the tip of a guarded branch, and `git pull <remote> <branch>` / `git pull . <branch>`.

Out of scope, on purpose:
  - a script FILE run by a shell (`bash m.sh`, `./m.sh`, `bash < m.sh`): the file can be written by the same command line
    or changed between the check and the run, so reading it when the hook runs proves nothing, and refusing every script
    run would refuse most work. The merge inside a script is the author's to check; the pass record is the control.
    (`xargs -a FILE git merge` and `xargs git merge < FILE` are different: the refs cannot be known, so they are refused
    as computed refs, like `$( )`.)
  - git aliases and shell aliases or functions;
  - `$'...'` strings and variable expansion of the subcommand or the ref (`git $cmd`, `git merge $b`);
  - cherry-pick, rebase and `reset --hard`, and `git fetch . x:main`, which move a branch without `git merge`;
  - a hand-forged pass record or override file. The records are advisory and trust-based: an agent that writes one
    has breached the protocol, and the breach is to be reported, not worked around.
"""
import importlib.util
import json
import os
import re
import shlex
import subprocess
import sys

SEPARATORS = r"\|\||&&|[;&|\n()]"
GUARDED = re.compile(r"^(?:refs/)?(?:heads/|remotes/[^/]+/|[^/]+/)?(?:feat|fix|tools|perf|chore)/")
SHELLS = {"bash", "sh", "zsh", "dash", "ksh"}
KEYWORDS = {"{", "}", "if", "then", "else", "elif", "fi", "do", "done", "while", "until", "!"}
ABORTING = {"--abort", "--continue", "--quit"}
VALUE_OPTIONS = {"-m", "-F", "-s", "-X", "--message", "--file", "--strategy", "--strategy-option", "--into-name"}
GIT_VALUE_OPTIONS = {"-c", "--git-dir", "--work-tree", "--namespace", "--exec-path"}


def split_segments(command):
    return [text for text, _ in split_with_separators(command)]


def split_with_separators(command):
    """[(simple command, the separator that came before it)], split on separators outside quotes (a `|` in
    `grep -E "a|git merge x"` is a pattern). Unbalanced quotes fall back to splitting everywhere."""
    segs, cur, quote, i, sep = [], [], None, 0, ";"
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
                segs.append(("".join(cur), sep))
                cur, sep = [], m.group(0)
                i += len(m.group(0))
                continue
            cur.append(c)
        i += 1
    if quote:
        return [(s, ";") for s in re.split(SEPARATORS, command)]
    segs.append(("".join(cur), sep))
    return segs


SUBST_MARK = "\x01"  # stands for a $( ) or backtick substitution in a command whose text is parsed


def _join(base, path):
    if SUBST_MARK in path:  # a computed directory: stay where we are
        return base
    return os.path.normpath(os.path.join(base, os.path.expanduser(path)))


def _sibling(name):
    """Load a sibling hook file, for the substitution scanner and wrapper tables both hooks share."""
    try:
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), name)
        spec = importlib.util.spec_from_file_location("sibling_" + re.sub(r"\W", "_", name), path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    except Exception as e:
        print("check-tdd hook: cannot load %s (%s); substitutions and wrapper options are not followed" % (name, e), file=sys.stderr)
        return None


_MEMCAP = _sibling("require-memcap.py")


def substitutions(text):
    return _MEMCAP.substitutions(text) if _MEMCAP else []


def _mask(text):
    """The text with every $( ) and backtick substitution replaced by SUBST_MARK, so that its body neither splits the
    command nor hides a computed ref. The bodies are parsed on their own, by merge_targets()."""
    if not _MEMCAP:
        return text
    out, i, single, double = [], 0, False, False
    while i < len(text):
        c = text[i]
        if single:
            single = c != "'"
            out.append(c)
        elif c == "\\":
            out.append(text[i:i + 2])
            i += 1
        elif c == "'" and not double:
            single = True
            out.append(c)
        elif c == '"':
            double = not double
            out.append(c)
        elif c == "$" and text.startswith("(", i + 1):
            out.append(SUBST_MARK)
            i = _MEMCAP._close_paren(text, i + 2)
        elif c == "`":
            out.append(SUBST_MARK)
            i += 1
            while i < len(text) and text[i] != "`":
                i += 2 if text[i] == "\\" else 1
        else:
            out.append(c)
        i += 1
    return "".join(out)


def _words(seg):
    try:
        return shlex.split(seg.strip())
    except ValueError:
        return []


def _strip(words):
    """Drop what comes before the command: assignments, shell keywords, wrapper words with their options, systemd-run."""
    i = 0
    while i < len(words) and _MEMCAP:
        w = words[i]
        base = w.rsplit("/", 1)[-1]
        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", w) or w in KEYWORDS:
            i += 1
        elif base == "systemd-run":
            i = _MEMCAP.systemd_run_command(words, i)
        elif base in _MEMCAP.WRAPPERS:
            i = _MEMCAP.skip_wrapper(words, i)
        else:
            break
    return words[i:]


def _git_global_options(words, here):
    """(directory, index of the subcommand) after git's global options (-C, --git-dir, --work-tree, -c ...)."""
    d, j = here, 1
    while j < len(words) and words[j].startswith("-"):
        w = words[j]
        if w == "-C" and j + 1 < len(words):
            d, j = _join(d, words[j + 1]), j + 2
        elif w.split("=", 1)[0] in ("--git-dir", "--work-tree"):
            value, j = (w.split("=", 1)[1], j + 1) if "=" in w else (words[j + 1] if j + 1 < len(words) else "", j + 2)
            d = _join(d, value if w.startswith("--work-tree") or os.path.basename(value.rstrip("/")) != ".git" else os.path.dirname(value.rstrip("/")))
        else:
            j += 2 if w in GIT_VALUE_OPTIONS else 1
    return d, j


def _positionals(args):
    out, k = [], 0
    while k < len(args):
        if args[k] in VALUE_OPTIONS:
            k += 2
        elif args[k].startswith("-"):
            k += 1
        else:
            out.append(args[k])
            k += 1
    return out


def _alias(sub, args, aliases):
    """`git branch N S`, `git tag N S`, `git checkout -b N S`, `git switch -c N S` make N a name for S later in the
    same command line, before N exists for the hook to resolve."""
    if sub in ("branch", "tag") and not {"-d", "-D", "--delete"} & set(args):
        pos = _positionals(args)
    elif sub in ("checkout", "switch") and {"-b", "-B", "-c", "-C"} & set(args):
        pos = [a for a in args if not a.startswith("-")]
    else:
        return
    if len(pos) >= 2:
        aliases[pos[0]] = pos[1]


def _git_targets(words, here, aliases, stdin_refs=()):
    """Targets of one `git ...` command (words[0] is git). `stdin_refs` are refs that xargs feeds a `git merge`."""
    out = []
    d, j = _git_global_options(words, here)
    sub, args = (words[j], words[j + 1:]) if j < len(words) else ("", [])
    if sub == "merge" and not ABORTING & set(args):
        for a in list(_positionals(args)) + list(stdin_refs):
            out.append((d, a, "unknown") if SUBST_MARK in a else (d, a))
            if a in aliases:
                out.append((d, aliases[a]))
    elif sub == "pull":
        pos = _positionals(args)
        for spec in pos[1:]:
            src = spec.split(":", 1)[0].lstrip("+")
            if SUBST_MARK in src:
                out.append((d, src, "unknown"))
            elif src:
                out.append((d, src if pos[0] == "." else "%s/%s" % (pos[0], src), "pull"))
    else:
        _alias(sub, args, aliases)
    return out


ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "a": "\a", "b": "\b", "f": "\f", "v": "\v", "e": "\x1b", "E": "\x1b",
           "\\": "\\", "'": "'", '"': '"'}


def _unescape(text):
    """printf / echo -e backslash escapes: \\n \\t ..., \\xHH, \\NNN and \\0NNN (octal), \\uHHHH. Anything else is kept."""
    out, i = [], 0
    while i < len(text):
        c = text[i]
        if c != "\\" or i + 1 >= len(text):
            out.append(c)
            i += 1
            continue
        d = text[i + 1]
        m = re.match(r"x([0-9a-fA-F]{1,2})|u([0-9a-fA-F]{1,4})|0?([0-7]{1,3})", text[i + 1:])
        if m and d != "0" or (m and d == "0"):
            out.append(chr(int(m.group(1) or m.group(2) or m.group(3), 16 if m.group(1) or m.group(2) else 8)))
            i += 1 + len(m.group(0))
        elif d in ESCAPES:
            out.append(ESCAPES[d])
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _printf(args):
    """The text `printf FORMAT ARG...` prints: escapes decoded, %s / %b / %d filled from the arguments, the format
    repeated while arguments are left over (as printf does)."""
    if args and args[0] == "--":
        args = args[1:]
    if not args:
        return ""
    fmt, rest, out = _unescape(args[0]), list(args[1:]), []
    while True:
        used, pos = False, 0
        for m in re.finditer(r"%(%|[-+ #0-9.*]*[a-zA-Z])", fmt):
            out.append(fmt[pos:m.start()])
            pos = m.end()
            if m.group(1) == "%":
                out.append("%")
            else:
                arg = rest.pop(0) if rest else ""
                out.append(_unescape(arg) if m.group(1).endswith("b") else arg)
                used = True
        out.append(fmt[pos:])
        if not (used and rest):
            return "".join(out)


def _echo(args):
    """The text `echo [-neE] ARG...` prints; -e decodes escapes."""
    escapes = False
    while args and re.match(r"^-[neE]+$", args[0]):
        escapes = escapes or ("e" in args[0] and args[0].rfind("e") > args[0].rfind("E"))
        args = args[1:]
    text = " ".join(args)
    return _unescape(text) if escapes else text


def _shell_strings(words, name, piped_from):
    """Command lines a shell (or eval) is given: `-c STRING`, a herestring, or the text piped in by echo/printf."""
    if name == "eval":
        return [" ".join(words[1:])]
    strings = [words[k + 1] for k, a in enumerate(words[1:-1], 1) if a.startswith("-") and not a.startswith("--") and "c" in a]
    for k, w in enumerate(words):
        if w.startswith("<<<"):
            strings.append(w[3:] or (words[k + 1] if k + 1 < len(words) else ""))
    bare = not strings and all(w.startswith("-") for w in words[1:])  # reads its commands from standard input
    if bare and piped_from and piped_from[0].rsplit("/", 1)[-1] in ("echo", "printf"):
        if piped_from[0].rsplit("/", 1)[-1] == "printf":
            strings.append(_printf(piped_from[1:]))
        else:
            strings.append(_echo(piped_from[1:]))

    return list(dict.fromkeys(strings))


def _xargs_git(words, here, aliases, piped_from):
    """`xargs [options] git merge ...`: the refs come from the pipe (echo/printf: its words; else unknown)."""
    k, value_options, from_file = 1, (_MEMCAP.XARGS_VALUE_OPTIONS if _MEMCAP else set()), False
    while k < len(words) and words[k].startswith("-") and words[k] != "--":
        w = words[k]
        from_file = from_file or w in ("-a", "--arg-file") or w.startswith("--arg-file=") or (w.startswith("-a") and not w.startswith("--"))
        k += 2 if w in value_options else 1
    sub = _strip(words[k + (1 if k < len(words) and words[k] == "--" else 0):])
    if not sub or sub[0].rsplit("/", 1)[-1] != "git":
        return []
    from_file = from_file or any(w == "<" or (w.startswith("<") and not w.startswith("<<")) for w in words)
    if from_file:
        stdin = [SUBST_MARK]  # the refs are in a file: unknown
    elif piped_from and piped_from[0].rsplit("/", 1)[-1] in ("echo", "printf"):
        stdin = [w for w in piped_from[1:] if not w.startswith("-")]
    else:
        stdin = [SUBST_MARK] if piped_from else []  # something other than echo feeds it: unknown refs
    return _git_targets(sub, here, aliases, stdin)


def merge_targets(command, cwd, depth=0, aliases=None):
    """[(directory, ref)] for every ref a `git merge` names, [(directory, "<remote>/<branch>", "pull")] for a
    `git pull`, and [(directory, text, "unknown")] for a ref that is computed ($( ) or backticks, or xargs input from
    something other than echo), in the command and in what it runs: `cd <dir> &&`, `git -C <dir>`, `--git-dir`,
    wrapper words and their options, `bash -c`/`eval` strings, a command line piped or herestring-ed into a shell,
    `xargs git merge`, substitutions. A branch made earlier in the command line (`git branch tmp feat/x; git merge
    tmp`) also yields its start. --abort/--continue/--quit name nothing."""
    out, here, prev = [], cwd, []
    aliases = {} if aliases is None else aliases
    if depth > 4:
        return out
    for inner in substitutions(command):
        out += merge_targets(inner, here, depth + 1, aliases)
    for seg, sep in split_with_separators(_mask(command)):
        words = _strip(_words(seg))
        piped_from, prev = (prev if sep == "|" else []), words
        if not words:
            continue
        name = words[0].rsplit("/", 1)[-1]
        if name == "cd" and len(words) > 1:
            here = _join(here, words[1])
        elif name in SHELLS or name == "eval":
            for s in _shell_strings(words, name, piped_from):
                out += merge_targets(s, here, depth + 1, aliases)
        elif name == "xargs":
            out += _xargs_git(words, here, aliases, piped_from)
        elif name == "git":
            out += _git_targets(words, here, aliases)
    return out


def is_guarded(ref):
    """A feat/, fix/, tools/, perf/ or chore/ branch, however it is spelled: bare, heads/..., refs/heads/...,
    <remote>/..., refs/remotes/<remote>/.... Not a tag."""
    return not ref.startswith("refs/tags/") and bool(GUARDED.match(ref))


def git_out(directory, *args):
    p = subprocess.run(["git", "-C", directory, *args], capture_output=True, text=True)
    return p.stdout.strip() if p.returncode == 0 else None


MAX_BATCH_LIMIT, MAX_GREEN_LINES = 10, 150  # check-tdd's defaults: a record made with looser options does not count


def record_problem(path, sha, directory):
    """None if the record at `path` proves a clean check that covers this merge, else the reason it does not."""
    try:
        with open(path) as f:
            rec = json.load(f)
    except OSError:
        return "missing"
    except ValueError:
        return "it is not readable"
    if not isinstance(rec, dict) or rec.get("result") != "pass" or rec.get("head") != sha:
        return "it is not a pass record for this sha"
    o = rec.get("options")
    if not isinstance(o, dict):
        return "it does not record the options of its run (an older checker made it)"
    loose = [k for k, bad in (("lenient_tests", o.get("lenient_tests") is not False), ("no_compat", o.get("no_compat") is not False),
                              ("first_parent", o.get("first_parent") is not False),
                              ("batch_limit", not isinstance(o.get("batch_limit"), int) or o["batch_limit"] > MAX_BATCH_LIMIT),
                              ("max_green_lines", not isinstance(o.get("max_green_lines"), int) or o["max_green_lines"] > MAX_GREEN_LINES)) if bad]
    if loose:
        return "options: it was made with loosened options (%s)" % ", ".join(loose)
    base, mb = rec.get("base"), git_out(directory, "merge-base", "HEAD", sha)
    if not base or not mb:
        return "it does not name its base, or HEAD shares no history with the branch"
    if git_out(directory, "merge-base", "--is-ancestor", base, mb) is None:
        return "it covers %s..%s, but this merge brings in %s..%s" % (base[:12], sha[:12], mb[:12], sha[:12])
    needs = detect_adapter(directory, base, sha)
    if needs and rec.get("adapter") != needs:
        return "its adapter (%s) is not the one this repo needs for the range (%s)" % (rec.get("adapter"), needs)
    return None


def detect_adapter(directory, base, sha):
    """The adapter check-tdd's detection gives for the range base..sha (the same rule as scripts/tdd/check-tdd.py)."""
    def has(path):
        return git_out(directory, "cat-file", "-e", "%s:%s" % (sha, path)) is not None
    if has("package.json"):
        return "node"
    changed = (git_out(directory, "diff", "--name-only", base, sha) or "").split()
    if changed and not any(f.startswith(("src/", "include/", "test/")) for f in changed) and any(f.startswith(("scripts/", ".claude/hooks/")) for f in changed):
        return "python"
    return "native" if has("test/unit") else None


def is_guarded_tip(directory, ref, sha):
    """A guarded branch name, or any name, sha or expression that resolves to the tip of a guarded branch (local or
    remote): `git branch tmp feat/x; git merge tmp` is a merge of feat/x. main, HEAD and release tips that are not also
    guarded tips are not."""
    if is_guarded(ref):
        return True
    return any(is_guarded(r) for r in (git_out(directory, "for-each-ref", "--points-at", sha, "--format=%(refname)") or "").split())


def merge_decision(command, cwd):
    """("deny", reason), ("overridden", note) or None. A merge or pull of a guarded branch needs
    <git-common-dir>/check-tdd/pass/<sha>.json from a complete check-tdd run with the default options that covers
    the commits the merge brings in. A file <git-common-dir>/check-tdd/override/<sha> lets one merge through: only
    the Principal Engineer creates it, and only on the owner's explicit words. A merge ref that does not resolve is
    git's to refuse; a pull of a guarded branch that was never fetched is refused, because its tip is unknown."""
    notes = []
    for target in merge_targets(command, cwd):
        d, ref, pull = target[0], target[1], len(target) == 3 and target[2] == "pull"
        if len(target) == 3 and target[2] == "unknown":
            return "deny", ("Refused: the ref of this `git merge` or `git pull` is computed ($( ) or backticks), so the guard cannot tell "
                            "which branch it is: pass the ref explicitly, as a branch name or a sha.")
        if not os.path.isdir(d):
            continue
        sha = git_out(d, "rev-parse", "--verify", "-q", ref + "^{commit}")
        common = git_out(d, "rev-parse", "--path-format=absolute", "--git-common-dir")
        if not common:
            continue
        if not sha:
            if pull and is_guarded(ref):
                return "deny", ("Refused: `git pull %s` brings in a guarded branch whose tip is not known here. Fetch it first "
                                "(git fetch), then run check-tdd over it and merge with `git merge`." % ref)
            continue
        if not is_guarded_tip(d, ref, sha):
            continue
        problem = record_problem(os.path.join(common, "check-tdd", "pass", sha + ".json"), sha, d)
        if problem is None:
            continue
        override = os.path.join(common, "check-tdd", "override", sha)
        if os.path.exists(override):
            notes.append("check-tdd overridden for %s (%s) by %s" % (ref, sha[:12], override))
            continue
        top = git_out(d, "rev-parse", "--show-toplevel") or d
        base = git_out(d, "merge-base", "HEAD", sha) or "main"
        why = "has no check-tdd pass record" if problem == "missing" else "has a check-tdd pass record that does not count (%s)" % problem
        return "deny", (
            "Refused: `git %s %s` (%s) %s. Run the checker over the branch, in the foreground "
            "under the cap, until every commit PASSes: systemd-run --user --scope -q -p MemoryMax=4G -p MemorySwapMax=0 "
            "-p RuntimeMaxSec=900 -- taskset -c <your cores> python3 scripts/tdd/check-tdd.py %s %s..%s  (add --budget "
            "and rerun if it stops incomplete; agents/STANDARDS.md section 3). Do not merge around it. The one exception "
            "is a file %s : only the Principal Engineer creates it, and only on the owner's explicit words."
            % ("pull" if pull else "merge", ref, sha[:12], why, top, base[:12], sha, override))
    if notes:
        return "overridden", "; ".join(notes)
    return None


def main():
    try:
        data = json.load(sys.stdin)
        command = (data.get("tool_input") or {}).get("command")
        if command is None:
            return 0
        if not isinstance(command, str):
            raise TypeError("command is %s" % type(command).__name__)
        res = merge_decision(command, data.get("cwd") or os.getcwd())
    except Exception as e:  # onFailure is `continue`: a broken guard must not block work
        print("check-tdd hook error (allowing): %s: %s" % (type(e).__name__, e), file=sys.stderr)
        return 0
    if res and res[0] == "deny":
        print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse", "permissionDecision": "deny",
                                                 "permissionDecisionReason": res[1]}}))
    elif res:
        print(res[1], file=sys.stderr)
        print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse", "additionalContext": res[1]}}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
