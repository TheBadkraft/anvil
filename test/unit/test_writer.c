/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_writer.c - Unit tests for the streaming writer (anvil_writer.h)   *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_writer.c                                          *
 * ---------------------------------------------------------------------- *
 * Links src/writer/ and testbit only - no reader code - which is itself  *
 * the proof that the writer stands alone. Exact-output assertions here;  *
 * test_writer_roundtrip.c proves the output reads back through the real  *
 * parser.                                                                *
 * ********************************************************************** */

#include "anvil_writer.h"
#include "anvil_writer_vtable.h"
#include "testbit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void th(void) {}

// Finishes `w`, compares its text to `expected`, disposes it.
static void expect_text(anvil_writer w, const char *expected, const char *msg) {
   TestBit.is_not_null(w, msg);
   if (!w) {
      return;
   }
   bool finished = anvil_writer_finish(w);
   TestBit.is_true(finished, msg);
   size_t len = 0;
   const char *data = anvil_writer_data(w, &len);
   TestBit.is_not_null(data, msg);
   if (data) {
      TestBit.is_equal_str(expected, data, msg);
      TestBit.is_equal_int((long long)strlen(expected), (long long)len, msg);
   }
   anvil_writer_dispose(w);
}

// Runs `fn` against a fresh writer and expects the sticky error `code`.
typedef bool (*step_fn)(anvil_writer);
static void expect_error(anvil_writer_dialect d, step_fn fn, anvil_writer_err_code code,
                         const char *msg) {
   anvil_writer w = anvil_writer_new(d);
   TestBit.is_not_null(w, msg);
   if (!w) {
      return;
   }
   bool ok = fn(w);
   TestBit.is_false(ok, msg);
   TestBit.is_equal_int(code, anvil_writer_get_error(w), msg);
   TestBit.is_null(anvil_writer_data(w, NULL), msg);
   anvil_writer_dispose(w);
}

/* ---------------------------------------------------------------------- *
 * W01 - lifecycle: an empty document is just the shebang; NULL is safe
 * ---------------------------------------------------------------------- */
static void test_w01_lifecycle(void) {
   expect_text(anvil_writer_new(ANVIL_WRITER_AML), "#!aml\n\n", "W01: empty AML document");
   expect_text(anvil_writer_new(ANVIL_WRITER_AMP), "#!amp\n\n", "W01: empty AMP document");

   anvil_writer_dispose(NULL);
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ARGUMENT, anvil_writer_get_error(NULL),
                        "W01: NULL handle reports INVALID_ARGUMENT");
   TestBit.is_false(anvil_writer_statement(NULL, "x", NULL), "W01: NULL handle call fails");
   TestBit.is_null(anvil_writer_data(NULL, NULL), "W01: NULL handle has no data");
   TestBit.is_null(anvil_writer_new((anvil_writer_dialect)99), "W01: unknown dialect rejected");
   TestBit.is_true(anvil_writer_error_message(ANVIL_WRITER_ERR_STATE) != NULL,
                   "W01: error message never NULL");
   TestBit.is_true(anvil_writer_error_message((anvil_writer_err_code)999) != NULL,
                   "W01: unknown code still has a message");
}

/* ---------------------------------------------------------------------- *
 * W02 - every scalar kind, canonical `name := value;` lines
 * ---------------------------------------------------------------------- */
static void test_w02_scalars(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "name", NULL);
   anvil_writer_bare(w, "David");
   anvil_writer_statement(w, "n", NULL);
   anvil_writer_numeric(w, "-1.5e+3");
   anvil_writer_statement(w, "yes", NULL);
   anvil_writer_bool(w, true);
   anvil_writer_statement(w, "no", NULL);
   anvil_writer_bool(w, false);
   anvil_writer_statement(w, "nothing", NULL);
   anvil_writer_null(w);
   anvil_writer_statement(w, "s", NULL);
   anvil_writer_string(w, "a \"q\" \\ b\n\t\r", 12);
   anvil_writer_statement(w, "empty", NULL);
   anvil_writer_string(w, "", 0);
   anvil_writer_statement(w, "tagged", NULL);
   anvil_writer_blob(w, "date", "2026-07-07", 10);
   anvil_writer_statement(w, "untagged", NULL);
   anvil_writer_blob(w, NULL, "raw", 3);
   anvil_writer_statement(w, "alias", NULL);
   anvil_writer_varref(w, "name");
   expect_text(w,
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
               "alias := $name;\n",
               "W02: all scalar kinds");
}

/* ---------------------------------------------------------------------- *
 * W03 - int and double helpers produce valid, shortest numeric text
 * ---------------------------------------------------------------------- *
 * A double's text must read back as the same double, with the grammar's
 * mandatory signed exponent.
 */
static void test_w03_int_and_double(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "a", NULL);
   anvil_writer_int(w, 0);
   anvil_writer_statement(w, "b", NULL);
   anvil_writer_int(w, INT64_MIN);
   anvil_writer_statement(w, "c", NULL);
   anvil_writer_int(w, INT64_MAX);
   anvil_writer_statement(w, "d", NULL);
   anvil_writer_double(w, 0.1);
   anvil_writer_statement(w, "e", NULL);
   anvil_writer_double(w, 1e20);
   anvil_writer_statement(w, "f", NULL);
   anvil_writer_double(w, -2.5);
   anvil_writer_statement(w, "g", NULL);
   anvil_writer_double(w, 100.0);
   expect_text(w,
               "#!aml\n\n"
               "a := 0;\n"
               "b := -9223372036854775808;\n"
               "c := 9223372036854775807;\n"
               "d := 0.1;\n"
               "e := 1e+20;\n"
               "f := -2.5;\n"
               "g := 100;\n",
               "W03: int/double text");

   w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "x", NULL);
   TestBit.is_false(anvil_writer_double(w, 1.0 / 0.0), "W03: infinity rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_NUMERIC, anvil_writer_get_error(w),
                        "W03: infinity is INVALID_NUMERIC");
   anvil_writer_dispose(w);
   w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "x", NULL);
   TestBit.is_false(anvil_writer_double(w, 0.0 / 0.0), "W03: NaN rejected");
   anvil_writer_dispose(w);
}

/* ---------------------------------------------------------------------- *
 * W04 - module header: attributes then includes, blank line before body
 * ---------------------------------------------------------------------- */
static void test_w04_header(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_attribute(w, "types", NULL);
   anvil_writer_attribute(w, "mixed", "true");
   anvil_writer_attribute_string(w, "note", "hello, world]", 13);
   anvil_writer_include(w, "base.anvl");
   anvil_writer_statement(w, "x", NULL);
   anvil_writer_int(w, 1);
   expect_text(w,
               "#!aml\n\n"
               "@[types]\n"
               "@[mixed=true]\n"
               "@[note=\"hello, world]\"]\n"
               "include \"base.anvl\";\n"
               "\n"
               "x := 1;\n",
               "W04: header attributes and include");
}

/* ---------------------------------------------------------------------- *
 * W05 - statement attributes group into one @[...], after `: base`
 * ---------------------------------------------------------------------- */
static void test_w05_statement_attributes_and_base(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "server", NULL);
   anvil_writer_attribute(w, "env", "production");
   anvil_writer_attribute(w, "active", NULL);
   anvil_writer_attribute_string(w, "label", "a b", 3);
   anvil_writer_begin_object(w);
   anvil_writer_statement(w, "host", NULL);
   anvil_writer_bare(w, "localhost");
   anvil_writer_end_object(w);
   anvil_writer_statement(w, "derived", "server");
   anvil_writer_attribute(w, "x", NULL);
   anvil_writer_begin_object(w);
   anvil_writer_statement(w, "port", NULL);
   anvil_writer_int(w, 8080);
   anvil_writer_end_object(w);
   expect_text(w,
               "#!aml\n\n"
               "server @[env=production, active, label=\"a b\"] := {\n"
               "   host := localhost;\n"
               "};\n"
               "derived : server @[x] := {\n"
               "   port := 8080;\n"
               "};\n",
               "W05: statement attributes and base");
}

/* ---------------------------------------------------------------------- *
 * W06 - collections: inline scalars, multi-line objects, nesting
 * ---------------------------------------------------------------------- */
static void test_w06_collections(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "tags", NULL);
   anvil_writer_begin_array(w);
   anvil_writer_bare(w, "alpha");
   anvil_writer_string(w, "be ta", 5);
   anvil_writer_int(w, 3);
   anvil_writer_end_array(w);
   anvil_writer_statement(w, "coords", NULL);
   anvil_writer_begin_tuple(w);
   anvil_writer_int(w, 10);
   anvil_writer_int(w, 20);
   anvil_writer_end_tuple(w);
   anvil_writer_statement(w, "grid", NULL);
   anvil_writer_begin_array(w);
   anvil_writer_begin_tuple(w);
   anvil_writer_int(w, 1);
   anvil_writer_int(w, 2);
   anvil_writer_end_tuple(w);
   anvil_writer_begin_array(w);
   anvil_writer_int(w, 3);
   anvil_writer_end_array(w);
   anvil_writer_end_array(w);
   anvil_writer_statement(w, "outer", NULL);
   anvil_writer_begin_object(w);
   anvil_writer_statement(w, "items", NULL);
   anvil_writer_begin_array(w);
   anvil_writer_begin_object(w);
   anvil_writer_statement(w, "id", NULL);
   anvil_writer_int(w, 1);
   anvil_writer_end_object(w);
   anvil_writer_begin_object(w);
   anvil_writer_statement(w, "id", NULL);
   anvil_writer_int(w, 2);
   anvil_writer_statement(w, "tags", NULL);
   anvil_writer_begin_array(w);
   anvil_writer_bare(w, "x");
   anvil_writer_end_array(w);
   anvil_writer_end_object(w);
   anvil_writer_end_array(w);
   anvil_writer_end_object(w);
   expect_text(w,
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
               "W06: collections layout");
}

/* ---------------------------------------------------------------------- *
 * W07 - AMP: scalar statements and scalar-only collections are legal
 * ---------------------------------------------------------------------- */
static void test_w07_amp_legal(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AMP);
   anvil_writer_statement(w, "id", NULL);
   anvil_writer_int(w, 7);
   anvil_writer_statement(w, "tags", NULL);
   anvil_writer_begin_array(w);
   anvil_writer_bare(w, "a");
   anvil_writer_bare(w, "b");
   anvil_writer_end_array(w);
   anvil_writer_statement(w, "pos", NULL);
   anvil_writer_begin_tuple(w);
   anvil_writer_int(w, 1);
   anvil_writer_int(w, 2);
   anvil_writer_end_tuple(w);
   anvil_writer_statement(w, "data", NULL);
   anvil_writer_blob(w, "bin", "xyz", 3);
   expect_text(w,
               "#!amp\n\n"
               "id := 7;\n"
               "tags := [a, b];\n"
               "pos := (1, 2);\n"
               "data := @bin`xyz`;\n",
               "W07: legal AMP document");
}

/* ---------------------------------------------------------------------- *
 * W08 - AMP forbids what the AMP parser forbids
 * ---------------------------------------------------------------------- */
static bool amp_module_attr(anvil_writer w) { return anvil_writer_attribute(w, "a", NULL); }
static bool amp_include(anvil_writer w) { return anvil_writer_include(w, "x.anvl"); }
static bool amp_base(anvil_writer w) { return anvil_writer_statement(w, "a", "b"); }
static bool amp_stmt_attr(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_attribute(w, "k", NULL);
}
static bool amp_object(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_object(w);
}
static bool amp_varref(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_varref(w, "b");
}
static bool amp_nested_array(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_array(w) &&
          anvil_writer_begin_array(w);
}
static bool amp_object_element(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_tuple(w) &&
          anvil_writer_begin_object(w);
}

static void test_w08_amp_forbidden(void) {
   expect_error(ANVIL_WRITER_AMP, amp_module_attr, ANVIL_WRITER_ERR_DIALECT, "W08: module attribute");
   expect_error(ANVIL_WRITER_AMP, amp_include, ANVIL_WRITER_ERR_DIALECT, "W08: include");
   expect_error(ANVIL_WRITER_AMP, amp_base, ANVIL_WRITER_ERR_DIALECT, "W08: base");
   expect_error(ANVIL_WRITER_AMP, amp_stmt_attr, ANVIL_WRITER_ERR_DIALECT, "W08: statement attribute");
   expect_error(ANVIL_WRITER_AMP, amp_object, ANVIL_WRITER_ERR_DIALECT, "W08: object value");
   expect_error(ANVIL_WRITER_AMP, amp_varref, ANVIL_WRITER_ERR_DIALECT, "W08: varref");
   expect_error(ANVIL_WRITER_AMP, amp_nested_array, ANVIL_WRITER_ERR_DIALECT, "W08: nested array");
   expect_error(ANVIL_WRITER_AMP, amp_object_element, ANVIL_WRITER_ERR_DIALECT,
                "W08: object element");
}

/* ---------------------------------------------------------------------- *
 * W09 - identifiers: shape and reserved words
 * ---------------------------------------------------------------------- */
static bool id_empty(anvil_writer w) { return anvil_writer_statement(w, "", NULL); }
static bool id_null(anvil_writer w) { return anvil_writer_statement(w, NULL, NULL); }
static bool id_digit(anvil_writer w) { return anvil_writer_statement(w, "1a", NULL); }
static bool id_dash(anvil_writer w) { return anvil_writer_statement(w, "a-b", NULL); }
static bool id_base_bad(anvil_writer w) { return anvil_writer_statement(w, "a", "b c"); }
static bool id_kw_include(anvil_writer w) { return anvil_writer_statement(w, "include", NULL); }
static bool id_kw_import(anvil_writer w) { return anvil_writer_statement(w, "import", NULL); }
static bool id_kw_vars(anvil_writer w) { return anvil_writer_statement(w, "vars", NULL); }
static bool id_kw_true(anvil_writer w) { return anvil_writer_statement(w, "true", NULL); }
static bool id_kw_null(anvil_writer w) { return anvil_writer_statement(w, "null", NULL); }
static bool id_kw_base(anvil_writer w) { return anvil_writer_statement(w, "a", "false"); }
static bool id_attr_key(anvil_writer w) { return anvil_writer_attribute(w, "bad key", NULL); }
static bool id_varref(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_varref(w, "no.dots");
}

static void test_w09_identifiers(void) {
   expect_error(ANVIL_WRITER_AML, id_empty, ANVIL_WRITER_ERR_INVALID_IDENTIFIER, "W09: empty name");
   expect_error(ANVIL_WRITER_AML, id_null, ANVIL_WRITER_ERR_INVALID_ARGUMENT, "W09: NULL name");
   expect_error(ANVIL_WRITER_AML, id_digit, ANVIL_WRITER_ERR_INVALID_IDENTIFIER, "W09: leading digit");
   expect_error(ANVIL_WRITER_AML, id_dash, ANVIL_WRITER_ERR_INVALID_IDENTIFIER, "W09: dash");
   expect_error(ANVIL_WRITER_AML, id_base_bad, ANVIL_WRITER_ERR_INVALID_IDENTIFIER, "W09: bad base");
   expect_error(ANVIL_WRITER_AML, id_kw_include, ANVIL_WRITER_ERR_RESERVED_WORD, "W09: include");
   expect_error(ANVIL_WRITER_AML, id_kw_import, ANVIL_WRITER_ERR_RESERVED_WORD, "W09: import");
   expect_error(ANVIL_WRITER_AML, id_kw_vars, ANVIL_WRITER_ERR_RESERVED_WORD, "W09: vars");
   expect_error(ANVIL_WRITER_AML, id_kw_true, ANVIL_WRITER_ERR_RESERVED_WORD, "W09: true");
   expect_error(ANVIL_WRITER_AML, id_kw_null, ANVIL_WRITER_ERR_RESERVED_WORD, "W09: null");
   expect_error(ANVIL_WRITER_AML, id_kw_base, ANVIL_WRITER_ERR_RESERVED_WORD, "W09: reserved base");
   expect_error(ANVIL_WRITER_AML, id_attr_key, ANVIL_WRITER_ERR_INVALID_IDENTIFIER, "W09: attr key");
   expect_error(ANVIL_WRITER_AML, id_varref, ANVIL_WRITER_ERR_INVALID_IDENTIFIER, "W09: varref name");
}

/* ---------------------------------------------------------------------- *
 * W10 - numeric, bare, blob, attribute-value and include validation
 * ---------------------------------------------------------------------- */
#define VALUE_STEP(name, call)                                                                \
   static bool name(anvil_writer w) { return anvil_writer_statement(w, "a", NULL) && (call); }

VALUE_STEP(num_trailing_dot, anvil_writer_numeric(w, "1."))
VALUE_STEP(num_plus, anvil_writer_numeric(w, "+1"))
VALUE_STEP(num_unsigned_exp, anvil_writer_numeric(w, "1e5"))
VALUE_STEP(num_empty, anvil_writer_numeric(w, ""))
VALUE_STEP(num_hex, anvil_writer_numeric(w, "0x10"))
VALUE_STEP(bare_numeric, anvil_writer_bare(w, "123"))
VALUE_STEP(bare_trailing_dot, anvil_writer_bare(w, "1."))
VALUE_STEP(bare_signed_exp, anvil_writer_bare(w, "1.e+5"))
VALUE_STEP(bare_neg_numeric, anvil_writer_bare(w, "-1.5"))
VALUE_STEP(bare_true, anvil_writer_bare(w, "true"))
VALUE_STEP(bare_include, anvil_writer_bare(w, "include"))
VALUE_STEP(bare_space, anvil_writer_bare(w, "a b"))
VALUE_STEP(bare_empty, anvil_writer_bare(w, ""))
VALUE_STEP(bare_comment, anvil_writer_bare(w, "//x"))
VALUE_STEP(bare_block_comment, anvil_writer_bare(w, "/*x"))
VALUE_STEP(bare_at, anvil_writer_bare(w, "a@b"))
VALUE_STEP(bare_leading_dollar, anvil_writer_bare(w, "$x"))
VALUE_STEP(blob_backtick, anvil_writer_blob(w, "t", "a`b", 3))
VALUE_STEP(blob_long_tag, anvil_writer_blob(w, "abcdefghijklmnopqrstuvwxyz0123456", "x", 1))
VALUE_STEP(blob_bad_tag, anvil_writer_blob(w, "1x", "x", 1))

static bool attr_comma(anvil_writer w) { return anvil_writer_attribute(w, "k", "a,b"); }
static bool attr_bracket(anvil_writer w) { return anvil_writer_attribute(w, "k", "a]"); }
static bool attr_varref(anvil_writer w) { return anvil_writer_attribute(w, "k", "$x"); }
static bool attr_empty(anvil_writer w) { return anvil_writer_attribute(w, "k", ""); }
static bool attr_space(anvil_writer w) { return anvil_writer_attribute(w, "k", " a"); }
static bool attr_open_quote(anvil_writer w) { return anvil_writer_attribute(w, "k", "\"a"); }
static bool attr_string_quote(anvil_writer w) {
   return anvil_writer_attribute_string(w, "k", "a\"b", 3);
}
static bool inc_empty(anvil_writer w) { return anvil_writer_include(w, ""); }
static bool inc_quote(anvil_writer w) { return anvil_writer_include(w, "a\"b"); }
static bool inc_newline(anvil_writer w) { return anvil_writer_include(w, "a\nb"); }

static void test_w10_value_validation(void) {
   expect_error(ANVIL_WRITER_AML, num_trailing_dot, ANVIL_WRITER_ERR_INVALID_NUMERIC, "W10: '1.'");
   expect_error(ANVIL_WRITER_AML, num_plus, ANVIL_WRITER_ERR_INVALID_NUMERIC, "W10: '+1'");
   expect_error(ANVIL_WRITER_AML, num_unsigned_exp, ANVIL_WRITER_ERR_INVALID_NUMERIC,
                "W10: unsigned exponent");
   expect_error(ANVIL_WRITER_AML, num_empty, ANVIL_WRITER_ERR_INVALID_NUMERIC, "W10: empty numeric");
   expect_error(ANVIL_WRITER_AML, num_hex, ANVIL_WRITER_ERR_INVALID_NUMERIC, "W10: hex");
   expect_error(ANVIL_WRITER_AML, bare_numeric, ANVIL_WRITER_ERR_INVALID_BARE, "W10: bare '123'");
   expect_error(ANVIL_WRITER_AML, bare_trailing_dot, ANVIL_WRITER_ERR_INVALID_BARE,
                "W10: bare '1.' (the reader reads it as numeric)");
   expect_error(ANVIL_WRITER_AML, bare_signed_exp, ANVIL_WRITER_ERR_INVALID_BARE,
                "W10: bare '1.e+5' (the reader reads it as numeric)");
   expect_error(ANVIL_WRITER_AML, bare_neg_numeric, ANVIL_WRITER_ERR_INVALID_BARE, "W10: bare '-1.5'");
   expect_error(ANVIL_WRITER_AML, bare_true, ANVIL_WRITER_ERR_RESERVED_WORD, "W10: bare 'true'");
   expect_error(ANVIL_WRITER_AML, bare_include, ANVIL_WRITER_ERR_RESERVED_WORD, "W10: bare 'include'");
   expect_error(ANVIL_WRITER_AML, bare_space, ANVIL_WRITER_ERR_INVALID_BARE, "W10: bare with space");
   expect_error(ANVIL_WRITER_AML, bare_empty, ANVIL_WRITER_ERR_INVALID_BARE, "W10: empty bare");
   expect_error(ANVIL_WRITER_AML, bare_comment, ANVIL_WRITER_ERR_INVALID_BARE, "W10: bare '//x'");
   expect_error(ANVIL_WRITER_AML, bare_block_comment, ANVIL_WRITER_ERR_INVALID_BARE,
                "W10: bare slash-star");
   expect_error(ANVIL_WRITER_AML, bare_at, ANVIL_WRITER_ERR_INVALID_BARE, "W10: bare with '@'");
   expect_error(ANVIL_WRITER_AML, bare_leading_dollar, ANVIL_WRITER_ERR_INVALID_BARE,
                "W10: bare leading '$'");
   expect_error(ANVIL_WRITER_AML, blob_backtick, ANVIL_WRITER_ERR_INVALID_BLOB, "W10: blob backtick");
   expect_error(ANVIL_WRITER_AML, blob_long_tag, ANVIL_WRITER_ERR_INVALID_BLOB, "W10: 33-char tag");
   expect_error(ANVIL_WRITER_AML, blob_bad_tag, ANVIL_WRITER_ERR_INVALID_BLOB, "W10: bad blob tag");

   expect_error(ANVIL_WRITER_AML, attr_comma, ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE, "W10: attr ','");
   expect_error(ANVIL_WRITER_AML, attr_bracket, ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE, "W10: attr ']'");
   expect_error(ANVIL_WRITER_AML, attr_varref, ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE, "W10: attr '$'");
   expect_error(ANVIL_WRITER_AML, attr_empty, ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE, "W10: attr empty");
   expect_error(ANVIL_WRITER_AML, attr_space, ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE,
                "W10: attr leading space");
   expect_error(ANVIL_WRITER_AML, attr_open_quote, ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE,
                "W10: attr unterminated quote");
   expect_error(ANVIL_WRITER_AML, attr_string_quote, ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE,
                "W10: attribute_string with '\"'");
   expect_error(ANVIL_WRITER_AML, inc_empty, ANVIL_WRITER_ERR_INVALID_INCLUDE_PATH, "W10: empty include");
   expect_error(ANVIL_WRITER_AML, inc_quote, ANVIL_WRITER_ERR_INVALID_INCLUDE_PATH, "W10: include quote");
   expect_error(ANVIL_WRITER_AML, inc_newline, ANVIL_WRITER_ERR_INVALID_INCLUDE_PATH,
                "W10: include newline");
}

/* ---------------------------------------------------------------------- *
 * W11 - accepted edge cases: tokens that look risky but round-trip
 * ---------------------------------------------------------------------- */
static void test_w11_accepted_edges(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   const char *bares[] = {"1e5", "007x", "a-b", "/usr/bin", "a:b", "x$y", ".hidden", "_x", "Truthy",
                          "1.5.2", "09-02-2026"};
   for (size_t i = 0; i < sizeof bares / sizeof *bares; i++) {
      char name[16];
      snprintf(name, sizeof name, "b%zu", i);
      anvil_writer_statement(w, name, NULL);
      TestBit.is_true(anvil_writer_bare(w, bares[i]), bares[i]);
   }
   TestBit.is_equal_int(ANVIL_WRITER_OK, anvil_writer_get_error(w), "W11: no error after edge bares");
   anvil_writer_dispose(w);

   w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "n", NULL);
   TestBit.is_true(anvil_writer_numeric(w, "007"), "W11: leading zeros are numeric");
   anvil_writer_statement(w, "m", NULL);
   TestBit.is_true(anvil_writer_numeric(w, "18446744073709551615"), "W11: beyond int64 is numeric");
   anvil_writer_dispose(w);
}

/* ---------------------------------------------------------------------- *
 * W12 - collection size rules
 * ---------------------------------------------------------------------- */
static bool empty_array(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_array(w) &&
          anvil_writer_end_array(w);
}
static bool empty_tuple(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_tuple(w) &&
          anvil_writer_end_tuple(w);
}
static bool single_tuple(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_tuple(w) &&
          anvil_writer_int(w, 1) && anvil_writer_end_tuple(w);
}
static bool empty_object(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_object(w) &&
          anvil_writer_end_object(w);
}
static bool base_scalar(anvil_writer w) {
   return anvil_writer_statement(w, "a", "b") && anvil_writer_int(w, 1);
}
static bool base_array(anvil_writer w) {
   return anvil_writer_statement(w, "a", "b") && anvil_writer_begin_array(w);
}

static void test_w12_collection_rules(void) {
   expect_error(ANVIL_WRITER_AML, empty_array, ANVIL_WRITER_ERR_EMPTY_COLLECTION, "W12: empty array");
   expect_error(ANVIL_WRITER_AML, empty_tuple, ANVIL_WRITER_ERR_EMPTY_COLLECTION, "W12: empty tuple");
   expect_error(ANVIL_WRITER_AML, single_tuple, ANVIL_WRITER_ERR_EMPTY_COLLECTION, "W12: 1-tuple");
   expect_error(ANVIL_WRITER_AML, empty_object, ANVIL_WRITER_ERR_EMPTY_COLLECTION, "W12: empty object");
   expect_error(ANVIL_WRITER_AML, base_scalar, ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT,
                "W12: base with scalar value");
   expect_error(ANVIL_WRITER_AML, base_array, ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT,
                "W12: base with array value");
}

/* ---------------------------------------------------------------------- *
 * W13 - call-order (state) errors
 * ---------------------------------------------------------------------- */
static bool value_without_statement(anvil_writer w) { return anvil_writer_int(w, 1); }
static bool statement_in_array(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_array(w) &&
          anvil_writer_statement(w, "b", NULL);
}
static bool statement_twice(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_statement(w, "b", NULL);
}
static bool value_in_object(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_object(w) &&
          anvil_writer_int(w, 1);
}
static bool attr_after_value(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_int(w, 1) &&
          anvil_writer_attribute(w, "k", NULL);
}
static bool module_attr_after_statement(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_int(w, 1) &&
          anvil_writer_include(w, "x.anvl");
}
static bool attr_in_array(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_array(w) &&
          anvil_writer_attribute(w, "k", NULL);
}
static bool end_without_begin(anvil_writer w) { return anvil_writer_end_array(w); }
static bool mismatched_end(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_array(w) &&
          anvil_writer_int(w, 1) && anvil_writer_end_tuple(w);
}
static bool finish_open_statement(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_finish(w);
}
static bool finish_open_array(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_array(w) &&
          anvil_writer_int(w, 1) && anvil_writer_finish(w);
}
static bool write_after_finish(anvil_writer w) {
   return anvil_writer_finish(w) && anvil_writer_statement(w, "a", NULL);
}
static bool dangling_base_end(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_object(w) &&
          anvil_writer_statement(w, "b", NULL) && anvil_writer_end_object(w);
}

static void test_w13_state_errors(void) {
   expect_error(ANVIL_WRITER_AML, value_without_statement, ANVIL_WRITER_ERR_STATE,
                "W13: value with no statement");
   expect_error(ANVIL_WRITER_AML, statement_in_array, ANVIL_WRITER_ERR_STATE,
                "W13: statement inside array");
   expect_error(ANVIL_WRITER_AML, statement_twice, ANVIL_WRITER_ERR_STATE,
                "W13: statement before previous completed");
   expect_error(ANVIL_WRITER_AML, value_in_object, ANVIL_WRITER_ERR_STATE,
                "W13: bare value inside object");
   expect_error(ANVIL_WRITER_AML, attr_after_value, ANVIL_WRITER_ERR_STATE,
                "W13: attribute after value");
   expect_error(ANVIL_WRITER_AML, module_attr_after_statement, ANVIL_WRITER_ERR_STATE,
                "W13: include after the first statement");
   expect_error(ANVIL_WRITER_AML, attr_in_array, ANVIL_WRITER_ERR_STATE, "W13: attribute in array");
   expect_error(ANVIL_WRITER_AML, end_without_begin, ANVIL_WRITER_ERR_STATE, "W13: end with no begin");
   expect_error(ANVIL_WRITER_AML, mismatched_end, ANVIL_WRITER_ERR_STATE, "W13: mismatched end");
   expect_error(ANVIL_WRITER_AML, finish_open_statement, ANVIL_WRITER_ERR_UNFINISHED,
                "W13: finish with open statement");
   expect_error(ANVIL_WRITER_AML, finish_open_array, ANVIL_WRITER_ERR_UNFINISHED,
                "W13: finish with open array");
   expect_error(ANVIL_WRITER_AML, write_after_finish, ANVIL_WRITER_ERR_STATE,
                "W13: write after finish");
   expect_error(ANVIL_WRITER_AML, dangling_base_end, ANVIL_WRITER_ERR_STATE,
                "W13: end_object with a statement still open");
}

/* ---------------------------------------------------------------------- *
 * W14 - the first error is sticky and data stays unavailable
 * ---------------------------------------------------------------------- */
static void test_w14_sticky_error(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "bad name", NULL);
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_IDENTIFIER, anvil_writer_get_error(w),
                        "W14: first error recorded");
   TestBit.is_false(anvil_writer_statement(w, "ok", NULL), "W14: later valid call still fails");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_IDENTIFIER, anvil_writer_get_error(w),
                        "W14: error not overwritten");
   TestBit.is_false(anvil_writer_finish(w), "W14: finish fails");
   TestBit.is_null(anvil_writer_data(w, NULL), "W14: no data after error");
   anvil_writer_dispose(w);
}

/* ---------------------------------------------------------------------- *
 * W15 - data: only after finish; length out-param optional; large output
 * ---------------------------------------------------------------------- */
static void test_w15_data_and_growth(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "x", NULL);
   anvil_writer_int(w, 1);
   TestBit.is_null(anvil_writer_data(w, NULL), "W15: no data before finish");
   TestBit.is_true(anvil_writer_finish(w), "W15: finish");
   TestBit.is_true(anvil_writer_finish(w), "W15: finish is idempotent");
   TestBit.is_not_null(anvil_writer_data(w, NULL), "W15: length out-param is optional");
   anvil_writer_dispose(w);

   // Far past any initial buffer size: forces repeated growth.
   w = anvil_writer_new(ANVIL_WRITER_AML);
   size_t payload_len = 1000000;
   char *payload = malloc(payload_len);
   memset(payload, 'A', payload_len);
   anvil_writer_statement(w, "big", NULL);
   anvil_writer_string(w, payload, payload_len);
   TestBit.is_true(anvil_writer_finish(w), "W15: large document finishes");
   size_t len = 0;
   const char *data = anvil_writer_data(w, &len);
   TestBit.is_equal_int((long long)(strlen("#!aml\n\nbig := \"") + payload_len + strlen("\";\n")),
                        (long long)len, "W15: large document length");
   TestBit.is_true(data && data[len] == '\0', "W15: NUL-terminated past the reported length");
   free(payload);
   anvil_writer_dispose(w);
}

/* ---------------------------------------------------------------------- *
 * W16 - vtable fields are pointer-identical to the flat functions
 * ---------------------------------------------------------------------- */
static void test_w16_vtable_matches_flat(void) {
   TestBit.is_true(Writer.create == anvil_writer_new, "W16: create");
   TestBit.is_true(Writer.dispose == anvil_writer_dispose, "W16: dispose");
   TestBit.is_true(Writer.get_error == anvil_writer_get_error, "W16: get_error");
   TestBit.is_true(Writer.error_message == anvil_writer_error_message, "W16: error_message");
   TestBit.is_true(Writer.attribute == anvil_writer_attribute, "W16: attribute");
   TestBit.is_true(Writer.attribute_string == anvil_writer_attribute_string, "W16: attribute_string");
   TestBit.is_true(Writer.include == anvil_writer_include, "W16: include");
   TestBit.is_true(Writer.statement == anvil_writer_statement, "W16: statement");
   TestBit.is_true(Writer.null_value == anvil_writer_null, "W16: null_value");
   TestBit.is_true(Writer.bool_value == anvil_writer_bool, "W16: bool_value");
   TestBit.is_true(Writer.numeric == anvil_writer_numeric, "W16: numeric");
   TestBit.is_true(Writer.int_value == anvil_writer_int, "W16: int_value");
   TestBit.is_true(Writer.double_value == anvil_writer_double, "W16: double_value");
   TestBit.is_true(Writer.string == anvil_writer_string, "W16: string");
   TestBit.is_true(Writer.bare == anvil_writer_bare, "W16: bare");
   TestBit.is_true(Writer.blob == anvil_writer_blob, "W16: blob");
   TestBit.is_true(Writer.varref == anvil_writer_varref, "W16: varref");
   TestBit.is_true(Writer.begin_array == anvil_writer_begin_array, "W16: begin_array");
   TestBit.is_true(Writer.end_array == anvil_writer_end_array, "W16: end_array");
   TestBit.is_true(Writer.begin_tuple == anvil_writer_begin_tuple, "W16: begin_tuple");
   TestBit.is_true(Writer.end_tuple == anvil_writer_end_tuple, "W16: end_tuple");
   TestBit.is_true(Writer.begin_object == anvil_writer_begin_object, "W16: begin_object");
   TestBit.is_true(Writer.end_object == anvil_writer_end_object, "W16: end_object");
   TestBit.is_true(Writer.finish == anvil_writer_finish, "W16: finish");
   TestBit.is_true(Writer.data == anvil_writer_data, "W16: data");
}

/* ---------------------------------------------------------------------- *
 * W17 - a top-level name can't be declared twice (the reader's resolver
 * rejects it); nested objects may repeat a name, and a nested name may
 * match a top-level one
 * ---------------------------------------------------------------------- */
static bool dup_top_level(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_int(w, 1) &&
          anvil_writer_statement(w, "a", NULL);
}
static bool dup_top_level_after_object(anvil_writer w) {
   return anvil_writer_statement(w, "a", NULL) && anvil_writer_begin_object(w) &&
          anvil_writer_statement(w, "x", NULL) && anvil_writer_int(w, 1) &&
          anvil_writer_end_object(w) && anvil_writer_statement(w, "a", NULL);
}

static void test_w17_duplicate_names(void) {
   expect_error(ANVIL_WRITER_AML, dup_top_level, ANVIL_WRITER_ERR_DUPLICATE_NAME,
                "W17: duplicate top-level name");
   expect_error(ANVIL_WRITER_AML, dup_top_level_after_object, ANVIL_WRITER_ERR_DUPLICATE_NAME,
                "W17: duplicate top-level name after an object");
   expect_error(ANVIL_WRITER_AMP, dup_top_level, ANVIL_WRITER_ERR_DUPLICATE_NAME,
                "W17: duplicate top-level name in AMP");

   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "a", NULL);
   anvil_writer_begin_object(w);
   anvil_writer_statement(w, "a", NULL);
   anvil_writer_int(w, 1);
   anvil_writer_statement(w, "a", NULL);
   anvil_writer_int(w, 2);
   anvil_writer_end_object(w);
   anvil_writer_statement(w, "b", NULL);
   anvil_writer_begin_array(w);
   anvil_writer_begin_object(w);
   anvil_writer_statement(w, "a", NULL);
   anvil_writer_int(w, 1);
   anvil_writer_end_object(w);
   anvil_writer_end_array(w);
   expect_text(w,
               "#!aml\n\n"
               "a := {\n"
               "   a := 1;\n"
               "   a := 2;\n"
               "};\n"
               "b := [\n"
               "   {\n"
               "      a := 1;\n"
               "   }\n"
               "];\n",
               "W17: nested repeats are legal");

   // enough distinct names to force the name set to grow, then one repeat
   w = anvil_writer_new(ANVIL_WRITER_AML);
   char name[16];
   for (int i = 0; i < 5000; i++) {
      snprintf(name, sizeof name, "n%d", i);
      anvil_writer_statement(w, name, NULL);
      anvil_writer_int(w, i);
   }
   TestBit.is_equal_int(ANVIL_WRITER_OK, anvil_writer_get_error(w), "W17: 5000 distinct names");
   TestBit.is_false(anvil_writer_statement(w, "n4321", NULL), "W17: repeat after growth rejected");
   TestBit.is_equal_int(ANVIL_WRITER_ERR_DUPLICATE_NAME, anvil_writer_get_error(w),
                        "W17: repeat after growth is DUPLICATE_NAME");
   anvil_writer_dispose(w);
}

int main(void) {
   TestBit.run_ex("W01_lifecycle", NULL, test_w01_lifecycle, th);
   TestBit.run_ex("W02_scalars", NULL, test_w02_scalars, th);
   TestBit.run_ex("W03_int_and_double", NULL, test_w03_int_and_double, th);
   TestBit.run_ex("W04_header", NULL, test_w04_header, th);
   TestBit.run_ex("W05_statement_attributes_and_base", NULL,
                  test_w05_statement_attributes_and_base, th);
   TestBit.run_ex("W06_collections", NULL, test_w06_collections, th);
   TestBit.run_ex("W07_amp_legal", NULL, test_w07_amp_legal, th);
   TestBit.run_ex("W08_amp_forbidden", NULL, test_w08_amp_forbidden, th);
   TestBit.run_ex("W09_identifiers", NULL, test_w09_identifiers, th);
   TestBit.run_ex("W10_value_validation", NULL, test_w10_value_validation, th);
   TestBit.run_ex("W11_accepted_edges", NULL, test_w11_accepted_edges, th);
   TestBit.run_ex("W12_collection_rules", NULL, test_w12_collection_rules, th);
   TestBit.run_ex("W13_state_errors", NULL, test_w13_state_errors, th);
   TestBit.run_ex("W14_sticky_error", NULL, test_w14_sticky_error, th);
   TestBit.run_ex("W15_data_and_growth", NULL, test_w15_data_and_growth, th);
   TestBit.run_ex("W16_vtable_matches_flat", NULL, test_w16_vtable_matches_flat, th);
   TestBit.run_ex("W17_duplicate_names", NULL, test_w17_duplicate_names, th);
   return TestBit.report();
}
