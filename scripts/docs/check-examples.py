#!/usr/bin/env python3
"""check-examples: runs every fenced code sample in the documentation (FR-2611-anvl-docs-001).

    check-examples.py [--root DIR] [--dump anvil-dump] [--core PKG] [--js PKG] [--py PKG] [--net PKG]
                      [--strict] [--scratch DIR] [paths...]
"""
import argparse
import os
import re
import shutil
import sys
import tempfile

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



# ---------------------------------------------------------------------------------------------
# results, context and helpers
# ---------------------------------------------------------------------------------------------
class Result:
    """status is PASS, FAIL or SKIP. `needs` marks a SKIP caused by a missing package or toolchain
    (--strict turns it into a failure). `stdout` is what a run printed, for a following output block."""

    def __init__(self, status, reason="", needs=False, stdout=None):
        self.status, self.reason, self.needs, self.stdout = status, reason, needs, stdout


def passed(stdout=None):
    return Result("PASS", stdout=stdout)


def failed(reason):
    return Result("FAIL", reason)


def skipped(reason, needs=False):
    return Result("SKIP", reason, needs=needs)


KNOWN_MARKERS = {"fragment": (0, 0), "output": (0, 0), "error": (1, 1), "setup": (1, 1), "uses": (1, None)}
DOC_LANGS = {"anvl", "aml", "amp", ""}
JS_LANGS = {"js", "javascript", "mjs", "node"}


class Context:
    def __init__(self, scratch, dump=None, js=None):
        self.scratch, self.dump, self.js = scratch, dump, js


def check_document(s, text, files, ctx, expected):
    if not ctx.dump:
        return skipped("no --dump (an anvil-dump built from the native tag)", needs=True)
    if not ctx.js:
        return skipped("no --js (the anvil.js package)", needs=True)
    return failed("not implemented: documents")


# ---------------------------------------------------------------------------------------------
# one file
# ---------------------------------------------------------------------------------------------
def classify(lang):
    l = lang.lower()
    if l in DOC_LANGS:
        return "doc"
    if l in JS_LANGS:
        return "js"
    return "other"


HANDLERS = {}


def check_markers(s):
    """Returns (markers dict, error text). A marker with an unknown name or the wrong arguments is an error."""
    found = {}
    for kind, args in s.markers:
        if kind not in KNOWN_MARKERS:
            return None, "unknown example marker '%s' (known: %s)" % (" ".join([kind] + args), ", ".join(sorted(KNOWN_MARKERS)))
        lo, hi = KNOWN_MARKERS[kind]
        if len(args) < lo or (hi is not None and len(args) > hi) or (kind == "error" and not args[0].isdigit()):
            return None, "bad example marker '%s'" % " ".join([kind] + args)
        found.setdefault(kind, []).extend(args)
    return found, None


def check_sample(s, state, ctx):
    if s.orphan:
        return failed("example marker '%s' is not followed by a code fence" % " ".join([s.markers[0][0]] + s.markers[0][1]))
    marks, err = check_markers(s)
    if err:
        return failed(err)
    if "fragment" in marks:
        return skipped("marked fragment: not run")
    kind = classify(s.lang)
    handler = HANDLERS.get(kind)
    if handler is None:
        return failed("not implemented: %s samples" % kind)
    expected = int(marks["error"][0]) if "error" in marks else None
    return handler(s, s.text, {}, ctx, expected)


def check_file(path, rel, ctx):
    with open(path, encoding="utf-8") as f:
        samples = extract_samples(rel, f.read())
    state = {"setups": {}, "stdout": None}
    out = []
    for s in samples:
        res = check_sample(s, state, ctx)
        out.append((s, res))
    return out


HANDLERS["doc"] = lambda s, text, files, ctx, expected: check_document(s, text, files, ctx, expected)


def find_markdown(root, paths):
    targets = [os.path.join(root, "docs"), os.path.join(root, "site", "assets", "docs")] if not paths else paths
    found = []
    for t in targets:
        if os.path.isfile(t):
            found.append(t)
            continue
        for dp, dn, fn in os.walk(t):
            dn[:] = sorted(d for d in dn if d not in ("node_modules", ".git"))
            found.extend(os.path.join(dp, f) for f in sorted(fn) if f.endswith(".md"))
    return found


def main(argv=None):
    ap = argparse.ArgumentParser(description="Run every fenced code sample in the documentation.")
    ap.add_argument("paths", nargs="*", help="markdown files or directories (default: docs/ and site/assets/docs/ under --root)")
    ap.add_argument("--root", default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")))
    ap.add_argument("--dump", help="an anvil-dump binary built from the native tag being published")
    ap.add_argument("--core", help="the core package")
    ap.add_argument("--js", help="the anvil.js package")
    ap.add_argument("--py", help="the anvil.py package")
    ap.add_argument("--net", help="the anvil.net package")
    opts = ap.parse_args(argv)
    root = os.path.abspath(opts.root)
    scratch = tempfile.mkdtemp(prefix="check-examples-")
    try:
        ctx = Context(scratch, dump=opts.dump and os.path.abspath(opts.dump), js=opts.js and os.path.abspath(opts.js))
        counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
        for path in find_markdown(root, opts.paths):
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            for s, res in check_file(path, rel, ctx):
                counts[res.status] += 1
                label = s.lang or "(none)" if not s.orphan else "marker"
                print("%s %s:%d %s%s" % (res.status, s.file, s.line, label, " - " + res.reason if res.reason else ""))
        print("check-examples: %d passed, %d failed, %d skipped" % (counts["PASS"], counts["FAIL"], counts["SKIP"]))
        return 1 if counts["FAIL"] else 0
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
