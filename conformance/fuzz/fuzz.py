#!/usr/bin/env python3
"""Differential fuzz harness for the anvil-dump format (conformance/FORMAT.md).

Generates inputs (a grammar-aware generator, and a byte-level mutator over the conformance corpus), runs two
executables on each, and reports the first difference between their dumps, minimised. Any command that reads
the same input and prints the same format works as either side; the default is native's anvil-dump.

  fuzz.py --a build/tools/anvil-dump --b "node js/dump.js" --cases 1000000 --seed 1 --jobs 8

An executable is run in batch mode by default: `CMD --batch` with one input path per line on stdin, and the
output a sequence of `@@ <path>` lines each followed by that input's dump (anvil-dump implements it; a port
that does not can be run per input with --no-batch). Per input, `CMD [--fragment] <path>` is the plain form;
a `.frag` input is a value fragment.

Memory is bounded: at most jobs * 2 batches are in flight, a child gets 1 GiB of address space and may write 64 MiB, a case is at most 20,000 bytes and 6 levels deep. A tool failure (a crash, a timeout, stderr output) counts as a difference even when both sides fail alike.
Exit status: 0 when no difference was found, 1 when one was, 2 on a harness error.
Python 3 standard library only.
"""
import argparse, collections, difflib, os, random, resource, shlex, shutil, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor, FIRST_COMPLETED, wait

# Hard bounds: a child gets at most CHILD_MEMORY of address space and writes at most MAX_OUTPUT bytes (a larger
# output is a failure of the tool), a generated case is at most MAX_CASE bytes and MAX_DEPTH deep.
CHILD_MEMORY = 1 << 30
MAX_OUTPUT = 64 << 20
MAX_CASE = 20000
MAX_DEPTH = 6

# ---------------------------------------------------------------------------------------------- generator

INTERESTING = [b'"', b'`', b'@[', b'@', b']', b'[', b'{', b'}', b'(', b')', b':=', b':', b';', b',', b'$', b'$name', b'//', b'/*', b'*/',
               b'\\', b'\\n', b'\\"', b'#!aml\n', b'#!amp\n', b'e+', b'e-', b'E', b'.', b'..', b'-', b'+', b'0', b'9', b'\x00', b'\n', b'\r\n',
               b'\t', b' ', b'\xff', b'\x80', b'\xc3\xa9', b'true', b'false', b'null', b'include', b'vars', b'import', b'=', b'=>', b'#']
IDENTS = [b'a', b'b', b'x', b'name', b'item', b'k1', b'_u', b'big_name_with_underscores', b'Z', b'e']
ODD_IDENTS = [b'dotted.name', b'dash-name']
KEYWORDS = [b'include', b'vars', b'import', b'true', b'false', b'null']

def gen_number(r):
    kind = r.randrange(9)
    sign = r.choice([b'', b'', b'', b'-'] if r.clean else [b'', b'', b'', b'-', b'+'])
    digits = lambda n: bytes(r.choice(b'0123456789') for _ in range(n))
    if kind == 0: return sign + str(r.randrange(0, 10)).encode()
    if kind == 1: return sign + digits(r.randrange(1, 22))
    if kind == 2: return sign + digits(r.randrange(1, 6)) + b'.' + digits(r.randrange(1 if r.clean else 0, 6))
    if kind == 3: return sign + digits(r.randrange(1, 5)) + r.choice([b'e', b'E']) + r.choice([b'', b'+', b'-']) + digits(r.randrange(1 if r.clean else 0, 4))
    if kind == 4: return sign + digits(r.randrange(0, 3)) + b'.' + digits(r.randrange(1, 18)) + r.choice([b'', b'e5', b'E-7', b'e+22', b'e308', b'e-324', b'e999'])
    if kind == 5: return r.choice([b'9007199254740993', b'9223372036854775807', b'9223372036854775808', b'-9223372036854775809', b'18446744073709551615',
                                   b'1.7976931348623157e308', b'5e-324', b'0.1', b'0.30000000000000004', b'1e22', b'1e23', b'4.35', b'2.675', b'123456789012345678'])
    if kind == 6: return b'0' * r.randrange(1, 40) + digits(r.randrange(1, 4))
    if kind == 7 and not r.clean: return r.choice([b'1_0', b'0x1F', b'1e', b'.5', b'5.', b'--1', b'1.2.3', b'1f', b'NaN', b'Infinity', b'#12D4F', b'1,5'])
    return digits(r.randrange(1, 4)) + b'.' + digits(r.randrange(1, 4)) + b'e' + r.choice([b'', b'-', b'+']) + digits(r.randrange(1, 3))

def gen_string(r):
    pieces = []
    for _ in range(r.randrange(0, 8)):
        c = r.randrange(12)
        if c < 5: pieces.append(bytes(r.choice(b'abcXYZ 019_-') for _ in range(r.randrange(1, 6))))
        elif c < 8: pieces.append(r.choice([b'\\n', b'\\t', b'\\r', b'\\\\', b'\\"']))
        elif c == 8: pieces.append(r.choice([b'\\x', b'\\u00e9', b'\\0', b"\\'", b'\\ ', b'\\']))
        elif c == 9: pieces.append(r.choice([b'\xc3\xa9', b'\xe2\x82\xac', b'\xf0\x9f\x98\x80', b'\xff', b'\x00', b'\n', b'\t', b'\x7f']))
        else: pieces.append(r.choice([b'{', b']', b'`', b'@[a]', b'$x', b'//', b'/*', b';', b':=']))
    return b'"' + b''.join(pieces) + b'"'

def gen_blob(r):
    tag = r.choice([b'', b'', b'@date', b'@t', b'@b64', b'@a_b'] + ([] if r.clean else [b'@' + b'a' * r.choice([30, 31, 32, 33]), b'@1', b'@', b'@A-b']))
    body = bytes(r.choice(b'abc 019\n=+/\x00\xff\x80') for _ in range(r.randrange(0, 12)))
    return tag + b'`' + body + (b'`' if r.clean else r.choice([b'`', b'`', b'`', b'']))

def ws(r):
    if r.clean: return r.choice([b' ', b' ', b'  ', b''])
    return r.choice([b' ', b' ', b' ', b'\n', b'  ', b'\t', b' /* c */ ', b' // c\n', b''])

def gen_value(r, depth, names):
    k = r.randrange(16 if depth < 3 else 8)
    if depth > MAX_DEPTH: k = 2
    if k == 0: return r.choice([b'true', b'false', b'null', b'True', b'NULL'])
    if k == 1: return r.choice([b'hello', b'a.b', b'a-b', b'x1', b'\xc3\xa9t\xc3\xa9', b'1a', b'09-02-2026', b'a/b', b'a:b'] + ([] if r.clean else [r.choice(KEYWORDS)]))
    if k in (2, 3, 4): return gen_number(r)
    if k in (5, 6): return gen_string(r)
    if k == 7: return gen_blob(r)
    if k == 8 and names: return b'$' + r.choice(names)
    if k == 8: return r.choice([b'$nothing'] if r.clean else [b'$nothing', b'$', b'${x}', b'$"hi {x}"'])
    if k in (9, 10):
        n = r.choice([0, 1, 1, 2, 2, 3, 5])
        n = max(n, 1) if r.clean else n
        return b'[' + (b',' + ws(r)).join(gen_value(r, depth + 1, names) for _ in range(n)) + (b'' if r.clean else r.choice([b'', b'', b','])) + b']'
    if k == 11:
        n = r.choice([0, 1, 2, 2, 3, 4])
        n = max(n, 2) if r.clean else n
        return b'(' + (b',' + ws(r)).join(gen_value(r, depth + 1, names) for _ in range(n)) + (b'' if r.clean else r.choice([b'', b'', b','])) + b')'
    return gen_object(r, depth + 1, names)

def gen_attrs(r):
    if r.randrange(4): return b''
    items = []
    for _ in range(r.choice([1, 1, 2, 3] if r.clean else [0, 1, 1, 2, 3])):
        key = r.choice(IDENTS if r.clean else IDENTS + [b'1bad'])
        v = r.randrange(8)
        if v == 0: items.append(key)
        elif v == 1: items.append(key + b'=' + gen_number(r))
        elif v == 2: items.append(key + b'=' + gen_string(r))
        elif v == 3: items.append(key + b'=' + r.choice([b'true', b'null', b'word', b'a.b'] if r.clean else [b'true', b'null', b'word', b'1..5', b'1 2', b'a.b']))
        elif v == 4: items.append(key + b'=' + gen_blob(r))
        elif v == 5: items.append(key + b'=' + (b'1' if r.clean else b'$x'))
        else: items.append(key + b'=' + gen_number(r))
    return b'@[' + b', '.join(items) + b']'

def gen_stmt(r, depth, names, top):
    if depth > MAX_DEPTH: return b'x :=1;'
    name = r.choice(IDENTS) if (r.clean or r.randrange(12)) else r.choice(KEYWORDS + ODD_IDENTS + [b'1x', b''])
    if top and r.clean:  # top-level names must be unique in a valid document
        r.counter += 1
        name = name + b'%d' % r.counter
    if top: r.top_name = name
    attrs = gen_attrs(r)
    base = b''
    if names and not r.clean and r.randrange(8) == 0: base = b' : ' + r.choice(names)
    form = r.randrange(10)
    if form == 0:
        body = b''.join(gen_stmt(r, depth + 1, names, False) for _ in range(r.randrange(1 if r.clean else 0, 4)))
        return name + (b' ' + attrs if attrs else b'') + base + b' {' + body + b'}' + (b';' if r.clean else r.choice([b';', b';', b';', b'']))
    value = gen_value(r, depth, names) if not base else gen_object(r, depth + 1, names)
    return name + (b' ' + attrs if attrs else b'') + base + ws(r) + b':=' + ws(r) + value + (b';' if r.clean else r.choice([b';'] * 12 + [b'', b';;']))

def gen_object(r, depth, names):
    n = r.choice([1, 1, 2, 3] if r.clean else [0, 1, 1, 2, 3])
    return b'{' + ws(r) + b''.join(gen_stmt(r, depth + 1, names, False) + ws(r) for _ in range(n)) + b'}'

def gen_doc(r):
    r.counter = 0; r.top_name = b''
    out = []
    dialect = b'aml' if r.clean else r.choice([b'aml'] * 8 + [b'amp', b'asl', b'xyz'])
    if r.clean or r.randrange(12): out.append(b'#!' + dialect + (b'\n' if r.clean else r.choice([b'\n'] * 6 + [b'\r\n', b' \n', b''])))
    for _ in range(r.choice([0, 0, 1, 2])): out.append(b'@[doc]\n' if r.clean else (gen_attrs(r) or b'@[doc]'))
    names = []
    for _ in range(r.choice([0, 1, 2, 3, 4, 6])):
        s = gen_stmt(r, 0, names, True)
        out.append(s)
        top_name = r.top_name
        if r.randrange(4) and top_name: names.append(top_name)
        out.append(r.choice([b'\n'] * 5 + [b'\n\n']) if r.clean else r.choice([b'\n'] * 5 + [b'\n\n', b'\r\n', b' ', b'']))
        if r.randrange(10) == 0: out.append(r.choice([b'// comment\n', b'/* block */\n'] if r.clean else [b'// comment\n', b'/* block */\n', b'/* unterminated\n']))
    return b''.join(out)

def gen_fragment(r):
    return gen_value(r, 0, []) + r.choice([b'', b'', b';', b' ', b'\n'] if r.clean else [b'', b'', b';', b' ', b'\n', b' x'])

# ---------------------------------------------------------------------------------------------- mutator

def mutate(r, data, corpus):
    data = bytearray(data)
    for _ in range(r.choice([1, 1, 1, 2, 3, 5])):
        op = r.randrange(11)
        n = len(data)
        if op == 0 and n: data[r.randrange(n)] ^= 1 << r.randrange(8)
        elif op == 1 and n: data[r.randrange(n)] = r.randrange(256)
        elif op == 2: data[r.randrange(n + 1):0] = r.choice(INTERESTING)
        elif op == 3 and n:
            a = r.randrange(n); b = min(n, a + r.randrange(1, 9)); del data[a:b]
        elif op == 4 and n:
            a = r.randrange(n); b = min(n, a + r.randrange(1, 17)); data[b:b] = data[a:b]
        elif op == 5 and n > 1:
            a = r.randrange(n); b = r.randrange(n); data[a], data[b] = data[b], data[a]
        elif op == 6 and n: del data[r.randrange(n):]
        elif op == 7 and n and corpus:
            other = r.choice(corpus); a = r.randrange(len(other) + 1); b = min(len(other), a + r.randrange(1, 40))
            data[r.randrange(n + 1):0] = other[a:b]
        elif op == 8 and n:
            a = r.randrange(n); data[a:a + 1] = r.choice(INTERESTING)
        elif op == 9 and n:  # nudge a digit
            idx = [i for i, c in enumerate(data) if 48 <= c <= 57]
            if idx: data[r.choice(idx)] = r.choice(b'0123456789')
        elif op == 10 and n:  # swap a delimiter for another
            idx = [i for i, c in enumerate(data) if c in b'[](){}"`;,:@$']
            if idx: data[r.choice(idx)] = r.choice(b'[](){}"`;,:@$')
        if len(data) > 20000: del data[20000:]
    return bytes(data)

def make_case(seed, index, corpus):
    ext, data = _make_case(seed, index, corpus)
    return ext, data[:MAX_CASE]

def _make_case(seed, index, corpus):
    r = random.Random(seed * 1000003 + index)
    r.clean = r.randrange(100) < 55  # clean cases stay inside the grammar; the rest also inject errors
    pick = r.randrange(100)
    if pick < 12: return 'frag', gen_fragment(r)
    if pick < 22:
        ext, base = 'frag', gen_fragment(r)
        return ext, mutate(r, base, [c[1] for c in corpus])
    if pick < 55: return 'anvl', gen_doc(r)
    if pick < 70: return 'anvl', mutate(r, gen_doc(r), [c[1] for c in corpus])
    ext, data = r.choice(corpus)
    return ext, mutate(r, data, [c[1] for c in corpus]) if r.randrange(10) else data

# ---------------------------------------------------------------------------------------------- running

def _limits():
    resource.setrlimit(resource.RLIMIT_AS, (CHILD_MEMORY, CHILD_MEMORY))
    resource.setrlimit(resource.RLIMIT_FSIZE, (MAX_OUTPUT, MAX_OUTPUT))

def run_limited(args, timeout, stdin=None, cwd=None):
    """Runs a command with bounded memory and output (stdout goes to a file, not into this process); returns (stdout, stderr, returncode)."""
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        p = subprocess.Popen(args, stdin=subprocess.PIPE if stdin is not None else subprocess.DEVNULL, stdout=out, stderr=err, cwd=cwd, preexec_fn=_limits)
        try:
            p.communicate(stdin, timeout=timeout)
        except subprocess.TimeoutExpired:
            p.kill(); p.wait(); return b'[timeout]\n', b'', -9
        out.seek(0); err.seek(0)
        return out.read(MAX_OUTPUT + 1), err.read(4096), p.returncode

def run_plain(cmd, path, timeout):
    args = cmd + (['--fragment'] if path.endswith('.frag') else []) + [path]
    so, se, rc = run_limited(args, timeout)
    if so == b'[timeout]\n': return so
    return so[:MAX_OUTPUT] + (b'\n[stderr]\n' + se if se else b'') + (b'\n[exit %d]\n' % rc if rc else b'')

def run_batch(cmd, paths, timeout, cwd):
    so, se, rc = run_limited(cmd + ['--batch'], timeout * max(1, len(paths) // 50 + 1), stdin=('\n'.join(paths) + '\n').encode(), cwd=cwd)
    p = collections.namedtuple('P', 'returncode stderr')(rc, se)
    out = {}
    cur = None
    chunks = {}
    for line in so.splitlines(keepends=True):
        if line.startswith(b'@@ '):
            cur = line[3:].rstrip(b'\n').decode(); chunks[cur] = []
        elif cur is not None: chunks[cur].append(line)
    for path in paths: out[path] = b''.join(chunks.get(path, [b'[no output]\n']))
    return out, p

def dumps_for(cmd, paths, args, cwd):
    if args.no_batch:
        return {p: run_plain(cmd, p, args.timeout) for p in paths}
    out, p = run_batch(cmd, paths, args.timeout, cwd)
    if p.returncode != 0 or p.stderr:
        # a crash or tool error somewhere in the batch: fall back to per input so the culprit is identified
        return {path: run_plain(cmd, path, args.timeout) for path in paths}
    return out

def failed(dump):
    """A crash, a timeout, output on stderr or no output at all is a failure of the tool, equal on both sides or not."""
    return b'[exit ' in dump or b'[timeout]' in dump or b'[stderr]' in dump or b'[no output]' in dump

def differs(args, ext, data, tmp, tag='min'):
    path = os.path.join(tmp, '%s.%s' % (tag, ext))
    open(path, 'wb').write(data)
    rel = os.path.relpath(path)
    da, db = run_plain(args.a, rel, args.timeout), run_plain(args.b, rel, args.timeout)
    return da != db or failed(da) or failed(db)

def minimise(args, ext, data, tmp):
    """ddmin over bytes: the smallest input found that still makes the two dumps differ."""
    budget = args.min_budget
    chunk = max(1, len(data) // 2)
    while chunk >= 1 and budget > 0:
        i = 0
        shrunk = False
        while i < len(data) and budget > 0:
            cand = data[:i] + data[i + chunk:]
            budget -= 1
            if cand != data and differs(args, ext, cand, tmp):
                data = cand; shrunk = True
            else:
                i += chunk
        if not shrunk or chunk == 1:
            if chunk == 1: break
            chunk //= 2
    return data

def esc(b):
    return ''.join(chr(c) if 32 <= c < 127 and chr(c) not in '\\"' else {10: '\\n', 13: '\\r', 9: '\\t', 92: '\\\\', 34: '\\"'}.get(c, '\\x%02x' % c) for c in b)

def report(args, n, ext, data, da, db, tmp):
    print('DIFFERENCE in case %d (%s, %d bytes)' % (n, ext, len(data)))
    if not args.no_minimise:
        small = minimise(args, ext, data, tmp)
        if small != data:
            data = small
            path = os.path.join(tmp, 'rep.%s' % ext); open(path, 'wb').write(data)
            rel = os.path.relpath(path)
            da, db = run_plain(args.a, rel, args.timeout), run_plain(args.b, rel, args.timeout)
        print('minimised to %d bytes' % len(data))
    print('input (%s): "%s"' % (ext, esc(data)))
    diff = list(difflib.unified_diff(da.decode('utf-8', 'replace').splitlines(), db.decode('utf-8', 'replace').splitlines(), 'a', 'b', lineterm='', n=1))
    print('\n'.join(diff[:30]))
    if args.out:
        os.makedirs(args.out, exist_ok=True)
        base = os.path.join(args.out, 'diff-%04d' % n)
        open(base + '.' + ext, 'wb').write(data)
        open(base + '.a.dump', 'wb').write(da); open(base + '.b.dump', 'wb').write(db)
        print('saved %s.%s (+ .a.dump, .b.dump)' % (base, ext))
    print()
    sys.stdout.flush()

def load_corpus(root):
    out = []
    for dp, _, fns in os.walk(root):
        for fn in sorted(fns):
            if fn.endswith('.dump'): continue
            ext = 'frag' if fn.endswith('.frag') else ('amp' if fn.endswith('.amp') else 'anvl')
            try: data = open(os.path.join(dp, fn), 'rb').read()
            except OSError: continue
            if len(data) <= 20000: out.append((ext, data))
    out.sort()
    return out

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--a', required=True, help='first executable (a command line)')
    ap.add_argument('--b', required=True, help='second executable (a command line)')
    ap.add_argument('--cases', type=int, default=10000)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--batch', type=int, default=500, help='inputs per process run')
    ap.add_argument('--jobs', type=int, default=1)
    ap.add_argument('--corpus', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'cases'))
    ap.add_argument('--out', help='save each difference (minimised input and both dumps) here')
    ap.add_argument('--max-diffs', type=int, default=1, help='stop after this many differences (default: the first)')
    ap.add_argument('--no-batch', action='store_true')
    ap.add_argument('--no-minimise', action='store_true')
    ap.add_argument('--min-budget', type=int, default=3000, help='most runs spent minimising one difference')
    ap.add_argument('--timeout', type=int, default=20)
    ap.add_argument('--keep', action='store_true', help='keep the scratch directory')
    args = ap.parse_args()
    args.a = shlex.split(args.a); args.b = shlex.split(args.b)
    corpus = load_corpus(args.corpus)
    if not corpus: print('no corpus at', args.corpus, file=sys.stderr); return 2
    tmp = tempfile.mkdtemp(prefix='anvil-fuzz-')
    for side in ('a', 'b'):  # resolve a relative executable before changing directory
        cmd = getattr(args, side)
        if os.path.exists(cmd[0]): cmd[0] = os.path.abspath(cmd[0])
    os.chdir(tmp)  # the executables run here, so a relative include resolves in a scratch dir
    diffs = [0]; done = [0]

    def run_batch_index(bi):
        if diffs[0] >= args.max_diffs: return []
        lo = bi * args.batch; hi = min(args.cases, lo + args.batch)
        bdir = os.path.join(tmp, 'b%d' % bi); os.makedirs(bdir)
        cases = []
        for i in range(lo, hi):
            ext, data = make_case(args.seed, i, corpus)
            path = os.path.join('b%d' % bi, 'c%07d.%s' % (i, ext)); open(path, 'wb').write(data)
            cases.append((i, ext, data, path))
        paths = [c[3] for c in cases]
        da = dumps_for(args.a, paths, args, tmp); db = dumps_for(args.b, paths, args, tmp)
        bad = [(i, ext, data, da[p], db[p]) for i, ext, data, p in cases if da[p] != db[p] or failed(da[p]) or failed(db[p])]
        shutil.rmtree(bdir, ignore_errors=True)
        done[0] += len(cases)
        return bad

    nb = (args.cases + args.batch - 1) // args.batch
    try:
        # at most jobs * 2 batches in flight; a result is dealt with and dropped as soon as it completes
        with ThreadPoolExecutor(max_workers=args.jobs) as ex:
            pending = set(); nxt = 0
            while (nxt < nb or pending) and diffs[0] < args.max_diffs:
                while nxt < nb and len(pending) < args.jobs * 2:
                    pending.add(ex.submit(run_batch_index, nxt)); nxt += 1
                finished, pending = wait(pending, return_when=FIRST_COMPLETED)
                for fut in finished:
                    for (i, ext, data, a, b) in fut.result():
                        if diffs[0] >= args.max_diffs: break
                        diffs[0] += 1
                        report(args, i, ext, data, a, b, tmp)
            for fut in pending: fut.cancel()
    finally:
        if not args.keep: shutil.rmtree(tmp, ignore_errors=True)
    print('fuzz: cases=%d differences=%d seed=%d a=%s b=%s' % (done[0], diffs[0], args.seed, ' '.join(args.a), ' '.join(args.b)))
    return 1 if diffs[0] else 0

if __name__ == '__main__':
    sys.exit(main())
