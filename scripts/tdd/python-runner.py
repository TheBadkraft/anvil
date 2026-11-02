#!/usr/bin/env python3
"""check-tdd runner for tooling repos: python-runner.py <root> <out>

Finds every test_*.py under <root>/scripts and <root>/.claude/hooks, imports each (its own directory on
sys.path), runs its unittest cases and writes one JSON object per test to <out>:
  {"id": "scripts/tdd/test_x.py::Class.test_name", "s": "pass|fail|skip", "k": "assertion|error|crash", "m": "..."}
A failed assertion (AssertionError) is "assertion". Any other exception in a test is "error". A test file that
cannot be imported (ImportError, SyntaxError, anything at module level) is one "crash" record named by the file.
"""
import importlib.util
import json
import os
import sys
import unittest

ROOTS = ("scripts", os.path.join(".claude", "hooks"))


def find_tests(root):
    for r in ROOTS:
        for dp, dn, fn in os.walk(os.path.join(root, r)):
            dn[:] = sorted(d for d in dn if d not in ("__pycache__", "node_modules", ".git"))
            for f in sorted(fn):
                if f.startswith("test_") and f.endswith(".py"):
                    yield os.path.join(dp, f)


class Recorder(unittest.TestResult):
    def __init__(self, rel, modname, out):
        super().__init__()
        self.rel, self.modname, self.out = rel, modname, out

    def _id(self, test):
        return "%s::%s" % (self.rel, test.id()[len(self.modname) + 1:])

    def _put(self, test, s, k=None, m=""):
        tid = self._id(test)
        if tid not in self.out or self.out[tid]["s"] == "pass":
            self.out[tid] = {"id": tid, "s": s, "k": k, "m": m[:200]}

    def addSuccess(self, test):
        self._put(test, "pass")

    def addSkip(self, test, reason):
        self._put(test, "skip")

    def addFailure(self, test, err):
        self._put(test, "fail", "assertion", str(err[1]).splitlines()[0] if str(err[1]) else "AssertionError")

    def addError(self, test, err):
        self._put(test, "fail", "error", "%s: %s" % (err[0].__name__, str(err[1]).splitlines()[0] if str(err[1]) else ""))

    def addSubTest(self, test, subtest, err):
        if err is not None:
            kind = "assertion" if issubclass(err[0], AssertionError) else "error"
            self._put(test, "fail", kind, "%s: %s" % (err[0].__name__, err[1]))


def main(root, dest):
    root = os.path.realpath(root)
    os.chdir(root)
    out = {}
    for path in find_tests(root):
        rel = os.path.relpath(path, root)
        modname = "ctdd_" + os.path.splitext(os.path.basename(path))[0]
        sys.path.insert(0, os.path.dirname(path))
        try:
            spec = importlib.util.spec_from_file_location(modname, path)
            mod = importlib.util.module_from_spec(spec)
            sys.modules[modname] = mod
            spec.loader.exec_module(mod)
        except BaseException as e:  # ImportError, SyntaxError, a crash at module level
            out[rel] = {"id": rel, "s": "fail", "k": "crash", "m": ("%s: %s" % (type(e).__name__, e))[:200]}
            continue
        finally:
            sys.path.pop(0)
        unittest.defaultTestLoader.loadTestsFromModule(mod).run(Recorder(rel, modname, out))
    with open(dest, "w") as f:
        for rec in out.values():
            f.write(json.dumps(rec) + "\n")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
