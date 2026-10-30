#!/usr/bin/env python3
"""Generates conformance/cases/writer/ (FR-2610-anvl-conformance-003): writer scripts and the run each must print.

Each case is a writer script (conformance/FORMAT.md, "anvil-writer-script 1") plus the expected run
("anvil-writer-run 1"). The expectation is computed HERE, from a small model of the canonical layout and of the
writer's documented rules (include/anvil_writer.h), not by running native: the committed .dump is then compared
with native's real output by `make check-conformance`, so a disagreement is a finding in one or the other.

Deterministic. Usage: gen_writer.py <repo-root> [--force]   (refuses to overwrite cases/writer without --force)
"""
import os, re, shutil, struct, sys

root = sys.argv[1]
out_dir = os.path.join(root, "conformance", "cases", "writer")
if os.path.exists(out_dir):
    if "--force" not in sys.argv:
        sys.exit("cases/writer exists; pass --force to wipe and regenerate")
    shutil.rmtree(out_dir)

# ---- the error table: names in enum order, messages from src/writer/writer.c --------------------------------
ERRS = ["OK", "INVALID_ARGUMENT", "MEMORY", "STATE", "DIALECT", "INVALID_IDENTIFIER", "RESERVED_WORD",
        "INVALID_NUMERIC", "INVALID_BARE", "INVALID_BLOB", "INVALID_ATTRIBUTE_VALUE", "INVALID_INCLUDE_PATH",
        "EMPTY_COLLECTION", "INHERITANCE_REQUIRES_OBJECT", "UNFINISHED", "DUPLICATE_NAME", "DEPTH_EXCEEDED",
        "DUPLICATE_FIELD", "DUPLICATE_ATTRIBUTE_KEY"]
def ename(i): return "ANVIL_WRITER_OK" if i == 0 else "ANVIL_WRITER_ERR_" + ERRS[i]
src = open(os.path.join(root, "src", "writer", "writer.c")).read()
MSG = {}
for m in re.finditer(r'case (ANVIL_WRITER_\w+):\s*return "((?:[^"\\]|\\.)*)";', src):
    MSG[m.group(1)] = m.group(2)
assert len(MSG) == len(ERRS), MSG
def msg(code): return MSG[ename(code)] if 0 <= code < len(ERRS) else "unknown error"
def E(name): return ERRS.index(name)

# ---- quoting -------------------------------------------------------------------------------------------------
def dq(b):  # the dump's quoted string
    if isinstance(b, str): b = b.encode("utf-8")
    o = ['"']
    for c in b:
        if c == 0x5c: o.append("\\\\")
        elif c == 0x22: o.append('\\"')
        elif c == 10: o.append("\\n")
        elif c == 13: o.append("\\r")
        elif c == 9: o.append("\\t")
        elif c < 0x20 or c >= 0x7f: o.append("\\x%02x" % c)
        else: o.append(chr(c))
    o.append('"')
    return "".join(o)
def q(b): return dq(b)  # script arguments use the same escapes
def A(x): return "NULL" if x is None else q(x)

# ---- the canonical layout model -------------------------------------------------------------------------------
HDR = {"AML": b"#!aml\n\n", "AMP": b"#!amp\n\n"}

class Sc:   # a scalar: script steps and the text it writes
    def __init__(s, steps, text): s.steps = steps if isinstance(steps, list) else [steps]; s.text = text if isinstance(text, bytes) else text.encode()
class Arr:
    def __init__(s, *items): s.items = list(items)
class Tup:
    def __init__(s, *items): s.items = list(items)
class Obj:
    def __init__(s, *fields): s.fields = list(fields)   # Stmt objects
class Stmt:
    def __init__(s, name, value, base=None, attrs=()): s.name, s.value, s.base, s.attrs = name, value, base, list(attrs)
    # attrs: (key, None) flag | (key, "raw") | (key, ("q", bytes))

def Null(): return Sc("null", "null")
def Bool(v): return Sc("bool " + ("true" if v else "false"), "true" if v else "false")
def Int(n): return Sc("int %d" % n, "%d" % n)
def Num(t): return Sc("numeric " + q(t), t)
def Str(b):
    if isinstance(b, str): b = b.encode("utf-8")
    t = b'"'
    for c in b:
        t += {0x22: b'\\"', 0x5c: b'\\\\', 10: b'\\n', 9: b'\\t', 13: b'\\r'}.get(c, bytes([c]))
    return Sc("string " + q(b), t + b'"')
def Bare(t): return Sc("bare " + q(t), t)
def Blob(tag, data):
    if isinstance(data, str): data = data.encode("utf-8")
    steps = "blob %s %s" % (A(tag), q(data))
    return Sc(steps, (b"@" + tag.encode() if tag else b"") + b"`" + data + b"`")
def Ref(n): return Sc("varref " + q(n), "$" + n)
def fmt_double(x):
    for p in (15, 16, 17):
        t = "%.*g" % (p, x)
        if float(t) == x: break
    return t
def Dbl(x, word=None):
    bits = struct.unpack(">Q", struct.pack(">d", x))[0]
    return Sc("double " + (word or "0x%016x" % bits), fmt_double(x))

def steps_of(v):
    if isinstance(v, Sc): return list(v.steps)
    if isinstance(v, (Arr, Tup)):
        n = "array" if isinstance(v, Arr) else "tuple"
        r = ["begin_" + n]
        for i in v.items: r += steps_of(i)
        return r + ["end_" + n]
    if isinstance(v, Obj):
        r = ["begin_object"]
        for f in v.fields: r += stmt_steps(f)
        return r + ["end_object"]
    raise TypeError(v)
def stmt_steps(st):
    r = ["statement %s %s" % (q(st.name), A(st.base))]
    for k, val in st.attrs:
        if val is None: r.append("attribute %s NULL" % q(k))
        elif isinstance(val, tuple): r.append("attribute_string %s %s" % (q(k), q(val[1])))
        else: r.append("attribute %s %s" % (q(k), q(val)))
    return r + steps_of(st.value)

def ind(n): return b" " * (3 * n)
def render_val(v, level):
    if isinstance(v, Sc): return v.text
    if isinstance(v, (Arr, Tup)):
        o, ml = (b"[" if isinstance(v, Arr) else b"("), False
        for i, it in enumerate(v.items):
            if i: o += b","
            isobj = isinstance(it, Obj)
            if ml or isobj:
                ml = True
                o += b"\n" + ind(level + 1)
            elif i: o += b" "
            o += render_val(it, level + 1)
        if ml: o += b"\n" + ind(level)
        return o + (b"]" if isinstance(v, Arr) else b")")
    if isinstance(v, Obj):
        o = b"{\n"
        for f in v.fields: o += render_stmt(f, level + 1)
        return o + ind(level) + b"}"
def render_attrs(attrs):
    if not attrs: return b""
    parts = []
    for k, val in attrs:
        if val is None: parts.append(k.encode())
        elif isinstance(val, tuple): parts.append(k.encode() + b'="' + val[1] + b'"')
        else: parts.append(k.encode() + b"=" + val.encode())
    return b" @[" + b", ".join(parts) + b"]"
def render_stmt(st, level):
    o = ind(level) + st.name.encode()
    if st.base: o += b" : " + st.base.encode()
    return o + render_attrs(st.attrs) + b" := " + render_val(st.value, level) + b";\n"

# ---- cases ---------------------------------------------------------------------------------------------------
cases = []   # (area, name, script_text, dump_text, output_or_None, includes)
def run_text(records, nsteps, status, output):
    t = "anvil-writer-run 1\n" + "".join(r + "\n" for r in records) + "steps %d\n" % nsteps
    t += "status ok\n" if status == 0 else "status error code=%d name=%s\n" % (status, ename(status))
    t += "output none\n" if output is None else "output bytes=%d text=%s\n" % (len(output), dq(output))
    return t

def build(area, name, d, steps, desc=None, head=()):
    """Returns (script_text, line_of_step) with line_of_step[0] the dialect line; `head` goes before it."""
    lines = ["anvil-writer-script 1"]
    if desc: lines.append("# " + desc)
    lines += list(head)
    lines.append("dialect " + str(d))
    nums = [len(lines)]
    for s in steps:
        if s.startswith("#"): lines.append(s); nums.append(None); continue
        lines.append(s); nums.append(len(lines))
    return "\n".join(lines) + "\n", nums

def ok(area, name, d, body, out_body, desc=None, fin=True):
    """body: list of steps or a list of Stmt/values rendered by the caller. out_body: bytes after the header."""
    steps = list(body) + (["finish"] if fin else [])
    script, nums = build(area, name, d, steps, desc)
    out = (HDR[d] + out_body) if fin else None
    cases.append((area, name, script, run_text([], len(steps) + 1, 0, out), out))

def doc(area, name, d, stmts, header=b"", pre=(), desc=None):
    """A document of statements (rendered by the model)."""
    steps = list(pre)
    for s in stmts: steps += stmt_steps(s)
    body = header + (b"\n" if header and stmts else b"") + b"".join(render_stmt(s, 0) for s in stmts)
    ok(area, name, d, steps, body, desc)

def err(area, name, d, steps, code, desc=None):
    """The last step fails with `code`; the run stops there."""
    script, nums = build(area, name, d, steps, desc)
    i = len(steps) - 1
    line = nums[i + 1]
    call = steps[i]
    rec = "failed line=%d code=%d name=%s call=%s message=%s" % (line, code, ename(code), dq(call), dq(msg(code)))
    cases.append((area, name, script, run_text([rec], i + 2, code, None), None))

def raw(area, name, d, steps, make, desc=None, head=()):
    """make(nums) -> (records, nsteps, status, output); full control."""
    script, nums = build(area, name, d, steps, desc, head)
    records, nsteps, status, output = make(nums)
    cases.append((area, name, script, run_text(records, nsteps, status, output), output))

def failed(nums, i, steps, code):  # record for step i (0-based among steps)
    return "failed line=%d code=%d name=%s call=%s message=%s" % (nums[i + 1], code, ename(code), dq(steps[i]), dq(msg(code)))

S1 = lambda n, v, **k: Stmt(n, v, **k)

# ================================================================ lifecycle
L = "lifecycle"
ok(L, "empty_aml", "AML", [], b"")
ok(L, "empty_amp", "AMP", [], b"")
ok(L, "finish_twice", "AML", ["finish"], b"", desc="a second finish succeeds")
ok(L, "never_finished_aml", "AML", ["statement \"a\" NULL", "int 1"], b"", fin=False, desc="no finish: no output, status ok")
for d in ("AML", "AMP"):
    steps = ["data", "statement \"a\" NULL", "int 1", "data", "finish", "data"]
    raw(L, "data_around_finish_" + d.lower(), d, steps, lambda n, d=d, steps=steps: (
        ["data none", "data none", "data bytes=%d text=%s" % (len(HDR[d] + b"a := 1;\n"), dq(HDR[d] + b"a := 1;\n"))],
        7, 0, HDR[d] + b"a := 1;\n"))
raw(L, "get_error_ok", "AML", ["get_error", "finish", "get_error"], lambda n: (
    ["get_error code=0 name=ANVIL_WRITER_OK"] * 2, 4, 0, HDR["AML"]))
# error_message for every code and out-of-range values
steps = ["error_message %d" % c for c in list(range(0, 19)) + [19, 20, 100, -1, -2147483648, 2147483647]]
raw(L, "error_message_all", "AML", steps, lambda n: (
    ["error_message code=%d text=%s" % (c, dq(msg(c))) for c in list(range(0, 19)) + [19, 20, 100, -1, -2147483648, 2147483647]],
    len(steps) + 1, 0, HDR["AML"]), desc="every code, and values outside the enum")
# invalid dialect: the writer is NULL
for dv in (2, 7, -1, 255):
    steps = ["get_error", "statement \"a\" NULL"]
    script, nums = build(L, "x", dv, steps)
    cases.append((L, "dialect_invalid_%s" % (str(dv).replace("-", "neg")), script, run_text(
        ["failed line=%d code=1 name=%s call=%s message=%s" % (nums[0], ename(1), dq("dialect %d" % dv), dq(msg(1)))], 1, 1, None), None))
steps = ["get_error", "statement \"a\" NULL", "int 1", "finish", "data"]
def mk_dic(n):
    f = lambda line, call: "failed line=%d code=1 name=%s call=%s message=%s" % (line, ename(1), dq(call), dq(msg(1)))
    return ([f(n[0], "dialect 7"), "get_error code=1 name=ANVIL_WRITER_ERR_INVALID_ARGUMENT", f(n[2], steps[1]), f(n[3], steps[2]),
             f(n[4], steps[3]), "data none"], 7, 1, None)
raw(L, "dialect_invalid_continue", 7, steps, mk_dic, head=["option on-error continue"],
    desc="a NULL writer: every call fails with INVALID_ARGUMENT")
# sticky error and continue
steps = ["option on-error continue", "statement \"a\" NULL", "statement \"b\" NULL", "int 1", "bool true", "get_error", "finish", "data"]
def mk2(n):
    s = steps
    return ([failed(n, 2, s, E("STATE"))] + [failed(n, i, s, E("STATE")) for i in (3, 4)] + ["get_error code=3 name=ANVIL_WRITER_ERR_STATE", failed(n, 6, s, E("STATE")), "data none"], 9, E("STATE"), None)
raw(L, "sticky_error_continue", "AML", steps, mk2, desc="the first error is sticky; later calls fail with it")
steps = ["statement \"a\" NULL", "statement \"b\" NULL", "int 1"]
raw(L, "sticky_error_stop", "AML", steps, lambda n: ([failed(n, 1, steps, E("STATE"))], 3, E("STATE"), None), desc="stop at the first failed call; the rest is not run")
steps = ["statement \"a\" NULL", "int 1", "finish", "int 2", "data"]
raw(L, "call_after_finish", "AML", steps, lambda n: ([failed(n, 3, steps, E("STATE"))], 5, E("STATE"), None), desc="any call after finish is STATE (and the output is gone with the sticky error)")
steps = ["option on-error continue", "statement \"a\" NULL", "int 1", "finish", "int 2", "data", "get_error"]
raw(L, "call_after_finish_continue", "AML", steps, lambda n: ([failed(n, 3, steps, E("STATE")), "data none", "get_error code=3 name=ANVIL_WRITER_ERR_STATE"], 8, E("STATE"), None))
err(L, "unfinished_statement", "AML", ["statement \"a\" NULL", "finish"], E("UNFINISHED"))
err(L, "unfinished_array", "AML", ["statement \"a\" NULL", "begin_array", "int 1", "finish"], E("UNFINISHED"))
err(L, "unfinished_tuple", "AML", ["statement \"a\" NULL", "begin_tuple", "int 1", "int 2", "finish"], E("UNFINISHED"))
err(L, "unfinished_object", "AML", ["statement \"a\" NULL", "begin_object", "statement \"b\" NULL", "int 1", "finish"], E("UNFINISHED"))
err(L, "unfinished_inner_statement", "AML", ["statement \"a\" NULL", "begin_object", "statement \"b\" NULL", "finish"], E("UNFINISHED"))
err(L, "unfinished_amp", "AMP", ["statement \"a\" NULL", "begin_array", "finish"], E("UNFINISHED"))

# ================================================================ scalars
Z = "scalars"
for nm, v in (("null", Null()), ("true", Bool(True)), ("false", Bool(False))):
    for d in ("AML", "AMP"):
        doc(Z, "%s_%s" % (nm, d.lower()), d, [S1("v", v)])
for nm, n in (("zero", 0), ("one", 1), ("neg_one", -1), ("max", 2**63 - 1), ("min", -2**63), ("max_minus_1", 2**63 - 2),
              ("min_plus_1", -2**63 + 1), ("i32_max", 2**31 - 1), ("i32_min", -2**31), ("u32_max", 2**32 - 1),
              ("two_53", 2**53), ("two_53_plus_1", 2**53 + 1), ("neg_two_53_minus_1", -2**53 - 1), ("u64_midrange", 2**62),
              ("digits_19", 1234567890123456789), ("seven", 7), ("neg_seven", -7)):
    doc(Z, "int_" + nm, "AML", [S1("n", Int(n))])
doc(Z, "int_amp", "AMP", [S1("n", Int(-42))])
# numeric: every shape the grammar accepts is written verbatim
for i, t in enumerate(["0", "-0", "7", "-7", "007", "-0009", "10", "1.5", "-1.5", "0.0", "-0.0", "0.5", "00.5", "1e5", "1E5", "1e+5",
                       "1e-5", "1E+05", "-1.5e10", "1.5E-10", "123456789012345678901234567890", "1.7976931348623157e308",
                       "5e-324", "1e999", "-1e999", "1e-999", "9223372036854775807", "9223372036854775808", "-9223372036854775808",
                       "-9223372036854775809", "18446744073709551615", "0.1", "0.30000000000000004", "123.456e0", "1e0", "0e0"]):
    doc(Z, "numeric_%02d" % i, "AML", [S1("n", Num(t))], desc="numeric text %s" % t)
doc(Z, "numeric_long_digits", "AML", [S1("n", Num("1" * 400))], desc="a 400-digit integer text, written verbatim")
doc(Z, "numeric_long_fraction", "AML", [S1("n", Num("0." + "3" * 400))], desc="a 400-digit fraction, written verbatim")
for i, t in enumerate(["", " ", "abc", "+5", "5.", ".5", "-.5", "1.e3", "1e", "1e+", "1e-", "--1", "1-", "1 ", " 1", "0x10", "1_000", "1,5", "NaN", "Infinity",
                       "inf", "-", "1.5.5", "1e5.5", "1ee5", "\xe2\x82\xac", "1\n", "true", "null"]):
    err(Z, "numeric_bad_%02d" % i, "AML", ["statement \"n\" NULL", "numeric " + q(t.encode("latin-1"))], E("INVALID_NUMERIC"), desc="numeric text %s" % dq(t.encode("latin-1")))
err(Z, "numeric_null", "AML", ["statement \"n\" NULL", "numeric NULL"], E("INVALID_ARGUMENT"))
err(Z, "numeric_bad_before_state", "AML", ["numeric \"abc\""], E("INVALID_NUMERIC"), desc="the text is checked before the position")
err(Z, "numeric_state", "AML", ["numeric \"1\""], E("STATE"), desc="a value outside a statement")
# doubles: shortest text that reads back
dbl = [("zero", 0.0), ("neg_zero", -0.0), ("one", 1.0), ("neg_one", -1.0), ("half", 0.5), ("tenth", 0.1), ("third", 1 / 3), ("two_thirds", 2 / 3),
       ("pi", 3.141592653589793), ("e", 2.718281828459045), ("ten", 10.0), ("hundred", 100.0), ("million", 1e6), ("1e15", 1e15), ("1e16", 1e16),
       ("1e17", 1e17), ("1e21", 1e21), ("1e22", 1e22), ("1e100", 1e100), ("1e308", 1e308), ("1e-4", 1e-4), ("1e-5", 1e-5), ("1e-6", 1e-6),
       ("1e-7", 1e-7), ("1e-100", 1e-100), ("max", 1.7976931348623157e308), ("neg_max", -1.7976931348623157e308),
       ("min_normal", 2.2250738585072014e-308), ("max_subnormal", 2.225073858507201e-308), ("min_subnormal", 5e-324), ("neg_min_subnormal", -5e-324),
       ("eps", 2.220446049250313e-16), ("two_53", 9007199254740992.0), ("two_53_plus_2", 9007199254740994.0), ("0_3", 0.3), ("0_1_plus_0_2", 0.1 + 0.2),
       ("123456789", 123456789.0), ("123456789_123", 123456789.123), ("1_5", 1.5), ("neg_2_5e10", -2.5e10), ("17_digits", 0.12345678901234568),
       ("15_digits", 0.123456789012345), ("16_digits", 0.1234567890123457), ("big_int_valued", 123456789012345680000.0), ("4_35", 4.35), ("5e_1", 0.5e-1),
       ("float_32_max", 3.4028234663852886e38), ("1e23", 1e23), ("9_999e22", 9.999e22), ("2_to_63", 9223372036854775808.0), ("2_to_64", 18446744073709551616.0)]
for nm, x in dbl:
    doc(Z, "double_" + nm, "AML", [S1("d", Dbl(x))], desc="double %r" % x)
doc(Z, "double_decimal_word", "AML", [S1("d", Dbl(0.1, "0.1"))], desc="a decimal argument is converted like strtod")
doc(Z, "double_decimal_word_exp", "AML", [S1("d", Dbl(1.5e300, "1.5e300"))])
doc(Z, "double_amp", "AMP", [S1("d", Dbl(2.5))])
for nm, w in (("nan", "nan"), ("inf", "inf"), ("neg_inf", "-inf"), ("nan_bits", "0x7ff8000000000000"), ("neg_nan_bits", "0xfff8000000000000"),
              ("inf_bits", "0x7ff0000000000000"), ("decimal_overflow", "1e999"), ("decimal_neg_overflow", "-1e999")):
    err(Z, "double_nonfinite_" + nm, "AML", ["statement \"d\" NULL", "double " + w], E("INVALID_NUMERIC"))
err(Z, "double_state", "AML", ["double 1.5"], E("STATE"))
err(Z, "int_state", "AML", ["int 1"], E("STATE"))
err(Z, "null_state", "AML", ["null"], E("STATE"))
err(Z, "bool_state", "AMP", ["bool true"], E("STATE"))

# ================================================================ strings
T = "strings"
def sdoc(name, b, d="AML", desc=None): doc(T, name, d, [S1("s", Str(b))], desc=desc)
sdoc("empty", b"")
sdoc("empty_amp", b"", "AMP")
sdoc("plain", "hello world")
sdoc("space_only", " ")
sdoc("quote", '"')
sdoc("backslash", "\\")
sdoc("newline", "\n")
sdoc("tab", "\t")
sdoc("return", "\r")
sdoc("crlf", "\r\n")
sdoc("quote_in_text", 'say "hi"')
sdoc("backslash_n_text", "a\\nb", desc="backslash followed by n is two bytes, both escaped")
sdoc("double_backslash", "\\\\")
sdoc("backslash_quote", '\\"')
sdoc("all_escapes", '\\"\n\t\r')
sdoc("single_quote", "it's")
sdoc("backtick", "a`b")
sdoc("at_sign", "@tag")
sdoc("slashes", "// not a comment /* either */")
sdoc("hash_semicolon", "a; b := c; # d")
sdoc("braces", "{[(<)]}")
sdoc("dollar", "$name")
sdoc("looks_numeric", "123")
sdoc("looks_keyword", "true")
sdoc("utf8_2byte", "caf\u00e9")
sdoc("utf8_3byte", "\u20ac100")
sdoc("utf8_4byte", "\U0001f600")
sdoc("utf8_mixed", "na\u00efve \u65e5\u672c\u8a9e \U0001d11e")
sdoc("utf8_bom", "\ufeffx")
sdoc("utf8_nul_char", "\u0000".encode())
sdoc("invalid_utf8_ff", b"\xff\xfe")
sdoc("invalid_utf8_lone_continuation", b"a\x80b")
sdoc("invalid_utf8_truncated", b"\xe2\x82")
sdoc("invalid_utf8_overlong", b"\xc0\xaf")
sdoc("del_0x7f", b"\x7f")
sdoc("embedded_nul", b"a\x00b", desc="a NUL byte is data: strings are length-delimited")
sdoc("only_nul", b"\x00")
for c in range(1, 0x20):
    if c in (9, 10, 13): continue
    sdoc("control_%02x" % c, bytes([c]), desc="control byte %#04x is written raw (only quote, backslash, LF, TAB, CR are escaped)" % c)
sdoc("all_bytes_1_to_255", bytes(b for b in range(1, 256) if b != 0x22 or True), desc="every byte 1..255 in order")
sdoc("long_10000", "x" * 10000)
sdoc("long_escapes_4000", '"\\\n' * 1333, desc="a long run of bytes that all need escaping")
sdoc("long_utf8_6000", "\u20ac" * 2000)
sdoc("amp_escapes", 'a"b\\c\nd', "AMP")
# explicit length
raw(T, "len_prefix", "AML", ["statement \"s\" NULL", "string \"abcdef\" 3", "finish"], lambda n: ([], 4, 0, HDR["AML"] + b's := "abc";\n'))
raw(T, "len_zero", "AML", ["statement \"s\" NULL", "string \"abcdef\" 0", "finish"], lambda n: ([], 4, 0, HDR["AML"] + b's := "";\n'))
raw(T, "len_full", "AML", ["statement \"s\" NULL", "string \"abc\" 3", "finish"], lambda n: ([], 4, 0, HDR["AML"] + b's := "abc";\n'))
raw(T, "len_cuts_escape", "AML", ["statement \"s\" NULL", "string \"a\\nb\" 2", "finish"], lambda n: ([], 4, 0, HDR["AML"] + b's := "a\\n";\n'))
raw(T, "null_text_len0", "AML", ["statement \"s\" NULL", "string NULL", "finish"], lambda n: ([], 4, 0, HDR["AML"] + b's := "";\n'), desc="a NULL text with length 0 is the empty string")
err(T, "null_text_len3", "AML", ["statement \"s\" NULL", "string NULL 3"], E("INVALID_ARGUMENT"))
err(T, "string_state_top", "AML", ["string \"x\""], E("STATE"))
err(T, "string_state_after_value", "AML", ["statement \"s\" NULL", "string \"x\"", "string \"y\""], E("STATE"))
doc(T, "string_in_array", "AML", [S1("a", Arr(Str("a\"b"), Str(""), Str("\n")))])

# ================================================================ bare
B = "bare"
for i, t in enumerate(["abc", "ABC", "a", "_x", "_", "a1", "a_b", "a-b", "a.b", "a/b", "a:b", "a$b", "a-b.c/d:e$f", ".x", "..", "./x", "/usr/bin", "/", "./", "../x",
                       "1abc", "1_000", "0x10", "12abc", "1d", "1-2", "1.2.3", "1e5.5", "5.x", "1/", "9a.b", "trueish", "nullable", "included", "Include", "TRUE", "Null",
                       "NaN", "Infinity", "x" * 200, "a" + "-" * 50, "v1.2.3-rc1", "http:/x", "a:", "a-", "a.", "a/", "a$", "e5", "E5", "e", "_1e5", "-"[:0] + "x"]):
    if t == "": continue
    doc(B, "ok_%02d" % i, "AML", [S1("b", Bare(t))], desc="bare %s" % t[:40])
doc(B, "ok_amp", "AMP", [S1("b", Bare("ready"))])
doc(B, "in_array", "AML", [S1("a", Arr(Bare("x"), Bare("y-z"), Bare("/p")))])
for i, t in enumerate(["", " ", "a b", "-a", "-", "+a", "@a", "#a", "$a", "\"a\"", "a,b", "a;b", "a=b", "a(b)", "a[b]", "a{b}", "a\"b", "a`b", "a\\b", "a\tb", "a\nb", "\u00e9", "caf\u00e9", "a\u00e9",
                       "1", "-1", "1.5", "1e5", "1E5", "1e+5", "1e-5", "007", "1.", "1.e3", "1e", "1e+", "1e-", "1.5e", "12.5E+", "//", "//x", "/*", "/*x*/", "/* c */"]):
    code = E("INVALID_BARE")
    err(B, "bad_%02d" % i, "AML", ["statement \"b\" NULL", "bare " + q(t)], code, desc="bare %s" % dq(t)[:40])
for w in ("true", "false", "null", "include", "import", "vars"):
    err(B, "reserved_" + w, "AML", ["statement \"b\" NULL", "bare " + q(w)], E("RESERVED_WORD"))
err(B, "null_text", "AML", ["statement \"b\" NULL", "bare NULL"], E("INVALID_ARGUMENT"))
err(B, "state", "AML", ["bare \"x\""], E("STATE"))
err(B, "check_before_state", "AML", ["bare \"1\""], E("INVALID_BARE"))

# ================================================================ blobs
O = "blobs"
def bdoc(name, tag, data, d="AML", desc=None): doc(O, name, d, [S1("b", Blob(tag, data))], desc=desc)
bdoc("untagged", None, "hello")
bdoc("untagged_empty_tag", "", "hello", desc="an empty tag is untagged")
bdoc("untagged_amp", None, "x", "AMP")
bdoc("tagged", "md", "# Title")
bdoc("tagged_amp", "table", "[(1)]", "AMP")
bdoc("empty_data", "t", "")
bdoc("empty_untagged", None, "")
bdoc("newlines", "t", "line1\nline2\n")
bdoc("crlf", "t", "a\r\nb")
bdoc("tabs", None, "\ta\t")
bdoc("quotes_backslashes", "t", 'say "x" \\n \\\\ ', desc="a blob is verbatim: nothing is escaped")
bdoc("at_inside", "t", "@other x")
bdoc("semicolon_inside", "t", "a := 1; b := 2;")
bdoc("braces_inside", "t", "{[(")
bdoc("utf8", "t", "caf\u00e9 \u65e5\u672c \U0001f600")
bdoc("invalid_utf8", "t", b"\xff\xfe\x80")
bdoc("control_bytes", "t", bytes(range(1, 32)) + b"\x7f", desc="control bytes are verbatim")
bdoc("embedded_nul", "t", b"a\x00b", desc="blob content is length-delimited")
bdoc("all_bytes_except_backtick", "t", bytes(b for b in range(1, 256) if b != 0x60))
bdoc("long_10000", "t", "y" * 10000)
bdoc("tag_1_char", "a", "x")
bdoc("tag_underscore", "_", "x")
bdoc("tag_upper", "TAG", "x")
bdoc("tag_digits", "t1x2", "x")
bdoc("tag_31", "a" * 31, "x", desc="31 characters is the longest tag")
bdoc("tag_utid", "utid", "abc-123")
bdoc("tag_keyword_like", "true", "x", desc="a keyword is a fine blob tag")
for nm, tag in (("32", "a" * 32), ("100", "a" * 100), ("digit_start", "1a"), ("hyphen", "a-b"), ("dot", "a.b"), ("space", "a b"), ("at", "@a"),
                ("backtick", "a`b"), ("quote", "a\"b"), ("utf8", "caf\u00e9"), ("colon", "a:b"), ("leading_space", " a"), ("trailing_space", "a "), ("newline", "a\nb")):
    err(O, "bad_tag_" + nm, "AML", ["statement \"b\" NULL", "blob %s \"x\"" % q(tag)], E("INVALID_BLOB"), desc="tag %s" % dq(tag)[:40])
err(O, "backtick_in_data", "AML", ["statement \"b\" NULL", "blob \"t\" \"a`b\""], E("INVALID_BLOB"))
err(O, "backtick_only", "AML", ["statement \"b\" NULL", "blob NULL \"`\""], E("INVALID_BLOB"))
err(O, "backtick_at_start", "AML", ["statement \"b\" NULL", "blob \"t\" \"`x\""], E("INVALID_BLOB"))
err(O, "backtick_at_end", "AML", ["statement \"b\" NULL", "blob \"t\" \"x`\""], E("INVALID_BLOB"))
err(O, "backtick_after_nul", "AML", ["statement \"b\" NULL", "blob \"t\" \"\\x00`\""], E("INVALID_BLOB"), desc="the whole length is checked, past a NUL")
err(O, "data_null_len3", "AML", ["statement \"b\" NULL", "blob \"t\" NULL 3"], E("INVALID_ARGUMENT"))
raw(O, "data_null_len0", "AML", ["statement \"b\" NULL", "blob \"t\" NULL", "finish"], lambda n: ([], 4, 0, HDR["AML"] + b"b := @t``;\n"))
raw(O, "len_prefix", "AML", ["statement \"b\" NULL", "blob \"t\" \"ab`cd\" 2", "finish"], lambda n: ([], 4, 0, HDR["AML"] + b"b := @t`ab`;\n"), desc="a length shorter than the text stops before the backtick")
raw(O, "len_zero_with_backtick", "AML", ["statement \"b\" NULL", "blob NULL \"`\" 0", "finish"], lambda n: ([], 4, 0, HDR["AML"] + b"b := ``;\n"))
err(O, "state", "AML", ["blob \"t\" \"x\""], E("STATE"))
err(O, "bad_tag_before_state", "AML", ["blob \"1\" \"x\""], E("INVALID_BLOB"))
doc(O, "in_array", "AML", [S1("a", Arr(Blob("t", "x"), Blob(None, "y"), Blob("u", "")))])
doc(O, "in_tuple", "AMP", [S1("a", Tup(Blob("t", "x"), Int(1)))])

# ================================================================ collections
C = "collections"
doc(C, "array_one", "AML", [S1("a", Arr(Int(1)))])
doc(C, "array_ints", "AML", [S1("a", Arr(Int(1), Int(2), Int(3)))])
doc(C, "array_mixed", "AML", [S1("a", Arr(Int(1), Str("two"), Bare("three"), Null(), Bool(True), Dbl(4.5), Blob("t", "five")))])
doc(C, "array_amp", "AMP", [S1("a", Arr(Int(1), Str("x"), Bare("y"), Null(), Bool(False), Num("1.5"), Blob("t", "z")))])
doc(C, "tuple_two", "AML", [S1("t", Tup(Int(1), Int(2)))])
doc(C, "tuple_three", "AML", [S1("t", Tup(Bare("ok"), Int(1), Str("x")))])
doc(C, "tuple_amp", "AMP", [S1("t", Tup(Bare("ok"), Int(1), Str("x"), Null(), Bool(True), Dbl(0.25)))])
doc(C, "array_of_arrays", "AML", [S1("a", Arr(Arr(Int(1), Int(2)), Arr(Int(3)), Arr(Str("x"))))])
doc(C, "array_of_tuples", "AML", [S1("a", Arr(Tup(Int(1), Int(2)), Tup(Int(3), Int(4))))])
doc(C, "tuple_of_arrays", "AML", [S1("t", Tup(Arr(Int(1)), Arr(Int(2), Int(3))))])
doc(C, "tuple_of_tuples", "AML", [S1("t", Tup(Tup(Int(1), Int(2)), Tup(Int(3), Int(4))))])
doc(C, "object_one", "AML", [S1("o", Obj(S1("a", Int(1))))])
doc(C, "object_fields", "AML", [S1("o", Obj(S1("a", Int(1)), S1("b", Str("x")), S1("c", Arr(Int(1), Int(2))), S1("d", Tup(Int(1), Int(2))), S1("e", Null())))])
doc(C, "object_nested", "AML", [S1("o", Obj(S1("a", Obj(S1("b", Obj(S1("c", Int(1))))))))])
doc(C, "object_nested_siblings", "AML", [S1("o", Obj(S1("a", Obj(S1("x", Int(1)), S1("y", Int(2)))), S1("b", Obj(S1("x", Int(3))))))])
doc(C, "array_of_objects", "AML", [S1("a", Arr(Obj(S1("x", Int(1))), Obj(S1("x", Int(2)), S1("y", Int(3)))))])
doc(C, "array_object_after_scalars", "AML", [S1("a", Arr(Int(1), Int(2), Obj(S1("x", Int(1))), Int(3), Str("y")))], desc="once an object element is written, later elements go on their own lines")
doc(C, "array_object_last", "AML", [S1("a", Arr(Str("a"), Obj(S1("x", Int(1)))))])
doc(C, "tuple_with_object", "AML", [S1("t", Tup(Int(1), Obj(S1("x", Int(1)))))])
doc(C, "array_in_object_in_array", "AML", [S1("a", Arr(Obj(S1("l", Arr(Obj(S1("z", Int(1))), Int(2)))), Int(3)))])
doc(C, "object_in_tuple_in_array", "AML", [S1("a", Arr(Tup(Obj(S1("q", Null())), Int(1))))])
doc(C, "two_statements", "AML", [S1("a", Arr(Int(1))), S1("b", Obj(S1("c", Int(2)))), S1("d", Tup(Int(1), Int(2)))])
for depth in (2, 3, 10, 50, 100, 200):
    v = Int(1)
    for _ in range(depth): v = Arr(v)
    doc(C, "deep_arrays_%d" % depth, "AML", [S1("a", v)])
    v = Int(1)
    for _ in range(depth): v = Tup(v, Int(0))
    doc(C, "deep_tuples_%d" % depth, "AML", [S1("a", v)])
for depth in (2, 3, 10, 50, 100):
    v = Int(1)
    for i in range(depth): v = Obj(S1("f", v))
    doc(C, "deep_objects_%d" % depth, "AML", [S1("a", v)])
v = Int(1)
for i in range(60):
    v = [Arr, Tup, Obj][i % 3](*( [v] if i % 3 == 0 else [v, Int(0)] if i % 3 == 1 else [S1("f", v)]))
doc(C, "deep_mixed_60", "AML", [S1("a", v)])
doc(C, "wide_array_1000", "AML", [S1("a", Arr(*[Int(i) for i in range(1000)]))])
doc(C, "wide_object_300", "AML", [S1("o", Obj(*[S1("f%d" % i, Int(i)) for i in range(300)]))])
doc(C, "many_statements_300", "AML", [S1("s%d" % i, Int(i)) for i in range(300)])
# collection errors
for kind in ("array", "object"):
    err(C, "empty_" + kind, "AML", ["statement \"a\" NULL", "begin_" + kind, "end_" + kind], E("EMPTY_COLLECTION"))
for n in (0, 1):
    err(C, "tuple_with_%d" % n, "AML", ["statement \"a\" NULL", "begin_tuple"] + ["int 1"] * n + ["end_tuple"], E("EMPTY_COLLECTION"))
err(C, "empty_array_amp", "AMP", ["statement \"a\" NULL", "begin_array", "end_array"], E("EMPTY_COLLECTION"))
err(C, "empty_nested_array", "AML", ["statement \"a\" NULL", "begin_array", "begin_array", "end_array"], E("EMPTY_COLLECTION"))
err(C, "end_without_begin", "AML", ["end_array"], E("STATE"))
err(C, "end_tuple_without_begin", "AML", ["end_tuple"], E("STATE"))
err(C, "end_object_without_begin", "AML", ["end_object"], E("STATE"))
err(C, "end_array_closes_tuple", "AML", ["statement \"a\" NULL", "begin_tuple", "int 1", "int 2", "end_array"], E("STATE"))
err(C, "end_tuple_closes_array", "AML", ["statement \"a\" NULL", "begin_array", "int 1", "end_tuple"], E("STATE"))
err(C, "end_object_closes_array", "AML", ["statement \"a\" NULL", "begin_array", "int 1", "end_object"], E("STATE"))
err(C, "end_array_closes_object", "AML", ["statement \"a\" NULL", "begin_object", "statement \"b\" NULL", "int 1", "end_array"], E("STATE"))
err(C, "end_object_with_open_statement", "AML", ["statement \"a\" NULL", "begin_object", "statement \"b\" NULL", "end_object"], E("STATE"))
err(C, "begin_array_outside_statement", "AML", ["begin_array"], E("STATE"))
err(C, "begin_object_outside_statement", "AML", ["begin_object"], E("STATE"))
err(C, "begin_tuple_outside_statement", "AML", ["begin_tuple"], E("STATE"))
err(C, "scalar_in_object", "AML", ["statement \"a\" NULL", "begin_object", "int 1"], E("STATE"), desc="an object holds statements, not values")
err(C, "statement_in_array", "AML", ["statement \"a\" NULL", "begin_array", "statement \"b\" NULL"], E("STATE"))
err(C, "statement_in_tuple", "AML", ["statement \"a\" NULL", "begin_tuple", "statement \"b\" NULL"], E("STATE"))
err(C, "object_in_object_no_statement", "AML", ["statement \"a\" NULL", "begin_object", "begin_object"], E("STATE"))
err(C, "second_value_for_statement", "AML", ["statement \"a\" NULL", "begin_array", "int 1", "end_array", "int 2"], E("STATE"))
err(C, "end_after_finish", "AML", ["finish", "end_array"], E("STATE"))

# ================================================================ attributes
R = "attributes"
doc(R, "module_flag", "AML", [S1("a", Int(1))], header=b"@[doc]\n", pre=['attribute "doc" NULL'])
doc(R, "module_value", "AML", [S1("a", Int(1))], header=b"@[version=2]\n", pre=['attribute "version" "2"'])
doc(R, "module_several", "AML", [S1("a", Int(1))], header=b"@[a=1]\n@[b=x]\n@[c]\n", pre=['attribute "a" "1"', 'attribute "b" "x"', 'attribute "c" NULL'])
doc(R, "module_only", "AML", [], header=b"@[a=1]\n", pre=['attribute "a" "1"'], desc="a header and no statement")
doc(R, "module_string", "AML", [S1("a", Int(1))], header=b'@[title="hello world"]\n', pre=['attribute_string "title" "hello world"'])
doc(R, "module_quoted_raw", "AML", [S1("a", Int(1))], header=b'@[t="a, b]"]\n', pre=['attribute "t" "\\"a, b]\\""'], desc="a comma or bracket inside double quotes is allowed")
doc(R, "module_value_types", "AML", [], header=b'@[a=1]\n@[b=1.5]\n@[c=true]\n@[d=null]\n@[e=word]\n@[f="s"]\n@[g=-3]\n', pre=[
    'attribute "a" "1"', 'attribute "b" "1.5"', 'attribute "c" "true"', 'attribute "d" "null"', 'attribute "e" "word"', 'attribute "f" "\\"s\\""', 'attribute "g" "-3"'])
doc(R, "stmt_flag", "AML", [S1("a", Int(1), attrs=[("k", None)])])
doc(R, "stmt_value", "AML", [S1("a", Int(1), attrs=[("env", "production")])])
doc(R, "stmt_several", "AML", [S1("a", Int(1), attrs=[("x", "1"), ("y", None), ("z", "w")])])
doc(R, "stmt_string", "AML", [S1("a", Int(1), attrs=[("note", ("q", b"hi there"))])])
doc(R, "stmt_string_empty", "AML", [S1("a", Int(1), attrs=[("note", ("q", b""))])])
doc(R, "stmt_string_utf8", "AML", [S1("a", Int(1), attrs=[("note", ("q", "caf\u00e9 \u20ac".encode()))])])
doc(R, "stmt_string_special", "AML", [S1("a", Int(1), attrs=[("note", ("q", b"a, b] c = d \\ e"))])], desc="inside a quoted attribute only the quote and line breaks are refused")
doc(R, "stmt_on_object", "AML", [S1("o", Obj(S1("x", Int(1))), attrs=[("k", "v")])])
doc(R, "stmt_on_array", "AML", [S1("o", Arr(Int(1)), attrs=[("k", "v")])])
doc(R, "stmt_on_tuple", "AML", [S1("o", Tup(Int(1), Int(2)), attrs=[("k", "v")])])
doc(R, "stmt_on_blob", "AML", [S1("o", Blob("t", "x"), attrs=[("k", "v")])])
doc(R, "stmt_on_inner", "AML", [S1("o", Obj(S1("x", Int(1), attrs=[("k", "v")]), S1("y", Int(2), attrs=[("a", None), ("b", "1")])))])
doc(R, "stmt_with_base", "AML", [S1("base", Obj(S1("x", Int(1)))), S1("child", Obj(S1("y", Int(2))), base="base", attrs=[("k", "v")])])
doc(R, "module_and_stmt", "AML", [S1("a", Int(1), attrs=[("same", "1")])], header=b"@[same=1]\n", pre=['attribute "same" "1"'], desc="the same key in the header and on a statement is fine")
doc(R, "same_key_two_stmts", "AML", [S1("a", Int(1), attrs=[("k", "1")]), S1("b", Int(2), attrs=[("k", "2")])])
doc(R, "include_and_attrs", "AML", [S1("a", Int(1))], header=b'@[x=1]\ninclude "inc.anvl";\n', pre=['attribute "x" "1"', 'include "inc.anvl"'])
err(R, "dup_module_key", "AML", ['attribute "k" "1"', 'attribute "k" "2"'], E("DUPLICATE_ATTRIBUTE_KEY"))
err(R, "dup_module_key_flag", "AML", ['attribute "k" NULL', 'attribute "k" NULL'], E("DUPLICATE_ATTRIBUTE_KEY"))
err(R, "dup_module_key_string", "AML", ['attribute "k" "1"', 'attribute_string "k" "x"'], E("DUPLICATE_ATTRIBUTE_KEY"))
err(R, "dup_stmt_key", "AML", ['statement "a" NULL', 'attribute "k" "1"', 'attribute "k" "2"'], E("DUPLICATE_ATTRIBUTE_KEY"))
err(R, "dup_stmt_key_string", "AML", ['statement "a" NULL', 'attribute_string "k" "1"', 'attribute "k" NULL'], E("DUPLICATE_ATTRIBUTE_KEY"))
for i, k in enumerate(["", "1a", "a-b", "a.b", "a b", "a=b", "a]", "@a", "caf\u00e9", "a\n", "$a", " a", "-"]):
    err(R, "bad_key_%02d" % i, "AML", ["attribute %s NULL" % q(k)], E("INVALID_IDENTIFIER"), desc="key %s" % dq(k))
for w in ("true", "false", "null", "include", "import", "vars"):
    err(R, "reserved_key_" + w, "AML", ["attribute %s NULL" % q(w)], E("RESERVED_WORD"))
err(R, "key_null", "AML", ["attribute NULL NULL"], E("INVALID_ARGUMENT"))
err(R, "string_key_null", "AML", ["attribute_string NULL \"x\""], E("INVALID_ARGUMENT"))
err(R, "string_key_bad", "AML", ["attribute_string \"1a\" \"x\""], E("INVALID_IDENTIFIER"))
for i, v in enumerate(["", "a,b", "a]b", "a,", "]", ",", "$x", "[1]", "(1, 2)", "{x}", "`x`", "@t", " a", "a ", "\ta", "a\t", "a\nb", "a\rb", "\"open", "a\"b", "\"a\"\"", "x\"y\"z\"w", "a\"b,c"]):
    err(R, "bad_value_%02d" % i, "AML", ["attribute \"k\" %s" % q(v)], E("INVALID_ATTRIBUTE_VALUE"), desc="value %s" % dq(v))
for i, v in enumerate(["a", "1", "\"a b\"", "\"a,b\"", "\"a]b\"", "a=b", "a\"b c\"", "\"\"", "x y", "a;b", "a:b", "a.b-c", "\u00e9", "a`b", "a$b", "-1", "a@b", "a#b", "a(b", "a{b", "a[b"]):
    doc(R, "good_value_%02d" % i, "AML", [], header=b"@[k=" + v.encode() + b"]\n", pre=["attribute \"k\" %s" % q(v)], desc="value %s" % dq(v))
for i, v in enumerate(["a\"b", "a\nb", "a\rb", "\"", "\n"]):
    err(R, "bad_string_value_%02d" % i, "AML", ["attribute_string \"k\" %s" % q(v)], E("INVALID_ATTRIBUTE_VALUE"), desc="text %s" % dq(v))
err(R, "string_null_len3", "AML", ["attribute_string \"k\" NULL 3"], E("INVALID_ARGUMENT"))
raw(R, "string_null_len0", "AML", ['attribute_string "k" NULL', "finish"], lambda n: ([], 3, 0, HDR["AML"] + b'@[k=""]\n'))
raw(R, "string_len_prefix", "AML", ['attribute_string "k" "abc\\"d" 3', "finish"], lambda n: ([], 3, 0, HDR["AML"] + b'@[k="abc"]\n'), desc="a prefix before the quote is fine")
err(R, "state_after_value", "AML", ['statement "a" NULL', "int 1", 'attribute "k" NULL'], E("STATE"))
err(R, "state_after_statements", "AML", ['statement "a" NULL', "int 1", "finish", 'attribute "k" NULL'], E("STATE"))
err(R, "state_in_array", "AML", ['statement "a" NULL', "begin_array", 'attribute "k" NULL'], E("STATE"))
err(R, "state_in_object_no_stmt", "AML", ['statement "a" NULL', "begin_object", 'attribute "k" NULL'], E("STATE"))
err(R, "module_attr_after_statement", "AML", ['statement "a" NULL', "int 1", 'attribute_string "k" "x"'], E("STATE"))
err(R, "amp_module", "AMP", ['attribute "k" NULL'], E("DIALECT"))
err(R, "amp_statement", "AMP", ['statement "a" NULL', 'attribute "k" NULL'], E("DIALECT"))
err(R, "amp_string", "AMP", ['attribute_string "k" "x"'], E("DIALECT"))
err(R, "amp_before_key_check", "AMP", ['attribute "1a" "x,y"'], E("DIALECT"), desc="the dialect is checked before the arguments")

# ================================================================ statements
M = "statements"
doc(M, "simple", "AML", [S1("a", Int(1))])
doc(M, "names", "AML", [S1(n, Int(1)) for n in ("a", "A", "_", "_a", "a1", "a_b", "Z9_", "camelCase", "snake_case", "x" * 200)])
doc(M, "names_amp", "AMP", [S1(n, Int(1)) for n in ("utid", "seq", "payload")], desc="")
doc(M, "keyword_like_names", "AML", [S1(n, Int(1)) for n in ("trueish", "nullable", "included", "Include", "TRUE", "Null", "vars1", "importer")])
doc(M, "base_object", "AML", [S1("base", Obj(S1("x", Int(1)))), S1("child", Obj(S1("y", Int(2))), base="base")])
doc(M, "base_chain", "AML", [S1("a", Obj(S1("x", Int(1)))), S1("b", Obj(S1("y", Int(2))), base="a"), S1("c", Obj(S1("z", Int(3))), base="b")])
doc(M, "base_in_object", "AML", [S1("base", Obj(S1("x", Int(1)))), S1("o", Obj(S1("inner", Obj(S1("y", Int(2))), base="base")))])
doc(M, "same_name_in_two_objects", "AML", [S1("a", Obj(S1("x", Int(1)))), S1("b", Obj(S1("x", Int(2))))])
doc(M, "same_name_nested_levels", "AML", [S1("x", Obj(S1("x", Obj(S1("x", Int(1))))))], desc="a field may reuse an outer name")
for i, n in enumerate(["", "1a", "a-b", "a.b", "a b", "a:b", "a$", "@a", "$a", "caf\u00e9", "a\n", " a", "-", "a=b", "a;"]):
    err(M, "bad_name_%02d" % i, "AML", ["statement %s NULL" % q(n)], E("INVALID_IDENTIFIER"), desc="name %s" % dq(n))
for w in ("true", "false", "null", "include", "import", "vars"):
    err(M, "reserved_name_" + w, "AML", ["statement %s NULL" % q(w)], E("RESERVED_WORD"))
    err(M, "reserved_base_" + w, "AML", ['statement "a" %s' % q(w)], E("RESERVED_WORD"))
err(M, "name_null", "AML", ["statement NULL NULL"], E("INVALID_ARGUMENT"))
err(M, "bad_base", "AML", ['statement "a" "1b"'], E("INVALID_IDENTIFIER"))
err(M, "bad_base_dash", "AML", ['statement "a" "b-c"'], E("INVALID_IDENTIFIER"))
err(M, "empty_base", "AML", ['statement "a" ""'], E("INVALID_IDENTIFIER"))
err(M, "bad_name_and_bad_base", "AML", ['statement "1" "2"'], E("INVALID_IDENTIFIER"))
err(M, "dup_name", "AML", ['statement "a" NULL', "int 1", 'statement "a" NULL'], E("DUPLICATE_NAME"))
err(M, "dup_name_far", "AML", ['statement "a" NULL', "int 1", 'statement "b" NULL', "int 2", 'statement "c" NULL', "int 3", 'statement "a" NULL'], E("DUPLICATE_NAME"))
doc(M, "case_sensitive_names", "AML", [S1("a", Int(1)), S1("A", Int(2))])
err(M, "dup_field", "AML", ['statement "o" NULL', "begin_object", 'statement "x" NULL', "int 1", 'statement "x" NULL'], E("DUPLICATE_FIELD"))
err(M, "dup_field_nested", "AML", ['statement "o" NULL', "begin_object", 'statement "i" NULL', "begin_object", 'statement "x" NULL', "int 1", 'statement "x" NULL'], E("DUPLICATE_FIELD"))
err(M, "dup_field_in_array_object", "AML", ['statement "o" NULL', "begin_array", "begin_object", 'statement "x" NULL', "int 1", 'statement "x" NULL'], E("DUPLICATE_FIELD"))
err(M, "inheritance_scalar", "AML", ['statement "a" "b"', "int 1"], E("INHERITANCE_REQUIRES_OBJECT"))
err(M, "inheritance_array", "AML", ['statement "a" "b"', "begin_array"], E("INHERITANCE_REQUIRES_OBJECT"))
err(M, "inheritance_tuple", "AML", ['statement "a" "b"', "begin_tuple"], E("INHERITANCE_REQUIRES_OBJECT"))
err(M, "inheritance_blob", "AML", ['statement "a" "b"', 'blob "t" "x"'], E("INHERITANCE_REQUIRES_OBJECT"))
err(M, "inheritance_varref", "AML", ['statement "a" "b"', 'varref "c"'], E("INHERITANCE_REQUIRES_OBJECT"))
err(M, "inheritance_null", "AML", ['statement "a" "b"', "null"], E("INHERITANCE_REQUIRES_OBJECT"))
err(M, "statement_twice", "AML", ['statement "a" NULL', 'statement "b" NULL'], E("STATE"))
err(M, "value_without_statement", "AML", ["int 1"], E("STATE"))
err(M, "statement_after_finish", "AML", ["finish", 'statement "a" NULL'], E("STATE"))
err(M, "statement_in_open_object_statement", "AML", ['statement "o" NULL', "begin_object", 'statement "a" NULL', 'statement "b" NULL'], E("STATE"))
err(M, "state_before_name_check", "AML", ['statement "a" NULL', 'statement "1" NULL'], E("STATE"), desc="a statement while one is open is STATE, whatever the name")

# ================================================================ include
N = "include"
def idoc(name, paths, d="AML", desc=None, stmts=(S1("a", Int(1)),)):
    header = b"".join(b'include "' + p.encode() + b'";\n' for p in paths)
    cases_before = len(cases)
    doc(N, name, d, list(stmts), header=header, pre=['include %s' % q(p) for p in paths], desc=desc)
idoc("one", ["base.anvl"])
idoc("two", ["a.anvl", "b.anvl"])
idoc("subdir", ["sub/dir/x.anvl"])
idoc("dotdot", ["../up.anvl"])
idoc("dot_slash", ["./here.anvl"])
idoc("spaces", ["with space.anvl"])
idoc("utf8", ["caf\u00e9.anvl"])
idoc("quote_free_odd", ["a;b.anvl", "a'b.anvl", "a{b}.anvl", "a\\b.anvl"], desc="only a quote or a line break is refused")
idoc("no_statements", ["base.anvl"], stmts=())
idoc("same_twice", ["x.anvl", "x.anvl"], desc="the writer does not check for a repeated include")
idoc("long_path", ["d/" * 100 + "x.anvl"])
for i, p in enumerate(["", "a\"b", "a\nb", "a\rb", "\"", "\n", "x.anvl\"; evil := 1; //"]):
    err(N, "bad_path_%02d" % i, "AML", ["include %s" % q(p)], E("INVALID_INCLUDE_PATH"), desc="path %s" % dq(p))
err(N, "path_null", "AML", ["include NULL"], E("INVALID_ARGUMENT"))
err(N, "after_statement", "AML", ['statement "a" NULL', "int 1", 'include "x.anvl"'], E("STATE"))
err(N, "inside_statement", "AML", ['statement "a" NULL', 'include "x.anvl"'], E("STATE"))
err(N, "inside_object", "AML", ['statement "a" NULL', "begin_object", 'include "x.anvl"'], E("STATE"))
err(N, "after_finish", "AML", ["finish", 'include "x.anvl"'], E("STATE"))
err(N, "amp", "AMP", ['include "x.anvl"'], E("DIALECT"))
err(N, "amp_bad_path_still_dialect", "AMP", ['include ""'], E("DIALECT"))

# ================================================================ varref
F = "varref"
doc(F, "simple", "AML", [S1("a", Int(1)), S1("b", Ref("a"))])
doc(F, "in_array", "AML", [S1("a", Int(1)), S1("b", Arr(Ref("a"), Int(2), Ref("a")))])
doc(F, "in_tuple", "AML", [S1("a", Int(1)), S1("b", Tup(Ref("a"), Ref("a")))])
doc(F, "in_object", "AML", [S1("a", Int(1)), S1("o", Obj(S1("x", Ref("a"))))])
doc(F, "to_object", "AML", [S1("a", Obj(S1("x", Int(1)))), S1("b", Ref("a"))])
doc(F, "forward", "AML", [S1("b", Ref("a")), S1("a", Int(1))], desc="the reference may precede the definition")
doc(F, "underscore_name", "AML", [S1("_v", Int(1)), S1("b", Ref("_v"))])
for i, n in enumerate(["", "1a", "a-b", "a.b", "a b", "$a", "@a", "caf\u00e9", "a\n"]):
    err(F, "bad_name_%02d" % i, "AML", ['statement "b" NULL', "varref %s" % q(n)], E("INVALID_IDENTIFIER"), desc="name %s" % dq(n))
for w in ("true", "false", "null", "include", "import", "vars"):
    err(F, "reserved_" + w, "AML", ['statement "b" NULL', "varref %s" % q(w)], E("RESERVED_WORD"))
err(F, "null_name", "AML", ['statement "b" NULL', "varref NULL"], E("INVALID_ARGUMENT"))
err(F, "state", "AML", ['varref "a"'], E("STATE"))
err(F, "amp", "AMP", ['statement "b" NULL', 'varref "a"'], E("DIALECT"))
err(F, "amp_in_array", "AMP", ['statement "b" NULL', "begin_array", 'varref "a"'], E("DIALECT"))
err(F, "amp_bad_name_still_dialect", "AMP", ['varref "1"'], E("DIALECT"))
err(F, "bad_name_before_state", "AML", ['varref "1"'], E("INVALID_IDENTIFIER"))

# ================================================================ amp
P = "amp"
err(P, "object", "AMP", ['statement "a" NULL', "begin_object"], E("DIALECT"))
err(P, "base", "AMP", ['statement "a" "b"'], E("DIALECT"))
err(P, "base_bad_ident_still_dialect", "AMP", ['statement "a" "1"'], E("DIALECT"))
err(P, "array_in_array", "AMP", ['statement "a" NULL', "begin_array", "begin_array"], E("DIALECT"))
err(P, "tuple_in_array", "AMP", ['statement "a" NULL', "begin_array", "begin_tuple"], E("DIALECT"))
err(P, "array_in_tuple", "AMP", ['statement "a" NULL', "begin_tuple", "int 1", "begin_array"], E("DIALECT"))
err(P, "tuple_in_tuple", "AMP", ['statement "a" NULL', "begin_tuple", "begin_tuple"], E("DIALECT"))
err(P, "object_in_array", "AMP", ['statement "a" NULL', "begin_array", "begin_object"], E("DIALECT"))
doc(P, "scalars_in_array", "AMP", [S1("a", Arr(Int(1), Dbl(1.5), Str("s"), Bare("b"), Null(), Bool(True), Blob("t", "x"), Num("1e5")))])
doc(P, "statements", "AMP", [S1("a", Int(1)), S1("b", Str("x")), S1("c", Arr(Int(1), Int(2))), S1("d", Tup(Int(1), Int(2)))])
doc(P, "wide_array", "AMP", [S1("a", Arr(*[Int(i) for i in range(500)]))])
doc(P, "wide_tuple", "AMP", [S1("t", Tup(*[Str("f%d" % i) for i in range(200)]))])
doc(P, "blobs_everywhere", "AMP", [S1("x", Blob("table", "[(1), (2)]")), S1("y", Blob(None, "plain")), S1("z", Arr(Blob("a", "1"), Blob("b", "2")))])
err(P, "dup_name", "AMP", ['statement "a" NULL', "int 1", 'statement "a" NULL'], E("DUPLICATE_NAME"))
err(P, "empty_tuple", "AMP", ['statement "a" NULL', "begin_tuple", "end_tuple"], E("EMPTY_COLLECTION"))
err(P, "one_element_tuple", "AMP", ['statement "a" NULL', "begin_tuple", "int 1", "end_tuple"], E("EMPTY_COLLECTION"))

# ================================================================ envelope (King & Crucible)
K = "envelope"
def env(name, utid, seq, payload_tag, payload, d="AMP", desc=None):
    doc(K, name, d, [S1("utid", Blob("utid", utid)), S1("seq", Int(seq)), S1("payload", Blob(payload_tag, payload))], desc=desc)
env("request_basic", "0f8fad5b-d9cb-469f-a165-70867728950e", 1, "table", "[(1)]", desc="utid := @utid`...`; seq := N; payload := @table`[(...)]`;")
env("request_seq0", "u", 0, "table", "[(0)]")
env("request_seq_big", "abc", 9223372036854775807, "table", "[(1, 2, 3)]")
env("request_rows", "id-2", 42, "table", "[(1, \"a\"), (2, \"b\"), (3, null)]")
env("request_empty_payload", "id-3", 5, "table", "[]")
env("request_text_row", "id-4", 6, "table", '[("x", 1)]')
env("request_multiline", "id-5", 7, "table", "[\n   (1),\n   (2)\n]\n")
env("request_utf8", "id-6", 8, "table", "[(\"caf\u00e9\", \"\u65e5\u672c\")]")
env("request_other_tag", "id-7", 9, "json", '{"a": [1, 2]}')
env("request_untagged_payload", "id-8", 10, None, "raw bytes")
env("request_aml", "id-9", 11, "table", "[(1)]", d="AML")
env("request_long_payload", "id-10", 12, "table", "[" + ", ".join("(%d)" % i for i in range(1000)) + "]")
env("request_neg_seq", "id-11", -1, "table", "[(1)]")
doc(K, "request_attrs_free", "AMP", [S1("utid", Blob("utid", "a")), S1("seq", Int(3)), S1("payload", Blob("table", "[(1)]"))], desc="the envelope has no attributes (AMP)")
# responses: (status, fields...)
doc(K, "response_ok_int", "AMP", [S1("result", Tup(Bare("ok"), Int(42)))])
doc(K, "response_ok_fields", "AMP", [S1("result", Tup(Bare("ok"), Int(1), Str("two"), Bool(True), Null(), Dbl(2.5)))])
doc(K, "response_error", "AMP", [S1("result", Tup(Bare("error"), Int(404), Str("not found")))])
doc(K, "response_string_status", "AMP", [S1("result", Tup(Str("ok"), Int(1)))])
doc(K, "response_int_status", "AMP", [S1("result", Tup(Int(0), Str("fine")))])
doc(K, "response_with_blob", "AMP", [S1("result", Tup(Bare("ok"), Blob("table", "[(1)]")))])
doc(K, "response_escapes", "AMP", [S1("result", Tup(Bare("ok"), Str('say "hi"\nline2\ttab\\slash'), Str("")))])
doc(K, "response_many", "AMP", [S1("result", Tup(Bare("ok"), *[Int(i) for i in range(100)]))])
doc(K, "response_envelope", "AMP", [S1("utid", Blob("utid", "id-1")), S1("seq", Int(7)), S1("result", Tup(Bare("ok"), Int(1), Str("x")))])
doc(K, "response_list", "AMP", [S1("results", Arr(Str("a"), Str("b"), Str("c"))), S1("count", Int(3))])
doc(K, "response_int_edges", "AMP", [S1("result", Tup(Bare("ok"), Int(-2**63), Int(2**63 - 1), Int(0), Int(-1)))])
doc(K, "response_float_edges", "AMP", [S1("result", Tup(Bare("ok"), Dbl(0.1), Dbl(-0.0), Dbl(1e21), Dbl(5e-324), Dbl(123456789.125)))])
err(K, "response_single_field", "AMP", ['statement "result" NULL', "begin_tuple", "bare \"ok\"", "end_tuple"], E("EMPTY_COLLECTION"))
err(K, "response_nested_tuple", "AMP", ['statement "result" NULL', "begin_tuple", "bare \"ok\"", "begin_tuple"], E("DIALECT"))
err(K, "payload_backtick", "AMP", ['statement "payload" NULL', 'blob "table" "[(`)]"'], E("INVALID_BLOB"))
err(K, "bad_utid_tag", "AMP", ['statement "utid" NULL', 'blob "u-id" "x"'], E("INVALID_BLOB"))
err(K, "dup_seq", "AMP", ['statement "seq" NULL', "int 1", 'statement "seq" NULL'], E("DUPLICATE_NAME"))

# ---- write everything ----------------------------------------------------------------------------------------
seen = set()
counts = {}
for area, name, script, dump, out in cases:
    key = (area, name)
    assert key not in seen, key
    seen.add(key)
    base = os.path.join(out_dir, area, name + ".wscript")
    os.makedirs(os.path.dirname(base), exist_ok=True)
    with open(base, "wb") as f: f.write(script.encode("latin-1"))
    with open(base + ".dump", "wb") as f: f.write(dump.encode("latin-1"))
    counts[area] = counts.get(area, 0) + 1
for a in sorted(counts): print("%-12s %d" % (a, counts[a]))
print("total        %d" % sum(counts.values()))
