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
#include "internal/flat_internal.h"
#include "internal/module.h"
#include "testbit.h"
// ----------------
#include <sigma/list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
      CASE_ERR("P2.11 nested derived statement", "a := 1;\nx := { b : a := { c := 1; }; };", 4020, RES, 2, 12),
      CASE_ERR("P2.12 block-form derived statement", "a := 1;\nb : a { c := 1; };", 4020, RES, 2, 5),
      CASE_OK("P2.13 object base", "a := { x := 1; };\nb : a := { y := 2; };"),
      CASE_OK("P2.14 chain of object bases", "a := { x := 1; };\nb : a := { y := 2; };\nc : b := { z := 3; };"),
      CASE_ERR("P2.15 anonymous base stays 4052", "a { x := 1; };\nb : a := { y := 2; };", 4052, RES, 0, 0),
      CASE_ERR("P2.16 missing base stays 4051", "b : zz := { c := 1; };", 4051, RES, 0, 0),
      CASE_ERR("P2.17 non-object value after a base stays 4020 (parse time)", "a := { x := 1; };\nb : a := 5;", 4020, SYN, 2, 10),
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
      CASE_OK("P4.68 a blob is legal today", "x @[a=@t`z`] := 1;"),
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
   TestBit.is_true(m && strstr(m, "blob") == NULL, "P4.message: does not mention blobs");
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

int main(void) {
   TestBit.run_ex("P4_attribute_value_message", NULL, test_p4_attribute_value_message, th);
   TestBit.run_ex("P1_duplicate_object_fields", NULL, test_p1_duplicate_object_fields, th);
   TestBit.run_ex("P1_duplicate_object_fields_large", NULL, test_p1_duplicate_object_fields_large, th);
   TestBit.run_ex("P1_duplicate_attribute_keys", NULL, test_p1_duplicate_attribute_keys, th);
   TestBit.run_ex("P2_non_object_base", NULL, test_p2_non_object_base, th);
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
   return TestBit.report();
}
