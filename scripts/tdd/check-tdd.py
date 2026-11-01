#!/usr/bin/env python3
"""check-tdd: mechanical checker of the strict-TDD protocol (agents/STANDARDS.md section 3).

    check-tdd.py <repo> <base>..<head> [options]

Replays the commits of base..head (oldest first) in a scratch clone, runs the repo's test suite at
each commit and at its parent, classifies each commit by its subject tag and checks the rules:

  <ticket>: RED.       builds and runs; a test that passed (or did not exist) at the parent now fails,
                       and every newly failing test fails on an ASSERTION (never a TypeError, a missing
                       export, a build error or a crash). Test files change; product code only as stubs.
  <ticket>: GREEN.     no test that passed at the parent fails; at least one that failed passes; the test
                       files of the open REDs are unchanged. Name the targets: [tests: a, b]. Tests that
                       flipped beyond the named ones are a FAIL (a batch).
  <ticket>: PERF.      suite fully green before and after, no test file changed, and a gate record
                       [gate: path] that says fail -> pass.
  <ticket>: REFACTOR.  suite green before and after, no test file changed, the same set of tests.
  anything else        must not touch product code.

Run it under the memory cap, on your own cores (STANDARDS section 8):
  systemd-run --user --scope -q -p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=900 -- \
      taskset -c 6-7 python3 scripts/tdd/check-tdd.py ~/Projects/repos/anvil.js 8d484da..022aa4c

Exit: 0 all PASS, 1 any FAIL, 2 usage or environment error, 3 incomplete (--budget reached; run again,
results are cached per commit SHA).
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ADAPTER_VERSION = "4"
HERE = os.path.dirname(os.path.abspath(__file__))
ANSI = re.compile(r"\x1b\[[0-9;]*m")
TAG = re.compile(r"^(?P<ticket>[A-Za-z0-9._/-]+(?:,\s*[A-Za-z0-9._/-]+)*): (?P<kind>RED|GREEN|PERF|REFACTOR)\.")
GATE_TAG = re.compile(r"\[gate:\s*([^\]\s]+)\s*\]")


# ---------------------------------------------------------------------------------------------
# git helpers
# ---------------------------------------------------------------------------------------------
def git(repo, *args, check=True, text=True):
    p = subprocess.run(["git", "-C", repo, *args], capture_output=True, text=text)
    if check and p.returncode != 0:
        raise RuntimeError("git %s failed: %s" % (" ".join(args), p.stderr.strip()))
    return p.stdout


def under(path, prefixes):
    for pre in prefixes:
        if pre.endswith("/"):
            if path.startswith(pre):
                return True
        elif path == pre or path.startswith(pre + "/"):
            return True
    return False


# ---------------------------------------------------------------------------------------------
# test-output parsers (pure; unit-tested)
# ---------------------------------------------------------------------------------------------
def parse_node_events(lines, root=""):
    """JSON lines from node-reporter.js -> {id: {s, k, m}}.

    s: pass | fail | skip.  k (fails only): assertion | error | crash.  Children are reported before
    their parent, so a record's path is completed when its parent arrives."""
    done, pending = [], []
    for line in lines:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            ev = json.loads(line)
        except ValueError:
            continue
        kids = [p for p in pending if p["nesting"] > ev["nesting"]]
        pending = [p for p in pending if p["nesting"] <= ev["nesting"]]
        for kid in kids:
            kid["path"].insert(0, ev["name"])
            done.append(kid)
        ev["path"] = [ev["name"]]
        ev["container"] = bool(kids)
        pending.append(ev)
    done.extend(pending)
    tests, seen = {}, {}
    for ev in done:
        failed = ev["r"] == "fail"
        if ev["container"] and (not failed or ev.get("failureType") == "subtestsFailed"):
            continue
        if failed and ev.get("failureType") == "subtestsFailed":
            continue  # a suite that only reports its children's failures
        f = ev.get("file") or ""
        if root and f.startswith(root):
            f = f[len(root):].lstrip("/")
        is_file_crash = failed and ev["nesting"] == 0 and ev["name"] == f and not ev["container"]
        tid = f if is_file_crash else (f + " > " if f else "") + " > ".join(ev["path"])
        n = seen.get(tid, 0) + 1
        seen[tid] = n
        if n > 1:
            tid += " #%d" % n
        rec = {"s": ev["r"]}
        if failed:
            if is_file_crash:
                rec["k"] = "crash"
            elif ev.get("code") == "ERR_ASSERTION" or ev.get("errName") == "AssertionError":
                rec["k"] = "assertion"
            else:
                rec["k"] = "error"
            rec["m"] = "%s: %s" % (ev.get("errName") or ev.get("code") or "error", ev.get("message", ""))
        tests[tid] = rec
    return tests


def parse_testbit(text, suite, exit_code, build_ok=True):
    """Output of one native test binary built on testbit -> {id: {s, k, m}}.

    A test with a [TEST] line and no outcome line, in a process that did not exit cleanly, crashed."""
    tests = {}
    cur = None
    for line in ANSI.sub("", text).splitlines():
        s = line.strip()
        if s.startswith("[TEST]"):
            if cur is not None:
                tests[cur] = {"s": "fail", "k": "crash", "m": "no outcome (process ended)"}
            cur = "%s::%s" % (suite, s[len("[TEST]"):].strip())
        elif cur is not None and s.startswith("-- passed"):
            tests[cur] = {"s": "pass"}
            cur = None
        elif cur is not None and s.startswith("-- skipped"):
            tests[cur] = {"s": "skip"}
            cur = None
        elif cur is not None and s.startswith("-- FAILED"):
            tests[cur] = {"s": "fail", "k": "assertion", "m": s}
            cur = None
    if cur is not None:
        tests[cur] = {"s": "fail", "k": "crash", "m": "crashed or exited (status %s) inside the test" % exit_code}
    if not tests and exit_code != 0:
        tests["%s::<build>" % suite] = {"s": "fail", "k": "build", "m": "no test ran (build, link error or crash before the first test)"}
    return tests


# ---------------------------------------------------------------------------------------------
# adapters: run the repo's suite at the checked-out commit
# ---------------------------------------------------------------------------------------------
class Adapter:
    name = "?"
    product = []
    tests = []
    test_inputs = []  # product-side paths that are also test input (a vendored corpus): moving them can make a RED

    def run(self, work, opts):
        """-> {"ran": bool, "tests": {id: {s,k,m}}, "note": str}"""
        raise NotImplementedError


class NodeAdapter(Adapter):
    """anvil.js and other node:test repos: `npm test`, and `npm run compat` when package.json has it (anvil-node's own
    tests run against anvil.js; `npm test` does not cover them). Events come from the check-tdd reporter, written
    to a file by NODE_OPTIONS, so the inner `node --test` that the compat script spawns is captured too."""
    name = "node"
    product = ["src/", "node/", "js/", "vendor/anvil"]
    tests = ["test/"]
    test_inputs = ["vendor/anvil"]

    def _events(self, cmd, work, opts, root_strip, inner_tap=False):
        reporter = os.path.join(HERE, "node-reporter.js")
        fd, dest = tempfile.mkstemp(prefix="ctdd-ev-")
        os.close(fd)
        try:
            # `--test-reporter=` after the files would be taken as a script argument, so it goes in NODE_OPTIONS
            extra = "--test-reporter=%s --test-reporter-destination=%s" % (reporter, dest)
            if inner_tap:  # a script that passes its own --test-reporter: node wants one destination per reporter
                extra += " --test-reporter-destination=stdout"
            env = dict(os.environ, NODE_OPTIONS=(os.environ.get("NODE_OPTIONS", "") + " " + extra).strip())
            p = subprocess.run(cmd, cwd=work, capture_output=True, text=True, timeout=opts.run_timeout, env=env)
            with open(dest) as f:
                lines = f.read().splitlines()
        finally:
            os.unlink(dest)
        tests = parse_node_events(lines, root=root_strip)
        note = "" if tests else "%s produced no test events (exit %d): %s" % (" ".join(cmd), p.returncode, (p.stderr or p.stdout)[-300:].strip())
        return tests, note

    def run(self, work, opts):
        tests, note = self._events(["npm", "test", "--silent"], work, opts, os.path.realpath(work))
        try:
            with open(os.path.join(work, "package.json")) as f:
                scripts = json.load(f).get("scripts", {})
        except (OSError, ValueError):
            scripts = {}
        if tests and "compat" in scripts and not opts.no_compat:
            ctests, cnote = self._events(["npm", "run", "compat", "--silent"], work, opts, "", inner_tap=True)
            for tid, rec in ctests.items():
                tests["compat:" + re.sub(r"^/[^ ]*?anvil-compat-[^/]+/", "", tid)] = rec
            note = cnote
        return {"ran": bool(tests), "tests": tests, "note": note}


class NativeAdapter(Adapter):
    """ANVL Native: each test/unit/test_<suite>.c through `make -C test/unit test_<suite>` (testbit)."""
    name = "native"
    product = ["src/", "include/"]
    tests = ["test/"]

    def run(self, work, opts):
        unit = os.path.join(work, "test", "unit")
        suites = sorted(f[5:-2] for f in os.listdir(unit) if f.startswith("test_") and f.endswith(".c")) if os.path.isdir(unit) else []
        if not suites:
            return {"ran": False, "tests": {}, "note": "no test/unit/test_*.c"}
        shutil.rmtree(os.path.join(unit, "bin"), ignore_errors=True)
        tests = {}
        for s in suites:
            cmd = native_make_cmd(unit, s, opts)
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=opts.run_timeout)
            tests.update(parse_testbit(p.stdout, s, p.returncode))
        return {"ran": True, "tests": tests, "note": ""}


def pick_adapter(repo, name):
    if name == "node" or (name == "auto" and os.path.exists(os.path.join(repo, "package.json"))):
        return NodeAdapter()
    if name == "native" or (name == "auto" and os.path.isdir(os.path.join(repo, "test", "unit"))):
        return NativeAdapter()
    raise SystemExit("check-tdd: cannot pick an adapter for %s (use --adapter node|native)" % repo)


# ---------------------------------------------------------------------------------------------
# the rules (pure: parent result, commit result, facts -> verdict)
# ---------------------------------------------------------------------------------------------
def parse_tests_tag(msg):
    """Names in every `[tests: a, b]` of the message. The tag ends at its matching `]`: brackets inside names
    (`@[schema]`) nest. An unbalanced tag takes the rest of the line."""
    names = []
    for m in re.finditer(r"\[tests:", msg):
        depth, i, body = 1, m.end(), []
        while i < len(msg) and depth:
            c = msg[i]
            if c == "[":
                depth += 1
            elif c == "]":
                depth -= 1
                if depth == 0:
                    break
            elif c == "\n" and depth:
                break
            body.append(c)
            i += 1
        parts, cur, d = [], [], 0
        for c in "".join(body):
            if c == "[":
                d += 1
            elif c == "]":
                d -= 1
            if c == "," and d <= 0:
                parts.append("".join(cur))
                cur = []
            else:
                cur.append(c)
        parts.append("".join(cur))
        names += [x.strip() for x in parts if x.strip()]
    return names


def resolve_testbit(repo, given, fallbacks=()):
    """--testbit if given (it must exist), else ../testbit beside the repo, else the fallbacks; None if absent."""
    if given:
        return given if os.path.isdir(given) else None
    for c in [os.path.join(os.path.dirname(repo), "testbit")] + list(fallbacks):
        if os.path.isdir(c):
            return c
    return None


def native_make_cmd(unit, suite, opts):
    return ["make", "-C", unit, "test_" + suite, "TESTBIT=" + opts.testbit] + list(opts.make_var)


def cache_key(adapter_name, sha, opts):
    """Everything that changes what a run measures is in the key, so a bad-setup run cannot poison later ones."""
    parts = [adapter_name, "v" + ADAPTER_VERSION]
    if adapter_name == "native":
        h = hashlib.sha1(("\0".join([os.path.realpath(opts.testbit or "")] + list(opts.make_var))).encode()).hexdigest()[:10]
        parts.append("m" + h)
    elif getattr(opts, "no_compat", False):
        parts.append("nc")
    return "-".join(parts) + "-" + sha


def summarize(ids, cap=4):
    ids = sorted(ids)
    s = "; ".join(i if len(i) < 90 else i[:87] + "..." for i in ids[:cap])
    return s + (" (+%d more)" % (len(ids) - cap) if len(ids) > cap else "")


def name_matches(name, tid):
    return name == tid or name in tid


def judge(kind, parent, cur, facts, state, opts):
    """Return (reasons, notes, data). `parent`/`cur` are {"ran", "tests"}; `facts` has changed test
    files, product diff size, tests named, gate record. `state["open_reds"]` is updated."""
    reasons, notes, data = [], [], {}
    pt, ct = parent["tests"], cur["tests"]
    p_pass = {t for t, r in pt.items() if r["s"] == "pass"}
    p_fail = {t for t, r in pt.items() if r["s"] == "fail"}
    c_fail = {t for t, r in ct.items() if r["s"] == "fail"}
    c_pass = {t for t, r in ct.items() if r["s"] == "pass"}
    regress = {t for t in p_pass if t not in ct or ct[t]["s"] == "fail"}
    removed = {t for t in pt if t not in ct}
    newfail = {t for t in c_fail if t not in pt or pt[t]["s"] != "fail"}
    flipped = {t for t in p_fail if t in ct and ct[t]["s"] == "pass"}
    data.update(newly_failing=len(newfail), flipped=len(flipped), regressed=len(regress),
                product_lines=facts["product_lines"], test_files=len(facts["test_files"]))

    if kind in ("RED", "GREEN", "PERF", "REFACTOR") and not cur["ran"]:
        reasons.append("the suite did not run at this commit (%s)" % cur.get("note", ""))
        return reasons, notes, data

    if kind == "RED":
        if not facts["test_files"] and not facts["input_files"]:
            reasons.append("RED changes no test file")
        elif not facts["test_files"]:
            notes.append("RED moves a vendored test input (%s), no test file" % ", ".join(facts["input_files"]))
        if not newfail:
            reasons.append("no test newly fails (all tests that passed or were new still pass)")
        bad = {t for t in newfail if ct[t].get("k") != "assertion"}
        if bad:
            kinds = sorted({ct[t].get("k", "?") for t in bad})
            ex = "; ".join("%s [%s]" % (t if len(t) < 70 else t[:67] + "...", ct[t].get("m", "")[:70]) for t in sorted(bad)[:3])
            reasons.append("%d newly failing test(s) do not fail on an assertion (%s): %s" % (len(bad), "/".join(kinds), ex))
        if removed:
            notes.append("%d test(s) from the parent no longer run: %s" % (len(removed), summarize(removed)))
        if facts["product_lines"]:
            notes.append("product code changed (%d lines): stubs only?" % facts["product_lines"])
        if newfail and not bad:
            state["open_reds"].append({"sha": facts["sha"], "files": set(facts["test_files"]), "pending": set(newfail)})

    elif kind == "GREEN":
        if regress:
            reasons.append("%d test(s) that passed at the parent fail or are gone: %s" % (len(regress), summarize(regress)))
        if not flipped:
            reasons.append("no test that failed at the parent passes now")
        locked = set()
        for r in state["open_reds"]:
            locked |= {f for f in facts["test_files"] if f in r["files"]}
        if locked:
            reasons.append("test file(s) of an open RED changed: %s" % ", ".join(sorted(locked)))
        names = facts["named"]
        if not names:
            msg = "no [tests: ...] named"
            if opts.lenient_tests:
                notes.append(msg)
            else:
                reasons.append(msg)
            if len(flipped) > opts.batch_limit:
                reasons.append("batch: %d tests flipped at once (limit %d without [tests:])" % (len(flipped), opts.batch_limit))
        else:
            unmatched = [n for n in names if not any(name_matches(n, t) for t in flipped)]
            if unmatched:
                reasons.append("named test(s) did not flip: %s" % ", ".join(unmatched))
            extra = {t for t in flipped if not any(name_matches(n, t) for n in names)}
            data["flipped_beyond_named"] = len(extra)
            if extra:
                reasons.append("batch: %d test(s) flipped beyond the named: %s" % (len(extra), summarize(extra)))
        if facts["product_lines"] > opts.max_green_lines:
            reasons.append("batch: product diff of %d lines (limit %d) for %d flipped test(s)" % (facts["product_lines"], opts.max_green_lines, len(flipped)))
        notes.append("flipped %d, product diff %d lines" % (len(flipped), facts["product_lines"]))

    elif kind == "PERF":
        if p_fail or any(r["s"] == "fail" for r in pt.values()):
            reasons.append("suite not green at the parent (%d failing)" % len(p_fail))
        if c_fail:
            reasons.append("suite not green at the commit (%d failing)" % len(c_fail))
        if facts["test_files"]:
            reasons.append("test file(s) changed: %s" % ", ".join(sorted(facts["test_files"])[:4]))
        g = facts["gate"]
        if not g["path"]:
            reasons.append("no [gate: path] named")
        elif not g["exists"]:
            reasons.append("gate record %s not found" % g["path"])
        elif g["verdict"] != "fail->pass":
            reasons.append("gate record %s does not say fail -> pass (%s)" % (g["path"], g["verdict"] or "unreadable"))

    elif kind == "REFACTOR":
        if p_fail or c_fail:
            reasons.append("suite not green (parent %d, commit %d failing)" % (len(p_fail), len(c_fail)))
        if facts["test_files"]:
            reasons.append("test file(s) changed: %s" % ", ".join(sorted(facts["test_files"])[:4]))
        if set(pt) != set(ct):
            reasons.append("the test set differs (%d removed, %d added)" % (len(set(pt) - set(ct)), len(set(ct) - set(pt))))

    # update the open REDs: a RED stays open until every test it made fail passes
    for r in state["open_reds"]:
        r["pending"] = {t for t in r["pending"] if t in ct and ct[t]["s"] == "fail"}
    state["open_reds"] = [r for r in state["open_reds"] if r["pending"]]
    return reasons, notes, data


def judge_untagged(facts):
    if facts["product_files"]:
        return ["touches product code without a RED/GREEN/PERF/REFACTOR tag: %s" % summarize(facts["product_files"], 3)]
    return []


def read_gate(text):
    """A gate record says fail -> pass: JSON {before|parent: fail, after|commit: pass}, or text with 'fail -> pass'."""
    try:
        d = json.loads(text)
        if isinstance(d, dict):
            b = d.get("before", d.get("parent"))
            a = d.get("after", d.get("commit"))
            if str(b).lower() == "fail" and str(a).lower() == "pass":
                return "fail->pass"
            return "%s->%s" % (str(b).lower(), str(a).lower())
    except ValueError:
        pass
    if re.search(r"fail\w*\s*(->|=>|→)\s*pass", text, re.I):
        return "fail->pass"
    return "no fail->pass"


# ---------------------------------------------------------------------------------------------
# the run
# ---------------------------------------------------------------------------------------------
class Runner:
    def __init__(self, repo, adapter, opts, scratch):
        self.repo, self.adapter, self.opts, self.scratch = repo, adapter, opts, scratch
        self.work = os.path.join(scratch, "work")
        self.cache = opts.cache
        self.started = time.time()
        self.cloned = False
        self.runs = 0
        self.hits = 0

    def _clone(self):
        if self.cloned:
            return
        subprocess.run(["git", "clone", "-q", "--shared", "--no-checkout", self.repo, self.work], check=True, capture_output=True)
        self.cloned = True

    def _submodule_source(self, sha):
        for c in (os.path.join(self.repo, ".git", "modules", "vendor", "anvil"), os.path.join(self.repo, "vendor", "anvil"),
                  os.path.join(os.path.dirname(self.repo), "anvil")):
            if os.path.isdir(c) and subprocess.run(["git", "-C", c, "cat-file", "-e", sha + "^{commit}"], capture_output=True).returncode == 0:
                return c
        return None

    def _populate_submodule(self, sha):
        """vendor/anvil is a gitlink: fill it offline, as a shared clone (a real git checkout, as some tests read its history)."""
        out = git(self.work, "ls-tree", sha, "vendor/anvil", check=False).split()
        if len(out) < 3 or out[0] != "160000":
            return ""
        src = self._submodule_source(out[2])
        dest = os.path.join(self.work, "vendor", "anvil")
        os.makedirs(dest, exist_ok=True)
        if not src:
            return "vendor/anvil pin %s not available" % out[2][:7]
        shutil.rmtree(dest, ignore_errors=True)
        c = subprocess.run(["git", "clone", "-q", "--shared", "--no-checkout", src, dest], capture_output=True)
        if c.returncode == 0:
            c = subprocess.run(["git", "-C", dest, "checkout", "-q", "--detach", out[2]], capture_output=True)
        if c.returncode != 0:
            return "vendor/anvil pin %s not found in %s" % (out[2][:7], src)
        return ""

    def result(self, sha):
        key = os.path.join(self.cache, cache_key(self.adapter.name, sha, self.opts) + ".json")
        if os.path.exists(key):
            self.hits += 1
            with open(key) as f:
                return json.load(f)
        if self.opts.budget and time.time() - self.started > self.opts.budget:
            raise BudgetReached()
        self._clone()
        git(self.work, "checkout", "-q", "-f", "--detach", sha)
        git(self.work, "clean", "-fdxq")
        miss = self._populate_submodule(sha)
        t0 = time.time()
        try:
            res = self.adapter.run(self.work, self.opts)
        except subprocess.TimeoutExpired:
            res = {"ran": False, "tests": {}, "note": "suite timed out (%ds)" % self.opts.run_timeout}
        if miss:
            res["note"] = (res.get("note", "") + " " + miss).strip()
        res["seconds"] = round(time.time() - t0, 1)
        res["sha"] = sha
        os.makedirs(self.cache, exist_ok=True)
        with open(key + ".tmp", "w") as f:
            json.dump(res, f)
        os.replace(key + ".tmp", key)
        self.runs += 1
        return res


class BudgetReached(Exception):
    pass


def commit_facts(repo, adapter, sha, parent):
    base = parent if parent else git(repo, "hash-object", "-t", "tree", "/dev/null").strip()  # the empty tree
    files = [f for f in git(repo, "diff", "--name-only", "--no-renames", base, sha).splitlines() if f]
    test_files = [f for f in files if under(f, adapter.tests)]
    input_files = [f for f in files if under(f, adapter.test_inputs)]
    product_files = [f for f in files if under(f, adapter.product)]
    lines = 0
    if product_files:
        for ln in git(repo, "diff", "--numstat", "--no-renames", base, sha, "--", *adapter.product).splitlines():
            a, d, _ = (ln.split("\t") + ["", "", ""])[:3]
            lines += (int(a) if a.isdigit() else 0) + (int(d) if d.isdigit() else 0)
        if lines == 0:
            lines = 1  # a gitlink or binary change
    msg = git(repo, "log", "-1", "--format=%B", sha)
    named = []
    named = parse_tests_tag(msg)
    gm = GATE_TAG.search(msg)
    gate = {"path": gm.group(1) if gm else None, "exists": False, "verdict": None}
    return {"sha": sha, "test_files": test_files, "input_files": input_files, "product_files": product_files, "product_lines": lines,
            "named": named, "gate": gate, "subject": msg.splitlines()[0] if msg else ""}


def resolve_gate(repo, sha, gate, gate_root):
    if not gate["path"]:
        return
    p = subprocess.run(["git", "-C", repo, "show", "%s:%s" % (sha, gate["path"])], capture_output=True, text=True)
    text = p.stdout if p.returncode == 0 else None
    if text is None:
        for root in (gate_root, repo):
            if root and os.path.isfile(os.path.join(root, gate["path"])):
                with open(os.path.join(root, gate["path"])) as f:
                    text = f.read()
                break
    if text is not None:
        gate["exists"] = True
        gate["verdict"] = read_gate(text)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="check-tdd", description="Mechanical checker of the strict-TDD protocol (STANDARDS section 3).")
    ap.add_argument("repo")
    ap.add_argument("range", help="<base>..<head>: the commits reachable from head and not from base")
    ap.add_argument("--adapter", default="auto", choices=["auto", "node", "native"])
    ap.add_argument("--first-parent", action="store_true", help="walk first-parent commits only (default: every non-merge commit in the range, oldest first)")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    ap.add_argument("--cache", default=os.path.join(tempfile.gettempdir(), "check-tdd-cache-%d" % os.getuid()))
    ap.add_argument("--scratch", help="parent directory for the scratch clone (default: a fresh temp dir, removed afterwards)")
    ap.add_argument("--budget", type=int, default=0, help="stop starting new suite runs after this many seconds (exit 3; rerun resumes from the cache)")
    ap.add_argument("--run-timeout", type=int, default=900, help="per suite run, seconds")
    ap.add_argument("--no-compat", action="store_true", help="node: skip `npm run compat` (anvil-node's tests against anvil.js)")
    ap.add_argument("--lenient-tests", action="store_true", help="a GREEN without [tests: ...] is a note, not a FAIL (for history that predates the syntax)")
    ap.add_argument("--batch-limit", type=int, default=10, help="GREEN with no [tests:] fails when more than this many tests flip")
    ap.add_argument("--max-green-lines", type=int, default=150, help="a GREEN whose product diff is larger than this is a batch")
    ap.add_argument("--make-var", action="append", default=[], metavar="NAME=VALUE", help="native: extra make variable, repeatable (e.g. BUILD_NUM=500)")
    ap.add_argument("--testbit", default=None, help="native: testbit checkout (default: ../testbit beside the repo, then ~/Projects/repos/testbit; not found is an error)")
    ap.add_argument("--gate-root", default=None, help="directory to look for [gate: path] records not committed to the repo")
    opts = ap.parse_args(argv)

    repo = os.path.realpath(opts.repo)
    if ".." not in opts.range or opts.range.count("..") != 1:
        print("check-tdd: range must be <base>..<head>", file=sys.stderr)
        return 2
    base, head = opts.range.split("..")
    try:
        adapter = pick_adapter(repo, opts.adapter)
        base_sha = git(repo, "rev-parse", base + "^{commit}").strip()
        walk = ["rev-list", "--reverse"] + (["--first-parent"] if opts.first_parent else ["--no-merges", "--topo-order"]) + ["%s..%s" % (base_sha, head)]
        shas = git(repo, *walk).split()
    except (RuntimeError, SystemExit) as e:
        print("check-tdd: %s" % e, file=sys.stderr)
        return 2
    if adapter.name == "native":
        opts.testbit = resolve_testbit(repo, opts.testbit, [os.path.expanduser("~/Projects/repos/testbit")])
        if not opts.testbit:
            print("check-tdd: testbit not found (looked beside the repo and in ~/Projects/repos/testbit); pass --testbit DIR", file=sys.stderr)
            return 2
        opts.testbit = os.path.realpath(opts.testbit)
    os.makedirs(opts.cache, exist_ok=True)

    scratch = tempfile.mkdtemp(prefix="check-tdd-", dir=opts.scratch)
    runner = Runner(repo, adapter, opts, scratch)
    state = {"open_reds": []}
    records, incomplete = [], False
    t_start = time.time()
    try:
        for sha in shas:
            parents = git(repo, "rev-list", "--parents", "-n", "1", sha).split()[1:]
            msg = git(repo, "log", "-1", "--format=%B", sha)
            subject = msg.splitlines()[0] if msg else ""
            rec = {"sha": sha[:7], "subject": subject}
            m = TAG.match(subject)
            kind = m.group("kind") if m else None
            parent = parents[0] if parents else None
            facts = commit_facts(repo, adapter, sha, parent)
            if len(parents) > 1:
                rec.update(status="PASS", kind="MERGE", reasons=[], notes=["merge commit, not classified"])
                records.append(rec)
                continue
            if kind is None:
                reasons = judge_untagged(facts)
                rec.update(status="FAIL" if reasons else "PASS", kind="-", reasons=reasons, notes=[] if reasons else ["no product code touched"])
                records.append(rec)
                continue
            try:
                pres = runner.result(parent) if parent else {"ran": True, "tests": {}}
                cres = runner.result(sha)
            except BudgetReached:
                incomplete = True
                break
            resolve_gate(repo, sha, facts["gate"], opts.gate_root)
            reasons, notes, data = judge(kind, pres, cres, facts, state, opts)
            if not pres.get("ran", True) and kind != "RED":
                notes.append("parent suite did not run")
            rec.update(status="FAIL" if reasons else "PASS", kind=kind, reasons=reasons, notes=notes, data=data,
                       seconds=cres.get("seconds"))
            records.append(rec)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    fails = [r for r in records if r["status"] == "FAIL"]
    summary = {"repo": repo, "range": opts.range, "adapter": adapter.name, "commits": len(shas), "checked": len(records),
               "pass": len(records) - len(fails), "fail": len(fails), "incomplete": incomplete,
               "suite_runs": runner.runs, "cache_hits": runner.hits, "seconds": round(time.time() - t_start, 1)}
    if opts.json:
        json.dump({"summary": summary, "commits": records}, sys.stdout, indent=1)
        print()
    else:
        for r in records:
            detail = "; ".join(r["reasons"] + ([] if r["status"] == "FAIL" else r["notes"]))
            print("%s %s %-8s %s%s" % (r["status"], r["sha"], r["kind"], r["subject"][:70], (" | " + detail) if detail else ""))
        print("-- %(pass)d PASS, %(fail)d FAIL of %(checked)d/%(commits)d commits; %(suite_runs)d suite runs, %(cache_hits)d cached, %(seconds)ss%(inc)s" %
              dict(summary, inc="; INCOMPLETE (budget reached, rerun)" if incomplete else ""))
    if fails:
        return 1
    return 3 if incomplete else 0


if __name__ == "__main__":
    sys.exit(main())
