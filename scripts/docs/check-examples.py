#!/usr/bin/env python3
"""check-examples: runs every fenced code sample in the documentation (FR-2611-anvl-docs-001).

    check-examples.py [--root DIR] [--dump anvil-dump] [--core PKG] [--js PKG] [--py PKG] [--net PKG]
                      [--strict] [--scratch DIR] [paths...]

Scans docs/**/*.md and site/assets/docs/**/*.md under --root (or the paths given) and prints one line per
sample: `PASS|FAIL|SKIP <file>:<line> <language> [- reason]`, then a summary. Exit 1 on any FAIL; with
--strict also on a SKIP caused by a missing package or toolchain. A package is a directory, a tarball or a URL.

  anvl, aml, amp, unlabelled   a document: anvil-dump (--dump, built from the native tag) and anvil.js (--js)
                               must both accept it, or both reject it with the same code
  js                           run with node against the anvil.js package, installed in a scratch dir
  c                            compiled and linked against --core (include/ and lib/libanvil.a) and run
  python, csharp               run against --py and --net when the toolchain is there; otherwise SKIP with a reason
  anything else (sh, xml, ...) SKIP: not executable

A sample is marked by HTML comments right before its fence (blank lines may lie between):

  <!-- example: fragment [why] -->  not runnable alone: SKIP (the reason is printed)
  <!-- example: error 4052 -->      an intentionally invalid document: both parsers must reject it with this code
  <!-- example: setup NAME -->      checked as usual, and kept as NAME. A document is written as the file NAME in
                                   the directory of every sample that `uses` it (so an include finds it); code
                                   is prepended to every sample that `uses` it. A js sample that uses a document
                                   starts with `const anvl = require('anvil.js')` and `doc`, that document parsed
  <!-- example: uses NAME... -->   see setup
  <!-- example: alias NAME... -->  js: require('NAME') gives the anvil.js package (for samples written for
                                   anvil-node, whose API anvil.js carries)
  <!-- example: output -->         the block must equal, up to the final newline, what the sample before it printed

A C sample that has no main() is assembled: its #include lines are hoisted, its function definitions go to file
level and its other lines run inside a generated main(). The anvil headers and <stdio.h> <stdlib.h> <string.h>
<stdint.h> <stdbool.h> are always included. A sample with no marker that fails is a FAIL.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

MARKER = re.compile(r"^\s*<!--\s*example:\s*(.*?)\s*-->\s*$")
FENCE = re.compile(r"^(\s*)(`{3,}|~{3,})\s*([^\s`]*)")


class Sample:
    """One fenced code sample. `line` is the 1-based line of the opening fence (of the marker, for an orphan)."""

    def __init__(self, file, line, lang, text, markers, orphan=False):
        self.file, self.line, self.lang, self.text, self.markers, self.orphan = file, line, lang, text, markers, orphan


def parse_marker(body):
    parts = body.split()
    return (parts[0], parts[1:]) if parts else ("", [])


def extract_samples(path, text):
    """Every fenced sample of a markdown text, in order, with the `<!-- example: ... -->` markers right before it.

    A marker belongs to the next fence when only blank lines or other markers lie between them; a marker
    followed by anything else is an orphan, reported as a sample of its own.
    """
    lines = text.split("\n")
    out = []
    pending = []  # (line number, marker) not yet attached
    i = 0
    while i < len(lines):
        ln = lines[i]
        m = MARKER.match(ln)
        if m:
            pending.append((i + 1, parse_marker(m.group(1))))
            i += 1
            continue
        f = FENCE.match(ln)
        if f:
            indent, fence, lang = f.group(1), f.group(2), f.group(3)
            body = []
            j = i + 1
            while j < len(lines):
                c = lines[j]
                if c.strip().startswith(fence[0] * len(fence)) and not c.strip().strip(fence[0]):
                    break
                body.append(c[len(indent):] if c.startswith(indent) else c.lstrip())
                j += 1
            out.append(Sample(path, i + 1, lang, "\n".join(body) + "\n", [mk for _, mk in pending]))
            pending = []
            i = j + 1
            continue
        if ln.strip() == "":
            i += 1
            continue
        for pl, mk in pending:
            out.append(Sample(path, pl, "", "", [mk], orphan=True))
        pending = []
        i += 1
    for pl, mk in pending:
        out.append(Sample(path, pl, "", "", [mk], orphan=True))
    return out



# ---------------------------------------------------------------------------------------------
# results, context and helpers
# ---------------------------------------------------------------------------------------------
class Result:
    """status is PASS, FAIL or SKIP. `needs` marks a SKIP caused by a missing package or toolchain
    (--strict turns it into a failure). `stdout` is what a run printed, for a following output block."""

    def __init__(self, status, reason="", needs=False, stdout=None):
        self.status, self.reason, self.needs, self.stdout = status, reason, needs, stdout


def passed(stdout=None):
    return Result("PASS", stdout=stdout)


def failed(reason):
    return Result("FAIL", reason)


def skipped(reason, needs=False):
    return Result("SKIP", reason, needs=needs)


KNOWN_MARKERS = {"fragment": (0, None), "output": (0, 0), "error": (1, 1), "setup": (1, 1), "uses": (1, None), "alias": (1, None)}
DOC_LANGS = {"anvl", "aml", "amp", ""}
JS_LANGS = {"js", "javascript", "mjs", "node"}
PY_LANGS = {"python", "py"}
NET_LANGS = {"csharp", "cs", "c#"}
TIMEOUT = 60


def run_cmd(cmd, cwd, env, timeout=TIMEOUT):
    """Runs a command; returns (returncode, stdout, stderr). returncode is None on a timeout."""
    try:
        p = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout, stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired as e:
        return None, (e.stdout or b"").decode() if isinstance(e.stdout, bytes) else (e.stdout or ""), "timed out after %ds" % timeout
    except OSError as e:
        return 127, "", str(e)
    return p.returncode, p.stdout, p.stderr


def tail_text(text, n=300):
    """The end of a failure text on one line, without the stack frames ("    at ...") that bury the message."""
    text = " ".join(l.strip() for l in text.splitlines() if not re.match(r"^\s+at ", l))
    return text if len(text) <= n else "..." + text[-n:]


class Setup:
    """A named earlier sample: code to prepend (code languages) and files to put in the sample's directory."""

    def __init__(self, name, kind, lang, code, files):
        self.name, self.kind, self.lang, self.code, self.files = name, kind, lang, code, files


class Context:
    def __init__(self, scratch, dump=None, js=None, core=None, py=None, net=None):
        self.scratch, self.dump, self.js, self.core, self.py, self.net = scratch, dump, js, core, py, net
        self.counter = 0
        self.env = dict(os.environ)
        home = os.path.join(scratch, "home")
        tmp = os.path.join(scratch, "tmp")
        os.makedirs(home, exist_ok=True)
        os.makedirs(tmp, exist_ok=True)
        self.env.update({"HOME": home, "TMPDIR": tmp, "XDG_CACHE_HOME": os.path.join(home, ".cache"), "XDG_CONFIG_HOME": os.path.join(home, ".config")})
        self.jsenv = None

    def sample_dir(self, under_jsenv=False):
        self.counter += 1
        base = self.js_env() if under_jsenv else self.scratch
        d = os.path.join(base, "s%d" % self.counter)
        os.makedirs(d)
        return d

    def js_env(self):
        """A directory whose node_modules/anvil.js is the package under test, so require('anvil.js') finds it."""
        if self.jsenv is None:
            self.jsenv = os.path.join(self.scratch, "jsenv")
            mods = os.path.join(self.jsenv, "node_modules")
            os.makedirs(mods)
            shutil.copytree(self.js, os.path.join(mods, "anvil.js"))
        return self.jsenv


def write_files(directory, files):
    for name, text in files.items():
        p = os.path.join(directory, name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w") as f:
            f.write(text)


# ---------------------------------------------------------------------------------------------
# documents: anvil-dump and anvil.js must agree
# ---------------------------------------------------------------------------------------------
JS_PARSE = """const fs = require('fs');
const anvl = require(process.argv[2]);
const d = anvl.parse(fs.readFileSync(process.argv[3], 'utf8'));
if (d) { console.log(JSON.stringify({ ok: true })); }
else { const e = anvl.lastError() || {}; console.log(JSON.stringify({ ok: false, code: e.code, name: e.name, message: e.message, line: e.line, column: e.column })); }
"""


def native_parse(ctx, directory, fname):
    rc, out, err = run_cmd([ctx.dump, fname], directory, ctx.env)
    st = re.search(r"^status (ok|error)$", out, re.M)
    if rc != 0 or not st:
        return None, "anvil-dump failed (exit %s): %s" % (rc, tail_text(err or out))
    if st.group(1) == "ok":
        return {"ok": True}, None
    e = re.search(r"^error code=(\d+) name=(\S+)", out, re.M)
    m = re.search(r'message="((?:[^"\\]|\\.)*)"', out)
    lc = re.search(r"line=(\d+) column=(\d+)", out)
    return {"ok": False, "code": int(e.group(1)) if e else 0, "name": e.group(2) if e else "", "message": m.group(1) if m else "",
            "line": int(lc.group(1)) if lc else 0}, None


def js_parse(ctx, directory, fname):
    helper = os.path.join(ctx.scratch, "js-parse.js")
    if not os.path.exists(helper):
        with open(helper, "w") as f:
            f.write(JS_PARSE)
    rc, out, err = run_cmd(["node", helper, os.path.join(ctx.js_env(), "node_modules", "anvil.js"), fname], directory, ctx.env)
    try:
        return json.loads(out.strip().splitlines()[-1]), None
    except (ValueError, IndexError):
        return None, "anvil.js parse helper failed (exit %s): %s" % (rc, tail_text(err or out))


def describe(r):
    return "accepted" if r["ok"] else "rejected %s %s" % (r.get("code"), r.get("name") or "")


def check_document(s, text, files, ctx, expected):
    if not ctx.dump:
        return skipped("no --dump (an anvil-dump built from the native tag)", needs=True)
    if not ctx.js:
        return skipped("no --js (the anvil.js package)", needs=True)
    directory = ctx.sample_dir()
    write_files(directory, files)
    fname = "sample.amp" if s.lang == "amp" else "sample.anvl"
    write_files(directory, {fname: text})
    nat, e1 = native_parse(ctx, directory, fname)
    if e1:
        return failed(e1)
    js, e2 = js_parse(ctx, directory, fname)
    if e2:
        return failed(e2)
    if nat["ok"] != js["ok"] or (not nat["ok"] and nat["code"] != js["code"]):
        return failed("native and anvil.js disagree: native %s, anvil.js %s" % (describe(nat), describe(js)))
    if nat["ok"]:
        if expected is None:
            return passed()
        return failed("accepted by native and anvil.js, but the marker expects error %d" % expected)
    where = "%s at line %s: %s" % (describe(nat), nat.get("line"), nat.get("message"))
    if expected is None:
        return failed("rejected: " + where)
    if expected == nat["code"]:
        return passed()
    return failed("rejected with %s, but the marker expects error %d (%s)" % (nat["code"], expected, where))


# ---------------------------------------------------------------------------------------------
# one file
# ---------------------------------------------------------------------------------------------
def classify(lang):
    l = lang.lower()
    if l in DOC_LANGS:
        return "doc"
    if l in JS_LANGS:
        return "js"
    if l == "c":
        return "c"
    if l in PY_LANGS:
        return "py"
    if l in NET_LANGS:
        return "net"
    return "other"


HANDLERS = {}


def check_markers(s):
    """Returns (markers dict, error text). A marker with an unknown name or the wrong arguments is an error."""
    found = {}
    for kind, args in s.markers:
        if kind not in KNOWN_MARKERS:
            return None, "unknown example marker '%s' (known: %s)" % (" ".join([kind] + args), ", ".join(sorted(KNOWN_MARKERS)))
        lo, hi = KNOWN_MARKERS[kind]
        if len(args) < lo or (hi is not None and len(args) > hi) or (kind == "error" and not args[0].isdigit()):
            return None, "bad example marker '%s'" % " ".join([kind] + args)
        found.setdefault(kind, []).extend(args)
    return found, None


JS_DOC_PRELUDE = """const anvl = require('anvil.js');
const doc = anvl.parse(%s);
if (!doc) throw new Error('the setup document does not parse: ' + JSON.stringify(anvl.lastError()));
"""


def check_output(s, state):
    """An output block must equal, up to the final newline, what the sample before it printed."""
    if state["stdout"] is None:
        return failed("output block, but no preceding sample printed anything")
    want, got = s.text.rstrip("\n"), state["stdout"].replace("\r\n", "\n").rstrip("\n")
    if want == got:
        return passed()
    wl, gl = want.split("\n"), got.split("\n")
    for i in range(max(len(wl), len(gl))):
        a = wl[i] if i < len(wl) else None
        b = gl[i] if i < len(gl) else None
        if a != b:
            return failed("output differs at line %d: the block has %s, the run printed %s" % (i + 1, "nothing" if a is None else repr(a), "nothing" if b is None else repr(b)))


def check_sample(s, state, ctx):
    if s.orphan:
        return failed("example marker '%s' is not followed by a code fence" % " ".join([s.markers[0][0]] + s.markers[0][1]))
    marks, err = check_markers(s)
    if err:
        return failed(err)
    if "fragment" in marks:
        return skipped("marked fragment" + (": " + " ".join(marks["fragment"]) if marks["fragment"] else ": not run"))
    kind = classify(s.lang)
    if "output" in marks:
        return check_output(s, state)
    prefix, files, doc_text = "", {}, None
    for name in marks.get("uses", []):
        su = state["setups"].get(name)
        if su is None:
            return failed("uses '%s', but no earlier sample in this file is marked 'setup %s'" % (name, name))
        if su.kind == "code":
            if su.lang != s.lang:
                return failed("uses '%s', a %s sample, from a %s sample" % (name, su.lang, s.lang))
            prefix += su.code
        files.update(su.files)
        if su.kind == "doc":
            doc_text = su.files[name]
    handler = HANDLERS.get(kind)
    if handler is None:
        return failed("not implemented: %s samples" % kind)
    expected = int(marks["error"][0]) if "error" in marks else None
    if doc_text is not None and kind == "js":
        prefix = JS_DOC_PRELUDE % json.dumps(doc_text) + prefix
    text = prefix + s.text
    res = handler(s, text, files, ctx, expected)
    state["stdout"] = res.stdout
    for name in marks.get("setup", []):
        own = {name: s.text} if kind == "doc" else {}
        state["setups"][name] = Setup(name, "doc" if kind == "doc" else "code", s.lang, text if kind != "doc" else "", dict(files, **own))
    return res


def check_file(path, rel, ctx):
    with open(path, encoding="utf-8") as f:
        samples = extract_samples(rel, f.read())
    state = {"setups": {}, "stdout": None}
    out = []
    for s in samples:
        res = check_sample(s, state, ctx)
        out.append((s, res))
    return out


def run_code(ctx, directory, cmd, files, fname, text, what):
    write_files(directory, files)
    write_files(directory, {fname: text})
    rc, out, err = run_cmd(cmd, directory, ctx.env)
    if rc != 0:
        return failed("%s exited %s: %s" % (what, rc if rc is not None else "(timeout)", tail_text(err or out)))
    return passed(stdout=out)


C_PRELUDE = ["anvil_flat.h", "anvil_type_registry.h", "anvil_schema.h", "anvil_writer.h", "anvil_builder.h"]
C_STD = ["stdio.h", "stdlib.h", "string.h", "stdint.h", "stdbool.h"]
C_KEYWORDS = r"(?:if|for|while|switch|else|do|return|case|goto|sizeof)"
C_FUNC_HEAD = re.compile(r"^(?!" + C_KEYWORDS + r"\b)[A-Za-z_][\w \t\*]*?[\w\*]\s*\(")


def split_c(text):
    """(includes, functions, statements, has_main) of a C sample: preprocessor includes, top-level function
    definitions (a line at column 0 that opens one, up to its closing brace) and everything else."""
    includes, funcs, stmts, has_main = [], [], [], False
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        ln = lines[i]
        if re.match(r"^\s*#\s*include\b", ln):
            includes.append(ln.strip())
            i += 1
        elif C_FUNC_HEAD.match(ln) and not ln.rstrip().endswith(";"):
            depth, seen, block = 0, False, []
            while i < len(lines):
                block.append(lines[i])
                depth += lines[i].count("{") - lines[i].count("}")
                seen = seen or "{" in lines[i]
                i += 1
                if seen and depth <= 0:
                    break
            funcs.append("\n".join(block))
            if re.match(r"^[\w \t\*]*\bmain\s*\(", block[0]):
                has_main = True
        else:
            stmts.append(ln)
            i += 1
    return includes, funcs, stmts, has_main


def find_dir_with(root, marker):
    if os.path.exists(os.path.join(root, marker)):
        return root
    for dp, dn, fn in os.walk(root):
        if marker in fn or marker in dn:
            return dp
    return None


def assemble_c(text, include_dir):
    includes, funcs, stmts, has_main = split_c(text)
    head = ["#include <%s>" % h for h in C_STD]
    head += ['#include "%s"' % h for h in C_PRELUDE if os.path.exists(os.path.join(include_dir, h))]
    head += [i for i in includes if i not in head]
    body = list(funcs)
    if not has_main:
        body.append("int main(void) {\n" + "\n".join(stmts) + "\nreturn 0;\n}")
    else:
        body.append("\n".join(stmts))
    return "\n".join(head) + "\n\n" + "\n\n".join(body) + "\n"


def check_c(s, text, files, ctx, expected):
    if not ctx.core:
        return skipped("no --core (the core package)", needs=True)
    if shutil.which("gcc") is None:
        return skipped("no gcc on this machine", needs=True)
    inc = find_dir_with(ctx.core, "anvil_flat.h")
    lib = None
    for dp, dn, fn in os.walk(ctx.core):
        if "libanvil.a" in fn:
            lib = os.path.join(dp, "libanvil.a")
    if inc is None or lib is None:
        return failed("the core package has no anvil_flat.h and lib/libanvil.a")
    directory = ctx.sample_dir()
    write_files(directory, files)
    write_files(directory, {"sample.c": assemble_c(text, inc)})
    rc, out, err = run_cmd(["gcc", "-std=gnu11", "-w", "-I" + inc, "sample.c", lib, "-lm", "-o", "sample"], directory, ctx.env)
    if rc != 0:
        return failed("compile or link failed: " + tail_text(err or out))
    rc, out, err = run_cmd(["./sample"], directory, ctx.env)
    if rc != 0:
        return failed("the program exited %s: %s" % (rc if rc is not None else "(timeout)", tail_text(err or out)))
    return passed(stdout=out)


HANDLERS["c"] = check_c


def check_py(s, text, files, ctx, expected):
    if not ctx.py:
        return skipped("no --py (the anvil.py package)", needs=True)
    pkg = None
    for dp, dn, fn in os.walk(ctx.py):
        if os.path.basename(dp) == "anvil" and "__init__.py" in fn:
            pkg = os.path.dirname(dp)
            break
    if pkg is None:
        return failed("the anvil.py package has no anvil/__init__.py")
    ctx.env["PYTHONPATH"] = pkg
    ctx.env["PYTHONDONTWRITEBYTECODE"] = "1"
    return run_code(ctx, ctx.sample_dir(), [sys.executable, "sample.py"], files, "sample.py", text, "python")


NET_PROJECT = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net%s.0</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
    <AssemblyName>sample</AssemblyName>
  </PropertyGroup>
  <ItemGroup>
    <Reference Include="Anvil.Net"><HintPath>%s</HintPath></Reference>
  </ItemGroup>
</Project>
"""


def check_net(s, text, files, ctx, expected):
    if not ctx.net:
        return skipped("no --net (the anvil.net package)", needs=True)
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        return skipped("no dotnet SDK on this machine", needs=True)
    dll = None
    for dp, dn, fn in os.walk(ctx.net):
        if "Anvil.Net.dll" in fn:
            dll = os.path.join(dp, "Anvil.Net.dll")
    if dll is None:
        return failed("the anvil.net package has no Anvil.Net.dll")
    env = dict(ctx.env)
    home = env["HOME"]
    env.update({"DOTNET_CLI_HOME": home, "NUGET_PACKAGES": os.path.join(home, ".nuget"), "DOTNET_CLI_TELEMETRY_OPTOUT": "1",
                "DOTNET_NOLOGO": "1", "DOTNET_SKIP_FIRST_TIME_EXPERIENCE": "1", "MSBUILDDISABLENODEREUSE": "1"})
    rc, ver, err = run_cmd([dotnet, "--version"], ctx.scratch, env)
    if rc != 0:
        return failed("dotnet --version failed: " + tail_text(err or ver))
    directory = ctx.sample_dir()
    write_files(directory, files)
    write_files(directory, {"sample.csproj": NET_PROJECT % (ver.strip().split(".")[0], dll), "Program.cs": text})
    rc, out, err = run_cmd([dotnet, "build", "-c", "Release", "-o", "out", "--nologo", "-v", "q", "-nodeReuse:false",
                            "-p:UseSharedCompilation=false"], directory, env, timeout=300)
    if rc != 0:
        return failed("dotnet build failed: " + tail_text(out or err))
    for f in os.listdir(os.path.dirname(dll)):
        if f.endswith(".so"):
            shutil.copy(os.path.join(os.path.dirname(dll), f), os.path.join(directory, "out", f))
    rc, out, err = run_cmd([dotnet, os.path.join("out", "sample.dll")], directory, env)
    if rc != 0:
        return failed("dotnet exited %s: %s" % (rc if rc is not None else "(timeout)", tail_text(err or out)))
    return passed(stdout=out)


def check_other(s, text, files, ctx, expected):
    return skipped("language '%s' is not executable" % (s.lang or "(none)"))


HANDLERS["py"] = check_py
HANDLERS["net"] = check_net
HANDLERS["other"] = check_other


def check_js(s, text, files, ctx, expected):
    if not ctx.js:
        return skipped("no --js (the anvil.js package)", needs=True)
    fname = "sample.mjs" if re.search(r"^\s*(import|export)\s", text, re.M) else "sample.js"
    directory = ctx.sample_dir(under_jsenv=True)
    for name in [a for kind, args in s.markers if kind == "alias" for a in args]:
        shim = os.path.join(directory, "node_modules", name)
        write_files(shim, {})
        write_files(shim, {"package.json": json.dumps({"name": name, "main": "index.js"}),
                           "index.js": "module.exports = require('anvil.js');\n",
                           "index.mjs": "export * from 'anvil.js';\nexport { default } from 'anvil.js';\n"})
    return run_code(ctx, directory, ["node", fname], files, fname, text, "node")


HANDLERS["js"] = check_js
HANDLERS["doc"] = lambda s, text, files, ctx, expected: check_document(s, text, files, ctx, expected)


def find_markdown(root, paths):
    targets = [os.path.join(root, "docs"), os.path.join(root, "site", "assets", "docs")] if not paths else paths
    found = []
    for t in targets:
        if os.path.isfile(t):
            found.append(t)
            continue
        for dp, dn, fn in os.walk(t):
            dn[:] = sorted(d for d in dn if d not in ("node_modules", ".git"))
            found.extend(os.path.join(dp, f) for f in sorted(fn) if f.endswith(".md"))
    return found


def package_dir(arg, flag, scratch):
    """A package argument is a directory, a tarball, or an http(s):// or file:// URL of a tarball (the live
    downloads of a docs-only deploy). A tarball is unpacked in the scratch dir; the result is the package's
    top directory."""
    if arg is None:
        return None
    path = arg
    if re.match(r"^(https?|file)://", arg):
        dest = os.path.join(scratch, "download")
        os.makedirs(dest, exist_ok=True)
        path = os.path.join(dest, os.path.basename(arg.split("?")[0]))
        try:
            req = urllib.request.Request(arg, headers={"User-Agent": "anvil-check-examples/1"})  # the site refuses Python-urllib
            with urllib.request.urlopen(req, timeout=120) as r, open(path, "wb") as f:
                shutil.copyfileobj(r, f)
        except OSError as e:
            raise SystemExit("check-examples: cannot fetch %s %s: %s" % (flag, arg, e))
    if os.path.isdir(path):
        return os.path.abspath(path)
    if os.path.isfile(path) and re.search(r"\.(tar\.gz|tgz|tar)$", path):
        dest = os.path.join(scratch, "pkg" + flag.lstrip("-"))
        os.makedirs(dest)
        with tarfile.open(path) as t:
            t.extractall(dest, filter="data")
        top = os.listdir(dest)
        return os.path.join(dest, top[0]) if len(top) == 1 and os.path.isdir(os.path.join(dest, top[0])) else dest
    raise SystemExit("check-examples: %s %s is not a directory, a tarball or a URL" % (flag, arg))


def main(argv=None):
    ap = argparse.ArgumentParser(description="Run every fenced code sample in the documentation.")
    ap.add_argument("paths", nargs="*", help="markdown files or directories (default: docs/ and site/assets/docs/ under --root)")
    ap.add_argument("--root", default=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")))
    ap.add_argument("--dump", help="an anvil-dump binary built from the native tag being published")
    ap.add_argument("--core", help="the core package (directory, tarball or URL)")
    ap.add_argument("--js", help="the anvil.js package (directory, tarball or URL)")
    ap.add_argument("--py", help="the anvil.py package (directory, tarball or URL)")
    ap.add_argument("--net", help="the anvil.net package (directory, tarball or URL)")
    ap.add_argument("--strict", action="store_true", help="a sample skipped for a missing package or toolchain fails the run")
    ap.add_argument("--scratch", help="scratch directory (default: a temporary one, removed on exit)")
    opts = ap.parse_args(argv)
    root = os.path.abspath(opts.root)
    own = opts.scratch is None
    scratch = os.path.abspath(opts.scratch) if opts.scratch else tempfile.mkdtemp(prefix="check-examples-")
    os.makedirs(scratch, exist_ok=True)
    try:
        dump = os.path.abspath(opts.dump) if opts.dump else None
        if dump and not os.access(dump, os.X_OK):
            raise SystemExit("check-examples: --dump %s is not an executable" % opts.dump)
        ctx = Context(scratch, dump=dump, js=package_dir(opts.js, "--js", scratch), core=package_dir(opts.core, "--core", scratch),
                      py=package_dir(opts.py, "--py", scratch), net=package_dir(opts.net, "--net", scratch))
        counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
        needs_skips = 0
        for path in find_markdown(root, opts.paths):
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            for s, res in check_file(path, rel, ctx):
                counts[res.status] += 1
                if res.status == "SKIP" and res.needs:
                    needs_skips += 1
                label = s.lang or "(none)" if not s.orphan else "marker"
                print("%s %s:%d %s%s" % (res.status, s.file, s.line, label, " - " + res.reason if res.reason else ""))
        print("check-examples: %d passed, %d failed, %d skipped" % (counts["PASS"], counts["FAIL"], counts["SKIP"]))
        return 1 if counts["FAIL"] or (opts.strict and needs_skips) else 0
    finally:
        if own:
            shutil.rmtree(scratch, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
