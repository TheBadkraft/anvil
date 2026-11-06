#!/usr/bin/env python3
"""Tests for check-tdd. Fixture repos are tiny git repos with scripted commits, built in a temp dir.

Run (under the cap, STANDARDS section 8):
  systemd-run --user --scope -q -p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=600 -- \
      taskset -c 6-7 python3 scripts/tdd/test_check_tdd.py
"""
import contextlib
import importlib.util
import io
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("check_tdd", os.path.join(HERE, "check-tdd.py"))
ct = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ct)

TESTJS = "const test = require('node:test');\nconst assert = require('assert');\nconst lib = require('../src/lib.js');\n"


def case(name, body):
    return "test(%r, () => { %s });\n" % (name, body)


class Fixture:
    """A scratch git repo with a node:test project; commit() scripts history."""

    def __init__(self):
        self.dir = tempfile.mkdtemp(prefix="ctdd-fx-")
        self.run("init", "-q", "-b", "main")
        self.run("config", "user.email", "t@example.org")
        self.run("config", "user.name", "t")
        self.run("config", "commit.gpgsign", "false")
        self.n = 0

    def run(self, *a):
        return subprocess.run(["git", "-C", self.dir, *a], check=True, capture_output=True, text=True).stdout

    def commit(self, subject, files, body=""):
        for path, content in files.items():
            full = os.path.join(self.dir, path)
            if content is None:
                os.remove(full)
                continue
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "w") as f:
                f.write(content)
        self.run("add", "-A")
        self.run("commit", "-q", "--allow-empty", "-m", subject + ("\n\n" + body if body else ""))
        return self.run("rev-parse", "HEAD").strip()

    def base(self):
        return self.commit("init", {
            "package.json": json.dumps({"name": "fx", "scripts": {"test": "node --test test/*.test.js"}}),
            "src/lib.js": "exports.add = (a, b) => a + b;\n",
            "test/lib.test.js": TESTJS + case("add", "assert.strictEqual(lib.add(1, 2), 3);"),
        })

    def cleanup(self):
        shutil.rmtree(self.dir, ignore_errors=True)


class Base(unittest.TestCase):
    def setUp(self):
        self.fx = Fixture()
        self.cache = tempfile.mkdtemp(prefix="ctdd-cache-")
        self.addCleanup(self.fx.cleanup)
        self.addCleanup(shutil.rmtree, self.cache, True)

    def check(self, base, *extra):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = ct.main([self.fx.dir, base + "..HEAD", "--json", "--cache", self.cache, *extra])
        data = json.loads(out.getvalue()) if out.getvalue().strip() else {"commits": [], "summary": {}}
        return rc, data["commits"], data["summary"]

    def last(self, base, *extra):
        rc, commits, summary = self.check(base, *extra)
        self.assertTrue(commits, "no commits reported (rc=%s)" % rc)
        return rc, commits[-1]

    def assertFails(self, base, text, *extra):
        rc, rec = self.last(base, *extra)
        self.assertEqual(rec["status"], "FAIL", rec)
        self.assertEqual(rc, 1)
        self.assertIn(text, " | ".join(rec["reasons"]))
        return rec

    def assertPasses(self, base, *extra):
        rc, rec = self.last(base, *extra)
        self.assertEqual(rec["status"], "PASS", rec)
        self.assertEqual(rc, 0)
        return rec


MUL = TESTJS + case("add", "assert.strictEqual(lib.add(1, 2), 3);")


class RedRules(Base):
    def test_red_on_assertion_passes(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. mul", {
            "src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = () => 0;\n",
            "test/mul.test.js": TESTJS + case("mul", "assert.strictEqual(lib.mul(2, 3), 6);")})
        self.assertPasses(b)

    def test_red_with_type_error_fails(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. mul", {"test/mul.test.js": TESTJS + case("mul", "assert.strictEqual(lib.mul(2, 3), 6);")})
        self.assertFails(b, "do not fail on an assertion")

    def test_red_with_missing_module_fails(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. mul", {"test/mul.test.js": "require('../src/nothere.js');\n"})
        self.assertFails(b, "do not fail on an assertion")

    def test_red_with_thrown_error_fails(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. x", {"test/x.test.js": TESTJS + case("x", "throw new Error('boom');")})
        self.assertFails(b, "do not fail on an assertion")

    def test_red_where_nothing_fails_fails(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. x", {"test/x.test.js": TESTJS + case("x", "assert.ok(true);")})
        self.assertFails(b, "no test newly fails")

    def test_red_that_breaks_a_passing_test_on_assertion_passes(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. add changes", {"test/lib.test.js": TESTJS + case("add", "assert.strictEqual(lib.add(1, 2), 4);")})
        self.assertPasses(b)

    def test_red_without_test_change_fails(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. x", {"src/lib.js": "exports.add = () => 0;\n"})
        self.assertFails(b, "changes no test file")


def red_then(fx, extra_tests=None):
    """base + a RED that adds mul/sub tests failing on assertions, with stubs."""
    b = fx.base()
    fx.commit("T-1: RED. mul and sub", {
        "src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = () => 0;\nexports.sub = () => 0;\n",
        "test/ops.test.js": TESTJS + case("mul", "assert.strictEqual(lib.mul(2, 3), 6);") + case("sub", "assert.strictEqual(lib.sub(5, 3), 2);")})
    return b


FULL = "exports.add = (a, b) => a + b;\nexports.mul = (a, b) => a * b;\nexports.sub = (a, b) => a - b;\n"
MUL_ONLY = "exports.add = (a, b) => a + b;\nexports.mul = (a, b) => a * b;\nexports.sub = () => 0;\n"


class GreenRules(Base):
    def test_green_flipping_the_named_test_passes(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mul]", {"src/lib.js": MUL_ONLY})
        rec = self.assertPasses(b)
        self.assertEqual(rec["data"]["flipped"], 1)

    def test_green_flipping_more_than_named_is_a_batch(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mul]", {"src/lib.js": FULL})
        rec = self.assertFails(b, "batch: 1 test(s) flipped beyond the named")
        self.assertEqual(rec["data"]["flipped_beyond_named"], 1)

    def test_green_naming_a_file_covers_its_tests(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. ops [tests: test/ops.test.js]", {"src/lib.js": FULL})
        self.assertPasses(b)

    def test_green_naming_a_test_that_did_not_flip_fails(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mul, sub]", {"src/lib.js": MUL_ONLY})
        self.assertFails(b, "named test(s) did not flip: sub")

    def test_green_without_tests_named_fails(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul", {"src/lib.js": MUL_ONLY})
        self.assertFails(b, "no [tests: ...] named")

    def test_green_without_tests_named_is_a_note_when_lenient(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul", {"src/lib.js": MUL_ONLY})
        rec = self.assertPasses(b, "--lenient-tests")
        self.assertIn("no [tests: ...] named", " ".join(rec["notes"]))

    def test_lenient_green_still_fails_over_the_batch_limit(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. all", {"src/lib.js": FULL})
        self.assertFails(b, "batch: 2 tests flipped at once", "--lenient-tests", "--batch-limit", "1")

    def test_green_that_breaks_a_passing_test_fails(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mul]", {"src/lib.js": "exports.add = () => 0;\nexports.mul = (a, b) => a * b;\nexports.sub = () => 0;\n"})
        self.assertFails(b, "passed at the parent fail")

    def test_green_that_flips_nothing_fails(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. nothing [tests: mul]", {"src/other.js": "module.exports = 1;\n"})
        self.assertFails(b, "no test that failed at the parent passes")

    def test_green_that_edits_an_open_reds_test_file_fails(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mul]", {
            "src/lib.js": MUL_ONLY,
            "test/ops.test.js": TESTJS + case("mul", "assert.strictEqual(lib.mul(2, 3), 6);") + case("sub", "assert.ok(true);")})
        self.assertFails(b, "test file(s) of an open RED changed: test/ops.test.js")

    def test_red_closes_once_its_tests_pass(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. all [tests: test/ops.test.js]", {"src/lib.js": FULL})
        self.fx.commit("T-1: GREEN. add a case [tests: test/lib.test.js > add]", {
            "test/ops.test.js": TESTJS + case("mul", "assert.strictEqual(lib.mul(2, 3), 6);") + case("sub", "assert.strictEqual(lib.sub(5, 3), 2);") + case("extra", "assert.ok(true);"),
            "test/lib.test.js": TESTJS + case("add", "assert.strictEqual(lib.add(1, 2), 4);")})
        rc, commits, _ = self.check(b)
        self.assertEqual(len(commits), 3)
        # the last commit flips nothing, but it must not be blamed for the closed RED's file
        self.assertEqual([c for c in commits[-1]["reasons"] if "open RED" in c], [])
        self.assertEqual(commits[-1]["kind"], "GREEN")


class SizeAndInputs(Base):
    def test_green_with_a_large_product_diff_is_a_batch(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mul]", {"src/lib.js": MUL_ONLY + "".join("// pad %d\n" % i for i in range(40))})
        self.assertFails(b, "product diff of", "--max-green-lines", "30")

    def test_green_within_the_size_limit_passes(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mul]", {"src/lib.js": MUL_ONLY + "".join("// pad %d\n" % i for i in range(40))})
        self.assertPasses(b, "--max-green-lines", "400")

    def test_red_that_only_moves_a_vendored_pin_counts_as_changing_the_tests(self):
        b = self.fx.commit("init", {
            "package.json": json.dumps({"name": "fx", "scripts": {"test": "node --test test/*.test.js"}}),
            "src/lib.js": "exports.add = (a, b) => a + b;\n",
            "vendor/anvil/corpus.txt": "a\n",
            "test/lib.test.js": TESTJS + case("corpus", "assert.strictEqual(require('fs').readFileSync(require('path').join(__dirname, '../vendor/anvil/corpus.txt'), 'utf8'), 'a\\n');")})
        self.fx.commit("T-1: RED. the corpus moves", {"vendor/anvil/corpus.txt": "b\n"})
        rec = self.assertPasses(b)
        self.assertIn("vendored", " ".join(rec["notes"]))


class CompatSuite(Base):
    def compat_repo(self):
        compat = ("const { spawnSync } = require('node:child_process');\n"
                  "const r = spawnSync(process.execPath, ['--test', '--test-reporter=tap', 'test/compat/x.test.js'], { encoding: 'utf8' });\n"
                  "process.exit(r.status === 0 ? 0 : 1);\n")
        return self.fx.commit("init", {
            "package.json": json.dumps({"name": "fx", "scripts": {"test": "node --test test/*.test.js", "compat": "node scripts/compat.js"}}),
            "scripts/compat.js": compat,
            "src/lib.js": "exports.add = (a, b) => a + b;\n",
            "test/compat/x.test.js": "const test = require('node:test'); test('x', () => {});\n",
            "test/lib.test.js": TESTJS + case("add", "assert.strictEqual(lib.add(1, 2), 3);")})

    def test_a_red_in_the_compat_suite_is_seen(self):
        b = self.compat_repo()
        self.fx.commit("T-1: RED. x", {"test/compat/x.test.js": "const test = require('node:test'); test('x', () => { require('../src/lib.js').nope(); });\n"})
        self.assertFails(b, "do not fail on an assertion")

    def test_a_compat_assertion_failure_is_a_genuine_red(self):
        b = self.compat_repo()
        self.fx.commit("T-1: RED. x", {"test/compat/x.test.js": "const test = require('node:test'); test('x', () => { require('assert').strictEqual(1, 2); });\n"})
        self.assertPasses(b)

    def test_no_compat_is_refused_when_the_repo_has_a_compat_script(self):
        b = self.compat_repo()
        self.fx.commit("T-1: RED. x", {"test/compat/x.test.js": "const test = require('node:test'); test('x', () => { require('assert').strictEqual(1, 2); });\n"})
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            rc, commits, _ = self.check(b, "--no-compat")
        self.assertEqual((rc, commits), (2, []))
        self.assertIn("--no-compat", err.getvalue())

    def test_a_compat_run_with_no_events_is_a_setup_error(self):
        b = self.fx.commit("init", {
            "package.json": json.dumps({"name": "fx", "scripts": {"test": "node --test test/*.test.js", "compat": "node scripts/compat.js"}}),
            "scripts/compat.js": "process.exit(0);\n", "src/lib.js": "exports.add = (a, b) => a + b;\n",
            "test/lib.test.js": TESTJS + case("add", "assert.strictEqual(lib.add(1, 2), 3);")})
        self.fx.commit("T-1: REFACTOR. x", {"src/lib.js": "const f = (a, b) => a + b;\nexports.add = f;\n"})
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            rc, commits, _ = self.check(b)
        self.assertEqual((rc, commits), (2, []))
        self.assertIn("compat", err.getvalue())


class PerfRules(Base):
    def gate(self, text):
        return {"bench/gate.json": text}

    def test_perf_with_a_fail_to_pass_gate_passes(self):
        b = self.fx.base()
        files = {"src/lib.js": "exports.add = (a, b) => (a + b);\n"}
        files.update(self.gate('{"before": "fail", "after": "pass"}'))
        self.fx.commit("T-1: PERF. add [gate: bench/gate.json]", files)
        self.assertPasses(b)

    def test_perf_gate_text_form_passes(self):
        b = self.fx.base()
        files = {"src/lib.js": "exports.add = (a, b) => (a + b);\n"}
        files.update(self.gate("add warm: fail -> pass (1.04x -> 0.98x)\n"))
        self.fx.commit("T-1: PERF. add [gate: bench/gate.json]", files)
        self.assertPasses(b)

    def test_perf_without_a_gate_fails(self):
        b = self.fx.base()
        self.fx.commit("T-1: PERF. add", {"src/lib.js": "exports.add = (a, b) => (a + b);\n"})
        self.assertFails(b, "no [gate: path] named")

    def test_perf_with_a_missing_gate_file_fails(self):
        b = self.fx.base()
        self.fx.commit("T-1: PERF. add [gate: bench/nope.json]", {"src/lib.js": "exports.add = (a, b) => (a + b);\n"})
        self.assertFails(b, "bench/nope.json not found")

    def test_perf_with_a_pass_to_pass_gate_fails(self):
        b = self.fx.base()
        files = {"src/lib.js": "exports.add = (a, b) => (a + b);\n"}
        files.update(self.gate('{"before": "pass", "after": "pass"}'))
        self.fx.commit("T-1: PERF. add [gate: bench/gate.json]", files)
        self.assertFails(b, "does not say fail -> pass")

    def test_perf_that_changes_a_test_fails(self):
        b = self.fx.base()
        files = {"test/lib.test.js": TESTJS + case("add", "assert.strictEqual(lib.add(1, 2), 3);") + case("n", "assert.ok(1);")}
        files.update(self.gate('{"before": "fail", "after": "pass"}'))
        self.fx.commit("T-1: PERF. add [gate: bench/gate.json]", files)
        self.assertFails(b, "test file(s) changed")

    def test_perf_that_breaks_the_suite_fails(self):
        b = self.fx.base()
        files = {"src/lib.js": "exports.add = () => 0;\n"}
        files.update(self.gate('{"before": "fail", "after": "pass"}'))
        self.fx.commit("T-1: PERF. add [gate: bench/gate.json]", files)
        self.assertFails(b, "suite not green at the commit")


class RefactorRules(Base):
    def test_refactor_keeping_the_suite_green_passes(self):
        b = self.fx.base()
        self.fx.commit("T-1: REFACTOR. add", {"src/lib.js": "const add = (a, b) => a + b;\nexports.add = add;\n"})
        self.assertPasses(b)

    def test_refactor_changing_a_test_file_fails(self):
        b = self.fx.base()
        self.fx.commit("T-1: REFACTOR. add", {"test/lib.test.js": TESTJS + case("add", "assert.strictEqual(lib.add(1, 2), 3);") + "\n"})
        self.assertFails(b, "test file(s) changed")

    def test_refactor_breaking_a_test_fails(self):
        b = self.fx.base()
        self.fx.commit("T-1: REFACTOR. add", {"src/lib.js": "exports.add = () => 0;\n"})
        self.assertFails(b, "suite not green")


class UntaggedAndMisc(Base):
    def test_untagged_commit_touching_product_code_fails(self):
        b = self.fx.base()
        self.fx.commit("speed up add", {"src/lib.js": "exports.add = (a, b) => a + b + 0;\n"})
        self.assertFails(b, "touches product code without a RED/GREEN/PERF/REFACTOR tag")

    def test_untagged_commit_touching_only_docs_passes(self):
        b = self.fx.base()
        self.fx.commit("notes", {"README.md": "hi\n", "scripts/x.js": "1;\n"})
        self.assertPasses(b)

    def test_merge_commits_are_skipped_in_the_branch_walk(self):
        b = self.fx.base()
        self.fx.run("checkout", "-q", "-b", "feat")
        self.fx.commit("T-1: RED. mul", {
            "src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = () => 0;\n",
            "test/mul.test.js": TESTJS + case("mul", "assert.strictEqual(lib.mul(2, 3), 6);")})
        self.fx.run("checkout", "-q", "main")
        self.fx.run("merge", "-q", "--no-ff", "-m", "Merge feat", "feat")
        rc, commits, _ = self.check(b)
        self.assertEqual([c["kind"] for c in commits], ["RED"])
        self.assertEqual(rc, 0)
        rc, commits, _ = self.check(b, "--first-parent")
        self.assertEqual([c["kind"] for c in commits], ["MERGE"])

    def test_exit_code_is_nonzero_on_any_fail_and_results_are_cached(self):
        b = self.fx.base()
        counter = os.path.join(self.cache, "count")
        self.fx.commit("T-1: RED. x", {"test/x.test.js": "require('fs').appendFileSync(%r, 'x');\n" % counter + TESTJS + case("x", "assert.ok(true);")})
        def runs():
            return open(counter).read().count("x") if os.path.exists(counter) else 0
        rc1, _, s1 = self.check(b)
        runs1 = runs()
        rc2, _, s2 = self.check(b)
        runs2 = runs()
        self.assertEqual(rc1, 1)
        self.assertEqual(rc2, 1)
        self.assertEqual(runs1, 1)  # the test file only exists at the head commit: one run
        self.assertEqual(runs2, 1)  # the second check came from the cache
        self.assertEqual(s2.get("suite_runs"), 0)

    def test_budget_stops_with_exit_3_and_resumes(self):
        b = self.fx.base()
        self.fx.commit("T-1: REFACTOR. a", {"src/lib.js": "const f = (a, b) => a + b;\nexports.add = f;\n"})
        rc, commits, s = self.check(b, "--budget", "-1")
        self.assertEqual(rc, 3)
        self.assertTrue(s["incomplete"])
        rc, commits, s = self.check(b)
        self.assertEqual(rc, 0)

    def test_bad_range_is_a_usage_error(self):
        self.fx.base()
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(ct.main([self.fx.dir, "nope"]), 2)
            self.assertEqual(ct.main([self.fx.dir, "nope..HEAD"]), 2)


def brief(tests):
    """{id: pass|skip|<failure kind>}"""
    return {t: (r.get("k") if r["s"] == "fail" else r["s"]) for t, r in tests.items()}


class NodeParser(unittest.TestCase):
    def ev(self, r, name, nesting, file="/w/test/a.test.js", **kw):
        d = {"r": r, "name": name, "nesting": nesting, "file": file}
        d.update(kw)
        return json.dumps(d)

    def test_children_come_before_their_suite_and_get_a_path(self):
        lines = [self.ev("pass", "one", 1), self.ev("fail", "two", 1, failureType="testCodeFailure", code="ERR_ASSERTION", errName="AssertionError", message="m"),
                 self.ev("fail", "suite", 0, failureType="subtestsFailed", message="1 subtests failed")]
        t = ct.parse_node_events(lines, root="/w")
        self.assertEqual(brief(t), {"test/a.test.js > suite > one": "pass", "test/a.test.js > suite > two": "assertion"})

    def test_error_kinds(self):
        lines = [self.ev("fail", "t", 0, failureType="testCodeFailure", code=None, errName="TypeError", message="x is not a function"),
                 self.ev("fail", "u", 0, failureType="testCodeFailure", code="ERR_ASSERTION", errName="AssertionError", message="m"),
                 self.ev("fail", "test/b.test.js", 0, file="/w/test/b.test.js", failureType="testCodeFailure", errName="String", message="test failed")]
        t = ct.parse_node_events(lines, root="/w")
        self.assertEqual(brief(t), {"test/a.test.js > t": "error", "test/a.test.js > u": "assertion", "test/b.test.js": "crash"})

    def test_skips_noise_and_duplicate_names(self):
        lines = ["TAP noise", self.ev("pass", "d", 0), self.ev("skip", "d", 0), "{broken"]
        t = ct.parse_node_events(lines, root="/w")
        self.assertEqual(brief(t), {"test/a.test.js > d": "pass", "test/a.test.js > d #2": "skip"})


class NativeParser(unittest.TestCase):
    OUT = ("\n\x1b[1m[TEST]\x1b[0m alpha\n  ok one\n  \x1b[2m-- passed\x1b[0m\n"
           "\n[TEST] beta\n  FAIL two\n  -- FAILED (1 assertion)\n"
           "\n[TEST] gamma\n  -- skipped\n")

    def test_passed_failed_skipped(self):
        t = ct.parse_testbit(self.OUT, "header", 1)
        self.assertEqual(brief(t), {"header::alpha": "pass", "header::beta": "assertion", "header::gamma": "skip"})

    def test_a_test_with_no_outcome_is_a_crash(self):
        t = ct.parse_testbit("[TEST] alpha\n  ok\n-- passed\n[TEST] beta\n  ok x\n", "s", -11)
        self.assertEqual(brief(t), {"s::alpha": "pass", "s::beta": "crash"})

    def test_no_output_and_failure_is_a_build_error(self):
        t = ct.parse_testbit("gcc: error: x.c: No such file\nmake: *** [Makefile:1] Error 1\n", "s", 2)
        self.assertEqual(brief(t), {"s::<build>": "build"})

    def test_no_output_and_success_is_empty(self):
        self.assertEqual(ct.parse_testbit("", "s", 0), {})


class TestsTag(unittest.TestCase):
    def test_brackets_inside_names(self):
        msg = "T-1: GREEN. x [tests: a @[schema] b, c @[types]: d] tail\n\nbody"
        self.assertEqual(ct.parse_tests_tag(msg), ["a @[schema] b", "c @[types]: d"])

    def test_plain_and_repeated_and_absent(self):
        self.assertEqual(ct.parse_tests_tag("x [tests: a, b]"), ["a", "b"])
        self.assertEqual(ct.parse_tests_tag("x [tests: a]\n\n[tests: b]"), ["a", "b"])
        self.assertEqual(ct.parse_tests_tag("no tag [here]"), [])

    def test_unbalanced_tag_takes_the_rest(self):
        self.assertEqual(ct.parse_tests_tag("x [tests: a @[schema b"), ["a @[schema b"])


class GreenWithBrackets(Base):
    def test_green_naming_a_test_with_brackets_passes(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. brackets", {
            "src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = () => 0;\n",
            "test/mul.test.js": TESTJS + case("doc without @[schema] sets 11", "assert.strictEqual(lib.mul(2, 3), 6);")
            + case("doc with @[types] ok", "assert.strictEqual(lib.mul(2, 4), 8);")})
        self.fx.commit("T-1: GREEN. mul [tests: doc without @[schema] sets 11, doc with @[types] ok]",
                       {"src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = (a, b) => a * b;\n"})
        rc, commits, _ = self.check(b)
        self.assertEqual([c["status"] for c in commits], ["PASS", "PASS"], commits)


class NativeSetup(Base):
    def test_testbit_resolution_order(self):
        d = tempfile.mkdtemp(prefix="ctdd-tb-")
        self.addCleanup(shutil.rmtree, d, True)
        repo = os.path.join(d, "wt", "repo")
        os.makedirs(repo)
        fb = os.path.join(d, "fallback")
        os.makedirs(fb)
        given = os.path.join(d, "given")
        os.makedirs(given)
        self.assertEqual(ct.resolve_testbit(repo, given, [fb]), given)
        self.assertEqual(ct.resolve_testbit(repo, None, [fb]), fb)
        sib = os.path.join(d, "wt", "testbit")
        os.makedirs(sib)
        self.assertEqual(ct.resolve_testbit(repo, None, [fb]), sib)
        self.assertIsNone(ct.resolve_testbit(repo, os.path.join(d, "nope"), [fb]))
        shutil.rmtree(sib)
        shutil.rmtree(fb)
        self.assertIsNone(ct.resolve_testbit(repo, None, [fb]))

    def test_missing_testbit_is_a_loud_error_not_a_result(self):
        d = tempfile.mkdtemp(prefix="ctdd-nat-")
        self.addCleanup(shutil.rmtree, d, True)
        repo = os.path.join(d, "r")
        fx = Fixture()
        self.addCleanup(fx.cleanup)
        fx.commit("init", {"test/unit/test_a.c": "int main(void){return 0;}\n"})
        err = io.StringIO()
        with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
            rc = ct.main([fx.dir, "HEAD~0..HEAD", "--adapter", "native", "--testbit", os.path.join(d, "none"), "--cache", self.cache])
        self.assertEqual(rc, 2)
        self.assertIn("testbit not found", err.getvalue())

    def test_make_command_carries_testbit_and_vars(self):
        class O: testbit = "/tb"; make_var = ["BUILD_NUM=500", "X=1"]
        cmd = ct.native_make_cmd("/u", "header", O)
        self.assertEqual(cmd[:4], ["make", "-C", "/u", "test_header"])
        self.assertEqual(cmd[4:], ["TESTBIT=/tb", "BUILD_NUM=500", "X=1"])

    def test_cache_key_covers_testbit_and_vars(self):
        class A: testbit = "/tb"; make_var = []; no_compat = False
        class B: testbit = "/other"; make_var = []; no_compat = False
        class C: testbit = "/tb"; make_var = ["BUILD_NUM=5"]; no_compat = False
        k = {ct.cache_key("native", "abc", o) for o in (A, B, C)}
        self.assertEqual(len(k), 3)
        self.assertTrue(all("abc" in x for x in k))
        self.assertNotEqual(ct.cache_key("native", "abc", A), ct.cache_key("node", "abc", A))


class AdapterPaths(unittest.TestCase):
    def test_python_adapter_classifies_scripts_and_hooks(self):
        a = ct.PythonAdapter()
        tests = ["scripts/tdd/test_x.py", ".claude/hooks/test_h.py", "scripts/a/b/test_z.py"]
        product = ["scripts/tdd/x.py", "scripts/tdd/x.js", ".claude/hooks/h.py", "scripts/a/b/c.py"]
        neither = ["scripts/check.sh", "docs/a.md", "src/a.c", ".claude/settings.json", "scripts/data.json"]
        got = {p: (a.is_test(p), a.is_product(p)) for p in tests + product + neither}
        want = {p: (True, False) for p in tests}
        want.update({p: (False, True) for p in product})
        want.update({p: (False, False) for p in neither})
        self.assertEqual(got, want)

    def test_node_and_native_keep_their_prefixes(self):
        n, c = ct.NodeAdapter(), ct.NativeAdapter()
        self.assertEqual([n.is_product("src/a.js"), n.is_product("js/a.js"), n.is_test("test/a.js"), n.is_input("vendor/anvil"), n.is_product("docs/a.md")],
                         [True, True, True, True, False])
        self.assertEqual([c.is_product("src/a.c"), c.is_product("include/a.h"), c.is_test("test/unit/a.c"), c.is_input("vendor/anvil"), c.is_product("scripts/a.py")],
                         [True, True, True, False, False])


class PickAdapter(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix="ctdd-pick-")
        self.addCleanup(shutil.rmtree, self.d, True)
        os.makedirs(os.path.join(self.d, "test", "unit"))

    def test_scripts_only_range_picks_python(self):
        self.assertEqual(ct.pick_adapter(self.d, "auto", ["scripts/tdd/x.py", ".claude/hooks/h.py", "docs/changelog.md"]).name, "python")

    def test_a_range_touching_native_code_stays_native(self):
        self.assertEqual(ct.pick_adapter(self.d, "auto", ["src/a.c", "scripts/x.py"]).name, "native")
        self.assertEqual(ct.pick_adapter(self.d, "auto", None).name, "native")

    def test_a_node_repo_keeps_node(self):
        with open(os.path.join(self.d, "package.json"), "w") as f:
            f.write("{}")
        self.assertEqual(ct.pick_adapter(self.d, "auto", ["scripts/x.js"]).name, "node")

    def test_an_explicit_name_that_matches_the_detection_is_accepted(self):
        self.assertEqual(ct.pick_adapter(self.d, "native", ["src/a.c"]).name, "native")

    def test_an_explicit_name_that_contradicts_the_detection_is_refused(self):
        with self.assertRaises(SystemExit) as cm:
            ct.pick_adapter(self.d, "python", ["src/a.c"])
        self.assertIn("contradicts", str(cm.exception))

    def test_an_explicit_name_is_accepted_when_nothing_can_be_detected(self):
        os.rmdir(os.path.join(self.d, "test", "unit"))
        self.assertEqual(ct.pick_adapter(self.d, "python", ["src/a.c"]).name, "python")


PYTEST = "import unittest\nimport lib\n\n\nclass T(unittest.TestCase):\n"


def pycase(name, body):
    return "    def test_%s(self):\n        %s\n" % (name, body)


class PyBase(Base):
    def py_base(self):
        return self.fx.commit("init", {
            "scripts/lib.py": "def add(a, b):\n    return a + b\n",
            "scripts/test_lib.py": PYTEST + pycase("add", "self.assertEqual(lib.add(1, 2), 3)")})

    def check_py(self, base, *extra):
        return self.check(base, "--adapter", "python", *extra)

    def last_py(self, base, *extra):
        rc, commits, _ = self.check_py(base, *extra)
        self.assertTrue(commits, "no commits reported (rc=%s)" % rc)
        return rc, commits[-1]


STUB_MUL = "def add(a, b):\n    return a + b\n\n\ndef mul(a, b):\n    return 0\n"
REAL_MUL = "def add(a, b):\n    return a + b\n\n\ndef mul(a, b):\n    return a * b\n"


class PythonSuite(PyBase):
    def test_red_on_an_assertion_passes(self):
        b = self.py_base()
        self.fx.commit("T-1: RED. mul", {"scripts/lib.py": STUB_MUL, "scripts/test_mul.py": PYTEST + pycase("mul", "self.assertEqual(lib.mul(2, 3), 6)")})
        rc, rec = self.last_py(b)
        self.assertEqual((rec["status"], rc), ("PASS", 0), rec)
        self.assertEqual(rec["data"]["newly_failing"], 1)

    def test_red_on_an_attribute_error_fails(self):
        b = self.py_base()
        self.fx.commit("T-1: RED. mul", {"scripts/test_mul.py": PYTEST + pycase("mul", "self.assertEqual(lib.mul(2, 3), 6)")})
        rc, rec = self.last_py(b)
        self.assertEqual(rec["status"], "FAIL", rec)
        self.assertIn("do not fail on an assertion", " ".join(rec["reasons"]))

    def test_red_with_an_import_error_fails(self):
        b = self.py_base()
        self.fx.commit("T-1: RED. mul", {"scripts/test_mul.py": "import nothere\n"})
        rc, rec = self.last_py(b)
        self.assertEqual(rec["status"], "FAIL", rec)
        self.assertIn("do not fail on an assertion", " ".join(rec["reasons"]))

    def test_red_with_a_syntax_error_fails(self):
        b = self.py_base()
        self.fx.commit("T-1: RED. mul", {"scripts/test_mul.py": "def (:\n"})
        rc, rec = self.last_py(b)
        self.assertEqual(rec["status"], "FAIL", rec)

    def test_green_flipping_the_named_test_passes(self):
        b = self.py_base()
        self.fx.commit("T-1: RED. mul", {"scripts/lib.py": STUB_MUL, "scripts/test_mul.py": PYTEST + pycase("mul", "self.assertEqual(lib.mul(2, 3), 6)")})
        self.fx.commit("T-1: GREEN. mul [tests: test_mul]", {"scripts/lib.py": REAL_MUL})
        rc, rec = self.last_py(b)
        self.assertEqual((rec["status"], rc), ("PASS", 0), rec)
        self.assertEqual(rec["data"]["flipped"], 1)

    def test_a_hook_test_is_discovered(self):
        b = self.py_base()
        self.fx.commit("T-1: RED. hook", {".claude/hooks/h.py": "def f():\n    return 0\n",
                                         ".claude/hooks/test_h.py": "import unittest\nimport h\n\n\nclass H(unittest.TestCase):\n" + pycase("f", "self.assertEqual(h.f(), 1)")})
        rc, rec = self.last_py(b)
        self.assertEqual((rec["status"], rec["data"]["newly_failing"]), ("PASS", 1), rec)

    def test_untagged_product_change_fails_and_docs_pass(self):
        b = self.py_base()
        self.fx.commit("tidy", {"scripts/lib.py": "def add(a, b):\n    return b + a\n"})
        self.fx.commit("notes", {"docs/x.md": "x\n", "scripts/run.sh": "true\n"})
        rc, commits, _ = self.check_py(b)
        self.assertEqual([c["status"] for c in commits], ["FAIL", "PASS"], commits)

    def test_auto_picks_python_for_a_scripts_only_range(self):
        b = self.py_base()
        self.fx.commit("T-1: RED. mul", {"scripts/lib.py": STUB_MUL, "scripts/test_mul.py": PYTEST + pycase("mul", "self.assertEqual(lib.mul(2, 3), 6)")})
        rc, commits, s = self.check(b)
        self.assertEqual((s.get("adapter"), [c["status"] for c in commits]), ("python", ["PASS"]), commits)


class AccidentClass(Base):
    """A careless but honest run must not produce a wrong PASS."""

    def test_a_contradicting_adapter_is_exit_2_not_a_verdict(self):
        b = self.fx.base()
        self.fx.commit("speed up add", {"src/lib.js": "exports.add = (a, b) => b + a;\n"})
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            rc, commits, _ = self.check(b, "--adapter", "python")
        self.assertEqual((rc, commits), (2, []))
        self.assertIn("contradicts", err.getvalue())

    def test_first_parent_is_recorded_in_the_options(self):
        b = self.fx.base()
        head = self.fx.commit("T-1: REFACTOR. add", {"src/lib.js": "const f = (a, b) => a + b;\nexports.add = f;\n"})
        self.check(b, "--first-parent")
        common = self.fx.run("rev-parse", "--path-format=absolute", "--git-common-dir").strip()
        with open(os.path.join(common, "check-tdd", "pass", head + ".json")) as f:
            self.assertIs(json.load(f)["options"].get("first_parent"), True)

    def many_red(self, n):
        cases = "".join(case("t%d" % i, "assert.strictEqual(lib.mul(2, 3), 6);") for i in range(n))
        b = self.fx.base()
        self.fx.commit("T-1: RED. many", {"src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = () => 0;\n", "test/many.test.js": TESTJS + cases})
        return b

    def test_naming_a_test_file_still_counts_every_flipped_test_against_the_batch_limit(self):
        b = self.many_red(11)
        self.fx.commit("T-1: GREEN. mul [tests: test/many.test.js]", {"src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = (a, b) => a * b;\n"})
        self.assertFails(b, "batch: 11 tests flipped at once")

    def test_ten_flips_named_by_file_are_within_the_limit(self):
        b = self.many_red(10)
        self.fx.commit("T-1: GREEN. mul [tests: test/many.test.js]", {"src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = (a, b) => a * b;\n"})
        self.assertPasses(b)

    def test_a_red_that_renames_a_test_still_failing_from_an_open_red_weakens_it(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: RED. rename", {"test/ops.test.js": TESTJS + case("mul2", "assert.strictEqual(lib.mul(2, 3), 6);") + case("sub", "assert.strictEqual(lib.sub(5, 3), 2);")})
        self.assertFails(b, "weakens an open RED")

    def test_a_red_that_removes_a_failing_test_of_an_open_red_weakens_it(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: RED. drop", {"test/ops.test.js": TESTJS + case("sub", "assert.strictEqual(lib.sub(5, 3), 2);"), "test/extra.test.js": TESTJS + case("extra", "assert.strictEqual(lib.mul(1, 1), 2);")})
        self.assertFails(b, "weakens an open RED")

    def test_a_red_that_turns_a_failing_test_of_an_open_red_into_one_that_passes_weakens_it(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: RED. neutered", {
            "test/ops.test.js": TESTJS + case("mul", "assert.ok(true);") + case("sub", "assert.strictEqual(lib.sub(5, 3), 2);"),
            "test/extra.test.js": TESTJS + case("extra", "assert.strictEqual(lib.mul(1, 1), 2);")})
        rec = self.assertFails(b, "weakens an open RED")
        self.assertIn("pass", " ".join(rec["reasons"]))

    def test_a_red_that_changes_the_expectation_of_a_failing_test_of_an_open_red_is_allowed(self):
        # Documented, on purpose: the test still exists and still fails, and the checker sees outcomes, not intent.
        # A RED may correct a wrong expectation; the test diff is what the reviewer reads.
        b = red_then(self.fx)
        self.fx.commit("T-1: RED. corrected", {
            "test/ops.test.js": TESTJS + case("mul", "assert.strictEqual(lib.mul(2, 3), 7);") + case("sub", "assert.strictEqual(lib.sub(5, 3), 2);"),
            "test/extra.test.js": TESTJS + case("extra", "assert.strictEqual(lib.mul(1, 1), 2);")})
        self.assertPasses(b)

    def test_a_red_that_adds_a_failing_test_beside_an_open_red_is_fine(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: RED. more", {"test/extra.test.js": TESTJS + case("extra", "assert.strictEqual(lib.mul(1, 1), 2);")})
        self.assertPasses(b)


class ExactTestNames(Base):
    def test_a_substring_does_not_name_a_test(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mu]", {"src/lib.js": MUL_ONLY})
        self.assertFails(b, "named test(s) did not flip: mu")

    def test_the_basename_of_a_test_file_is_not_its_path(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. ops [tests: ops.test.js]", {"src/lib.js": FULL})
        self.assertFails(b, "named test(s) did not flip: ops.test.js")

    def test_a_full_test_id_names_a_test(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: test/ops.test.js > mul]", {"src/lib.js": MUL_ONLY})
        self.assertPasses(b)

    def test_a_test_name_with_commas_is_one_name(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. long names", {
            "src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = () => 0;\n",
            "test/ops.test.js": TESTJS + case("mul, then add, twice", "assert.strictEqual(lib.mul(2, 3), 6);") + case("plain", "assert.strictEqual(lib.mul(1, 3), 3);")})
        self.fx.commit("T-1: GREEN. mul [tests: mul, then add, twice, plain]", {"src/lib.js": "exports.add = (a, b) => a + b;\nexports.mul = (a, b) => a * b;\n"})
        rc, rec = self.last(b)
        self.assertEqual((rec["status"], rec["data"]["flipped"]), ("PASS", 2), rec)

    def test_a_comma_name_that_matches_no_test_is_reported_whole(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mul, nothing, here]", {"src/lib.js": MUL_ONLY})
        self.assertFails(b, "named test(s) did not flip: nothing, here")

    def test_a_tag_in_the_body_counts_once(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul", {"src/lib.js": MUL_ONLY}, body="[tests: mul]\n[tests: mul]")
        rc, rec = self.last(b)
        self.assertEqual((rec["status"], rec["data"]["flipped"]), ("PASS", 1), rec)


class TestSetRules(Base):
    def test_an_untagged_commit_that_edits_a_test_fails(self):
        b = self.fx.base()
        self.fx.commit("tidy the tests", {"test/lib.test.js": TESTJS + case("add", "assert.ok(true);")})
        self.assertFails(b, "touches test files without a RED/REFACTOR tag")

    def test_an_untagged_commit_that_deletes_a_test_fails(self):
        b = self.fx.base()
        self.fx.commit("drop a test", {"test/lib.test.js": None})
        self.assertFails(b, "touches test files without a RED/REFACTOR tag")

    def test_a_green_that_deletes_a_failing_test_reduces_the_test_set(self):
        b = red_then(self.fx)
        self.fx.commit("T-1: GREEN. mul [tests: mul]", {"src/lib.js": MUL_ONLY, "test/ops.test.js": None})
        self.assertFails(b, "reduces the test set")

    def test_a_red_that_replaces_tests_is_a_note(self):
        b = self.fx.base()
        self.fx.commit("T-1: RED. replace add", {"test/lib.test.js": TESTJS + case("add2", "assert.strictEqual(lib.add(2, 2), 5);")})
        rec = self.assertPasses(b)
        self.assertIn("replaces", " ".join(rec["notes"]))


class SetupErrors(Base):
    def test_a_missing_vendor_pin_is_a_setup_error_not_a_verdict(self):
        b = self.fx.base()
        self.fx.run("update-index", "--add", "--cacheinfo", "160000,%s,vendor/anvil" % ("a" * 40))
        self.fx.run("commit", "-q", "-m", "T-1: REFACTOR. pin")
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            rc, commits, _ = self.check(b)
        self.assertEqual((rc, commits), (2, []))
        self.assertIn("vendor/anvil", err.getvalue())


class PassRecordOptions(Base):
    def good_chain(self):
        b = self.fx.base()
        return b, self.fx.commit("T-1: REFACTOR. add", {"src/lib.js": "const f = (a, b) => a + b;\nexports.add = f;\n"})

    def record(self, head):
        common = self.fx.run("rev-parse", "--path-format=absolute", "--git-common-dir").strip()
        path = os.path.join(common, "check-tdd", "pass", head + ".json")
        return json.load(open(path)) if os.path.exists(path) else None

    def test_the_record_carries_the_options_of_the_run(self):
        b, head = self.good_chain()
        self.check(b, "--lenient-tests", "--batch-limit", "30", "--max-green-lines", "400")
        rec = self.record(head)
        self.assertEqual(rec and rec.get("options"), {"lenient_tests": True, "batch_limit": 30, "max_green_lines": 400, "no_compat": False, "first_parent": False})

    def test_a_failing_run_removes_the_pass_record_of_that_head(self):
        b = red_then(self.fx)
        head = self.fx.commit("T-1: GREEN. mul [tests: mul]", {"src/lib.js": MUL_ONLY + "".join("// pad %d\n" % i for i in range(40))})
        rc, _, _ = self.check(b)
        self.assertEqual((rc, self.record(head) is not None), (0, True))
        rc, _, _ = self.check(b, "--max-green-lines", "10")
        self.assertEqual((rc, self.record(head)), (1, None))


class CacheSourceHash(unittest.TestCase):
    class O:
        testbit = ""
        make_var = []
        no_compat = False

    def test_the_key_follows_the_runner_and_reporter_sources(self):
        d = tempfile.mkdtemp(prefix="ctdd-src-")
        self.addCleanup(shutil.rmtree, d, True)
        for f in ("python-runner.py", "node-reporter.js"):
            shutil.copy(os.path.join(HERE, f), d)
        old = ct.HERE
        ct.HERE = d
        self.addCleanup(setattr, ct, "HERE", old)
        keys = [ct.cache_key("python", "abc", self.O)]
        for f in ("python-runner.py", "node-reporter.js"):
            with open(os.path.join(d, f), "a") as fh:
                fh.write("\n// changed\n")
            keys.append(ct.cache_key("python", "abc", self.O))
        self.assertEqual(len(set(keys)), 3, keys)


class Signals(Base):
    def slow_repo(self):
        b = self.fx.commit("init", {
            "package.json": json.dumps({"name": "fx", "scripts": {"test": "node --test test/*.test.js"}}),
            "src/lib.js": "exports.add = (a, b) => a + b;\n",
            "test/slow.test.js": "const test = require('node:test');\ntest('slow', async () => { await new Promise((r) => setTimeout(r, 60000)); });\n"})
        self.fx.commit("T-1: REFACTOR. x", {"src/lib.js": "const f = (a, b) => a + b;\nexports.add = f;\n"})
        return b

    def leftovers(self, priv):
        mine = [n for n in os.listdir(priv) if n.startswith("check-tdd-")]
        procs = subprocess.run(["pgrep", "-f", priv], capture_output=True, text=True).stdout.split()
        return mine, [p for p in procs if p != str(os.getpid())]

    def run_and_signal(self, sig):
        import time as _time
        b = self.slow_repo()
        priv = tempfile.mkdtemp(prefix="ctdd-priv-")
        self.addCleanup(shutil.rmtree, priv, True)
        p = subprocess.Popen([sys.executable, os.path.join(HERE, "check-tdd.py"), self.fx.dir, b + "..HEAD", "--cache", self.cache],
                             env=dict(os.environ, TMPDIR=priv), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        deadline = _time.time() + 40
        while _time.time() < deadline and not [n for n in os.listdir(priv) if n.startswith("check-tdd-")]:
            _time.sleep(0.2)
        while _time.time() < deadline and not subprocess.run(["pgrep", "-f", priv + ".*slow.test.js"], capture_output=True, text=True).stdout.split():
            _time.sleep(0.2)
        p.send_signal(sig)
        try:
            p.wait(timeout=20)
        except subprocess.TimeoutExpired:
            p.kill()
            self.fail("the checker did not stop on the signal")
        finally:
            p.stdout.close()
            p.stderr.close()
        _time.sleep(0.5)
        return self.leftovers(priv)

    def test_sigterm_removes_the_scratch_clone_and_stops_the_suite(self):
        self.assertEqual(self.run_and_signal(signal.SIGTERM), ([], []))

    def test_sigint_removes_the_scratch_clone_and_stops_the_suite(self):
        self.assertEqual(self.run_and_signal(signal.SIGINT), ([], []))


class CacheIdentity(Base):
    def refactor_chain(self, fx=None, n=3):
        fx = fx or self.fx
        b = fx.base()
        for i in range(n):
            fx.commit("T-1: REFACTOR. step %d" % i, {"src/lib.js": "const f = (a, b) => a + b;\nexports.add = f; // %d\n" % i})
        return b

    def test_a_fresh_cache_reports_no_hits_and_counts_in_run_reuse_apart(self):
        b = self.refactor_chain()
        rc, commits, s = self.check(b)
        self.assertEqual((s["suite_runs"], s["cache_hits"], s.get("reused")), (4, 0, 2), s)

    def test_a_second_run_reads_every_result_from_the_cache_and_runs_nothing(self):
        b = self.refactor_chain()
        self.check(b)
        rc, commits, s = self.check(b)
        self.assertEqual((s["suite_runs"], s["cache_hits"], s.get("reused")), (0, 4, 2), s)

    def test_the_default_cache_directory_is_neither_read_nor_created_when_cache_is_given(self):
        b = self.refactor_chain(n=1)
        tmp = tempfile.mkdtemp(prefix="ctdd-tmp-")
        self.addCleanup(shutil.rmtree, tmp, True)
        old = tempfile.tempdir
        tempfile.tempdir = tmp
        try:
            self.check(b)
        finally:
            tempfile.tempdir = old
        self.assertEqual([n for n in os.listdir(tmp) if n.startswith("check-tdd-cache")], [])
        self.assertTrue([n for n in os.listdir(self.cache) if n.endswith(".json")])

    def test_two_repos_with_the_same_shas_do_not_share_results(self):
        env = {"GIT_AUTHOR_DATE": "2026-01-01T00:00:00Z", "GIT_COMMITTER_DATE": "2026-01-01T00:00:00Z"}
        old = {k: os.environ.get(k) for k in env}
        os.environ.update(env)
        try:
            b1 = self.refactor_chain()
            other = Fixture()
            self.addCleanup(other.cleanup)
            b2 = self.refactor_chain(other)
        finally:
            for k, v in old.items():
                if v is None:
                    os.environ.pop(k, None)
                else:
                    os.environ[k] = v
        self.assertEqual((b1, self.fx.run("rev-parse", "HEAD")), (b2, other.run("rev-parse", "HEAD")))
        self.check(b1)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ct.main([other.dir, b2 + "..HEAD", "--json", "--cache", self.cache])
        s = json.loads(out.getvalue())["summary"]
        self.assertEqual((s["suite_runs"], s["cache_hits"]), (4, 0), s)

    def test_cache_key_covers_the_repo_identity(self):
        class A: testbit = ""; make_var = []; no_compat = False
        self.assertNotEqual(ct.cache_key("node", "abc", A, "repo-one"), ct.cache_key("node", "abc", A, "repo-two"))

    def test_a_result_of_a_suite_that_did_not_run_is_not_cached(self):
        b = self.fx.commit("init", {"package.json": json.dumps({"name": "fx", "scripts": {}}), "src/lib.js": "exports.add = (a, b) => a + b;\n"})
        self.fx.commit("T-1: REFACTOR. x", {"src/lib.js": "const f = (a, b) => a + b;\nexports.add = f;\n"})
        rc, commits, s = self.check(b)
        self.assertEqual(commits[-1]["status"], "FAIL")
        self.assertEqual([n for n in os.listdir(self.cache) if n.endswith(".json")], [])


class PassRecords(Base):
    def record_path(self, sha):
        common = self.fx.run("rev-parse", "--path-format=absolute", "--git-common-dir").strip()
        return os.path.join(common, "check-tdd", "pass", sha + ".json")

    def has_pass(self, sha):
        with contextlib.redirect_stdout(io.StringIO()):
            return ct.main(["--has-pass", self.fx.dir, sha])

    def two_good_commits(self):
        b = self.fx.base()
        self.fx.commit("T-1: REFACTOR. add", {"src/lib.js": "const f = (a, b) => a + b;\nexports.add = f;\n"})
        return b, self.fx.commit("notes", {"README.md": "x\n"})

    def test_a_complete_passing_run_writes_a_record(self):
        b, head = self.two_good_commits()
        rc, _, _ = self.check(b)
        self.assertEqual(rc, 0)
        path = self.record_path(head)
        self.assertTrue(os.path.exists(path), path)
        with open(path) as f:
            rec = json.load(f)
        self.assertEqual((rec["head"], rec["base"], rec["range"], rec["adapter"], rec["commits"], rec["result"]),
                         (head, b, b + "..HEAD", "node", 2, "pass"))
        for key in ("checker_version", "checker_commit", "date"):
            self.assertTrue(rec.get(key), key)

    def test_has_pass_answers_from_the_record_alone(self):
        b, head = self.two_good_commits()
        self.assertEqual(self.has_pass(head), 1)
        self.check(b)
        self.assertEqual(self.has_pass(head), 0)
        self.assertEqual(self.has_pass(head[:8]), 0)
        self.assertEqual(self.has_pass(b), 1)
        self.assertEqual(self.has_pass("nosuchref"), 1)

    def test_a_failing_run_writes_no_record(self):
        b = self.fx.base()
        head = self.fx.commit("speed up add", {"src/lib.js": "exports.add = (a, b) => b + a;\n"})
        rc, _, _ = self.check(b)
        self.assertEqual(rc, 1)
        self.assertFalse(os.path.exists(self.record_path(head)))
        self.assertEqual(self.has_pass(head), 1)

    def test_an_incomplete_run_writes_no_record(self):
        b, head = self.two_good_commits()
        rc, _, _ = self.check(b, "--budget", "-1")
        self.assertEqual(rc, 3)
        self.assertFalse(os.path.exists(self.record_path(head)))


class GateRecord(unittest.TestCase):
    def test_forms(self):
        self.assertEqual(ct.read_gate('{"before": "fail", "after": "pass"}'), "fail->pass")
        self.assertEqual(ct.read_gate('{"parent": "FAIL", "commit": "PASS"}'), "fail->pass")
        self.assertEqual(ct.read_gate("x warm: fail -> pass"), "fail->pass")
        self.assertEqual(ct.read_gate("gate: FAILED => PASSED"), "fail->pass")
        self.assertNotEqual(ct.read_gate("pass -> pass"), "fail->pass")
        self.assertNotEqual(ct.read_gate('{"before": "pass", "after": "fail"}'), "fail->pass")


if __name__ == "__main__":
    unittest.main(verbosity=1)
