#!/usr/bin/env python3
"""Seeds conformance/cases/unit/ from the inline inputs of the native unit tests (one-shot; the output is
committed, so nothing needs Python at check time).

An input is taken when it is a C string literal (adjacent literals joined, escapes decoded, NUL bytes kept) that

  - starts with a "#!" dialect directive: a whole document, wherever it appears; or
  - is the first argument of a loader that takes document text (anvil_load_buffer, LOAD_LIT, ATTR_LOAD,
    setup_registered_doc, make_doc, aml_number_error): a whole document; or
  - is the first argument of a value-fragment parser (anvil_parse_value_fragment, expect_fragment_error,
    setup_fragment_doc): a fragment (.frag); or
  - is the first argument of expect_int / expect_double / expect_overflow: a number, wrapped in
    "#!aml\\n\\nx := <literal>;\\n" exactly as the test helper wraps it (.anvl); or
  - sits in the literal table of test/infra/numeric_fastpath/edge.c (same wrapping).

Inputs are deduplicated by content. Usage: extract_inline.py <repo-root> [--force]
It refuses to run over an existing cases/unit (the committed corpus keeps its names and dumps); --force wipes it first.
"""
import hashlib, os, re, sys

root = sys.argv[1]
out_dir = os.path.join(root, "conformance", "cases", "unit")

ESC = {"n": 10, "t": 9, "r": 13, "\\": 92, '"': 34, "'": 39, "a": 7, "b": 8, "f": 12, "v": 11, "?": 63}

def decode(lit_body):
    out = bytearray(); i = 0
    while i < len(lit_body):
        c = lit_body[i]
        if c != "\\":
            out += c.encode("utf-8"); i += 1; continue
        i += 1; e = lit_body[i]
        if e in ESC: out.append(ESC[e]); i += 1
        elif e == "x":
            j = i + 1
            while j < len(lit_body) and lit_body[j] in "0123456789abcdefABCDEF": j += 1
            out.append(int(lit_body[i + 1:j], 16) & 255); i = j
        elif e in "01234567":
            j = i
            while j < len(lit_body) and j < i + 3 and lit_body[j] in "01234567": j += 1
            out.append(int(lit_body[i:j], 8) & 255); i = j
        else: return None  # \u and friends: not used by the tests; skip rather than guess
    return bytes(out)

LIT = r'"(?:[^"\\\n]|\\.)*"'
SEQ = re.compile(r'(?:' + LIT + r'\s*)+')

def literal_at(text, pos):
    m = SEQ.match(text, pos)
    if not m: return None, pos
    parts = re.findall(LIT, m.group(0))
    data = bytearray()
    for p in parts:
        d = decode(p[1:-1])
        if d is None: return None, m.end()
        data += d
    return bytes(data), m.end()

DOC_FUNCS = ["anvil_load_buffer", "LOAD_LIT", "ATTR_LOAD", "setup_registered_doc", "make_doc", "aml_number_error"]
FRAG_FUNCS = ["anvil_parse_value_fragment", "expect_fragment_error", "setup_fragment_doc"]
NUM_FUNCS = ["expect_int", "expect_double", "expect_overflow"]
call = re.compile(r'\b(' + "|".join(DOC_FUNCS + FRAG_FUNCS + NUM_FUNCS) + r')\s*\(\s*(?=")')
shebang = re.compile(r'(?<![A-Za-z0-9_])(?=")')

def wrap(num): return b"#!aml\n\nx := " + num + b";\n"

found = {}  # content -> (ext, name)
def add(data, ext, stem, line):
    key = (ext, data)
    if key not in found:
        found[key] = "%s_L%d" % (stem, line)

test_dirs = ["unit", "functional"]
for d in test_dirs:
    base = os.path.join(root, "test", d)
    for fn in sorted(os.listdir(base)):
        if not fn.endswith(".c"): continue
        text = open(os.path.join(base, fn), encoding="utf-8", errors="replace").read()
        stem = re.sub(r"^test_", "", fn[:-2])
        line_of = lambda pos: text.count("\n", 0, pos) + 1
        for m in call.finditer(text):
            data, _ = literal_at(text, m.end())
            if data is None: continue
            f = m.group(1)
            if f in FRAG_FUNCS: add(data, "frag", stem, line_of(m.end()))
            elif f in NUM_FUNCS: add(wrap(data), "anvl", stem, line_of(m.end()))
            else: add(data, "anvl", stem, line_of(m.end()))
        for m in re.finditer(r'"#!', text):
            data, _ = literal_at(text, m.start())
            if data is None: continue
            add(data, "anvl", stem, line_of(m.start()))

edge = open(os.path.join(root, "test", "infra", "numeric_fastpath", "edge.c")).read()
table = re.search(r'L\[\]=\{(.*?)\};', edge, re.S).group(1)
for i, m in enumerate(re.finditer(LIT, table)):
    add(wrap(decode(m.group(0)[1:-1])), "anvl", "numeric_edge", i + 1)

if os.path.isdir(out_dir) and os.listdir(out_dir):
    if "--force" not in sys.argv: sys.exit("%s exists; pass --force to wipe and regenerate (dumps are removed too)" % out_dir)
    for fn in os.listdir(out_dir): os.remove(os.path.join(out_dir, fn))
os.makedirs(out_dir, exist_ok=True)
names = {}
for (ext, data), name in sorted(found.items(), key=lambda kv: (kv[1], kv[0][0])):
    name = "%s.%s" % (name, ext)
    if name in names: name = "%s_%s.%s" % (name.rsplit(".", 1)[0], hashlib.sha1(data).hexdigest()[:6], ext)
    names[name] = 1
    open(os.path.join(out_dir, name), "wb").write(data)
print("extracted %d inputs into %s" % (len(names), out_dir))
