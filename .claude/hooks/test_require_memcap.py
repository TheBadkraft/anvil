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


# Regression table (FR-2611-anvl-tooling-001): every command with the runtime the hook must name, or None to allow.
# The hook is a guardrail against a slip, not a security boundary: `eval "$x"` and aliases are out of scope.
BASE = [
    ("node x.js", "node"), ("python3 run.py", "python3"), ("python run.py", "python"), ("python3.11 run.py", "python3.11"),
    ("npm test", "npm"), ("npx tsc", "npx"), ("make all", "make"), ("dotnet build", "dotnet"), ("/usr/bin/node x.js", "node"),
    ("cat f | node script.js", "node"), ("echo hi; python3 run.py", "python3"), ("ls && make all", "make"),
    ("false || npm test", "npm"), ("(cd x && npm test)", "npm"), ("taskset -c 6-7 node x.js", "node"), ("FOO=1 node x.js", "node"),
    ("nice -n 5 python3 x.py", "python3"), ("timeout 60 node x.js", "node"), ("env A=1 B=2 make all", "make"),
    ("time node x.js", "node"), ("nohup node x.js", "node"), ("sudo make install", "make"), ("exec python3 x.py", "python3"),
    ("! node x.js", "node"), ("{ node x.js; }", "node"), ("if true; then node x.js; fi", "node"),
    ("for f in a b; do python3 $f.py; done", "python3"), ("while true; do make all; done", "make"),
    ('echo "a|b" | node script.js', "node"), ('echo "a;b"; python3 run.py', "python3"),
    ("node -e '1'", None), ('python3 -c "print(1)"', None), ("node --version", None), ("python3 -", None), ("make", None), ("npm -v", None),
    ("ls -l", None), ("git status", None), ("echo node x.js", None), ("cat node.js", None), ("grep -r node src", None),
    ('grep -E "a|node --test" file', None), ("grep -E 'a|python3 run.py' file", None), ('echo "x; node app.js"', None),
    ("echo 'a && python3 run.py'", None), ('git commit -m "tidy; python3 adapter notes"', None),
    ("systemd-run --user --scope -q -p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=60 -- node x.js", None),
    ("systemd-run --user --scope -q -p MemoryMax=4G -- taskset -c 6-7 python3 y.py", None),
    ("cat <<'EOF'\nyou run it with node x.js\nEOF", None),
]
SUBST = [
    ('echo "$(node x.js)"', "node"), ('x="$(python3 y.py)"', "python3"), ('x="$(make all)"', "make"), ("echo $(npm test)", "npm"),
    ("x=$(node y.js)", "node"), ("echo `node x.js`", "node"), ('x="`python3 y.py`"', "python3"), ("x=`make all`", "make"),
    ('echo "a $(echo "$(node x.js)") b"', "node"), ('echo "$(cat f | python3 y.py)"', "python3"), ('f() { echo "$(node x.js)"; }', "node"),
    ('git commit -m "$(npm test)"', "npm"), ('echo "$(ls)"', None), ('x="$(git rev-parse HEAD)"', None),
    ("echo '$(node x.js)'", None), ('echo "\\$(node x.js)"', None), ("echo '`node x.js`'", None), ('echo "$(node -e 1)"', None),
    ('echo "$(date)" "$(whoami)"', None), ("x=$(ls | wc -l)", None),
]
SAME_SEGMENT = [
    ('echo "systemd-run MemoryMax=4G"; node x.js', "node"), ("systemd-run --scope -- node x.js", "node"),
    ("systemd-run -p MemoryMax=4G -- true && node x.js", "node"), ("node x.js # systemd-run -p MemoryMax=4G", "node"),
    ("systemd-run -p MemoryMax=4G -- node a.js; node b.js", "node"), ("echo MemoryMax systemd-run | node x.js", "node"),
    ("systemd-run --user -p RuntimeMaxSec=9 -- python3 y.py", "python3"), ("MemoryMax=4G node x.js", "node"),
    ("systemd-run -p MemoryMax=4G -- node x.js", None), ("systemd-run --property=MemoryMax=2G --scope -- make all", None),
    ("systemd-run --user --scope -q -p MemoryMax=4G -p MemorySwapMax=0 -- taskset -c 6-7 npm test", None),
]
SHELL_STRINGS = [
    ('bash -c "node x.js"', "node"), ("sh -c 'python3 y.py'", "python3"), ('bash -lc "make all"', "make"), ('eval "node x.js"', "node"),
    ("eval node x.js", "node"), ("xargs node", "node"), ("echo a | xargs -n1 python3 y.py", "python3"), ("xargs -I{} node {}", "node"),
    ("find . -name x -exec node {} \\;", "node"), ("find . -exec python3 y.py {} +", "python3"), ('bash -c "echo $(node x.js)"', "node"),
    ("sudo bash -c 'make all'", "make"), ('env X=1 bash -c "npm test"', "npm"), ("time bash -c 'node x.js'", "node"),
    ("bash -c 'bash -c \"node x.js\"'", "node"), ("find . -execdir make {} \\;", "make"),
    ('bash -c "echo hi"', None), ("bash script.sh", None), ("xargs echo", None), ("find . -name x -exec ls {} \\;", None),
    ('eval "ls"', None), ("bash -c 'node -e 1'", None), ("xargs node -e 1", None), ('systemd-run -p MemoryMax=1G -- bash -c "node x.js"', None),
    ("xargs -n1 systemd-run -p MemoryMax=1G -- node", None), ("find . -type f | xargs grep node", None),
]


class RegressionTable(unittest.TestCase):
    def table(self, rows):
        got = {c: hook.heavy_use(c) for c, _ in rows}
        self.assertEqual(got, dict(rows))

    def test_the_corpus_has_at_least_eighty_commands(self):
        self.assertGreaterEqual(len(BASE) + len(SUBST) + len(SAME_SEGMENT) + len(SHELL_STRINGS), 80)

    def test_base_behaviour(self):
        self.table(BASE)

    def test_substitutions_in_and_out_of_double_quotes(self):
        self.table(SUBST)

    def test_the_cap_exempts_only_the_command_it_wraps(self):
        self.table(SAME_SEGMENT)

    def test_shell_strings_xargs_and_find_exec_are_commands(self):
        self.table(SHELL_STRINGS)


if __name__ == "__main__":
    unittest.main(verbosity=1)
