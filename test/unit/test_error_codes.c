/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_error_codes.c - BR-2610-anvl-parser-005: grammar consistency      *
 * ---------------------------------------------------------------------- *
 * Every case loads one buffer through the whole pipeline (anvil_load_    *
 * buffer) and checks the first recorded error: its internal code, the    *
 * public category, and the exact line and column. A case whose code is   *
 * ANVL_ERR_NONE must load without any error.                             *
 *                                                                        *
 *   Part 1  duplicate object fields and attribute keys are errors        *
 *   Part 2  a non-object base is an error                                *
 *   Part 3  rules native enforces under their own code                   *
 *   Part 4  the specific code where a generic one was reported           *
 *   Part 5  what stays as it was (characterization)                      *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_types.h"
#include "errors.h"
#include "error_code_pins.h" // generated from conformance/drift-tables.txt by test/unit/Makefile
#include "internal/flat_internal.h"
#include "internal/module.h"
#include "testbit.h"
// ----------------
#include <sigma/list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
   const char *name;
   const char *src;
   size_t len;
   int code;           // first recorded error, or ANVL_ERR_NONE for "loads clean"
   int category;       // anvil_get_error(); ANVIL_OK when clean
   size_t line, col;   // 1-based, of the first recorded error
} ec_case;

#define CASE_OK(name, src) {name, src, sizeof(src) - 1, ANVL_ERR_NONE, ANVIL_OK, 0, 0}
#define CASE_ERR(name, src, code, cat, line, col) {name, src, sizeof(src) - 1, code, cat, line, col}

static void th(void) {}

static void check(const ec_case *c) {
   char label[160];
   anvil_document doc = anvil_load_buffer(c->src, c->len);
   snprintf(label, sizeof label, "%s: loads", c->name);
   TestBit.is_not_null(doc, label);
   if (!doc) {
      return;
   }
   module_context ctx = anvil_internal_document_context(doc);
   anvl_error e = NULL;
   if (ctx && ctx->errors && List.size(ctx->errors) > 0) {
      e = anvl_error_get(ctx->errors, 0);
   }
   if (c->code == ANVL_ERR_NONE) {
      snprintf(label, sizeof label, "%s: no error", c->name);
      TestBit.is_false(anvil_has_errors(doc), label);
      anvil_dispose(doc);
      return;
   }
   snprintf(label, sizeof label, "%s: an error is recorded", c->name);
   TestBit.is_true(e != NULL, label);
   snprintf(label, sizeof label, "%s: code %d", c->name, c->code);
   TestBit.is_equal_int(c->code, e ? (long long)e->code : -1, label);
   snprintf(label, sizeof label, "%s: category", c->name);
   TestBit.is_equal_int(c->category, (long long)anvil_get_error(doc), label);
   snprintf(label, sizeof label, "%s: line %zu", c->name, c->line);
   TestBit.is_equal_int((long long)c->line, e ? (long long)e->line : -1, label);
   snprintf(label, sizeof label, "%s: column %zu", c->name, c->col);
   TestBit.is_equal_int((long long)c->col, e ? (long long)e->column : -1, label);
   anvil_dispose(doc);
}

static void run(const ec_case *cases, size_t n) {
   for (size_t i = 0; i < n; i++) {
      check(&cases[i]);
   }
}
#define RUN(arr) run(arr, sizeof(arr) / sizeof(arr[0]))

#define SYN ANVIL_ERR_SYNTAX
#define HDR ANVIL_ERR_HEADER
#define RES ANVIL_ERR_RESOLVE

/* ====================================================================== *
 * Part 1: duplicate keys
 * ====================================================================== */
static void test_p1_duplicate_object_fields(void) {
   static const ec_case cases[] = {
      CASE_ERR("P1.01 assign form", "x := { a := 1; a := 2; };", 4001, SYN, 1, 16),
      CASE_ERR("P1.02 nested, multi-line", "x := {\n  a := 1;\n  b := {\n    c := 1;\n    c := 2;\n  };\n};", 4001, SYN, 5, 5),
      CASE_ERR("P1.03 block form", "x {\n  a := 1;\n  a := 3;\n};", 4001, SYN, 3, 3),
      CASE_ERR("P1.04 assign then block", "x := { a := 1; a { b := 1; }; };", 4001, SYN, 1, 16),
      CASE_ERR("P1.05 reported before a later error in the same statement", "x := { a := 1; a := ; };", 4001, SYN, 1, 16),
      CASE_ERR("P1.06 third occurrence reports the second", "x := { a := 1; b := 1; a := 2; a := 3; };", 4001, SYN, 1, 24),
      CASE_OK("P1.07 names are case sensitive", "x := { a := 1; A := 2; };"),
      CASE_OK("P1.08 same name in sibling objects", "p := { a := 1; };\nq := { a := 1; };"),
      CASE_OK("P1.09 same name at two depths", "x := { a := { a := 1; }; };"),
      CASE_OK("P1.10 override of an inherited field", "b := { x := 1; };\nd : b := { x := 2; y := 3; };"),
      CASE_ERR("P1.11 top-level repeat stays 4053", "a := 1;\na := 2;", 4053, RES, 2, 1),
   };
   RUN(cases);
}
static void test_p1_duplicate_object_fields_large(void) {
   // more fields than a small linear scan: the duplicate of f3 is the 21st statement
   char buf[2048];
   size_t n = (size_t)snprintf(buf, sizeof buf, "x := {\n");
   for (int i = 0; i < 20; i++) {
      n += (size_t)snprintf(buf + n, sizeof buf - n, "  f%d := 1;\n", i);
   }
   n += (size_t)snprintf(buf + n, sizeof buf - n, "  f3 := 2;\n};");
   ec_case c = {"P1.12 duplicate among 21 fields", buf, n, 4001, SYN, 22, 3};
   check(&c);
   n = (size_t)snprintf(buf, sizeof buf, "x := {\n");
   for (int i = 0; i < 40; i++) {
      n += (size_t)snprintf(buf + n, sizeof buf - n, "  f%d := 1;\n", i);
   }
   n += (size_t)snprintf(buf + n, sizeof buf - n, "};");
   ec_case ok = {"P1.13 40 distinct fields", buf, n, ANVL_ERR_NONE, ANVIL_OK, 0, 0};
   check(&ok);
}
static void test_p1_duplicate_attribute_keys(void) {
   static const ec_case cases[] = {
      CASE_ERR("P1.20 key=value twice", "x @[a=1, a=2] := 1;", 4013, SYN, 1, 10),
      CASE_ERR("P1.21 flag twice", "x @[f, f] := 1;", 4013, SYN, 1, 8),
      CASE_ERR("P1.22 multi-line list", "x @[\n  a,\n  b,\n  a\n] := 1;", 4013, SYN, 4, 3),
      CASE_ERR("P1.23 on a nested statement", "x := { y @[k, k] := 1; };", 4013, SYN, 1, 15),
      CASE_ERR("P1.24 flag then valued", "x @[a, a=1] := 1;", 4013, SYN, 1, 8),
      CASE_ERR("P1.25 module attributes", "@[a=1, a=2]\nx := 1;", 4013, HDR, 1, 8),
      CASE_ERR("P1.26 module attributes across two blocks", "@[a]\n@[a]\nx := 1;", 4013, HDR, 2, 3),
      CASE_OK("P1.27 keys are case sensitive", "x @[a, A] := 1;"),
      CASE_OK("P1.28 same key on different statements", "x @[a] := 1;\ny @[a] := 2;"),
      CASE_OK("P1.29 module and statement may share a key", "@[a]\nx @[a] := 1;"),
   };
   RUN(cases);
}

/* ====================================================================== *
 * Part 2: a non-object base
 * ====================================================================== */
static void test_p2_non_object_base(void) {
   static const ec_case cases[] = {
      CASE_ERR("P2.01 numeric", "a := 1;\nb : a := { c := 1; };", 4020, RES, 2, 5),
      CASE_ERR("P2.02 string", "a := \"s\";\nb : a := { c := 1; };", 4020, RES, 2, 5),
      CASE_ERR("P2.03 array", "a := [1];\nb : a := { c := 1; };", 4020, RES, 2, 5),
      CASE_ERR("P2.04 tuple", "a := (1, 2);\nb : a := { c := 1; };", 4020, RES, 2, 5),
      CASE_ERR("P2.05 blob", "a := @t`z`;\nb : a := { c := 1; };", 4020, RES, 2, 5),
      CASE_ERR("P2.06 bool", "a := true;\nb : a := { c := 1; };", 4020, RES, 2, 5),
      CASE_ERR("P2.07 null", "a := null;\nb : a := { c := 1; };", 4020, RES, 2, 5),
      CASE_ERR("P2.08 bare word", "a := word;\nb : a := { c := 1; };", 4020, RES, 2, 5),
      CASE_ERR("P2.09 varref", "z := { q := 1; };\na := $z;\nb : a := { c := 1; };", 4020, RES, 3, 5),
      CASE_ERR("P2.10 base declared after the derived statement", "b : a := { c := 1; };\na := 1;", 4020, RES, 1, 5),
      CASE_ERR("P2.11 nested derived statement is 4022 first", "a := 1;\nx := { b : a := { c := 1; }; };", 4022, SYN, 2, 12),
      CASE_ERR("P2.12 block-form derived statement", "a := 1;\nb : a { c := 1; };", 4020, RES, 2, 5),
      CASE_OK("P2.13 object base", "a := { x := 1; };\nb : a := { y := 2; };"),
      CASE_OK("P2.14 chain of object bases", "a := { x := 1; };\nb : a := { y := 2; };\nc : b := { z := 3; };"),
      CASE_ERR("P2.15 anonymous base stays 4052", "a { x := 1; };\nb : a := { y := 2; };", 4052, RES, 0, 0),
      CASE_ERR("P2.16 missing base stays 4051", "b : zz := { c := 1; };", 4051, RES, 0, 0),
      CASE_ERR("P2.17 non-object value after a base stays 4020 (parse time)", "a := { x := 1; };\nb : a := 5;", 4020, SYN, 2, 10),
   };
   RUN(cases);
}

static void test_p7_nested_base_message(void) {
   const char *m = anvl_error_code_message(ANVL_ERR_PARSER_NESTED_BASE);
   TestBit.is_equal_int(4022, ANVL_ERR_PARSER_NESTED_BASE, "P7.code: ANVL_ERR_PARSER_NESTED_BASE is 4022");
   TestBit.is_true(m && strcmp(m, "Only a top-level statement can have a base") == 0, "P7.message: the text");
   const char *n = anvl_error_code_name(ANVL_ERR_PARSER_NESTED_BASE);
   TestBit.is_true(n && strcmp(n, "ANVL_ERR_PARSER_NESTED_BASE") == 0, "P7.name: the enum identifier");
}

/* ====================================================================== *
 * Part 7: only a top-level statement has a base (4022; BR-2611-anvl-resolver-001)
 * ====================================================================== */
static void test_p7_nested_base(void) {
   static const ec_case cases[] = {
      CASE_ERR("P7.01 the reproduction: a block on its enclosing statement", "s1 := { f3 : s1 { a := 1; }; };", 4022, SYN, 1, 14),
      CASE_ERR("P7.02 assign form", "x := { b : a := { c := 1; }; };", 4022, SYN, 1, 12),
      CASE_ERR("P7.03 block form", "x := { b : a { c := 1; }; };", 4022, SYN, 1, 12),
      CASE_ERR("P7.04 depth 3, assign form", "x := { y := { z : a := { c := 1; }; }; };", 4022, SYN, 1, 19),
      CASE_ERR("P7.05 depth 3, block form", "x := { y := { z : a { c := 1; }; }; };", 4022, SYN, 1, 19),
      CASE_ERR("P7.06 inside a block-form statement", "x { y : a { c := 1; }; };", 4022, SYN, 1, 9),
      CASE_ERR("P7.07 the base names an existing top-level object", "a := { q := 1; };\nx := { b : a := { c := 1; }; };", 4022, SYN, 2, 12),
      CASE_ERR("P7.08 the base names a missing name: 4022 wins over 4051", "x := { b : nothere { c := 1; }; };", 4022, SYN, 1, 12),
      CASE_ERR("P7.09 refused before the rest of the statement is read", "x := { b : a := 1; };", 4022, SYN, 1, 12),
      CASE_ERR("P7.12 a ':' with no name after it is not a base", "x := { b : := 1; };", 2001, SYN, 1, 12),
      CASE_ERR("P7.13 a JSON-style colon stays the expected-identifier error", "x := { a: 1; };", 2001, SYN, 1, 11),
      CASE_ERR("P7.10 after other fields, on a later line", "a := { q := 1; };\nx := {\n  f := 1;\n  b : a { c := 1; };\n};", 4022, SYN, 4, 7),
      CASE_ERR("P7.11 in a sibling of a valid top-level base", "a := { q := 1; };\nb : a := { r := 2; };\nc := { d : a := { s := 3; }; };", 4022, SYN, 3, 12),
      CASE_OK("P7.20 a top-level base, assign form", "a := { q := 1; };\nb : a := { r := 2; };"),
      CASE_OK("P7.21 a top-level base, block form", "a := { q := 1; };\nb : a { r := 2; };"),
      CASE_OK("P7.22 a nested object without a base", "a := { q := 1; };\nb : a := { r := { s := 2; }; };"),
   };
   RUN(cases);
}

/* ====================================================================== *
 * Part 3: rules enforced under their own code
 * ====================================================================== */
static void test_p3_multiple_shebang(void) {
   static const ec_case cases[] = {
      CASE_ERR("P3.01 second shebang in the header", "#!aml\n#!aml\nx := 1;", 2004, HDR, 2, 1),
      CASE_ERR("P3.02 second shebang after statements", "#!aml\nx := 1;\n#!aml\ny := 2;", 2004, SYN, 3, 1),
      CASE_ERR("P3.03 second shebang, other dialect token", "#!aml\n#!amp\nx := 1;", 2004, HDR, 2, 1),
      CASE_ERR("P3.04 shebang after statements, none first", "x := 1;\n#!aml\ny := 2;", 2005, SYN, 2, 1),
      CASE_OK("P3.05 one shebang", "#!aml\nx := 1;"),
   };
   RUN(cases);
}
static void test_p3_module_attributes_after_statements(void) {
   static const ec_case cases[] = {
      CASE_ERR("P3.10 attributes then a statement", "x := 1;\n@[a] y := 2;", 3015, SYN, 2, 1),
      CASE_ERR("P3.11 attributes alone", "x := 1;\n@[a];", 3015, SYN, 2, 1),
      CASE_ERR("P3.12 after several statements", "x := 1;\ny := 2;\n  @[a]\nz := 3;", 3015, SYN, 3, 3),
      CASE_OK("P3.13 module attributes in the header", "@[a]\n@[b=1]\nx := 1;"),
   };
   RUN(cases);
}
static void test_p3_include_not_first(void) {
   static const ec_case cases[] = {
      CASE_ERR("P3.20 include after a statement", "x := 1;\ninclude \"y.anvl\";", 4201, SYN, 2, 1),
      CASE_ERR("P3.21 include after module attributes", "@[a]\ninclude \"y.anvl\";", 4201, HDR, 2, 1),
      CASE_ERR("P3.22 include after attributes and a statement", "@[a]\nx := 1;\ninclude \"y.anvl\";", 4201, SYN, 3, 1),
      CASE_ERR("P3.23 nested include stays a keyword error", "x := { include \"y\"; };", 4003, SYN, 1, 15),
   };
   RUN(cases);
}

/* ====================================================================== *
 * Part 4: the specific code
 * ====================================================================== */
static void test_p4_empty_tuple_element(void) {
   static const ec_case cases[] = {
      CASE_ERR("P4.01 4007 double comma", "x := (1,,2);", 4007, SYN, 1, 9),
      CASE_ERR("P4.02 4007 leading comma", "x := (,1);", 4007, SYN, 1, 7),
      CASE_ERR("P4.03 4007 in AMP", "#!amp\nx := (1,,2);", 4007, SYN, 2, 9),
      CASE_ERR("P4.04 4007 nested tuple", "x := [(1, 2), (3,,4)];", 4007, SYN, 1, 18),
      CASE_ERR("P4.05 an array has no such code", "x := [1,,2];", 2003, SYN, 1, 9),
      CASE_OK("P4.06 trailing comma in a tuple is legal", "x := (1, 2,);"),
   };
   RUN(cases);
}
static void test_p4_trailing_comma_in_object(void) {
   static const ec_case cases[] = {
      CASE_ERR("P4.10 3004 comma for the semicolon", "x := { a := 1, };", 3004, SYN, 1, 14),
      CASE_ERR("P4.11 3004 block form", "x { a := 1, };", 3004, SYN, 1, 11),
      CASE_ERR("P4.12 3004 after a nested object", "x := { a := { b := 1; }, };", 3004, SYN, 1, 24),
      CASE_ERR("P4.13 3004 extra comma after the semicolon", "x := { a := 1; ,};", 3004, SYN, 1, 16),
      CASE_ERR("P4.14 3004 comma on the next line", "x := {\n  a := 1,\n};", 3004, SYN, 2, 9),
      CASE_ERR("P4.15 a comma between fields stays 4017", "x := { a := 1, b := 2 };", 4017, SYN, 1, 14),
      CASE_ERR("P4.16 top level stays 4017", "x := 1,", 4017, SYN, 1, 7),
   };
   RUN(cases);
}
static void test_p4_object_key(void) {
   static const ec_case cases[] = {
      CASE_ERR("P4.20 3001 semicolon", "x := { ; };", 3001, SYN, 1, 8),
      CASE_ERR("P4.21 3001 assign without a key", "x := { := 1; };", 3001, SYN, 1, 8),
      CASE_ERR("P4.22 3001 comma first", "x := { , a := 1; };", 3001, SYN, 1, 8),
      CASE_ERR("P4.23 3001 after a field", "x := { a := 1; ; };", 3001, SYN, 1, 16),
      CASE_ERR("P4.24 4002 quoted key", "x := { \"a\" := 1; };", 4002, SYN, 1, 8),
      CASE_ERR("P4.25 4002 numeric key", "x := { 1 := 1; };", 4002, SYN, 1, 8),
      CASE_ERR("P4.26 4002 varref key", "x := { $a := 1; };", 4002, SYN, 1, 8),
      CASE_ERR("P4.27 4002 attribute block with no name", "x := { @[a] y := 1; };", 4002, SYN, 1, 8),
      CASE_ERR("P4.28 4002 block form", "x {\n  7 := 1;\n};", 4002, SYN, 2, 3),
      CASE_ERR("P4.29 top level stays 2001", "1 := 2;", 2001, SYN, 1, 1),
      CASE_ERR("P4.30 top level stray semicolon stays 2001", ";", 2001, SYN, 1, 1),
   };
   RUN(cases);
}
static void test_p4_attribute_block(void) {
   static const ec_case cases[] = {
      CASE_ERR("P4.40 2008 empty", "x @[] := 1;", 2008, SYN, 1, 5),
      CASE_ERR("P4.41 2008 blanks and comment", "x @[ /* c */ ] := 1;", 2008, SYN, 1, 14),
      CASE_ERR("P4.42 2008 module", "@[]\nx := 1;", 2008, HDR, 1, 3),
      CASE_ERR("P4.43 4012 leading digit", "x @[1a] := 1;", 4012, SYN, 1, 5),
      CASE_ERR("P4.44 4012 trailing comma", "x @[a,] := 1;", 4012, SYN, 1, 7),
      CASE_ERR("P4.45 4012 quoted key", "x @[\"k\"=1] := 1;", 4012, SYN, 1, 5),
      CASE_ERR("P4.46 4012 module", "@[a,,b]\nx := 1;", 4012, HDR, 1, 5),
      CASE_ERR("P4.47 a hyphen after a key stays 3100", "x @[a-b] := 1;", 3100, SYN, 1, 6),
      CASE_ERR("P4.48 3014 flag then flag", "x @[a b] := 1;", 3014, SYN, 1, 7),
      CASE_ERR("P4.49 3014 module", "@[a b]\nx := 1;", 3014, HDR, 1, 5),
      CASE_ERR("P4.50 3014 across lines", "x @[\n  a\n  b\n] := 1;", 3014, SYN, 3, 3),
      CASE_ERR("P4.51 junk after a key stays 3100", "x @[a;] := 1;", 3100, SYN, 1, 6),
      CASE_ERR("P4.52 4011 not a bracket, statement", "x @a := 1;", 4011, SYN, 1, 3),
      CASE_ERR("P4.53 4011 not a bracket, statement start", "@a x := 1;", 4011, SYN, 1, 1),
      CASE_ERR("P4.54 4011 nested statement start", "x := { @a y := 1; };", 4011, SYN, 1, 8),
      CASE_ERR("P4.55 4011 after a base", "b : a @x := { c := 1; };", 4011, SYN, 1, 7),
   };
   RUN(cases);
}
static void test_p4_attribute_value(void) {
   static const ec_case cases[] = {
      CASE_ERR("P4.60 2006 array", "x @[a=[1,2]] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.61 2006 tuple", "x @[a=(1,2)] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.62 2006 unbalanced array", "x @[a=[1]] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.63 2006 object with a comma", "x @[a={p := 1;, q}] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.64 2006 module", "@[a=[1,2]]\nx := 1;", 2006, HDR, 1, 5),
      CASE_ERR("P4.65 2006 after other attributes, multi-line", "x @[\n  k,\n  a = [1, 2]\n] := 1;", 2006, SYN, 3, 7),
      CASE_ERR("P4.66 2006 a balanced object (BR-2610-anvl-parser-007)", "x @[a={1}] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.67 2006 a balanced tuple", "x @[a=(1 2)] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.100 2006 a one-name object", "x @[k={a}] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.101 2006 a one-element tuple", "x @[k=(1)] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.102 2006 a one-element array", "x @[k=[x]] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.103 2006 an empty array", "x @[k=[]] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.104 2006 an empty object", "x @[k={}] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.105 2006 an empty tuple", "x @[k=()] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.106 2006 a closed object, then another attribute", "x @[k={a}, m=1] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.107 2006 a bracket after whitespace", "x @[k=   (1)] := 1;", 2006, SYN, 1, 10),
      CASE_ERR("P4.108 2006 nested statement", "o := { y @[k={a}] := 1; };", 2006, SYN, 1, 14),
      CASE_ERR("P4.109 2006 module, balanced object", "@[k={a}]\nx := 1;", 2006, HDR, 1, 5),
      CASE_ERR("P4.110 2006 module, balanced tuple", "@[k=(1)]\nx := 1;", 2006, HDR, 1, 5),
      CASE_ERR("P4.111 2006 module, second attribute", "@[a=1, b={c}]\nx := 1;", 2006, HDR, 1, 10),
      CASE_ERR("P4.112 a '$' reference stays 2009", "x @[k=$v] := 1;", 2009, SYN, 1, 7),
      CASE_OK("P4.113 a bare word needs no quotes", "x @[status=done] := 1;"),
      CASE_OK("P4.114 a bare number", "x @[n=5, f=-1.5e3] := 1;"),
      CASE_OK("P4.115 a bare bool and null", "x @[on=true, off=false, z=null] := 1;"),
      CASE_OK("P4.116 a quoted string holding brackets", "x @[a=\"{a}\", b=\"(1)\", c=\"[x]\"] := 1;"),
      CASE_OK("P4.117 a bare word with a bracket inside", "x @[a=b{c}, d=e(f)] := 1;"),
      CASE_OK("P4.118 module scalars", "@[a=done, b=5, c=true]\nx := 1;"),
      CASE_ERR("P4.68 2006 a tagged blob (the owner's ruling: attributes stay small)", "x @[a=@t`z`] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.130 2006 an untagged blob", "x @[a=`z`] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.131 2006 a blob holding a comma", "x @[a=`x,y`] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.132 2006 a blob holding a bracket", "x @[a=@t`x]y`] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.133 2006 an unterminated blob", "x @[a=`abc] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.134 2006 an empty blob", "x @[a=``] := 1;", 2006, SYN, 1, 7),
      CASE_ERR("P4.135 2006 a blob after another attribute", "x @[k=1, a=@t`z`] := 1;", 2006, SYN, 1, 12),
      CASE_ERR("P4.136 2006 a blob on a nested statement", "o := { y @[a=`z`] := 1; };", 2006, SYN, 1, 14),
      CASE_ERR("P4.137 2006 a module attribute blob", "@[a=@t`z`]\nx := 1;", 2006, HDR, 1, 5),
      CASE_ERR("P4.138 2006 a module attribute untagged blob", "@[a=`z`]\nx := 1;", 2006, HDR, 1, 5),
      CASE_OK("P4.139 a blob as a statement's value is fine", "x @[a=1] := @t`z`;"),
      CASE_OK("P4.140 a backtick inside a quoted attribute string is fine", "x @[a=\"`z`\", b=\"@t\"] := 1;"),
      CASE_OK("P4.69 a string with brackets is legal", "x @[a=\"[1,2\"] := 1;"),
   };
   RUN(cases);
}
static void test_p4_unexpected_char(void) {
   static const ec_case cases[] = {
      CASE_ERR("P4.70 5001 where a value is expected", "x := \x01;", 5001, SYN, 1, 6),
      CASE_ERR("P4.71 5001 where := is expected", "x\x01 := 1;", 5001, SYN, 1, 2),
      CASE_ERR("P4.72 5001 where a name is expected", "\x01x := 1;", 5001, SYN, 1, 1),
      CASE_ERR("P4.73 5001 NUL as a value start", "x := \0;", 5001, SYN, 1, 6),
      CASE_ERR("P4.74 5001 DEL", "x := \x7f;", 5001, SYN, 1, 6),
      CASE_ERR("P4.75 5001 nested key", "x := { \x02 := 1; };", 5001, SYN, 1, 8),
      CASE_ERR("P4.76 a missing value stays 2003", "x := ;", 2003, SYN, 1, 6),
      CASE_ERR("P4.77 a tab is whitespace", "x\t:=\t;", 2003, SYN, 1, 6),
   };
   RUN(cases);
}

/* ====================================================================== *
 * Part 6: keywords and reserved words are refused as names at any level
 * ====================================================================== */
static void test_p6_keywords_as_names(void) {
   static const ec_case cases[] = {
      CASE_ERR("P6.01 include as an attribute key", "x @[include] := 1;", 4004, SYN, 1, 5),
      CASE_ERR("P6.02 import", "x @[import] := 1;", 4004, SYN, 1, 5),
      CASE_ERR("P6.03 vars", "x @[vars] := 1;", 4004, SYN, 1, 5),
      CASE_ERR("P6.04 true", "x @[true] := 1;", 4004, SYN, 1, 5),
      CASE_ERR("P6.05 false", "x @[false=1] := 1;", 4004, SYN, 1, 5),
      CASE_ERR("P6.06 null after another key", "x @[a, null] := 1;", 4004, SYN, 1, 8),
      CASE_ERR("P6.07 module attribute", "@[vars]\nx := 1;", 4004, HDR, 1, 3),
      CASE_ERR("P6.08 module attribute, second block", "@[a]\n@[null]\nx := 1;", 4004, HDR, 2, 3),
      CASE_ERR("P6.09 nested statement", "x := { y @[false] := 1; };", 4004, SYN, 1, 12),
      CASE_ERR("P6.10 multi-line", "x @[\n  a,\n  include\n] := 1;", 4004, SYN, 3, 3),
      CASE_OK("P6.11 a longer word is not a keyword", "x @[included, trueish, nulls] := 1;"),
      CASE_OK("P6.12 keywords are case sensitive", "x @[Include, TRUE, Null] := 1;"),
      CASE_ERR("P6.20 statement name, top level (4003)", "true := 1;", 4003, SYN, 1, 5),
      CASE_ERR("P6.21 object field (4003)", "x := { true := 1; };", 4003, SYN, 1, 12),
      CASE_ERR("P6.22 object field, import", "x := { import := 1; };", 4003, SYN, 1, 14),
      CASE_ERR("P6.23 field of a block", "x {\n  vars := 1;\n};", 4003, SYN, 2, 7),
      CASE_ERR("P6.24 field at depth 2", "x := { y := { null := 1; }; };", 4003, SYN, 1, 19),
      CASE_ERR("P6.25 field inside an array", "x := [ { false := 1; } ];", 4003, SYN, 1, 15),
   };
   RUN(cases);
}

static void test_p4_attribute_value_message(void) {
   const char *m = anvl_error_code_message(ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE);
   TestBit.is_not_null(m, "P4.message: 2006 has a message");
   TestBit.is_true(m && strstr(m, "must be scalar") != NULL,
                   "P4.message: says attribute values must be scalars");
   TestBit.is_true(m && strstr(m, "bare or quoted") != NULL,
                   "P4.message: says bare or quoted, so quoting is not required");
   TestBit.is_true(m && strstr(m, "blob") != NULL, "P4.message: names blobs among the refused values");
}

/* ====================================================================== *
 * Part 5: what stays as it was
 * ====================================================================== */
static void test_p5_unchanged(void) {
   static const ec_case cases[] = {
      CASE_ERR("P5.01 empty array is 4009 (3011 is a synonym)", "x := [];", 4009, SYN, 1, 7),
      CASE_ERR("P5.02 one-element tuple is 3013 (4005 is a synonym)", "x := (1);", 3013, SYN, 1, 8),
      CASE_ERR("P5.03 empty tuple", "x := ();", 3012, SYN, 1, 7),
      CASE_ERR("P5.04 empty object", "x := {};", 3010, SYN, 1, 7),
      CASE_ERR("P5.05 a keyword attribute name is 4004 (owner decision)", "x @[include] := 1;", 4004, SYN, 1, 5),
      CASE_ERR("P5.06 a keyword attribute name with a value", "x @[import=1, vars=2] := 1;", 4004, SYN, 1, 5),
      CASE_ERR("P5.07 a block and a name repeated at top level is 4053 (4015 is not used)", "x { a := 1; };\nx := 2;", 4053, RES, 2, 1),
      CASE_ERR("P5.08 a missing := is 2002 (3016 is not used)", "x = 1;", 2002, SYN, 1, 3),
      CASE_ERR("P5.09 include as a name at the start is a header error", "include := 1;", 3100, HDR, 1, 9),
   };
   RUN(cases);
}

/* ====================================================================== *
 * Part 8: references that lead back to a container holding them
 * (BR-2611-anvl-resolver-002). The error is 4054, category RESOLVE, at the `$` of one reference on the cycle:
 * the first such reference in source order (documents in load order). A walk follows (1) a container's own
 * members, (2) fields inherited from a base, (3) a resolved `$` reference to its target statement's value.
 * ====================================================================== */
// Documents that load clean and must keep doing so: a reference to a scalar, a reference with no way back,
// the plain chain cycle (unresolved, null), an override that breaks the cycle, a diamond of aliased fields.
static const char *const p8_valid_sources[] = {
   "a := $b; b := $a;",
   "a := $a;",
   "a := $b; b := $a; c := [ $a ];",
   "n := 1; a := { x := $n; y := [ $n, $n ]; };",
   "a := { x := $b; }; b := { y := 1; };",
   "a := { x := $b; }; b : a := { x := 1; };",
   "a := { x := $c; }; b : a := { x := 1; }; c : b := { w := 2; };",
   "base := { v := [ 1, 2 ]; }; a : base := { p := 1; }; b : base := { q := 2; }; c := { x := $a; y := $b; };",
   "a := [ $nothing ];",
   "a := { x := $b; }; b := $c; c := { y := 1; };",
   "a := [ $b, $c ]; b := { p := 1; }; c := ( 1, $b );",
   "a { x := 1; }; b := { y := $a; };",
   "a := { x := $b; y := $c; }; b := { z := $c; }; c := { w := 1; };",
};

static void test_p8_references_that_stay_valid(void) {
   for (size_t i = 0; i < sizeof p8_valid_sources / sizeof p8_valid_sources[0]; i++) {
      char name[64];
      snprintf(name, sizeof name, "P8.valid.%02zu", i + 1);
      ec_case c = CASE_OK(name, "");
      c.src = p8_valid_sources[i];
      c.len = strlen(c.src);
      check(&c);
   }
}

// Walks a value through the public, transparent accessors. A document that loaded clean must never loop: the
// walk stays under a depth and a visit cap. Returns false when a cap is hit.
#define P8_WALK_DEPTH 200
#define P8_WALK_VISITS 100000
static bool p8_walk(anvil_value v, size_t depth, size_t *visits) {
   if (!v) {
      return true;
   }
   if (depth > P8_WALK_DEPTH || ++*visits > P8_WALK_VISITS) {
      return false;
   }
   anvil_value_type t = anvil_value_get_type(v);
   size_t n = (t == ANVIL_VALUE_ARRAY || t == ANVIL_VALUE_TUPLE || t == ANVIL_VALUE_OBJECT) ? anvil_value_get_count(v) : 0;
   for (size_t i = 0; i < n; i++) {
      anvil_value child = t == ANVIL_VALUE_OBJECT ? anvil_statement_get_value(anvil_value_get_statement(v, i))
                                                  : anvil_value_get_element(v, i);
      if (!p8_walk(child, depth + 1, visits)) {
         return false;
      }
   }
   return true;
}

static bool p8_walk_document(anvil_document doc) {
   size_t visits = 0;
   anvil_statement_iterator it = anvil_document_get_statements(doc);
   anvil_statement st = NULL;
   bool ok = true;
   while (ok && anvil_statement_iterator_next(it, &st)) {
      ok = p8_walk(anvil_statement_get_value(st), 0, &visits);
   }
   anvil_statement_iterator_dispose(it);
   return ok;
}

static void test_p8_clean_documents_walk_without_looping(void) {
   for (size_t i = 0; i < sizeof p8_valid_sources / sizeof p8_valid_sources[0]; i++) {
      char label[96];
      anvil_document doc = anvil_load_buffer(p8_valid_sources[i], strlen(p8_valid_sources[i]));
      snprintf(label, sizeof label, "P8.walk.%02zu: loads clean", i + 1);
      TestBit.is_true(doc && !anvil_has_errors(doc), label);
      snprintf(label, sizeof label, "P8.walk.%02zu: the transparent walk ends", i + 1);
      TestBit.is_true(doc && p8_walk_document(doc), label);
      anvil_dispose(doc);
   }
}

#define RC ANVL_ERR_RESOLVER_REFERENCE_CYCLE
#define RCASE(name, src, line, col) CASE_ERR(name, src, RC, RES, line, col)

static void test_p8_a_container_holding_a_reference_to_itself(void) {
   static const ec_case cases[] = {
      RCASE("P8.self.01 s1 := { a := $s1; };", "s1 := { a := $s1; };", 1, 14),
      RCASE("P8.self.02 one object deeper", "s1 := { a := { b := $s1; }; };", 1, 21),
      RCASE("P8.self.03 an array", "a := [ $a ];", 1, 8),
      RCASE("P8.self.04 a tuple", "a := ( 1, $a );", 1, 11),
      RCASE("P8.self.05 block form", "a { x := $a; };", 1, 10),
      RCASE("P8.self.06 many containers down", "a := [ [ { x := ( 1, [ $a ] ); } ] ];", 1, 24),
      RCASE("P8.self.07 the line of the reference", "s1 := {\n   a := 1;\n   b := $s1;\n};", 3, 9),
   };
   RUN(cases);
}

static void test_p8_references_that_refer_to_each_other(void) {
   static const ec_case cases[] = {
      RCASE("P8.mutual.01 two objects", "a := { x := $b; }; b := { y := $a; };", 1, 13),
      RCASE("P8.mutual.02 declared the other way round", "b := { y := $a; }; a := { x := $b; };", 1, 13),
      RCASE("P8.mutual.03 a ring of three", "c := { x := $a; }; a := { y := $b; }; b := { z := $c; };", 1, 13),
      RCASE("P8.mutual.04 block form", "a { x := $b; }; b { y := $a; };", 1, 10),
      RCASE("P8.mutual.05 through a chain: only the reference inside the container is on the cycle", "a := $b; b := { c := $a; };", 1, 22),
      RCASE("P8.mutual.06 through a longer chain", "a := $b; b := $c; c := [ $a ];", 1, 26),
      RCASE("P8.mutual.07 a scalar reference beside the cycle is not it", "r := { p := $a; q := $b; }; a := { x := $r; }; b := 1;", 1, 13),
   };
   RUN(cases);
}

static void test_p8_cycles_closed_through_inheritance(void) {
   static const ec_case cases[] = {
      RCASE("P8.inherit.01 the inherited field refers to the deriving statement", "a := { x := $b; }; b : a := { z := 1; };", 1, 13),
      RCASE("P8.inherit.02 b only inherits the reference to c, c refers to b", "a := { x := $c; }; b : a := { y := 1; }; c := { z := $b; };", 1, 13),
      RCASE("P8.inherit.03 the base refers to the statement that derives from it", "base := { r := $leaf; }; leaf : base := { q := 1; };", 1, 16),
      RCASE("P8.inherit.04 three levels of bases", "a := { x := $d; }; b : a := { m := 1; }; c : b := { n := 2; }; d : c := { k := 3; };", 1, 13),
      RCASE("P8.inherit.05 an own reference, no aliased field involved", "a := { x := 1; }; b : a := { y := $c; }; c := { z := $b; };", 1, 35),
      RCASE("P8.inherit.06 a block-form deriving statement", "a := { x := $b; }; b : a { z := 1; };", 1, 13),
   };
   RUN(cases);
}

// The position is the `$` of the first reference, in source order (the documents in load order), that lies on a cycle.
static void test_p8_which_reference_is_reported(void) {
   static const ec_case cases[] = {
      RCASE("P8.pos.01 the second statement, not the first valid one", "ok := 1;\nb := [ $b ];\na := [ $a ];", 2, 8),
      RCASE("P8.pos.02 the first in source order, not where a walk starts", "c := { x := $a; };\na := { y := $b; };\nb := { z := $c; };", 1, 13),
      RCASE("P8.pos.03 an earlier reference that is not on the cycle is skipped", "u := $r;\nr := [ $r ];", 2, 8),
      RCASE("P8.pos.04 two separate cycles: the first", "a := [ $a ];\nb := [ $b ];", 1, 8),
   };
   RUN(cases);
}

static void test_p8_error_code_name_and_message(void) {
   TestBit.is_equal_int(4054, ANVL_ERR_RESOLVER_REFERENCE_CYCLE, "P8: the code is 4054");
   const char *m = anvl_error_code_message(ANVL_ERR_RESOLVER_REFERENCE_CYCLE);
   TestBit.is_true(m && strcmp(m, "A reference leads back to a value that contains it") == 0, "P8: the message");
   const char *n = anvl_error_code_name(ANVL_ERR_RESOLVER_REFERENCE_CYCLE);
   TestBit.is_true(n && strcmp(n, "ANVL_ERR_RESOLVER_REFERENCE_CYCLE") == 0, "P8: the name is the enum identifier");
}

static void test_p8_public_error_for_a_cycle(void) {
   const char *src = "s1 := { a := $s1; };";
   anvil_document doc = anvil_load_buffer(src, strlen(src));
   TestBit.is_true(doc && anvil_has_errors(doc), "P8: the load fails");
   anvil_error err = doc ? anvil_document_get_error(doc) : NULL;
   char msg[128] = {0};
   anvil_error_get_message(err, msg, sizeof msg);
   TestBit.is_equal_str("A reference leads back to a value that contains it", msg, "P8: the public message");
   TestBit.is_equal_int(ANVIL_ERR_RESOLVE, anvil_get_error(doc), "P8: the category is RESOLVE");
   anvil_dispose(doc);
}

// Includes: the documents are taken in load order (the dump's doc numbers), so the root's references come first.
static void p8_file(const char *label, const char *path, size_t line, size_t col, const char *source_suffix) {
   char l[160];
   anvil_document doc = anvil_load(path);
   snprintf(l, sizeof l, "%s: loads", label);
   TestBit.is_not_null(doc, l);
   if (!doc) {
      return;
   }
   snprintf(l, sizeof l, "%s: fails", label);
   TestBit.is_true(anvil_has_errors(doc), l);
   anvil_error err = anvil_document_get_error(doc);
   snprintf(l, sizeof l, "%s: category RESOLVE", label);
   TestBit.is_equal_int(ANVIL_ERR_RESOLVE, anvil_get_error(doc), l);
   char msg[128] = {0};
   anvil_error_get_message(err, msg, sizeof msg);
   snprintf(l, sizeof l, "%s: the message", label);
   TestBit.is_equal_str("A reference leads back to a value that contains it", msg, l);
   snprintf(l, sizeof l, "%s: line %zu", label, line);
   TestBit.is_equal_int((long long)line, (long long)anvil_error_get_line(err), l);
   snprintf(l, sizeof l, "%s: column %zu", label, col);
   TestBit.is_equal_int((long long)col, (long long)anvil_error_get_column(err), l);
   char src[256] = {0};
   anvil_error_get_source(err, src, sizeof src);
   size_t sl = strlen(src), xl = strlen(source_suffix);
   snprintf(l, sizeof l, "%s: source '%s'", label, source_suffix);
   TestBit.is_true(sl >= xl && strcmp(src + sl - xl, source_suffix) == 0 && (xl > 0 || sl == 0), l);
   anvil_dispose(doc);
}

static void test_p8_across_included_documents(void) {
   // root: include child; root := { x := $child; }   child: child := { y := $root; }
   p8_file("P8.inc.01 two documents refer to each other: the root's reference", "../../conformance/cases/refcycle/inc/pair_root.anvl", 3, 16, "");
   // the root only refers to a self-referring array of the child
   p8_file("P8.inc.02 the cycle is in the included document", "../../conformance/cases/refcycle/inc/child_self_root.anvl", 1, 8, "child_self.anvl");
   // the root's own reference to the cyclic statement is not on the cycle, the child's is
   p8_file("P8.inc.03 an earlier root reference that is not on the cycle", "../../conformance/cases/refcycle/inc/skip_root.anvl", 1, 8, "skip_child.anvl");
   // an included base refers to a statement of the root that derives from it
   p8_file("P8.inc.04 closed through a base in another document", "../../conformance/cases/refcycle/inc/base_root.anvl", 1, 16, "base_child.anvl");
   // a reference into an included document with no way back is fine
   anvil_document doc = anvil_load("../../conformance/cases/refcycle/inc/ok_root.anvl");
   TestBit.is_true(doc && !anvil_has_errors(doc), "P8.inc.05 a reference into an included document, no way back: loads");
   TestBit.is_true(doc && p8_walk_document(doc), "P8.inc.05: and walks");
   anvil_dispose(doc);
}

// n top-level members s0..s(n-1) of one array each; member i refers to s(i+1) (and s(i+2) when `ladder`), the last to
// s0 when `ring`, else it holds a scalar. The text is allocated; the caller frees it.
static char *p8_chain_text(size_t n, bool ring, bool ladder, size_t *len) {
   size_t cap = n * 64 + 64;
   char *t = malloc(cap);
   size_t at = 0;
   for (size_t i = 0; i < n; i++) {
      at += (size_t)snprintf(t + at, cap - at, "s%zu := [ ", i);
      bool any = false;
      if (i + 1 < n || ring) {
         at += (size_t)snprintf(t + at, cap - at, "$s%zu", (i + 1) % n);
         any = true;
      }
      if (ladder && (i + 2 < n || ring)) {
         at += (size_t)snprintf(t + at, cap - at, "%s$s%zu", any ? ", " : "", (i + 2) % n);
         any = true;
      }
      if (!any) {
         at += (size_t)snprintf(t + at, cap - at, "1");
      }
      at += (size_t)snprintf(t + at, cap - at, " ];\n");
   }
   *len = at;
   return t;
}

static double p8_seconds_since(const struct timespec *t0) {
   struct timespec t1;
   clock_gettime(CLOCK_MONOTONIC, &t1);
   return (double)(t1.tv_sec - t0->tv_sec) + (double)(t1.tv_nsec - t0->tv_nsec) / 1e9;
}

static void test_p8_detection_is_linear(void) {
   struct { const char *name; bool ring; bool ladder; } shapes[] = {
      {"chain", false, false}, {"ring", true, false}, {"ladder", false, true}, {"ladder ring", true, true},
   };
   for (size_t k = 0; k < 4; k++) {
      size_t len = 0;
      char *text = p8_chain_text(100000, shapes[k].ring, shapes[k].ladder, &len);
      struct timespec t0;
      clock_gettime(CLOCK_MONOTONIC, &t0);
      anvil_document doc = anvil_load_buffer(text, len);
      char label[96];
      snprintf(label, sizeof label, "P8.scale: a 100000-statement %s %s", shapes[k].name, shapes[k].ring ? "is refused" : "loads");
      TestBit.is_true(doc && anvil_has_errors(doc) == shapes[k].ring, label);
      if (doc && shapes[k].ring) {
         anvil_error e = anvil_document_get_error(doc);
         TestBit.is_equal_int(1, (long long)anvil_error_get_line(e), "P8.scale: the ring is reported at the first reference");
         TestBit.is_equal_int(9, (long long)anvil_error_get_column(e), "P8.scale: ... column 9");
      }
      snprintf(label, sizeof label, "P8.scale: the %s is done in well under 20 s", shapes[k].name);
      TestBit.is_true(p8_seconds_since(&t0) < 20.0, label);
      anvil_dispose(doc);
      free(text);
   }
   // one container with 100000 references to one object
   size_t cap = 100000 * 24 + 128, at = 0;
   char *t = malloc(cap);
   at += (size_t)snprintf(t + at, cap - at, "t := { v := 1; };\nhub := {\n");
   for (size_t i = 0; i < 100000; i++) {
      at += (size_t)snprintf(t + at, cap - at, "f%zu := $t;\n", i);
   }
   at += (size_t)snprintf(t + at, cap - at, "};\n");
   struct timespec t0;
   clock_gettime(CLOCK_MONOTONIC, &t0);
   anvil_document doc = anvil_load_buffer(t, at);
   TestBit.is_true(doc && !anvil_has_errors(doc), "P8.scale: a hub of 100000 references to one object loads");
   TestBit.is_true(p8_seconds_since(&t0) < 20.0, "P8.scale: the hub is done in well under 20 s");
   anvil_dispose(doc);
   free(t);
}

/* BR-2611-anvl-resolver-002 (F1): a base field is inherited by many statements; the pass must walk its subtree once, not
 * once per deriving statement. Scaling m (the size of the inherited field) and n (the deriving statements) by 4 must cost
 * about 4 times as much (linear), not 16 (quadratic). One resolved reference at the end turns the pass on. */
static double p8_inherit_seconds(size_t m, bool nested) {
   size_t cap = m * 24 + m * 24 + 256, at = 0;
   char *t = malloc(cap + m * 40);
   at += (size_t)snprintf(t + at, cap, "base := { big := %s", nested ? "{ " : "[ ");
   for (size_t i = 0; i < m; i++) {
      at += (size_t)snprintf(t + at, 40, nested ? "f%zu := 1; " : "1, ", i);
   }
   at += (size_t)snprintf(t + at, 8, nested ? "}; };\n" : " ]; };\n");
   for (size_t i = 0; i < m; i++) {
      at += (size_t)snprintf(t + at, 48, "d%zu : base := { x := 1; };\n", i);
   }
   at += (size_t)snprintf(t + at, 32, "t := 1;\nr := $t;\n");
   clock_t c0 = clock();
   anvil_document doc = anvil_load_buffer(t, at);
   double s = (double)(clock() - c0) / CLOCKS_PER_SEC;
   TestBit.is_true(doc && !anvil_has_errors(doc), "P8.inherit-scale: the document loads");
   anvil_dispose(doc);
   free(t);
   return s;
}

static void test_p8_inherited_subtrees_are_scanned_once(void) {
   for (int nested = 0; nested < 2; nested++) {
      double small = p8_inherit_seconds(4000, nested), big = p8_inherit_seconds(16000, nested);
      char label[96];
      snprintf(label, sizeof label, "P8.inherit-scale: 4x the work costs less than 9x the time (%s)", nested ? "object" : "array");
      TestBit.is_true(big < 9.0 * (small > 0.01 ? small : 0.01), label);
   }
}

/* BR-2611-anvl-resolver-002 (F5): a base field holding m references, inherited by n statements. The pass must not copy the
 * field's m edges into each deriving statement: the edges it builds are O(n + m), not n * m. */
static void test_p8_inherited_references_are_not_copied(void) {
   size_t m = 3000, n = 3000, cap = (m + n) * 40 + 256, at = 0;
   char *t = malloc(cap);
   at += (size_t)snprintf(t + at, cap - at, "t := 1;\nbase := { a := [ ");
   for (size_t i = 0; i < m; i++) {
      at += (size_t)snprintf(t + at, cap - at, "$t, ");
   }
   at += (size_t)snprintf(t + at, cap - at, "1 ]; };\n");
   for (size_t i = 0; i < n; i++) {
      at += (size_t)snprintf(t + at, cap - at, "d%zu : base := { x := 1; };\n", i);
   }
   anvil_document doc = anvil_load_buffer(t, at);
   TestBit.is_true(doc && !anvil_has_errors(doc), "P8.copies: the document loads");
   TestBit.is_true(anvl_resolver_last_edges > 0, "P8.copies: the pass ran and counted its edges");
   TestBit.is_true(anvl_resolver_last_edges <= 3 * (m + n) + 16, "P8.copies: the edges are O(n + m), not n * m");
   anvil_dispose(doc);
   free(t);
}


/* ---------------------------------------------------------------------- *
 * FR-2611-anvl-errors-002 (option B): the specific code is stable public ABI. anvil_error_get_code() and
 * anvil_error_get_name() read it from the error handle. Buffer convention as anvil_error_get_message.
 * ---------------------------------------------------------------------- */
static anvil_error failed_error(const char *src, anvil_document *out) {
   *out = anvil_load_buffer(src, strlen(src));
   return *out ? anvil_document_get_error(*out) : NULL;
}

static void expect_code_and_name(const char *src, int code, const char *name, int category, const char *tag) {
   anvil_document doc;
   anvil_error err = failed_error(src, &doc);
   TestBit.is_not_null(err, tag);
   TestBit.is_equal_int(code, (long long)anvil_error_get_code(err), tag);
   char buf[96] = {0};
   size_t n = anvil_error_get_name(err, buf, sizeof buf);
   TestBit.is_equal_str(name, buf, tag);
   TestBit.is_equal_int((long long)strlen(name), (long long)n, tag);
   TestBit.is_equal_int(category, anvil_get_error(doc), tag);
   anvil_dispose(doc);
}

static void test_e1_code_and_name_of_a_resolver_error(void) {
   expect_code_and_name("s1 := { a := $s1; };", 4054, "ANVL_ERR_RESOLVER_REFERENCE_CYCLE", ANVIL_ERR_RESOLVE,
                        "E1: a reference cycle");
   expect_code_and_name("a := 1;\na := 2;", 4053, "ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER", ANVIL_ERR_RESOLVE,
                        "E1: a repeated name");
}

static void test_e2_code_and_name_of_a_syntax_error(void) {
   expect_code_and_name("s1 := { f3 : s1 { a := 1; }; };", 4022, "ANVL_ERR_PARSER_NESTED_BASE", ANVIL_ERR_SYNTAX,
                        "E2: a nested base");
}

static void test_e3_code_and_name_of_a_file_error(void) {
   anvil_document doc = anvil_load("does-not-exist-error-code-abi.anvl");
   anvil_error err = doc ? anvil_document_get_error(doc) : NULL;
   TestBit.is_not_null(err, "E3: the load fails with a detail object");
   TestBit.is_equal_int(1002, (long long)anvil_error_get_code(err), "E3: the code");
   char buf[64] = {0};
   anvil_error_get_name(err, buf, sizeof buf);
   TestBit.is_equal_str("ANVL_ERR_IO_FILE_NOT_FOUND", buf, "E3: the name");
   anvil_dispose(doc);
}

static void test_e4_a_null_error_has_no_code_and_an_empty_name(void) {
   TestBit.is_equal_int(ANVL_ERR_NONE, (long long)anvil_error_get_code(NULL), "E4: NULL code is ANVL_ERR_NONE");
   char buf[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
   TestBit.is_equal_int(0, (long long)anvil_error_get_name(NULL, buf, sizeof buf), "E4: NULL returns 0");
   TestBit.is_equal_int(0, buf[0], "E4: NULL writes an empty string");
   TestBit.is_equal_int(0, (long long)anvil_error_get_name(NULL, NULL, 0), "E4: NULL with no buffer");
}

static void test_e5_name_follows_the_buffer_convention(void) {
   anvil_document doc;
   anvil_error err = failed_error("s1 := { a := $s1; };", &doc);
   const char *full = "ANVL_ERR_RESOLVER_REFERENCE_CYCLE";
   size_t len = strlen(full);
   TestBit.is_equal_int((long long)len, (long long)anvil_error_get_name(err, NULL, 0), "E5: size query");
   char small[8];
   memset(small, 'x', sizeof small);
   TestBit.is_equal_int((long long)len, (long long)anvil_error_get_name(err, small, sizeof small),
                        "E5: truncated call returns the full length");
   TestBit.is_equal_int(0, strcmp(small, "ANVL_ER"), "E5: truncated to buflen-1 and NUL-terminated");
   char one[1] = {'x'};
   TestBit.is_equal_int((long long)len, (long long)anvil_error_get_name(err, one, 1), "E5: buflen 1 size");
   TestBit.is_equal_int(0, one[0], "E5: buflen 1 writes only the NUL");
   char zero[2] = {'x', 'y'};
   TestBit.is_equal_int((long long)len, (long long)anvil_error_get_name(err, zero, 0), "E5: buflen 0 size");
   TestBit.is_true(zero[0] == 'x' && zero[1] == 'y', "E5: buflen 0 writes nothing");
   char exact[64] = {0};
   anvil_error_get_name(err, exact, len + 1);
   TestBit.is_equal_str(full, exact, "E5: exact fit");
   anvil_dispose(doc);
}

static void test_e6_every_code_has_its_name_through_the_api(void) {
   // Each table entry: the public enum constant has the pinned value and anvl_error_code_name (which the
   // accessor returns) the pinned name. Entries the build cannot produce are checked by name only.
   size_t n = sizeof ERROR_CODE_PINS / sizeof ERROR_CODE_PINS[0];
   for (size_t i = 0; i < n; i++) {
      char tag[128];
      snprintf(tag, sizeof tag, "E6: %s", ERROR_CODE_PINS[i].name);
      const char *got = anvl_error_code_name((anvl_err_code)ERROR_CODE_PINS[i].value);
      TestBit.is_true(got && strcmp(got, ERROR_CODE_PINS[i].name) == 0, tag);
   }
   TestBit.is_true(n >= 109, "E6: the pinned table keeps its 109 entries (codes are never removed)");
}

static void test_e7_pinned_codes_keep_their_numbers(void) {
   // The numbers are the enum constants as the public header declares them; the pins come from the committed
   // conformance/drift-tables.txt. Moving a number makes this fail until the table (and every binding that
   // mirrors it) is deliberately changed.
   size_t n = sizeof ERROR_CODE_PINS / sizeof ERROR_CODE_PINS[0];
   int moved = 0;
   for (size_t i = 0; i < n; i++) {
      if ((long long)ERROR_CODE_PINS[i].constant != ERROR_CODE_PINS[i].value) {
         moved++;
      }
   }
   TestBit.is_equal_int(0, moved, "E7: no pinned code changed its number");
}

/* ----------------------------------------------------------------------- *
 * The lock (FR-2611-anvl-errors-002): the pin table test/unit/error_code_pins.h is committed and edited only by
 * hand. It is not generated from conformance/drift-tables.txt, so regenerating that table can never move a pin.
 * ----------------------------------------------------------------------- */
static bool file_contains(const char *path, const char *needle) {
   FILE *f = fopen(path, "rb");
   if (!f) {
      return false;
   }
   char line[512];
   bool found = false;
   while (!found && fgets(line, sizeof line, f)) {
      found = strstr(line, needle) != NULL;
   }
   fclose(f);
   return found;
}

static void test_e8_the_pin_table_is_committed_and_hand_maintained(void) {
   TestBit.is_true(file_contains("error_code_pins.h", "edited only by hand"),
                   "E8: test/unit/error_code_pins.h is committed and says it is edited only by hand");
   TestBit.is_true(file_contains("error_code_pins.h", "ABI break"), "E8: the table says a changed line is an ABI break");
   TestBit.is_true(!file_contains("Makefile", "error_code_pins.h: $(ANVL)"), "E8: no Makefile rule generates the pins");
}

static void test_e9_every_code_in_the_public_header_is_pinned(void) {
   FILE *f = fopen("../../include/anvil_error_codes.h", "rb");
   TestBit.is_not_null(f, "E9: anvil_error_codes.h readable");
   if (!f) {
      return;
   }
   char line[256];
   int seen = 0;
   size_t n = sizeof ERROR_CODE_PINS / sizeof ERROR_CODE_PINS[0];
   while (fgets(line, sizeof line, f)) {
      char name[96];
      long long value;
      const char *p = strstr(line, "ANVL_ERR_");
      int got = p ? sscanf(p, "%95[A-Z0-9_] = %lld", name, &value) : 0;
      if (p && got == 1 && fgets(line, sizeof line, f)) { // the number wrapped onto the next line
         got = sscanf(line, " %lld", &value) == 1 ? 2 : 0;
      }
      if (got == 2) {
         bool pinned = false;
         for (size_t i = 0; i < n; i++) {
            if (strcmp(ERROR_CODE_PINS[i].name, name) == 0 && ERROR_CODE_PINS[i].value == value) {
               pinned = true;
            }
         }
         if (!pinned) {
            printf("    E9: %s = %lld is in the header but not pinned (add it to test/unit/error_code_pins.h)\n", name, value);
         }
         TestBit.is_true(pinned, name);
         seen++;
      }
   }
   fclose(f);
   TestBit.is_true(seen >= 108, "E9: the header scan found the codes");
   TestBit.is_equal_int(seen, (long long)n, "E9: the pins hold exactly the header codes (no pin without a code)");
}

static void test_e10_the_stale_table_message_protects_the_pinned_codes(void) {
   TestBit.is_true(file_contains("../../scripts/check-drift-tables.sh", "stable ABI"),
                   "E10: check-drift-tables says pinned codes are stable ABI instead of inviting regeneration");
}

int main(void) {
   TestBit.run_ex("P4_attribute_value_message", NULL, test_p4_attribute_value_message, th);
   TestBit.run_ex("P1_duplicate_object_fields", NULL, test_p1_duplicate_object_fields, th);
   TestBit.run_ex("P1_duplicate_object_fields_large", NULL, test_p1_duplicate_object_fields_large, th);
   TestBit.run_ex("P1_duplicate_attribute_keys", NULL, test_p1_duplicate_attribute_keys, th);
   TestBit.run_ex("P2_non_object_base", NULL, test_p2_non_object_base, th);
   TestBit.run_ex("P7_nested_base", NULL, test_p7_nested_base, th);
   TestBit.run_ex("P7_nested_base_message", NULL, test_p7_nested_base_message, th);
   TestBit.run_ex("P3_multiple_shebang", NULL, test_p3_multiple_shebang, th);
   TestBit.run_ex("P3_module_attributes_after_statements", NULL,
                  test_p3_module_attributes_after_statements, th);
   TestBit.run_ex("P3_include_not_first", NULL, test_p3_include_not_first, th);
   TestBit.run_ex("P4_empty_tuple_element", NULL, test_p4_empty_tuple_element, th);
   TestBit.run_ex("P4_trailing_comma_in_object", NULL, test_p4_trailing_comma_in_object, th);
   TestBit.run_ex("P4_object_key", NULL, test_p4_object_key, th);
   TestBit.run_ex("P4_attribute_block", NULL, test_p4_attribute_block, th);
   TestBit.run_ex("P4_attribute_value", NULL, test_p4_attribute_value, th);
   TestBit.run_ex("P4_unexpected_char", NULL, test_p4_unexpected_char, th);
   TestBit.run_ex("P5_unchanged", NULL, test_p5_unchanged, th);
   TestBit.run_ex("P6_keywords_as_names", NULL, test_p6_keywords_as_names, th);
   TestBit.run_ex("P8_references_that_stay_valid", NULL, test_p8_references_that_stay_valid, th);
   TestBit.run_ex("P8_clean_documents_walk_without_looping", NULL, test_p8_clean_documents_walk_without_looping, th);
   TestBit.run_ex("P8_error_code_name_and_message", NULL, test_p8_error_code_name_and_message, th);
   TestBit.run_ex("P8_public_error_for_a_cycle", NULL, test_p8_public_error_for_a_cycle, th);
   TestBit.run_ex("P8_a_container_holding_a_reference_to_itself", NULL, test_p8_a_container_holding_a_reference_to_itself, th);
   TestBit.run_ex("P8_references_that_refer_to_each_other", NULL, test_p8_references_that_refer_to_each_other, th);
   TestBit.run_ex("P8_cycles_closed_through_inheritance", NULL, test_p8_cycles_closed_through_inheritance, th);
   TestBit.run_ex("P8_which_reference_is_reported", NULL, test_p8_which_reference_is_reported, th);
   TestBit.run_ex("P8_across_included_documents", NULL, test_p8_across_included_documents, th);
   TestBit.run_ex("P8_detection_is_linear", NULL, test_p8_detection_is_linear, th);
   TestBit.run_ex("P8_inherited_references_are_not_copied", NULL, test_p8_inherited_references_are_not_copied, th);
   TestBit.run_ex("P8_inherited_subtrees_are_scanned_once", NULL, test_p8_inherited_subtrees_are_scanned_once, th);
   TestBit.run_ex("E1_code_and_name_of_a_resolver_error", NULL, test_e1_code_and_name_of_a_resolver_error, th);
   TestBit.run_ex("E2_code_and_name_of_a_syntax_error", NULL, test_e2_code_and_name_of_a_syntax_error, th);
   TestBit.run_ex("E3_code_and_name_of_a_file_error", NULL, test_e3_code_and_name_of_a_file_error, th);
   TestBit.run_ex("E4_a_null_error_has_no_code_and_an_empty_name", NULL, test_e4_a_null_error_has_no_code_and_an_empty_name, th);
   TestBit.run_ex("E5_name_follows_the_buffer_convention", NULL, test_e5_name_follows_the_buffer_convention, th);
   TestBit.run_ex("E6_every_code_has_its_name_through_the_api", NULL, test_e6_every_code_has_its_name_through_the_api, th);
   TestBit.run_ex("E7_pinned_codes_keep_their_numbers", NULL, test_e7_pinned_codes_keep_their_numbers, th);
   TestBit.run_ex("E8_the_pin_table_is_committed_and_hand_maintained", NULL, test_e8_the_pin_table_is_committed_and_hand_maintained, th);
   TestBit.run_ex("E9_every_code_in_the_public_header_is_pinned", NULL, test_e9_every_code_in_the_public_header_is_pinned, th);
   TestBit.run_ex("E10_the_stale_table_message_protects_the_pinned_codes", NULL, test_e10_the_stale_table_message_protects_the_pinned_codes, th);
   return TestBit.report();
}
