#!/usr/bin/env python3
"""Tests for require-tdd-pass.py (the merge guard). Run (capped, STANDARDS section 8):
  systemd-run --user --scope -q -p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=300 -- \
      taskset -c 6-7 python3 .claude/hooks/test_require_tdd_pass.py
"""
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
HOOK = os.path.join(HERE, "require-tdd-pass.py")
spec = importlib.util.spec_from_file_location("require_tdd_pass", HOOK)
hook = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hook)


class SplitSegments(unittest.TestCase):
    def test_separators_outside_quotes_split(self):
        self.assertEqual([s.strip() for s in hook.split_segments("cd x && git merge a; ls | wc")], ["cd x", "git merge a", "ls", "wc"])

    def test_separators_inside_quotes_do_not_split(self):
        self.assertEqual([s.strip() for s in hook.split_segments('grep -E "a|git merge feat/x" f')], ['grep -E "a|git merge feat/x" f'])
        self.assertEqual([s.strip() for s in hook.split_segments("echo 'a; b' && true")], ["echo 'a; b'", "true"])


class MergeTargets(unittest.TestCase):
    def targets(self, command, cwd="/w"):
        return hook.merge_targets(command, cwd)

    def test_plain_merge_uses_the_cwd(self):
        self.assertEqual(self.targets("git merge feat/x"), [("/w", "feat/x")])

    def test_git_dash_C_directory_absolute_and_relative(self):
        self.assertEqual(self.targets("git -C /r merge fix/y"), [("/r", "fix/y")])
        self.assertEqual(self.targets("git -C sub merge fix/y"), [("/w/sub", "fix/y")])

    def test_a_preceding_cd_sets_the_directory(self):
        self.assertEqual(self.targets("cd /r && git merge tools/z"), [("/r", "tools/z")])
        self.assertEqual(self.targets("cd sub && git merge origin/chore/q"), [("/w/sub", "origin/chore/q")])
        self.assertEqual(self.targets("cd /r; git -C inner merge perf/p"), [("/r/inner", "perf/p")])

    def test_options_and_their_values_are_skipped(self):
        self.assertEqual(self.targets('git merge --no-ff -m "msg; with | stuff" feat/x'), [("/w", "feat/x")])
        self.assertEqual(self.targets("git merge -s ours -X theirs feat/a fix/b"), [("/w", "feat/a"), ("/w", "fix/b")])
        self.assertEqual(self.targets("git -c core.editor=true merge --no-edit feat/x"), [("/w", "feat/x")])

    def test_abort_continue_and_no_ref_name_nothing(self):
        for c in ("git merge --abort", "git merge --continue", "git merge --quit", "git merge"):
            self.assertEqual(self.targets(c), [], c)

    def test_other_branches_are_still_returned(self):
        self.assertEqual(self.targets("git merge --ff-only release/v0.13.1-rc"), [("/w", "release/v0.13.1-rc")])

    def test_quoted_or_piped_mentions_are_not_merges(self):
        for c in ('echo "git merge feat/x"', 'grep -E "a|git merge feat/x" f', "git log --oneline | head", "git status", "ls"):
            self.assertEqual(self.targets(c), [], c)


class Guarded(unittest.TestCase):
    def test_guarded_prefixes_with_or_without_origin(self):
        for r in ("feat/x", "origin/feat/x", "fix/a", "tools/b", "perf/c", "chore/d", "origin/chore/d", "refs/heads/feat/x", "refs/remotes/origin/fix/a"):
            self.assertTrue(hook.is_guarded(r), r)

    def test_exempt_and_unrelated_names(self):
        for r in ("release/v1", "origin/release/v1", "integration/v0.13-native-fixes", "docs/x", "main", "origin/main", "feat", "feature/x", "a1b2c3d"):
            self.assertFalse(hook.is_guarded(r), r)


def run_hook(command, cwd, raw=None):
    payload = raw if raw is not None else json.dumps({"tool_input": {"command": command}, "cwd": cwd})
    p = subprocess.run([sys.executable, HOOK], input=payload, capture_output=True, text=True)
    out = json.loads(p.stdout) if p.stdout.strip() else None
    return p.returncode, out, p.stderr


def verdict(out):
    return out["hookSpecificOutput"]["permissionDecision"] if out and "permissionDecision" in out.get("hookSpecificOutput", {}) else "allow"


def reason(out):
    return json.dumps(out) if out else ""


class Decision(unittest.TestCase):
    def sh(self, *a):
        return subprocess.run(["git", "-C", self.repo, *a], check=True, capture_output=True, text=True).stdout.strip()

    def setUp(self):
        self.repo = os.path.realpath(tempfile.mkdtemp(prefix="ctdd-hook-"))
        self.addCleanup(shutil.rmtree, self.repo, True)
        self.sh("init", "-q", "-b", "main")
        for k, v in (("user.email", "t@example.org"), ("user.name", "t"), ("commit.gpgsign", "false")):
            self.sh("config", k, v)
        self.sh("commit", "-q", "--allow-empty", "-m", "init")
        for b in ("feat/x", "release/v1", "docs/x", "integration/i", "fix/r"):
            self.sh("checkout", "-q", "-b", b, "main")
            self.sh("commit", "-q", "--allow-empty", "-m", b)
        self.sh("checkout", "-q", "main")
        self.sh("update-ref", "refs/remotes/origin/fix/r", self.sh("rev-parse", "fix/r"))
        self.sha = self.sh("rev-parse", "feat/x")
        self.common = os.path.join(self.repo, ".git")

    def record(self, sha=None, **over):
        sha = sha or self.sha
        d = os.path.join(self.common, "check-tdd", "pass")
        os.makedirs(d, exist_ok=True)
        rec = {"head": sha, "result": "pass"}
        rec.update(over)
        with open(os.path.join(d, sha + ".json"), "w") as f:
            json.dump(rec, f)

    def decide(self, command, cwd=None):
        rc, out, err = run_hook(command, cwd or self.repo)
        self.assertEqual(rc, 0, err)
        return verdict(out), out, err

    def test_denied_without_a_record_naming_the_command_and_the_override(self):
        v, out, _ = self.decide("git merge feat/x")
        self.assertEqual(v, "deny")
        text = out["hookSpecificOutput"]["permissionDecisionReason"]
        self.assertRegex(text, r"check-tdd\.py %s \S+\.\.%s" % (re.escape(self.repo), self.sha))
        self.assertIn(os.path.join(self.common, "check-tdd", "override", self.sha), text)
        self.assertIn("owner", text)

    def test_allowed_with_a_pass_record_for_that_sha(self):
        self.record()
        self.assertEqual(self.decide("git merge feat/x")[0], "allow")

    def test_a_record_for_another_sha_or_a_failed_record_does_not_count(self):
        self.record(self.sh("rev-parse", "fix/r"))
        self.assertEqual(self.decide("git merge feat/x")[0], "deny")
        self.record(result="fail")
        self.assertEqual(self.decide("git merge feat/x")[0], "deny")
        self.record(head="0" * 40)
        self.assertEqual(self.decide("git merge feat/x")[0], "deny")

    def test_an_override_file_lets_it_through_and_says_so(self):
        d = os.path.join(self.common, "check-tdd", "override")
        os.makedirs(d)
        open(os.path.join(d, self.sha), "w").close()
        v, out, err = self.decide("git merge feat/x")
        self.assertEqual(v, "allow")
        self.assertIn("overridden", reason(out) + err)

    def test_exempt_branches_and_unresolvable_refs_and_aborts_pass(self):
        for c in ("git merge release/v1", "git merge --ff-only release/v1", "git merge docs/x", "git merge integration/i",
                  "git merge feat/missing", "git merge --abort", "git merge --continue", "git merge main"):
            self.assertEqual(self.decide(c)[0], "allow", c)

    def test_directory_forms_and_remote_names_are_guarded(self):
        for c in ("git -C %s merge feat/x" % self.repo, "cd %s && git merge feat/x" % self.repo, "git merge --no-ff -m 'ship it; now' feat/x",
                  "git merge --ff-only feat/x", "git merge origin/fix/r"):
            cwd = "/" if "-C" in c or "cd " in c else self.repo
            self.assertEqual(self.decide(c, cwd)[0], "deny", c)

    def test_a_quoted_mention_is_not_a_merge(self):
        for c in ('grep -E "a|git merge feat/x" notes.md', 'echo "git merge feat/x"', "git log feat/x", "ls"):
            self.assertEqual(self.decide(c)[0], "allow", c)

    def test_outside_a_repository_nothing_resolves(self):
        d = tempfile.mkdtemp(prefix="ctdd-nogit-")
        self.addCleanup(shutil.rmtree, d, True)
        self.assertEqual(self.decide("git merge feat/x", d)[0], "allow")

    def test_internal_errors_fail_open_with_a_note(self):
        rc, out, err = run_hook(None, None, raw="not json")
        self.assertEqual((rc, out), (0, None))
        rc, out, err = run_hook(None, None, raw=json.dumps({"tool_input": {"command": 5}, "cwd": self.repo}))
        self.assertEqual((rc, out), (0, None))
        self.assertIn("check-tdd hook error", err)


if __name__ == "__main__":
    unittest.main(verbosity=1)
