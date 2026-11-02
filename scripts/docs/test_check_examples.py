#!/usr/bin/env python3
"""Tests of scripts/docs/check-examples.py (FR-2611-anvl-docs-001).

The fixtures are markdown documents the tests write; the packages are the tarballs published in
site/assets/downloads (core and anvil.js), a fake `anvil-dump` that speaks the real dump's status and
error lines for a few documents, and a fake anvil.py package.
"""
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
SCRIPT = os.path.join(HERE, "check-examples.py")
DOWNLOADS = os.path.join(REPO, "site", "assets", "downloads")
CORE_TGZ = os.path.join(DOWNLOADS, "anvil-v0.13.1-rc-linux-x86_64.tar.gz")
JS_TGZ = os.path.join(DOWNLOADS, "anvil.js-v0.13.1-rc.tar.gz")

FAKE_DUMP = r'''#!/usr/bin/env python3
import os, re, sys
t = open(sys.argv[-1]).read()
print("anvil-dump 1")
print("mode document")
print("input-bytes %d" % len(t))
m = re.search(r'include "([^"]+)"', t)
if m and not os.path.exists(m.group(1)):
    print("status error"); print("dialect AML")
    print('error code=4204 name=ANVL_ERR_INCLUDE_FILE_NOT_FOUND category=4 line=1 column=1 message="x"')
elif "5." + ";" in t:
    print("status error"); print("dialect AML")
    print('error code=5009 name=ANVL_ERR_PARSER_INVALID_NUMBER category=5 line=2 column=6 message="x"')
elif "NATIVE_REJECTS" in t:
    print("status error"); print("dialect AML")
    print('error code=4053 name=ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER category=4 line=1 column=1 message="x"')
else:
    print("status ok"); print("dialect AML")
'''

FAKE_PY_INIT = 'def version():\n    return "fake-1"\n'

TMP = None
CORE_DIR = JS_DIR = DUMP = PY_DIR = None


def load_module():
    spec = importlib.util.spec_from_file_location("check_examples", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def setUpModule():
    global TMP, CORE_DIR, JS_DIR, DUMP, PY_DIR
    TMP = tempfile.mkdtemp(prefix="check-examples-test-")
    for name, tgz in (("core", CORE_TGZ), ("js", JS_TGZ)):
        dest = os.path.join(TMP, name)
        os.makedirs(dest)
        with tarfile.open(tgz) as t:
            t.extractall(dest)
        top = os.listdir(dest)
        assert len(top) == 1
        if name == "core":
            CORE_DIR = os.path.join(dest, top[0])
        else:
            JS_DIR = os.path.join(dest, top[0])
    DUMP = os.path.join(TMP, "fake-anvil-dump")
    with open(DUMP, "w") as f:
        f.write(FAKE_DUMP)
    os.chmod(DUMP, 0o755)
    PY_DIR = os.path.join(TMP, "py")
    os.makedirs(os.path.join(PY_DIR, "src", "anvil"))
    with open(os.path.join(PY_DIR, "src", "anvil", "__init__.py"), "w") as f:
        f.write(FAKE_PY_INIT)


def tearDownModule():
    shutil.rmtree(TMP, ignore_errors=True)


class Doc:
    """Builds a markdown file; block() returns the 1-based line number of the opening fence."""

    def __init__(self):
        self.lines = ["# fixture", ""]

    def block(self, lang, body, *markers, indent=""):
        for m in markers:
            self.lines.append("<!-- example: %s -->" % m)
        self.lines.append("")  # a blank line between marker and fence
        self.lines.append(indent + "```" + lang)
        line = len(self.lines)
        for b in body.split("\n"):
            self.lines.append(indent + b if b else "")
        self.lines.append(indent + "```")
        self.lines.append("")
        return line

    def prose(self, text):
        self.lines.extend([text, ""])

    def text(self):
        return "\n".join(self.lines) + "\n"


LINE = re.compile(r"^(PASS|FAIL|SKIP) (\S+):(\d+) (\S+)(?: - (.*))?$")


class Run:
    def __init__(self, proc, root):
        self.proc = proc
        self.root = root
        self.exit = proc.returncode
        self.out = proc.stdout
        self.results = {}
        for ln in proc.stdout.splitlines():
            m = LINE.match(ln)
            if m:
                self.results[(m.group(2), int(m.group(3)))] = (m.group(1), m.group(4), m.group(5) or "")

    def at(self, line, path="docs/t.md"):
        return self.results.get((path, line), ("MISSING", "", ""))


def run_check(doc, files=None, dump=True, js=True, core=True, py=None, net=None, strict=False, extra=(), env=None):
    root = tempfile.mkdtemp(prefix="root-", dir=TMP)
    os.makedirs(os.path.join(root, "docs"))
    for rel, text in (files or {"docs/t.md": doc.text() if doc else ""}).items():
        p = os.path.join(root, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w") as f:
            f.write(text)
    args = [sys.executable, SCRIPT, "--root", root]
    if dump:
        args += ["--dump", DUMP]
    if js:
        args += ["--js", JS_DIR]
    if core:
        args += ["--core", CORE_DIR]
    if py:
        args += ["--py", py]
    if net:
        args += ["--net", net]
    if strict:
        args.append("--strict")
    args += list(extra)
    e = dict(os.environ)
    e.update(env or {})
    proc = subprocess.run(args, capture_output=True, text=True, timeout=300, env=e, cwd=root)
    return Run(proc, root)


class ExtractTests(unittest.TestCase):
    def setUp(self):
        self.ce = load_module()

    def extract(self, doc):
        return self.ce.extract_samples("docs/t.md", doc.text())

    def test_extract_finds_each_fence_with_file_line_and_language(self):
        d = Doc()
        a = d.block("anvl", "x := 1;")
        d.prose("between")
        b = d.block("js", "console.log(1);")
        s = self.extract(d)
        self.assertEqual(len(s), 2)
        self.assertEqual([(x.file, x.line, x.lang) for x in s], [("docs/t.md", a, "anvl"), ("docs/t.md", b, "js")])
        self.assertEqual(s[0].text, "x := 1;\n")

    def test_extract_unlabelled_fence_has_empty_language(self):
        d = Doc()
        d.block("", "x := 1;")
        s = self.extract(d)
        self.assertEqual(len(s), 1)
        self.assertEqual(s[0].lang, "")

    def test_extract_strips_the_fence_indent_from_the_text(self):
        d = Doc()
        d.block("js", "if (1) {\n  f();\n}", indent="   ")
        s = self.extract(d)
        self.assertEqual(len(s), 1)
        self.assertEqual(s[0].text, "if (1) {\n  f();\n}\n")

    def test_extract_attaches_markers_across_blank_lines(self):
        d = Doc()
        d.block("anvl", "x := 1;", "fragment")
        s = self.extract(d)
        self.assertEqual(len(s), 1)
        self.assertEqual(s[0].markers, [("fragment", [])])

    def test_extract_does_not_attach_a_marker_separated_by_prose(self):
        d = Doc()
        d.lines.append("<!-- example: fragment -->")
        d.prose("some prose")
        d.block("anvl", "x := 1;")
        s = self.extract(d)
        real = [x for x in s if x.lang == "anvl"]
        self.assertEqual(len(real), 1)
        self.assertEqual(real[0].markers, [])
        self.assertEqual(len([x for x in s if x.orphan]), 1)

    def test_extract_collects_several_markers_on_one_sample(self):
        d = Doc()
        d.block("js", "x();", "setup b", "uses a")
        s = self.extract(d)
        self.assertEqual(len(s), 1)
        self.assertEqual(s[0].markers, [("setup", ["b"]), ("uses", ["a"])])

    def test_extract_reports_an_orphan_marker(self):
        d = Doc()
        d.lines.append("<!-- example: fragment -->")
        d.prose("nothing follows but prose")
        s = self.extract(d)
        self.assertEqual(len(s), 1)
        self.assertTrue(s[0].orphan)
        self.assertEqual(s[0].line, 3)


class DocumentTests(unittest.TestCase):
    def test_valid_document_passes_in_native_and_anvil_js(self):
        d = Doc()
        n = d.block("anvl", "#!aml\nname := David;")
        r = run_check(d)
        self.assertEqual(r.at(n)[0], "PASS", r.out)

    def test_unmarked_invalid_document_fails_with_the_code(self):
        d = Doc()
        n = d.block("anvl", "#!aml\nx := 5.;")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("5009", st[2])

    def test_error_marker_passes_when_both_reject_with_that_code(self):
        d = Doc()
        n = d.block("anvl", "#!aml\nx := 5.;", "error 5009")
        r = run_check(d)
        self.assertEqual(r.at(n)[0], "PASS", r.out)

    def test_error_marker_fails_when_the_code_differs(self):
        d = Doc()
        n = d.block("anvl", "#!aml\nx := 5.;", "error 4052")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("5009", st[2])
        self.assertIn("4052", st[2])

    def test_error_marker_fails_when_the_document_is_valid(self):
        d = Doc()
        n = d.block("anvl", "#!aml\nx := 1;", "error 4052")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("accepted", st[2])

    def test_native_and_anvil_js_disagreeing_is_a_fail(self):
        d = Doc()
        n = d.block("anvl", "#!aml\nx := 1; // NATIVE_REJECTS")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("disagree", st[2])

    def test_setup_document_is_visible_to_a_uses_sample_as_a_file(self):
        d = Doc()
        a = d.block("anvl", "#!aml\nvin := 1;", "setup base.anvl")
        b = d.block("anvl", '#!aml\ninclude "base.anvl";\nother := 2;', "uses base.anvl")
        r = run_check(d)
        self.assertEqual(r.at(a)[0], "PASS", r.out)
        self.assertEqual(r.at(b)[0], "PASS", r.out)

    def test_include_without_setup_fails(self):
        d = Doc()
        n = d.block("anvl", '#!aml\ninclude "base.anvl";\nother := 2;')
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("4204", st[2])

    def test_fragment_marker_skips_with_reason(self):
        d = Doc()
        n = d.block("anvl", "this is not a document", "fragment")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "SKIP", r.out)
        self.assertIn("fragment", st[2])

    def test_unknown_marker_fails(self):
        d = Doc()
        n = d.block("anvl", "x := 1;", "frobnicate")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("marker", st[2])

    def test_orphan_marker_fails(self):
        d = Doc()
        d.lines.append("<!-- example: fragment -->")
        d.prose("no fence follows")
        r = run_check(d)
        self.assertEqual(r.at(3)[0], "FAIL", r.out)

    def test_documents_skip_without_dump_and_say_why(self):
        d = Doc()
        n = d.block("anvl", "#!aml\nx := 1;")
        r = run_check(d, dump=False)
        st = r.at(n)
        self.assertEqual(st[0], "SKIP", r.out)
        self.assertIn("--dump", st[2])

    def test_documents_skip_without_js_and_say_why(self):
        d = Doc()
        n = d.block("anvl", "#!aml\nx := 1;")
        r = run_check(d, js=False)
        st = r.at(n)
        self.assertEqual(st[0], "SKIP", r.out)
        self.assertIn("--js", st[2])

    def test_unlabelled_fence_is_checked_as_a_document(self):
        d = Doc()
        n = d.block("", "#!aml\nx := 5.;")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("5009", st[2])

    def test_aml_and_amp_labels_are_documents(self):
        d = Doc()
        a = d.block("aml", "#!aml\nx := 5.;")
        b = d.block("amp", "#!amp\nx := 1;")
        r = run_check(d)
        self.assertEqual(r.at(a)[0], "FAIL", r.out)
        self.assertEqual(r.at(b)[0], "PASS", r.out)


class JsTests(unittest.TestCase):
    def test_js_sample_passes_against_installed_anvil_js(self):
        d = Doc()
        n = d.block("js", "const anvl = require('anvil.js');\nif (!anvl.parse('#!aml\\nx := 1;\\n')) throw new Error('no');")
        r = run_check(d)
        self.assertEqual(r.at(n)[0], "PASS", r.out)

    def test_js_sample_that_throws_fails(self):
        d = Doc()
        n = d.block("js", "throw new Error('boom');")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("boom", st[2])

    def test_js_sample_runs_in_a_scratch_dir(self):
        d = Doc()
        n = d.block("js", "require('fs').writeFileSync('litter.txt', 'x');")
        r = run_check(d)
        self.assertEqual(r.at(n)[0], "PASS", r.out)
        self.assertEqual(sorted(os.listdir(r.root)), ["docs"])

    def test_js_uses_prepends_the_setup_code(self):
        d = Doc()
        a = d.block("js", "const anvl = require('anvil.js');", "setup lib")
        b = d.block("js", "if (typeof anvl.parse !== 'function') throw new Error('x');", "uses lib")
        r = run_check(d)
        self.assertEqual(r.at(a)[0], "PASS", r.out)
        self.assertEqual(r.at(b)[0], "PASS", r.out)

    def test_js_without_uses_does_not_see_earlier_samples(self):
        d = Doc()
        d.block("js", "const anvl = require('anvil.js');", "setup lib")
        b = d.block("js", "anvl.parse('x := 1;');")
        r = run_check(d)
        self.assertEqual(r.at(b)[0], "FAIL", r.out)

    def test_js_skips_without_package_and_says_why(self):
        d = Doc()
        n = d.block("js", "console.log(1);")
        r = run_check(d, js=False)
        st = r.at(n)
        self.assertEqual(st[0], "SKIP", r.out)
        self.assertIn("--js", st[2])

    def test_output_marker_passes_when_stdout_matches(self):
        d = Doc()
        a = d.block("js", "console.log('a');\nconsole.log('b');")
        b = d.block("", "a\nb", "output")
        r = run_check(d)
        self.assertEqual(r.at(a)[0], "PASS", r.out)
        self.assertEqual(r.at(b)[0], "PASS", r.out)

    def test_output_marker_fails_when_stdout_differs(self):
        d = Doc()
        d.block("js", "console.log('a');")
        b = d.block("", "z", "output")
        r = run_check(d)
        st = r.at(b)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("output differs", st[2])

    def test_output_marker_without_a_preceding_run_fails(self):
        d = Doc()
        b = d.block("", "a", "output")
        r = run_check(d)
        st = r.at(b)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("no preceding", st[2])


class CTests(unittest.TestCase):
    def test_c_program_compiles_links_and_runs(self):
        d = Doc()
        n = d.block("c", '#include "anvil_flat.h"\n#include <string.h>\nint main(void) {\n   const char *s = "#!aml\\nname := David;\\n";\n'
                         '   anvil_document doc = anvil_load_buffer(s, strlen(s));\n   if (!doc || anvil_has_errors(doc)) return 1;\n'
                         '   anvil_dispose(doc);\n   return 0;\n}')
        r = run_check(d)
        self.assertEqual(r.at(n)[0], "PASS", r.out)

    def test_c_statements_are_wrapped_in_main(self):
        d = Doc()
        n = d.block("c", 'const char *s = "#!aml\\nname := David;\\n";\nanvil_document doc = anvil_load_buffer(s, strlen(s));\n'
                         'printf("%d\\n", (int)anvil_has_errors(doc));')
        r = run_check(d)
        self.assertEqual(r.at(n)[0], "PASS", r.out)

    def test_c_compile_error_fails(self):
        d = Doc()
        n = d.block("c", "int x = no_such_function();")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("compile", st[2])

    def test_c_runtime_failure_fails(self):
        d = Doc()
        n = d.block("c", "int main(void) { return 3; }")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("exit", st[2])

    def test_c_function_only_sample_compiles_and_links(self):
        d = Doc()
        n = d.block("c", "void show(anvil_value v) {\n   char t[8] = {0};\n   anvil_value_get_text(v, t, sizeof(t));\n   puts(t);\n}")
        r = run_check(d)
        self.assertEqual(r.at(n)[0], "PASS", r.out)

    def test_c_uses_combines_setup_statements(self):
        d = Doc()
        a = d.block("c", 'const char *s = "#!aml\\nport := 8080;\\n";\nanvil_document doc = anvil_load_buffer(s, strlen(s));', "setup load")
        b = d.block("c", 'anvil_statement st = anvil_document_find_statement(doc, "port");\nif (!st) return 1;', "uses load")
        r = run_check(d)
        self.assertEqual(r.at(a)[0], "PASS", r.out)
        self.assertEqual(r.at(b)[0], "PASS", r.out)

    def test_c_output_marker_compares_stdout(self):
        d = Doc()
        d.block("c", 'printf("port = %d\\n", 8080);')
        ok = d.block("", "port = 8080", "output")
        d.block("c", 'printf("x\\n");')
        bad = d.block("", "y", "output")
        r = run_check(d)
        self.assertEqual(r.at(ok)[0], "PASS", r.out)
        self.assertEqual(r.at(bad)[0], "FAIL", r.out)

    def test_c_skips_without_core(self):
        d = Doc()
        n = d.block("c", "int x = 1;")
        r = run_check(d, core=False)
        st = r.at(n)
        self.assertEqual(st[0], "SKIP", r.out)
        self.assertIn("--core", st[2])


class OtherLanguageTests(unittest.TestCase):
    def test_python_sample_passes_against_the_package(self):
        d = Doc()
        n = d.block("python", "import anvil\nassert anvil.version() == 'fake-1'")
        r = run_check(d, py=PY_DIR)
        self.assertEqual(r.at(n)[0], "PASS", r.out)

    def test_python_sample_that_raises_fails(self):
        d = Doc()
        n = d.block("python", "import anvil\nraise RuntimeError('nope')")
        r = run_check(d, py=PY_DIR)
        st = r.at(n)
        self.assertEqual(st[0], "FAIL", r.out)
        self.assertIn("nope", st[2])

    def test_python_skips_without_package_and_says_why(self):
        d = Doc()
        n = d.block("python", "import anvil")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "SKIP", r.out)
        self.assertIn("--py", st[2])

    def test_csharp_skips_without_package_and_says_why(self):
        d = Doc()
        n = d.block("csharp", "Console.WriteLine(1);")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "SKIP", r.out)
        self.assertIn("--net", st[2])

    def test_shell_sample_is_skipped_as_not_executable(self):
        d = Doc()
        n = d.block("sh", "npm install ./x.tgz")
        r = run_check(d)
        st = r.at(n)
        self.assertEqual(st[0], "SKIP", r.out)
        self.assertIn("not executable", st[2])


class CommandLineTests(unittest.TestCase):
    def test_exit_is_nonzero_on_a_fail(self):
        d = Doc()
        d.block("js", "throw new Error('x');")
        r = run_check(d)
        self.assertEqual(r.exit, 1, r.out)

    def test_exit_is_zero_with_only_pass_and_skip(self):
        d = Doc()
        d.block("js", "console.log(1);")
        d.block("sh", "ls")
        r = run_check(d)
        self.assertEqual(r.exit, 0, r.out + r.proc.stderr)

    def test_strict_makes_a_missing_toolchain_skip_fail_the_run(self):
        d = Doc()
        d.block("python", "import anvil")
        loose = run_check(d)
        strict = run_check(d, strict=True)
        self.assertEqual(loose.exit, 0, loose.out)
        self.assertEqual(strict.exit, 1, strict.out)

    def test_strict_does_not_fail_on_marked_fragments_or_shell(self):
        d = Doc()
        d.block("anvl", "nothing", "fragment")
        d.block("sh", "ls")
        r = run_check(d, strict=True)
        self.assertEqual(r.exit, 0, r.out)

    def test_default_scan_covers_docs_and_site_docs(self):
        d = Doc()
        n = d.block("js", "console.log(1);")
        files = {"docs/a.md": d.text(), "docs/sub/b.md": d.text(), "site/assets/docs/a.md": d.text(), "other/c.md": d.text()}
        r = run_check(None, files=files)
        self.assertIn(("docs/a.md", n), r.results)
        self.assertIn(("docs/sub/b.md", n), r.results)
        self.assertIn(("site/assets/docs/a.md", n), r.results)
        self.assertNotIn(("other/c.md", n), r.results)

    def test_package_given_as_a_tarball_and_as_a_file_url(self):
        d = Doc()
        n = d.block("js", "require('anvil.js');")
        c = d.block("c", "int x = 1;")
        r = run_check(d, js=False, core=False, extra=["--js", JS_TGZ, "--core", "file://" + CORE_TGZ])
        self.assertEqual(r.at(n)[0], "PASS", r.out + r.proc.stderr)
        self.assertEqual(r.at(c)[0], "PASS", r.out + r.proc.stderr)

    def test_summary_line_counts_each_status(self):
        d = Doc()
        d.block("js", "console.log(1);")
        d.block("js", "throw new Error('x');")
        d.block("sh", "ls")
        r = run_check(d)
        self.assertIn("1 passed, 1 failed, 1 skipped", r.out)

    def test_scratch_is_removed_on_exit(self):
        d = Doc()
        d.block("js", "console.log(1);")
        tmp = tempfile.mkdtemp(prefix="tmpdir-", dir=TMP)
        r = run_check(d, env={"TMPDIR": tmp})
        self.assertEqual(r.exit, 0, r.out)
        self.assertEqual(os.listdir(tmp), [])


if __name__ == "__main__":
    unittest.main()
