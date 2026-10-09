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
 * B04 - header, attributes and base: W04/W05's text
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
               "@[types]\n"
               "@[mixed=true]\n"
               "@[note=\"hello, world]\"]\n"
               "include \"base.anvl\";\n"
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
   TestBit.is_not_null(anvil_builder_add(b, obj, "a", anvil_builder_int(b, 2)), "B07: nested repeat is legal");

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
   return TestBit.report();
}
