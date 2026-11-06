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
        for c in ("node -e '1'", 'python3 -c "print(1)"', "node --version", "python3 -", "make --version", "make -n all"):
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
    ("node -e '1'", None), ('python3 -c "print(1)"', None), ("node --version", None), ("python3 -", None), ("npm -v", None),
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
    ('bash -c "echo hi"', None), ("xargs echo", None), ("find . -name x -exec ls {} \\;", None),
    ('eval "ls"', None), ("bash -c 'node -e 1'", None), ("xargs node -e 1", None), ('systemd-run -p MemoryMax=1G -- bash -c "node x.js"', None),
    ("xargs -n1 systemd-run -p MemoryMax=1G -- node", None), ("find . -type f | xargs grep node", None),
]


WRAPPER_OPTIONS = [
    ("sudo -u bob node x.js", "node"), ("sudo -u bob -g grp make all", "make"), ("sudo -E -u bob python3 x.py", "python3"),
    ("timeout -s KILL 5 node x.js", "node"), ("timeout 5 python3 x.py", "python3"), ("timeout --signal=KILL 5 node x.js", "node"),
    ("timeout -k 1 5s make all", "make"), ("env -u FOO make all", "make"), ("env -i node x.js", "node"), ("env -i A=1 node x.js", "node"),
    ("taskset 0x3 node x.js", "node"), ("taskset 3 python3 x.py", "python3"), ("taskset -c 6-7 node x.js", "node"),
    ("taskset -c 0,1 python3 x.py", "python3"), ("nice -n 5 node x.js", "node"), ("nice -5 make all", "make"),
    ("ionice -c 2 -n 4 node x.js", "node"), ("ionice -c3 python3 x.py", "python3"), ("chrt 10 node x.js", "node"),
    ("chrt -f 10 node x.js", "node"), ("stdbuf -oL node x.js", "node"), ("nohup nice -n 5 node x.js", "node"),
    ("sudo -u bob ls", None), ("timeout -s KILL 5 ls", None), ("env -u FOO ls", None), ("taskset -c 6-7 ls", None),
    ("nice -n 5 ls", None), ("sudo -u bob node -e 1", None), ("chrt 10 ls", None),
    ("systemd-run -p MemoryMax=1G -- sudo -u bob node x.js", None),
]
SCRIPTS = [
    ("bash script.sh", "bash"), ("sh script.sh", "sh"), ("./script.sh", "script.sh"), ("scripts/check-dump-valgrind.sh", "check-dump-valgrind.sh"),
    ("bash -x script.sh", "bash"), ("sudo bash script.sh", "bash"), ("sh -e ./a.sh arg", "sh"), ("env X=1 ./run.sh", "run.sh"),
    ("/abs/path/x.sh", "x.sh"), ("bash scripts/x.sh && true", "bash"), ("ls && ./build.sh", "build.sh"), ("bash -o pipefail x.sh", "bash"),
    ("bash -n script.sh", None), ("bash --version", None), ("bash", None), ("sh -c 'echo hi'", None), ("ls script.sh", None),
    ("cat x.sh", None), ("echo ./x.sh", None), ("grep foo x.sh", None), ("systemd-run -p MemoryMax=1G -- bash script.sh", None),
    ("systemd-run -p MemoryMax=1G -- ./x.sh", None),
]
CAP_FORMS = [
    ("systemd-run --description=MemoryMax -- node x.js", "node"), ('systemd-run --description "MemoryMax=4G" -- node x.js', "node"),
    ("systemd-run -E MemoryMax=1 -- node x.js", "node"), ("systemd-run -p MemorySwapMax=0 -- node x.js", "node"),
    ("systemd-run -p RuntimeMaxSec=MemoryMax -- node x.js", "node"),
    ("systemd-run --property=MemoryMax=4G -- node x.js", None), ("systemd-run -pMemoryMax=4G -- node x.js", None),
    ("systemd-run --property MemoryMax=4G -- node x.js", None), ('systemd-run -p "MemoryMax=4G" -- node x.js', None),
]

GRADLE = [
    ("gradle build", "gradle"), ("./gradlew test", "gradlew"), ("gradlew build", "gradlew"), ("/opt/g/bin/gradle assemble", "gradle"),
    ("gradle -q build", "gradle"), ("gradle -p java test", "gradle"), ("cd java && ./gradlew --no-daemon test", "gradlew"),
    ("timeout 60 gradle build", "gradle"), ("bash -c './gradlew test'", "gradlew"),
    ("gradle --version", None), ("./gradlew -v", None), ("gradle --help", None), ("gradle --stop", None), ("ls gradle", None),
    ("echo gradle build", None), ("systemd-run -p MemoryMax=4G -- ./gradlew test", None), ("cat gradlew", None),
]
# A bare `make` runs the default target, which builds: it is heavy. `make -n`, `--version` and the like stay allowed.
BARE_MAKE = [("make", "make"), ("ls && make", "make"), ("taskset -c 6-7 make", "make"), ("make -j8", "make"),
             ("make --version", None), ("make -n", None), ("make -q all", None), ("systemd-run -p MemoryMax=1G -- make", None)]
NO_CAP = [
    ("systemd-run -p MemoryMax=0 -- node x.js", "node"), ("systemd-run -p MemoryMax=infinity -- node x.js", "node"),
    ("systemd-run --property=MemoryMax=infinity -- python3 y.py", "python3"), ("systemd-run --property MemoryMax=0 -- make all", "make"),
    ("systemd-run -pMemoryMax=0 -- node x.js", "node"), ("systemd-run -p MemoryMax= -- node x.js", "node"),
    ("systemd-run -p MemoryMax=0G -- node x.js", "node"), ("systemd-run -p MemoryMax=max -- node x.js", "node"),
    ("systemd-run -p MemoryMax=0 -p MemoryMax=4G -- node x.js", None), ("systemd-run -p MemoryMax=50% -- node x.js", None),
    ("systemd-run -p MemoryMax=512M -- node x.js", None), ("systemd-run -p MemoryMax=1073741824 -- node x.js", None),
]
ENV_SPLIT = [
    ('env -S "node x.js"', "node"), ("env -S 'FOO=1 python3 y.py'", "python3"), ('env -S"node x.js"', "node"),
    ("env --split-string='make all'", "make"), ('env -S "node x.js" extra', "node"), ("env A=1 -S 'npm test'", "npm"),
    ('env -S "ls -l"', None), ('env -S "node -e 1"', None), ("systemd-run -p MemoryMax=1G -- env -S 'node x.js'", None),
]
WHICH = [
    ("$(which node) x.js", "node"), ("`which node` x.js", "node"), ("$(command -v python3) y.py", "python3"),
    ('"$(which node)" x.js', "node"), ("$(which -a node) x.js", "node"), ("$(readlink -f $(which node)) x.js", "node"),
    ("FOO=1 $(which make) all", "make"), ("x && $(type -p npm) test", "npm"), ("sudo $(which node) x.js", "node"),
    ("$(which ls) -l", None), ("echo $(which node)", None), ("ls -l $(which node)", None), ("$(which node) -e 1", None),
    ("systemd-run -p MemoryMax=1G -- $(which node) x.js", None),
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

    def test_wrapper_options_with_values_are_skipped(self):
        self.table(WRAPPER_OPTIONS)

    def test_a_script_run_uncapped_is_heavy(self):
        self.table(SCRIPTS)

    def test_only_a_property_memory_max_caps(self):
        self.table(CAP_FORMS)

    def test_gradle_is_heavy(self):
        self.table(GRADLE)

    def test_a_bare_make_is_heavy(self):
        self.table(BARE_MAKE)

    def test_a_memory_max_that_does_not_limit_is_not_a_cap(self):
        self.table(NO_CAP)

    def test_env_split_string_is_a_command_line(self):
        self.table(ENV_SPLIT)

    def test_a_command_named_by_which_is_that_command(self):
        self.table(WHICH)


if __name__ == "__main__":
    unittest.main(verbosity=1)
