/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_builder.c - Unit tests for the document builder (anvil_builder.h) *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_builder.c                                         *
 * ---------------------------------------------------------------------- *
 * Links src/writer/ and testbit only - no reader code. Exact-output      *
 * assertions use the same expected text as test_writer.c, so builder and *
 * writer can't disagree; test_builder_roundtrip.c checks the output      *
 * through the real parser.                                               *
 * ********************************************************************** */

#include "anvil_builder.h"
#include "anvil_builder_vtable.h"
#include "anvil_writer.h"
#include "testbit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void th(void) {}

static void expect_emit(anvil_builder b, const char *expected, const char *msg) {
   TestBit.is_not_null(b, msg);
   if (!b) {
      return;
   }
   size_t len = 0;
   const char *text = anvil_builder_emit(b, &len);
   TestBit.is_not_null(text, msg);
   if (text) {
      TestBit.is_equal_str(expected, text, msg);
      TestBit.is_equal_int((long long)strlen(expected), (long long)len, msg);
   } else {
      fprintf(stderr, "[%s] emit failed: %s\n", msg,
              anvil_writer_error_message(anvil_builder_get_error(b)));
   }
}

// `add` to the document with a fresh scalar already created by the caller.
#define ADD(b, name, node) anvil_builder_add((b), NULL, (name), (node))

/* ---------------------------------------------------------------------- *
 * B01 - lifecycle, NULL safety, empty documents
 * ---------------------------------------------------------------------- */
static void test_b01_lifecycle(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   expect_emit(b, "#!aml\n\n", "B01: empty AML document");
   TestBit.is_equal_int(ANVIL_WRITER_OK, anvil_builder_get_error(b), "B01: emit succeeded");
   anvil_builder_dispose(b);
   b = anvil_builder_new(ANVIL_WRITER_AMP);
   expect_emit(b, "#!amp\n\n", "B01: empty AMP document");
   anvil_builder_dispose(b);

   anvil_builder_dispose(NULL);
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ARGUMENT, anvil_builder_get_error(NULL),
                        "B01: NULL builder reports INVALID_ARGUMENT");
   TestBit.is_null(anvil_builder_new((anvil_writer_dialect)99), "B01: unknown dialect rejected");
   TestBit.is_null(anvil_builder_null(NULL), "B01: NULL builder makes no node");
   TestBit.is_null(anvil_builder_emit(NULL, NULL), "B01: NULL builder emits nothing");
   TestBit.is_false(anvil_builder_append(NULL, NULL, NULL), "B01: NULL append fails");
   TestBit.is_null(anvil_builder_add(NULL, NULL, "x", NULL), "B01: NULL add fails");
}

/* ---------------------------------------------------------------------- *
 * B02 - every scalar kind: the same text test_writer.c's W02 expects
 * ---------------------------------------------------------------------- */
static void test_b02_scalars(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   ADD(b, "name", anvil_builder_bare(b, "David"));
   ADD(b, "n", anvil_builder_numeric(b, "-1.5e+3"));
   ADD(b, "yes", anvil_builder_bool(b, true));
   ADD(b, "no", anvil_builder_bool(b, false));
   ADD(b, "nothing", anvil_builder_null(b));
   ADD(b, "s", anvil_builder_string(b, "a \"q\" \\ b\n\t\r", 12));
   ADD(b, "empty", anvil_builder_string(b, "", 0));
   ADD(b, "tagged", anvil_builder_blob(b, "date", "2026-07-07", 10));
   ADD(b, "untagged", anvil_builder_blob(b, NULL, "raw", 3));
   ADD(b, "alias", anvil_builder_varref(b, "name"));
   ADD(b, "i", anvil_builder_int(b, INT64_MIN));
   ADD(b, "d", anvil_builder_double(b, 1e20));
   expect_emit(b,
               "#!aml\n\n"
               "name := David;\n"
               "n := -1.5e+3;\n"
               "yes := true;\n"
               "no := false;\n"
               "nothing := null;\n"
               "s := \"a \\\"q\\\" \\\\ b\\n\\t\\r\";\n"
               "empty := \"\";\n"
               "tagged := @date`2026-07-07`;\n"
               "untagged := `raw`;\n"
               "alias := $name;\n"
               "i := -9223372036854775808;\n"
               "d := 1e+20;\n",
               "B02: all scalar kinds");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B03 - collections and objects: W06's layout, built bottom-up
 * ---------------------------------------------------------------------- */
static void test_b03_collections(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);

   anvil_node tags = anvil_builder_array(b);
   anvil_builder_append(b, tags, anvil_builder_bare(b, "alpha"));
   anvil_builder_append(b, tags, anvil_builder_string(b, "be ta", 5));
   anvil_builder_append(b, tags, anvil_builder_int(b, 3));
   ADD(b, "tags", tags);

   anvil_node coords = anvil_builder_tuple(b);
   anvil_builder_append(b, coords, anvil_builder_int(b, 10));
   anvil_builder_append(b, coords, anvil_builder_int(b, 20));
   ADD(b, "coords", coords);

   anvil_node grid = anvil_builder_array(b);
   anvil_node pair = anvil_builder_tuple(b);
   anvil_builder_append(b, pair, anvil_builder_int(b, 1));
   anvil_builder_append(b, pair, anvil_builder_int(b, 2));
   anvil_node inner = anvil_builder_array(b);
   anvil_builder_append(b, inner, anvil_builder_int(b, 3));
   anvil_builder_append(b, grid, pair);
   anvil_builder_append(b, grid, inner);
   ADD(b, "grid", grid);

   // built inside-out: elements are filled before their parent is attached
   anvil_node first = anvil_builder_object(b);
   anvil_builder_add(b, first, "id", anvil_builder_int(b, 1));
   anvil_node second = anvil_builder_object(b);
   anvil_builder_add(b, second, "id", anvil_builder_int(b, 2));
   anvil_node second_tags = anvil_builder_array(b);
   anvil_builder_append(b, second_tags, anvil_builder_bare(b, "x"));
   anvil_builder_add(b, second, "tags", second_tags);
   anvil_node items = anvil_builder_array(b);
   anvil_builder_append(b, items, first);
   anvil_builder_append(b, items, second);
   anvil_node outer = anvil_builder_object(b);
   anvil_builder_add(b, outer, "items", items);
   ADD(b, "outer", outer);

   expect_emit(b,
               "#!aml\n\n"
               "tags := [alpha, \"be ta\", 3];\n"
               "coords := (10, 20);\n"
               "grid := [(1, 2), [3]];\n"
               "outer := {\n"
               "   items := [\n"
               "      {\n"
               "         id := 1;\n"
               "      },\n"
               "      {\n"
               "         id := 2;\n"
               "         tags := [x];\n"
               "      }\n"
               "   ];\n"
               "};\n",
               "B03: collections layout");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B04 - header, attributes and base: W04/W05's text. The include was added after the attributes,
 * yet is emitted first: the reader requires includes before module attributes (BR-2610-anvl-writer-003)
 * ---------------------------------------------------------------------- */
static void test_b04_header_attributes_base(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   TestBit.is_true(anvil_builder_attribute(b, "types", NULL), "B04: flag module attribute");
   TestBit.is_true(anvil_builder_attribute(b, "mixed", "true"), "B04: keyed module attribute");
   TestBit.is_true(anvil_builder_attribute_string(b, "note", "hello, world]", 13),
                   "B04: string module attribute");
   TestBit.is_true(anvil_builder_include(b, "base.anvl"), "B04: include");

   anvil_node server = anvil_builder_object(b);
   anvil_builder_add(b, server, "host", anvil_builder_bare(b, "localhost"));
   anvil_member sm = ADD(b, "server", server);
   TestBit.is_true(anvil_builder_member_attribute(b, sm, "env", "production"), "B04: attr env");
   TestBit.is_true(anvil_builder_member_attribute(b, sm, "active", NULL), "B04: attr flag");
   TestBit.is_true(anvil_builder_member_attribute_string(b, sm, "label", "a b", 3), "B04: attr string");

   anvil_node derived = anvil_builder_object(b);
   anvil_builder_add(b, derived, "port", anvil_builder_int(b, 8080));
   anvil_member dm = ADD(b, "derived", derived);
   TestBit.is_true(anvil_builder_set_base(b, dm, "server"), "B04: base");
   anvil_builder_member_attribute(b, dm, "x", NULL);

   expect_emit(b,
               "#!aml\n\n"
               "include \"base.anvl\";\n"
               "@[types]\n"
               "@[mixed=true]\n"
               "@[note=\"hello, world]\"]\n"
               "\n"
               "server @[env=production, active, label=\"a b\"] := {\n"
               "   host := localhost;\n"
               "};\n"
               "derived : server @[x] := {\n"
               "   port := 8080;\n"
               "};\n",
               "B04: header, attributes and base");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B05 - AMP: legal documents, and eager rejection of what AMP forbids
 * ---------------------------------------------------------------------- */
static void test_b05_amp(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AMP);
   ADD(b, "id", anvil_builder_int(b, 7));
   anvil_node tags = anvil_builder_array(b);
   anvil_builder_append(b, tags, anvil_builder_bare(b, "a"));
   anvil_builder_append(b, tags, anvil_builder_bare(b, "b"));
   ADD(b, "tags", tags);
   anvil_node pos = anvil_builder_tuple(b);
   anvil_builder_append(b, pos, anvil_builder_int(b, 1));
   anvil_builder_append(b, pos, anvil_builder_int(b, 2));
   ADD(b, "pos", pos);
   ADD(b, "data", anvil_builder_blob(b, "bin", "xyz", 3));
   expect_emit(b,
               "#!amp\n\n"
               "id := 7;\n"
               "tags := [a, b];\n"
               "pos := (1, 2);\n"
               "data := @bin`xyz`;\n",
               "B05: legal AMP document");

   TestBit.is_null(anvil_builder_object(b), "B05: object rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DIALECT, anvil_builder_get_error(b), "B05: object is DIALECT");
   TestBit.is_null(anvil_builder_varref(b, "x"), "B05: varref rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DIALECT, anvil_builder_get_error(b), "B05: varref is DIALECT");
   TestBit.is_false(anvil_builder_attribute(b, "a", NULL), "B05: module attribute rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DIALECT, anvil_builder_get_error(b), "B05: attribute is DIALECT");
   TestBit.is_false(anvil_builder_include(b, "x.anvl"), "B05: include rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DIALECT, anvil_builder_get_error(b), "B05: include is DIALECT");

   anvil_member m = ADD(b, "later", anvil_builder_int(b, 1));
   TestBit.is_not_null(m, "B05: member added");
   TestBit.is_false(anvil_builder_set_base(b, m, "x"), "B05: base rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DIALECT, anvil_builder_get_error(b), "B05: base is DIALECT");
   TestBit.is_false(anvil_builder_member_attribute(b, m, "k", NULL), "B05: member attribute rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DIALECT, anvil_builder_get_error(b),
                        "B05: member attribute is DIALECT");

   anvil_node outer = anvil_builder_array(b);
   anvil_node nested = anvil_builder_array(b);
   TestBit.is_false(anvil_builder_append(b, outer, nested), "B05: nested collection rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DIALECT, anvil_builder_get_error(b),
                        "B05: nested collection is DIALECT");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B06 - value validation happens at the call, and a failed call isn't sticky
 * ---------------------------------------------------------------------- */
static void test_b06_eager_validation(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
#define EXPECT_NULL(call, code, msg)                                                          \
   do {                                                                                        \
      TestBit.is_null((call), msg);                                                            \
      TestBit.is_equal_int((code), anvil_builder_get_error(b), msg);                          \
   } while (0)
   EXPECT_NULL(anvil_builder_numeric(b, "1."), ANVIL_WRITER_ERR_INVALID_NUMERIC, "B06: numeric '1.'");
   EXPECT_NULL(anvil_builder_numeric(b, NULL), ANVIL_WRITER_ERR_INVALID_ARGUMENT, "B06: NULL numeric");
   EXPECT_NULL(anvil_builder_double(b, 1.0 / 0.0), ANVIL_WRITER_ERR_INVALID_NUMERIC, "B06: infinity");
   EXPECT_NULL(anvil_builder_bare(b, "123"), ANVIL_WRITER_ERR_INVALID_BARE, "B06: bare '123'");
   EXPECT_NULL(anvil_builder_bare(b, "true"), ANVIL_WRITER_ERR_RESERVED_WORD, "B06: bare 'true'");
   EXPECT_NULL(anvil_builder_bare(b, "a b"), ANVIL_WRITER_ERR_INVALID_BARE, "B06: bare with space");
   EXPECT_NULL(anvil_builder_blob(b, "t", "a`b", 3), ANVIL_WRITER_ERR_INVALID_BLOB, "B06: blob backtick");
   EXPECT_NULL(anvil_builder_blob(b, "1x", "x", 1), ANVIL_WRITER_ERR_INVALID_BLOB, "B06: blob tag");
   EXPECT_NULL(anvil_builder_string(b, NULL, 3), ANVIL_WRITER_ERR_INVALID_ARGUMENT, "B06: NULL string");
   EXPECT_NULL(anvil_builder_varref(b, "no.dots"), ANVIL_WRITER_ERR_INVALID_IDENTIFIER, "B06: varref name");
   EXPECT_NULL(anvil_builder_varref(b, "include"), ANVIL_WRITER_ERR_RESERVED_WORD, "B06: varref keyword");

   anvil_node ok = anvil_builder_int(b, 1);
   EXPECT_NULL(anvil_builder_add(b, NULL, "bad name", ok), ANVIL_WRITER_ERR_INVALID_IDENTIFIER,
               "B06: statement name");
   EXPECT_NULL(anvil_builder_add(b, NULL, "include", ok), ANVIL_WRITER_ERR_RESERVED_WORD,
               "B06: reserved statement name");
   EXPECT_NULL(anvil_builder_add(b, NULL, NULL, ok), ANVIL_WRITER_ERR_INVALID_ARGUMENT,
               "B06: NULL statement name");
   EXPECT_NULL(anvil_builder_add(b, NULL, "x", NULL), ANVIL_WRITER_ERR_INVALID_ARGUMENT,
               "B06: NULL value");

   TestBit.is_false(anvil_builder_attribute(b, "bad key", NULL), "B06: module attribute key");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_IDENTIFIER, anvil_builder_get_error(b), "B06: key code");
   TestBit.is_false(anvil_builder_attribute(b, "k", "a,b"), "B06: module attribute value");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE, anvil_builder_get_error(b),
                        "B06: value code");
   TestBit.is_false(anvil_builder_include(b, ""), "B06: empty include");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_INCLUDE_PATH, anvil_builder_get_error(b),
                        "B06: include code");

   // none of that poisoned the builder, and a success clears the error
   TestBit.is_not_null(ADD(b, "fine", ok), "B06: valid call after failures succeeds");
   TestBit.is_equal_int(ANVIL_WRITER_OK, anvil_builder_get_error(b), "B06: success clears the error");
   expect_emit(b, "#!aml\n\nfine := 1;\n", "B06: only the valid statement was added");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B07 - structural rules: handles, attachment, cycles, names, bases
 * ---------------------------------------------------------------------- */
static void test_b07_structure(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_builder other = anvil_builder_new(ANVIL_WRITER_AML);

   anvil_node scalar = anvil_builder_int(b, 1);
   anvil_node arr = anvil_builder_array(b);
   TestBit.is_false(anvil_builder_append(b, scalar, anvil_builder_int(b, 2)), "B07: append to a scalar");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_STATE, anvil_builder_get_error(b), "B07: append to scalar is STATE");
   TestBit.is_false(anvil_builder_append(b, NULL, scalar), "B07: append to NULL");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ARGUMENT, anvil_builder_get_error(b),
                        "B07: append to NULL is INVALID_ARGUMENT");

   TestBit.is_true(anvil_builder_append(b, arr, scalar), "B07: first attach");
   TestBit.is_false(anvil_builder_append(b, arr, scalar), "B07: attaching an attached node");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_STATE, anvil_builder_get_error(b), "B07: re-attach is STATE");
   TestBit.is_null(ADD(b, "again", scalar), "B07: an element can't also be a statement value");

   TestBit.is_false(anvil_builder_append(b, arr, arr), "B07: a collection inside itself");
   anvil_node a1 = anvil_builder_array(b);
   anvil_node a2 = anvil_builder_array(b);
   anvil_builder_append(b, a1, a2);
   TestBit.is_false(anvil_builder_append(b, a2, a1), "B07: a cycle through an ancestor");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_STATE, anvil_builder_get_error(b), "B07: cycle is STATE");

   anvil_node o1 = anvil_builder_object(b);
   anvil_node o2 = anvil_builder_object(b);
   TestBit.is_not_null(anvil_builder_add(b, o1, "inner", o2), "B07: object inside object");
   TestBit.is_null(anvil_builder_add(b, o2, "back", o1), "B07: object cycle");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_STATE, anvil_builder_get_error(b), "B07: object cycle is STATE");

   anvil_node foreign = anvil_builder_int(other, 1);
   TestBit.is_false(anvil_builder_append(b, anvil_builder_array(b), foreign), "B07: node from another builder");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ARGUMENT, anvil_builder_get_error(b),
                        "B07: foreign node is INVALID_ARGUMENT");

   TestBit.is_null(anvil_builder_add(b, scalar, "x", anvil_builder_int(b, 1)), "B07: add to a non-object");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_STATE, anvil_builder_get_error(b), "B07: add to non-object is STATE");

   TestBit.is_not_null(ADD(b, "a", anvil_builder_int(b, 1)), "B07: first top-level 'a'");
   TestBit.is_null(ADD(b, "a", anvil_builder_int(b, 2)), "B07: duplicate top-level name");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_NAME, anvil_builder_get_error(b),
                        "B07: duplicate is DUPLICATE_NAME");
   anvil_node obj = anvil_builder_object(b);
   TestBit.is_not_null(anvil_builder_add(b, obj, "a", anvil_builder_int(b, 1)), "B07: nested 'a'");
   // BR-2610-anvl-parser-005: a repeated field in one object is invalid now (the reader rejects it)
   TestBit.is_null(anvil_builder_add(b, obj, "a", anvil_builder_int(b, 2)), "B07: nested repeat is rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_FIELD, anvil_builder_get_error(b),
                        "B07: nested repeat is DUPLICATE_FIELD");

   anvil_member scalar_member = ADD(b, "s", anvil_builder_int(b, 1));
   TestBit.is_false(anvil_builder_set_base(b, scalar_member, "a"), "B07: base on a scalar statement");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT, anvil_builder_get_error(b),
                        "B07: base on scalar is INHERITANCE_REQUIRES_OBJECT");
   anvil_member object_member = ADD(b, "o", obj);
   TestBit.is_false(anvil_builder_set_base(b, object_member, "bad name"), "B07: invalid base name");
   TestBit.is_false(anvil_builder_set_base(b, object_member, "true"), "B07: reserved base name");
   TestBit.is_true(anvil_builder_set_base(b, object_member, "a"), "B07: valid base");
   TestBit.is_false(anvil_builder_set_base(b, NULL, "a"), "B07: base on a NULL member");

   anvil_builder_dispose(other);
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B16 - BR-2610-anvl-parser-007: an attribute value must be a scalar
 * ---------------------------------------------------------------------- */
static void test_b16_attribute_values_are_scalars(void) {
   static const char *const bad[] = {"[x", "[x]", "(1)", "{a}", "{}", "()", "[]", "(1,2)", "@t`z`", "`z`", "@t``"};
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_member m = ADD(b, "s", anvil_builder_int(b, 1));
   for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      TestBit.is_false(anvil_builder_attribute(b, "k", bad[i]), bad[i]);
      TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE, anvil_builder_get_error(b),
                           "B16: module attribute code");
      TestBit.is_false(anvil_builder_member_attribute(b, m, "k", bad[i]), bad[i]);
      TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE, anvil_builder_get_error(b),
                           "B16: member attribute code");
   }
   TestBit.is_true(anvil_builder_attribute(b, "a", "done"), "B16: bare word module attribute");
   TestBit.is_true(anvil_builder_attribute(b, "b", "5"), "B16: number module attribute");
   TestBit.is_true(anvil_builder_member_attribute(b, m, "c", "true"), "B16: bool member attribute");
   TestBit.is_true(anvil_builder_member_attribute(b, m, "d", "\"{a}\""), "B16: quoted brackets");
   TestBit.is_true(anvil_builder_member_attribute_string(b, m, "e", "(1)", 3), "B16: string text with brackets");
   size_t len = 0;
   const char *text = anvil_builder_emit(b, &len);
   TestBit.is_not_null(text, "B16: emits");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B17 - BR-2610-anvl-writer-002: a same-document non-object base fails at write/emit
 * ---------------------------------------------------------------------- */
static void test_b17_base_must_be_object_at_emit(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   ADD(b, "n", anvil_builder_int(b, 1));
   anvil_node arr = anvil_builder_array(b);
   anvil_builder_append(b, arr, anvil_builder_int(b, 1));
   ADD(b, "arr", arr);
   ADD(b, "bare", anvil_builder_bare(b, "word"));
   anvil_node o = anvil_builder_object(b);
   anvil_builder_add(b, o, "f", anvil_builder_int(b, 1));
   ADD(b, "obj", o);
   const char *bases[] = {"n", "arr", "bare"};
   for (size_t i = 0; i < 3; i++) {
      anvil_node d = anvil_builder_object(b);
      anvil_builder_add(b, d, "g", anvil_builder_int(b, 2));
      anvil_member dm = ADD(b, i == 0 ? "d0" : i == 1 ? "d1" : "d2", d);
      TestBit.is_true(anvil_builder_set_base(b, dm, bases[i]), "B17: set_base itself accepts it");
      TestBit.is_null(anvil_builder_emit(b, NULL), bases[i]);
      TestBit.is_equal_int(ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT, anvil_builder_get_error(b),
                           "B17: emit reports INHERITANCE_REQUIRES_OBJECT");
      TestBit.is_true(anvil_builder_set_base(b, dm, "obj"), "B17: repointing the base to an object");
   }
   const char *text = anvil_builder_emit(b, NULL);
   TestBit.is_not_null(text, "B17: an object base emits");

   // a base added after the member that derives from it, and a nested derived member
   anvil_builder c = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node outer = anvil_builder_object(c);
   anvil_node inner = anvil_builder_object(c);
   anvil_builder_add(c, inner, "g", anvil_builder_int(c, 1));
   anvil_member nested = anvil_builder_add(c, outer, "inner", inner);
   anvil_builder_set_base(c, nested, "later");
   ADD(c, "outer", outer);
   ADD(c, "later", anvil_builder_int(c, 3));
   TestBit.is_null(anvil_builder_emit(c, NULL), "B17: a nested member's scalar base, declared later");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT, anvil_builder_get_error(c),
                        "B17: nested, late-declared base is refused too");
   anvil_builder_dispose(c);

   // a base the builder does not hold (from an include) is accepted
   anvil_builder e = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_builder_include(e, "base.anvl");
   anvil_node eo = anvil_builder_object(e);
   anvil_builder_add(e, eo, "g", anvil_builder_int(e, 1));
   anvil_member em = ADD(e, "d", eo);
   anvil_builder_set_base(e, em, "from_include");
   TestBit.is_not_null(anvil_builder_emit(e, NULL), "B17: a base outside the builder is accepted");
   anvil_builder_dispose(e);
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B08 - what's only knowable at emit, and recovery by completing the tree
 * ---------------------------------------------------------------------- */
static void test_b08_emit_time_errors(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node arr = anvil_builder_array(b);
   ADD(b, "a", arr);
   TestBit.is_null(anvil_builder_emit(b, NULL), "B08: empty array fails at emit");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_EMPTY_COLLECTION, anvil_builder_get_error(b),
                        "B08: empty array is EMPTY_COLLECTION");
   anvil_builder_append(b, arr, anvil_builder_int(b, 1));
   expect_emit(b, "#!aml\n\na := [1];\n", "B08: completing the array lets emit succeed");

   anvil_node tup = anvil_builder_tuple(b);
   anvil_builder_append(b, tup, anvil_builder_int(b, 1));
   ADD(b, "t", tup);
   TestBit.is_null(anvil_builder_emit(b, NULL), "B08: 1-tuple fails at emit");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_EMPTY_COLLECTION, anvil_builder_get_error(b),
                        "B08: 1-tuple is EMPTY_COLLECTION");
   anvil_builder_append(b, tup, anvil_builder_int(b, 2));
   expect_emit(b, "#!aml\n\na := [1];\nt := (1, 2);\n", "B08: completing the tuple");

   ADD(b, "o", anvil_builder_object(b));
   TestBit.is_null(anvil_builder_emit(b, NULL), "B08: empty object fails at emit");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_EMPTY_COLLECTION, anvil_builder_get_error(b),
                        "B08: empty object is EMPTY_COLLECTION");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B09 - nesting depth
 * ---------------------------------------------------------------------- */
static anvil_builder nested_arrays(int levels) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node inner = anvil_builder_int(b, 1);
   for (int i = 0; i < levels; i++) {
      anvil_node wrap = anvil_builder_array(b);
      anvil_builder_append(b, wrap, inner);
      inner = wrap;
   }
   ADD(b, "deep", inner);
   return b;
}

static void test_b09_depth(void) {
   anvil_builder b = nested_arrays(ANVIL_BUILDER_MAX_DEPTH);
   size_t len = 0;
   const char *text = anvil_builder_emit(b, &len);
   TestBit.is_not_null(text, "B09: nesting at the limit emits");
   TestBit.is_true(text && len == strlen("#!aml\n\ndeep := ") + 2 * ANVIL_BUILDER_MAX_DEPTH + 1 +
                                       strlen(";\n"),
                   "B09: nesting at the limit has every bracket");
   anvil_builder_dispose(b);

   b = nested_arrays(ANVIL_BUILDER_MAX_DEPTH + 1);
   TestBit.is_null(anvil_builder_emit(b, NULL), "B09: nesting past the limit fails");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DEPTH_EXCEEDED, anvil_builder_get_error(b),
                        "B09: past the limit is DEPTH_EXCEEDED");
   anvil_builder_dispose(b);

   // a very deep tree must be disposed without recursing
   b = nested_arrays(200000);
   anvil_builder_dispose(b);
   TestBit.is_true(true, "B09: a 200000-deep tree disposes without blowing the stack");
}

/* ---------------------------------------------------------------------- *
 * B10 - find, and incremental building
 * ---------------------------------------------------------------------- */
static void test_b10_find_and_incremental(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node cfg = anvil_builder_object(b);
   anvil_builder_add(b, cfg, "host", anvil_builder_bare(b, "h"));
   anvil_member cfg_member = ADD(b, "cfg", cfg);
   TestBit.is_true(anvil_builder_find(b, NULL, "cfg") == cfg_member, "B10: find a top-level statement");
   TestBit.is_null(anvil_builder_find(b, NULL, "missing"), "B10: find a missing statement");
   TestBit.is_not_null(anvil_builder_find(b, cfg, "host"), "B10: find inside an object");
   TestBit.is_null(anvil_builder_find(b, cfg, "port"), "B10: find a missing field");
   TestBit.is_null(anvil_builder_find(b, NULL, NULL), "B10: find with a NULL name");

   anvil_builder_add(b, cfg, "port", anvil_builder_int(b, 1));
   expect_emit(b, "#!aml\n\ncfg := {\n   host := h;\n   port := 1;\n};\n", "B10: first emit");
   anvil_builder_add(b, cfg, "tls", anvil_builder_bool(b, true));
   anvil_builder_member_attribute(b, cfg_member, "late", NULL);
   expect_emit(b,
               "#!aml\n\ncfg @[late] := {\n   host := h;\n   port := 1;\n   tls := true;\n};\n",
               "B10: changed after emit, emitted again");
   expect_emit(b,
               "#!aml\n\ncfg @[late] := {\n   host := h;\n   port := 1;\n   tls := true;\n};\n",
               "B10: emitting twice is stable");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B11 - anvil_builder_write into a caller's writer
 * ---------------------------------------------------------------------- */
static void test_b11_write_into_writer(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_builder_attribute(b, "doc", NULL);
   ADD(b, "x", anvil_builder_int(b, 1));

   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   TestBit.is_true(anvil_builder_write(b, w), "B11: write into a fresh writer");
   TestBit.is_true(anvil_writer_finish(w), "B11: the caller finishes");
   size_t len = 0;
   const char *text = anvil_writer_data(w, &len);
   TestBit.is_equal_str("#!aml\n\n@[doc]\n\nx := 1;\n", text ? text : "", "B11: writer text");
   anvil_writer_dispose(w);

   anvil_builder plain = anvil_builder_new(ANVIL_WRITER_AML);
   ADD(plain, "x", anvil_builder_int(plain, 1));
   w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "x", NULL);
   anvil_writer_int(w, 9);
   TestBit.is_false(anvil_builder_write(plain, w), "B11: a writer that already has 'x' rejects it");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_NAME, anvil_builder_get_error(plain),
                        "B11: writer's error is reported by the builder");
   anvil_writer_dispose(w);
   anvil_builder_dispose(plain);

   TestBit.is_false(anvil_builder_write(b, NULL), "B11: NULL writer");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ARGUMENT, anvil_builder_get_error(b),
                        "B11: NULL writer is INVALID_ARGUMENT");

   anvil_writer amp = anvil_writer_new(ANVIL_WRITER_AMP);
   TestBit.is_false(anvil_builder_write(b, amp), "B11: an AML builder into an AMP writer");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DIALECT, anvil_builder_get_error(b), "B11: mismatch is DIALECT");
   anvil_writer_dispose(amp);
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B12 - many statements: the top-level name set grows, repeats still found
 * ---------------------------------------------------------------------- */
static void test_b12_many_statements(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   char name[16];
   for (int i = 0; i < 20000; i++) {
      snprintf(name, sizeof name, "n%d", i);
      if (!ADD(b, name, anvil_builder_int(b, i))) {
         TestBit.is_true(false, "B12: add failed");
         break;
      }
   }
   TestBit.is_null(ADD(b, "n12345", anvil_builder_int(b, 0)), "B12: repeat after growth rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_NAME, anvil_builder_get_error(b), "B12: DUPLICATE_NAME");
   size_t len = 0;
   const char *text = anvil_builder_emit(b, &len);
   TestBit.is_not_null(text, "B12: 20000 statements emit");
   TestBit.is_true(text && strstr(text, "n19999 := 19999;\n") != NULL, "B12: last statement present");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B13 - vtable fields are pointer-identical to the flat functions
 * ---------------------------------------------------------------------- */
static void test_b13_vtable_matches_flat(void) {
   TestBit.is_true(Builder.create == anvil_builder_new, "B13: create");
   TestBit.is_true(Builder.dispose == anvil_builder_dispose, "B13: dispose");
   TestBit.is_true(Builder.get_error == anvil_builder_get_error, "B13: get_error");
   TestBit.is_true(Builder.attribute == anvil_builder_attribute, "B13: attribute");
   TestBit.is_true(Builder.attribute_string == anvil_builder_attribute_string, "B13: attribute_string");
   TestBit.is_true(Builder.include == anvil_builder_include, "B13: include");
   TestBit.is_true(Builder.null_value == anvil_builder_null, "B13: null_value");
   TestBit.is_true(Builder.bool_value == anvil_builder_bool, "B13: bool_value");
   TestBit.is_true(Builder.numeric == anvil_builder_numeric, "B13: numeric");
   TestBit.is_true(Builder.int_value == anvil_builder_int, "B13: int_value");
   TestBit.is_true(Builder.double_value == anvil_builder_double, "B13: double_value");
   TestBit.is_true(Builder.string == anvil_builder_string, "B13: string");
   TestBit.is_true(Builder.bare == anvil_builder_bare, "B13: bare");
   TestBit.is_true(Builder.blob == anvil_builder_blob, "B13: blob");
   TestBit.is_true(Builder.varref == anvil_builder_varref, "B13: varref");
   TestBit.is_true(Builder.array == anvil_builder_array, "B13: array");
   TestBit.is_true(Builder.tuple == anvil_builder_tuple, "B13: tuple");
   TestBit.is_true(Builder.object == anvil_builder_object, "B13: object");
   TestBit.is_true(Builder.append == anvil_builder_append, "B13: append");
   TestBit.is_true(Builder.add == anvil_builder_add, "B13: add");
   TestBit.is_true(Builder.set_base == anvil_builder_set_base, "B13: set_base");
   TestBit.is_true(Builder.member_attribute == anvil_builder_member_attribute, "B13: member_attribute");
   TestBit.is_true(Builder.member_attribute_string == anvil_builder_member_attribute_string,
                   "B13: member_attribute_string");
   TestBit.is_true(Builder.find == anvil_builder_find, "B13: find");
   TestBit.is_true(Builder.write == anvil_builder_write, "B13: write");
   TestBit.is_true(Builder.emit == anvil_builder_emit, "B13: emit");
}

/* ---------------------------------------------------------------------- *
 * B14 (BR-2610-anvl-parser-005) - a field repeated within one object, at any
 * depth, and an attribute key repeated on one statement or in the module
 * header, are rejected: the reader rejects both
 * ---------------------------------------------------------------------- */
static void test_b14_duplicates(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node o1 = anvil_builder_object(b);
   TestBit.is_not_null(anvil_builder_add(b, o1, "a", anvil_builder_int(b, 1)), "B14: depth 1 first");
   TestBit.is_not_null(anvil_builder_add(b, o1, "b", anvil_builder_int(b, 1)), "B14: depth 1 other");
   TestBit.is_null(anvil_builder_add(b, o1, "a", anvil_builder_int(b, 2)), "B14: depth 1 repeat");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_FIELD, anvil_builder_get_error(b),
                        "B14: depth 1 repeat is DUPLICATE_FIELD");
   anvil_node o2 = anvil_builder_object(b);
   TestBit.is_not_null(anvil_builder_add(b, o1, "inner", o2), "B14: nested object");
   TestBit.is_not_null(anvil_builder_add(b, o2, "c", anvil_builder_int(b, 1)), "B14: depth 2 first");
   TestBit.is_null(anvil_builder_add(b, o2, "c", anvil_builder_int(b, 2)), "B14: depth 2 repeat");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_FIELD, anvil_builder_get_error(b),
                        "B14: depth 2 repeat is DUPLICATE_FIELD");
   TestBit.is_not_null(anvil_builder_add(b, o2, "a", anvil_builder_int(b, 3)), "B14: same name at another depth");

   anvil_node arr = anvil_builder_array(b);
   anvil_node e1 = anvil_builder_object(b), e2 = anvil_builder_object(b);
   TestBit.is_not_null(anvil_builder_add(b, e1, "k", anvil_builder_int(b, 1)), "B14: array element 1");
   TestBit.is_not_null(anvil_builder_add(b, e2, "k", anvil_builder_int(b, 2)), "B14: same name in element 2");
   TestBit.is_null(anvil_builder_add(b, e2, "k", anvil_builder_int(b, 3)), "B14: repeat inside an array element");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_FIELD, anvil_builder_get_error(b),
                        "B14: array element repeat is DUPLICATE_FIELD");
   anvil_builder_append(b, arr, e1);
   anvil_builder_append(b, arr, e2);
   anvil_node tup = anvil_builder_tuple(b);
   anvil_node t1 = anvil_builder_int(b, 1), t2 = anvil_builder_object(b);
   TestBit.is_not_null(anvil_builder_add(b, t2, "z", anvil_builder_int(b, 1)), "B14: tuple object first");
   TestBit.is_null(anvil_builder_add(b, t2, "z", anvil_builder_int(b, 2)), "B14: repeat inside a tuple object");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_FIELD, anvil_builder_get_error(b),
                        "B14: tuple object repeat is DUPLICATE_FIELD");
   anvil_builder_append(b, tup, t1);
   anvil_builder_append(b, tup, t2);

   anvil_member m1 = ADD(b, "obj", o1);
   ADD(b, "arr", arr);
   ADD(b, "tup", tup);
   // statement attributes
   TestBit.is_true(anvil_builder_member_attribute(b, m1, "k", NULL), "B14: attribute");
   TestBit.is_true(anvil_builder_member_attribute(b, m1, "j", "1"), "B14: another attribute");
   TestBit.is_false(anvil_builder_member_attribute(b, m1, "k", "2"), "B14: repeated attribute key");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_ATTRIBUTE_KEY, anvil_builder_get_error(b),
                        "B14: repeated attribute is DUPLICATE_ATTRIBUTE_KEY");
   TestBit.is_false(anvil_builder_member_attribute_string(b, m1, "j", "s", 1), "B14: repeat as a string");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_ATTRIBUTE_KEY, anvil_builder_get_error(b),
                        "B14: repeat as a string is DUPLICATE_ATTRIBUTE_KEY");
   anvil_member m2 = ADD(b, "other", anvil_builder_int(b, 1));
   TestBit.is_true(anvil_builder_member_attribute(b, m2, "k", NULL), "B14: same key on another statement");
   // module attributes
   TestBit.is_true(anvil_builder_attribute(b, "k", NULL), "B14: module attribute");
   TestBit.is_true(anvil_builder_include(b, "k"), "B14: an include path is not an attribute key");
   TestBit.is_false(anvil_builder_attribute(b, "k", "1"), "B14: repeated module attribute");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_ATTRIBUTE_KEY, anvil_builder_get_error(b),
                        "B14: repeated module attribute is DUPLICATE_ATTRIBUTE_KEY");
   TestBit.is_false(anvil_builder_attribute_string(b, "k", "s", 1), "B14: repeated module attribute, string");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_ATTRIBUTE_KEY, anvil_builder_get_error(b),
                        "B14: repeated module attribute (string) is DUPLICATE_ATTRIBUTE_KEY");
   size_t len = 0;
   TestBit.is_not_null(anvil_builder_emit(b, &len), "B14: what was accepted still emits");

   // enough fields to force the set to grow, then one repeat
   anvil_builder big = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node bo = anvil_builder_object(big);
   char name[16];
   bool all = true;
   for (int i = 0; i < 3000; i++) {
      snprintf(name, sizeof name, "f%d", i);
      all = all && anvil_builder_add(big, bo, name, anvil_builder_int(big, i)) != NULL;
   }
   TestBit.is_true(all, "B14: 3000 distinct fields");
   TestBit.is_null(anvil_builder_add(big, bo, "f1234", anvil_builder_int(big, 0)), "B14: repeat after growth");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_FIELD, anvil_builder_get_error(big),
                        "B14: repeat after growth is DUPLICATE_FIELD");
   anvil_builder_dispose(big);
   anvil_builder_dispose(b);
}

/* B15 (BR-2610-anvl-parser-005) - keywords and reserved words are refused as names at any level */
static void test_b15_keyword_names(void) {
   const anvil_writer_err_code R = ANVIL_WRITER_ERR_RESERVED_WORD;
   const char *words[] = {"include", "import", "vars", "true", "false", "null"};
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node o = anvil_builder_object(b);
   anvil_node deep = anvil_builder_object(b);
   anvil_member holder = anvil_builder_add(b, o, "p", deep);
   TestBit.is_not_null(holder, "B15: a plain field");
   anvil_member m = ADD(b, "x", anvil_builder_int(b, 1));
   for (size_t i = 0; i < 6; i++) {
      TestBit.is_false(anvil_builder_attribute(b, words[i], NULL), words[i]);
      TestBit.is_equal_int(R, anvil_builder_get_error(b), words[i]);
      TestBit.is_false(anvil_builder_attribute_string(b, words[i], "s", 1), words[i]);
      TestBit.is_equal_int(R, anvil_builder_get_error(b), words[i]);
      TestBit.is_false(anvil_builder_member_attribute(b, m, words[i], NULL), words[i]);
      TestBit.is_equal_int(R, anvil_builder_get_error(b), words[i]);
      TestBit.is_false(anvil_builder_member_attribute_string(b, m, words[i], "s", 1), words[i]);
      TestBit.is_equal_int(R, anvil_builder_get_error(b), words[i]);
      TestBit.is_null(ADD(b, words[i], anvil_builder_int(b, 1)), words[i]);
      TestBit.is_equal_int(R, anvil_builder_get_error(b), words[i]);
      TestBit.is_null(anvil_builder_add(b, o, words[i], anvil_builder_int(b, 1)), words[i]);
      TestBit.is_equal_int(R, anvil_builder_get_error(b), words[i]);
      TestBit.is_null(anvil_builder_add(b, deep, words[i], anvil_builder_int(b, 1)), words[i]);
      TestBit.is_equal_int(R, anvil_builder_get_error(b), words[i]);
   }
   TestBit.is_true(anvil_builder_member_attribute(b, m, "included", NULL), "B15: a longer word is fine");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B18 - BR-2610-anvl-writer-004: a base cycle among the top-level members fails at write/emit
 * ---------------------------------------------------------------------- */
// A top-level object member with one field, named `name`.
static anvil_member obj_member(anvil_builder b, const char *name) {
   anvil_node o = anvil_builder_object(b);
   anvil_builder_add(b, o, "f", anvil_builder_int(b, 1));
   return ADD(b, name, o);
}

static void expect_emit_error(anvil_builder b, anvil_writer_err_code code, const char *msg) {
   TestBit.is_null(anvil_builder_emit(b, NULL), msg);
   TestBit.is_equal_int(code, anvil_builder_get_error(b), msg);
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   TestBit.is_false(anvil_builder_write(b, w), msg);
   TestBit.is_equal_int(code, anvil_builder_get_error(b), msg);
   TestBit.is_null(anvil_writer_data(w, NULL), msg); // nothing was written
   anvil_writer_dispose(w);
}

static void test_b18_base_cycles(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_member d = obj_member(b, "d");
   TestBit.is_true(anvil_builder_set_base(b, d, "d"), "B18: set_base itself accepts a self base");
   expect_emit_error(b, ANVIL_WRITER_ERR_INHERITANCE_CYCLE, "B18: a statement based on itself");
   anvil_builder_dispose(b);

   b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_member a = obj_member(b, "a"), bb = obj_member(b, "b"), c = obj_member(b, "c");
   anvil_builder_set_base(b, a, "b");
   anvil_builder_set_base(b, bb, "a");
   expect_emit_error(b, ANVIL_WRITER_ERR_INHERITANCE_CYCLE, "B18: a -> b -> a");
   anvil_builder_set_base(b, bb, "c");
   TestBit.is_not_null(anvil_builder_emit(b, NULL), "B18: a chain a -> b -> c is not a cycle");
   anvil_builder_set_base(b, c, "a");
   expect_emit_error(b, ANVIL_WRITER_ERR_INHERITANCE_CYCLE, "B18: a -> b -> c -> a");
   anvil_builder_set_base(b, c, "b"); // c -> b -> c ... and a -> b
   expect_emit_error(b, ANVIL_WRITER_ERR_INHERITANCE_CYCLE, "B18: a member that leads into a cycle it is not part of");
   anvil_builder_dispose(b);

   // an include does not excuse a cycle
   b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_builder_include(b, "x.anvl");
   anvil_member s = obj_member(b, "s");
   anvil_builder_set_base(b, s, "s");
   expect_emit_error(b, ANVIL_WRITER_ERR_INHERITANCE_CYCLE, "B18: a cycle with an include");
   anvil_builder_dispose(b);

   // a derived chain declared in any order is fine
   b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_member z = obj_member(b, "z"), y = obj_member(b, "y"), x = obj_member(b, "x");
   anvil_builder_set_base(b, z, "y");
   anvil_builder_set_base(b, y, "x");
   (void)x;
   TestBit.is_not_null(anvil_builder_emit(b, NULL), "B18: z -> y -> x emits");
   anvil_builder_dispose(b);

   // the non-object check still comes first
   b = anvil_builder_new(ANVIL_WRITER_AML);
   ADD(b, "n", anvil_builder_int(b, 1));
   anvil_member p = obj_member(b, "p"), q = obj_member(b, "q");
   anvil_builder_set_base(b, p, "q");
   anvil_builder_set_base(b, q, "p");
   anvil_member r = obj_member(b, "r");
   anvil_builder_set_base(b, r, "n");
   expect_emit_error(b, ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT, "B18: a scalar base is reported before a cycle");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B19 - BR-2610-anvl-writer-004: with no include, a base that names no top-level member fails at
 * write/emit (the reader's MISSING_BASE); with any include it is accepted
 * ---------------------------------------------------------------------- */
static void test_b19_missing_base(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_member d = obj_member(b, "d");
   TestBit.is_true(anvil_builder_set_base(b, d, "nothere"), "B19: set_base itself accepts it");
   expect_emit_error(b, ANVIL_WRITER_ERR_MISSING_BASE, "B19: a base that names nothing, no include");
   anvil_builder_include(b, "base.anvl");
   TestBit.is_not_null(anvil_builder_emit(b, NULL), "B19: an include added later makes it acceptable");
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   TestBit.is_true(anvil_builder_write(b, w), "B19: write accepts it too");
   anvil_writer_dispose(w);
   anvil_builder_dispose(b);

   // declared later, or on a nested member whose base is a top-level name
   b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node outer = anvil_builder_object(b);
   anvil_node inner = anvil_builder_object(b);
   anvil_builder_add(b, inner, "g", anvil_builder_int(b, 1));
   anvil_member nested = anvil_builder_add(b, outer, "inner", inner);
   anvil_builder_set_base(b, nested, "later");
   ADD(b, "outer", outer);
   expect_emit_error(b, ANVIL_WRITER_ERR_MISSING_BASE, "B19: nested member, base declared nowhere");
   obj_member(b, "later");
   TestBit.is_not_null(anvil_builder_emit(b, NULL), "B19: the base declared later is found");
   anvil_builder_dispose(b);

   // a sibling is not in scope: the reader looks bases up among the top-level names only
   b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node o = anvil_builder_object(b);
   anvil_node sib = anvil_builder_object(b);
   anvil_builder_add(b, sib, "g", anvil_builder_int(b, 1));
   anvil_builder_add(b, o, "sib", sib);
   anvil_node der = anvil_builder_object(b);
   anvil_builder_add(b, der, "g", anvil_builder_int(b, 1));
   anvil_member dm = anvil_builder_add(b, o, "der", der);
   anvil_builder_set_base(b, dm, "sib");
   ADD(b, "o", o);
   expect_emit_error(b, ANVIL_WRITER_ERR_MISSING_BASE, "B19: a sibling base");
   anvil_builder_dispose(b);

   // a member of an object that is never attached counts too
   b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node loose = anvil_builder_object(b);
   anvil_node ld = anvil_builder_object(b);
   anvil_builder_add(b, ld, "g", anvil_builder_int(b, 1));
   anvil_member lm = anvil_builder_add(b, loose, "ld", ld);
   anvil_builder_set_base(b, lm, "nothere");
   expect_emit_error(b, ANVIL_WRITER_ERR_MISSING_BASE, "B19: an unattached member with a missing base");
   anvil_builder_dispose(b);

   // the non-object check comes before the missing-base check
   b = anvil_builder_new(ANVIL_WRITER_AML);
   ADD(b, "n", anvil_builder_int(b, 1));
   anvil_member m1 = obj_member(b, "m1"), m2 = obj_member(b, "m2");
   anvil_builder_set_base(b, m1, "gone");
   anvil_builder_set_base(b, m2, "n");
   expect_emit_error(b, ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT, "B19: a scalar base is reported first");
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * B20 - BR-2610-anvl-writer-005: a member handle from another builder is refused
 * ---------------------------------------------------------------------- */
static void test_b20_foreign_member(void) {
   anvil_builder b1 = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_builder b2 = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_member m = obj_member(b1, "m");
   TestBit.is_false(anvil_builder_set_base(b2, m, "x"), "B20: set_base with a foreign member");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ARGUMENT, anvil_builder_get_error(b2), "B20: set_base is INVALID_ARGUMENT");
   TestBit.is_false(anvil_builder_member_attribute(b2, m, "k", "v"), "B20: member_attribute with a foreign member");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ARGUMENT, anvil_builder_get_error(b2), "B20: member_attribute is INVALID_ARGUMENT");
   TestBit.is_false(anvil_builder_member_attribute_string(b2, m, "k", "v", 1), "B20: member_attribute_string with a foreign member");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ARGUMENT, anvil_builder_get_error(b2), "B20: member_attribute_string is INVALID_ARGUMENT");
   expect_emit(b1, "#!aml\n\nm := {\n   f := 1;\n};\n", "B20: the member of b1 is unchanged");
   TestBit.is_true(anvil_builder_member_attribute(b1, m, "k", "v"), "B20: b1 can still use it");
   // the foreign check comes before the dialect check
   anvil_builder amp = anvil_builder_new(ANVIL_WRITER_AMP);
   TestBit.is_false(anvil_builder_set_base(amp, m, "x"), "B20: foreign member into an AMP builder");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ARGUMENT, anvil_builder_get_error(amp), "B20: foreign before DIALECT");
   anvil_builder_dispose(amp);
   anvil_builder_dispose(b2);
   anvil_builder_dispose(b1);
}

/* ---------------------------------------------------------------------- *
 * B21 - the base checks of write/emit are linear in the number of members: a long chain, bases declared
 * later, and a ring, all under a generous bound (the first version walked every chain with a linear find,
 * so 3000 members took seconds; 100k must not blow the stack either)
 * ---------------------------------------------------------------------- */
static double seconds_since(const struct timespec *t0) {
   struct timespec t1;
   clock_gettime(CLOCK_MONOTONIC, &t1);
   return (double)(t1.tv_sec - t0->tv_sec) + (double)(t1.tv_nsec - t0->tv_nsec) / 1e9;
}

// n top-level object members m0..m(n-1); member i has base m(i+1), the last one none (or m0 when `ring`).
static anvil_builder chain_builder(size_t n, bool ring) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_member *ms = malloc(n * sizeof *ms);
   char name[32];
   for (size_t i = 0; i < n; i++) {
      snprintf(name, sizeof name, "m%zu", i);
      anvil_node o = anvil_builder_object(b);
      anvil_builder_add(b, o, "f", anvil_builder_int(b, 1));
      ms[i] = ADD(b, name, o);
   }
   for (size_t i = 0; i < n; i++) {
      if (i + 1 < n || ring) {
         snprintf(name, sizeof name, "m%zu", (i + 1) % n);
         anvil_builder_set_base(b, ms[i], name);
      }
   }
   free(ms);
   return b;
}

static void test_b21_base_checks_are_linear(void) {
   struct timespec t0;
   clock_gettime(CLOCK_MONOTONIC, &t0);
   anvil_builder b = chain_builder(3000, false);
   size_t len = 0;
   const char *text = anvil_builder_emit(b, &len);
   TestBit.is_not_null(text, "B21: a 3000-member chain emits");
   TestBit.is_true(seconds_since(&t0) < 2.0, "B21: a 3000-member chain emits in well under 2 s");
   anvil_builder_dispose(b);

   // a long chain, a ring and a chain that ends in a cycle: iterative, so the stack does not matter
   b = chain_builder(100000, false);
   clock_gettime(CLOCK_MONOTONIC, &t0); // the emit alone; a quadratic check would take minutes even natively
   TestBit.is_not_null(anvil_builder_emit(b, NULL), "B21: a 100000-member chain emits");
   TestBit.is_true(seconds_since(&t0) < 60.0, "B21: a 100000-member chain emits in well under 60 s");
   anvil_builder_dispose(b);
   b = chain_builder(100000, true);
   TestBit.is_null(anvil_builder_emit(b, NULL), "B21: a 100000-member ring is refused");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INHERITANCE_CYCLE, anvil_builder_get_error(b), "B21: the ring is a cycle");
   anvil_builder_dispose(b);
   b = chain_builder(50000, false);
   anvil_member last = anvil_builder_find(b, NULL, "m49999");
   anvil_builder_set_base(b, last, "m25000"); // the tail loops back into the middle of the chain
   TestBit.is_null(anvil_builder_emit(b, NULL), "B21: a chain that ends in a cycle is refused");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INHERITANCE_CYCLE, anvil_builder_get_error(b), "B21: the tail cycle");
   anvil_builder_dispose(b);

}

int main(void) {
   TestBit.run_ex("B01_lifecycle", NULL, test_b01_lifecycle, th);
   TestBit.run_ex("B02_scalars", NULL, test_b02_scalars, th);
   TestBit.run_ex("B03_collections", NULL, test_b03_collections, th);
   TestBit.run_ex("B04_header_attributes_base", NULL, test_b04_header_attributes_base, th);
   TestBit.run_ex("B05_amp", NULL, test_b05_amp, th);
   TestBit.run_ex("B06_eager_validation", NULL, test_b06_eager_validation, th);
   TestBit.run_ex("B07_structure", NULL, test_b07_structure, th);
   TestBit.run_ex("B08_emit_time_errors", NULL, test_b08_emit_time_errors, th);
   TestBit.run_ex("B09_depth", NULL, test_b09_depth, th);
   TestBit.run_ex("B10_find_and_incremental", NULL, test_b10_find_and_incremental, th);
   TestBit.run_ex("B11_write_into_writer", NULL, test_b11_write_into_writer, th);
   TestBit.run_ex("B12_many_statements", NULL, test_b12_many_statements, th);
   TestBit.run_ex("B13_vtable_matches_flat", NULL, test_b13_vtable_matches_flat, th);
   TestBit.run_ex("B14_duplicates", NULL, test_b14_duplicates, th);
   TestBit.run_ex("B15_keyword_names", NULL, test_b15_keyword_names, th);
   TestBit.run_ex("B16_attribute_values_are_scalars", NULL, test_b16_attribute_values_are_scalars, th);
   TestBit.run_ex("B17_base_must_be_object_at_emit", NULL, test_b17_base_must_be_object_at_emit, th);
   TestBit.run_ex("B18_base_cycles", NULL, test_b18_base_cycles, th);
   TestBit.run_ex("B19_missing_base", NULL, test_b19_missing_base, th);
   TestBit.run_ex("B20_foreign_member", NULL, test_b20_foreign_member, th);
   TestBit.run_ex("B21_base_checks_are_linear", NULL, test_b21_base_checks_are_linear, th);
   return TestBit.report();
}
