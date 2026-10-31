#!/usr/bin/env python3
"""Generates the SCRIPTS of conformance/cases/builder/ (FR-2610-anvl-conformance-004).

Each case is a builder script (conformance/FORMAT.md, "anvil-builder-script 1"). The generator writes scripts only.
The expected runs (.bscript.dump) belong to native: they are written by `make update-conformance`
(anvil-dump --builder-script) and checked by `make check-conformance`. There is deliberately no model of the builder
in here: the dump is the specification.

Deterministic. Usage:
  gen_builder.py <repo-root> [--force]   write the scripts (refuses if cases/builder exists, unless --force)
--force rewrites the scripts and removes scripts that are no longer generated; it never touches a .dump.
"""
import os, sys

args = [a for a in sys.argv[1:] if a != "--force"]
force = "--force" in sys.argv[1:]
root = args[0]
out_dir = os.path.join(root, "conformance", "cases", "builder")
if os.path.exists(out_dir) and not force:
    sys.exit("cases/builder exists; pass --force to rewrite the scripts")

def q(b):
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
def A(x): return "NULL" if x is None else q(x)

cases = []   # (area, name, text)
def case(area, name, steps, dialect="AML", cont=False, desc=None, b2=None):
    L = ["anvil-builder-script 1"]
    if desc: L.append("# " + desc)
    if cont: L.append("option on-error continue")
    L.append("dialect " + dialect)
    if b2: L.append("builder2 " + b2)
    L += steps
    cases.append((area, name, "\n".join(L) + "\n"))

# ---- step helpers (each returns a list of lines) -----------------------------------------------------------------
def top(name, value_steps, h="v"):
    """value_steps ends by binding h; adds it as a top-level statement."""
    return value_steps + ["add NULL %s %s" % (q(name), h)]
def let(h, call): return "let %s = %s" % (h, call)
def doc1(value_call, name="v"):
    return [let("v", value_call), "add NULL %s v" % q(name), "emit"]

Z = "AML"
# ============================================================================================ lifecycle
LF = "lifecycle"
case(LF, "empty_aml", ["emit"])
case(LF, "empty_amp", ["emit"], "AMP")
case(LF, "empty_emit_twice", ["emit", "emit"])
case(LF, "empty_write", ["write"])
for d in ("-1", "2", "3", "7", "255", "2147483647", "-2147483648"):
    case(LF, "dialect_invalid_%s" % d.replace("-", "m"), ["get_error", "emit"], d, cont=True, desc="the builder is the null builder")
for d in ("0", "1"):
    case(LF, "dialect_int_%s" % d, doc1('string "x"'), d, desc="the raw enum value")
case(LF, "dialect_invalid_all_calls", [
    "attribute \"a\" NULL", "include \"x\"", let("n", "null"), let("o", "object"), "append n n", let("m", 'add NULL "a" n'),
    'set_base m "b"', 'member_attribute m "k" "v"', 'member_attribute_string m "k" "v"', 'find NULL "a"', "write", "write AMP", "get_error"],
    "9", cont=True, desc="every call on the null builder fails with INVALID_ARGUMENT, find returns none")
case(LF, "get_error_fresh", ["get_error"])
case(LF, "get_error_after_ok", [let("v", "null"), "get_error"])
case(LF, "get_error_after_fail", [let("v", 'numeric "x"'), "get_error"], cont=True)
case(LF, "stop_on_first_failure", [let("v", 'numeric "x"'), let("w", "null"), "get_error"], desc="the default stops at the first failed call")
case(LF, "continue_after_failure", [let("v", 'numeric "x"'), let("w", "null"), "add NULL \"w\" w", "emit"], cont=True)
case(LF, "not_sticky", [let("a", "int 1"), 'add NULL "x" a', let("b", "int 2"), 'add NULL "x" b', "get_error", let("c", "int 3"), 'add NULL "y" c', "get_error", "emit"],
     cont=True, desc="a failed call changes nothing; the next call works")
case(LF, "failed_constructor_then_good", [let("a", 'bare "1"'), let("b", 'bare "ok"'), 'add NULL "a" a', 'add NULL "b" b', "get_error", "emit"], cont=True)
case(LF, "emit_twice_same", doc1("int 1") [:2] + ["emit", "emit"])
case(LF, "emit_after_change", [let("a", "int 1"), 'add NULL "a" a', "emit", let("b", "bool true"), 'add NULL "b" b', "emit", let("c", "null"), 'add NULL "c" c', "emit"],
     desc="re-emit after changing the builder")
case(LF, "emit_fail_then_fix", [let("a", "array"), 'add NULL "a" a', "emit", let("e", "int 1"), "append a e", "emit"], cont=True,
     desc="an empty array fails at emit; adding an element fixes it")
case(LF, "emit_failure_keeps_builder", [let("a", "tuple"), 'add NULL "t" a', let("x", "int 1"), "append a x", "emit", "get_error", "emit", let("y", "int 2"), "append a y", "emit"], cont=True,
     desc="a 1-tuple fails at emit, and the builder is unchanged")
case(LF, "write_after_emit", [let("a", "int 1"), 'add NULL "a" a', "emit", "write", "emit", "write"])
case(LF, "write_default", doc1("int 1")[:2] + ["write"])
case(LF, "write_amp_builder", doc1("int 1")[:2] + ["write", "write AML", "write AMP"], "AMP")
case(LF, "write_aml_into_amp", doc1("int 1")[:2] + ["write AMP"], desc="writer dialect differs from the builder's")
case(LF, "write_null_writer", doc1("int 1")[:2] + ["write NULL", "get_error"], cont=True)
case(LF, "write_aml_features_into_amp", [let("o", "object"), let("i", "int 1"), 'add o "k" i', 'add NULL "o" o', "write AMP", "get_error"], cont=True,
     desc="AML-only constructs into an AMP writer fail with DIALECT")
case(LF, "write_failure", [let("a", "array"), 'add NULL "a" a', "write", "get_error"], cont=True, desc="write fails like emit")
case(LF, "write_then_emit_failure", [let("a", "array"), 'add NULL "a" a', "write", "emit", "get_error"], cont=True)
case(LF, "emit_amp_and_write_aml", [let("a", "int 1"), 'add NULL "a" a', "emit", "write AML", "write AMP"], "AMP")
for c in list(range(-1, 21)):
    case(LF, "error_message_%s" % str(c).replace("-", "m"), ["error_message %d" % c])
case(LF, "error_message_extremes", ["error_message 2147483647", "error_message -2147483648"])
case(LF, "get_error_all_via_calls", [
    let("n", 'numeric "1x"'), let("b", 'bare ""'), let("s", "string NULL 3"), let("t", 'blob "a-b" "x"'), let("r", 'varref "1"'),
    'add NULL "1" NULL', 'attribute "" NULL', 'include ""', "get_error"], cont=True)

# ============================================================================================ values
V = "values"
case(V, "null", doc1("null"))
case(V, "bool_true", doc1("bool true"))
case(V, "bool_false", doc1("bool false"))
for i, t in enumerate(["0", "1", "-1", "007", "1.5", "-1.5", "1e5", "1E5", "1e+5", "1e-5", "1.5e10", "0.5", "123456789012345678901234567890", "-0", "00", "1_000"[:1],
                       "+1", ".5", "5.", "0x10", "1e", "1.e3", "abc", "", " 1", "1 ", "--1", "1.2.3", "NaN", "inf", "1,5", "١٢٣", "1\n"]):
    case(V, "numeric_%02d" % i, doc1("numeric %s" % q(t)), desc="numeric %s" % q(t)[:30])
case(V, "numeric_null", doc1("numeric NULL"))
for n, v in (("zero", 0), ("one", 1), ("minus_one", -1), ("max", 9223372036854775807), ("min", -9223372036854775808), ("big", 1234567890123), ("pow2_53", 9007199254740993), ("ten", 10), ("neg_ten", -10)):
    case(V, "int_" + n, doc1("int %d" % v))
case(V, "int_leading_zeros", doc1("int 007"), desc="the script integer 007 is 7")
D = [("zero", "0"), ("neg_zero", "-0"), ("neg_zero_bits", "0x8000000000000000"), ("one", "1"), ("half", "0.5"), ("neg_half", "-0.5"), ("third", "0.3333333333333333"), ("pi", "3.141592653589793"),
     ("tenth", "0.1"), ("sum_tenth", "0.30000000000000004"), ("e5", "1e5"), ("e15", "1e15"), ("e16", "1e16"), ("e17", "1e17"), ("e21", "1e21"), ("e22", "1e22"), ("em5", "1e-5"), ("em7", "1e-7"), ("em10", "1e-10"),
     ("big", "1e300"), ("tiny", "1e-300"), ("max", "0x7fefffffffffffff"), ("min_normal", "0x0010000000000000"), ("min_denormal", "0x0000000000000001"), ("neg_max", "0xffefffffffffffff"),
     ("int_valued", "12345678"), ("int_valued_big", "9007199254740992"), ("two53p1", "9007199254740993"), ("point_one_two_three", "123.456"), ("underflow", "1e-999"), ("plus_sign", "+1.5"), ("dot5", ".5"), ("five_dot", "5."),
     ("e_upper", "1E+5"), ("bits_one", "0x3ff0000000000000"), ("bits_third", "0x3fd5555555555555"), ("bits_1e23", "0x44b52d02c7e14af6"), ("subnormal_big", "0x000fffffffffffff"), ("neg_pi", "-3.141592653589793"),
     ("nan", "nan"), ("inf", "inf"), ("neg_inf", "-inf"), ("overflow", "1e999"), ("neg_overflow", "-1e999"), ("bits_nan", "0x7ff8000000000000"), ("bits_inf", "0x7ff0000000000000"), ("bits_neg_inf", "0xfff0000000000000")]
for n, t in D:
    case(V, "double_" + n, doc1("double " + t))
S = [("empty", ""), ("plain", "hello"), ("space", " a b "), ("quote", 'say "x"'), ("backslash", "a\\b"), ("newline", "a\nb"), ("tab", "a\tb"), ("cr", "a\rb"), ("crlf", "a\r\nb"), ("all_escapes", '"\\\n\t\r'),
     ("hash_semicolon", "a; b := c; # d"), ("braces", "{[(<)]}"), ("dollar", "$name"), ("looks_numeric", "123"), ("looks_keyword", "true"), ("utf8_2", "café"), ("utf8_3", "€100"), ("utf8_4", "\U0001f600"),
     ("utf8_mixed", "naïve 日本語 \U0001d11e"), ("bom", "﻿x"), ("nul_char", "\u0000"), ("embedded_nul", b"a\x00b"), ("only_nul", b"\x00"), ("invalid_ff", b"\xff\xfe"), ("invalid_continuation", b"a\x80b"),
     ("invalid_truncated", b"\xe2\x82"), ("overlong", b"\xc0\xaf"), ("del", b"\x7f"), ("long_10000", "x" * 10000), ("long_escapes", '"\\\n' * 1000), ("long_utf8", "€" * 2000), ("all_bytes", bytes(range(1, 256)))]
for c in range(1, 0x20):
    if c not in (9, 10, 13): S.append(("control_%02x" % c, bytes([c])))
for n, t in S:
    case(V, "string_" + n, doc1("string " + q(t)))
for n, call in (("len_prefix", 'string "abcdef" 3'), ("len_zero", 'string "abcdef" 0'), ("len_full", 'string "abc" 3'), ("len_cuts_escape", 'string "a\\nb" 2'), ("null_text", "string NULL"), ("null_text_len0", "string NULL 0"),
                ("null_text_len3", "string NULL 3"), ("len_cuts_nul", 'string "a\\x00bc" 3'), ("len_past_nul", 'string "a\\x00bc" 2')):
    case(V, "string_" + n, doc1(call))
B = ["abc", "ABC", "a", "_x", "_", "a1", "a_b", "a-b", "a.b", "a/b", "a:b", "a$b", "a-b.c/d:e$f", ".x", "..", "./x", "/usr/bin", "/", "./", "../x", "1abc", "1_000", "0x10", "12abc", "1d", "1-2", "1.2.3", "1e5.5", "5.x",
     "trueish", "nullable", "included", "TRUE", "Null", "NaN", "Infinity", "x" * 200, "v1.2.3-rc1", "http:/x", "e5", "E5", "e", "_1e5"]
for i, t in enumerate(B): case(V, "bare_ok_%02d" % i, doc1("bare " + q(t)), desc="bare %s" % q(t)[:40])
for i, t in enumerate(["", " ", "a b", "-a", "-", "+a", "@a", "#a", "$a", '"a"', "a,b", "a;b", "a=b", "a(b)", "a[b]", "a{b}", "a`b", "a\\b", "a\tb", "a\nb", "é", "1", "-1", "1.5", "1e5", "1E5", "1e+5", "007", "1.", "1.e3", "1e", "//", "/*", "/* c */"]):
    case(V, "bare_bad_%02d" % i, doc1("bare " + q(t)), desc="bare %s" % q(t)[:40])
for w in ("true", "false", "null", "include", "import", "vars"): case(V, "bare_reserved_" + w, doc1("bare " + q(w)))
case(V, "bare_null_text", doc1("bare NULL"))
for n, tag, data in (("untagged", None, "hello"), ("empty_tag", "", "hello"), ("tagged", "md", "# Title"), ("empty_data", "t", ""), ("empty_untagged", None, ""), ("newlines", "t", "line1\nline2\n"), ("crlf", "t", "a\r\nb"),
                     ("quotes", "t", 'say "x" \\n'), ("at_inside", "t", "@other x"), ("semicolon", "t", "a := 1; b := 2;"), ("braces", "t", "{[("), ("utf8", "t", "café 日本 \U0001f600"), ("invalid_utf8", "t", b"\xff\xfe\x80"),
                     ("control", "t", bytes(range(1, 32)) + b"\x7f"), ("nul", "t", b"a\x00b"), ("all_bytes", "t", bytes(b for b in range(1, 256) if b != 0x60)), ("long", "t", "y" * 10000), ("tag_1", "a", "x"), ("tag_underscore", "_", "x"),
                     ("tag_upper", "TAG", "x"), ("tag_digits", "t1x2", "x"), ("tag_31", "a" * 31, "x"), ("tag_utid", "utid", "abc-123"), ("tag_keyword", "true", "x")):
    case(V, "blob_" + n, doc1("blob %s %s" % (A(tag), q(data))))
for n, tag in (("32", "a" * 32), ("100", "a" * 100), ("digit_start", "1a"), ("hyphen", "a-b"), ("dot", "a.b"), ("space", "a b"), ("at", "@a"), ("backtick", "a`b"), ("quote", 'a"b'), ("utf8", "café"), ("colon", "a:b"), ("newline", "a\nb")):
    case(V, "blob_bad_tag_" + n, doc1("blob %s \"x\"" % q(tag)))
case(V, "blob_backtick_in_data", doc1('blob "t" "a`b"'))
case(V, "blob_backtick_only", doc1('blob NULL "`"'))
case(V, "blob_null_data", doc1('blob "t" NULL'))
case(V, "blob_null_data_len3", doc1('blob "t" NULL 3'))
case(V, "blob_len_prefix", doc1('blob "t" "abcdef" 3'))
for n, nm in (("ok", "x"), ("underscore", "_a"), ("dotted_bad", "a.b"), ("upper", "Abc"), ("long", "n" * 100), ("digit", "1")):
    case(V, "varref_" + n, doc1("varref " + q(nm)))
case(V, "varref_null", doc1("varref NULL"))
case(V, "varref_reserved", doc1('varref "true"'))
case(V, "varref_amp", doc1('varref "x"'), "AMP")
case(V, "varref_in_array", [let("a", "array"), let("r", 'varref "x"'), "append a r", 'add NULL "a" a', "emit"])
for k in ("null", "bool true", "int 1", 'string "s"', 'bare "ok"', 'blob "t" "d"', "double 1.5"):
    case(V, "unattached_%s" % k.split()[0], [let("v", k), "emit"], desc="a value never attached is not in the document")
case(V, "many_values", sum([[let("v%d" % i, "int %d" % i), 'add NULL "n%d" v%d' % (i, i)] for i in range(40)], []) + ["emit"])

# ============================================================================================ names
N = "names"
for i, nm in enumerate(["a", "abc", "A", "_a", "_", "a1", "a_b", "x" * 31, "x" * 100, "x" * 1000, "true_", "nullable", "included", "Include", "TRUE", "e5", "i", "vars_", "import_"]):
    case(N, "ok_%02d" % i, doc1("int 1", nm) if False else [let("v", "int 1"), "add NULL %s v" % q(nm), "emit"], desc="name %s" % q(nm)[:40])
for i, nm in enumerate(["", "1", "1a", "a-b", "a.b", "a b", "a$", "$a", "@a", "a:b", "a/b", "a;b", "a=b", "café", "é", "a\nb", "a\tb", " a", "a ", "a\"b", "a(b)", "-a"]):
    case(N, "bad_%02d" % i, [let("v", "int 1"), "add NULL %s v" % q(nm), "emit"], desc="name %s" % q(nm)[:40])
for w in ("true", "false", "null", "include", "import", "vars"):
    case(N, "reserved_" + w, [let("v", "int 1"), "add NULL %s v" % q(w), "emit"])
case(N, "null_name", [let("v", "int 1"), "add NULL NULL v", "emit"])
case(N, "dup_top", [let("a", "int 1"), 'add NULL "x" a', let("b", "int 2"), 'add NULL "x" b', "emit"])
case(N, "dup_top_case_differs", [let("a", "int 1"), 'add NULL "x" a', let("b", "int 2"), 'add NULL "X" b', "emit"])
case(N, "dup_top_after_failure", [let("a", "int 1"), 'add NULL "x" a', let("b", "int 2"), 'add NULL "x" b', 'add NULL "y" b', "emit"], cont=True, desc="the value of a failed add stays detached")
case(N, "dup_field", [let("o", "object"), let("a", "int 1"), 'add o "k" a', let("b", "int 2"), 'add o "k" b', 'add NULL "o" o', "emit"], cont=True)
case(N, "same_field_different_objects", [let("o", "object"), let("p", "object"), let("a", "int 1"), let("b", "int 2"), 'add o "k" a', 'add p "k" b', 'add NULL "o" o', 'add NULL "p" p', "emit"])
case(N, "field_same_as_top_name", [let("o", "object"), let("a", "int 1"), let("b", "int 2"), 'add o "o" a', 'add NULL "o" o', 'add NULL "k" b', "emit"])
case(N, "order_kept", sum([[let("v%d" % i, "int %d" % i), 'add NULL %s v%d' % (q(n), i)] for i, n in enumerate(["z", "a", "m", "b", "y"])], []) + ["emit"], desc="statements are emitted in the order added")
case(N, "many_top_300", sum([[let("v", "int %d" % i), 'add NULL "n%d" v' % i] for i in range(300)], []) + ["emit"])
case(N, "many_fields_300", [let("o", "object")] + sum([[let("v", "int %d" % i), 'add o "f%d" v' % i] for i in range(300)], []) + ['add NULL "o" o', "emit"])
case(N, "bad_name_value_stays_detached", [let("v", "int 1"), 'add NULL "1" v', 'add NULL "ok" v', "emit"], cont=True)

# ============================================================================================ collections
C = "collections"
def arr(kind, vals, h="a", name="a"):
    return [let(h, kind)] + sum([[let("e%d" % i, v), "append %s e%d" % (h, i)] for i, v in enumerate(vals)], []) + ["add NULL %s %s" % (q(name), h), "emit"]
for n, vals in (("ints", ["int 1", "int 2", "int 3"]), ("one", ["int 1"]), ("mixed", ["int 1", 'string "s"', "bool true", "null", 'bare "x"', 'blob "t" "d"', "double 2.5"]), ("strings", ['string ""', 'string "a\\nb"']),
                ("with_varref", ['varref "x"', "int 1"]), ("long_1000", ["int %d" % i for i in range(1000)]), ("two", ["int 1", "int 2"])):
    case(C, "array_" + n, arr("array", vals))
for n, vals in (("two", ["int 1", "int 2"]), ("three", ["int 1", 'string "s"', "bool true"]), ("one_fails", ["int 1"]), ("long_500", ["int %d" % i for i in range(500)]), ("mixed", ["null", 'bare "ok"', "double 1.5", 'blob "t" "x"'])):
    case(C, "tuple_" + n, arr("tuple", vals))
case(C, "array_empty", [let("a", "array"), 'add NULL "a" a', "emit"])
case(C, "tuple_empty", [let("a", "tuple"), 'add NULL "a" a', "emit"])
case(C, "object_empty", [let("a", "object"), 'add NULL "a" a', "emit"])
case(C, "empty_array_in_array", [let("a", "array"), let("b", "array"), "append a b", 'add NULL "a" a', "emit"])
case(C, "empty_nested_after_good", [let("a", "array"), let("x", "int 1"), "append a x", let("b", "array"), "append a b", 'add NULL "a" a', "emit"])
case(C, "unattached_empty_ok", [let("a", "array"), let("t", "tuple"), let("o", "object"), "emit"])
case(C, "empty_unattached_array_inside_ok", [let("a", "array"), let("b", "array"), "append a b", "emit"], desc="unattached collections are never emitted")
case(C, "array_of_arrays", [let("a", "array"), let("b", "array"), let("c", "array"), let("x", "int 1"), let("y", "int 2"), let("z", "int 3"), "append b x", "append b y", "append c z", "append a b", "append a c", 'add NULL "m" a', "emit"])
case(C, "array_of_tuples", [let("a", "array"), let("t1", "tuple"), let("t2", "tuple"), let("x", "int 1"), let("y", "int 2"), let("z", "int 3"), let("w", "int 4"), "append t1 x", "append t1 y", "append t2 z", "append t2 w", "append a t1", "append a t2", 'add NULL "p" a', "emit"])
case(C, "tuple_of_arrays", [let("t", "tuple"), let("a", "array"), let("b", "array"), let("x", "int 1"), let("y", "int 2"), "append a x", "append b y", "append t a", "append t b", 'add NULL "t" t', "emit"])
case(C, "array_of_objects", [let("a", "array"), let("o1", "object"), let("o2", "object"), let("x", "int 1"), let("y", "int 2"), 'add o1 "k" x', 'add o2 "k" y', "append a o1", "append a o2", 'add NULL "list" a', "emit"])
case(C, "object_in_tuple", [let("t", "tuple"), let("o", "object"), let("x", "int 1"), let("y", "int 2"), 'add o "k" x', "append t o", "append t y", 'add NULL "t" t', "emit"])
case(C, "object_nested", [let("a", "object"), let("b", "object"), let("c", "object"), let("x", "int 1"), 'add c "leaf" x', 'add b "c" c', 'add a "b" b', 'add NULL "a" a', "emit"])
case(C, "object_with_collections", [let("o", "object"), let("a", "array"), let("t", "tuple"), let("x", "int 1"), let("y", "int 2"), let("y2", "int 2"), let("z", "int 3"), "append a x", "append a y", "append t y2", "append t z", 'add o "arr" a', 'add o "tup" t', 'add NULL "o" o', "emit"])
case(C, "object_all_kinds", [let("o", "object")] + sum([[let("v", c), 'add o %s v' % q("f%d" % i)] for i, c in enumerate(["null", "bool true", "int 7", 'string "s"', 'bare "b"', 'blob "t" "d"', 'varref "r"', "double 0.25"])], []) + ['add NULL "o" o', "emit"])
case(C, "object_build_order", [let("o", "object"), 'add NULL "o" o', let("x", "int 1"), 'add o "k" x', "emit"], desc="members may be added after the object is attached")
case(C, "array_append_after_attach", [let("a", "array"), 'add NULL "a" a', let("x", "int 1"), "append a x", let("y", "int 2"), "append a y", "emit"])
case(C, "deep_build_bottom_up", [let("c", "array"), let("x", "int 1"), "append c x", let("b", "array"), "append b c", let("a", "array"), "append a b", 'add NULL "a" a', "emit"])
# attach rules
case(C, "append_twice", [let("a", "array"), let("b", "array"), let("x", "int 1"), "append a x", "append b x", "get_error", 'add NULL "a" a', "emit"], cont=True)
case(C, "append_same_twice", [let("a", "array"), let("x", "int 1"), "append a x", "append a x", "get_error"], cont=True)
case(C, "append_to_self", [let("a", "array"), "append a a", "get_error"], cont=True)
case(C, "append_ancestor", [let("a", "array"), let("b", "array"), let("c", "array"), "append a b", "append b c", "append c a", "get_error", "append c b", "get_error", "append c c", "get_error"], cont=True)
case(C, "append_cycle_tuple", [let("a", "tuple"), let("b", "tuple"), "append a b", "append b a", "get_error"], cont=True)
case(C, "append_to_scalar", [let("a", "int 1"), let("b", "int 2"), "append a b", "get_error"], cont=True)
case(C, "append_to_object", [let("o", "object"), let("b", "int 2"), "append o b", "get_error"], cont=True)
case(C, "append_to_null", [let("b", "int 2"), "append NULL b", "get_error"], cont=True)
case(C, "append_null_element", [let("a", "array"), "append a NULL", "get_error"], cont=True)
case(C, "append_both_null", ["append NULL NULL", "get_error"], cont=True)
case(C, "append_value_already_statement", [let("a", "array"), let("x", "int 1"), 'add NULL "x" x', "append a x", "get_error"], cont=True)
case(C, "add_value_already_element", [let("a", "array"), let("x", "int 1"), "append a x", 'add NULL "x" x', "get_error"], cont=True)
case(C, "add_value_twice", [let("x", "int 1"), 'add NULL "x" x', 'add NULL "y" x', "get_error"], cont=True)
case(C, "add_value_twice_in_object", [let("o", "object"), let("x", "int 1"), 'add o "x" x', 'add o "y" x', "get_error"], cont=True)
case(C, "add_object_to_itself", [let("o", "object"), 'add o "k" o', "get_error"], cont=True)
case(C, "add_ancestor_into_descendant", [let("a", "object"), let("b", "object"), 'add a "b" b', 'add b "a" a', "get_error"], cont=True)
case(C, "add_to_array", [let("a", "array"), let("x", "int 1"), 'add a "k" x', "get_error"], cont=True)
case(C, "add_to_scalar", [let("a", "int 1"), let("x", "int 1"), 'add a "k" x', "get_error"], cont=True)
case(C, "add_null_value", ['add NULL "k" NULL', "get_error"], cont=True)
case(C, "add_attached_subtree_root_again", [let("o", "object"), let("x", "int 1"), 'add o "k" x', 'add NULL "o" o', 'add NULL "o2" o', "get_error"], cont=True)
case(C, "reattach_after_failed_append", [let("a", "array"), let("x", "int 1"), "append a a", "append a x", 'add NULL "a" a', "emit"], cont=True)
case(C, "tuple_one_nested", [let("a", "array"), let("t", "tuple"), let("x", "int 1"), "append t x", "append a t", 'add NULL "a" a', "emit"], desc="a 1-tuple deep in the tree fails at emit")
case(C, "tuple_one_unattached", [let("t", "tuple"), let("x", "int 1"), "append t x", "emit"])

# ============================================================================================ attributes
AT = "attributes"
VALS = [("flag", None), ("word", "v"), ("int", "42"), ("neg", "-1"), ("float", "1.5"), ("path", "a/b.c"), ("true", "true"), ("null", "null"), ("dotted", "a.b"), ("long", "x" * 200)]
for n, v in VALS:
    case(AT, "module_" + n, ["attribute %s %s" % (q("k"), A(v)), "emit"])
    case(AT, "member_" + n, [let("a", "int 1"), let("m", 'add NULL "a" a'), "member_attribute m %s %s" % (q("k"), A(v)), "emit"])
for n, v in (("empty", ""), ("space", "a b"), ("quote", 'a"b'), ("semicolon", "a;b"), ("brace", "{"), ("bracket", "]"), ("comma", "a,b"), ("equals", "a=b"), ("hash", "#x"), ("backtick", "a`b"), ("newline", "a\nb"),
             ("utf8", "café"), ("at", "@x"), ("dollar", "$x"), ("paren", "(x)"), ("backslash", "a\\b"), ("tab", "a\tb"), ("colon", "a:b")):
    case(AT, "module_value_" + n, ["attribute \"k\" %s" % q(v), "emit"], desc="raw value %s" % q(v)[:30])
    case(AT, "member_value_" + n, [let("a", "int 1"), let("m", 'add NULL "a" a'), "member_attribute m \"k\" %s" % q(v), "emit"], desc="raw value %s" % q(v)[:30])
for n, t in (("empty", ""), ("plain", "hello"), ("quote", 'say "x"'), ("backslash", "a\\b"), ("newline", "a\nb"), ("tab", "a\tb"), ("cr", "a\rb"), ("utf8", "café 日"), ("nul", b"a\x00b"), ("control", bytes([1, 2, 31, 127])), ("semicolon", "a; b"),
             ("long", "z" * 5000), ("invalid_utf8", b"\xff\xfe")):
    case(AT, "module_string_" + n, ["attribute_string \"k\" %s" % q(t), "emit"])
    case(AT, "member_string_" + n, [let("a", "int 1"), let("m", 'add NULL "a" a'), "member_attribute_string m \"k\" %s" % q(t), "emit"])
case(AT, "module_string_len", ['attribute_string "k" "abcdef" 3', "emit"])
case(AT, "module_string_null_len0", ['attribute_string "k" NULL', "emit"])
case(AT, "module_string_null_len3", ['attribute_string "k" NULL 3', "emit"])
case(AT, "member_string_len", [let("a", "int 1"), let("m", 'add NULL "a" a'), 'member_attribute_string m "k" "abcdef" 2', "emit"])
case(AT, "member_string_null_len3", [let("a", "int 1"), let("m", 'add NULL "a" a'), 'member_attribute_string m "k" NULL 3', "get_error"], cont=True)
for i, k in enumerate(["a", "A", "_a", "a1", "a_b", "a-b", "a.b", "k" * 100, "true", "include", "null", "vars"]):
    case(AT, "key_ok_%02d" % i, ["attribute %s \"v\"" % q(k), "emit"], desc="module key %s" % q(k)[:30])
    case(AT, "member_key_ok_%02d" % i, [let("a", "int 1"), let("m", 'add NULL "a" a'), "member_attribute m %s \"v\"" % q(k), "emit"], desc="member key %s" % q(k)[:30])
for i, k in enumerate(["", "1", "1a", "a b", "a$", "@a", "a=b", "a;b", "café", "a\nb", "-a", "a:b"]):
    case(AT, "key_bad_%02d" % i, ["attribute %s \"v\"" % q(k), "get_error"], cont=True, desc="module key %s" % q(k)[:30])
    case(AT, "member_key_bad_%02d" % i, [let("a", "int 1"), let("m", 'add NULL "a" a'), "member_attribute m %s \"v\"" % q(k), "get_error"], cont=True, desc="member key %s" % q(k)[:30])
case(AT, "module_key_null", ["attribute NULL \"v\"", "get_error"], cont=True)
case(AT, "module_string_key_null", ["attribute_string NULL \"v\"", "get_error"], cont=True)
case(AT, "module_dup_key", ['attribute "k" "1"', 'attribute "k" "2"', "get_error", "emit"], cont=True)
case(AT, "module_dup_key_string_vs_raw", ['attribute "k" "1"', 'attribute_string "k" "2"', "get_error", "emit"], cont=True)
case(AT, "module_keys_many", sum([['attribute %s "%d"' % (q("k%d" % i), i)] for i in range(50)], []) + ["emit"])
case(AT, "module_order_kept", ['attribute "z" NULL', 'attribute "a" "1"', 'attribute_string "m" "s"', "emit"])
case(AT, "member_dup_key", [let("a", "int 1"), let("m", 'add NULL "a" a'), 'member_attribute m "k" "1"', 'member_attribute m "k" "2"', "get_error", "emit"], cont=True)
case(AT, "member_dup_key_string_vs_raw", [let("a", "int 1"), let("m", 'add NULL "a" a'), 'member_attribute m "k" "1"', 'member_attribute_string m "k" "2"', "get_error", "emit"], cont=True)
case(AT, "member_same_key_on_two_members", [let("a", "int 1"), let("b", "int 2"), let("m", 'add NULL "a" a'), let("n", 'add NULL "b" b'), 'member_attribute m "k" "1"', 'member_attribute n "k" "2"', "emit"])
case(AT, "member_many_attrs", [let("a", "int 1"), let("m", 'add NULL "a" a')] + ['member_attribute m %s "%d"' % (q("k%d" % i), i) for i in range(30)] + ["emit"])
case(AT, "member_attr_order_kept", [let("a", "int 1"), let("m", 'add NULL "a" a'), 'member_attribute m "z" NULL', 'member_attribute m "a" "1"', 'member_attribute_string m "m" "s"', "emit"])
case(AT, "member_attr_after_emit", [let("a", "int 1"), let("m", 'add NULL "a" a'), "emit", 'member_attribute m "k" "v"', "emit"])
case(AT, "member_attr_null_member", ['member_attribute NULL "k" "v"', "get_error"], cont=True)
case(AT, "member_attr_string_null_member", ['member_attribute_string NULL "k" "v"', "get_error"], cont=True)
case(AT, "member_attr_on_object_member", [let("o", "object"), let("x", "int 1"), let("m", 'add o "k" x'), 'member_attribute m "inner" "1"', 'add NULL "o" o', "emit"])
case(AT, "member_attr_on_nested_and_top", [let("o", "object"), let("x", "int 1"), let("m", 'add o "k" x'), 'member_attribute m "inner" "1"', let("p", 'add NULL "o" o'), 'member_attribute p "outer" "2"', "emit"])
case(AT, "member_attr_failed_then_ok", [let("a", "int 1"), let("m", 'add NULL "a" a'), 'member_attribute m "1" "v"', 'member_attribute m "k" "a b"', 'member_attribute m "k" "ok"', "emit"], cont=True)
case(AT, "attrs_and_statements", ['attribute "version" "1"', 'attribute_string "title" "T"', let("a", "int 1"), let("m", 'add NULL "a" a'), 'member_attribute m "doc" "x"', let("b", "string \"s\""), 'add NULL "b" b', "emit"])
case(AT, "module_attr_after_statements", [let("a", "int 1"), 'add NULL "a" a', 'attribute "late" "1"', "emit"], desc="module attributes are written before statements whenever they were added")
for call in ('attribute "k" "v"', 'attribute_string "k" "v"', 'include "x.anvl"'):
    case(AT, "amp_" + call.split()[0], [call, "get_error"], "AMP", cont=True)
case(AT, "amp_member_attribute", [let("a", "int 1"), let("m", 'add NULL "a" a'), 'member_attribute m "k" "v"', 'member_attribute_string m "k" "v"', 'set_base m "b"', "emit"], "AMP", cont=True)
case(AT, "amp_member_attribute_null_member", ['member_attribute NULL "k" "v"', "get_error"], "AMP", cont=True, desc="a null member is checked before the dialect")

# ============================================================================================ include
I = "include"
for i, p in enumerate(["a.anvl", "a", "dir/a.anvl", "./a.anvl", "../a.anvl", "/abs/a.anvl", "a b.anvl", "café.anvl", "a-b_c.d.anvl", "x" * 200, "a;b", "a\"b", "a'b", "a\\b", "..", ".", "~", "a:b", "a*b"]):
    case(I, "path_%02d" % i, ["include %s" % q(p), "emit"], desc="path %s" % q(p)[:40])
for i, p in enumerate(["", "a\nb", "a\tb", "a\rb", "a\x01b", "a\x7fb"]):
    case(I, "path_bad_%02d" % i, ["include %s" % q(p), "get_error"], cont=True, desc="path %s" % q(p)[:40])
case(I, "path_null", ["include NULL", "get_error"], cont=True)
case(I, "single", ['include "a.anvl"', "emit"])
case(I, "several_order_kept", ['include "z.anvl"', 'include "a.anvl"', 'include "m.anvl"', "emit"])
case(I, "duplicate_path", ['include "a.anvl"', 'include "a.anvl"', "emit"])
case(I, "with_statement", ['include "a.anvl"', let("v", "int 1"), 'add NULL "x" v', "emit"])
case(I, "before_attributes", ['include "a.anvl"', 'attribute "k" "v"', "emit"])
case(I, "after_attributes", ['attribute "k" "v"', 'include "a.anvl"', "emit"], desc="BR-2610-anvl-writer-003: includes are written first, then attributes")
case(I, "interleaved", ['attribute "a" "1"', 'include "x.anvl"', 'attribute "b" "2"', 'include "y.anvl"', 'attribute_string "c" "3"', 'include "z.anvl"', "emit"], desc="includes first in the order added, then attributes in the order added")
case(I, "attrs_then_includes_then_statements", ['attribute "a" "1"', 'attribute "b" NULL', 'include "x.anvl"', 'include "y.anvl"', let("v", "int 1"), 'add NULL "n" v', let("w", "int 2"), let("m", 'add NULL "m" w'), "emit"])
case(I, "after_statements", [let("v", "int 1"), 'add NULL "n" v', 'include "late.anvl"', "emit"])
case(I, "after_emit", ['attribute "k" "v"', "emit", 'include "a.anvl"', "emit"])
case(I, "interleaved_write", ['attribute "a" "1"', 'include "x.anvl"', "write", "emit"])
case(I, "failed_between", ['include "a.anvl"', 'include ""', 'attribute "k" "v"', 'attribute "1" "v"', 'include "b.anvl"', "emit"], cont=True)
case(I, "many", ['include "f%d.anvl"' % i for i in range(60)] + ["emit"])
case(I, "amp", ['include "a.anvl"', "get_error"], "AMP", cont=True)

# ============================================================================================ bases
BS = "bases"
def obj_member(name="base", h="o", mh="m"):
    return [let(h + "_v", "int 1"), let(h, "object"), 'add %s "k" %s_v' % (h, h), let(mh, 'add NULL %s %s' % (q(name), h))]
case(BS, "valid", obj_member("base") + [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "base"', "emit"])
case(BS, "valid_after_target", [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "base"'] + obj_member("base") + ["emit"], desc="the base may be declared later")
case(BS, "missing_target", [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "nothere"', "emit"], desc="a base that names no top-level member is accepted")
case(BS, "target_is_self", [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "d" d'), 'set_base dm "d"', "emit"])
case(BS, "replaced", obj_member("base") + obj_member("other", "p", "pm") + [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "base"', 'set_base dm "other"', "emit"], desc="set_base again replaces the base")
case(BS, "replaced_back_to_valid", [let("n", "int 1"), 'add NULL "scalar" n', let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "scalar"', "emit", 'set_base dm "base"', "emit"], cont=True,
     desc="emit fails while the base names a scalar, and works after the base is replaced")
case(BS, "chain", obj_member("a") + [let("b", "object"), let("x", "int 2"), 'add b "j" x', let("bm", 'add NULL "b" b'), 'set_base bm "a"', let("c", "object"), let("y", "int 3"), 'add c "l" y', let("cm", 'add NULL "c" c'), 'set_base cm "b"', "emit"])
case(BS, "several_derived", obj_member("base") + sum([[let("d%d" % i, "object"), let("x%d" % i, "int %d" % i), 'add d%d "j" x%d' % (i, i), let("m%d" % i, 'add NULL "d%d" d%d' % (i, i)), 'set_base m%d "base"' % i] for i in range(4)], []) + ["emit"])
case(BS, "base_with_attributes", obj_member("base") + [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "base"', 'member_attribute dm "k" "v"', 'member_attribute_string dm "s" "text"', "emit"])
case(BS, "base_set_after_attributes", obj_member("base") + [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'member_attribute dm "k" "v"', 'set_base dm "base"', "emit"])
case(BS, "nested_member_base", obj_member("base") + [let("outer", "object"), let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add outer "derived" d'), 'set_base dm "base"', 'add NULL "outer" outer', "emit"])
for bi, bad in enumerate(("", "1", "a-b", "a.b", "a b", "true", "null", "include", "café")):
    case(BS, "bad_base_%02d" % bi, obj_member("base") + ['set_base m %s' % q(bad), "get_error"], cont=True, desc="base %s" % q(bad))
case(BS, "bad_base_null", obj_member("base") + ["set_base m NULL", "get_error"], cont=True)
case(BS, "null_member", ['set_base NULL "b"', "get_error"], cont=True)

# BR-2610-anvl-writer-004: base cycles and bases that name nothing
def two_objs(a="a", b="b"):
    return [let("oa", "object"), let("xa", "int 1"), 'add oa "f" xa', let("ma", 'add NULL %s oa' % q(a)),
            let("ob", "object"), let("xb", "int 2"), 'add ob "f" xb', let("mb", 'add NULL %s ob' % q(b))]
case(BS, "cycle_self", [let("o", "object"), let("x", "int 1"), 'add o "f" x', let("m", 'add NULL "d" o'), 'set_base m "d"', "emit", "get_error"], cont=True, desc="a statement based on itself")
case(BS, "cycle_two", two_objs() + ['set_base ma "b"', 'set_base mb "a"', "emit", "write", "get_error"], cont=True, desc="a -> b -> a")
case(BS, "cycle_three", two_objs() + [let("oc", "object"), let("xc", "int 3"), 'add oc "f" xc', let("mc", 'add NULL "c" oc'), 'set_base ma "b"', 'set_base mb "c"', 'set_base mc "a"', "emit"], cont=True, desc="a -> b -> c -> a")
case(BS, "cycle_chain_ok", two_objs() + [let("oc", "object"), let("xc", "int 3"), 'add oc "f" xc', let("mc", 'add NULL "c" oc'), 'set_base ma "b"', 'set_base mb "c"', "emit"], desc="a -> b -> c is a chain")
case(BS, "cycle_leads_into", two_objs() + [let("oc", "object"), let("xc", "int 3"), 'add oc "f" xc', let("mc", 'add NULL "c" oc'), 'set_base mc "a"', 'set_base ma "b"', 'set_base mb "a"', "emit"], cont=True, desc="c is not in the cycle a -> b -> a but leads into it")
case(BS, "cycle_with_include", ['include "x.anvl"'] + two_objs() + ['set_base ma "b"', 'set_base mb "a"', "emit"], cont=True, desc="an include does not excuse a cycle")
case(BS, "cycle_broken_by_replace", two_objs() + ['set_base ma "b"', 'set_base mb "a"', "emit", 'set_base mb "nothing"', "emit"], cont=True, desc="replacing a base in the cycle by one that names nothing gives MISSING_BASE instead")
case(BS, "cycle_repaired", two_objs() + [let("oc", "object"), let("xc", "int 3"), 'add oc "f" xc', let("mc", 'add NULL "c" oc'), 'set_base ma "b"', 'set_base mb "a"', "emit", 'set_base mb "c"', "emit"], cont=True, desc="repointing b at c ends the cycle")
case(BS, "cycle_vs_scalar_first", [let("n", "int 1"), 'add NULL "n" n'] + two_objs("p", "q") + [let("oc", "object"), let("xc", "int 3"), 'add oc "f" xc', let("mc", 'add NULL "r" oc'), 'set_base ma "q"', 'set_base mb "p"', 'set_base mc "n"', "emit"], cont=True, desc="the scalar base is reported before the cycle")
case(BS, "cycle_nested_member_not_part", two_objs() + [let("outer", "object"), let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add outer "d" d'), 'set_base dm "a"', 'add NULL "outer" outer', 'set_base ma "b"', "emit"], desc="a nested member named like a top-level one is not part of a chain")
case(BS, "missing_no_include", two_objs() + ['set_base ma "nothere"', "emit", "write", "get_error"], cont=True, desc="no include: the reader would reject MISSING_BASE")
case(BS, "missing_with_include", ['include "x.anvl"'] + two_objs() + ['set_base ma "nothere"', "emit", "write"], desc="an include may supply the base")
case(BS, "missing_include_added_later", two_objs() + ['set_base ma "nothere"', "emit", 'include "x.anvl"', "emit"], cont=True, desc="the check looks at the includes held at emit time")
case(BS, "missing_declared_later", two_objs() + ['set_base ma "later"', let("ol", "object"), let("xl", "int 3"), 'add ol "f" xl', 'add NULL "later" ol', "emit"], desc="a base declared after the statement is found")
case(BS, "missing_sibling", [let("outer", "object"), let("s", "object"), let("sx", "int 1"), 'add s "f" sx', 'add outer "sib" s', let("d", "object"), let("dx", "int 2"), 'add d "f" dx', let("dm", 'add outer "der" d'), 'set_base dm "sib"', 'add NULL "outer" outer', "emit"], cont=True, desc="bases are looked up among the top-level names only")
case(BS, "missing_sibling_with_include", ['include "x.anvl"', let("outer", "object"), let("s", "object"), let("sx", "int 1"), 'add s "f" sx', 'add outer "sib" s', let("d", "object"), let("dx", "int 2"), 'add d "f" dx', let("dm", 'add outer "der" d'), 'set_base dm "sib"', 'add NULL "outer" outer', "emit"])
case(BS, "missing_replaced_by_valid", two_objs() + ['set_base ma "nothere"', "emit", 'set_base ma "b"', "emit"], cont=True)
case(BS, "missing_vs_scalar_first", [let("n", "int 1"), 'add NULL "n" n'] + two_objs("p", "q") + ['set_base ma "gone"', 'set_base mb "n"', "emit"], cont=True, desc="the scalar base is reported before the missing one")
case(BS, "missing_nested_non_object_with_include", ['include "x.anvl"', let("o", "object"), let("n", "int 1"), 'add o "target" n', 'add NULL "outer" o', let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "target"', "emit"])
# non-object values: refused at set_base
for k in ("null", "bool true", "int 1", 'string "s"', 'bare "x"', 'blob "t" "d"', 'varref "r"', "array", "tuple"):
    nm = k.split()[0]
    case(BS, "non_object_value_%s" % nm, [let("v", k), let("m", 'add NULL "n" v'), 'set_base m "b"', "get_error"], cont=True, desc="BR-2610-anvl-writer-002: a base needs an object value")
case(BS, "non_object_value_array_filled", [let("v", "array"), let("x", "int 1"), "append v x", let("m", 'add NULL "n" v'), 'set_base m "b"', "get_error", "emit"], cont=True)
# base naming a top-level non-object: fails at emit/write
for k in ("null", "bool true", "int 1", 'string "s"', 'bare "x"', 'blob "t" "d"', 'varref "r"'):
    nm = k.split()[0]
    case(BS, "target_non_object_%s" % nm, [let("t", k), 'add NULL "target" t'] + [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "target"', "emit", "write", "get_error"], cont=True,
         desc="BR-2610-anvl-writer-002: the base names a top-level member that is not an object")
case(BS, "target_non_object_array", [let("t", "array"), let("e", "int 1"), "append t e", 'add NULL "target" t', let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "target"', "emit"])
case(BS, "target_non_object_tuple", [let("t", "tuple"), let("e", "int 1"), let("f", "int 2"), "append t e", "append t f", 'add NULL "target" t', let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "target"', "emit"])
case(BS, "target_non_object_declared_later", [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "target"', let("t", "int 1"), 'add NULL "target" t', "emit"])
case(BS, "target_nested_non_object_not_checked", [let("o", "object"), let("n", "int 1"), 'add o "target" n', 'add NULL "outer" o', let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "target"', "emit"],
     desc="only top-level names are looked up")
# nested and unattached members: the check runs over every member the builder created
case(BS, "nested_member_target_non_object", [let("t", "int 1"), 'add NULL "target" t', let("outer", "object"), let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add outer "derived" d'), 'set_base dm "target"', 'add NULL "outer" outer', "emit"])
case(BS, "unattached_member_target_non_object", [let("t", "int 1"), 'add NULL "target" t', let("loose", "object"), let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add loose "derived" d'), 'set_base dm "target"', "emit", "write", "get_error"], cont=True,
     desc="a member of an object that is never attached still fails the check")
case(BS, "unattached_member_missing_target", [let("loose", "object"), let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add loose "derived" d'), 'set_base dm "nothere"', "emit"])
case(BS, "unattached_member_valid_target", obj_member("base") + [let("loose", "object"), let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add loose "derived" d'), 'set_base dm "base"', "emit"])
case(BS, "unattached_member_in_array_target_non_object", [let("t", "int 1"), 'add NULL "target" t', let("arr", "array"), let("o", "object"), let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add o "derived" d'), 'set_base dm "target"', "append arr o", "emit"],
     desc="the array holding the object is never attached")
case(BS, "target_becomes_non_object_later", obj_member("base") + [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "base"', "emit", let("s", "int 5"), 'add NULL "base2" s', 'set_base dm "base2"', "emit"], cont=True)
case(BS, "failed_set_base_keeps_old", obj_member("base") + [let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "base"', 'set_base dm "1x"', "emit"], cont=True)
case(BS, "non_object_set_base_does_not_stick", [let("v", "int 1"), let("m", 'add NULL "n" v'), 'set_base m "b"', "emit"], cont=True)
case(BS, "target_is_failed_member", [let("t", "int 1"), 'add NULL "target" t', 'add NULL "target" t', let("d", "object"), let("x", "int 2"), 'add d "j" x', let("dm", 'add NULL "derived" d'), 'set_base dm "target"', "emit"], cont=True)
case(BS, "amp_set_base", [let("v", "int 1"), let("m", 'add NULL "n" v'), 'set_base m "b"', "get_error"], "AMP", cont=True)

# ============================================================================================ find
FD = "find"
case(FD, "found_top", [let("v", "int 1"), 'add NULL "a" v', 'find NULL "a"'])
case(FD, "none_top", [let("v", "int 1"), 'add NULL "a" v', 'find NULL "b"'])
case(FD, "empty_doc", ['find NULL "a"'])
case(FD, "case_sensitive", [let("v", "int 1"), 'add NULL "a" v', 'find NULL "A"'])
case(FD, "found_in_object", [let("o", "object"), let("v", "int 1"), 'add o "k" v', 'find o "k"', 'find o "z"'])
case(FD, "object_member_not_in_document", [let("o", "object"), let("v", "int 1"), 'add o "k" v', 'add NULL "o" o', 'find NULL "k"', 'find NULL "o"', 'find o "k"'])
case(FD, "first_of_each", [let("a", "int 1"), let("b", "int 2"), 'add NULL "a" a', 'add NULL "b" b', 'find NULL "a"', 'find NULL "b"'])
case(FD, "in_array", [let("a", "array"), 'find a "k"', "get_error"], cont=True, desc="find on a non-object is none, and sets no error")
case(FD, "in_scalar", [let("a", "int 1"), 'find a "k"', "get_error"], cont=True)
case(FD, "null_name", ["find NULL NULL"])
case(FD, "empty_name", ['find NULL ""'])
case(FD, "bad_name_not_error", ['find NULL "1 bad"', "get_error"], cont=True)
case(FD, "keeps_previous_error", [let("v", 'numeric "x"'), 'find NULL "a"', "get_error"], cont=True, desc="find does not touch the error")
case(FD, "does_not_clear_error", [let("v", 'numeric "x"'), 'find NULL "a"', let("w", "null"), "get_error"], cont=True)
case(FD, "then_set_base", [let("b", "object"), let("bx", "int 1"), 'add b "k" bx', 'add NULL "base" b', let("o", "object"), let("x", "int 1"), 'add o "k" x', 'add NULL "o" o', let("m", 'find NULL "o"'), 'set_base m "base"', "emit"], desc="find returns the member add returned")
case(FD, "then_attribute", [let("v", "int 1"), 'add NULL "a" v', let("m", 'find NULL "a"'), 'member_attribute m "k" "v"', "emit"])
case(FD, "then_set_base_non_object", [let("v", "int 1"), 'add NULL "a" v', let("m", 'find NULL "a"'), 'set_base m "base"', "get_error"], cont=True)
case(FD, "none_then_use", [let("m", 'find NULL "a"'), 'member_attribute m "k" "v"', "get_error", 'set_base m "b"', "get_error"], cont=True)
case(FD, "nested_find", [let("o", "object"), let("p", "object"), let("x", "int 1"), 'add p "leaf" x', 'add o "p" p', 'add NULL "o" o', let("m", 'find o "p"'), 'member_attribute m "k" "v"', 'find p "leaf"', "emit"])
case(FD, "find_null_object_is_document", [let("v", "int 1"), 'add NULL "a" v', 'find NULL "a"'])
case(FD, "find_duplicate_rejected", [let("a", "int 1"), let("b", "int 2"), 'add NULL "a" a', 'add NULL "a" b', 'find NULL "a"'], cont=True)
case(FD, "find_in_unattached_object", [let("o", "object"), let("x", "int 1"), 'add o "k" x', 'find o "k"', 'find NULL "k"'])
case(FD, "amp_find", [let("v", "int 1"), 'add NULL "a" v', 'find NULL "a"', 'find NULL "b"'], "AMP")
case(FD, "find_after_failed_add", [let("v", "int 1"), 'add NULL "a" v', 'add NULL "a" v', 'find NULL "a"'], cont=True)
case(FD, "find_long_name", [let("v", "int 1"), 'add NULL %s v' % q("n" * 500), 'find NULL %s' % q("n" * 500)])

# ============================================================================================ depth
DP = "depth"
def chain(kind, n, leaf="int 1", wrap=None):
    """n nested containers of `kind` (array, tuple, object) around a leaf, attached as the top-level statement d."""
    L = [let("c%d" % i, kind) for i in range(1, n + 1)]
    if kind == "object":
        L += ['add c%d "k" c%d' % (i, i + 1) for i in range(1, n)]
        L += [let("leaf", leaf), 'add c%d "k" leaf' % n]
    else:
        L += ["append c%d c%d" % (i, i + 1) for i in range(1, n)]
        if kind == "tuple":
            L += [let("x%d" % i, "int %d" % i) for i in range(1, n + 1)] + ["append c%d x%d" % (i, i) for i in range(1, n + 1)]
            L += [let("leaf", leaf), "append c%d leaf" % n]
        else:
            L += [let("leaf", leaf), "append c%d leaf" % n]
    return L
for kind in ("array", "tuple", "object"):
    for n in (1, 2, 10, 255, 256, 257, 258, 300):
        if kind == "tuple" and n in (258, 300): continue
        case(DP, "%s_%d" % (kind, n), chain(kind, n) + ['add NULL "d" c1', "emit"], desc="%d nested %ss" % (n, kind))
for kind in ("array", "object"):
    case(DP, "%s_257_get_error_then_retry" % kind, chain(kind, 257) + ['add NULL "d" c1', "emit", "get_error", "emit", "write", "get_error"], cont=True, desc="DEPTH_EXCEEDED is builder-only")
    case(DP, "%s_300_unattached_ok" % kind, chain(kind, 300) + ["emit"], desc="a tree that is not attached to the document is never checked")
    case(DP, "%s_257_in_second_statement" % kind, [let("a", "int 1"), 'add NULL "a" a'] + chain(kind, 257) + ['add NULL "d" c1', "emit"])
# every tuple needs two elements: its child plus one scalar (the innermost gets the extra leaf)
def mixed_ok(n):
    kinds = ["array", "tuple", "object"]
    L = [let("c%d" % i, kinds[i % 3]) for i in range(1, n + 1)]
    for i in range(1, n):
        L.append('add c%d "k" c%d' % (i, i + 1) if kinds[i % 3] == "object" else "append c%d c%d" % (i, i + 1))
    for i in range(1, n + 1):
        if kinds[i % 3] == "tuple" and True:
            L += [let("t%d" % i, "int 1"), "append c%d t%d" % (i, i)]
            if i == n: L += [let("t%d_b" % i, "int 2"), "append c%d t%d_b" % (i, i)]
    k = kinds[n % 3]
    if k != "tuple": L += [let("leaf", "int 9"), ('add c%d "leaf" leaf' % n) if k == "object" else ("append c%d leaf" % n)]
    return L
for n in (255, 256, 257):
    case(DP, "mixed_%d" % n, mixed_ok(n) + ['add NULL "d" c1', "emit"], desc="array, tuple and object alternating, %d deep" % n)
case(DP, "wide_not_deep", [let("a", "array")] + sum([[let("b%d" % i, "array"), let("x%d" % i, "int %d" % i), "append b%d x%d" % (i, i), "append a b%d" % i] for i in range(300)], []) + ['add NULL "a" a', "emit"], desc="300 siblings are one level deep")
case(DP, "depth_precedes_writer_errors", chain("array", 257) + ['add NULL "d" c1', let("e", "array"), 'add NULL "empty" e', "emit"], cont=True, desc="the depth check runs before anything is written, so an empty array elsewhere is not reached")
case(DP, "empty_array_beats_nothing", chain("array", 256) + ['add NULL "d" c1', let("e", "array"), 'add NULL "empty" e', "emit"], cont=True)
case(DP, "bases_precede_depth", chain("array", 257) + ['add NULL "d" c1', let("t", "int 1"), 'add NULL "target" t', let("o", "object"), let("x", "int 1"), 'add o "k" x', let("m", 'add NULL "derived" o'), 'set_base m "target"', "emit"], cont=True,
     desc="the base check runs before the depth check")
case(DP, "depth_amp_arrays", [let("v", "int 1"), let("a", "array"), "append a v", 'add NULL "a" a', "emit"], "AMP")
case(DP, "depth_object_in_array_256", [let("a", "array")] + [] + chain("object", 255) + ["append a c1", 'add NULL "d" a', "emit"], desc="an array holding 255 nested objects is 256 deep")
case(DP, "depth_object_in_array_257", [let("a", "array")] + chain("object", 256) + ["append a c1", 'add NULL "d" a', "emit"])

# ============================================================================================ amp
AM = "amp"
case(AM, "scalars", sum([[let("v%d" % i, c), 'add NULL "s%d" v%d' % (i, i)] for i, c in enumerate(["null", "bool true", "int 5", 'string "s"', 'bare "b"', 'blob "t" "d"', "double 1.5", 'numeric "2"'])], []) + ["emit"], "AMP")
case(AM, "array_of_scalars", [let("a", "array")] + sum([[let("e%d" % i, c), "append a e%d" % i] for i, c in enumerate(["int 1", 'string "s"', "null"])], []) + ['add NULL "a" a', "emit"], "AMP")
case(AM, "tuple_of_scalars", [let("a", "tuple"), let("x", "int 1"), let("y", "bool true"), "append a x", "append a y", 'add NULL "t" a', "emit"], "AMP")
case(AM, "array_in_array", [let("a", "array"), let("b", "array"), "append a b", "get_error"], "AMP", cont=True)
case(AM, "tuple_in_array", [let("a", "array"), let("b", "tuple"), "append a b", "get_error"], "AMP", cont=True)
case(AM, "array_in_tuple", [let("a", "tuple"), let("b", "array"), "append a b", "get_error"], "AMP", cont=True)
case(AM, "object", ["object", "get_error"], "AMP", cont=True)
case(AM, "varref", ['varref "x"', "get_error"], "AMP", cont=True)
case(AM, "varref_bad_name", ['varref "1"', "get_error"], "AMP", cont=True, desc="the dialect is checked first")
case(AM, "include", ['include "a"', "get_error"], "AMP", cont=True)
case(AM, "include_bad_path", ['include ""', "get_error"], "AMP", cont=True, desc="the dialect is checked first")
case(AM, "attribute", ['attribute "a" "b"', "get_error"], "AMP", cont=True)
case(AM, "attribute_bad_key", ['attribute "1" "b"', "get_error"], "AMP", cont=True)
case(AM, "member_attribute", [let("v", "int 1"), let("m", 'add NULL "a" v'), 'member_attribute m "k" "v"', "get_error"], "AMP", cont=True)
case(AM, "set_base", [let("v", "int 1"), let("m", 'add NULL "a" v'), 'set_base m "b"', "get_error"], "AMP", cont=True)
case(AM, "add_to_object_handle", [let("v", "int 1"), 'add NULL "a" v', "emit"], "AMP")
case(AM, "empty_array", [let("a", "array"), 'add NULL "a" a', "emit"], "AMP", cont=True)
case(AM, "tuple_one", [let("a", "tuple"), let("x", "int 1"), "append a x", 'add NULL "a" a', "emit"], "AMP", cont=True)
case(AM, "dup_top", [let("a", "int 1"), 'add NULL "x" a', let("b", "int 2"), 'add NULL "x" b', "get_error"], "AMP", cont=True)
case(AM, "builder2_amp_foreign_object", [let("n", "int 1"), "let o = other object", "get_error", "other get_error"], "AML", cont=True, b2="AMP")
case(AM, "amp_big_array", [let("a", "array")] + sum([[let("e%d" % i, "int %d" % i), "append a e%d" % i] for i in range(300)], []) + ['add NULL "a" a', "emit"], "AMP")
case(AM, "amp_depth_irrelevant", [let("a", "array"), let("b", "array"), "append a b", "get_error"], "AMP", cont=True)

# ============================================================================================ handles
HD = "handles"
case(HD, "null_everywhere", ["append NULL NULL", 'add NULL "a" NULL', 'set_base NULL "b"', 'member_attribute NULL "k" "v"', 'member_attribute_string NULL "k" "v"', 'find NULL "a"', "emit", "get_error"], cont=True)
case(HD, "null_collection", [let("x", "int 1"), "append NULL x", "get_error"], cont=True)
case(HD, "null_object_is_document", [let("x", "int 1"), 'add NULL "a" x', "emit"])
case(HD, "failed_let_is_null", [let("n", 'numeric "x"'), let("a", "array"), "append a n", "get_error", 'add NULL "n" n', "get_error"], cont=True)
case(HD, "failed_add_member_null", [let("a", "int 1"), 'add NULL "x" a', let("b", "int 2"), let("m", 'add NULL "x" b'), 'set_base m "b"', "get_error", 'member_attribute m "k" "v"', "get_error"], cont=True)
case(HD, "rebind", [let("v", "int 1"), 'add NULL "a" v', let("v", "int 2"), 'add NULL "b" v', "emit"], desc="a name may be bound again")
case(HD, "rebind_to_failed", [let("v", "int 1"), let("v", 'numeric "x"'), 'add NULL "a" v', "get_error"], cont=True)
case(HD, "handle_kept_across_emit", [let("a", "array"), let("x", "int 1"), "append a x", 'add NULL "a" a', "emit", let("y", "int 2"), "append a y", "emit"])
case(HD, "node_not_member", [let("o", "object"), let("x", "int 1"), 'add o "k" x', 'add NULL "o" o', "emit"])
case(HD, "unbound_result_dropped", ["int 1", "string \"s\"", "array", "emit"], desc="a constructor without let is legal and the node is dropped")
case(HD, "add_without_let", [let("x", "int 1"), 'add NULL "a" x', "emit"])
case(HD, "handle_names", sum([[let(n, "int %d" % i), 'add NULL %s %s' % (q("n%d" % i), n)] for i, n in enumerate(["a", "A", "_a", "a1", "_", "add", "emit", "int", "find", "dialect", "x" * 100])], []) + ["emit"], desc="handle names are not reserved against call names")
# the second builder
case(HD, "foreign_value_into_add", [let("n", 'string "x"'), "let o = other object", 'other add o "a" n', "get_error", "other get_error"], cont=True, b2="AML")
case(HD, "foreign_value_into_document", [let("n", 'string "x"'), 'other add NULL "a" n', "other get_error", "emit"], cont=True, b2="AML")
case(HD, "foreign_collection", [let("a", "array"), "let n = other int 1", "append a n", "get_error", "other append a n", "other get_error"], cont=True, b2="AML")
case(HD, "foreign_object_target", [let("o", "object"), "let n = other int 1", 'other add o "k" n', "other get_error", 'add o "k" n', "get_error"], cont=True, b2="AML")
case(HD, "foreign_member_set_base", [let("o", "object"), let("m", 'add NULL "o" o'), 'other set_base m "b"', "other get_error", "get_error"], cont=True, b2="AML")
case(HD, "foreign_member_attribute", [let("v", "int 1"), let("m", 'add NULL "v" v'), 'other member_attribute m "k" "v"', "other get_error", 'other member_attribute_string m "k" "v"', "other get_error", "emit"], cont=True, b2="AML")
case(HD, "foreign_member_dialect_order", [let("v", "int 1"), let("m", 'add NULL "v" v'), 'other set_base m "b"', "other get_error", 'other member_attribute m "k" "v"', "other get_error"], cont=True, b2="AMP", desc="the foreign check comes before the dialect check")
case(HD, "foreign_member_both_ways", [let("v", "int 1"), let("m", 'add NULL "v" v'), "let w = other int 2", "let n = other add NULL \"w\" w", 'member_attribute n "k" "v"', "get_error", 'other member_attribute m "k" "v"', "other get_error", 'member_attribute m "k" "v"', "emit", "other emit"], cont=True, b2="AML")
case(HD, "foreign_find", [let("o", "object"), 'other find o "k"', 'other find NULL "k"', "other get_error"], cont=True, b2="AML")
case(HD, "foreign_find_keeps_error", [let("o", "object"), 'other find o "k"', "other get_error"], cont=True, b2="AML")
case(HD, "two_builders_independent", [let("a", "int 1"), 'add NULL "x" a', "let b = other int 2", 'other add NULL "y" b', "emit", "other emit", "get_error", "other get_error"], b2="AML")
case(HD, "two_builders_errors_independent", [let("a", 'numeric "x"'), "let b = other int 2", "get_error", "other get_error"], cont=True, b2="AML")
case(HD, "second_builder_dialects", [let("a", "int 1"), 'add NULL "x" a', "let o = other object", "other get_error", "emit"], "AMP", cont=True, b2="AML")
case(HD, "second_builder_amp", ["let o = other object", "other get_error", "let v = other varref \"x\"", "other get_error", "let a = other int 1", 'other add NULL "a" a', "other emit"], "AML", cont=True, b2="AMP")
case(HD, "second_builder_invalid", ["let n = other int 1", "other get_error", "other emit", 'other find NULL "a"'], cont=True, b2="9")
case(HD, "second_builder_invalid_foreign", [let("n", "int 1"), "other append n n", "other get_error", "get_error"], cont=True, b2="-3")
case(HD, "other_get_error_fresh", ["other get_error", "get_error"], b2="AML")
case(HD, "other_emit_empty", ["other emit", "emit"], b2="AMP")
case(HD, "other_write", [let("a", "int 1"), 'add NULL "a" a', "let b = other int 2", 'other add NULL "b" b', "other write", "write", "other write AMP"], b2="AML")
case(HD, "cycle_through_other", [let("a", "array"), "let b = other array", "append a b", "other append b a", "get_error", "other get_error"], cont=True, b2="AML")
case(HD, "foreign_value_stays_usable", [let("n", 'string "x"'), "let o = other object", 'other add o "a" n', 'add NULL "a" n', "emit"], cont=True, b2="AML")

# ============================================================================================ envelope (King & Crucible)
KC = "envelope"
def kc(name, fields, dialect="AMP", desc=None, tail=("emit",)):
    steps = []
    for i, (nm, call) in enumerate(fields):
        steps += [let("f%d" % i, call), 'add NULL %s f%d' % (q(nm), i)]
    case(KC, name, steps + list(tail), dialect, desc=desc)
def req(utid="id-1", seq=3, payload=('blob "table" "[(1)]"')):
    return [("utid", 'blob "utid" %s' % q(utid)), ("seq", "int %d" % seq), ("payload", payload)]
kc("request_basic", req())
kc("request_aml", req(), "AML")
kc("request_seq0", req(seq=0))
kc("request_neg_seq", req(seq=-1))
kc("request_seq_big", req(seq=9223372036854775807))
kc("request_seq_min", req(seq=-9223372036854775808))
kc("request_other_tag", req(payload='blob "rows" "[(1,2)]"'))
kc("request_untagged_payload", req(payload='blob NULL "raw"'))
kc("request_empty_payload", req(payload='blob "table" ""'))
kc("request_multiline", req(payload='blob "table" "[\\n(1),\\n(2)\\n]"'))
kc("request_utf8", req(payload='blob "text" "café 日本"'.encode().decode()))
kc("request_long_payload", req(payload='blob "table" %s' % q("[" + ", ".join("(%d)" % i for i in range(2000)) + "]")))
kc("request_rows", req(payload='blob "table" "[(1, \\"a\\"), (2, \\"b\\")]"'))
kc("request_attrs_free", req(), desc="the envelope has no attributes")
kc("request_with_member_attribute", req(), "AML", tail=(let("m", 'find NULL "seq"'), 'member_attribute m "k" "v"', "emit"))
kc("request_member_attribute_amp", req(), "AMP", tail=(let("m", 'find NULL "seq"'), 'member_attribute m "k" "v"', "get_error", "emit"), desc="AMP refuses attributes")
kc("bad_utid_tag", [("utid", 'blob "ut-id" "x"'), ("seq", "int 1")])
kc("payload_backtick", req(payload='blob "table" "a`b"'))
kc("dup_seq", req() + [("seq", "int 4")])
kc("dup_utid", req() + [("utid", 'blob "utid" "other"')])
def resp(name, result_steps, status_steps=None, dialect="AMP", utid="id-1", seq=7, desc=None, tail=("emit",)):
    steps = [let("u", 'blob "utid" %s' % q(utid)), 'add NULL "utid" u', let("s", "int %d" % seq), 'add NULL "seq" s'] + result_steps + list(tail)
    case(KC, name, steps, dialect, desc=desc)
def result_tuple(vals):
    return [let("r", "tuple")] + sum([[let("r%d" % i, v), "append r r%d" % i] for i, v in enumerate(vals)], []) + ['add NULL "result" r']
def result_array(vals):
    return [let("r", "array")] + sum([[let("r%d" % i, v), "append r r%d" % i] for i, v in enumerate(vals)], []) + ['add NULL "result" r']
resp("response_ok_fields", result_tuple(['bare "ok"', "int 1", 'string "x"']))
resp("response_ok_int", result_tuple(['bare "ok"', "int 42"]))
resp("response_error", result_tuple(['bare "error"', 'string "boom: \\"quoted\\""', "int 500"]))
resp("response_string_status", result_tuple(['string "ok"', "int 1"]))
resp("response_int_status", result_tuple(["int 0", "int 1"]))
resp("response_list", result_array(["int 1", "int 2", "int 3"]))
resp("response_many", result_array(["int %d" % i for i in range(200)]))
resp("response_float_edges", result_tuple(["double 0.1", "double 1e21", "double -0", "double 0x0000000000000001"]))
resp("response_int_edges", result_tuple(["int 9223372036854775807", "int -9223372036854775808", "int 0"]))
resp("response_escapes", result_tuple(['string "a\\nb\\t\\"c\\"\\\\"', 'string "\\x01\\x7f"']))
resp("response_with_blob", result_tuple(['bare "ok"', 'blob "table" "[(1)]"']))
resp("response_single_field_fails", [let("r", "tuple"), let("r0", 'bare "ok"'), "append r r0", 'add NULL "result" r'], desc="a 1-tuple fails at emit")
resp("response_nested_tuple_fails", [let("r", "tuple"), let("r0", 'bare "ok"'), "append r r0", let("i", "tuple"), let("i0", "int 1"), let("i1", "int 2"), "append i i0", "append i i1", "append r i", 'add NULL "result" r'], desc="a nested tuple is refused in AMP")
resp("response_aml", result_tuple(['bare "ok"', "int 1"]), dialect="AML")
resp("response_nested_aml", [let("r", "tuple"), let("r0", 'bare "ok"'), "append r r0", let("i", "tuple"), let("i0", "int 1"), let("i1", "int 2"), "append i i0", "append i i1", "append r i", 'add NULL "result" r'], dialect="AML")
resp("response_write_and_emit", result_tuple(['bare "ok"', "int 1"]), tail=("emit", "write", "write AML"))
case(KC, "request_then_response_one_builder", [let("u", 'blob "utid" "id-1"'), 'add NULL "utid" u', let("s", "int 1"), 'add NULL "seq" s', let("p", 'blob "table" "[(1)]"'), 'add NULL "payload" p', "emit"], "AMP", b2="AMP")
case(KC, "two_envelopes_two_builders", [let("u", 'blob "utid" "id-1"'), 'add NULL "utid" u', let("s", "int 1"), 'add NULL "seq" s', "emit",
     "let v = other blob \"utid\" \"id-2\"", 'other add NULL "utid" v', "let t = other int 2", 'other add NULL "seq" t', "other emit"], "AMP", b2="AMP")

# ============================================================================================ flywire-shaped documents
FW = "flywire"
def fw_schema():
    s = ['attribute "schema" "orders"', 'attribute_string "title" "Order tables"', 'include "common.anvl"']
    s += [let("base", "object"), let("bid", "int 1"), 'add base "id" bid', let("bm", 'add NULL "base_table" base'), 'member_attribute bm "abstract" NULL']
    s += [let("orders", "object"), let("t", "string \"orders\""), 'add orders "table" t', let("cols", "array")]
    for i, (nm, ty) in enumerate([("id", "int64"), ("customer", "string"), ("total", "decimal"), ("placed", "timestamp")]):
        s += [let("c%d" % i, "object"), let("n%d" % i, "string %s" % q(nm)), 'add c%d "name" n%d' % (i, i), let("y%d" % i, "bare %s" % q(ty)), 'add c%d "type" y%d' % (i, i), "append cols c%d" % i]
    s += ['add orders "columns" cols', let("om", 'add NULL "orders" orders'), 'set_base om "base_table"', 'member_attribute om "version" "3"', 'member_attribute_string om "note" "primary table"']
    return s
case(FW, "schema_doc", fw_schema() + ["emit"])
case(FW, "schema_doc_write", fw_schema() + ["write"])
case(FW, "schema_doc_emit_twice", fw_schema() + ["emit", "emit"])
case(FW, "schema_doc_base_scalar", fw_schema() + [let("s", "int 1"), 'add NULL "scalar" s', let("d", "object"), let("dx", "int 1"), 'add d "x" dx', let("dm", 'add NULL "bad" d'), 'set_base dm "scalar"', "emit", "get_error"], cont=True)
def fw_config():
    s = ['attribute "profile" "prod"', 'include "base.anvl"', 'include "secrets.anvl"']
    s += [let("db", "object"), let("h", 'string "db.internal"'), 'add db "host" h', let("p", "int 5432"), 'add db "port" p', let("pw", 'varref "db_password"'), 'add db "password" pw', let("tls", "bool true"), 'add db "tls" tls']
    s += [let("dbm", 'add NULL "database" db'), 'member_attribute dbm "secret" NULL']
    s += [let("hosts", "array")] + sum([[let("hs%d" % i, 'string "node%d.example"' % i), "append hosts hs%d" % i] for i in range(5)], []) + ['add NULL "hosts" hosts']
    s += [let("lim", "tuple"), let("l0", "int 10"), let("l1", "int 100"), "append lim l0", "append lim l1", 'add NULL "limits" lim']
    s += [let("tp", "double 0.75"), 'add NULL "timeout_ratio" tp', let("none", "null"), 'add NULL "fallback" none']
    s += [let("rep", "object"), let("rr", "int 3"), 'add rep "replicas" rr', let("repm", 'add NULL "replica" rep'), 'set_base repm "database"']
    return s
case(FW, "config_doc", fw_config() + ["emit"])
case(FW, "config_doc_write", fw_config() + ["write"])
case(FW, "config_doc_find", fw_config() + ['find NULL "database"', 'find NULL "replica"', 'find NULL "nope"', "emit"])
case(FW, "config_doc_member_attrs_after", fw_config() + ['find NULL "hosts"', "emit"])
case(FW, "rows_doc", [let("rows", "array")] + sum([[let("r%d" % i, "tuple"), let("a%d" % i, "int %d" % i), let("b%d" % i, 'string "row %d"' % i), let("c%d" % i, "double %s" % (i + 0.5)), "append r%d a%d" % (i, i), "append r%d b%d" % (i, i), "append r%d c%d" % (i, i), "append rows r%d" % i] for i in range(25)], []) + ['add NULL "rows" rows', "emit"])
case(FW, "nested_objects_doc", [let("root", "object")] + sum([[let("o%d" % i, "object"), let("v%d" % i, "int %d" % i), 'add o%d "value" v%d' % (i, i)] for i in range(6)], []) + ['add root "child%d" o%d' % (i, i) for i in range(6)] + ['add NULL "root" root', "emit"])
case(FW, "settings_with_blobs", [let("sql", 'blob "sql" "SELECT a, b FROM t WHERE c = \\"x\\";"'), 'add NULL "query" sql', let("md", 'blob "md" "# Notes\\n- one\\n- two\\n"'), 'add NULL "notes" md', let("raw", "blob NULL \"opaque\""), 'add NULL "raw" raw', "emit"])
case(FW, "flags_and_modules", ['attribute "module" "flywire.core"', 'attribute "strict" NULL', 'attribute_string "description" "FlyWire core module"', 'include "types.anvl"',
     let("v", 'string "1.2.3"'), let("m", 'add NULL "version" v'), 'member_attribute m "frozen" NULL', 'member_attribute m "since" "1.0"', "emit"])
case(FW, "invalid_doc_dup_table", fw_schema() + [let("dup", "int 1"), 'add NULL "orders" dup', "get_error", "emit"], cont=True)

# ---- write ------------------------------------------------------------------------------------------------------------
os.makedirs(out_dir, exist_ok=True)
seen = set()
for area, name, text in cases:
    assert (area, name) not in seen, (area, name)
    seen.add((area, name))
    d = os.path.join(out_dir, area)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name + ".bscript"), "w", newline="") as f: f.write(text)
if force:   # remove scripts that are no longer generated (never a .dump)
    for dirpath, _, files in os.walk(out_dir):
        for fn in files:
            if fn.endswith(".bscript") and (os.path.basename(dirpath), fn[:-len(".bscript")]) not in seen:
                os.remove(os.path.join(dirpath, fn))
print("gen_builder: %d scripts" % len(cases))
