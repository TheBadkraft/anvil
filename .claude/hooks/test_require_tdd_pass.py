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


class MergeTargetsMoreSlips(unittest.TestCase):
    def targets(self, command, cwd="/w"):
        return hook.merge_targets(command, cwd)

    def test_wrapper_options_with_values_are_skipped(self):
        for c in ("sudo -u bob git merge feat/x", "timeout -s KILL 5 git merge feat/x", "timeout 5 git merge feat/x",
                  "env -u X git merge feat/x", "env -i git merge feat/x", "taskset -c 0,1 git merge feat/x",
                  "taskset 0x1 git merge feat/x", "nice -n 5 git merge feat/x", "ionice -c 2 -n 4 git merge feat/x",
                  "chrt 10 git merge feat/x", "systemd-run --user -p MemoryMax=1G -- git merge feat/x"):
            self.assertEqual(self.targets(c), [("/w", "feat/x")], c)

    def test_a_branch_made_in_the_same_command_is_followed_to_its_start(self):
        for c in ("git branch tmp feat/x; git merge tmp", "git tag t feat/x && git merge t", "git checkout -b tmp feat/x && git merge tmp",
                  "git switch -c tmp feat/x; git merge tmp"):
            self.assertIn(("/w", "feat/x"), self.targets(c), c)

    def test_a_computed_ref_is_an_unknown_target(self):
        for c in ("git merge $(git rev-parse feat/x)", "git merge `git rev-parse feat/x`", 'git merge "$(cat ref)"',
                  "git pull origin $(echo feat/x)"):
            self.assertTrue([t for t in self.targets(c) if len(t) == 3 and t[2] == "unknown"], c)
        self.assertEqual([t for t in self.targets('git merge -m "$(date)" feat/x') if len(t) == 3], [])

    def test_a_computed_directory_does_not_hide_the_merge(self):
        self.assertIn(("/w", "feat/x"), self.targets('cd "$(git rev-parse --show-toplevel)" && git merge feat/x'))
        self.assertIn(("/w", "feat/x"), self.targets('git -C "$(pwd)" merge feat/x'))


class MergeTargetsPipes(unittest.TestCase):
    """A command given to a shell on its standard input, or a ref given to xargs."""

    def targets(self, command, cwd="/w"):
        return hook.merge_targets(command, cwd)

    def test_text_piped_into_a_shell_is_a_command_line(self):
        for c in ("printf 'git merge feat/x' | sh", 'echo "git merge feat/x" | bash', "echo 'git merge feat/x' | sh -s",
                  "echo git merge feat/x | bash -e"):
            self.assertEqual(self.targets(c), [("/w", "feat/x")], c)

    def test_a_herestring_or_heredoc_into_a_shell_is_a_command_line(self):
        for c in ("bash <<< 'git merge feat/x'", 'sh <<<"git merge feat/x"', "bash <<'EOF'\ngit merge feat/x\nEOF"):
            self.assertEqual(self.targets(c), [("/w", "feat/x")], c)

    def test_xargs_git_merge_takes_its_refs_from_the_pipe(self):
        self.assertEqual(self.targets("echo feat/x | xargs git merge"), [("/w", "feat/x")])
        self.assertEqual(self.targets("echo feat/x fix/y | xargs -n1 git merge --no-ff"), [("/w", "feat/x"), ("/w", "fix/y")])
        unknown = self.targets('git branch --list "feat/*" | xargs git merge')
        self.assertEqual([t[2] for t in unknown if len(t) == 3], ["unknown"])
        self.assertEqual(self.targets("xargs git merge feat/x"), [("/w", "feat/x")])

    def test_harmless_pipes_name_nothing(self):
        for c in ("printf 'hello' | sh", "echo feat/x | xargs echo", "echo 'git merge feat/x' | cat", "ls | xargs git status", "echo hi | bash script.sh"):
            self.assertEqual(self.targets(c), [], c)


class MergeTargetsSlips(unittest.TestCase):
    """The plausible slips: a wrapper word, a shell string, a substitution, --git-dir, a pull."""

    def targets(self, command, cwd="/w"):
        return hook.merge_targets(command, cwd)

    def test_wrapper_words_keywords_and_an_absolute_git_path(self):
        for c in ("env GIT_X=1 git merge feat/x", "command git merge feat/x", "time git merge feat/x", "nohup git merge feat/x",
                  "sudo git merge feat/x", "exec git merge feat/x", "/usr/bin/git merge feat/x", "! git merge feat/x",
                  "{ git merge feat/x; }", "if true; then git merge feat/x; fi", "for i in 1; do git merge feat/x; done",
                  "while false; do git merge feat/x; done", "FOO=1 git merge feat/x"):
            self.assertEqual(self.targets(c), [("/w", "feat/x")], c)

    def test_shell_strings_are_parsed_as_commands(self):
        self.assertEqual(self.targets('bash -c "git merge feat/x"'), [("/w", "feat/x")])
        self.assertEqual(self.targets("sh -c 'cd /r && git merge feat/x'"), [("/r", "feat/x")])
        self.assertEqual(self.targets('eval "git merge fix/y"'), [("/w", "fix/y")])

    def test_substitutions_and_backticks_are_commands(self):
        self.assertEqual(self.targets('echo "$(git merge feat/x)"'), [("/w", "feat/x")])
        self.assertEqual(self.targets("x=`git merge feat/x`"), [("/w", "feat/x")])
        self.assertEqual(self.targets("echo '$(git merge feat/x)'"), [])

    def test_git_dir_and_work_tree_options(self):
        self.assertEqual(self.targets("git --git-dir=/r/.git merge feat/x"), [("/r", "feat/x")])
        self.assertEqual(self.targets("git --git-dir /r/.git merge feat/x"), [("/r", "feat/x")])
        self.assertEqual(self.targets("git --work-tree=/t merge feat/x"), [("/t", "feat/x")])

    def test_a_pull_of_a_branch_is_a_merge_of_it(self):
        self.assertEqual(self.targets("git pull origin feat/x"), [("/w", "origin/feat/x", "pull")])
        self.assertEqual(self.targets("git pull . fix/y"), [("/w", "fix/y", "pull")])
        self.assertEqual(self.targets("git pull --no-rebase -s ours origin chore/z"), [("/w", "origin/chore/z", "pull")])
        self.assertEqual(self.targets("git pull origin feat/a:main"), [("/w", "origin/feat/a", "pull")])
        self.assertEqual(self.targets("git pull"), [])
        self.assertEqual(self.targets("git pull origin"), [])


class Guarded(unittest.TestCase):
    def test_other_spellings_of_a_branch(self):
        for r in ("heads/feat/x", "upstream/feat/x", "refs/remotes/upstream/fix/a", "remotes/origin/chore/d"):
            self.assertTrue(hook.is_guarded(r), r)
        for r in ("upstream/release/v1", "refs/remotes/upstream/docs/a", "refs/tags/v1"):
            self.assertFalse(hook.is_guarded(r), r)

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
        self.init = self.sh("rev-parse", "main")
        self.common = os.path.join(self.repo, ".git")

    def record(self, sha=None, **over):
        sha = sha or self.sha
        d = os.path.join(self.common, "check-tdd", "pass")
        os.makedirs(d, exist_ok=True)
        rec = {"head": sha, "result": "pass", "base": self.init,
               "options": {"lenient_tests": False, "batch_limit": 10, "max_green_lines": 150, "no_compat": False, "first_parent": False}, "adapter": "python"}
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

    def test_a_sha_or_a_tag_at_the_tip_of_a_guarded_branch_is_guarded(self):
        self.sh("tag", "v-tip", self.sha)
        for c in ("git merge %s" % self.sha, "git merge %s" % self.sha[:10], "git merge v-tip"):
            self.assertEqual(self.decide(c)[0], "deny", c)
        self.assertEqual(self.decide("git merge %s" % self.sh("rev-parse", "release/v1"))[0], "allow")
        self.assertEqual(self.decide("git merge %s" % self.init)[0], "allow")

    def test_other_remotes_and_spellings_resolve_to_the_branch(self):
        self.sh("update-ref", "refs/remotes/upstream/feat/x", self.sha)
        for c in ("git merge upstream/feat/x", "git merge heads/feat/x", "git merge refs/remotes/upstream/feat/x"):
            self.assertEqual(self.decide(c)[0], "deny", c)

    def test_plausible_slips_are_denied(self):
        for c in ("env A=1 git merge feat/x", "sudo git merge feat/x", "/usr/bin/git merge feat/x", "! git merge feat/x",
                  "if true; then git merge feat/x; fi", 'bash -c "git merge feat/x"', 'eval "git merge feat/x"',
                  'echo "$(git merge feat/x)"', "x=`git merge feat/x`", 'git "merge" feat/x', "git m\\erge feat/x"):
            self.assertEqual(self.decide(c)[0], "deny", c)
        self.assertEqual(self.decide("git --git-dir=%s/.git merge feat/x" % self.repo, "/")[0], "deny")

    def test_a_pull_of_a_guarded_branch_is_guarded(self):
        for c in ("git pull . feat/x", "git pull origin fix/r", "git pull --no-rebase . feat/x"):
            self.assertEqual(self.decide(c)[0], "deny", c)
        v, out, _ = self.decide("git pull origin feat/never-fetched")
        self.assertEqual(v, "deny")
        self.assertIn("fetch", out["hookSpecificOutput"]["permissionDecisionReason"])
        for c in ("git pull origin main", "git pull", "git pull . release/v1", "git pull origin release/v1"):
            self.assertEqual(self.decide(c)[0], "allow", c)

    def test_a_record_must_cover_what_the_merge_brings_in(self):
        self.record(base=self.sha)  # a HEAD~1..HEAD run: the branch's other commits were never checked
        self.assertEqual(self.decide("git merge feat/x")[0], "deny")
        self.record(base=self.sh("rev-parse", "fix/r"))
        self.assertEqual(self.decide("git merge feat/x")[0], "deny")
        self.record(base=None)
        self.assertEqual(self.decide("git merge feat/x")[0], "deny")
        self.record(base=self.init)
        self.assertEqual(self.decide("git merge feat/x")[0], "allow")

    def test_a_merge_given_to_a_shell_on_its_input_is_judged(self):
        for c in ("printf 'git merge feat/x' | sh", "bash <<< 'git merge feat/x'", "echo feat/x | xargs git merge",
                  'git branch --list "feat/*" | xargs git merge'):
            self.assertEqual(self.decide(c)[0], "deny", c)
        self.record()
        self.assertEqual(self.decide("printf 'git merge feat/x' | sh")[0], "allow")

    def test_a_first_parent_record_is_refused(self):
        good = {"lenient_tests": False, "batch_limit": 10, "max_green_lines": 150, "no_compat": False}
        self.record(options=dict(good, first_parent=True))
        v, out, _ = self.decide("git merge feat/x")
        self.assertEqual(v, "deny")
        self.assertIn("first_parent", out["hookSpecificOutput"]["permissionDecisionReason"])
        self.record(options=good)  # an older checker did not record it
        self.assertEqual(self.decide("git merge feat/x")[0], "deny")

    def test_a_record_for_the_wrong_adapter_is_refused(self):
        self.sh("checkout", "-q", "-b", "feat/node", "main")
        with open(os.path.join(self.repo, "package.json"), "w") as f:
            f.write("{}")
        self.sh("add", "package.json")
        self.sh("commit", "-q", "-m", "pkg")
        node_sha = self.sh("rev-parse", "HEAD")
        self.sh("checkout", "-q", "main")
        self.record(node_sha, adapter="python")
        v, out, _ = self.decide("git merge feat/node")
        self.assertEqual(v, "deny")
        self.assertIn("adapter", out["hookSpecificOutput"]["permissionDecisionReason"])
        self.record(node_sha, adapter="node")
        self.assertEqual(self.decide("git merge feat/node")[0], "allow")

    def test_any_name_at_the_tip_of_a_guarded_branch_is_guarded(self):
        self.sh("branch", "tmp", self.sha)
        self.sh("update-ref", "refs/remotes/origin/zzz", self.sha)
        for c in ("git merge tmp", "git merge origin/zzz", "git merge refs/heads/tmp"):
            self.assertEqual(self.decide(c)[0], "deny", c)
        for c in ("git merge main", "git merge HEAD", "git merge release/v1", "git merge docs/x"):
            self.assertEqual(self.decide(c)[0], "allow", c)

    def test_a_branch_made_in_the_same_command_does_not_hide_the_tip(self):
        for c in ("git branch tmp feat/x; git merge tmp", "git checkout -b tmp feat/x && git merge tmp"):
            self.assertEqual(self.decide(c)[0], "deny", c)

    def test_a_computed_ref_is_refused_with_the_instruction_to_pass_it_explicitly(self):
        for c in ("git merge $(git rev-parse feat/x)", "git merge `git rev-parse feat/x`", "git pull origin $(echo feat/x)"):
            v, out, _ = self.decide(c)
            self.assertEqual(v, "deny", c)
            self.assertIn("pass the ref explicitly", out["hookSpecificOutput"]["permissionDecisionReason"])
        self.record()
        self.assertEqual(self.decide('git merge -m "$(date)" feat/x')[0], "allow")

    def test_a_record_made_with_loosened_options_is_refused(self):
        good = {"lenient_tests": False, "batch_limit": 10, "max_green_lines": 150, "no_compat": False, "first_parent": False}
        for bad in ({"lenient_tests": True}, {"batch_limit": 11}, {"max_green_lines": 151}, {"no_compat": True}):
            self.record(options=dict(good, **bad))
            v, out, _ = self.decide("git merge feat/x")
            self.assertEqual(v, "deny", bad)
            self.assertIn("options", out["hookSpecificOutput"]["permissionDecisionReason"])
        self.record(options=None)
        self.assertEqual(self.decide("git merge feat/x")[0], "deny")
        self.record(options=dict(good, batch_limit=5, max_green_lines=100))
        self.assertEqual(self.decide("git merge feat/x")[0], "allow")

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
