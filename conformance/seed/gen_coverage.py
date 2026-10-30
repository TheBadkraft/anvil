#!/usr/bin/env python3
"""Seeds conformance/cases/coverage/ (FR-2610-anvl-conformance-002): the cases the Phase 0 review found missing.

  includes/  the include graph: a missing include, a missing include in a nested child, a failing child, a
             self include (exact spelling = cycle; "./" spelling = same document), mutual includes, byte-identical
             copies, the pre-order walk with a diamond, paths with "..", paths at the 1024-byte limit
  nesting/   arrays, tuples, objects, attributes and fragments nested up to 400 levels deep
  columns/   error columns after multibyte characters (2, 3 and 4 bytes), tabs, invalid UTF-8, a BOM, CR, CRLF
  escapes/   the string escapes: \\t in every position, and every backslash pair that is not an escape
  numbers/   the integer and float overflow boundaries, rounding ties, subnormals, signed zero, long spellings
  errors/    inputs for error codes the other groups do not reach

Deterministic; the output is committed and not regenerated at check time (like the other seed scripts).
Usage: gen_coverage.py <repo-root>      Refuses to run over an existing cases/coverage unless --force.
"""
import os, shutil, sys
from decimal import Decimal, getcontext

root = sys.argv[1]
out = os.path.join(root, "conformance", "cases", "coverage")
if os.path.exists(out):
    if "--force" not in sys.argv:
        sys.exit("cases/coverage exists; pass --force to wipe and regenerate")
    shutil.rmtree(out)

def put(rel, data):
    path = os.path.join(out, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if isinstance(data, str):
        data = data.encode("utf-8")
    with open(path, "wb") as f:
        f.write(data)

# ------------------------------------------------------------------------------------------------ includes
I = "includes/"
put(I + "leaf_a.anvl", "a := 1;\n")
put(I + "leaf_b.anvl", "b := 2;\n")
put(I + "leaf_c.anvl", "c := 3;\n")
put(I + "leaf_d.anvl", "d := 4;\n")
put(I + "leaf_e.anvl", "e := 5;\n")
put(I + "leaf_a_copy.anvl", "a := 1;\n")                       # byte-identical to leaf_a.anvl
put(I + "empty.anvl", "")
put(I + "missing_root.anvl", 'include "no_such_file.anvl";\nx := 1;\n')
put(I + "missing_second.anvl", 'include "leaf_a.anvl";\ninclude "no_such_file.anvl";\nx := 1;\n')
put(I + "mid_with_missing.anvl", 'include "no_such_file.anvl";\nm := 1;\n')
put(I + "missing_nested.anvl", 'include "mid_with_missing.anvl";\nx := 1;\n')
put(I + "child_unterminated.anvl", 'c := "never closed;\n')
put(I + "child_bad_number.anvl", "\n\nn := 1e;\n")
put(I + "child_wrong_shebang.anvl", "#!zzz\nq := 1;\n")
put(I + "failing_child_string.anvl", 'include "child_unterminated.anvl";\nx := 1;\n')
put(I + "failing_child_number.anvl", 'include "leaf_a.anvl";\ninclude "child_bad_number.anvl";\n')
put(I + "failing_child_shebang.anvl", 'include "child_wrong_shebang.anvl";\n')
put(I + "self_exact.anvl", 'include "self_exact.anvl";\nx := 1;\n')                 # same spelling: a cycle
put(I + "self_dot.anvl", 'include "./self_dot.anvl";\nx := 1;\n')                   # other spelling, same bytes: the same document
put(I + "mutual_a.anvl", 'include "mutual_b.anvl";\na := 1;\n')                     # a -> b -> a, by name: a cycle
put(I + "mutual_b.anvl", 'include "mutual_a.anvl";\nb := 1;\n')
put(I + "mutual_dot_a.anvl", 'include "./mutual_dot_b.anvl";\na := 1;\n')           # a -> b -> a by "./" name: no cycle error
put(I + "mutual_dot_b.anvl", 'include "./mutual_dot_a.anvl";\nb := 1;\n')
put(I + "identical_copies.anvl", 'include "leaf_a.anvl";\ninclude "leaf_a_copy.anvl";\nx := 1;\n')
put(I + "same_twice.anvl", 'include "leaf_a.anvl";\ninclude "leaf_a.anvl";\nx := 1;\n')
put(I + "same_twice_dot.anvl", 'include "leaf_a.anvl";\ninclude "./leaf_a.anvl";\nx := 1;\n')
put(I + "root_equals_child.anvl", 'include "leaf_a.anvl";\na := 1;\n')
put(I + "include_empty_file.anvl", 'include "empty.anvl";\nx := 1;\n')
# the pre-order walk: root -> [w_a, w_b]; w_a -> [w_c]; w_c -> [leaf_e]; w_b -> [leaf_d, w_c].
# Pre-order depth first gives root, w_a, w_c, leaf_e, w_b, leaf_d; w_c is not repeated under w_b.
put(I + "walk_c.anvl", 'include "leaf_e.anvl";\nc := 3;\n')
put(I + "walk_a.anvl", 'include "walk_c.anvl";\na := 1;\n')
put(I + "walk_b.anvl", 'include "leaf_d.anvl";\ninclude "walk_c.anvl";\nb := 2;\n')
put(I + "walk_root.anvl", 'include "walk_a.anvl";\ninclude "walk_b.anvl";\nroot := 0;\n')
put(I + "walk_root_reversed.anvl", 'include "walk_b.anvl";\ninclude "walk_a.anvl";\nroot := 0;\n')
# a path with directories: a child's includes resolve against the child's own directory
put(I + "sub/s1.anvl", 'include "s2.anvl";\ns1 := 1;\n')
put(I + "sub/s2.anvl", 'include "../leaf_b.anvl";\ns2 := 2;\n')
put(I + "dirs_root.anvl", 'include "sub/s1.anvl";\nx := 1;\n')
put(I + "dotdot_root.anvl", 'include "sub/../leaf_c.anvl";\ninclude "sub/./s2.anvl";\nx := 1;\n')
put(I + "amp_child.amp", "#!amp\nk := 1;\n")
put(I + "aml_includes_amp.anvl", 'include "amp_child.amp";\nx := 1;\n')
put(I + "child_aml_shebang.amp", "#!aml\nk := { a := 1; };\n")
put(I + "amp_extension_child_aml_content.anvl", 'include "child_aml_shebang.amp";\nx := 1;\n')
# the include path limit: the joined path "<directory>/<name>" must be shorter than 1024 bytes
# The joined path is "coverage/includes/<name>" (the root is run as cases/coverage/includes/<file>, so the
# directory is 17 bytes plus the "/"): it fits while len(name) <= 1005 (not found), and is too long from 1006 on.
# A name of 1024 bytes or more is too long whatever the directory.
for n in (100, 900, 1000, 1005, 1006, 1007, 1023, 1024, 1025, 2000, 5000):
    put(I + "path_len_%d.anvl" % n, 'include "%s";\nx := 1;\n' % ("b" * n))
put(I + "abs_missing.anvl", 'include "/nonexistent-anvil-conformance-dir/x.anvl";\n')
put(I + "bare_missing_suffix.anvl", 'include "leaf_a";\n')
put(I + "no_semicolon_include.anvl", 'include "leaf_a.anvl"\nx := 1;\n')
put(I + "include_after_statement.anvl", 'x := 1;\ninclude "leaf_a.anvl";\n')
put(I + "include_before_shebang_attr.anvl", '#!aml\n@[m="1"]\ninclude "leaf_a.anvl";\nx := 1;\n')

# ------------------------------------------------------------------------------------------------ nesting
N = "nesting/"
def arr(n): return "x := " + "[" * n + "1" + "]" * n + ";\n"
def tup(n): return "x := " + "(1," * n + "1" + ")" * n + ";\n"
def obj(n): return "x := " + "{ a := " * n + "1; }" + "; }" * (n - 1) + ";\n"
def mixed(n):
    opens = ["[", "(1,", "{ a := ", "[{ a := ", "((1,"]
    # alternates array / tuple / object, closing in reverse
    s, c = "", ""
    for i in range(n):
        k = i % 3
        if k == 0: s += "["; c = "]" + c
        elif k == 1: s += "(1,"; c = ")" + c
        else: s += "{ a := "; c = "; }" + c
    return "x := " + s + "1" + c + ";\n"
for n in (1, 2, 16, 64, 100, 256, 400):
    put(N + "array_%d.anvl" % n, arr(n))
for n in (2, 16, 64, 100, 256):
    put(N + "tuple_%d.anvl" % n, tup(n))
    put(N + "object_%d.anvl" % n, obj(n))
for n in (9, 99, 255):
    put(N + "mixed_%d.anvl" % n, mixed(n))
put(N + "array_wide_2000.anvl", "x := [" + ",".join(str(i) for i in range(2000)) + "];\n")
put(N + "object_wide_500.anvl", "x := { " + " ".join("k%d := %d;" % (i, i) for i in range(500)) + " };\n")
put(N + "array_of_arrays_100.anvl", "x := [" + ",".join("[%d,%d]" % (i, i + 1) for i in range(100)) + "];\n")
for n in (100, 256):
    put(N + "fragment_array_%d.frag" % n, "[" * n + "1" + "]" * n)
put(N + "fragment_object_100.frag", "{ a := " * 100 + "1; }" + "; }" * 99)
put(N + "attr_value_nested_array_error.anvl", "x @[k=" + "[" * 50 + "1" + "]" * 50 + "] := 1;\n")
put(N + "unclosed_array_100.anvl", "x := " + "[" * 100 + "1;\n")
put(N + "unclosed_array_400.anvl", "x := " + "[" * 400 + "1")
put(N + "unclosed_object_100.anvl", "x := " + "{ a := " * 100 + "1; }" + "; }" * 98)
put(N + "unclosed_tuple_100.anvl", "x := " + "(1," * 100 + "1")
put(N + "close_without_open.anvl", "x := 1" + "]" * 100 + ";\n")
put(N + "array_chain_refs.anvl", "a := 1;\n" + "".join("v%d := [$%s];\n" % (i, "a" if i == 0 else "v%d" % (i - 1)) for i in range(60)))

# ------------------------------------------------------------------------------------------------ columns
C = "columns/"
prefixes = {
    "plain": "abc",
    "e2": "\u00e9\u00e9",              # 2-byte
    "cjk3": "\u65e5\u672c\u8a9e",      # 3-byte
    "emoji4": "\U0001F600",            # 4-byte
    "mixed": "a\u00e9\u65e5\U0001F600z",
    "tab": "\t",
    "tabs": "\t\t\t",
    "tabspace": " \t \t",
    "tab_e": "\u00e9\t\u00e9",
    "bad_utf8": b"\xff\xfe\x80",
    "overlong": b"\xc0\xaf",
    "lone_lead": b"\xe6\x97",
    "nul": b"\x00",
    "ctrl": b"\x01\x1f\x7f",
}
def enc(p): return p if isinstance(p, bytes) else p.encode("utf-8")
templates = {
    "str_then_junk": lambda p: b'x := "' + p + b'" y;\n',                 # 4017 after the string
    "comment_then_num": lambda p: b"/* " + p + b" */ x := 5.;\n",          # 5009
    "line_comment_next": lambda p: b"# " + p + b"\ny := 1e;\n",            # 5008 on line 2
    "multiline_string": lambda p: b'x := "' + p + b"\n" + p + b'" @;\n',   # error on line 2 after the prefix
    "before_assign": lambda p: b"x " + p + b" := 1;\n",                    # the prefix in the statement itself
    "leading": lambda p: p + b" := 1;\n",                                  # 2001 at the first byte
    "unterminated": lambda p: b"x := " + p + b' "abc\n',                   # 5002 after the prefix
}
for pn, p in prefixes.items():
    for tn, t in templates.items():
        put(C + "%s__%s.anvl" % (tn, pn), t(enc(p)))
put(C + "bom_then_error.anvl", b"\xef\xbb\xbfx := 5.;\n")
put(C + "bom_ok.anvl", b"\xef\xbb\xbfx := 1;\n")
put(C + "bom_shebang.anvl", b"\xef\xbb\xbf#!aml\nx := 1;\n")
put(C + "crlf_error_line3.anvl", b"x := 1;\r\ny := 2;\r\nz := 5.;\r\n")
put(C + "cr_only_error.anvl", b"x := 1;\ry := 2;\rz := 5.;\r")
put(C + "lf_error_line3.anvl", b"x := 1;\ny := 2;\nz := 5.;\n")
put(C + "mixed_eol_error.anvl", b"x := 1;\r\ny := 2;\rz := 3;\nw := 5.;\n")
put(C + "formfeed_vt.anvl", b"x := 1;\x0b\x0cy := 5.;\n")
put(C + "tab_indent_error.anvl", b"a := {\n\tb := {\n\t\tc := 5.;\n\t};\n};\n")
put(C + "tab_after_token.anvl", b"x\t:=\t1\ty;\n")
put(C + "tab_attr_error.anvl", b'x @[\ta\t=\t"1"\t,\t] := 1;\n')
put(C + "mb_attr_error.anvl", 'x @[k="\u00e9\u00e9", ] := 1;\n')
put(C + "mb_ident_value.anvl", "x := \u00e9;\n")
put(C + "mb_in_string_ok.anvl", 'x := "\u00e9\u65e5\U0001F600";\ny := "tab\there";\n')
put(C + "mb_blob_ok.anvl", "x := `\u00e9\u65e5\U0001F600`;\ny := @t`\t\u00e9`;\n")
put(C + "mb_fragment_error.frag", '["\u00e9\u00e9" "x"]')
put(C + "tab_fragment_error.frag", '[1,\t\t2\t3]')
put(C + "nul_in_string_error.anvl", b'x := "a\x00b" y;\n')
put(C + "nul_alone.anvl", b"\x00")
put(C + "high_bytes_alone.anvl", b"\xff")
put(C + "error_on_last_byte.anvl", b"x := 1e")
put(C + "error_at_eof_after_tab.anvl", b"x := \"abc\t")

# ------------------------------------------------------------------------------------------------ escapes
E = "escapes/"
esc = {
    "t_only": r'\t', "t_start": r'\tabc', "t_end": r'abc\t', "t_middle": r'a\tb', "t_double": r'\t\t',
    "t_triple_mixed": r'a\tb\tc\td', "t_with_n": r'\t\n', "t_n_t": r'\n\t', "t_r_t": r'\r\t\r',
    "t_after_backslash": r'\\\t', "escaped_backslash_then_t": r'\\t', "backslash_t_backslash": r'\\t\\',
    "t_before_quote": r'\t\"', "quote_t": r'\"\t', "t_run_16": r'\t' * 16,
    "raw_tab_byte": "a\tb", "raw_tab_and_escape": "\t" + r'\t' + "\t",
    "n": r'\n', "r": r'\r', "backslash": r'\\', "quote": r'\"', "all_five": r'\n\t\r\\\"',
    "unknown_x": r'\x41', "unknown_u": r'\u00e9', "unknown_u_brace": r'\u{1F600}', "unknown_0": r'\0',
    "unknown_slash": r'\/', "unknown_squote": r"\'", "unknown_a": r'\a', "unknown_b": r'\b', "unknown_f": r'\f',
    "unknown_v": r'\v', "unknown_T": r'\T', "unknown_N": r'\N', "unknown_space": '\\ ', "unknown_digit": r'\1',
    "unknown_utf8": "\\\u00e9", "unknown_then_t": r'\q\t', "t_then_unknown": r'\t\q', "backslash_only_before_quote": '\\\\',
    "triple_backslash_quote": r'\\\"', "four_backslashes": r'\\\\', "five_backslashes_t": r'\\\\\t',
    "raw_newline": "a\nb", "raw_cr": "a\rb", "raw_nul": "a\x00b", "empty": "", "unicode": "\u00e9\\t\u65e5",
    "backslash_newline": "\\\n",
}
for name, body in esc.items():
    put(E + "str_%s.anvl" % name, 'x := "' + body + '";\n')
for name in ("t_only", "t_double", "t_after_backslash", "escaped_backslash_then_t", "unknown_x", "all_five"):
    put(E + "frag_%s.frag" % name, '"' + esc[name] + '"')
    put(E + "attr_%s.anvl" % name, 'x @[k="' + esc[name] + '"] := 1;\n')
    put(E + "array_%s.anvl" % name, 'x := ["' + esc[name] + '", "' + esc[name] + '"];\n')
    put(E + "object_%s.anvl" % name, 'x := { f := "' + esc[name] + '"; };\n')
put(E + "tuple_t.anvl", 'x := ("a\\tb", "\\t");\n')
put(E + "blob_backslash_t.anvl", 'x := `a\\tb`;\ny := @t`\\t\\n`;\n')
put(E + "key_with_t_escape_error.anvl", 'x := { "a\\tb" := 1; };\n')
put(E + "unterminated_after_backslash.anvl", 'x := "abc\\')
put(E + "unterminated_after_escape.anvl", 'x := "abc\\t')
put(E + "escaped_quote_unterminated.anvl", 'x := "abc\\"')
put(E + "attr_flag_and_tabbed.anvl", 'x @[a, b="\\t", c] := 1;\n')

# ------------------------------------------------------------------------------------------------ numbers
Nn = "numbers/"
getcontext().prec = 700
half = 2 ** 1024 - 2 ** 970           # the midpoint between DBL_MAX and 2^1024: strtod rounds it up, to infinity
nums = {
    "i64_max": "9223372036854775807", "i64_max_plus1": "9223372036854775808", "i64_max_plus2": "9223372036854775809",
    "i64_min": "-9223372036854775808", "i64_min_minus1": "-9223372036854775809", "i64_min_minus2": "-9223372036854775810",
    "u64_max": "18446744073709551615", "u64_max_plus1": "18446744073709551616", "i64_x10": "92233720368547758070",
    "i64_max_padded": "0009223372036854775807", "i64_over_padded": "0009223372036854775808", "i64_min_padded": "-0009223372036854775808",
    "i64_min_over_padded": "-0009223372036854775809",
    "zero_padded_70": "0" * 70 + "5", "zero_padded_neg": "-" + "0" * 70 + "7", "neg_zero": "-0", "neg_zero_padded": "-000", "zero": "0",
    "digits_100": "1" + "0" * 99, "digits_400_neg": "-" + "9" * 400, "ones_19": "1111111111111111111", "ones_20": "11111111111111111111",
    "i53_plus1": "9007199254740993", "i63_pow": "4611686018427387904",
    "f_dbl_max": "1.7976931348623157e308", "f_dbl_max_158": "1.7976931348623158e308",
    "f_over_159": "1.7976931348623159e308", "f_over_159_neg": "-1.7976931348623159e308", "f_over_2e308": "2e308",
    "f_dbl_max_digits": "%d.0" % (2 ** 1024 - 2 ** 971),
    "f_halfway": "%d.0" % half, "f_halfway_minus1": "%d.0" % (half - 1), "f_halfway_plus1": "%d.0" % (half + 1),
    "f_halfway_neg": "-%d.0" % half, "f_halfway_minus1_neg": "-%d.0" % (half - 1),
    "f_1e308": "1e308", "f_1e309": "1e309", "f_1e999": "1e999", "f_1e1000000": "1e1000000", "f_neg_1e999": "-1e999",
    "f_exp_digits_huge": "1e99999999999999999999", "f_exp_plus": "1e+309", "f_exp_upper": "1E309", "f_exp_upper_ok": "1E308",
    "f_leading_zero_exp": "1e+0000000000000000000000000000000000000308", "f_leading_zero_exp_over": "1e+0000000000000000000000000000000000000309",
    "f_zero_huge_exp": "0e99999999999999999999", "f_zero_dot_huge_exp": "0.0e999", "f_under_1e999": "1e-999", "f_under_neg": "-1e-999",
    "f_exp_digits_huge_neg": "1e-99999999999999999999",
    "f_min_subnormal": "5e-324", "f_min_subnormal_exact": "4.9406564584124654e-324", "f_tie_to_zero": "2.4703282292062327e-324",
    "f_tie_up": "2.4703282292062328e-324", "f_below_tie": "2.4703282292062326e-324", "f_max_subnormal": "2.2250738585072009e-308",
    "f_min_normal": "2.2250738585072014e-308", "f_subnormal_mid": "1e-310",
    "f_zero": "0.0", "f_neg_zero": "-0.0", "f_zero_exp": "0e0", "f_neg_zero_exp": "-0e5", "f_zero_many": "0." + "0" * 400,
    "f_point_one": "0.1", "f_point_three": "0.3", "f_one_third": "0.3333333333333333", "f_pi": "3.141592653589793",
    "f_pi_long": "3.14159265358979323846264338327950288419716939937510582097494459",
    "f_tie_2p53": "9007199254740993.0", "f_tie_2p53_exp": "9007199254740993e0", "f_2p53_plus3": "9007199254740995.0",
    "f_1e22": "1e22", "f_1e23": "1e23", "f_1e23_long": "1.00000000000000000000000e23", "f_8p5e22": "8.5e22",
    "f_15_digits": "123456789012345.0", "f_16_digits": "1234567890123456.0", "f_17_digits": "12345678901234567.0",
    "f_long_mantissa": "0." + "1234567890" * 40, "f_long_mantissa_exp": "0." + "1234567890" * 40 + "e300",
    "f_exp_zero": "1.5e0", "f_exp_neg": "2.5e-5", "f_capital": "2.5E-5", "f_leading_zeros": "0000.5e0",
    "f_int_exp": "12e3", "f_negative": "-123.456e-7", "f_trailing_zeros": "1.500000000000000000000",
    "f_sub_denormal_digits": "0." + "0" * 330 + "1", "f_big_digits": "1" + "0" * 308 + ".0",
    "f_big_digits_over": "1" + "0" * 309 + ".0", "f_1e22_digits": "10000000000000000000000.0",
    "f_fast_edge_22": "1234567e22", "f_fast_edge_23": "1234567e23", "f_fast_div_22": "1234567e-22", "f_fast_div_23": "1234567e-23",
    "f_denormal_neg": "-4.9e-324",
}
for name, s in nums.items():
    put(Nn + "%s.anvl" % name, "x := %s;\n" % s)
put(Nn + "frag_i64_over.frag", "9223372036854775808")
put(Nn + "frag_f_over.frag", "1.7976931348623159e308")
put(Nn + "frag_f_max.frag", "1.7976931348623157e308")
put(Nn + "frag_neg_i64_over.frag", "-9223372036854775809")
put(Nn + "array_mixed_overflow.anvl", "x := [9223372036854775807, 9223372036854775808, 1e308, 1e309, -9223372036854775808, -9223372036854775809];\n")
put(Nn + "attr_overflow.anvl", "x @[a=9223372036854775808, b=1e309, c=-0.0] := 1;\n")
put(Nn + "not_numbers.anvl", "a := .5;\nb := 1e5.5;\nc := 1.2.3;\nd := 0x10;\ne := 1_000;\nf := 1-2;\ng := 1d;\nh := NaN;\ni := Infinity;\nj := 12abc;\n")
put(Nn + "plus_sign_error.anvl", "x := +5;\n")
put(Nn + "dot_no_digits_error.anvl", "x := 5.;\n")
put(Nn + "dot_exp_error.anvl", "x := 1.e5;\n")
put(Nn + "exp_no_digits_error.anvl", "x := 1e;\n")
put(Nn + "exp_sign_no_digits_error.anvl", "x := 1e+;\n")
put(Nn + "neg_dot_error.anvl", "x := -.5;\n")
put(Nn + "minus_alone_error.anvl", "x := -;\n")
put(Nn + "double_minus_error.anvl", "x := --5;\n")

# the token boundary after a number: ; , ] ) } space tab CR LF or the end of input end a value; anything else
# makes the whole word a bare literal (or, after "5." or "1e", an error)
bounds = {"slash_comment": "1/* c */", "hash": "1#x", "dot": "1.5.", "letter": "1.5x", "dot_letter": "5.x", "dot_dash": "5.-",
          "exp_letter": "1ex", "exp_dot": "1e5.", "minus": "1-", "minus_digit": "1-2", "colon": "1:2", "slash": "1/2",
          "dollar": "1$", "underscore": "1_", "close_paren": "(1)", "close_brace": "{ a := 1 }", "tab": "1\t",
          "cr": "1\r", "comma": "[1,2]", "neg_letter": "-a", "neg_bare": "-1a", "dot_exp_boundary": "5.e", "dot_semicolon": "5.;"}
for name, v in bounds.items():
    put(Nn + "bound_%s.anvl" % name, "x := %s;\n" % v)
put(Nn + "bound_eof.frag", "1.5e3")
put(Nn + "bound_eof_error.frag", "1e")
put(Nn + "bound_dot_eof.frag", "5.")
put(Nn + "bound_tab_array.frag", "[1\t,\t2\t]")

# ------------------------------------------------------------------------------------------------ errors
R = "errors/"
put(R + "tuple_unclosed_after_comma.anvl", "x := (1,")
put(R + "tuple_unclosed_after_comma_nl.anvl", "x := (1,\n")
put(R + "tuple_unclosed_after_two.anvl", "x := (1, 2,")
put(R + "tuple_unclosed_nested.anvl", "x := ((1, 2),")
put(R + "tuple_unclosed_in_array.anvl", "x := [(1, 2),")
put(R + "tuple_unclosed_in_object.anvl", "x := { a := (1,")
put(R + "tuple_unclosed_in_attr.anvl", "x @[k=(1,")
put(R + "tuple_unclosed.frag", "(1,")
put(R + "tuple_unclosed_two.frag", "(1, 2,")
put(R + "tuple_comma_semicolon.anvl", "x := (1, 2,;\n")
put(R + "tuple_missing_comma_eof.anvl", "x := (1, 2")
put(R + "tuple_one_element.anvl", "x := (1);\n")
put(R + "tuple_one_element_comma.anvl", "x := (1,);\n")
put(R + "array_unclosed_eof.anvl", "x := [1, 2")
put(R + "array_trailing_comma.anvl", "x := [1, 2,];\n")
put(R + "object_unclosed_eof.anvl", "x := { a := 1;")
put(R + "object_unclosed_mid.anvl", "x := { a := 1")
put(R + "empty_object.anvl", "x := { };\n")
put(R + "empty_array.anvl", "x := [];\n")
put(R + "empty_tuple.anvl", "x := ();\n")
put(R + "empty_attr_block.anvl", "x @[] := 1;\n")
put(R + "attr_trailing_comma.anvl", "x @[a=1,] := 1;\n")
put(R + "attr_missing_comma.anvl", "x @[a=1 b=2] := 1;\n")
put(R + "attr_duplicate_key.anvl", "x @[a=1, a=2] := 1;\n")
put(R + "attr_keyword_key.anvl", "x @[include] := 1;\n")
put(R + "attr_unclosed.anvl", "x @[a=1\n := 1;\n")
put(R + "attr_not_a_block.anvl", "x @a=1 := 1;\n")
put(R + "object_duplicate_field.anvl", "x := { a := 1; a := 2; };\n")
put(R + "document_duplicate_name.anvl", "x := 1;\nx := 2;\n")
put(R + "object_non_identifier_key.anvl", 'x := { 1 := 2; };\n')
put(R + "object_missing_assign.anvl", "x := { a 1; };\n")
put(R + "object_colon_instead.anvl", "x := { a : 1; };\n")
put(R + "object_rocket.anvl", "x := { a => 1; };\n")
put(R + "assign_rocket.anvl", "x => 1;\n")
put(R + "assign_equals.anvl", "x = 1;\n")
put(R + "double_assign.anvl", "x := := 1;\n")
put(R + "second_shebang.anvl", "#!aml\n#!aml\nx := 1;\n")
put(R + "shebang_after_statement.anvl", "x := 1;\n#!aml\n")
put(R + "shebang_amp_then_asl.anvl", "#!amp\n#!asl\nx := 1;\n")
put(R + "module_attr_after_statement.anvl", "x := 1;\n@[m=1]\ny := 2;\n")
put(R + "module_attr_duplicate.anvl", "@[a]\n@[a]\nx := 1;\n")
put(R + "module_attr_ok.anvl", "@[a=1]\n@[b=\"s\"]\nx := 1;\n")
put(R + "vars_block_is_keyword.anvl", "vars { a := 1; };\nx := $a;\n")
put(R + "vars_twice_is_keyword.anvl", "vars { a := 1; };\nvars { b := 2; };\n")
put(R + "vars_not_first_is_keyword.anvl", "x := 1;\nvars { a := 1; };\n")
put(R + "varref_missing_target.anvl", "x := $nope;\n")
put(R + "varref_chain.anvl", "a := 1;\nb := $a;\nc := $b;\n")
put(R + "varref_cycle_three.anvl", "a := $c;\nb := $a;\nc := $b;\n")
put(R + "varref_self.anvl", "a := $a;\n")
put(R + "interp_unterminated.anvl", 'x := $"abc{y";\n')
put(R + "braced_ref_unterminated.anvl", "x := ${abc;\n")
put(R + "braced_ref_unterminated_eof.anvl", "x := ${abc")
put(R + "inherit_from_scalar.anvl", "a := 1;\nb : a := { c := 1; };\n")
put(R + "inherit_unknown_base.anvl", "b : zzz := { c := 1; };\n")
put(R + "inherit_cycle.anvl", "a : b := { x := 1; };\nb : a := { y := 1; };\n")
put(R + "inherit_self.anvl", "a : a := { x := 1; };\n")
put(R + "blob_tag_invalid.anvl", "x := @1bad`abc`;\n")
put(R + "blob_unterminated.anvl", "x := `abc;\n")
put(R + "blob_tagged_unterminated.anvl", "x := @t`abc;\n")
put(R + "block_comment_unterminated.anvl", "/* never closed\nx := 1;\n")
put(R + "block_comment_nested_unterminated.anvl", "/* a /* b */ x := 1;\n")
put(R + "keyword_as_name.anvl", "true := 1;\n")
put(R + "keyword_as_value_name.anvl", "x := true_;\ny := nul;\nz := null;\n")
put(R + "asl_dialect_without_asl.anvl", "#!asl\nx := 1;\n")
put(R + "amp_object_not_allowed.amp", "#!amp\nx := { a := 1; };\n")
put(R + "amp_array_of_array.amp", "#!amp\nx := [[1]];\n")
put(R + "amp_tuple_not_allowed.amp", "#!amp\nx := (1, 2);\n")
put(R + "amp_varref_not_allowed.amp", "#!amp\nx := $y;\n")
put(R + "amp_include_not_allowed.amp", '#!amp\ninclude "x.anvl";\ny := 1;\n')
put(R + "amp_import_not_allowed.amp", '#!amp\nimport "m";\ny := 1;\n')
put(R + "import_in_aml.anvl", 'import "m";\nx := 1;\n')
put(R + "stray_closers.anvl", "x := 1;\n}\n")
put(R + "stray_semicolons.anvl", ";;;\n")
put(R + "unexpected_char_at.anvl", "@\n")
put(R + "unexpected_char_hash_bang_mid.anvl", "x := 1; #!aml\n")
put(R + "unexpected_dollar.anvl", "$\n")
put(R + "unexpected_backslash.anvl", "x := \\;\n")
put(R + "freeform_unterminated.anvl", "x := ```abc\n")
put(R + "bare_hex_zz.anvl", "x := 0xZZ;\n")
put(R + "bare_hex_empty.anvl", "x := 0x;\n")
put(R + "bare_hex_1f.anvl", "x := 0x1F;\n")
put(R + "null_bool_values.anvl", "a := null;\nb := true;\nc := false;\nd := TRUE;\ne := Null;\n")
put(R + "empty_statement_name.anvl", ":= 1;\n")
put(R + "name_with_digit_first.anvl", "1x := 1;\n")
put(R + "name_with_dash.anvl", "a-b := 1;\n")
put(R + "name_very_long.anvl", "n" * 5000 + " := 1;\n")
put(R + "string_very_long.anvl", 'x := "' + "s" * 30000 + '";\n')
# the shebang: "#!" and exactly three letters, after optional leading white space; what follows is ordinary content
for name, text in {"minified": "#!amlx := 1;", "leading_ws": " \n\t#!amp\nx := 1;\n", "short": "#!am", "short_nl": "#!am\nx := 1;\n",
                   "upper": "#!AML\nx := 1;\n", "bang_only": "#!\nx := 1;\n", "bang_only_eof": "#!", "unknown": "#!xyz\nx := 1;\n",
                   "four_letters": "#!amlx\ny := 1;\n", "crlf": "#!aml\r\nx := 1;\n", "space_after_bang": "#! aml\nx := 1;\n",
                   "amp_in_anvl_file": "#!amp\nx := 1;\n", "after_comment": "# c\n#!amp\nx := 1;\n", "trailing_text": "#!aml trailing\nx := 1;\n",
                   "asl_short": "#!as", "asl_then_aml": "#!asl\n#!aml\n"}.items():
    put(R + "shebang_%s.anvl" % name, text)
put(R + "shebang_aml_in_amp_file.amp", "#!aml\nx := { a := 1; };\n")
put(R + "amp_no_shebang.amp", "x := 1;\ny := { a := 1; };\n")
put(R + "amp_no_shebang_ok.amp", "x := 1;\ny := [1, 2];\n")
put(R + "many_statements.anvl", "".join("s%d := %d;\n" % (i, i) for i in range(300)))
# BR-2610-anvl-parser-005: duplicate keys, a non-object base, and the specific error codes
G = "errors/"
put(G + "object_duplicate_nested.anvl", "x := {\n  a := 1;\n  b := {\n    c := 1;\n    c := 2;\n  };\n};\n")
put(G + "object_duplicate_block_form.anvl", "x {\n  a := 1;\n  a := 3;\n};\n")
put(G + "object_duplicate_assign_then_block.anvl", "x := { a := 1; a { b := 1; }; };\n")
put(G + "object_duplicate_before_later_error.anvl", "x := { a := 1; a := ; };\n")
put(G + "object_duplicate_third.anvl", "x := { a := 1; b := 1; a := 2; a := 3; };\n")
put(G + "object_duplicate_large.anvl", "x := {\n" + "".join("  f%d := 1;\n" % i for i in range(20)) + "  f3 := 2;\n};\n")
put(G + "object_distinct_large.anvl", "x := {\n" + "".join("  f%d := 1;\n" % i for i in range(40)) + "};\n")
put(G + "object_case_distinct.anvl", "x := { a := 1; A := 2; };\n")
put(G + "object_same_name_two_depths.anvl", "x := { a := { a := 1; }; };\n")
put(G + "object_override_inherited.anvl", "b := { x := 1; };\nd : b := { x := 2; y := 3; };\n")
put(G + "attr_duplicate_flag.anvl", "x @[f, f] := 1;\n")
put(G + "attr_duplicate_multiline.anvl", "x @[\n  a,\n  b,\n  a\n] := 1;\n")
put(G + "attr_duplicate_nested.anvl", "x := { y @[k, k] := 1; };\n")
put(G + "attr_duplicate_flag_then_valued.anvl", "x @[a, a=1] := 1;\n")
put(G + "attr_case_distinct.anvl", "x @[a, A] := 1;\n")
put(G + "module_attr_duplicate_one_block.anvl", "@[a=1, a=2]\nx := 1;\n")
put(G + "module_attr_statement_same_key.anvl", "@[a]\nx @[a] := 1;\n")
for name, base in {"string": '"s"', "array": "[1]", "tuple": "(1, 2)", "blob": "@t`z`", "bool": "true", "null": "null",
                   "bare": "word", "numeric": "1"}.items():
    put(G + "inherit_from_%s.anvl" % name, "a := %s;\nb : a := { c := 1; };\n" % base)
put(G + "inherit_from_varref.anvl", "z := { q := 1; };\na := $z;\nb : a := { c := 1; };\n")
put(G + "inherit_from_scalar_forward.anvl", "b : a := { c := 1; };\na := 1;\n")
put(G + "inherit_from_scalar_nested.anvl", "a := 1;\nx := { b : a := { c := 1; }; };\n")
put(G + "inherit_from_scalar_block_form.anvl", "a := 1;\nb : a { c := 1; };\n")
put(G + "inherit_from_object_chain.anvl", "a := { x := 1; };\nb : a := { y := 2; };\nc : b := { z := 3; };\n")
put(G + "inherit_from_anonymous.anvl", "a { x := 1; };\nb : a := { y := 2; };\n")
put(G + "inherit_scalar_value_after_base.anvl", "a := { x := 1; };\nb : a := 5;\n")
put(G + "shebang_second_other_dialect.anvl", "#!aml\n#!amp\nx := 1;\n")
put(G + "shebang_second_after_statement.anvl", "#!aml\nx := 1;\n#!aml\ny := 2;\n")
put(G + "shebang_one_ok.anvl", "#!aml\nx := 1;\n")
put(G + "module_attr_after_statement_named.anvl", "x := 1;\n@[a] y := 2;\n")
put(G + "module_attr_after_statement_bare.anvl", "x := 1;\n@[a];\n")
put(G + "module_attr_after_statements_indented.anvl", "x := 1;\ny := 2;\n  @[a]\nz := 3;\n")
put(G + "include_after_statement.anvl", 'x := 1;\ninclude "y.anvl";\n')
put(G + "include_after_module_attr.anvl", '@[a]\ninclude "y.anvl";\n')
put(G + "include_after_attr_and_statement.anvl", '@[a]\nx := 1;\ninclude "y.anvl";\n')
put(G + "include_nested_keyword.anvl", 'x := { include "y"; };\n')
put(G + "include_as_first_name.anvl", "include := 1;\n")
put(G + "tuple_empty_element_double.anvl", "x := (1,,2);\n")
put(G + "tuple_empty_element_leading.anvl", "x := (,1);\n")
put(G + "tuple_empty_element_nested.anvl", "x := [(1, 2), (3,,4)];\n")
put(G + "tuple_empty_element_amp.amp", "#!amp\nx := (1,,2);\n")
put(G + "tuple_empty_element.frag", "(1,,2)")
put(G + "tuple_trailing_comma_ok.anvl", "x := (1, 2,);\n")
put(G + "array_empty_element.anvl", "x := [1,,2];\n")
put(G + "object_trailing_comma_assign.anvl", "x := { a := 1, };\n")
put(G + "object_trailing_comma_block.anvl", "x { a := 1, };\n")
put(G + "object_trailing_comma_after_object.anvl", "x := { a := { b := 1; }, };\n")
put(G + "object_trailing_comma_after_semicolon.anvl", "x := { a := 1; ,};\n")
put(G + "object_trailing_comma_next_line.anvl", "x := {\n  a := 1,\n};\n")
put(G + "object_comma_between_fields.anvl", "x := { a := 1, b := 2 };\n")
put(G + "statement_trailing_comma_top.anvl", "x := 1,")
put(G + "object_key_missing_semicolon.anvl", "x := { ; };\n")
put(G + "object_key_missing_assign.anvl", "x := { := 1; };\n")
put(G + "object_key_missing_comma.anvl", "x := { , a := 1; };\n")
put(G + "object_key_missing_after_field.anvl", "x := { a := 1; ; };\n")
put(G + "object_key_quoted.anvl", 'x := { "a" := 1; };\n')
put(G + "object_key_numeric.anvl", "x := { 1 := 1; };\n")
put(G + "object_key_varref.anvl", "x := { $a := 1; };\n")
put(G + "object_key_attribute_block.anvl", "x := { @[a] y := 1; };\n")
put(G + "object_key_numeric_block_form.anvl", "x {\n  7 := 1;\n};\n")
put(G + "top_level_numeric_name.anvl", "1 := 2;\n")
put(G + "top_level_stray_semicolon.anvl", ";\n")
put(G + "attr_empty_with_comment.anvl", "x @[ /* c */ ] := 1;\n")
put(G + "module_attr_empty.anvl", "@[]\nx := 1;\n")
put(G + "attr_invalid_key_digit.anvl", "x @[1a] := 1;\n")
put(G + "attr_invalid_key_after_comma.anvl", "x @[a,] := 1;\n")
put(G + "attr_invalid_key_quoted.anvl", 'x @["k"=1] := 1;\n')
put(G + "module_attr_invalid_key.anvl", "@[a,,b]\nx := 1;\n")
put(G + "attr_hyphen_after_key.anvl", "x @[a-b] := 1;\n")
put(G + "attr_missing_comma_flags.anvl", "x @[a b] := 1;\n")
put(G + "module_attr_missing_comma.anvl", "@[a b]\nx := 1;\n")
put(G + "attr_missing_comma_multiline.anvl", "x @[\n  a\n  b\n] := 1;\n")
put(G + "attr_junk_after_key.anvl", "x @[a;] := 1;\n")
put(G + "attr_not_a_bracket_after_name.anvl", "x @a := 1;\n")
put(G + "attr_not_a_bracket_statement_start.anvl", "@a x := 1;\n")
put(G + "attr_not_a_bracket_nested.anvl", "x := { @a y := 1; };\n")
put(G + "attr_not_a_bracket_after_base.anvl", "b : a @x := { c := 1; };\n")
put(G + "attr_value_array.anvl", "x @[a=[1,2]] := 1;\n")
put(G + "attr_value_tuple.anvl", "x @[a=(1,2)] := 1;\n")
put(G + "attr_value_array_unbalanced.anvl", "x @[a=[1]] := 1;\n")
put(G + "attr_value_object_split.anvl", "x @[a={p := 1;, q}] := 1;\n")
put(G + "module_attr_value_array.anvl", "@[a=[1,2]]\nx := 1;\n")
put(G + "attr_value_array_multiline.anvl", "x @[\n  k,\n  a = [1, 2]\n] := 1;\n")
# BR-2610-anvl-parser-007: an attribute value must be a scalar; one starting with [, ( or { is 2006
# wherever a comma or ] falls
put(G + "attr_value_object_balanced.anvl", "x @[a={1}] := 1;\n")
put(G + "attr_value_tuple_balanced.anvl", "x @[a=(1 2)] := 1;\n")
put(G + "attr_value_object_one_name.anvl", "x @[k={a}] := 1;\n")
put(G + "attr_value_tuple_one_element.anvl", "x @[k=(1)] := 1;\n")
put(G + "attr_value_array_one_element.anvl", "x @[k=[x]] := 1;\n")
put(G + "attr_value_array_empty.anvl", "x @[k=[]] := 1;\n")
put(G + "attr_value_object_empty.anvl", "x @[k={}] := 1;\n")
put(G + "attr_value_tuple_empty.anvl", "x @[k=()] := 1;\n")
put(G + "attr_value_object_then_attribute.anvl", "x @[k={a}, m=1] := 1;\n")
put(G + "attr_value_tuple_after_whitespace.anvl", "x @[k=   (1)] := 1;\n")
put(G + "attr_value_object_nested_statement.anvl", "o := { y @[k={a}] := 1; };\n")
put(G + "module_attr_value_object.anvl", "@[k={a}]\nx := 1;\n")
put(G + "module_attr_value_tuple.anvl", "@[k=(1)]\nx := 1;\n")
put(G + "module_attr_value_after_scalar.anvl", "@[a=1, b={c}]\nx := 1;\n")
put(G + "attr_value_varref_stays_2009.anvl", "x @[k=$v] := 1;\n")
put(G + "attr_value_bare_word_ok.anvl", "x @[status=done] := 1;\n")
put(G + "attr_value_scalars_ok.anvl", "x @[n=5, f=-1.5e3, on=true, off=false, z=null, s=done] := 1;\n")
put(G + "attr_value_bracket_in_bare_word_ok.anvl", "x @[a=b{c}, d=e(f)] := 1;\n")
put(G + "attr_value_quoted_brackets_ok.anvl", 'x @[a="{a}", b="(1)", c="[x]"] := 1;\n')
put(G + "module_attr_value_scalars_ok.anvl", "@[a=done, b=5, c=true]\nx := 1;\n")
# BR-2610-anvl-writer-002 / the owner's 2026-10-30 ruling: any non-object base is 4020; the bare block
# form `ident { }` is an anonymous, read-only block and cannot be a base (4052)
put(G + "inherit_from_object_ok.anvl", "a := { x := 1; };\nb : a := { y := 2; };\n")
put(G + "inherit_from_object_forward_ok.anvl", "b : a := { y := 2; };\na := { x := 1; };\n")
put(G + "inherit_from_anonymous_nested.anvl", "a { x := 1; };\no := { b : a := { y := 2; }; };\n")
put(G + "inherit_from_anonymous_block_form.anvl", "a { x := 1; };\nb : a { y := 2; };\n")
put(G + "inherit_from_anonymous_forward.anvl", "b : a := { y := 2; };\na { x := 1; };\n")
put(G + "inherit_anonymous_varref_unresolved.anvl", "a { x := 1; };\nb := $a;\n")
put(G + "attr_value_blob_ok.anvl", "x @[a=@t`z`] := 1;\n")
put(G + "attr_value_string_brackets_ok.anvl", 'x @[a="[1,2"] := 1;\n')
put(G + "char_control_value.anvl", "x := \x01;\n")
put(G + "char_control_after_name.anvl", "x\x01 := 1;\n")
put(G + "char_control_statement_start.anvl", "\x01x := 1;\n")
put(G + "char_nul_value.anvl", "x := \x00;\n")
put(G + "char_del_value.anvl", "x := \x7f;\n")
put(G + "char_control_object_key.anvl", "x := { \x02 := 1; };\n")
put(G + "value_missing.anvl", "x := ;\n")
put(G + "tabs_then_missing_value.anvl", "x\t:=\t;\n")
put(G + "frag_at_digit_bare.frag", "@2")
# keywords and reserved words are refused as names at any level (owner decision): attribute keys are 4004
for kw in ("include", "import", "vars", "true", "false", "null"):
    put(G + "attr_key_keyword_%s.anvl" % kw, "x @[%s] := 1;\n" % kw)
put(G + "attr_key_keyword_valued.anvl", "x @[import=1, vars=2] := 1;\n")
put(G + "attr_key_keyword_second.anvl", "x @[a, null] := 1;\n")
put(G + "attr_key_keyword_nested.anvl", "x := { y @[false] := 1; };\n")
put(G + "attr_key_keyword_multiline.anvl", "x @[\n  a,\n  include\n] := 1;\n")
put(G + "module_attr_key_keyword.anvl", "@[vars]\nx := 1;\n")
put(G + "module_attr_key_keyword_second_block.anvl", "@[a]\n@[null]\nx := 1;\n")
put(G + "attr_key_keyword_prefix_ok.anvl", "x @[included, trueish, nulls] := 1;\n")
put(G + "attr_key_keyword_case_ok.anvl", "x @[Include, TRUE, Null] := 1;\n")
put(G + "name_keyword_top.anvl", "true := 1;\n")
put(G + "name_keyword_field.anvl", "x := { true := 1; };\n")
put(G + "name_keyword_field_import.anvl", "x := { import := 1; };\n")
put(G + "name_keyword_field_block_form.anvl", "x {\n  vars := 1;\n};\n")
put(G + "name_keyword_field_depth2.anvl", "x := { y := { null := 1; }; };\n")
put(G + "name_keyword_field_in_array.anvl", "x := [ { false := 1; } ];\n")
print("coverage cases written under", out)
