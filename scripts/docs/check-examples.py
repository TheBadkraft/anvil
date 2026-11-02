#!/usr/bin/env python3
"""check-examples: runs every fenced code sample in the documentation (FR-2611-anvl-docs-001).

    check-examples.py [--root DIR] [--dump anvil-dump] [--core PKG] [--js PKG] [--py PKG] [--net PKG]
                      [--strict] [--scratch DIR] [paths...]
"""
import argparse
import os
import re
import shutil
import subprocess
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
PY_LANGS = {"python", "py"}
NET_LANGS = {"csharp", "cs", "c#"}
TIMEOUT = 60


def run_cmd(cmd, cwd, env, timeout=TIMEOUT):
    """Runs a command; returns (returncode, stdout, stderr). returncode is None on a timeout."""
    try:
        p = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout, stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired as e:
        return None, (e.stdout or b"").decode() if isinstance(e.stdout, bytes) else (e.stdout or ""), "timed out after %ds" % timeout
    except OSError as e:
        return 127, "", str(e)
    return p.returncode, p.stdout, p.stderr


def tail_text(text, n=300):
    text = " ".join(text.split())
    return text if len(text) <= n else "..." + text[-n:]


class Setup:
    """A named earlier sample: code to prepend (code languages) and files to put in the sample's directory."""

    def __init__(self, name, kind, lang, code, files):
        self.name, self.kind, self.lang, self.code, self.files = name, kind, lang, code, files


class Context:
    def __init__(self, scratch, dump=None, js=None, core=None, py=None, net=None):
        self.scratch, self.dump, self.js, self.core, self.py, self.net = scratch, dump, js, core, py, net
        self.counter = 0
        self.env = dict(os.environ)
        home = os.path.join(scratch, "home")
        tmp = os.path.join(scratch, "tmp")
        os.makedirs(home, exist_ok=True)
        os.makedirs(tmp, exist_ok=True)
        self.env.update({"HOME": home, "TMPDIR": tmp, "XDG_CACHE_HOME": os.path.join(home, ".cache"), "XDG_CONFIG_HOME": os.path.join(home, ".config")})
        self.jsenv = None

    def sample_dir(self, under_jsenv=False):
        self.counter += 1
        base = self.js_env() if under_jsenv else self.scratch
        d = os.path.join(base, "s%d" % self.counter)
        os.makedirs(d)
        return d

    def js_env(self):
        """A directory whose node_modules/anvil.js is the package under test, so require('anvil.js') finds it."""
        if self.jsenv is None:
            self.jsenv = os.path.join(self.scratch, "jsenv")
            mods = os.path.join(self.jsenv, "node_modules")
            os.makedirs(mods)
            shutil.copytree(self.js, os.path.join(mods, "anvil.js"))
        return self.jsenv


def write_files(directory, files):
    for name, text in files.items():
        p = os.path.join(directory, name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w") as f:
            f.write(text)


# ---------------------------------------------------------------------------------------------
# documents: anvil-dump and anvil.js must agree
# ---------------------------------------------------------------------------------------------
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
    if l == "c":
        return "c"
    if l in PY_LANGS:
        return "py"
    if l in NET_LANGS:
        return "net"
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
    if "output" in marks:
        return failed("output blocks are not implemented")
    prefix, files = "", {}
    for name in marks.get("uses", []):
        su = state["setups"].get(name)
        if su is None:
            return failed("uses '%s', but no earlier sample in this file is marked 'setup %s'" % (name, name))
        if su.kind == "code":
            if su.lang != s.lang:
                return failed("uses '%s', a %s sample, from a %s sample" % (name, su.lang, s.lang))
            prefix += su.code
        files.update(su.files)
    handler = HANDLERS.get(kind)
    if handler is None:
        return failed("not implemented: %s samples" % kind)
    expected = int(marks["error"][0]) if "error" in marks else None
    text = prefix + s.text
    res = handler(s, text, files, ctx, expected)
    for name in marks.get("setup", []):
        own = {name: s.text} if kind == "doc" else {}
        state["setups"][name] = Setup(name, "doc" if kind == "doc" else "code", s.lang, text if kind != "doc" else "", dict(files, **own))
    return res


def check_file(path, rel, ctx):
    with open(path, encoding="utf-8") as f:
        samples = extract_samples(rel, f.read())
    state = {"setups": {}, "stdout": None}
    out = []
    for s in samples:
        res = check_sample(s, state, ctx)
        out.append((s, res))
    return out


def run_code(ctx, directory, cmd, files, fname, text, what):
    write_files(directory, files)
    write_files(directory, {fname: text})
    rc, out, err = run_cmd(cmd, directory, ctx.env)
    if rc != 0:
        return failed("%s exited %s: %s" % (what, rc if rc is not None else "(timeout)", tail_text(err or out)))
    return passed(stdout=out)


def check_js(s, text, files, ctx, expected):
    if not ctx.js:
        return skipped("no --js (the anvil.js package)", needs=True)
    fname = "sample.mjs" if re.search(r"^\s*(import|export)\s", text, re.M) else "sample.js"
    return run_code(ctx, ctx.sample_dir(under_jsenv=True), ["node", fname], files, fname, text, "node")


HANDLERS["js"] = check_js
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


def package_dir(arg, flag):
    if arg is None:
        return None
    if not os.path.isdir(arg):
        raise SystemExit("check-examples: %s %s is not a directory" % (flag, arg))
    return os.path.abspath(arg)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Run every fenced code sample in the documentation.")
    ap.add_argument("paths", nargs="*", help="markdown files or directories (default: docs/ and site/assets/docs/ under --root)")
    ap.add_argument("--root", default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")))
    ap.add_argument("--dump", help="an anvil-dump binary built from the native tag being published")
    ap.add_argument("--core", help="the core package (directory)")
    ap.add_argument("--js", help="the anvil.js package (directory)")
    ap.add_argument("--py", help="the anvil.py package (directory)")
    ap.add_argument("--net", help="the anvil.net package (directory)")
    ap.add_argument("--strict", action="store_true", help="a sample skipped for a missing package or toolchain fails the run")
    ap.add_argument("--scratch", help="scratch directory (default: a temporary one, removed on exit)")
    opts = ap.parse_args(argv)
    root = os.path.abspath(opts.root)
    own = opts.scratch is None
    scratch = os.path.abspath(opts.scratch) if opts.scratch else tempfile.mkdtemp(prefix="check-examples-")
    os.makedirs(scratch, exist_ok=True)
    try:
        dump = os.path.abspath(opts.dump) if opts.dump else None
        if dump and not os.access(dump, os.X_OK):
            raise SystemExit("check-examples: --dump %s is not an executable" % opts.dump)
        ctx = Context(scratch, dump=dump, js=package_dir(opts.js, "--js"), core=package_dir(opts.core, "--core"),
                      py=package_dir(opts.py, "--py"), net=package_dir(opts.net, "--net"))
        counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
        needs_skips = 0
        for path in find_markdown(root, opts.paths):
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            for s, res in check_file(path, rel, ctx):
                counts[res.status] += 1
                if res.status == "SKIP" and res.needs:
                    needs_skips += 1
                label = s.lang or "(none)" if not s.orphan else "marker"
                print("%s %s:%d %s%s" % (res.status, s.file, s.line, label, " - " + res.reason if res.reason else ""))
        print("check-examples: %d passed, %d failed, %d skipped" % (counts["PASS"], counts["FAIL"], counts["SKIP"]))
        return 1 if counts["FAIL"] or (opts.strict and needs_skips) else 0
    finally:
        if own:
            shutil.rmtree(scratch, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
