#!/usr/bin/env python3
"""PreToolUse(Bash) guard: a feature, fix, tools, perf or chore branch is merged only after check-tdd passed on it.

The Principal Engineer merges branches into main after the Verification Engineer passes them. check-tdd
(scripts/tdd/check-tdd.py) is the mechanical check of the strict-TDD protocol (agents/STANDARDS.md section 3). Its
complete, all-PASS run leaves <git-common-dir>/check-tdd/pass/<sha>.json. This hook refuses `git merge <ref>` when ref
is a guarded branch and no such record exists for the exact sha. See merge_decision() for the override.
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
WRAPPERS = {"env", "command", "exec", "time", "nohup", "sudo", "setsid", "builtin", "nice", "ionice", "timeout", "stdbuf"}
ABORTING = {"--abort", "--continue", "--quit"}
VALUE_OPTIONS = {"-m", "-F", "-s", "-X", "--message", "--file", "--strategy", "--strategy-option", "--into-name"}
GIT_VALUE_OPTIONS = {"-c", "--git-dir", "--work-tree", "--namespace", "--exec-path"}


def split_segments(command):
    """Split into simple commands on separators outside quotes (a `|` in `grep -E "a|git merge x"` is a pattern).
    Unbalanced quotes fall back to splitting everywhere."""
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


def _join(base, path):
    return os.path.normpath(os.path.join(base, os.path.expanduser(path)))


def _sibling(name):
    """Load a sibling hook file, for the substitution scanner both hooks share."""
    try:
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), name)
        spec = importlib.util.spec_from_file_location("sibling_" + re.sub(r"\W", "_", name), path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    except Exception as e:
        print("check-tdd hook: cannot load %s (%s); substitutions in commands are not followed" % (name, e), file=sys.stderr)
        return None


_MEMCAP = _sibling("require-memcap.py")


def substitutions(text):
    return _MEMCAP.substitutions(text) if _MEMCAP else []


def _words(seg):
    try:
        return shlex.split(seg.strip())
    except ValueError:
        return []


def _strip(words):
    """Drop what comes before the command: assignments, shell keywords, wrapper words and their options."""
    i = 0
    while i < len(words):
        w = words[i]
        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", w) or w in KEYWORDS:
            i += 1
        elif w.rsplit("/", 1)[-1] in WRAPPERS:
            i += 1
            while i < len(words) and (words[i].startswith("-") or re.match(r"^[0-9][0-9,.:smh-]*$", words[i])):
                i += 1
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


def merge_targets(command, cwd, depth=0):
    """[(directory, ref)] for every ref a `git merge` names, and [(directory, "<remote>/<branch>", "pull")] for a
    `git pull`, in the command and in what it runs: `cd <dir> &&`, `git -C <dir>`, `--git-dir`, wrapper words,
    `bash -c`/`eval` strings, `$( )` and backticks. --abort/--continue/--quit name nothing."""
    out, here = [], cwd
    if depth > 4:
        return out
    for inner in substitutions(command):
        out += merge_targets(inner, here, depth + 1)
    for seg in split_segments(command):
        words = _strip(_words(seg))
        if not words:
            continue
        name = words[0].rsplit("/", 1)[-1]
        if name == "cd" and len(words) > 1:
            here = _join(here, words[1])
        elif name in SHELLS or name == "eval":
            strings = [words[k + 1] for k, a in enumerate(words[1:-1], 1) if a.startswith("-") and not a.startswith("--") and "c" in a] if name in SHELLS else [" ".join(words[1:])]
            for s in strings:
                out += merge_targets(s, here, depth + 1)
        elif name == "git":
            d, j = _git_global_options(words, here)
            sub, args = (words[j], words[j + 1:]) if j < len(words) else ("", [])
            if sub == "merge" and not ABORTING & set(args):
                out += [(d, a) for a in _positionals(args)]
            elif sub == "pull":
                pos = _positionals(args)
                for spec in pos[1:]:
                    src = spec.split(":", 1)[0].lstrip("+")
                    if src:
                        out.append((d, src if pos[0] == "." else "%s/%s" % (pos[0], src), "pull"))
    return out


def is_guarded(ref):
    """A feat/, fix/, tools/, perf/ or chore/ branch, however it is spelled: bare, heads/..., refs/heads/...,
    <remote>/..., refs/remotes/<remote>/.... Not a tag."""
    return not ref.startswith("refs/tags/") and bool(GUARDED.match(ref))


def git_out(directory, *args):
    p = subprocess.run(["git", "-C", directory, *args], capture_output=True, text=True)
    return p.stdout.strip() if p.returncode == 0 else None


def valid_record(path, sha):
    try:
        with open(path) as f:
            rec = json.load(f)
    except (OSError, ValueError):
        return False
    return isinstance(rec, dict) and rec.get("result") == "pass" and rec.get("head") == sha


def merge_decision(command, cwd):
    """("deny", reason), ("overridden", note) or None. A guarded ref that resolves needs
    <git-common-dir>/check-tdd/pass/<sha>.json from a complete check-tdd run. A file
    <git-common-dir>/check-tdd/override/<sha> lets one merge through: only the Principal Engineer creates it,
    and only on the owner's explicit words. A ref that does not resolve is git's to refuse, not ours."""
    notes = []
    for target in merge_targets(command, cwd):
        d, ref = target[0], target[1]
        if not is_guarded(ref) or not os.path.isdir(d):
            continue
        sha = git_out(d, "rev-parse", "--verify", "-q", ref + "^{commit}")
        common = git_out(d, "rev-parse", "--path-format=absolute", "--git-common-dir")
        if not sha or not common:
            continue
        if valid_record(os.path.join(common, "check-tdd", "pass", sha + ".json"), sha):
            continue
        override = os.path.join(common, "check-tdd", "override", sha)
        if os.path.exists(override):
            notes.append("check-tdd overridden for %s (%s) by %s" % (ref, sha[:12], override))
            continue
        top = git_out(d, "rev-parse", "--show-toplevel") or d
        base = git_out(d, "merge-base", "HEAD", sha) or "main"
        return "deny", (
            "Refused: `git merge %s` (%s) has no check-tdd pass record. Run the checker over the branch, in the foreground "
            "under the cap, until every commit PASSes: systemd-run --user --scope -q -p MemoryMax=4G -p MemorySwapMax=0 "
            "-p RuntimeMaxSec=900 -- taskset -c <your cores> python3 scripts/tdd/check-tdd.py %s %s..%s  (add --budget "
            "and rerun if it stops incomplete; agents/STANDARDS.md section 3). Do not merge around it. The one exception "
            "is a file %s : only the Principal Engineer creates it, and only on the owner's explicit words."
            % (ref, sha[:12], top, base[:12], sha, override))
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
