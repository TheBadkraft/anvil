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


def main():
    return 0


if __name__ == "__main__":
    sys.exit(main())
