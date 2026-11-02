#!/usr/bin/env python3
"""PreToolUse(Bash) guard: a feature, fix, tools, perf or chore branch is merged only after check-tdd passed on it.

The Principal Engineer merges branches into main after the Verification Engineer passes them. check-tdd
(scripts/tdd/check-tdd.py) is the mechanical check of the strict-TDD protocol (agents/STANDARDS.md section 3). Its
complete, all-PASS run leaves <git-common-dir>/check-tdd/pass/<sha>.json. This hook refuses `git merge <ref>` when ref
is a guarded branch and no such record exists for the exact sha. See merge_decision() for the override.
"""
import json
import os
import re
import shlex
import subprocess
import sys

SEPARATORS = r"\|\||&&|[;&|\n()]"
GUARDED = re.compile(r"^(?:refs/heads/|refs/remotes/origin/|origin/)?(?:feat|fix|tools|perf|chore)/")
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


def merge_targets(command, cwd):
    """[(directory, ref)] for every ref named by a `git merge` in the command, following `cd <dir> &&` and
    `git -C <dir>`. Merge options and their values are skipped, and --abort/--continue/--quit name nothing."""
    out, here = [], cwd
    for seg in split_segments(command):
        try:
            words = shlex.split(seg.strip())
        except ValueError:
            continue
        while words and re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", words[0]):
            words = words[1:]
        if not words:
            continue
        if words[0] == "cd" and len(words) > 1:
            here = _join(here, words[1])
            continue
        if words[0] != "git":
            continue
        d, j = here, 1
        while j < len(words) and words[j].startswith("-"):
            if words[j] == "-C" and j + 1 < len(words):
                d = _join(d, words[j + 1])
                j += 2
            else:
                j += 2 if words[j] in GIT_VALUE_OPTIONS else 1
        if j >= len(words) or words[j] != "merge":
            continue
        args = words[j + 1:]
        if ABORTING & set(args):
            continue
        k = 0
        while k < len(args):
            if args[k] in VALUE_OPTIONS:
                k += 2
            elif args[k].startswith("-"):
                k += 1
            else:
                out.append((d, args[k]))
                k += 1
    return out


def is_guarded(ref):
    return bool(GUARDED.match(ref))


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
    for d, ref in merge_targets(command, cwd):
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
        if "merge" not in command:
            return 0
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
