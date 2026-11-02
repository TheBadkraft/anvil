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
import subprocess
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
        self.fx.commit("T-1: GREEN. ops [tests: ops.test.js]", {"src/lib.js": FULL})
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
        self.fx.commit("T-1: GREEN. all [tests: ops.test.js]", {"src/lib.js": FULL})
        self.fx.commit("T-1: GREEN. add a case [tests: lib.test.js > add]", {
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

    def test_no_compat_skips_it(self):
        b = self.compat_repo()
        self.fx.commit("T-1: RED. x", {"test/compat/x.test.js": "const test = require('node:test'); test('x', () => { require('assert').strictEqual(1, 2); });\n"})
        self.assertFails(b, "no test newly fails", "--no-compat")


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

    def test_explicit_name_wins(self):
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
