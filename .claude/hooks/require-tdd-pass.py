#!/usr/bin/env python3
"""PreToolUse(Bash) guard (stub: compiles and returns defaults, so the tests can reach a genuine RED)."""
import sys


def split_segments(command):
    return [command]


def merge_targets(command, cwd):
    return []


def is_guarded(ref):
    return False


def main():
    return 0


if __name__ == "__main__":
    sys.exit(main())
