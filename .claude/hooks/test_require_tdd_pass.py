#!/usr/bin/env python3
"""Tests for require-tdd-pass.py (the merge guard). Run (capped, STANDARDS section 8):
  systemd-run --user --scope -q -p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=300 -- \
      taskset -c 6-7 python3 .claude/hooks/test_require_tdd_pass.py
"""
import importlib.util
import json
import os
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


if __name__ == "__main__":
    unittest.main(verbosity=1)
