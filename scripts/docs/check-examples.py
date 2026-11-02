#!/usr/bin/env python3
"""check-examples: runs every fenced code sample in the documentation (FR-2611-anvl-docs-001).

    check-examples.py [--root DIR] [--dump anvil-dump] [--core PKG] [--js PKG] [--py PKG] [--net PKG]
                      [--strict] [--scratch DIR] [paths...]
"""
import re
import sys

MARKER = re.compile(r"^\s*<!--\s*example:\s*(.*?)\s*-->\s*$")
FENCE = re.compile(r"^(\s*)(`{3,}|~{3,})\s*([^\s`]*)")


class Sample:
    """One fenced code sample. `line` is the 1-based line of the opening fence (of the marker, for an orphan)."""

    def __init__(self, file, line, lang, text, markers, orphan=False):
        self.file, self.line, self.lang, self.text, self.markers, self.orphan = file, line, lang, text, markers, orphan


def parse_marker(body):
    parts = body.split()
    return (parts[0], parts[1:]) if parts else ("", [])


def extract_samples(path, text):
    """Every fenced sample of a markdown text, in order, with the `<!-- example: ... -->` markers right before it.

    A marker belongs to the next fence when only blank lines or other markers lie between them; a marker
    followed by anything else is an orphan, reported as a sample of its own.
    """
    lines = text.split("\n")
    out = []
    pending = []  # (line number, marker) not yet attached
    i = 0
    while i < len(lines):
        ln = lines[i]
        m = MARKER.match(ln)
        if m:
            pending.append((i + 1, parse_marker(m.group(1))))
            i += 1
            continue
        f = FENCE.match(ln)
        if f:
            indent, fence, lang = f.group(1), f.group(2), f.group(3)
            body = []
            j = i + 1
            while j < len(lines):
                c = lines[j]
                if c.strip().startswith(fence[0] * len(fence)) and not c.strip().strip(fence[0]):
                    break
                body.append(c[len(indent):] if c.startswith(indent) else c.lstrip())
                j += 1
            out.append(Sample(path, i + 1, lang, "\n".join(body) + "\n", [mk for _, mk in pending]))
            pending = []
            i = j + 1
            continue
        if ln.strip() == "":
            i += 1
            continue
        for pl, mk in pending:
            out.append(Sample(path, pl, "", "", [mk], orphan=True))
        pending = []
        i += 1
    for pl, mk in pending:
        out.append(Sample(path, pl, "", "", [mk], orphan=True))
    return out


def main(argv=None):
    print("check-examples: not implemented", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
