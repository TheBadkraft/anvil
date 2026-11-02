#!/usr/bin/env python3
"""check-examples: runs every fenced code sample in the documentation (FR-2611-anvl-docs-001). Stub."""
import sys


class Sample:
    def __init__(self, file, line, lang, text, markers, orphan=False):
        self.file, self.line, self.lang, self.text, self.markers, self.orphan = file, line, lang, text, markers, orphan


def extract_samples(path, text):
    return []


def main(argv=None):
    print("check-examples: not implemented", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
