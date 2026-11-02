#!/usr/bin/env python3
"""Tests for require-memcap.py. Run (capped, STANDARDS section 8):
  systemd-run --user --scope -q -p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=120 -- \
      taskset -c 6-7 python3 .claude/hooks/test_require_memcap.py
"""
import importlib.util
import json
import os
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
HOOK = os.path.join(HERE, "require-memcap.py")
spec = importlib.util.spec_from_file_location("require_memcap", HOOK)
hook = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hook)


def run_hook(command):
    p = subprocess.run([sys.executable, HOOK], input=json.dumps({"tool_input": {"command": command}}), capture_output=True, text=True)
    return json.loads(p.stdout) if p.stdout.strip() else None


class QuoteAwareSplitting(unittest.TestCase):
    def test_a_pipe_inside_a_quoted_pattern_is_not_a_separator(self):
        self.assertIsNone(hook.heavy_use('grep -E "a|node --test" file'))
        self.assertIsNone(hook.heavy_use("grep -E 'a|python3 run.py' file"))

    def test_a_semicolon_or_ampersands_inside_quotes_are_not_separators(self):
        self.assertIsNone(hook.heavy_use('echo "x; node app.js"'))
        self.assertIsNone(hook.heavy_use("echo 'a && python3 run.py'"))
        self.assertIsNone(hook.heavy_use('git commit -m "tidy; python3 adapter notes"'))

    def test_a_real_separator_after_a_quoted_one_still_splits(self):
        self.assertEqual(hook.heavy_use('echo "a|b" | node script.js'), "node")
        self.assertEqual(hook.heavy_use('echo "a;b"; python3 run.py'), "python3")


class UnquotedBehaviourIsKept(unittest.TestCase):
    def test_heavy_commands_are_found_after_separators(self):
        self.assertEqual(hook.heavy_use("cat f | node script.js"), "node")
        self.assertEqual(hook.heavy_use("echo hi; python3 run.py"), "python3")
        self.assertEqual(hook.heavy_use("ls && make all"), "make")
        self.assertEqual(hook.heavy_use("(cd x && npm test)"), "npm")
        self.assertEqual(hook.heavy_use("taskset -c 6-7 node x.js"), "node")
        self.assertEqual(hook.heavy_use("FOO=1 node x.js"), "node")

    def test_inline_checks_and_version_queries_are_allowed(self):
        for c in ("node -e '1'", 'python3 -c "print(1)"', "node --version", "python3 -", "make"):
            self.assertIsNone(hook.heavy_use(c), c)

    def test_heredoc_body_lines_only_count_when_they_start_with_a_runtime(self):
        self.assertIsNone(hook.heavy_use("cat <<'EOF'\nyou run it with node x.js\nEOF"))


class HookOutput(unittest.TestCase):
    def test_denies_an_uncapped_runtime_with_the_cap_command(self):
        out = run_hook("node bench/x.js")
        self.assertEqual(out["hookSpecificOutput"]["permissionDecision"], "deny")
        self.assertIn("systemd-run", out["hookSpecificOutput"]["permissionDecisionReason"])

    def test_allows_a_capped_runtime_and_a_quoted_mention(self):
        self.assertIsNone(run_hook("systemd-run --user --scope -q -p MemoryMax=4G -- node bench/x.js"))
        self.assertIsNone(run_hook('grep -E "a|node --test" notes.md'))


if __name__ == "__main__":
    unittest.main(verbosity=1)
