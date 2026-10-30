#!/usr/bin/env python3
"""Seeds conformance/cases/grammar/: hand-picked inputs aimed at the places an independent implementation
drifts - the number grammar, strings and escapes, blobs, attributes, the empty and one-element rules,
statement forms, comments, includes and error positions. Each is a document (.anvl); numbers also
as value fragments (.frag). The output is committed; native's dumps are generated from it.
Usage: gen_grammar.py <repo-root> [--force]
"""
import os, shutil, sys

root = sys.argv[1]
out = os.path.join(root, "conformance", "cases", "grammar")
if os.path.isdir(out) and os.listdir(out):
    if "--force" not in sys.argv: sys.exit("%s exists; pass --force to wipe and regenerate (dumps are removed too)" % out)
    shutil.rmtree(out)
os.makedirs(out, exist_ok=True)

cases = {}
def doc(name, body, shebang=b"#!aml\n"): cases[name + ".anvl"] = shebang + body
def stmt(name, value): doc(name, b"x := " + value + b";\n")

# --- numbers: the spelling decides the kind; valid and invalid forms
numbers = ["0", "-0", "+0", "+1", "1", "-1", "007", "00", "0.0", "-0.0", "0.5", ".5", "5.", "-.5", "1.0", "1.50", "2020", "2020.0",
   "1e3", "1E3", "1e+3", "1e-3", "1E-3", "5e+3", "1e", "1e+", "1e-", "e3", "1.e3", "1.5e", "1.5e3", "1.5E+3", "1e0003", "1e99999",
   "1e-99999", "-1e999", "1e308", "1.7976931348623157e308", "1.7976931348623159e308", "5e-324", "2.4703282292062327e-324",
   "9007199254740991", "9007199254740992", "9007199254740993", "-9007199254740993", "9223372036854775807", "9223372036854775808",
   "-9223372036854775808", "-9223372036854775809", "18446744073709551615", "18446744073709551616",
   "123456789012345678901234567890", "0.1", "0.2", "0.30000000000000004", "3.141592653589793238462643383279", "1_000", "1,000",
   "1 000", "1.2.3", "1..2", "--1", "-+1", "+-1", "- 1", "+ 1", "0x10", "0X10", "0b101", "0o17", "#12D4F", "#FF", "#", "Infinity",
   "-Infinity", "NaN", "nan", "inf", "1f", "1d", "1L", "10n", "1e5.5", "١٢٣", "1.5.5e5", "0e0", "0E0", "-0e0", "0e-0", "00.5",
   "-00.5", "1.", "100000000000000000000.0", "0.000000000000000000001", "123e-5", "-123e-5", "999999999999999e22", "1e22", "1e23"]
for i, n in enumerate(numbers):
    safe = "".join(c if c.isalnum() else "_" for c in n)[:24]
    stmt("num_%03d_%s" % (i, safe), n.encode("utf-8"))
    cases["num_%03d_%s.frag" % (i, safe)] = n.encode("utf-8")
    if i % 4 == 0:  # also in array / tuple / attribute / object positions
        doc("num_%03d_%s_array" % (i, safe), b"x := [" + n.encode() + b", 1];\n")
        doc("num_%03d_%s_attr" % (i, safe), b"x @[k=" + n.encode() + b"] := 1;\n")

# --- strings and escapes
strings = {
  "empty": b'""', "plain": b'"hello"', "esc_n": b'"a\\nb"', "esc_t": b'"a\\tb"', "esc_r": b'"a\\rb"', "esc_backslash": b'"a\\\\b"',
  "esc_quote": b'"a\\"b"', "esc_unknown_x": b'"a\\xb"', "esc_unknown_u": b'"a\\u0041b"', "esc_unknown_0": b'"a\\0b"',
  "esc_unknown_q": b"\"a\\'b\"", "esc_unknown_space": b'"a\\ b"', "esc_trailing_backslash_quote": b'"a\\"',
  "esc_double_backslash_end": b'"a\\\\"', "esc_triple": b'"\\\\\\n"', "raw_newline": b'"a\nb"', "raw_crlf": b'"a\r\nb"', "raw_tab": b'"a\tb"',
  "nul": b'"a\x00b"', "utf8": '"héllo € \U0001f600"'.encode(), "invalid_utf8": b'"a\xff\xfeb"', "lone_continuation": b'"\x80"',
  "overlong": b'"\xc0\xaf"', "bom_inside": b'"\xef\xbb\xbfa"', "hash": b'"#fff"', "braces": b'"{[(;)]}"', "at": b'"@[x]"',
  "backtick": b'"a`b"', "dollar": b'"$name"', "comment_like": b'"// not a comment"', "block_comment_like": b'"/* x */"',
  "long": b'"' + b"abcdefghij" * 300 + b'"', "unterminated": b'"abc', "unterminated_escape": b'"abc\\', "unterminated_nl": b'"abc\n',
  "single_quote": b"'abc'", "adjacent": b'"a" "b"', "interp_dollar": b'$"hello {name}"',
}
for k, v in strings.items(): stmt("str_" + k, v)

# --- blobs
blobs = {
  "untagged": b"`abc`", "tagged": b"@date`2026-07-07`", "empty": b"``", "empty_tagged": b"@t``", "newline": b"`a\nb`", "nul": b"`a\x00b`",
  "bytes_high": b"`\x80\xff\xfe`", "utf8": "`hé`".encode(), "tag_underscore": b"@a_b`x`", "tag_digit_start": b"@1a`x`", "tag_dash": b"@a-b`x`",
  "tag_31": b"@" + b"a" * 31 + b"`x`", "tag_32": b"@" + b"a" * 32 + b"`x`", "tag_empty": b"@`x`", "tag_space": b"@ date`x`", "tag_space2": b"@date `x`",
  "tag_upper": b"@Date`x`", "tag_keyword": b"@null`x`", "tag_dot": b"@a.b`x`", "unterminated": b"`abc", "unterminated_tagged": b"@t`abc",
  "double_tick": b"`a``b`", "backslash": b"`a\\`b`", "backslash_end": b"`a\\`", "big": b"`" + bytes(range(1, 256)) * 4 + b"`", "tag_no_backtick": b"@date",
  "tag_then_string": b'@date"x"', "base64": b"@b64`SGVsbG8gd29ybGQ=`", "crlf": b"`a\r\nb`",
}
for k, v in blobs.items(): stmt("blob_" + k, v)

# --- bare literals, keywords, booleans, null
bare = {"word": b"hello", "dotted": b"a.b.c", "trailing_dot": b"a.", "leading_dot": b".a", "dash": b"a-b", "underscore": b"_a", "digit_start": b"1a",
  "true": b"true", "false": b"false", "null": b"null", "True": b"True", "NULL": b"NULL", "include": b"include", "vars": b"vars", "import": b"import",
  "colon": b"a:b", "slash": b"a/b", "unicode": "héllo".encode(), "star": b"a*", "empty": b"", "varref": b"$other", "varref_braced": b"${other}",
  "varref_space": b"$ other", "varref_missing": b"$nothing", "varref_bad": b"$1", "varref_self": b"$x", "varref_dollar_only": b"$"}
for k, v in bare.items(): stmt("bare_" + k, v)
doc("varref_target", b"a := 1;\nb := $a;\nc := [$a, $b];\nd := { e := $a; };\n")
doc("varref_chain_doubling", b"a0 := [1, 2];\n" + b"".join(b"a%d := [$a%d, $a%d];\n" % (i, i - 1, i - 1) for i in range(1, 40)))  # expanding references would print 2^39 lines
doc("varref_cycle", b"a := $b;\nb := $a;\n")
doc("varref_to_object", b"a := { k := 1; };\nb := $a;\n")
doc("varref_forward", b"b := $a;\na := 2;\n")

# --- arrays, tuples, objects: empty and one-element rules
colls = {
  "array_empty": b"[]", "array_one": b"[1]", "array_two": b"[1, 2]", "array_trailing_comma": b"[1, 2,]", "array_missing_comma": b"[1 2]",
  "array_leading_comma": b"[, 1]", "array_double_comma": b"[1,, 2]", "array_nested": b"[[1, 2], [3]]", "array_mixed": b'[1, "a", true, null, x, `b`, (1, 2), { k := 1; }]',
  "array_unterminated": b"[1, 2", "array_close_only": b"]", "array_newlines": b"[\n1,\n2\n]", "array_comments": b"[1, /* c */ 2, // d\n 3]",
  "tuple_empty": b"()", "tuple_one": b"(1)", "tuple_one_comma": b"(1,)", "tuple_two": b"(1, 2)", "tuple_three": b"(1, 2, 3)", "tuple_trailing": b"(1, 2,)",
  "tuple_nested": b"((1, 2), (3, 4))", "tuple_unterminated": b"(1, 2", "tuple_missing_comma": b"(1 2)",
  "object_empty": b"{}", "object_one": b"{ a := 1; }", "object_two": b"{ a := 1; b := 2; }", "object_nested": b"{ a := { b := { c := 1; }; }; }",
  "object_dup": b"{ a := 1; a := 2; }", "object_no_semicolon": b"{ a := 1 }", "object_trailing_comma": b"{ a := 1, }", "object_unterminated": b"{ a := 1;",
  "object_keyword_field": b"{ include := 1; }", "object_null_field": b"{ null := 1; }", "object_number_field": b"{ 1 := 1; }", "object_string_field": b'{ "a" := 1; }',
  "object_colon": b"{ a: 1; }", "object_attr_field": b"{ a @[k=1] := 1; }", "object_block_field": b"{ a { b := 1; }; }", "object_inherit": b"{ a : b := { c := 1; }; }",
  "deep_array": b"[" * 40 + b"1" + b"]" * 40, "deep_object": b"{ a := " * 20 + b"1;" + b" }" * 20,
}
for k, v in colls.items(): stmt("coll_" + k, v)

# --- attributes
attrs = [
  ("flag", b"x @[a] := 1;\n"), ("kv", b"x @[a=1] := 1;\n"), ("multi", b"x @[a=1, b=\"s\", c, d=true, e=null] := 1;\n"), ("empty", b"x @[] := 1;\n"),
  ("dup", b"x @[a=1, a=2] := 1;\n"), ("missing_comma", b"x @[a=1 b=2] := 1;\n"), ("trailing_comma", b"x @[a=1,] := 1;\n"), ("leading_comma", b"x @[,a=1] := 1;\n"),
  ("range", b"x @[range=1..5] := 1;\n"), ("two_words", b"x @[x=1 2] := 1;\n"), ("keyword_key", b"x @[include] := 1;\n"), ("true_key", b"x @[true=1] := 1;\n"),
  ("digit_key", b"x @[1a=1] := 1;\n"), ("underscore_key", b"x @[_a=1] := 1;\n"), ("dash_key", b"x @[a-b=1] := 1;\n"), ("dot_key", b"x @[a.b=1] := 1;\n"),
  ("string_value", b"x @[a=\"he said \\\"hi\\\"\"] := 1;\n"), ("string_bracket", b"x @[a=\"]\"] := 1;\n"), ("string_comma", b"x @[a=\"1,2\"] := 1;\n"),
  ("blob_value", b"x @[a=`b`] := 1;\n"), ("tagged_blob_value", b"x @[a=@t`b`] := 1;\n"), ("varref_value", b"y := 1;\nx @[a=$y] := 1;\n"), ("array_value", b"x @[a=[1,2]] := 1;\n"),
  ("object_value", b"x @[a={ b := 1; }] := 1;\n"), ("number_value", b"x @[a=-1.5e3] := 1;\n"), ("bad_number", b"x @[a=1e] := 1;\n"), ("bare_dotted", b"x @[a=b.c] := 1;\n"),
  ("whitespace", b"x @[ a = 1 , b = 2 ] := 1;\n"), ("newlines", b"x @[\n a = 1,\n b = 2\n] := 1;\n"), ("comment_inside", b"x @[a=1 /* c */, b] := 1;\n"),
  ("unterminated", b"x @[a=1 := 1;\n"), ("double_block", b"x @[a] @[b] := 1;\n"), ("before_name", b"@[a] x := 1;\n"), ("on_block", b"x @[a] { y := 1; };\n"),
  ("on_inherit", b"b := { k := 1; };\nx : b @[a] := { j := 2; };\n"), ("nul_value", b"x @[a=\"b\x00c\"] := 1;\n"), ("utf8_value", "x @[a=\"hé\"] := 1;\n".encode()),
]
for k, v in attrs: doc("attr_" + k, v)
module_attrs = [
  ("one", b"@[doc]\nx := 1;\n"), ("kv", b"@[a=1]\n@[b=\"s\"]\nx := 1;\n"), ("dup", b"@[a]\n@[a]\nx := 1;\n"), ("after_stmt", b"x := 1;\n@[a]\ny := 2;\n"),
  ("only", b"@[a]\n"), ("multi_in_block", b"@[a, b=1]\nx := 1;\n"), ("semicolon", b"@[a];\nx := 1;\n"),
]
for k, v in module_attrs: doc("modattr_" + k, v)

# --- statements, headers, comments, whitespace
docs = [
  ("empty_body", b""), ("whitespace_only", b"   \n\n  \t\n"), ("comment_only", b"// hi\n"), ("block_comment_only", b"/* hi */"), ("nested_block_comment", b"/* a /* b */ c */ x := 1;\n"),
  ("unterminated_block_comment", b"/* never closed\nx := 1;\n"), ("line_comment_eof", b"x := 1; // c"), ("comment_in_value", b"x := /* c */ 1;\n"), ("comment_between", b"x /* c */ := 1;\n"),
  ("no_semicolon", b"x := 1\n"), ("double_semicolon", b"x := 1;;\n"), ("lone_semicolon", b";\n"), ("no_value", b"x := ;\n"), ("no_assign", b"x 1;\n"), ("colon_equals_space", b"x : = 1;\n"),
  ("equals_only", b"x = 1;\n"), ("rocket", b"x => 1;\n"), ("crlf", b"x := 1;\r\ny := 2;\r\n"), ("cr_only", b"x := 1;\ry := 2;\r"), ("tabs", b"x\t:=\t1;\n"), ("bom", b"\xef\xbb\xbfx := 1;\n"),
  ("nul_in_body", b"x := 1;\x00y := 2;\n"), ("nul_in_comment", b"// a\x00b\nx := 1;\n"), ("high_bytes_ident", b"\xc3\xa9 := 1;\n"), ("unicode_ident", "héllo := 1;\n".encode()),
  ("repeat_names", b"x := 1;\nx := 2;\n"), ("order", b"c := 1;\na := 2;\nb := 3;\n"), ("keyword_ident_include", b"include := 1;\n"), ("keyword_ident_true", b"true := 1;\n"),
  ("keyword_ident_null", b"null := 1;\n"), ("keyword_ident_vars", b"vars := 1;\n"), ("keyword_ident_import", b"import := 1;\n"), ("ident_digit", b"1x := 1;\n"), ("ident_underscore", b"_x := 1;\n"),
  ("ident_dash", b"a-b := 1;\n"), ("ident_dot", b"a.b := 1;\n"), ("ident_long", b"a" * 300 + b" := 1;\n"), ("ident_case", b"X := 1;\nx := 2;\n"),
  ("block", b"ns { a := 1; b := 2; };\n"), ("block_empty", b"ns { };\n"), ("block_dup_anon", b"{ a := 1; };\n{ b := 2; };\n"), ("block_nested", b"ns { in { a := 1; }; };\n"),
  ("block_no_semicolon", b"ns { a := 1; }\n"), ("block_inherit", b"b := { k := 1; };\nx : b { j := 2; };\n"), ("inherit", b"b := { k := 1; };\nx : b := { j := 2; };\n"),
  ("inherit_missing", b"x : nothing := { j := 2; };\n"), ("inherit_scalar", b"b := 1;\nx : b := { j := 2; };\n"), ("inherit_cycle", b"a : b := { x := 1; };\nb : a := { y := 2; };\n"),
  ("inherit_override", b"b := { k := 1; m := 2; };\nx : b := { k := 9; };\n"), ("inherit_chain", b"a := { p := 1; };\nb : a := { q := 2; };\nc : b := { r := 3; };\n"),
  ("inherit_to_scalar", b"b := { k := 1; };\nx : b := 5;\n"), ("inherit_anon", b"{ k := 1; };\nx : := { j := 2; };\n"),
  ("dup_top", b"x := 1;\nx := 2;\n"), ("vars_block", b"vars { a := 1; };\nx := $a;\n"), ("import_stmt", b"import \"x\";\n"), ("function_decl", b"f (a, b) => { return a; };\n"),
  ("shebang_dup", b"#!aml\nx := 1;\n"), ("shebang_late", b"x := 1;\n#!aml\n"), ("shebang_bad_dialect", None), ("long_line", b"x := \"" + b"a" * 4000 + b"\";\n"),
]
for k, v in docs:
    if v is not None: doc("doc_" + k, v)
cases["doc_no_shebang.anvl"] = b"x := 1;\n"
cases["doc_empty_file.anvl"] = b""
cases["doc_amp_array_nested.amp"] = b"#!amp\nx := [[1], 2];\n"
cases["doc_amp_object.amp"] = b"#!amp\nx := { a := 1; };\n"
cases["doc_amp_tuple.amp"] = b"#!amp\nx := (1, 2);\n"
cases["doc_amp_scalars.amp"] = b"#!amp\na := 1;\nb := \"s\";\nc := `x`;\nd := @t`y`;\ne := [1, \"a\", true];\nf := null;\ng := word;\n"
cases["doc_amp_ref.amp"] = b"#!amp\na := 1;\nb := $a;\n"
cases["doc_amp_attr.amp"] = b"#!amp\nx @[a=1] := 1;\n"
cases["doc_amp_block.amp"] = b"#!amp\nns { a := 1; };\n"
cases["doc_amp_include.amp"] = b"#!amp\ninclude \"x.anvl\";\n"
cases["doc_aml_ext.aml"] = b"#!aml\nx := 1;\n"
cases["doc_amp_ext_no_shebang.amp"] = b"x := 1;\n"
cases["doc_amp_ext_aml_shebang.amp"] = b"#!aml\nx := 1;\n"
cases["doc_aml_ext_amp_shebang.aml"] = b"#!amp\nx := 1;\n"
for d in [b"", b" ", b"aml ", b"AML", b"xyz", b"asl", b"aml\r", b"aml extra", b"am", b"amlx", b"amp"]:
    n = "shebang_" + "".join(c if c.isalnum() else "_%02x" % ord(c) for c in d.decode())
    cases["doc_" + (n if d else "shebang_empty") + ".anvl"] = b"#!" + d + b"\nx := 1;\n"
cases["doc_shebang_no_newline.anvl"] = b"#!aml"
cases["doc_shebang_space_before.anvl"] = b" #!aml\nx := 1;\n"
cases["doc_shebang_blank_lines.anvl"] = b"\n\n#!aml\nx := 1;\n"
cases["doc_shebang_comment_first.anvl"] = b"// c\n#!aml\nx := 1;\n"
cases["doc_shebang_crlf.anvl"] = b"#!aml\r\nx := 1;\r\n"

# --- includes (resolved relative to the including file)
# (including a directory or an empty path is left out on purpose: BR-2610-anvl-001, an invalid malloc size in files_load)
cases["inc_target.anvl"] = b"#!aml\nshared := 1;\ngroup := { k := 2; };\n"
cases["inc_one.anvl"] = b"#!aml\ninclude \"inc_target.anvl\";\ny := $shared;\n"
cases["inc_two_same.anvl"] = b"#!aml\ninclude \"inc_target.anvl\";\ninclude \"inc_target.anvl\";\n"
cases["inc_chain_mid.anvl"] = b"#!aml\ninclude \"inc_target.anvl\";\nmid := 1;\n"
cases["inc_chain.anvl"] = b"#!aml\ninclude \"inc_chain_mid.anvl\";\ntop := $mid;\n"
cases["inc_missing.anvl"] = b"#!aml\ninclude \"nothing_here.anvl\";\n"
cases["inc_self.anvl"] = b"#!aml\ninclude \"inc_self.anvl\";\n"
cases["inc_cycle_a.anvl"] = b"#!aml\ninclude \"inc_cycle_b.anvl\";\n"
cases["inc_cycle_b.anvl"] = b"#!aml\ninclude \"inc_cycle_a.anvl\";\n"
cases["inc_after_stmt.anvl"] = b"#!aml\nx := 1;\ninclude \"inc_target.anvl\";\n"
cases["inc_no_quotes.anvl"] = b"#!aml\ninclude inc_target.anvl;\n"
cases["inc_no_semicolon.anvl"] = b"#!aml\ninclude \"inc_target.anvl\"\n"
cases["inc_dup_name.anvl"] = b"#!aml\ninclude \"inc_target.anvl\";\nshared := 5;\n"
cases["inc_with_attr.anvl"] = b"#!aml\n@[doc]\ninclude \"inc_target.anvl\";\n"
cases["inc_in_object.anvl"] = b"#!aml\nx := { include \"inc_target.anvl\"; };\n"
cases["inc_subdir.anvl"] = b"#!aml\ninclude \"inc_sub/inner.anvl\";\n"
cases["inc_sub/inner.anvl"] = b"#!aml\ninclude \"../inc_target.anvl\";\ninner := 1;\n"
cases["inc_amp.amp"] = b"#!amp\ninclude \"inc_target.anvl\";\n"
cases["inc_amp_target.amp"] = b"#!amp\na := 1;\n"
cases["inc_aml_of_amp.anvl"] = b"#!aml\ninclude \"inc_amp_target.amp\";\n"
cases["inc_error_in_child_target.anvl"] = b"#!aml\nbad := ;\n"
cases["inc_error_in_child.anvl"] = b"#!aml\ninclude \"inc_error_in_child_target.anvl\";\n"

for name, data in cases.items():
    p = os.path.join(out, name)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "wb").write(data)
print("wrote %d inputs into %s" % (len(cases), out))
