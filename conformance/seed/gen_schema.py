#!/usr/bin/env python3
"""Generates the SCRIPTS of conformance/cases/schema/ (FR-2610-anvl-001).

Each case is a schema script (conformance/FORMAT.md, "anvil-schema-script 1"): it carries its own files (types,
schema and data documents) and the steps that load a type registry, load a schema and validate values against it.
The generator writes scripts only. The expected runs (.sscript.dump) belong to native: they are written by
`make update-conformance` (anvil-dump --schema-script) and checked by `make check-conformance`. There is deliberately
no model of the schema or the type registry in here: the dump is the specification.

Deterministic. Usage:
  gen_schema.py <repo-root> [--force]   write the scripts (refuses if cases/schema exists, unless --force)
--force rewrites the scripts and removes scripts that are no longer generated; it never touches a .dump.
"""
import os, re, sys

args = [a for a in sys.argv[1:] if a != "--force"]
force = "--force" in sys.argv[1:]
root = args[0]
out_dir = os.path.join(root, "conformance", "cases", "schema")
if os.path.exists(out_dir) and not force:
    sys.exit("cases/schema exists; pass --force to rewrite the scripts")


def q(b):
    if isinstance(b, str):
        b = b.encode("utf-8")
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


cases = []  # (area, name, text)


def fileblock(name, content):
    """A file step: the block form when the content is whole lines of printable text, else the quoted form."""
    raw = content.encode("utf-8") if isinstance(content, str) else content
    if raw.endswith(b"\n") and b"\x00" not in raw and all(b"\r" not in ln for ln in raw.split(b"\n")) \
            and all(ln.decode("utf-8", "replace") .isprintable() or ln == b"" or b"\t" in ln for ln in raw.split(b"\n")) \
            and not (isinstance(content, str) and any(ord(c) > 0x7e for c in content)):
        lines = raw.decode("ascii").split("\n")[:-1]
        return ["file %s" % q(name)] + ["|" + ln for ln in lines]
    return ["file %s %s" % (q(name), q(raw))]


def case(area, name, files, steps, desc=None):
    L = ["anvil-schema-script 1"]
    if desc:
        L.append("# " + desc)
    for fname, content in files:
        L += fileblock(fname, content)
    L += steps
    cases.append((area, name, "\n".join(L) + "\n"))


def ident(s):
    return "".join(c if c.isalnum() else "_" for c in s).strip("_").lower() or "x"


SCHEMA = "#!aml\n@[schema]\n"
TYPES = "#!aml\n@[types]\n"


def vv(s, text, *opts):
    return "validate_value %s %s%s" % (s, q(text), "".join(" " + o for o in opts))


def rec(**kw):
    return "{ " + " ".join("%s := %s;" % (k, v) for k, v in kw.items()) + " }"


# --------------------------------------------------------------------------------------------------------------
# registry: type definitions, every kind, every constraint, every diagnostic
# --------------------------------------------------------------------------------------------------------------
R = "registry"
KINDS = ["Numeric", "String", "Bool", "Object", "Tuple", "Array", "enum", "Blob"]
for k in KINDS:
    body = "type := %s;" % k + (" values := [a, b];" if k == "enum" else "")
    case(R, "kind_" + k.lower(), [("t.anvl", TYPES + "T := { %s };\n" % body)],
         ['registry r load "t.anvl"', 'type find r "T"', 'type resolve r "T"'])

# a type body, find it: (name, body)
TYPE_BODIES = [
    ("size_zero", "type := String; size := 0;"), ("size_five", "type := String; size := 5;"),
    ("size_big", "type := String; size := 9223372036854775807;"),
    ("size_overflow", "type := String; size := 9223372036854775808;"),
    ("size_negative", "type := String; size := -1;"), ("size_fraction", "type := String; size := 1.5;"),
    ("size_exponent", "type := String; size := 1e2;"), ("size_string", 'type := String; size := "5";'),
    ("size_bare", "type := String; size := five;"), ("size_bool", "type := String; size := true;"),
    ("size_null", "type := String; size := null;"), ("size_array", "type := String; size := [5];"),
    ("size_leading_zeros", "type := String; size := 007;"), ("size_negative_zero", "type := String; size := -0;"),
    ("size_on_numeric", "type := Numeric; size := 3;"),
    ("format_date", "type := String; format := iso8601_date;"), ("format_datetime", "type := String; format := iso8601_datetime;"),
    ("format_uuid", "type := String; format := uuid;"), ("format_quoted", 'type := String; format := "uuid";'),
    ("format_unknown", "type := String; format := email;"), ("format_empty", 'type := String; format := "";'),
    ("format_case", "type := String; format := UUID;"), ("format_number", "type := String; format := 5;"),
    ("format_array", "type := String; format := [uuid];"), ("format_nul", 'type := String; format := "uu\x00id";'),
    ("format_on_numeric", "type := Numeric; format := uuid;"),
    ("values_three", "type := enum; values := [a, b, c];"), ("values_one", "type := enum; values := [a];"),
    ("values_strings", 'type := enum; values := ["x y", "", "q\\"r"];'),
    ("values_numbers", "type := enum; values := [1, 2.5, true, null];"),
    ("values_mixed", 'type := enum; values := [a, "b", 3];'), ("values_duplicates", "type := enum; values := [a, a];"),
    ("values_nul", 'type := enum; values := ["a\x00b", a];'), ("values_unicode", 'type := enum; values := ["héllo", "日本"];'),
    ("values_nested", "type := enum; values := [[1], {a := 1;}];"), ("values_tuple", "type := enum; values := (a, b);"),
    ("values_not_array", "type := enum; values := 5;"), ("values_string", 'type := enum; values := "a";'),
    ("values_bare", "type := enum; values := a;"), ("values_object", "type := enum; values := { a := 1; };"),
    ("values_missing", "type := enum;"), ("values_on_string", "type := String; values := [a, b];"),
    ("values_blob", "type := enum; values := [`one`, @t`two`];"),
    ("type_missing", "size := 3;"), ("type_unknown", "type := Nope;"), ("type_lower", "type := numeric;"),
    ("type_upper_enum", "type := Enum;"), ("type_quoted", 'type := "Numeric";'), ("type_number", "type := 5;"),
    ("type_array", "type := [Numeric];"), ("type_object", "type := { a := 1; };"), ("type_null", "type := null;"),
    ("type_types_ref", "type := types.Other;"), ("type_nul", 'type := "Num\x00eric";'), ("type_empty", 'type := "";'),
    ("type_bool", "type := true;"), ("type_blob", "type := `Numeric`;"),
    ("integer_true", "type := Numeric; integer := true;"), ("integer_bad", "type := Numeric; integer := maybe;"),
    ("integer_number", "type := Numeric; integer := 1;"), ("integer_nul", 'type := Numeric; integer := "tr\x00ue";'),
    ("nullable_true", "type := String; nullable := true;"), ("nullable_bad", "type := String; nullable := 2;"),
    ("tag_ident", "type := Blob; tag := png;"), ("tag_string", 'type := Blob; tag := "png";'),
    ("tag_bad", "type := Blob; tag := 5;"), ("tag_array", "type := Blob; tag := [a];"),
    ("fields_ok", "type := Object; fields := { a := { type := String; }; };"),
    ("fields_bad", "type := Object; fields := 5;"), ("fields_array", "type := Object; fields := [1];"),
    ("extra_keys", "type := String; pooled := true; mask := 3; size := 4;"),
    ("repeat_constraint", "type := String;"),
    ("all_bad", "type := Numeric; size := x; min := y; max := z; integer := 1; nullable := 2;"),
    ("min_max_conflict", "type := Numeric; min := 10; max := 1;"),
]
for n, body in TYPE_BODIES:
    case(R, "def_" + n, [("t.anvl", TYPES + "T := { %s };\nAfter := { type := Bool; };\n" % body)],
         ['registry r load "t.anvl"', 'type find r "T"', 'type find r "After"'])

NUM_BOUNDS = ["0", "-0", "-0.0", "1", "-1", "0.5", "-0.5", "1.0", "1e3", "1E3", "1e+3", "1e-3", "1.5e2", "007", "00.50",
              "9007199254740992", "9007199254740993", "9223372036854775807", "9223372036854775808", "-9223372036854775808",
              "-9223372036854775809", "18446744073709551615", "1e308", "1.7976931348623157e308", "1.7976931348623159e308",
              "1e999", "-1e999", "5e-324", "1e-999", "2.4703282292062327e-324", "123456789012345678901234567890",
              "0.1", "0.30000000000000004", "4.9e-324", "1e+0000000000000000000308",
              "1.", "1e", ".5", "+1", "abc", '"5"', "true", "null", "[1]", "(1)", "{a:=1;}", "`1`", "NaN", "Infinity", "0x10", "1_000", "-"]
for i, v in enumerate(NUM_BOUNDS):
    inner = v if not v.startswith("{") else "{ a := 1; }"
    case(R, "bound_%02d" % i, [("t.anvl", TYPES + "Lo := { type := Numeric; min := %s; };\nHi := { type := Numeric; max := %s; };\nBoth := { type := Numeric; min := %s; max := %s; };\n" % (inner, inner, inner, inner))],
         ['registry r load "t.anvl"', 'type find r "Lo"', 'type find r "Hi"', 'type find r "Both"'], desc="bound " + v)

# loads: where types come from
case(R, "load_not_types", [("a.anvl", "#!aml\nx := 1;\n")], ['registry r load "a.anvl"', 'type find r "x"', 'registry s includes "a.anvl"', 'type find s "x"'])
case(R, "load_missing", [], ['registry r load "nope.anvl"', 'registry s includes "nope.anvl"', 'type find r "T"'])
case(R, "load_syntax_error", [("a.anvl", TYPES + "T := { type := ;\n")], ['registry r load "a.anvl"', 'registry s includes "a.anvl"', 'type find r "T"'])
case(R, "load_empty_doc", [("a.anvl", "")], ['registry r load "a.anvl"', 'registry s includes "a.anvl"'])
case(R, "load_types_only_attr", [("a.anvl", "#!aml\n@[types]\n")], ['registry r load "a.anvl"'])
case(R, "load_types_with_other_attrs", [("a.anvl", "#!aml\n@[types, version=2, note]\nT := { type := Bool; };\n")], ['registry r load "a.anvl"', 'type find r "T"'])
case(R, "load_schema_doc_as_types", [("a.anvl", SCHEMA + "f := { type := String; };\n")], ['registry r load "a.anvl"', 'type find r "f"'])
case(R, "load_non_object_statements", [("a.anvl", TYPES + "X := 5;\nY := [1, 2];\nZ := \"s\";\nT := { type := Bool; };\nW := (1, 2);\nV := null;\n")],
     ['registry r load "a.anvl"', 'type find r "X"', 'type find r "Y"', 'type find r "T"', 'type find r "V"'])
case(R, "load_block_statements", [("a.anvl", TYPES + "T { type := Bool; };\nU := { type := String; };\n")], ['registry r load "a.anvl"', 'type find r "T"', 'type find r "U"'])
case(R, "load_statement_attrs", [("a.anvl", TYPES + "@[doc=\"x\"] T := { type := Bool; };\n")], ['registry r load "a.anvl"', 'type find r "T"'])
case(R, "load_many_types", [("a.anvl", TYPES + "".join("T%d := { type := Numeric; min := %d; };\n" % (i, i) for i in range(40)))],
     ['registry r load "a.anvl"'] + ['type find r "T%d"' % i for i in (0, 1, 17, 39)] + ['type find r "T40"'])
case(R, "load_duplicate_in_file", [("a.anvl", TYPES + "T := { type := String; };\nT := { type := Numeric; };\n")], ['registry r load "a.anvl"', 'type find r "T"'])
case(R, "load_dup_with_bad_first", [("a.anvl", TYPES + "T := { type := Nope; };\nT := { type := Numeric; };\n")], ['registry r load "a.anvl"', 'type find r "T"'])
case(R, "load_bases_between_types", [("a.anvl", TYPES + "Base := { type := String; size := 3; };\nT : Base := { format := uuid; };\n")],
     ['registry r load "a.anvl"', 'type find r "Base"', 'type find r "T"'])
case(R, "load_base_not_object", [("a.anvl", TYPES + "Base := 5;\nT : Base := { type := String; };\n")], ['registry r load "a.anvl"', 'type find r "T"'])
case(R, "load_base_missing", [("a.anvl", TYPES + "T : Nope := { type := String; };\n")], ['registry r load "a.anvl"', 'type find r "T"'])
case(R, "load_base_cycle", [("a.anvl", TYPES + "A : B := { type := String; };\nB : A := { type := String; };\n")], ['registry r load "a.anvl"', 'type find r "A"'])
case(R, "load_variable_reference", [("a.anvl", TYPES + "Base := { type := String; };\nT := { type := $Base; };\n")], ['registry r load "a.anvl"', 'type find r "T"'])
case(R, "load_names_unicode", [("a.anvl", TYPES + "Héllo := { type := String; };\n")], ['registry r load "a.anvl"', 'type find r "Héllo"', 'type find r "Hello"'])
case(R, "load_name_long", [("a.anvl", TYPES + "%s := { type := String; };\n" % ("N" * 300))], ['registry r load "a.anvl"', 'type find r %s' % q("N" * 300), 'type find r %s' % q("N" * 299)])
case(R, "load_name_case", [("a.anvl", TYPES + "Foo := { type := String; };\nfoo := { type := Numeric; };\n")], ['registry r load "a.anvl"', 'type find r "Foo"', 'type find r "foo"', 'type find r "FOO"'])
case(R, "find_empty_name", [("a.anvl", TYPES + "T := { type := String; };\n")], ['registry r load "a.anvl"', 'type find r ""', 'type resolve r ""', 'type find r "types.T"', 'type find r " T"'])
case(R, "find_with_nul_free_names", [("a.anvl", TYPES + "T := { type := String; };\n")], ['registry r load "a.anvl"', 'type find r "T "', 'type find r "t"', 'type find r "T\\n"'])
case(R, "constraints_together", [("a.anvl", TYPES + "T := { type := Numeric; size := 8; min := -1.5; max := 1e3; integer := true; nullable := true; };\n")], ['registry r load "a.anvl"', 'type find r "T"'])
case(R, "enum_in_numeric", [("a.anvl", TYPES + "T := { type := Numeric; values := [a]; };\n")], ['registry r load "a.anvl"', 'type find r "T"'])
case(R, "dispose_source_independent", [("a.anvl", TYPES + "T := { type := String; size := 4; };\n")], ['registry r load "a.anvl"', 'registry s includes "a.anvl"', 'type find r "T"', 'type find s "T"'],
     desc="the source documents are disposed before the registries are read")

# includes
INC = "includes"
T1 = TYPES + "A := { type := String; size := 1; };\n"
case(INC, "chain", [("main.anvl", '#!aml\ninclude "b.anvl";\n'), ("b.anvl", '#!aml\ninclude "c.anvl";\n@[types]\nB := { type := Bool; };\n'), ("c.anvl", T1)],
     ['registry r includes "main.anvl"', 'type find r "A"', 'type find r "B"'])
case(INC, "walk_through_non_types", [("main.anvl", '#!aml\ninclude "mid.anvl";\n'), ("mid.anvl", '#!aml\ninclude "t.anvl";\nx := 1;\n'), ("t.anvl", T1)],
     ['registry r includes "main.anvl"', 'type find r "A"', 'type find r "x"'])
case(INC, "two_includes", [("main.anvl", '#!aml\ninclude "a.anvl";\ninclude "b.anvl";\n'), ("a.anvl", T1), ("b.anvl", TYPES + "B := { type := Bool; };\n")],
     ['registry r includes "main.anvl"', 'type find r "A"', 'type find r "B"'])
case(INC, "same_file_twice", [("main.anvl", '#!aml\ninclude "a.anvl";\ninclude "m2.anvl";\n'), ("m2.anvl", '#!aml\ninclude "a.anvl";\n'), ("a.anvl", T1)],
     ['registry r includes "main.anvl"', 'type find r "A"'])
case(INC, "collision_two_files", [("main.anvl", '#!aml\ninclude "a.anvl";\ninclude "b.anvl";\n'), ("a.anvl", T1), ("b.anvl", TYPES + "A := { type := Numeric; };\n")],
     ['registry r includes "main.anvl"', 'type find r "A"'])
case(INC, "collision_override_chain", [("main.anvl", '#!aml\ninclude "a.anvl";\n'), ("a.anvl", '#!aml\ninclude "b.anvl";\n@[types]\nA := { type := Numeric; };\n'), ("b.anvl", T1)],
     ['registry r includes "main.anvl"', 'type find r "A"'])
case(INC, "root_has_types", [("main.anvl", '#!aml\ninclude "a.anvl";\n@[types]\nM := { type := Bool; };\n'), ("a.anvl", T1)],
     ['registry r includes "main.anvl"', 'type find r "A"', 'type find r "M"', 'registry s load "main.anvl"', 'type find s "M"', 'type find s "A"'])
case(INC, "no_includes", [("main.anvl", "#!aml\nx := 1;\n")], ['registry r includes "main.anvl"', 'type find r "x"'])
case(INC, "missing_include", [("main.anvl", '#!aml\ninclude "gone.anvl";\n')], ['registry r includes "main.anvl"'])
case(INC, "include_cycle", [("d/a.anvl", '#!aml\ninclude "b.anvl";\n@[types]\nA := { type := Bool; };\n'), ("d/b.anvl", '#!aml\ninclude "a.anvl";\n@[types]\nB := { type := Bool; };\n')],
     ['registry r includes "d/a.anvl"', 'type find r "A"', 'type find r "B"'], desc="the roots carry a directory part: a bare root name misses the cycle (BR-2610-anvl-004)")
case(INC, "self_include", [("d/a.anvl", '#!aml\ninclude "a.anvl";\n@[types]\nA := { type := Bool; };\n')], ['registry r includes "d/a.anvl"', 'type find r "A"'], desc="a directory part in the root name: see BR-2610-anvl-004")
case(INC, "subdirectories", [("main.anvl", '#!aml\ninclude "lib/a.anvl";\n'), ("lib/a.anvl", '#!aml\ninclude "sub/b.anvl";\n@[types]\nA := { type := Bool; };\nA2 := { type := Bool; };\n'), ("lib/sub/b.anvl", TYPES + "B := { type := Bool; };\nA2 := { type := String; };\n")],
     ['registry r includes "main.anvl"', 'type find r "A"', 'type find r "A2"', 'type find r "B"'])
case(INC, "parent_include", [("lib/a.anvl", '#!aml\ninclude "../t.anvl";\n'), ("t.anvl", T1)], ['registry r includes "lib/a.anvl"', 'type find r "A"'])
case(INC, "diagnostics_from_deep_file", [("main.anvl", '#!aml\ninclude "a.anvl";\n'), ("a.anvl", TYPES + "Bad := { type := Nope; };\nGood := { type := String; size := x; };\n")],
     ['registry r includes "main.anvl"', 'type find r "Bad"', 'type find r "Good"'])
case(INC, "attributes_on_include_doc", [("main.anvl", '#!aml\n@[types]\ninclude "a.anvl";\n'), ("a.anvl", T1)], ['registry r includes "main.anvl"', 'type find r "A"'])
case(INC, "load_vs_includes_on_types_doc", [("a.anvl", T1)], ['registry r load "a.anvl"', 'registry s includes "a.anvl"', 'type find r "A"', 'type find s "A"'])
case(INC, "include_of_dir_name", [("main.anvl", '#!aml\ninclude "lib";\n'), ("lib/a.anvl", T1)], ['registry r includes "main.anvl"'])
case(INC, "wide_fanout", [("main.anvl", "#!aml\n" + "".join('include "f%d.anvl";\n' % i for i in range(12)))] + [("f%d.anvl" % i, TYPES + "F%d := { type := Numeric; min := %d; };\n" % (i, i)) for i in range(12)],
     ['registry r includes "main.anvl"'] + ['type find r "F%d"' % i for i in (0, 5, 11)])

# resolve
RS = "resolve"
for nm in ["Numeric", "String", "Bool", "Object", "Tuple", "Array", "Blob", "enum"]:
    case(RS, "native_" + nm.lower(), [], ['type resolve NULL "%s"' % nm, 'type find NULL "%s"' % nm])
for _i, nm in enumerate(["numeric", "NUMERIC", "Enum", "ENUM", "string", "Integer", "Int32", "Float", "Any", "", "types.Numeric", "Numeric ", "Null", "Date", "Boolean", "bool"]):
    case(RS, "other_%02d_" % _i + ident(nm or "empty"), [], ['type resolve NULL %s' % q(nm)])
case(RS, "custom_via_registry", [("t.anvl", TYPES + "VIN := { type := String; size := 17; };\n")], ['registry r load "t.anvl"', 'type resolve r "VIN"', 'type resolve NULL "VIN"', 'type resolve r "Numeric"'])
case(RS, "custom_shadowing_native", [("t.anvl", TYPES + "String := { type := Numeric; };\nNumeric := { type := String; size := 2; };\n")],
     ['registry r load "t.anvl"', 'type resolve r "String"', 'type find r "String"', 'type resolve r "Numeric"', 'type find r "Numeric"'])
case(RS, "resolve_missing", [("t.anvl", TYPES + "VIN := { type := String; };\n")], ['registry r load "t.anvl"', 'type resolve r "Nope"'])
case(RS, "null_registry_after_failed_load", [], ['registry r load "gone.anvl"', 'type resolve r "Bool"', 'type resolve r "VIN"', 'type find r "VIN"'])
case(RS, "dropped_definitions_do_not_resolve", [("t.anvl", TYPES + "Bad := { type := Nope; };\nNoType := { size := 1; };\n")],
     ['registry r load "t.anvl"', 'type resolve r "Bad"', 'type resolve r "NoType"'])
case(RS, "native_has_no_constraints", [], ['type resolve NULL "String"', 'type resolve NULL "Numeric"', 'type resolve NULL "enum"'])

# --------------------------------------------------------------------------------------------------------------
# schema loading: diagnostics and introspection
# --------------------------------------------------------------------------------------------------------------
D = "diagnostics"
RULE_BODIES = [
    ("type_native_lower", "type := string;"), ("type_unknown", "type := Nope;"), ("type_types_missing", "type := types.Nope;"),
    ("type_types_typo", "type := types.Prority;"), ("type_quoted", 'type := "String";'), ("type_number", "type := 5;"),
    ("type_array", "type := [String];"), ("type_object", "type := { a := 1; };"), ("type_null", "type := null;"),
    ("type_bool", "type := true;"), ("type_empty", 'type := "";'), ("type_nul", 'type := "Str\x00ing";'),
    ("type_enum_no_values", "type := enum;"), ("type_types_prefix_only", "type := types.;"), ("type_types_native", "type := types.String;"),
    ("type_dotted", "type := a.b;"),
    ("required_true", "type := String; required := true;"), ("required_false", "type := String; required := false;"),
    ("required_bad", "type := String; required := maybe;"), ("required_number", "type := String; required := 1;"),
    ("required_string", 'type := String; required := "true";'), ("required_null", "type := String; required := null;"),
    ("required_nul", 'type := String; required := "tr\x00ue";'), ("required_array", "type := String; required := [true];"),
    ("nullable_bad", "type := String; nullable := 1;"), ("nullable_string", 'type := String; nullable := "true";'),
    ("integer_bad", "type := Numeric; integer := yes;"), ("integer_nul", 'type := Numeric; integer := "fa\x00lse";'),
    ("size_ok", "type := String; size := 5;"), ("size_negative", "type := String; size := -1;"), ("size_fraction", "type := String; size := 1.5;"),
    ("size_string", 'type := String; size := "5";'), ("size_overflow", "type := String; size := 9223372036854775808;"),
    ("size_exp", "type := String; size := 1e2;"), ("size_bare", "type := String; size := x;"), ("size_null", "type := String; size := null;"),
    ("min_bad", "type := Numeric; min := x;"), ("max_bad", "type := Numeric; max := x;"), ("min_string", 'type := Numeric; min := "1";'),
    ("min_overflow", "type := Numeric; min := 1e999;"), ("max_overflow_int", "type := Numeric; max := 9223372036854775808;"),
    ("min_gt_max", "type := Numeric; min := 5; max := 1;"), ("min_eq_max", "type := Numeric; min := 5; max := 5;"),
    ("min_gt_max_float", "type := Numeric; min := 0.5; max := 0.25;"), ("min_gt_max_exp", "type := Numeric; min := 1e3; max := 999;"),
    ("min_gt_max_big", "type := Numeric; min := 9223372036854775807; max := -9223372036854775808;"),
    ("min_gt_max_close", "type := Numeric; min := 9007199254740993; max := 9007199254740992;"),
    ("min_bad_and_max", "type := Numeric; min := x; max := 5;"), ("min_gt_max_on_string", "type := String; min := 5; max := 1;"),
    ("values_ok", "type := enum; values := [a, b];"), ("values_not_array", "type := enum; values := 5;"), ("values_string", 'type := enum; values := "a";'),
    ("values_object", "type := enum; values := { a := 1; };"), ("values_tuple", "type := enum; values := (a, b);"),
    ("values_on_string", "type := String; values := [a, b];"), ("values_nested", "type := enum; values := [[1], {a:=1;}];"),
    ("format_ok", "type := String; format := uuid;"), ("format_unknown", "type := String; format := email;"), ("format_quoted_unknown", 'type := String; format := "x y";'),
    ("format_number", "type := String; format := 5;"), ("format_nul", 'type := String; format := "uu\x00id";'), ("format_array", "type := String; format := [uuid];"),
    ("format_on_numeric", "type := Numeric; format := uuid;"), ("format_case", "type := String; format := Uuid;"),
    ("tag_ok", "type := Blob; tag := png;"), ("tag_number", "type := Blob; tag := 5;"), ("tag_array", "type := Blob; tag := [a];"),
    ("tag_nul", 'type := Blob; tag := "pn\x00g";'), ("tag_on_string", "type := String; tag := png;"), ("tag_empty", 'type := Blob; tag := "";'),
    ("min_items_ok", "type := Array; min_items := 1;"), ("min_items_bad", "type := Array; min_items := x;"), ("min_items_negative", "type := Array; min_items := -1;"),
    ("min_items_fraction", "type := Array; min_items := 1.5;"), ("max_items_bad", "type := Array; max_items := true;"), ("max_items_negative", "type := Array; max_items := -2;"),
    ("items_conflict", "type := Array; min_items := 3; max_items := 1;"), ("items_equal", "type := Array; min_items := 2; max_items := 2;"),
    ("items_conflict_zero", "type := Array; min_items := 1; max_items := 0;"),
    ("items_not_object", "type := Array; items := 5;"), ("items_array", "type := Array; items := [1];"), ("items_ok", "type := Array; items := { type := String; };"),
    ("items_bad_inner", "type := Array; items := { type := Nope; };"), ("items_inner_conflict", "type := Array; items := { type := Numeric; min := 3; max := 1; };"),
    ("items_nested_bad_constraint", "type := Array; items := { type := String; size := x; };"),
    ("fields_not_object", "type := Object; fields := 5;"), ("fields_array", "type := Object; fields := [1];"), ("fields_string", 'type := Object; fields := "x";'),
    ("fields_ok", "type := Object; fields := { a := { type := String; }; };"), ("fields_inner_unknown_type", "type := Object; fields := { a := { type := Nope; }; };"),
    ("fields_inner_not_rule", "type := Object; fields := { a := 5; b := { type := String; }; };"), ("fields_deep_bad", "type := Object; fields := { a := { type := Object; fields := { b := { size := x; }; }; }; };"),
    ("fields_inner_conflict", "type := Object; fields := { a := { type := Numeric; min := 2; max := 1; }; };"),
    ("fields_on_string", "type := String; fields := { a := { type := String; }; };"),
    ("items_on_object", "type := Object; items := { type := String; };"),
    ("unknown_keys", "type := String; pooled := true; mask := 5; whatever := [1];"), ("no_keys_only_extra", "pooled := true;"),
    ("empty_type_value_only_required", "required := true;"),
    ("everything_bad", "type := Numeric; required := 1; nullable := 1; integer := 1; size := x; min := y; max := z; values := 1; format := q; tag := 1; min_items := a; max_items := b;"),
    ("many_conflicts", "type := Array; min := 5; max := 1; min_items := 5; max_items := 1;"),
]
for n, body in RULE_BODIES:
    case(D, "rule_" + n, [("s.anvl", SCHEMA + "a := { %s };\nb := { type := String; };\n" % body)],
         ['schema s load "s.anvl"', vv("s", "{ a := 1; b := \"x\"; }")])

# non-rule statements
NONRULES = [("number", "5"), ("string", '"x"'), ("bare", "word"), ("true", "true"), ("null", "null"), ("array", "[1, 2]"), ("tuple", "(1, 2)"),
            ("blob", "`blob`"), ("tagged_blob", "@t`blob`"), ("variable_ref", "$other")]
for n, v in NONRULES:
    case(D, "not_rule_" + n, [("s.anvl", SCHEMA + "other := { type := String; };\na := %s;\nb := { type := Bool; };\n" % v)],
         ['schema s load "s.anvl"', vv("s", "{ a := 1; b := true; other := \"x\"; }")])
case(D, "not_rule_block_form", [("s.anvl", SCHEMA + "a { type := String; };\nb := { type := Bool; };\n")], ['schema s load "s.anvl"', vv("s", '{ a := "x"; b := true; }')])
case(D, "rule_block_and_assign_forms", [("s.anvl", SCHEMA + "a { type := String; required := true; };\nb := { type := Bool; };\n")], ['schema s load "s.anvl"', vv("s", "{ b := true; }")])
case(D, "not_schema_doc", [("s.anvl", "#!aml\na := { type := String; };\n")], ['schema s load "s.anvl"', vv("s", "{ a := 1; }"), 'violations s'])
case(D, "types_doc_as_schema", [("s.anvl", TYPES + "a := { type := String; };\n")], ['schema s load "s.anvl"'])
case(D, "missing_doc", [], ['schema s load "gone.anvl"', vv("s", "{ a := 1; }")])
case(D, "syntax_error_doc", [("s.anvl", SCHEMA + "a := { type := ;\n")], ['schema s load "s.anvl"'])
case(D, "empty_schema", [("s.anvl", SCHEMA)], ['schema s load "s.anvl"', vv("s", "{ a := 1; }"), vv("s", "{ a := 1; }", "allow_unknown")])
case(D, "attributes_on_schema", [("s.anvl", "#!aml\n@[schema, table=assets, version=3]\na := { type := String; };\n")], ['schema s load "s.anvl"', vv("s", '{ a := "x"; }')])
case(D, "attributes_on_rules", [("s.anvl", SCHEMA + "@[hint=x] a := { type := String; };\nb := { type := Bool; };\n")], ['schema s load "s.anvl"', vv("s", '{ a := "x"; b := true; }')])
case(D, "registry_passthrough_each_code", [("t.anvl", TYPES + "Dup := { type := String; };\nBadBase := { type := Nope; };\nBadSize := { type := String; size := x; };\nBadValues := { type := enum; values := 3; };\nOk := { type := Bool; };\n"),
      ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\na := { type := types.Ok; };\nb := { type := types.BadBase; };\nc := { type := types.BadSize; };\nd := { type := types.BadValues; };\n')],
     ['schema s load "s.anvl"', vv("s", "{ a := true; b := 1; c := \"abcdef\"; d := x; }")])
case(D, "registry_collision_in_schema", [("t1.anvl", TYPES + "T := { type := String; };\n"), ("t2.anvl", TYPES + "T := { type := Numeric; };\n"), ("s.anvl", '#!aml\ninclude "t1.anvl";\ninclude "t2.anvl";\n@[schema]\na := { type := types.T; };\n')],
     ['schema s load "s.anvl"', vv("s", '{ a := 1; }'), vv("s", '{ a := "x"; }')])
case(D, "diagnostic_order", [("t.anvl", TYPES + "Bad := { type := Nope; };\n"), ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\nz := 5;\ny := { type := types.Typo; };\nx := { min := 3; max := 1; };\nw := { type := Object; fields := { v := { type := Nope; }; u := 1; }; };\n')],
     ['schema s load "s.anvl"'])
case(D, "self_reference_direct", [("t.anvl", TYPES + "Node := { type := Object; fields := { next := { type := types.Node; }; value := { type := Numeric; }; }; };\n"), ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\nroot := { type := types.Node; };\n')],
     ['schema s load "s.anvl"', vv("s", "{ root := { value := 1; next := { value := 2; }; }; }"), vv("s", "{ root := { value := 1; }; }")])
case(D, "self_reference_indirect", [("t.anvl", TYPES + "A := { type := Object; fields := { b := { type := types.B; }; }; };\nB := { type := Object; fields := { a := { type := types.A; }; }; };\n"), ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\nroot := { type := types.A; };\n')],
     ['schema s load "s.anvl"', vv("s", "{ root := { b := { a := { b := { x := 1; }; }; }; }; }")])
case(D, "self_reference_via_array", [("t.anvl", TYPES + "Tree := { type := Object; fields := { kids := { type := Array; items := { type := types.Tree; }; }; }; };\n"), ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\ntree := { type := types.Tree; };\n')],
     ['schema s load "s.anvl"', vv("s", "{ tree := { kids := [ { kids := [ 1 ]; } ]; }; }")])
case(D, "same_type_twice_is_not_a_cycle", [("t.anvl", TYPES + "P := { type := Object; fields := { x := { type := Numeric; }; }; };\n"), ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\na := { type := types.P; };\nb := { type := types.P; };\nc := { type := Object; fields := { d := { type := types.P; }; e := { type := types.P; }; }; };\n')],
     ['schema s load "s.anvl"', vv("s", "{ a := { x := 1; }; b := { y := 1; }; c := { d := { x := 2; }; e := { x := \"s\"; }; }; }")])

# rules / introspection
RL = "rules"
case(RL, "every_kind", [("s.anvl", SCHEMA + "".join("%s := { type := %s; %s };\n" % (k.lower(), k, "values := [a, b];" if k == "enum" else "") for k in KINDS) + "untyped := { pooled := true; };\nempty_rule := { required := true; };\n")],
     ['schema s load "s.anvl"'] + ['field s "%s"' % k.lower() for k in KINDS] + ['field s "untyped"', 'field s "empty_rule"', 'field s "nope"'])
case(RL, "all_constraints", [("s.anvl", SCHEMA + 'f := { type := String; required := true; nullable := true; size := 10; format := "uuid"; values := [a, "b c"]; };\nn := { type := Numeric; integer := true; min := -1.5; max := 2e3; };\nb := { type := Blob; tag := "png"; size := 100; };\na := { type := Array; min_items := 1; max_items := 9; items := { type := Numeric; }; };\n')],
     ['schema s load "s.anvl"'])
case(RL, "declaration_order", [("s.anvl", SCHEMA + "".join("%s := { type := Bool; };\n" % c for c in "zebra apple mango Apple _x a1 a10 a2".split()))], ['schema s load "s.anvl"'])
case(RL, "duplicate_rule_names", [("s.anvl", SCHEMA + "a := { type := String; };\na := { type := Numeric; };\n")], ['schema s load "s.anvl"', 'field s "a"'])
case(RL, "type_name_as_written", [("t.anvl", TYPES + "Id := { type := Numeric; min := 1; };\n"), ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\nid := { type := types.Id; };\nplain := { type := Numeric; };\nenumf := { type := enum; values := [x]; };\n')],
     ['schema s load "s.anvl"'])
case(RL, "inherit_all_from_type", [("t.anvl", TYPES + "T := { type := String; size := 5; format := uuid; nullable := true; };\nN := { type := Numeric; min := 1; max := 9; integer := true; };\nE := { type := enum; values := [a, b, c]; };\nB := { type := Blob; tag := png; size := 3; };\n"),
      ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\nt := { type := types.T; };\nn := { type := types.N; };\ne := { type := types.E; };\nb := { type := types.B; };\n')],
     ['schema s load "s.anvl"'])
case(RL, "override_inherited", [("t.anvl", TYPES + "T := { type := String; size := 5; format := uuid; nullable := true; };\nN := { type := Numeric; min := 1; max := 9; integer := true; };\nE := { type := enum; values := [a, b, c]; };\n"),
      ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\nt := { type := types.T; size := 7; format := iso8601_date; nullable := false; };\nn := { type := types.N; min := 3; integer := false; };\ne := { type := types.E; values := [z]; };\n')],
     ['schema s load "s.anvl"', vv("s", '{ t := "2026-01-01"; n := 2.5; e := z; }'), vv("s", '{ t := null; n := 2; e := a; }')])
case(RL, "inline_fields_replace_type_fields", [("t.anvl", TYPES + "P := { type := Object; fields := { x := { type := Numeric; }; y := { type := Numeric; }; }; };\n"),
      ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\ninherit := { type := types.P; };\nreplace := { type := types.P; fields := { z := { type := String; }; }; };\n')],
     ['schema s load "s.anvl"', vv("s", "{ inherit := { x := 1; y := 2; }; replace := { x := 1; z := \"s\"; }; }")])
case(RL, "nested_rules", [("s.anvl", SCHEMA + "o := { type := Object; fields := { a := { type := Object; fields := { b := { type := Array; items := { type := Object; fields := { c := { type := String; }; }; }; }; }; }; d := { type := Bool; }; }; };\n")],
     ['schema s load "s.anvl"', 'field s "o" "a"', 'field s "o" "a" "b"', 'field s "o" "d"', 'field s "o" "a" "zzz"', 'field s "o" "d" "x"'])
case(RL, "field_lookup_names", [("s.anvl", SCHEMA + 'a := { type := Bool; };\nA := { type := Bool; };\nb_c := { type := Bool; };\n')], ['schema s load "s.anvl"', 'field s "a"', 'field s "A"', 'field s ""', 'field s "a "'])
case(RL, "field_lookup_dotted_names", [("s.anvl", SCHEMA + "o := { type := Object; fields := { a := { type := Bool; }; }; };\n")], ['schema s load "s.anvl"', 'field s "o.a"', 'field s "o" "a"'])

EXTRAS = [("bool", "true"), ("number", "42"), ("float", "1.5e2"), ("big", "9223372036854775808"), ("string", '"text"'), ("escape", '"a\\nb\\t\\"c\\""'), ("bare", "word"), ("null", "null"),
          ("blob", "`raw`"), ("tagged", "@md`# hi`"), ("array", "[1, two, \"three\", 4.5]"), ("tuple", "(1, 2, 3)"), ("object", "{ a := 1; b := { c := [1]; }; }"),
          ("nested_array", "[[1, 2], [3]]"), ("unicode", '"héllo 日本"'), ("nul_string", '"a\x00b"'), ("variable", "$sibling"), ("empty_string", '""'),
          ("negative", "-7"), ("float_neg_zero", "-0.0"), ("overflow_float", "1e999")]
for n, v in EXTRAS:
    case(RL, "extra_" + n, [("s.anvl", SCHEMA + "sibling := { type := Bool; };\nf := { type := String; pooled := %s; after := 1; };\n" % v)], ['schema s load "s.anvl"'])
case(RL, "extras_nested_and_items", [("s.anvl", SCHEMA + "o := { type := Object; mask := 1; fields := { a := { type := String; note := \"n\"; }; }; };\nl := { type := Array; items := { type := String; cap := 3; }; };\n")], ['schema s load "s.anvl"'])
case(RL, "extras_known_keys_are_not_extras", [("s.anvl", SCHEMA + "f := { type := String; required := true; nullable := true; integer := false; size := 1; min := 1; max := 2; min_items := 1; max_items := 2; values := [a]; format := uuid; tag := t; items := { type := String; }; fields := { a := { type := String; }; }; mine := 1; };\n")],
     ['schema s load "s.anvl"'])
case(RL, "dispose_source_independent", [("s.anvl", SCHEMA + "f := { type := String; size := 3; extra := [1, 2]; };\n")], ['schema s load "s.anvl"', vv("s", '{ f := "abcd"; }')],
     desc="the schema document is disposed before the rules are read")
case(RL, "two_schemas_one_file", [("s.anvl", SCHEMA + "f := { type := String; size := 3; };\n")], ['schema a load "s.anvl"', 'schema b load "s.anvl"', vv("a", '{ f := "abcd"; }'), vv("b", '{ f := "ab"; }'), 'violations a', 'violations b'])
case(RL, "many_fields", [("s.anvl", SCHEMA + "".join("f%d := { type := Numeric; min := %d; };\n" % (i, i) for i in range(60)))], ['schema s load "s.anvl"', 'field s "f0"', 'field s "f59"'])

# --------------------------------------------------------------------------------------------------------------
# validation: value kinds
# --------------------------------------------------------------------------------------------------------------
K = "kinds"
VALUES = [("null", "null"), ("true", "true"), ("false", "false"), ("int", "1"), ("negint", "-5"), ("float", "1.5"), ("exp", "1e3"), ("string", '"s"'), ("empty_string", '""'),
          ("bare", "word"), ("blob", "`blob`"), ("tagged_blob", "@png`xyz`"), ("array", "[1, 2]"), ("tuple", "(1, 2)"), ("object", "{ a := 1; }"), ("nested_object", "{ a := { b := 1; }; }"),
          ("string_number", '"12"'), ("bare_true_word", "True"), ("array_of_strings", '["a", "b"]'), ("unicode", '"é"')]
RULES_BY_KIND = [("numeric", "type := Numeric;"), ("string", "type := String;"), ("bool", "type := Bool;"), ("object", "type := Object;"), ("tuple", "type := Tuple;"),
                 ("array", "type := Array;"), ("enum", "type := enum; values := [word, s, a];"), ("blob", "type := Blob;"), ("untyped", "pooled := true;"),
                 ("numeric_nullable", "type := Numeric; nullable := true;"), ("string_nullable", "type := String; nullable := true;"), ("enum_nullable", "type := enum; values := [a]; nullable := true;"),
                 ("object_with_fields", "type := Object; fields := { a := { type := Numeric; }; };"), ("array_of_numeric", "type := Array; items := { type := Numeric; };")]
for kn, body in RULES_BY_KIND:
    case(K, "matrix_" + kn, [("s.anvl", SCHEMA + "f := { %s };\n" % body)], ['schema s load "s.anvl"'] + [vv("s", "{ f := %s; }" % v) for _, v in VALUES], desc="every value form against a %s rule" % kn)

# required / unknown / options
O = "options"
base_schema = SCHEMA + "a := { type := Numeric; required := true; };\nb := { type := String; };\nc := { type := Bool; required := true; nullable := true; };\n"
RECS = [("empty_ok", "{ a := 1; c := true; }"), ("missing_a", "{ c := true; }"), ("missing_all", "{ b := \"x\"; }"), ("unknown", "{ a := 1; c := null; z := 1; }"),
        ("unknown_many", "{ y := 1; a := 1; c := true; x := 2; z := 3; }"), ("wrong_and_missing", "{ a := \"s\"; }"), ("null_required", "{ a := null; c := null; }"),
        ("everything_wrong", "{ a := true; b := 1; c := 1; d := 2; }"), ("order_reversed", "{ c := true; b := \"x\"; a := 1; }")]
OPTSETS = [(), ("skip_required",), ("allow_unknown",), ("skip_required", "allow_unknown"), ("no_opts",), ("allow_unknown", "skip_required")]
case(O, "records_by_options", [("s.anvl", base_schema)], ['schema s load "s.anvl"'] + [vv("s", r, *o) for _, r in RECS for o in OPTSETS], desc="every record under every option set")
case(O, "skip_required_nested", [("s.anvl", SCHEMA + "o := { type := Object; required := true; fields := { x := { type := Numeric; required := true; }; y := { type := String; }; }; };\nl := { type := Array; items := { type := Object; fields := { t := { type := String; required := true; }; }; }; };\n")],
     ['schema s load "s.anvl"'] + [vv("s", r, *o) for r in ["{ o := { y := \"s\"; }; }", "{ l := [ { t := \"a\"; }, { u := 1; } ]; }", "{ o := { x := 1; z := 2; }; l := [ { u := 1; } ]; }"] for o in OPTSETS[:4]])
case(O, "skip_required_still_checks_present", [("s.anvl", SCHEMA + "a := { type := Numeric; required := true; min := 5; };\nb := { type := String; size := 2; };\n")],
     ['schema s load "s.anvl"', vv("s", '{ a := 1; b := "xyz"; }', "skip_required"), vv("s", '{ b := "xyz"; }', "skip_required"), vv("s", '{ zz := 1; }', "skip_required")])
case(O, "unresolved_type_not_excused", [("s.anvl", SCHEMA + "a := { type := types.Nope; required := true; nullable := true; };\nb := { type := Nope2; };\n")],
     ['schema s load "s.anvl"'] + [vv("s", r, *o) for r in ["{ zz := 1; }", "{ a := 1; }", "{ a := null; b := null; }", "{ z := 1; }"] for o in OPTSETS[:4]])
case(O, "options_independent_between_calls", [("s.anvl", base_schema)], ['schema s load "s.anvl"', vv("s", "{ z := 1; }", "allow_unknown", "skip_required"), vv("s", "{ z := 1; }"), vv("s", "{ z := 1; }", "no_opts")])
case(O, "empty_record_fragment_forms", [("s.anvl", base_schema)], ['schema s load "s.anvl"', vv("s", "{ a := 1; c := true; }"), vv("s", "{a:=1;c:=true;}"), vv("s", "\n{\n a := 1;\n c := true;\n}\n"), vv("s", "{ a := 1; /* c */ c := true; }"), vv("s", "{ a := 1; c := true; } ")])
case(O, "fragment_not_an_object", [("s.anvl", base_schema)], ['schema s load "s.anvl"'] + [vv("s", v) for _, v in VALUES[:16] if _ not in ("object", "nested_object")], desc="a non-object value is one TYPE_MISMATCH with an empty path")
case(O, "fragment_syntax_errors", [("s.anvl", base_schema)], ['schema s load "s.anvl"', vv("s", "{ a := 1; c := true; }")] + [vv("s", t) for t in ["{ a := ", "{ a = 1 }", "{ a := 1 }", "", "   ", "{ }", "{ a := 1; } extra", "{ a := 1; }; { b := 2; }", "a := 1;", "{ a := [ ]; }", "{ a := 'x'; }", "{ a := 1; a := 2; }", "{ a := }"]])
case(O, "null_value_and_handles", [("s.anvl", base_schema)], ['schema s load "s.anvl"', vv("s", "{ z := 1; }"), "validate_value s NULL", 'violations s', vv("s", "{ a := 1; c := true; }"), 'validate_value s NULL', 'violations s', 'validate_value NULL NULL', 'validate_value NULL "{ a := 1; }"', 'violations NULL', 'validate NULL "s.anvl"'])

# numeric
N = "numeric"
NUMS = ["0", "-0", "-0.0", "0.0", "1", "-1", "0.5", "-0.5", "2", "2.0", "2.5", "3", "10", "10.0", "10.000000000001", "11", "1e1", "1E1", "1e+1", "1e-1", "100e-1", "0.1e2",
        "9007199254740991", "9007199254740992", "9007199254740993", "9223372036854775807", "9223372036854775808", "-9223372036854775808", "-9223372036854775809", "18446744073709551615",
        "1e308", "1.7976931348623157e308", "1.7976931348623159e308", "1e999", "-1e999", "5e-324", "1e-999", "007", "-007", "00010", "0.30000000000000004", "999.9999999999999", "1000", "1e3", "-1e3",
        "2147483647", "2147483648", "-2147483648", "-2147483649", "4294967295", "4294967296", "123456789012345678901234567890", "1.5e300", "-1.5e300"]
COMBOS = [("none", ""), ("min0", "min := 0;"), ("max10", "max := 10;"), ("min0_max10", "min := 0; max := 10;"), ("min_half", "min := 0.5;"), ("max_neg_half", "max := -0.5;"),
          ("min1e3", "min := 1e3;"), ("integer", "integer := true;"), ("integer_1_3", "integer := true; min := 1; max := 3;"), ("min_i64min", "min := -9223372036854775808;"),
          ("max_i64max", "max := 9223372036854775807;"), ("min_2p53p1", "min := 9007199254740993;"), ("max_2p53", "max := 9007199254740992;"), ("max_dblmax", "max := 1.7976931348623157e308;"),
          ("min_dblmin_neg", "min := -1.7976931348623157e308;"), ("int32", "integer := true; min := -2147483648; max := 2147483647;"), ("min_eq_max", "min := 10; max := 10;"),
          ("exp_bounds", "min := 1e-1; max := 1e1;"), ("min_gt_max", "min := 10; max := 0;"), ("min_overflow", "min := 1e999;"), ("min_zero_neg", "min := -0.0;"), ("integer_false", "integer := false; min := 0;"),
          ("min_tiny", "min := 5e-324;"), ("max_tiny_neg", "max := -5e-324;"), ("min_u64", "min := 18446744073709551615;"), ("min_bignum", "min := 123456789012345678901234567890;")]
for cn, body in COMBOS:
    case(N, "range_" + cn, [("s.anvl", SCHEMA + "n := { type := Numeric; %s };\n" % body)], ['schema s load "s.anvl"'] + [vv("s", "{ n := %s; }" % v) for v in NUMS], desc=body)
case(N, "integer_by_value_not_spelling", [("s.anvl", SCHEMA + "n := { type := Numeric; integer := true; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ n := %s; }" % v) for v in ["2", "2.0", "2.50", "1e+3", "1e-1", "1e2", "12e-1", "0.0", "-0.0", "1.0000000000000001", "9007199254740993.0", "1e400", "-2.0", "5e-324"]])
case(N, "numbers_as_other_kinds", [("s.anvl", SCHEMA + "s := { type := String; };\nb := { type := Bool; };\ne := { type := enum; values := [1, 2.0, 1e1, 007]; };\n")],
     ['schema s load "s.anvl"'] + [vv("s", "{ %s := %s; }" % (f, v)) for f in "sbe" for v in ["1", "2.0", "1e1", "007", "2", "10", "true"]])
case(N, "enum_numeric_membership", [("s.anvl", SCHEMA + "e := { type := enum; values := [1, 2.5, \"3\", 1e2]; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ e := %s; }" % v) for v in ["1", "1.0", "2.5", "2.50", "3", '"3"', "1e2", "100", '"1e2"', "01"]])

# strings
S = "strings"
STRS = [("empty", '""'), ("a", '"a"'), ("abc", '"abc"'), ("abcd", '"abcd"'), ("abcde", '"abcde"'), ("accent", '"héllo"'), ("cjk", '"日本語"'), ("astral", '"\U0001f600"'), ("newline", '"a\\nb"'),
        ("tab", '"a\\tb"'), ("backslash", '"a\\\\b"'), ("quote", '"a\\"b"'), ("unknown_escape", '"a\\xb"'), ("nul", '"a\x00b"'), ("bare", "abc"), ("bare_long", "abcdef"), ("number", "12345"),
        ("number_long", "123456"), ("float", "1.5"), ("true", "true"), ("null", "null"), ("blob", "`abc`"), ("blob_long", "`abcdef`"), ("tagged", "@t`abc`"), ("tagged_long", "@tag`abcdef`"),
        ("empty_blob", "``"), ("multiline", '"a\nb\nc"'), ("spaces", '"     "'), ("array", '["abc"]'), ("object", "{ a := 1; }")]
for sz in (0, 1, 3, 5):
    case(S, "size_%d" % sz, [("s.anvl", SCHEMA + "f := { type := String; size := %d; };\n" % sz)], ['schema s load "s.anvl"'] + [vv("s", "{ f := %s; }" % v) for _, v in STRS], desc="size counts bytes")
case(S, "size_on_other_kinds", [("s.anvl", SCHEMA + "n := { type := Numeric; size := 2; };\ne := { type := enum; values := [aaa, bb]; size := 2; };\nu := { size := 2; };\n")],
     ['schema s load "s.anvl"'] + [vv("s", "{ %s := %s; }" % (f, v)) for f in "neu" for v in ["1", "123", "12.5", "aaa", "bb", '"xyz"', "`xyz`", "true"]])
case(S, "string_kinds_accepted", [("s.anvl", SCHEMA + "f := { type := String; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ f := %s; }" % v) for _, v in STRS])
case(S, "enum_membership", [("s.anvl", SCHEMA + 'e := { type := enum; values := [open, "in progress", "", "a\x00b", "héllo", Closed]; };\n')],
     ['schema s load "s.anvl"'] + [vv("s", "{ e := %s; }" % v) for v in ["open", '"open"', "Open", "closed", "Closed", '"in progress"', '"in  progress"', '""', '"a\x00b"', '"a"', "`open`", "@t`open`", '"héllo"', '"hello"', "null", "1", "true", "[open]", '"open "']])
case(S, "enum_label_edge", [("s.anvl", SCHEMA + 'e := { type := enum; values := [a, ab, "a\x00"]; };\n')], ['schema s load "s.anvl"'] + [vv("s", "{ e := %s; }" % v) for v in ['"a"', '"ab"', '"a\x00"', '"a\x00b"', '"abc"', "a", "ab"]])
case(S, "enum_does_not_accept_blob", [("s.anvl", SCHEMA + "e := { type := enum; values := [abc]; };\n")], ['schema s load "s.anvl"', vv("s", "{ e := `abc`; }"), vv("s", "{ e := @t`abc`; }"), vv("s", "{ e := abc; }")])
case(S, "values_on_untyped_and_string", [("s.anvl", SCHEMA + "a := { values := [x, y]; };\nb := { type := String; values := [x, y]; };\nc := { type := Numeric; values := [1, 2]; };\n")],
     ['schema s load "s.anvl"'] + [vv("s", "{ %s := %s; }" % (f, v)) for f in "abc" for v in ["x", '"y"', "z", "1", "2", "3", "`x`"]])
case(S, "escapes_decoded_for_size_and_values", [("s.anvl", SCHEMA + 'f := { type := String; size := 3; };\ne := { type := enum; values := ["a\\nb"]; };\n')], ['schema s load "s.anvl"', vv("s", '{ f := "a\\nb"; }'), vv("s", '{ f := "a\\\\nb"; }'), vv("s", '{ e := "a\\nb"; }'), vv("s", '{ e := "a\\\\nb"; }')])

# formats
F = "format"
FMT = {
    "iso8601_date": ["2026-01-01", "2026-02-28", "2026-02-29", "2024-02-29", "2100-02-29", "2000-02-29", "1900-02-29", "2026-12-31", "2026-13-01", "2026-00-10", "2026-04-31", "2026-04-30", "2026-01-00", "2026-01-32",
                     "0000-01-01", "9999-12-31", "2026-1-1", "20260101", "2026-01-01 ", " 2026-01-01", "2026-01-01T00:00:00Z", "", "26-01-01", "2026/01/01", "2026-01-011", "+2026-01-01", "2026-01-0a", "٢٠٢٦-01-01",
                     "2026-02-30", "2026-06-31", "2026-11-31", "2023-02-29", "2400-02-29", "2026-W01-1"],
    "iso8601_datetime": ["2026-10-29T12:30:00Z", "2026-10-29T12:30:00.123+05:30", "2026-12-31T23:59:60Z", "2026-10-29T12:30:00", "2026-10-29 12:30:00Z", "2026-10-29T12:30:00+05", "2026-10-29t12:30:00z",
                         "2026-10-29T12:30:00z", "2026-10-29T12:30:00-08:00", "2026-10-29T12:30:00+0530", "2026-10-29T24:00:00Z", "2026-10-29T23:60:00Z", "2026-10-29T23:59:61Z", "2026-10-29T12:30Z", "2026-10-29T12:30:00.Z",
                         "2026-10-29T12:30:00.1Z", "2026-10-29T12:30:00.123456789Z", "2026-02-29T12:30:00Z", "2024-02-29T12:30:00Z", "2026-10-29T12:30:00+24:00", "2026-10-29T12:30:00+23:59", "2026-10-29T12:30:00+05:60",
                         "2026-10-29T25:30:00Z", "2026-10-29T12:30:00 Z", "2026-10-29T12:30:00ZZ", "2026-10-29", "T12:30:00Z", "2026-10-29T1:30:00Z", "", "2026-10-29T12:30:00-00:00", "0000-01-01T00:00:00Z", "9999-12-31T23:59:59Z",
                         "2026-10-29T12:30:00,5Z", "2026-10-29T12:30:00+05:30:00"],
    "uuid": ["550e8400-e29b-41d4-a716-446655440000", "550E8400-E29B-41D4-A716-446655440000", "550e8400-E29B-41d4-a716-446655440000", "00000000-0000-0000-0000-000000000000", "ffffffff-ffff-ffff-ffff-ffffffffffff",
             "550e8400e29b41d4a716446655440000", "550e8400-e29b-41d4-a716-44665544000", "550e8400-e29b-41d4-a716-4466554400000", "550e8400-e29b-41d4-a716-44665544000g", "{550e8400-e29b-41d4-a716-446655440000}",
             "550e8400-e29b-41d4-a716-446655440000 ", " 550e8400-e29b-41d4-a716-446655440000", "", "urn:uuid:550e8400-e29b-41d4-a716-446655440000", "550e840-0e29b-41d4-a716-446655440000", "550e8400-e29b-41d4-a716_446655440000",
             "550e8400-e29b-11d4-a716-446655440000", "550e8400-e29b-51d4-f716-446655440000", "g50e8400-e29b-41d4-a716-446655440000"],
}
for fmt, vals in FMT.items():
    case(F, fmt + "_strings", [("s.anvl", SCHEMA + "f := { type := String; format := %s; };\n" % fmt)], ['schema s load "s.anvl"'] + [vv("s", "{ f := %s; }" % q(v)) for v in vals])
    case(F, fmt + "_kinds", [("s.anvl", SCHEMA + "f := { type := String; format := %s; };\n" % fmt)], ['schema s load "s.anvl"'] + [vv("s", "{ f := %s; }" % v) for v in
         ["%s" % vals[0], q(vals[0]), "`%s`" % vals[0], "@t`%s`" % vals[0], "``", "1", "1.5", "true", "null", "[%s]" % q(vals[0]), "(%s, %s)" % (q(vals[0]), q(vals[0])), "{ a := 1; }"]])
    case(F, fmt + "_bare", [("s.anvl", SCHEMA + "f := { type := String; format := %s; };\n" % fmt)], ['schema s load "s.anvl"'] + [vv("s", "{ f := %s; }" % v) for v in [x for x in vals if re.match(r"^[0-9A-Za-z:.+-]+$", x)][:20]])
case(F, "format_inherited_and_overridden", [("t.anvl", TYPES + "D := { type := String; format := iso8601_date; };\nU := { type := String; format := uuid; };\n"),
     ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\nd := { type := types.D; };\nu := { type := types.U; format := iso8601_date; };\nx := { type := types.D; format := nonsense; };\n')],
     ['schema s load "s.anvl"', vv("s", '{ d := "2026-01-01"; u := "2026-01-01"; x := "garbage"; }'), vv("s", '{ d := "garbage"; u := "550e8400-e29b-41d4-a716-446655440000"; x := "2026-02-30"; }')])
case(F, "format_with_size_and_values", [("s.anvl", SCHEMA + 'f := { type := String; format := iso8601_date; size := 8; values := ["2026-01-01"]; };\n')], ['schema s load "s.anvl"', vv("s", '{ f := "2026-01-01"; }'), vv("s", '{ f := "2026-01-02"; }'), vv("s", '{ f := "x"; }')])
case(F, "format_in_nested_positions", [("s.anvl", SCHEMA + 'o := { type := Object; fields := { d := { type := String; format := iso8601_date; }; }; };\nl := { type := Array; items := { type := String; format := uuid; }; };\n')],
     ['schema s load "s.anvl"', vv("s", '{ o := { d := "2026-13-01"; }; l := [ "550e8400-e29b-41d4-a716-446655440000", "nope" ]; }')])
case(F, "format_on_non_strings", [("s.anvl", SCHEMA + "n := { type := Numeric; format := uuid; };\nu := { format := uuid; };\ne := { type := enum; values := [x]; format := uuid; };\n")],
     ['schema s load "s.anvl"'] + [vv("s", "{ %s := %s; }" % (f, v)) for f in "nue" for v in ["1", '"x"', "x", "`x`"]])
case(F, "format_with_nul_payload", [("s.anvl", SCHEMA + "f := { type := String; format := uuid; };\n")], ['schema s load "s.anvl"', vv("s", '{ f := "550e8400-e29b-41d4-a716-44665544000\x00"; }'), vv("s", '{ f := "\x00"; }')])

# blobs
B = "blobs"
case(B, "tag_matching", [("s.anvl", SCHEMA + "b := { type := Blob; tag := png; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ b := %s; }" % v) for v in
     ["@png`x`", "@PNG`x`", "@jpg`x`", "`x`", "@png``", "@pngg`x`", "@pn`x`", '"x"', "x", "1", "null", "[1]", "@t`png`", "@png`a\nb`"]])
case(B, "tag_quoted_string_form", [("s.anvl", SCHEMA + 'b := { type := Blob; tag := "png"; };\nc := { type := Blob; tag := "p n"; };\n')], ['schema s load "s.anvl"', vv("s", "{ b := @png`x`; c := @png`x`; }"), vv("s", "{ b := `x`; }")])
case(B, "blob_without_tag_requirement", [("s.anvl", SCHEMA + "b := { type := Blob; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ b := %s; }" % v) for v in ["`x`", "@png`x`", "@a`x`", "``", '"x"', "x", "null"]])
case(B, "blob_size", [("s.anvl", SCHEMA + "b := { type := Blob; size := 4; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ b := %s; }" % v) for v in ["``", "`abcd`", "`abcde`", "@longtag`abcd`", "@longtag`abcde`", "`éé`", "`ééé`", "`a\x00b`", "`a\x00bcd`", "`ab\ncd`"]])
case(B, "blob_as_string", [("s.anvl", SCHEMA + "s := { type := String; size := 3; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ s := %s; }" % v) for v in ["`abc`", "`abcd`", "@t`abc`", "@t`abcd`", "``"]])
case(B, "blob_tag_and_size_and_format", [("s.anvl", SCHEMA + "b := { type := Blob; tag := txt; size := 36; format := uuid; };\n")], ['schema s load "s.anvl"', vv("s", "{ b := @txt`550e8400-e29b-41d4-a716-446655440000`; }"), vv("s", "{ b := @txt`550e8400-e29b-41d4-a716-44665544000`; }"), vv("s", "{ b := @png`x`; }"), vv("s", "{ b := `550e8400-e29b-41d4-a716-446655440000`; }")])
case(B, "blob_inherited_tag", [("t.anvl", TYPES + "Png := { type := Blob; tag := png; size := 5; };\n"), ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\na := { type := types.Png; };\nb := { type := types.Png; tag := jpg; };\n')],
     ['schema s load "s.anvl"', vv("s", "{ a := @png`12345`; b := @jpg`12345`; }"), vv("s", "{ a := @jpg`123456`; b := @png`1`; }")])
case(B, "blob_in_array", [("s.anvl", SCHEMA + "l := { type := Array; items := { type := Blob; tag := t; }; };\n")], ['schema s load "s.anvl"', vv("s", "{ l := [ @t`a`, `b`, @t`c`, 3 ]; }")])
case(B, "tag_malformed_rule", [("s.anvl", SCHEMA + "b := { type := Blob; tag := 5; };\n")], ['schema s load "s.anvl"', vv("s", "{ b := @x`y`; }"), vv("s", "{ b := `y`; }")])

# collections
C = "collections"
case(C, "array_counts", [("s.anvl", SCHEMA + "a := { type := Array; min_items := 2; max_items := 3; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ a := %s; }" % v) for v in ["[1]", "[1, 2]", "[1, 2, 3]", "[1, 2, 3, 4]", "[[1, 2]]", "(1, 2)", "(1, 2)", "5"]])
case(C, "array_counts_only_min", [("s.anvl", SCHEMA + "a := { type := Array; min_items := 1; };\nb := { type := Array; max_items := 1; };\nc := { type := Array; min_items := 0; max_items := 0; };\n")], ['schema s load "s.anvl"', vv("s", "{ a := [1]; b := [1]; c := [1]; }"), vv("s", "{ a := [1, 2]; b := [1, 2]; c := [1, 2]; }")])
case(C, "array_counts_on_untyped", [("s.anvl", SCHEMA + "a := { min_items := 2; };\nb := { max_items := 1; };\n")], ['schema s load "s.anvl"', vv("s", "{ a := [1]; b := [1, 2]; }"), vv("s", "{ a := 5; b := (1, 2, 3); }")])
case(C, "array_conflicting_counts", [("s.anvl", SCHEMA + "a := { type := Array; min_items := 3; max_items := 1; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ a := %s; }" % v) for v in ["[1]", "[1, 2]", "[1, 2, 3]", "[1, 2, 3, 4]"]])
case(C, "array_items_by_kind", [("s.anvl", SCHEMA + "".join("%s := { type := Array; items := { %s }; };\n" % (k, b) for k, b in RULES_BY_KIND[:9]))],
     ['schema s load "s.anvl"'] + [vv("s", "{ %s := [ 1, \"s\", word, true, null, `b`, [1], (1, 2), { a := 1; } ]; }" % k) for k, _ in RULES_BY_KIND[:9]])
case(C, "array_item_paths", [("s.anvl", SCHEMA + "a := { type := Array; items := { type := Numeric; max := 5; }; };\n")], ['schema s load "s.anvl"', vv("s", "{ a := [1, 9, \"x\", 3, 10, true]; }")] + [vv("s", "{ a := [%s]; }" % ", ".join(["1"] * n + ["9"])) for n in (9, 10, 11, 99, 100)])
case(C, "array_of_arrays", [("s.anvl", SCHEMA + "m := { type := Array; items := { type := Array; items := { type := Numeric; }; }; };\n")], ['schema s load "s.anvl"', vv("s", "{ m := [ [1, 2], [3, \"x\"], 7, [[1]] ]; }")])
case(C, "array_of_objects_deep", [("s.anvl", SCHEMA + "r := { type := Array; items := { type := Object; fields := { k := { type := String; required := true; }; l := { type := Array; items := { type := Object; fields := { z := { type := Numeric; }; }; }; }; }; }; };\n")],
     ['schema s load "s.anvl"', vv("s", "{ r := [ { k := \"a\"; l := [ { z := 1; }, { z := \"b\"; }, { y := 1; } ]; }, { l := [ 5 ]; } ]; }")])
case(C, "tuple_rule", [("s.anvl", SCHEMA + "t := { type := Tuple; };\nu := { type := Tuple; items := { type := Numeric; }; min_items := 2; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ t := %s; u := %s; }" % (a, b)) for a, b in [("(1, 2)", "(1, 2)"), ("[1, 2]", "[1]"), ("(1, 2)", "(1, 3)"), ("1", "(\"x\", 2)")]])
case(C, "object_fields", [("s.anvl", SCHEMA + "o := { type := Object; fields := { a := { type := Numeric; required := true; }; b := { type := String; }; }; };\n")],
     ['schema s load "s.anvl"'] + [vv("s", "{ o := %s; }" % v) for v in ["{ a := 1; }", "{ b := \"x\"; }", "{ a := \"x\"; b := 2; }", "{ a := 1; c := 3; }", "{ a := 1; b := \"x\"; c := 1; d := 2; }", "[1]", "5", "null", "{ a := null; }"]])
case(C, "object_without_fields_accepts_any", [("s.anvl", SCHEMA + "o := { type := Object; };\n")], ['schema s load "s.anvl"', vv("s", "{ o := { anything := [1, 2]; goes := { deep := 1; }; }; }")])
case(C, "object_depth", [("s.anvl", SCHEMA + "a := { type := Object; fields := { b := { type := Object; fields := { c := { type := Object; fields := { d := { type := Object; fields := { e := { type := Numeric; required := true; }; }; }; }; }; }; }; }; };\n")],
     ['schema s load "s.anvl"', vv("s", "{ a := { b := { c := { d := { e := \"x\"; }; }; }; }; }"), vv("s", "{ a := { b := { c := { d := { f := 1; }; }; }; }; }"), vv("s", "{ a := { b := { c := { d := 1; }; }; }; }")])
def deep_rule(n):
    inner = "{ type := Numeric; required := true; }"
    for i in reversed(range(n)):
        inner = "{ type := Object; fields := { y%d := %s; }; }" % (i, inner)
    return inner
def deep_value(n, leaf):
    v = leaf
    for i in reversed(range(n)):
        v = "{ y%d := %s; }" % (i, v)
    return v
case(C, "object_depth_20", [("s.anvl", SCHEMA + "x := %s;\n" % deep_rule(20))], ['schema s load "s.anvl"', vv("s", "{ x := %s; }" % deep_value(20, '"s"')), vv("s", "{ x := %s; }" % deep_value(19, "{ z := 1; }")), vv("s", "{ x := %s; }" % deep_value(20, "5"))])
case(C, "object_fields_in_array_in_object", [("s.anvl", SCHEMA + "o := { type := Object; fields := { l := { type := Array; items := { type := Object; fields := { n := { type := Numeric; min := 0; }; }; }; }; }; };\n")],
     ['schema s load "s.anvl"', vv("s", "{ o := { l := [ { n := 1; }, { n := -1; } ]; }; }")])
case(C, "object_inherit_fields_from_type", [("t.anvl", TYPES + "Person := { type := Object; fields := { id := { type := Numeric; required := true; }; name := { type := String; size := 4; }; }; };\n"),
      ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\nowner := { type := types.Person; };\nstaff := { type := Array; items := { type := types.Person; }; };\n')],
     ['schema s load "s.anvl"', vv("s", '{ owner := { name := "toolong"; x := 1; }; staff := [ { id := 1; }, { x := 1; }, { id := "x"; name := "ab"; } ]; }'), vv("s", '{ owner := { id := 1; }; }')])
case(C, "nested_required_and_nullable", [("s.anvl", SCHEMA + "o := { type := Object; fields := { a := { type := Numeric; required := true; nullable := true; }; b := { type := String; nullable := true; }; }; };\n")],
     ['schema s load "s.anvl"'] + [vv("s", "{ o := %s; }" % v) for v in ["{ a := null; }", "{ a := null; b := null; }", "{ b := null; }", "{ a := 1; b := null; }", "{ a := 1; b := 2; }"]])
case(C, "unknown_at_every_depth", [("s.anvl", SCHEMA + "o := { type := Object; fields := { l := { type := Array; items := { type := Object; fields := { k := { type := Bool; }; }; }; }; }; };\n")],
     ['schema s load "s.anvl"', vv("s", "{ o := { l := [ { k := true; zz := 1; } ]; yy := 2; }; xx := 3; }"), vv("s", "{ o := { l := [ { k := true; zz := 1; } ]; yy := 2; }; xx := 3; }", "allow_unknown")])
case(C, "duplicate_data_names", [("s.anvl", SCHEMA + "a := { type := Numeric; };\n")], ['schema s load "s.anvl"', vv("s", "{ a := 1; a := 2; }"), vv("s", '{ a := 1; a := "x"; }'), vv("s", '{ a := "x"; a := 1; }'), vv("s", "{ z := 1; z := 2; }")],
     desc="a repeated key is a parse error of the fragment (4001), not a violation")
case(C, "data_object_field_order", [("s.anvl", SCHEMA + "".join("%s := { type := Numeric; required := true; };\n" % c for c in "abcde"))], ['schema s load "s.anvl"', vv("s", "{ e := \"x\"; c := \"x\"; a := \"x\"; z := 1; y := 1; }"), vv("s", "{ z := 1; }")])

# nullable
NU = "nullable"
case(NU, "null_by_rule", [("s.anvl", SCHEMA + "".join("%s := { %s };\n" % (k, b) for k, b in RULES_BY_KIND[:9] + [("n_numeric", "type := Numeric; nullable := true; min := 1; max := 2;"), ("n_string", "type := String; nullable := true; size := 0; format := uuid;"), ("n_enum", "type := enum; nullable := true; values := [a];"), ("n_obj", "type := Object; nullable := true; fields := { x := { type := Numeric; required := true; }; };"), ("n_arr", "type := Array; nullable := true; min_items := 3;")]))],
     ['schema s load "s.anvl"', vv("s", "{ " + " ".join("%s := null;" % k for k, _ in RULES_BY_KIND[:9] + [("n_numeric", ""), ("n_string", ""), ("n_enum", ""), ("n_obj", ""), ("n_arr", "")]) + " }")])
case(NU, "nullable_false_overrides_type", [("t.anvl", TYPES + "T := { type := String; nullable := true; };\n"), ("s.anvl", '#!aml\ninclude "t.anvl";\n@[schema]\na := { type := types.T; };\nb := { type := types.T; nullable := false; };\n')],
     ['schema s load "s.anvl"', vv("s", "{ a := null; b := null; }")])
case(NU, "nullable_array_items", [("s.anvl", SCHEMA + "l := { type := Array; items := { type := Numeric; nullable := true; }; };\nm := { type := Array; items := { type := Numeric; }; };\n")], ['schema s load "s.anvl"', vv("s", "{ l := [1, null, 3]; m := [1, null, 3]; }")])
case(NU, "null_absent_present", [("s.anvl", SCHEMA + "a := { type := Numeric; required := true; nullable := true; };\nb := { type := Numeric; required := true; };\nc := { type := Numeric; nullable := true; };\n")],
     ['schema s load "s.anvl"', vv("s", "{ zz := 1; }"), vv("s", "{ a := null; b := null; c := null; }"), vv("s", "{ a := null; b := 1; }"), vv("s", "{ b := 1; }")])
case(NU, "null_word_forms", [("s.anvl", SCHEMA + "a := { type := String; nullable := true; size := 2; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ a := %s; }" % v) for v in ["null", '"null"', "`null`", "Null", "NULL", "nil", "none"]])

# data documents (validate)
DT = "data"
sch = SCHEMA + "id := { type := Numeric; required := true; integer := true; min := 1; };\ntitle := { type := String; required := true; size := 8; };\ntags := { type := Array; max_items := 2; items := { type := String; size := 3; }; };\nmeta := { type := Object; fields := { k := { type := String; }; }; };\n"
DATAS = [("valid", "id := 1;\ntitle := \"ok\";\ntags := [a, b];\nmeta := { k := \"v\"; };\n"), ("empty", ""), ("only_comment", "// nothing\n"), ("missing_required", "tags := [a];\n"), ("all_wrong", "id := 0.5;\ntitle := 1;\ntags := [aaaa, bbbb, c];\nmeta := { z := 1; };\nextra := 1;\n"),
         ("order", "meta := { k := 1; };\nextra := 1;\ntitle := \"toolongtitle\";\nid := 0;\n"), ("dup_statement", "id := 1;\nid := 2;\ntitle := \"x\";\n"), ("syntax_error", "id := ;\n"), ("comments", "// head\nid := 1; /* inline */\ntitle := \"t\"; // tail\n"),
         ("block_form", "id := 1;\ntitle := \"t\";\nmeta { k := \"v\"; };\n"), ("with_include", "include \"extra.anvl\";\nid := 1;\ntitle := \"t\";\n"), ("variable_reference", "base := 1;\nid := $base;\ntitle := \"t\";\n"),
         ("variable_unresolved", "id := $nope;\ntitle := \"t\";\n"), ("base_inheritance", "d := { k := \"v\"; };\nmeta : d := { z := 1; };\nid := 1;\ntitle := \"t\";\n"), ("blob_values", "id := 1;\ntitle := `blob`;\n"), ("bom_crlf", "id := 1;\r\ntitle := \"t\";\r\n"),
         ("null_values", "id := null;\ntitle := null;\n"), ("shebang_amp", "#!amp\nid := 1;\ntitle := \"t\";\n"), ("many_tags", "id := 1;\ntitle := \"t\";\ntags := " + "[" + ", ".join("abc" for _ in range(30)) + "];\n")]
for n, d in DATAS:
    case(DT, "doc_" + n, [("s.anvl", sch), ("d.anvl", ("#!aml\n" + d) if not d.startswith("#!") else d), ("extra.anvl", "#!aml\nincluded := 1;\n")], ['schema s load "s.anvl"', 'validate s "d.anvl"', 'violations s'])
case(DT, "doc_missing_file", [("s.anvl", sch)], ['schema s load "s.anvl"', 'validate s "gone.anvl"', 'violations s'])
case(DT, "doc_is_schema_itself", [("s.anvl", sch)], ['schema s load "s.anvl"', 'validate s "s.anvl"'])
case(DT, "repeated_validation", [("s.anvl", sch), ("good.anvl", "#!aml\nid := 1;\ntitle := \"t\";\n"), ("bad.anvl", "#!aml\nid := 0;\n")],
     ['schema s load "s.anvl"', 'validate s "bad.anvl"', 'validate s "good.anvl"', 'violations s', 'validate s "bad.anvl"', 'validate_value s "{ id := 1; title := \\"t\\"; }"', 'violations s'])
case(DT, "two_schemas_do_not_share_violations", [("s.anvl", sch), ("bad.anvl", "#!aml\nid := 0;\n")], ['schema a load "s.anvl"', 'schema b load "s.anvl"', 'validate a "bad.anvl"', 'violations b', 'validate b "bad.anvl"', 'violations a'])
case(DT, "doc_with_schema_attr", [("s.anvl", sch), ("d.anvl", "#!aml\n@[schema]\nid := 1;\ntitle := \"t\";\n")], ['schema s load "s.anvl"', 'validate s "d.anvl"'])
case(DT, "doc_many_statements", [("s.anvl", SCHEMA + "n := { type := Numeric; };\n"), ("d.anvl", "#!aml\n" + "".join("k%d := %d;\n" % (i, i) for i in range(200)))], ['schema s load "s.anvl"', 'validate s "d.anvl"'])

# lifetime / null handles
LT = "lifetime"
case(LT, "violations_persist", [("s.anvl", SCHEMA + "a := { type := Numeric; required := true; };\n")], ['schema s load "s.anvl"', 'violations s', vv("s", "{ zz := 1; }"), 'violations s', 'violations s', vv("s", "{ a := 1; }"), 'violations s'])
case(LT, "failed_call_leaves_violations", [("s.anvl", SCHEMA + "a := { type := Numeric; required := true; };\n")], ['schema s load "s.anvl"', vv("s", "{ zz := 1; }"), 'validate_value s NULL', 'violations s', vv("s", "{ a := ", "skip_required"), 'violations s', 'validate s "gone.anvl"', 'violations s'])
case(LT, "null_handles", [], ['violations NULL', 'validate NULL "gone.anvl"', 'validate_value NULL "{ a := 1; }"', 'validate_value NULL NULL', 'field NULL "a"', 'field NULL "a" "b"', 'type find NULL "A"', 'type resolve NULL "Numeric"', 'type resolve NULL "A"'])
case(LT, "failed_load_binds_null", [], ['registry r load "gone.anvl"', 'schema s load "gone.anvl"', 'type find r "A"', 'violations s', vv("s", "{ a := 1; }"), 'field s "a"'])
case(LT, "load_failed_schema_then_good", [("s.anvl", SCHEMA + "a := { type := Numeric; };\n")], ['schema bad load "gone.anvl"', 'schema good load "s.anvl"', vv("bad", "{ a := 1; }"), vv("good", "{ a := \"x\"; }"), 'violations bad', 'violations good'])
case(LT, "many_validations_one_schema", [("s.anvl", SCHEMA + "a := { type := Numeric; min := 3; };\n")], ['schema s load "s.anvl"'] + [vv("s", "{ a := %d; }" % i) for i in range(12)] + ['violations s'])
case(LT, "registry_and_schema_same_file", [("t.anvl", '#!aml\n@[types, schema]\nT := { type := String; };\n')], ['registry r load "t.anvl"', 'schema s load "t.anvl"', 'type find r "T"', 'field s "T"'], desc="a document with both attributes")
case(LT, "schema_and_types_same_file_other_order", [("t.anvl", '#!aml\n@[schema, types]\nT := { type := String; };\n')], ['registry r load "t.anvl"', 'schema s load "t.anvl"', 'field s "T"'])

# --------------------------------------------------------------------------------------------------------------
# FlyWire shapes
# --------------------------------------------------------------------------------------------------------------
FW = "flywire"
PG_TYPES = """#!aml
@[types]
Int32 := {
    type := Numeric;
    min := -2147483648;
    max := 2147483647;
    integer := true;
};
Str := {
    type := String;
};
Date := {
    type := String;
    format := "iso8601_date";
};
Int64 := {
    type := Numeric;
};
Timestamptz := {
    type := String;
    format := "iso8601_datetime";
};
Timestamp := {
    type := Numeric;
};
Text := {
    type := String;
};
Bool := {
    type := Numeric;
};
Float64 := {
    type := Numeric;
    min := -1.7976931348623157e+308;
    max := 1.7976931348623157e+308;
};
Uuid := {
    type := String;
    format := "uuid";
};
"""
ASSETS = """#!aml
include "schema-gen/postgres.types.anvl";

@[schema, table=assets]

asset_id      { type := types.Int32; indexBy := 1; };
asset_type    { type := types.Int32; required := true; };
vin_serial    { type := types.Str; size := 17; ascii := true; required := true; };
make          { pooled := true; size := 64; required := true; };
model         { pooled := true; size := 64; required := true; };
year          { type := types.Int32; required := true; };
license_plate { pooled := true; size := 16; nullable := true; };
state         { type := types.Str; size := 2; ascii := true; nullable := true; };
fleet_id      { pooled := true; size := 32; required := true; };
status        { pooled := true; size := 32; required := true; values := ["active", "maintenance", "out_of_service"]; };
created_at    { type := types.Timestamptz; };
updated_at    { type := types.Timestamptz; };
"""
FWF = [("schema-gen/postgres.types.anvl", PG_TYPES), ("assets.meta.anvl", ASSETS)]
GOOD = {"asset_id": "7", "asset_type": "2", "vin_serial": '"1HGCM82633A004352"', "make": '"Volvo"', "model": '"VNL"', "year": "2024", "license_plate": '"ABC123"', "state": '"TX"', "fleet_id": '"F-1"', "status": '"active"',
        "created_at": '"2026-10-26T12:00:00.123456Z"', "updated_at": '"2026-10-26T12:00:00Z"'}


def fwrec(**over):
    d = dict(GOOD)
    for k, v in over.items():
        if v is None:
            d.pop(k, None)
        else:
            d[k] = v
    return "{ " + " ".join("%s := %s;" % (k, v) for k, v in d.items()) + " }"


case(FW, "assets_load", FWF, ['schema s load "assets.meta.anvl"'])
case(FW, "types_registry", FWF, ['registry r load "schema-gen/postgres.types.anvl"'] + ['type find r "%s"' % n for n in "Int32 Str Date Int64 Timestamptz Timestamp Text Bool Float64 Uuid Nope".split()])
case(FW, "assets_valid", FWF, ['schema s load "assets.meta.anvl"', vv("s", fwrec()), vv("s", fwrec(), "skip_required")])
case(FW, "assets_required_missing", FWF, ['schema s load "assets.meta.anvl"', vv("s", "{ zz := 1; }"), vv("s", "{ zz := 1; }", "skip_required"), vv("s", fwrec(asset_type=None, year=None, make=None, status=None, fleet_id=None, vin_serial=None, model=None))])
case(FW, "assets_unknown_field", FWF, ['schema s load "assets.meta.anvl"', vv("s", fwrec(nope="1")), vv("s", fwrec(nope="1"), "allow_unknown"), vv("s", fwrec(asset_id=None, Make='"x"'))])
# int32 / int64 boundaries
I32 = ["0", "-1", "2147483647", "2147483648", "-2147483648", "-2147483649", "4294967296", "9223372036854775807", "9223372036854775808", "2.5", "2.0", "1e3", "1e10", "-0", "1.0e0", '"7"', "true", "null", "[1]", "007", "2147483647.0", "2147483648.0", "-2147483648.5"]
case(FW, "int32_validation", FWF, ['schema s load "assets.meta.anvl"'] + [vv("s", fwrec(year=v), "skip_required") for v in I32], desc="year is types.Int32")
case(FW, "int32_validation_asset_id", FWF, ['schema s load "assets.meta.anvl"'] + [vv("s", "{ asset_id := %s; }" % v, "skip_required") for v in I32])
I64 = ["0", "9007199254740991", "9007199254740992", "9007199254740993", "9223372036854775807", "9223372036854775808", "-9223372036854775808", "-9223372036854775809", "1.5", "1e3", "1e30", '"9223372036854775807"', '"1"', '"x"', "true", "null", "123456789012345678901234567890"]
case(FW, "int64_validation", [("schema-gen/postgres.types.anvl", PG_TYPES), ("s.anvl", '#!aml\ninclude "schema-gen/postgres.types.anvl";\n@[schema, table=t]\nbig { type := types.Int64; };\nbig_req { type := types.Int64; required := true; };\nf64 { type := types.Float64; };\nts { type := types.Timestamp; };\nflag { type := types.Bool; };\n')],
     ['schema s load "s.anvl"'] + [vv("s", "{ big := %s; }" % v, "skip_required") for v in I64] + [vv("s", "{ f64 := %s; }" % v, "skip_required") for v in ["0", "1.5", "1e308", "1.7976931348623157e308", "1.7976931348623159e308", "1e999", "-1e999", "5e-324", '"NaN"', '"Infinity"', "NaN", "Infinity", "true"]]
     + [vv("s", "{ ts := %s; flag := %s; }" % (a, b), "skip_required") for a, b in [("1700000000", "1"), ('"2026-10-26T12:00:00"', "0"), ("1.5", "2"), ("true", "true"), ("null", "null")]])
case(FW, "str_size_bytes_vs_characters", FWF, ['schema s load "assets.meta.anvl"'] + [vv("s", fwrec(state=q(v)), "skip_required") for v in ["TX", "T", "", "Té", "é", "日", "日本", "\U0001f600", "abc", "A\\nB"]] +
     [vv("s", fwrec(vin_serial=q(v)), "skip_required") for v in ["12345678901234567", "123456789012345678", "é" * 8, "é" * 9, "é" * 8 + "x", "é" * 8 + "xy"]], desc="size counts bytes, not characters")
case(FW, "status_values", FWF, ['schema s load "assets.meta.anvl"'] + [vv("s", fwrec(status=v)) for v in ['"active"', '"maintenance"', '"out_of_service"', '"Active"', '"retired"', '""', "active", "null", "1", '"active "']])
case(FW, "nullable_fields", FWF, ['schema s load "assets.meta.anvl"', vv("s", fwrec(license_plate="null", state="null")), vv("s", fwrec(make="null")), vv("s", fwrec(asset_id="null", created_at="null")), vv("s", fwrec(year="null"))])
case(FW, "timestamptz_formats", FWF, ['schema s load "assets.meta.anvl"'] + [vv("s", fwrec(created_at=q(v)), "skip_required") for v in ["2026-10-26T12:00:00.123456Z", "2026-10-26T12:00:00Z", "2026-10-26T12:00:00+00:00", "2026-10-25 09:03:00.5-05", "2026-10-25 09:03:00Z", "2026-10-25T09:03:00-05", "2026-10-25T09:03:00", "x", "", "2026-10-26T12:00:00.1234567Z", "2026-02-29T00:00:00Z"]])
case(FW, "date_formats", [("schema-gen/postgres.types.anvl", PG_TYPES), ("s.anvl", '#!aml\ninclude "schema-gen/postgres.types.anvl";\n@[schema, table=t]\nd { type := types.Date; };\nu { type := types.Uuid; };\ntx { type := types.Text; };\n')],
     ['schema s load "s.anvl"'] + [vv("s", "{ d := %s; }" % q(v)) for v in ["2023-02-28", "2023-02-29", "x", "2026-13-01", "2024-02-29", ""]] + [vv("s", "{ u := %s; tx := %s; }" % (q(a), q(b))) for a, b in [("550e8400-e29b-41d4-a716-446655440000", "t"), ("nope", ""), ("550E8400-E29B-41D4-A716-446655440000", "é")]])
case(FW, "repeated_key_is_a_parse_error", FWF, ['schema s load "assets.meta.anvl"', vv("s", '{ make := "a"; make := "b"; }', "skip_required"), vv("s", '{ make := "a"; model := "m"; make := "b"; }'), vv("s", '{ asset_id := 1; asset_id := 1; }', "skip_required"), vv("s", '{ make := "a"; }', "skip_required")],
     desc="a key repeated in a record is 4001 at parse: the fragment never reaches the schema")
case(FW, "repeated_key_in_document", FWF + [("d.anvl", "#!aml\nmake := \"a\";\nmake := \"b\";\n")], ['schema s load "assets.meta.anvl"', 'validate s "d.anvl"', 'violations s'])
case(FW, "pooled_extras", [("schema-gen/postgres.types.anvl", PG_TYPES), ("p.anvl", '#!aml\ninclude "schema-gen/postgres.types.anvl";\n@[schema, table=pooled_fixture]\n\nid     { type := types.Int32; };\nvin    { type := types.Str; size := 17; ascii := true; };\nmake   { pooled := true; };\nstatus { pooled := true; };\n')],
     ['schema s load "p.anvl"', vv("s", '{ id := 1; vin := "x"; make := 5; status := [1]; }'), vv("s", '{ make := null; status := { a := 1; }; }')])
case(FW, "bounds_fixture", [("schema-gen/postgres.types.anvl", PG_TYPES), ("b.anvl", '#!aml\ninclude "schema-gen/postgres.types.anvl";\n@[schema, table=bounds_fixture]\n\nat_most  { type := types.Int32; max := 4; };\nat_least { type := types.Int32; min := 1; };\nbetween  { type := types.Int32; min := 1; max := 4; };\n')],
     ['schema s load "b.anvl"'] + [vv("s", "{ at_most := %s; at_least := %s; between := %s; }" % (v, v, v)) for v in ["0", "1", "4", "5", "-2147483648", "2147483647", "2147483648", "2.5", "3"]])
case(FW, "legacy_table_schema", [("v.anvl", "#!aml\n\nTableSchema := {\n    vehicle_id    := { type := int32 }\n    vin           := { type := str, size := 17 }\n    year          := { type := int32 }\n}\n")], ['schema s load "v.anvl"', 'registry r load "v.anvl"'],
     desc="the pre-types file shape of FlyWire's vehicles fixture: not a schema document")
case(FW, "legacy_semicolon_schema", [("v.anvl", "#!aml\n@[schema]\nvehicle_id := { type := int32; };\nvin := { type := str; size := 17; };\n")], ['schema s load "v.anvl"', vv("s", "{ vehicle_id := 1; vin := \"x\"; }")],
     desc="int32 and str are not native type names")
case(FW, "typo_in_types_reference", [("schema-gen/postgres.types.anvl", PG_TYPES), ("s.anvl", '#!aml\ninclude "schema-gen/postgres.types.anvl";\n@[schema]\nasset_id { type := types.Int23; };\n')], ['schema s load "s.anvl"', vv("s", "{ asset_id := 1; }")],
     desc="a schema that reports a diagnostic would validate less than it says: FlyWire refuses it")
case(FW, "crosscheck_set_boundaries", FWF, ['schema s load "assets.meta.anvl"'] + [vv("s", fwrec(**{k: v}), o) for k, v, o in
     [("year", "2024", "skip_required"), ("year", "2024.5", "skip_required"), ("make", '"%s"' % ("m" * 64), "skip_required"), ("make", '"%s"' % ("m" * 65), "skip_required"), ("fleet_id", '"%s"' % ("f" * 32), "skip_required"), ("fleet_id", '"%s"' % ("f" * 33), "skip_required"),
      ("license_plate", '"%s"' % ("p" * 17), "skip_required"), ("state", '"TXX"', "skip_required"), ("asset_id", "null", "skip_required"), ("status", "null", "skip_required"), ("created_at", "5", "skip_required"), ("asset_type", "true", "skip_required")]])
case(FW, "set_batch_elements", FWF, ['schema s load "assets.meta.anvl"', vv("s", fwrec()), vv("s", fwrec(year="1.5")), vv("s", fwrec(make=None)), vv("s", "{ zz := 1; }", "skip_required"), 'violations s'],
     desc="validating the elements of set := [ {...}, {...} ] one by one, reading the violations between calls")
case(FW, "condition_leaf_one_field", FWF, ['schema s load "assets.meta.anvl"'] + [vv("s", "{ %s := %s; }" % (f, v), "skip_required") for f, v in [("year", "2024"), ("year", "-1"), ("status", '"retired"'), ("make", '"x"'), ("fleet_id", "null"), ("asset_id", "2147483648")]])

# --------------------------------------------------------------------------------------------------------------
# Rivet shapes
# --------------------------------------------------------------------------------------------------------------
RV = "rivet"
RTYPES = """#!aml
// types/rivet.types.anvl: shared types for the kind schemas in schemas/.
@[types]

Title := { type := String; size := 300; };
Summary := { type := String; size := 4000; };
Tag := { type := String; size := 64; };
Slug := { type := String; size := 63; };
Handle := { type := String; size := 63; };
IdCode := { type := String; size := 63; };
TicketId := { type := String; size := 120; };
Timestamp := { type := String; format := iso8601_datetime; };
Priority := { type := enum; values := [critical, high, normal, low]; };
Severity := { type := enum; values := [critical, major, minor, trivial]; };
"""
COMMON = """include "../types/rivet.types.anvl";
@[schema]

title := { type := types.Title; required := true; };
project := { type := types.Slug; required := true; };
code := { type := types.IdCode; };
summary := { type := types.Summary; };
tags := { type := Array; items := { type := types.Tag; }; };
priority := { type := types.Priority; };
"""
PEOPLE = """filed_by := { type := types.Handle; };
requested_by := { type := types.Handle; };
assignee := { type := types.Handle; };
"""


def sections(*names):
    return "sections := {\n   type := Object;\n   fields := {\n" + "".join("      %s := { type := String; };\n" % n for n in names) + "   };\n};\n"


KINDSCH = {
    "BR": "#!aml\n" + COMMON + "severity := { type := types.Severity; };\n" + PEOPLE + sections("reproduction", "impact", "workaround", "root_cause", "test_cases", "notes"),
    "FR": "#!aml\n" + COMMON + PEOPLE + sections("motivation", "design", "test_cases", "non_goals", "notes"),
    "SR": "#!aml\n" + COMMON + PEOPLE + sections("motivation", "design", "test_cases", "notes"),
    "TASK": "#!aml\n" + COMMON + PEOPLE + sections("description", "reason", "notes"),
}
RIVET_FILES = [("types/rivet.types.anvl", RTYPES)] + [("schemas/%s.schema.anvl" % k, v) for k, v in KINDSCH.items()] + [
    ("schemas/actions/comment.schema.anvl", "#!aml\ninclude \"../../types/rivet.types.anvl\";\n@[schema]\n\nid := { type := types.TicketId; required := true; };\nbody := { type := String; required := true; };\ninternal := { type := Bool; };\n")]
for k in KINDSCH:
    case(RV, k.lower() + "_load", RIVET_FILES, ['schema s load "schemas/%s.schema.anvl"' % k])
case(RV, "types_registry", RIVET_FILES, ['registry r load "types/rivet.types.anvl"'] + ['type find r "%s"' % n for n in "Title Summary Tag Slug Handle IdCode TicketId Timestamp Priority Severity".split()])
case(RV, "comment_action", RIVET_FILES, ['schema s load "schemas/actions/comment.schema.anvl"', vv("s", '{ id := "BR-2610-anvl-001"; body := "x"; internal := true; }'), vv("s", '{ id := 5; body := 1; internal := "no"; extra := 1; }'), vv("s", '{ zz := 1; }'), vv("s", '{ body := "x"; }', "skip_required")])
ticket = '{ title := "Null bitmap off by one"; project := "flywire"; priority := high; severity := major; tags := [codec, nulls]; sections := { reproduction := "steps"; impact := "bad"; }; }'
case(RV, "br_valid", RIVET_FILES, ['schema s load "schemas/BR.schema.anvl"', vv("s", ticket), vv("s", ticket, "skip_required"), vv("s", '{ title := "x"; project := "p"; }')])
case(RV, "br_invalid", RIVET_FILES, ['schema s load "schemas/BR.schema.anvl"'] + [vv("s", t) for t in [
    '{ zz := 1; }', '{ title := "x"; }', '{ title := "x"; project := "p"; priority := urgent; }', '{ title := "x"; project := "p"; severity := high; }', '{ title := "x"; project := "p"; tags := [ok, "%s"]; }' % ("t" * 65),
    '{ title := "x"; project := "p"; sections := { design := "no"; }; }', '{ title := "x"; project := "p"; sections := { reproduction := 5; }; }', '{ title := "x"; project := "p"; unknown := 1; }',
    '{ title := "%s"; project := "p"; }' % ("t" * 301), '{ title := "x"; project := "%s"; }' % ("p" * 64), '{ title := "x"; project := "p"; tags := "one"; }', '{ title := 5; project := 6; code := 7; }',
    '{ title := "x"; project := "p"; sections := [1]; }', '{ title := null; project := "p"; }', '{ title := "x"; project := "p"; assignee := "%s"; }' % ("a" * 64)]])
case(RV, "edit_with_skip_required", RIVET_FILES, ['schema s load "schemas/FR.schema.anvl"'] + [vv("s", t, "skip_required") for t in ['{ zz := 1; }', '{ priority := low; }', '{ priority := nope; }', '{ title := "new"; }', '{ sections := { notes := "n"; }; }', '{ code := "C-1"; unknown := 1; }']])
case(RV, "all_kinds_one_run", RIVET_FILES, sum([['schema %s load "schemas/%s.schema.anvl"' % (k.lower(), k), vv(k.lower(), '{ title := "t"; project := "p"; sections := { notes := "n"; design := "d"; description := "x"; reproduction := "r"; }; }')] for k in KINDSCH], []))
case(RV, "timestamp_type_in_a_field", RIVET_FILES + [("schemas/stamp.schema.anvl", '#!aml\ninclude "../types/rivet.types.anvl";\n@[schema]\nat := { type := types.Timestamp; required := true; };\nuntil := { type := types.Timestamp; nullable := true; };\n')],
     ['schema s load "schemas/stamp.schema.anvl"'] + [vv("s", "{ at := %s; until := null; }" % q(v)) for v in ["2026-10-31T10:00:00Z", "2026-10-31 10:00:00Z", "2026-10-31T10:00:00+01:00", "2026-10-31", ""]])
case(RV, "enum_types_in_fields", RIVET_FILES + [("schemas/e.schema.anvl", '#!aml\ninclude "../types/rivet.types.anvl";\n@[schema]\np := { type := types.Priority; };\ns := { type := types.Severity; };\nl := { type := Array; items := { type := types.Priority; }; };\n')],
     ['schema s load "schemas/e.schema.anvl"', vv("s", "{ p := critical; s := major; l := [low, normal, bad]; }"), vv("s", '{ p := "high"; s := "Major"; l := [ "low", 1 ]; }')])

# --------------------------------------------------------------------------------------------------------------
os.makedirs(out_dir, exist_ok=True)
seen = set()
for area, name, text in cases:
    assert (area, name) not in seen, (area, name)
    seen.add((area, name))
    d = os.path.join(out_dir, area)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name + ".sscript"), "w", newline="") as f:
        f.write(text)
if force:  # remove scripts that are no longer generated (never a .dump)
    for dirpath, _, files in os.walk(out_dir):
        for fn in files:
            if fn.endswith(".sscript") and (os.path.basename(dirpath), fn[:-len(".sscript")]) not in seen:
                os.remove(os.path.join(dirpath, fn))
print("gen_schema: %d scripts" % len(cases))
