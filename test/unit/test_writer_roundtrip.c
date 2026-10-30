/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_writer_roundtrip.c - The writer's output through the real parser  *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_writer_roundtrip.c                                *
 * ---------------------------------------------------------------------- *
 * The invariant the writer exists to hold: whatever it emits, the reader *
 * parses back to the same kinds and text. This is the drift gate between *
 * the writer's own grammar knowledge (src/writer/) and the parser's.     *
 * Links reader + writer.                                                 *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_types.h"
#include "anvil_builder.h"
#include "anvil_writer.h"
#include "internal/source_registry.h"
#include "testbit.h"
// ----------------
#include "../utilities/helpers.h"
#include <dirent.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
}

/* ---------------------------------------------------------------------- *
 * Helpers
 * ---------------------------------------------------------------------- */
// Finishes `w`, loads its text through the real parser and disposes `w`.
static anvil_document load_from(anvil_writer w, const char *msg) {
   bool finished = anvil_writer_finish(w);
   TestBit.is_true(finished, msg);
   size_t len = 0;
   const char *data = anvil_writer_data(w, &len);
   anvil_document doc = data ? anvil_load_buffer(data, len) : NULL;
   if (doc && anvil_has_errors(doc)) {
      char message[256] = {0};
      anvil_error_get_message(anvil_document_get_error(doc), message, sizeof message);
      fprintf(stderr, "[%s] reader rejected writer output: %s\n%s\n", msg, message, data);
   }
   anvil_writer_dispose(w);
   return doc;
}

static char *text_of(anvil_value v) {
   size_t need = anvil_value_get_text(v, NULL, 0);
   char *buf = malloc(need + 1);
   anvil_value_get_text(v, buf, need + 1);
   return buf;
}

static anvil_value value_of(anvil_document doc, const char *name) {
   anvil_statement s = anvil_document_find_statement(doc, name);
   return s ? anvil_statement_get_value(s) : NULL;
}

// Asserts `name`'s value has `kind` and (if `text` is non-NULL) exactly that text.
static void expect_value(anvil_document doc, const char *name, anvil_value_type kind,
                         const char *text, size_t text_len, const char *msg) {
   anvil_value v = value_of(doc, name);
   TestBit.is_not_null(v, msg);
   if (!v) {
      return;
   }
   TestBit.is_equal_int(kind, anvil_value_get_type(v), msg);
   if (text) {
      char *got = text_of(v);
      size_t got_len = anvil_value_get_text(v, NULL, 0);
      TestBit.is_equal_int((long long)text_len, (long long)got_len, msg);
      TestBit.is_true(memcmp(text, got, text_len) == 0, msg);
      free(got);
   }
}

/* ---------------------------------------------------------------------- *
 * RT01 - every scalar kind reads back as the same kind and text
 * ---------------------------------------------------------------------- */
static void test_rt01_scalars(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "word", NULL);
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
   anvil_writer_string(w, "hello world", 11);
   anvil_writer_statement(w, "empty", NULL);
   anvil_writer_string(w, "", 0);
   anvil_writer_statement(w, "tagged", NULL);
   anvil_writer_blob(w, "date", "2026-07-07", 10);
   anvil_writer_statement(w, "untagged", NULL);
   anvil_writer_blob(w, NULL, "raw\ncontent", 11);
   anvil_document doc = load_from(w, "RT01");
   TestBit.is_not_null(doc, "RT01: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "RT01: no errors");
   expect_value(doc, "word", ANVIL_VALUE_BARE, "David", 5, "RT01: bare");
   expect_value(doc, "n", ANVIL_VALUE_NUMERIC, "-1.5e+3", 7, "RT01: numeric");
   expect_value(doc, "yes", ANVIL_VALUE_BOOL, "true", 4, "RT01: true");
   expect_value(doc, "no", ANVIL_VALUE_BOOL, "false", 5, "RT01: false");
   expect_value(doc, "nothing", ANVIL_VALUE_NULL, NULL, 0, "RT01: null");
   expect_value(doc, "s", ANVIL_VALUE_STRING, "hello world", 11, "RT01: string");
   expect_value(doc, "empty", ANVIL_VALUE_STRING, "", 0, "RT01: empty string");
   expect_value(doc, "tagged", ANVIL_VALUE_BLOB, "2026-07-07", 10, "RT01: tagged blob");
   expect_value(doc, "untagged", ANVIL_VALUE_BLOB, "raw\ncontent", 11, "RT01: untagged blob");
   anvil_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * RT02 - string escaping decodes back to the exact original bytes
 * ---------------------------------------------------------------------- */
static void test_rt02_string_escapes(void) {
   const char original[] = "quote\" back\\slash nl\n tab\t cr\r end \\n literal \\q utf8: h\xc3\xa9llo";
   size_t len = sizeof original - 1;
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "s", NULL);
   anvil_writer_string(w, original, len);
   anvil_document doc = load_from(w, "RT02");
   TestBit.is_not_null(doc, "RT02: document loaded");
   if (!doc) {
      return;
   }
   expect_value(doc, "s", ANVIL_VALUE_STRING, original, len, "RT02: string bytes round-trip");
   anvil_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * RT03 - risky-looking bare literals and numerics keep their kind
 * ---------------------------------------------------------------------- */
static void test_rt03_edge_tokens(void) {
   const char *bares[] = {"1ex",     "1e3x", "007x", "a-b",        "/usr/bin", "a:b",  "x$y",
                          ".hidden", "_x",   "Truthy",     "1.5.2",    "09-02-2026", "//unc"};
   const size_t bare_count = sizeof bares / sizeof *bares - 1; // "//unc" is rejected by the writer
   const char *numerics[] = {"0", "-0", "007", "3.14", "-1285", "1.7976931348623157e+308", "5e-07", "1e5", "1E5", "1e308",
                             "18446744073709551615"};
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   char name[16];
   for (size_t i = 0; i < bare_count; i++) {
      snprintf(name, sizeof name, "b%zu", i);
      anvil_writer_statement(w, name, NULL);
      TestBit.is_true(anvil_writer_bare(w, bares[i]), bares[i]);
   }
   for (size_t i = 0; i < sizeof numerics / sizeof *numerics; i++) {
      snprintf(name, sizeof name, "n%zu", i);
      anvil_writer_statement(w, name, NULL);
      TestBit.is_true(anvil_writer_numeric(w, numerics[i]), numerics[i]);
   }
   anvil_document doc = load_from(w, "RT03");
   TestBit.is_not_null(doc, "RT03: document loaded");
   if (!doc) {
      return;
   }
   for (size_t i = 0; i < bare_count; i++) {
      snprintf(name, sizeof name, "b%zu", i);
      expect_value(doc, name, ANVIL_VALUE_BARE, bares[i], strlen(bares[i]), bares[i]);
   }
   for (size_t i = 0; i < sizeof numerics / sizeof *numerics; i++) {
      snprintf(name, sizeof name, "n%zu", i);
      expect_value(doc, name, ANVIL_VALUE_NUMERIC, numerics[i], strlen(numerics[i]), numerics[i]);
   }
   anvil_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * RT04 - int/double helpers read back as the same number
 * ---------------------------------------------------------------------- */
static void test_rt04_int_and_double(void) {
   const double doubles[] = {0.1,  -0.0,        1e20,   1e-7, 123456789.123456789,
                             5e-324, 1.7976931348623157e308, -2.5, 100.0, 1.0 / 3.0};
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "min", NULL);
   anvil_writer_int(w, INT64_MIN);
   anvil_writer_statement(w, "max", NULL);
   anvil_writer_int(w, INT64_MAX);
   char name[16];
   for (size_t i = 0; i < sizeof doubles / sizeof *doubles; i++) {
      snprintf(name, sizeof name, "d%zu", i);
      anvil_writer_statement(w, name, NULL);
      TestBit.is_true(anvil_writer_double(w, doubles[i]), name);
   }
   anvil_document doc = load_from(w, "RT04");
   TestBit.is_not_null(doc, "RT04: document loaded");
   if (!doc) {
      return;
   }
   expect_value(doc, "min", ANVIL_VALUE_NUMERIC, "-9223372036854775808", 20, "RT04: int64 min");
   expect_value(doc, "max", ANVIL_VALUE_NUMERIC, "9223372036854775807", 19, "RT04: int64 max");
   for (size_t i = 0; i < sizeof doubles / sizeof *doubles; i++) {
      snprintf(name, sizeof name, "d%zu", i);
      anvil_value v = value_of(doc, name);
      TestBit.is_not_null(v, name);
      if (!v) {
         continue;
      }
      TestBit.is_equal_int(ANVIL_VALUE_NUMERIC, anvil_value_get_type(v), name);
      char *text = text_of(v);
      TestBit.is_true(strtod(text, NULL) == doubles[i], name);
      free(text);
   }
   anvil_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * RT05 - arrays, tuples, nested collections, anonymous objects
 * ---------------------------------------------------------------------- */
static void test_rt05_collections(void) {
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
   anvil_document doc = load_from(w, "RT05");
   TestBit.is_not_null(doc, "RT05: document loaded");
   if (!doc) {
      return;
   }

   anvil_value tags = value_of(doc, "tags");
   TestBit.is_equal_int(ANVIL_VALUE_ARRAY, anvil_value_get_type(tags), "RT05: tags is an array");
   TestBit.is_equal_int(3, (long long)anvil_value_get_count(tags), "RT05: tags has 3 elements");
   TestBit.is_equal_int(ANVIL_VALUE_BARE, anvil_value_get_type(anvil_value_get_element(tags, 0)),
                        "RT05: tags[0] is BARE");
   TestBit.is_equal_int(ANVIL_VALUE_STRING, anvil_value_get_type(anvil_value_get_element(tags, 1)),
                        "RT05: tags[1] is STRING");
   TestBit.is_equal_int(ANVIL_VALUE_NUMERIC, anvil_value_get_type(anvil_value_get_element(tags, 2)),
                        "RT05: tags[2] is NUMERIC");

   anvil_value coords = value_of(doc, "coords");
   TestBit.is_equal_int(ANVIL_VALUE_TUPLE, anvil_value_get_type(coords), "RT05: coords is a tuple");
   TestBit.is_equal_int(2, (long long)anvil_value_get_count(coords), "RT05: coords has 2 elements");

   anvil_value grid = value_of(doc, "grid");
   TestBit.is_equal_int(2, (long long)anvil_value_get_count(grid), "RT05: grid has 2 elements");
   TestBit.is_equal_int(ANVIL_VALUE_TUPLE, anvil_value_get_type(anvil_value_get_element(grid, 0)),
                        "RT05: grid[0] is a tuple");
   TestBit.is_equal_int(ANVIL_VALUE_ARRAY, anvil_value_get_type(anvil_value_get_element(grid, 1)),
                        "RT05: grid[1] is an array");

   anvil_value outer = value_of(doc, "outer");
   TestBit.is_equal_int(ANVIL_VALUE_OBJECT, anvil_value_get_type(outer), "RT05: outer is an object");
   anvil_statement items_stmt = anvil_value_find_statement(outer, "items");
   TestBit.is_not_null(items_stmt, "RT05: outer.items found");
   if (items_stmt) {
      anvil_value items = anvil_statement_get_value(items_stmt);
      TestBit.is_equal_int(2, (long long)anvil_value_get_count(items), "RT05: items has 2 objects");
      anvil_value second = anvil_value_get_element(items, 1);
      TestBit.is_equal_int(ANVIL_VALUE_OBJECT, anvil_value_get_type(second),
                           "RT05: items[1] is an anonymous object");
      TestBit.is_equal_int(2, (long long)anvil_value_get_count(second),
                           "RT05: items[1] has 2 statements");
      anvil_statement id = anvil_value_find_statement(second, "id");
      TestBit.is_not_null(id, "RT05: items[1].id found");
      if (id) {
         char *text = text_of(anvil_statement_get_value(id));
         TestBit.is_equal_str("2", text, "RT05: items[1].id is 2");
         free(text);
      }
   }
   anvil_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * RT06 - module/statement attributes and inheritance
 * ---------------------------------------------------------------------- */
static void test_rt06_attributes_and_base(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_attribute(w, "doclevel", NULL);
   anvil_writer_attribute(w, "mode", "strict");
   anvil_writer_statement(w, "base", NULL);
   anvil_writer_begin_object(w);
   anvil_writer_statement(w, "x", NULL);
   anvil_writer_int(w, 1);
   anvil_writer_end_object(w);
   anvil_writer_statement(w, "derived", "base");
   anvil_writer_attribute(w, "env", "production");
   anvil_writer_attribute(w, "active", NULL);
   anvil_writer_attribute_string(w, "label", "a, b", 4);
   anvil_writer_begin_object(w);
   anvil_writer_statement(w, "y", NULL);
   anvil_writer_int(w, 2);
   anvil_writer_end_object(w);
   anvil_document doc = load_from(w, "RT06");
   TestBit.is_not_null(doc, "RT06: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "RT06: no errors");
   TestBit.is_equal_int(2, (long long)anvil_document_get_attribute_count(doc),
                        "RT06: two module attributes");
   TestBit.is_not_null(anvil_document_find_attribute(doc, "doclevel"), "RT06: flag module attribute");
   anvil_attribute mode = anvil_document_find_attribute(doc, "mode");
   TestBit.is_not_null(mode, "RT06: keyed module attribute");
   if (mode) {
      char buf[32] = {0};
      anvil_attribute_get_value(mode, buf, sizeof buf);
      TestBit.is_equal_str("strict", buf, "RT06: module attribute value");
   }

   anvil_statement derived = anvil_document_find_statement(doc, "derived");
   TestBit.is_not_null(derived, "RT06: derived found");
   if (derived) {
      TestBit.is_equal_int(3, (long long)anvil_statement_get_attribute_count(derived),
                           "RT06: three statement attributes");
      anvil_attribute env = anvil_statement_find_attribute(derived, "env");
      TestBit.is_not_null(env, "RT06: env attribute");
      if (env) {
         char buf[32] = {0};
         anvil_attribute_get_value(env, buf, sizeof buf);
         TestBit.is_equal_str("production", buf, "RT06: env value");
      }
      anvil_attribute label = anvil_statement_find_attribute(derived, "label");
      TestBit.is_not_null(label, "RT06: label attribute");
      if (label) {
         char buf[32] = {0};
         anvil_attribute_get_value(label, buf, sizeof buf);
         TestBit.is_equal_str("\"a, b\"", buf, "RT06: quoted attribute value keeps its quotes");
      }
      TestBit.is_equal_int(2, (long long)anvil_value_get_count(anvil_statement_get_value(derived)),
                           "RT06: derived inherits base's field alongside its own");
   }
   anvil_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * RT07 - a VarRef resolves transparently to its target
 * ---------------------------------------------------------------------- */
static void test_rt07_varref(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "name", NULL);
   anvil_writer_string(w, "David", 5);
   anvil_writer_statement(w, "alias", NULL);
   anvil_writer_varref(w, "name");
   anvil_writer_statement(w, "list", NULL);
   anvil_writer_begin_array(w);
   anvil_writer_varref(w, "name");
   anvil_writer_bare(w, "x");
   anvil_writer_end_array(w);
   anvil_document doc = load_from(w, "RT07");
   TestBit.is_not_null(doc, "RT07: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "RT07: no errors");
   expect_value(doc, "alias", ANVIL_VALUE_STRING, "David", 5, "RT07: alias resolves to name's value");
   anvil_value list = value_of(doc, "list");
   char *text = text_of(anvil_value_get_element(list, 0));
   TestBit.is_equal_str("David", text, "RT07: varref element resolves");
   free(text);
   anvil_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * RT08 - AMP documents parse as AMP
 * ---------------------------------------------------------------------- */
static void test_rt08_amp(void) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AMP);
   anvil_writer_statement(w, "id", NULL);
   anvil_writer_int(w, 7);
   anvil_writer_statement(w, "name", NULL);
   anvil_writer_string(w, "msg", 3);
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
   anvil_document doc = load_from(w, "RT08");
   TestBit.is_not_null(doc, "RT08: AMP document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "RT08: AMP parser accepts the writer's AMP output");
   expect_value(doc, "id", ANVIL_VALUE_NUMERIC, "7", 1, "RT08: id");
   expect_value(doc, "pos", ANVIL_VALUE_TUPLE, NULL, 0, "RT08: pos");
   expect_value(doc, "data", ANVIL_VALUE_BLOB, "xyz", 3, "RT08: data");
   anvil_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * RT09 - an emitted include resolves through the real include loader
 * ---------------------------------------------------------------------- */
static bool write_file(const char *path, anvil_writer w) {
   size_t len = 0;
   const char *data = anvil_writer_data(w, &len);
   FILE *f = data ? fopen(path, "wb") : NULL;
   if (!f) {
      return false;
   }
   fwrite(data, 1, len, f);
   fclose(f);
   return true;
}

static void test_rt09_include(void) {
   char dir[] = "/tmp/anvil_writer_rt_XXXXXX";
   TestBit.is_not_null(mkdtemp(dir), "RT09: temp dir created");
   char base_path[256], main_path[256];
   snprintf(base_path, sizeof base_path, "%s/base.anvl", dir);
   snprintf(main_path, sizeof main_path, "%s/main.anvl", dir);

   anvil_writer base = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(base, "shared", NULL);
   anvil_writer_string(base, "from base", 9);
   anvil_writer_finish(base);
   TestBit.is_true(write_file(base_path, base), "RT09: base written");
   anvil_writer_dispose(base);

   anvil_writer main_w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_include(main_w, "base.anvl");
   anvil_writer_statement(main_w, "own", NULL);
   anvil_writer_varref(main_w, "shared");
   anvil_writer_finish(main_w);
   TestBit.is_true(write_file(main_path, main_w), "RT09: main written");
   anvil_writer_dispose(main_w);

   anvil_document doc = anvil_load(main_path);
   TestBit.is_not_null(doc, "RT09: main loaded");
   if (doc) {
      TestBit.is_false(anvil_has_errors(doc), "RT09: no errors, include resolved");
      expect_value(doc, "own", ANVIL_VALUE_STRING, "from base", 9,
                   "RT09: own's varref resolves into the included document");
      anvil_dispose(doc);
   }
   remove(base_path);
   remove(main_path);
   rmdir(dir);
}

static void test_rt09b_include_and_module_attribute(void) {
   char dir[] = "/tmp/anvil_writer_rt_XXXXXX";
   TestBit.is_not_null(mkdtemp(dir), "RT09b: temp dir created");
   char base_path[256], main_path[256];
   snprintf(base_path, sizeof base_path, "%s/base.anvl", dir);
   snprintf(main_path, sizeof main_path, "%s/main.anvl", dir);
   anvil_writer base = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(base, "shared", NULL);
   anvil_writer_int(base, 1);
   anvil_writer_finish(base);
   TestBit.is_true(write_file(base_path, base), "RT09b: base written");
   anvil_writer_dispose(base);

   // a header with an include and a module attribute: the order the writer accepts loads
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   TestBit.is_true(anvil_writer_include(w, "base.anvl"), "RT09b: include");
   TestBit.is_true(anvil_writer_attribute(w, "doc", "x"), "RT09b: module attribute after the include");
   anvil_writer_statement(w, "own", NULL);
   anvil_writer_int(w, 2);
   anvil_writer_finish(w);
   TestBit.is_true(write_file(main_path, w), "RT09b: main written");
   anvil_writer_dispose(w);
   anvil_document doc = anvil_load(main_path);
   TestBit.is_not_null(doc, "RT09b: main loaded");
   if (doc) {
      TestBit.is_false(anvil_has_errors(doc), "RT09b: the reader accepts include then module attribute");
      anvil_dispose(doc);
   }

   // the other order is what the reader rejects (4201); the writer must not produce it
   w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_attribute(w, "doc", "x");
   bool included = anvil_writer_include(w, "base.anvl");
   anvil_writer_statement(w, "own", NULL);
   anvil_writer_int(w, 2);
   bool finished = anvil_writer_finish(w);
   TestBit.is_false(included && finished, "RT09b: the writer does not emit a module attribute before an include");
   anvil_writer_dispose(w);
   remove(base_path);
   remove(main_path);
   rmdir(dir);
}

/* ---------------------------------------------------------------------- *
 * RT10 - fixed point over every real fixture
 * ---------------------------------------------------------------------- *
 * Copies a loaded document back out through the writer using only the
 * public reader API, then repeats on the result: the second copy must be
 * byte-identical to the first (the writer's output is canonical), and
 * must parse without errors. An inheritance base isn't exposed by the
 * reader API, so an inherited object copies as its merged fields; blob
 * tags are (anvil_value_get_blob_tag) and survive the copy.
 */
static bool copy_value(anvil_writer w, anvil_value v);

static bool copy_statements(anvil_writer w, anvil_value obj) {
   size_t n = anvil_value_get_count(obj);
   for (size_t i = 0; i < n; i++) {
      anvil_statement s = anvil_value_get_statement(obj, i);
      char name[256];
      anvil_statement_get_name(s, name, sizeof name);
      if (!anvil_writer_statement(w, name, NULL)) {
         return false;
      }
      size_t attrs = anvil_statement_get_attribute_count(s);
      for (size_t a = 0; a < attrs; a++) {
         anvil_attribute attr = anvil_statement_get_attribute(s, a);
         char key[128], val[256];
         anvil_attribute_get_key(attr, key, sizeof key);
         size_t vlen = anvil_attribute_get_value(attr, val, sizeof val);
         if (!anvil_writer_attribute(w, key, vlen ? val : NULL)) {
            return false;
         }
      }
      if (!copy_value(w, anvil_statement_get_value(s))) {
         return false;
      }
   }
   return true;
}

static bool copy_value(anvil_writer w, anvil_value v) {
   anvil_value_type kind = anvil_value_get_type(v);
   size_t need = anvil_value_get_text(v, NULL, 0);
   char *text = text_of(v);
   bool ok = true;
   switch (kind) {
   case ANVIL_VALUE_NULL:
      ok = anvil_writer_null(w);
      break;
   case ANVIL_VALUE_BOOL:
      ok = anvil_writer_bool(w, strcmp(text, "true") == 0);
      break;
   case ANVIL_VALUE_NUMERIC:
      ok = anvil_writer_numeric(w, text);
      break;
   case ANVIL_VALUE_STRING:
      ok = anvil_writer_string(w, text, need);
      break;
   case ANVIL_VALUE_BARE:
      ok = anvil_writer_bare(w, text);
      break;
   case ANVIL_VALUE_BLOB: {
      char tag[64];
      size_t tag_len = anvil_value_get_blob_tag(v, tag, sizeof tag);
      ok = anvil_writer_blob(w, tag_len ? tag : NULL, text, need);
      break;
   }
   case ANVIL_VALUE_ARRAY:
   case ANVIL_VALUE_TUPLE: {
      bool is_array = kind == ANVIL_VALUE_ARRAY;
      ok = is_array ? anvil_writer_begin_array(w) : anvil_writer_begin_tuple(w);
      size_t n = anvil_value_get_count(v);
      for (size_t i = 0; ok && i < n; i++) {
         ok = copy_value(w, anvil_value_get_element(v, i));
      }
      ok = ok && (is_array ? anvil_writer_end_array(w) : anvil_writer_end_tuple(w));
      break;
   }
   case ANVIL_VALUE_OBJECT:
      ok = anvil_writer_begin_object(w) && copy_statements(w, v) && anvil_writer_end_object(w);
      break;
   }
   free(text);
   return ok;
}

// Copies `doc` into a finished writer, or returns NULL if the writer rejected something.
static anvil_writer copy_document(anvil_document doc, const char *label) {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   size_t attrs = anvil_document_get_attribute_count(doc);
   for (size_t a = 0; a < attrs; a++) {
      anvil_attribute attr = anvil_document_get_attribute(doc, a);
      char key[128], val[256];
      anvil_attribute_get_key(attr, key, sizeof key);
      size_t vlen = anvil_attribute_get_value(attr, val, sizeof val);
      anvil_writer_attribute(w, key, vlen ? val : NULL);
   }
   anvil_statement_iterator it = anvil_document_get_statements(doc);
   anvil_statement s = NULL;
   bool ok = true;
   while (ok && anvil_statement_iterator_next(it, &s)) {
      char name[256];
      anvil_statement_get_name(s, name, sizeof name);
      size_t sattrs = anvil_statement_get_attribute_count(s);
      ok = anvil_writer_statement(w, name, NULL);
      for (size_t a = 0; ok && a < sattrs; a++) {
         anvil_attribute attr = anvil_statement_get_attribute(s, a);
         char key[128], val[256];
         anvil_attribute_get_key(attr, key, sizeof key);
         size_t vlen = anvil_attribute_get_value(attr, val, sizeof val);
         ok = anvil_writer_attribute(w, key, vlen ? val : NULL);
      }
      ok = ok && copy_value(w, anvil_statement_get_value(s));
   }
   anvil_statement_iterator_dispose(it);
   if (!ok || !anvil_writer_finish(w)) {
      fprintf(stderr, "[%s] writer rejected a copied document: %s\n", label,
              anvil_writer_error_message(anvil_writer_get_error(w)));
      anvil_writer_dispose(w);
      return NULL;
   }
   return w;
}

static void test_rt10_fixture_fixed_point(void) {
   const char *dir_path = "../../test/fixtures";
   DIR *dir = opendir(dir_path);
   TestBit.is_not_null(dir, "RT10: fixtures directory opened");
   if (!dir) {
      return;
   }
   int copied = 0;
   struct dirent *entry;
   while ((entry = readdir(dir)) != NULL) {
      size_t n = strlen(entry->d_name);
      if (n < 6 || strcmp(entry->d_name + n - 5, ".anvl") != 0) {
         continue;
      }
      th();
      anvil_document original = anvil_load(fixture_path(entry->d_name));
      if (!original || anvil_has_errors(original)) {
         if (original) {
            anvil_dispose(original);
         }
         continue; // error fixtures and include fixtures that don't load standalone
      }
      anvil_writer first = copy_document(original, entry->d_name);
      TestBit.is_not_null(first, entry->d_name);
      // Disposed before the re-read: a document whose source text is byte-identical to one
      // still loaded is rejected by the source registry as a duplicate, and an already-canonical
      // fixture's copy is exactly that.
      anvil_dispose(original);
      if (first) {
         size_t len1 = 0;
         const char *data1 = anvil_writer_data(first, &len1);
         char *text1 = malloc(len1 + 1); // by length: the output can contain NUL bytes
         memcpy(text1, data1, len1);
         text1[len1] = '\0';
         anvil_document reread = anvil_load_buffer(text1, len1);
         TestBit.is_true(reread && !anvil_has_errors(reread), entry->d_name);
         if (reread && !anvil_has_errors(reread)) {
            anvil_writer second = copy_document(reread, entry->d_name);
            TestBit.is_not_null(second, entry->d_name);
            if (second) {
               size_t len2 = 0;
               const char *text2 = anvil_writer_data(second, &len2);
               bool same = len1 == len2 && memcmp(text1, text2, len1) == 0;
               if (!same) {
                  fprintf(stderr, "[%s] not a fixed point:\n--- first\n%s\n--- second\n%s\n",
                          entry->d_name, text1, text2);
               }
               TestBit.is_true(same, entry->d_name);
               anvil_writer_dispose(second);
            }
         }
         if (reread) {
            anvil_dispose(reread);
         }
         free(text1);
         anvil_writer_dispose(first);
         copied++;
      } else {
         fprintf(stderr, "[%s] copy failed\n", entry->d_name);
      }
   }
   closedir(dir);
   TestBit.is_true(copied >= 20, "RT10: at least 20 real fixtures round-tripped");
}

/* ---------------------------------------------------------------------- *
 * RT11 - whatever the writer accepts, the reader reads back identically
 * ---------------------------------------------------------------------- *
 * Deterministic pseudo-random candidates drawn from an alphabet full of
 * grammar-significant characters. The writer may reject a candidate (that
 * is its job); the only failure is an accepted candidate the reader
 * disagrees with.
 */
static unsigned rng_state = 12345u;
static unsigned rng(void) {
   rng_state = rng_state * 1664525u + 1013904223u;
   return rng_state >> 8;
}

static void random_text(char *out, size_t max_len, const char *alphabet) {
   size_t alpha_len = strlen(alphabet);
   size_t len = 1 + rng() % max_len;
   for (size_t i = 0; i < len; i++) {
      out[i] = alphabet[rng() % alpha_len];
   }
   out[len] = '\0';
}

static void test_rt11_accepted_candidates_read_back(void) {
   const char *bare_alpha = "ab1_-./:$e+.,;@`\"{}[]()#\\ x09";
   const char *num_alpha = "0123456789-+.eE";
   const char *string_alpha = "ab \"\\\n\t\r;:=,{}[]()@`$/*#'";
   const char *attr_alpha = "ab1=,]\" $-.[({`@";
   int accepted_bare = 0, accepted_numeric = 0, accepted_attr = 0;

   for (int round = 0; round < 3000; round++) {
      char bare[24], num[24], str[24], attr[24];
      random_text(bare, 12, bare_alpha);
      random_text(num, 12, num_alpha);
      random_text(str, 16, string_alpha);
      random_text(attr, 10, attr_alpha);

      anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
      bool bare_ok = false, num_ok = false, attr_ok = false;
      anvil_writer_statement(w, "s", NULL);
      anvil_writer_string(w, str, strlen(str));
      anvil_writer_statement(w, "b", NULL);
      bare_ok = anvil_writer_bare(w, bare);
      if (!bare_ok) {
         anvil_writer_dispose(w);
         w = anvil_writer_new(ANVIL_WRITER_AML);
         anvil_writer_statement(w, "s", NULL);
         anvil_writer_string(w, str, strlen(str));
         anvil_writer_statement(w, "b", NULL);
         anvil_writer_null(w);
      }
      anvil_writer_statement(w, "n", NULL);
      num_ok = anvil_writer_numeric(w, num);
      if (!num_ok) {
         anvil_writer_dispose(w);
         w = anvil_writer_new(ANVIL_WRITER_AML);
         anvil_writer_statement(w, "s", NULL);
         anvil_writer_string(w, str, strlen(str));
         anvil_writer_statement(w, "b", NULL);
         bare_ok ? anvil_writer_bare(w, bare) : anvil_writer_null(w);
         anvil_writer_statement(w, "n", NULL);
         anvil_writer_null(w);
      }
      anvil_writer_statement(w, "a", NULL);
      attr_ok = anvil_writer_attribute(w, "k", attr);
      if (!attr_ok) {
         // a failed call poisons the writer: rebuild without the attribute
         anvil_writer_dispose(w);
         w = anvil_writer_new(ANVIL_WRITER_AML);
         anvil_writer_statement(w, "s", NULL);
         anvil_writer_string(w, str, strlen(str));
         anvil_writer_statement(w, "b", NULL);
         bare_ok ? anvil_writer_bare(w, bare) : anvil_writer_null(w);
         anvil_writer_statement(w, "n", NULL);
         num_ok ? anvil_writer_numeric(w, num) : anvil_writer_null(w);
         anvil_writer_statement(w, "a", NULL);
      }
      anvil_writer_null(w);

      th();
      anvil_document doc = load_from(w, "RT11");
      TestBit.is_true(doc && !anvil_has_errors(doc), "RT11: reader accepts the writer's output");
      if (!doc) {
         return;
      }
      if (!anvil_has_errors(doc)) {
         expect_value(doc, "s", ANVIL_VALUE_STRING, str, strlen(str), str);
         if (bare_ok) {
            accepted_bare++;
            expect_value(doc, "b", ANVIL_VALUE_BARE, bare, strlen(bare), bare);
         }
         if (num_ok) {
            accepted_numeric++;
            expect_value(doc, "n", ANVIL_VALUE_NUMERIC, num, strlen(num), num);
         }
         if (attr_ok) {
            accepted_attr++;
            anvil_attribute got = anvil_statement_find_attribute(anvil_document_find_statement(doc, "a"), "k");
            TestBit.is_not_null(got, attr);
            if (got) {
               char buf[64] = {0};
               anvil_attribute_get_value(got, buf, sizeof buf);
               TestBit.is_equal_str(attr, buf, attr);
            }
         }
      }
      anvil_dispose(doc);
   }
   TestBit.is_true(accepted_bare > 50, "RT11: enough bare candidates were accepted to mean something");
   TestBit.is_true(accepted_numeric > 50, "RT11: enough numeric candidates were accepted");
   TestBit.is_true(accepted_attr > 50, "RT11: enough attribute candidates were accepted");
}

/* ---------------------------------------------------------------------- *
 * RT12 - the reader really does reject a duplicate top-level name (the
 * reason the writer refuses to emit one), and nested repeats too
 * ---------------------------------------------------------------------- */
static void test_rt12_duplicate_names_match_reader(void) {
   const char top_dup[] = "#!aml\n\na := 1;\na := 2;\n";
   anvil_document doc = anvil_load_buffer(top_dup, sizeof top_dup - 1);
   TestBit.is_true(doc && anvil_has_errors(doc), "RT12: reader rejects a duplicate top-level name");
   if (doc) {
      anvil_dispose(doc);
   }

   const char nested_dup[] = "#!aml\n\no := {\n   a := 1;\n   a := 2;\n};\n";
   doc = anvil_load_buffer(nested_dup, sizeof nested_dup - 1);
   // BR-2610-anvl-parser-005: a repeated object field is an error everywhere now
   TestBit.is_true(doc && anvil_has_errors(doc), "RT12: reader rejects nested repeated names");
   if (doc) {
      anvil_dispose(doc);
   }
}

/* ---------------------------------------------------------------------- *
 * RT14 (BR-2610-anvl-parser-005) - the property that ties writer, builder and
 * reader together: anything the writer or the builder accepts, the reader
 * loads. A generated sweep draws names and attribute keys from a tiny
 * alphabet so repeats are common; every repeat must be rejected by the writer
 * or the builder, and every document they do emit must load.
 * ---------------------------------------------------------------------- */
static uint64_t sweep_state;
static unsigned sweep_next(unsigned n) {
   sweep_state ^= sweep_state << 13;
   sweep_state ^= sweep_state >> 7;
   sweep_state ^= sweep_state << 17;
   return (unsigned)(sweep_state % n);
}
// two of the draws are reserved words, so keyword names are exercised as well as repeats
static const char *const SWEEP_NAMES[] = {"a", "b", "c", "d", "include", "true"};
static const char *const SWEEP_KEYS[] = {"k", "m", "n", "vars", "null"};

// Writes one statement (and, for an object, its fields) with random content. False once the
// writer has rejected something.
static bool sweep_writer_value(anvil_writer w, int depth);
static bool sweep_writer_statement(anvil_writer w, int depth) {
   if (!anvil_writer_statement(w, SWEEP_NAMES[sweep_next(6)], NULL)) {
      return false;
   }
   unsigned na = sweep_next(3);
   for (unsigned i = 0; i < na; i++) {
      const char *key = SWEEP_KEYS[sweep_next(5)];
      bool ok = sweep_next(2) ? anvil_writer_attribute(w, key, NULL)
                              : anvil_writer_attribute_string(w, key, "v", 1);
      if (!ok) {
         return false;
      }
   }
   return sweep_writer_value(w, depth);
}
static bool sweep_writer_value(anvil_writer w, int depth) {
   unsigned pick = sweep_next(depth >= 3 ? 2 : 5);
   if (pick == 0) {
      return anvil_writer_int(w, (int64_t)sweep_next(100));
   }
   if (pick == 1) {
      return anvil_writer_bare(w, "word");
   }
   if (pick == 2 || pick == 3) { // object
      if (!anvil_writer_begin_object(w)) {
         return false;
      }
      unsigned n = 1 + sweep_next(4);
      for (unsigned i = 0; i < n; i++) {
         if (!sweep_writer_statement(w, depth + 1)) {
            return false;
         }
      }
      return anvil_writer_end_object(w);
   }
   if (!anvil_writer_begin_array(w)) { // array of objects and scalars
      return false;
   }
   unsigned n = 1 + sweep_next(3);
   for (unsigned i = 0; i < n; i++) {
      if (!(sweep_next(2) ? anvil_writer_int(w, 1) : sweep_writer_value(w, depth + 1))) {
         return false;
      }
   }
   return anvil_writer_end_array(w);
}
static bool dup_code(anvil_writer_err_code c) {
   return c == ANVIL_WRITER_ERR_DUPLICATE_NAME || c == ANVIL_WRITER_ERR_DUPLICATE_FIELD ||
          c == ANVIL_WRITER_ERR_DUPLICATE_ATTRIBUTE_KEY || c == ANVIL_WRITER_ERR_RESERVED_WORD;
}

static void test_rt14_writer_sweep(void) {
   sweep_state = UINT64_C(0x9E3779B97F4A7C15);
   int accepted = 0, rejected = 0, not_loading = 0, wrong_reject = 0;
   for (int doc = 0; doc < 4000; doc++) {
      anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
      bool ok = true;
      unsigned nm = sweep_next(3);
      for (unsigned i = 0; ok && i < nm; i++) {
         ok = anvil_writer_attribute(w, SWEEP_KEYS[sweep_next(5)], NULL);
      }
      unsigned ns = 1 + sweep_next(4);
      for (unsigned i = 0; ok && i < ns; i++) {
         ok = sweep_writer_statement(w, 0);
      }
      ok = ok && anvil_writer_finish(w);
      if (ok) {
         accepted++;
         size_t len = 0;
         const char *text = anvil_writer_data(w, &len);
         anvil_document d = anvil_load_buffer(text, len);
         if (!d || anvil_has_errors(d)) {
            not_loading++;
         }
         anvil_dispose(d);
      } else {
         rejected++;
         if (!dup_code(anvil_writer_get_error(w))) {
            wrong_reject++;
         }
      }
      anvil_writer_dispose(w);
   }
   TestBit.is_true(accepted > 100, "RT14: the writer sweep accepted a good share of documents");
   TestBit.is_true(rejected > 300, "RT14: the writer sweep hit repeats");
   TestBit.is_equal_int(0, not_loading, "RT14: every document the writer accepted loads");
   TestBit.is_equal_int(0, wrong_reject, "RT14: the writer rejected only repeats and reserved words");
}

static anvil_node sweep_builder_value(anvil_builder b, int depth);
static void sweep_builder_fill(anvil_builder b, anvil_node object, int depth, unsigned n) {
   for (unsigned i = 0; i < n; i++) {
      anvil_node v = sweep_builder_value(b, depth + 1);
      anvil_member m = anvil_builder_add(b, object, SWEEP_NAMES[sweep_next(6)], v);
      if (!m) {
         continue; // a repeat: rejected, the document goes on without it
      }
      unsigned na = sweep_next(3);
      for (unsigned k = 0; k < na; k++) {
         const char *key = SWEEP_KEYS[sweep_next(5)];
         if (sweep_next(2)) {
            (void)anvil_builder_member_attribute(b, m, key, NULL);
         } else {
            (void)anvil_builder_member_attribute_string(b, m, key, "v", 1);
         }
      }
   }
}
static anvil_node sweep_builder_value(anvil_builder b, int depth) {
   unsigned pick = sweep_next(depth >= 3 ? 2 : 5);
   if (pick == 0) {
      return anvil_builder_int(b, (int64_t)sweep_next(100));
   }
   if (pick == 1) {
      return anvil_builder_bare(b, "word");
   }
   if (pick == 2 || pick == 3) {
      anvil_node o = anvil_builder_object(b);
      sweep_builder_fill(b, o, depth, 1 + sweep_next(4));
      return o;
   }
   anvil_node a = anvil_builder_array(b);
   unsigned n = 1 + sweep_next(3);
   for (unsigned i = 0; i < n; i++) {
      (void)anvil_builder_append(b, a, sweep_next(2) ? anvil_builder_int(b, 1) : sweep_builder_value(b, depth + 1));
   }
   return a;
}

static void test_rt14_builder_sweep(void) {
   sweep_state = UINT64_C(0xD1B54A32D192ED03);
   int emitted = 0, refused = 0, not_loading = 0;
   for (int doc = 0; doc < 4000; doc++) {
      anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
      unsigned nm = sweep_next(3);
      for (unsigned i = 0; i < nm; i++) {
         (void)anvil_builder_attribute(b, SWEEP_KEYS[sweep_next(5)], NULL);
      }
      sweep_builder_fill(b, NULL, 0, 1 + sweep_next(4));
      size_t len = 0;
      const char *text = anvil_builder_emit(b, &len);
      if (text) {
         emitted++;
         anvil_document d = anvil_load_buffer(text, len);
         if (!d || anvil_has_errors(d)) {
            not_loading++;
         }
         anvil_dispose(d);
      } else {
         refused++; // an object left empty because its only field was a repeat, say
      }
      anvil_builder_dispose(b);
   }
   TestBit.is_true(emitted > 300, "RT14: the builder sweep emitted a good share of documents");
   TestBit.is_equal_int(0, not_loading, "RT14: every document the builder emits loads");
   (void)refused;
}

// RT14, bases (BR-2610-anvl-writer-002): for every kind of value a top-level member can hold, a
// member that derives from it is emitted exactly when that value is an object, and what is
// emitted loads. The reader's 4020 is the reason for every refusal.
static void test_rt14_builder_bases(void) {
   for (int kind = 0; kind < 8; kind++) {
      anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
      anvil_node t;
      switch (kind) {
      case 0: t = anvil_builder_int(b, 1); break;
      case 1: t = anvil_builder_bare(b, "word"); break;
      case 2: t = anvil_builder_string(b, "s", 1); break;
      case 3: t = anvil_builder_null(b); break;
      case 4: t = anvil_builder_bool(b, true); break;
      case 5:
         t = anvil_builder_array(b);
         anvil_builder_append(b, t, anvil_builder_int(b, 1));
         break;
      case 6:
         t = anvil_builder_tuple(b);
         anvil_builder_append(b, t, anvil_builder_int(b, 1));
         anvil_builder_append(b, t, anvil_builder_int(b, 2));
         break;
      default:
         t = anvil_builder_object(b);
         anvil_builder_add(b, t, "f", anvil_builder_int(b, 1));
         break;
      }
      anvil_builder_add(b, NULL, "t", t);
      anvil_node d = anvil_builder_object(b);
      anvil_builder_add(b, d, "g", anvil_builder_int(b, 2));
      anvil_member dm = anvil_builder_add(b, NULL, "d", d);
      anvil_builder_set_base(b, dm, "t");
      size_t len = 0;
      const char *text = anvil_builder_emit(b, &len);
      TestBit.is_equal_int(kind == 7, text != NULL, "RT14: a base is emitted exactly when it is an object");
      if (text) {
         anvil_document doc = anvil_load_buffer(text, len);
         TestBit.is_true(doc && !anvil_has_errors(doc), "RT14: an object base loads");
         anvil_dispose(doc);
      } else {
         const char src[] = "#!aml\n\nt := 1;\nd : t := { g := 2; };\n";
         anvil_document doc = anvil_load_buffer(src, sizeof src - 1);
         TestBit.is_true(doc && anvil_has_errors(doc), "RT14: the reader refuses what the builder refused");
         anvil_dispose(doc);
      }
      anvil_builder_dispose(b);
   }
}

// The block form `ident { ... }` is an anonymous, read-only block: it cannot be a base (4052). The
// builder only ever writes `name := { ... }`, so it cannot name a block as a base at all; what it
// writes always loads.
static void test_rt14_anonymous_block_base(void) {
   const char src[] = "#!aml\n\nblk { f := 1; };\nd : blk := { g := 2; };\n";
   anvil_document doc = anvil_load_buffer(src, sizeof src - 1);
   TestBit.is_true(doc && anvil_has_errors(doc), "RT14: the reader refuses a block as a base");
   anvil_dispose(doc);

   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_node o = anvil_builder_object(b);
   anvil_builder_add(b, o, "f", anvil_builder_int(b, 1));
   anvil_builder_add(b, NULL, "blk", o);
   size_t len = 0;
   const char *text = anvil_builder_emit(b, &len);
   TestBit.is_true(text && !strstr(text, "blk {"), "RT14: the builder writes an object as 'name := {', never as a block");
   anvil_builder_dispose(b);
}

// RT14, attribute values: whatever the builder takes as an attribute value loads.
static void test_rt14_builder_attribute_values(void) {
   sweep_state = UINT64_C(0xA24BAED4963EE407);
   const char *alpha = "ab1=,]\" $-.[({)}`@";
   int accepted = 0, refused = 0, not_loading = 0;
   for (int round = 0; round < 3000; round++) {
      char v[12];
      for (int i = 0; i < 6; i++) {
         v[i] = alpha[sweep_next((unsigned)strlen(alpha))];
      }
      v[1 + sweep_next(5)] = '\0';
      anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
      anvil_member m = anvil_builder_add(b, NULL, "a", anvil_builder_int(b, 1));
      bool member = sweep_next(2);
      bool ok = member ? anvil_builder_member_attribute(b, m, "k", v) : anvil_builder_attribute(b, "k", v);
      if (!ok) {
         refused++;
         TestBit.is_equal_int(ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE, anvil_builder_get_error(b),
                              "RT14: an attribute value is refused as INVALID_ATTRIBUTE_VALUE");
         anvil_builder_dispose(b);
         continue;
      }
      accepted++;
      size_t len = 0;
      const char *text = anvil_builder_emit(b, &len);
      anvil_document doc = text ? anvil_load_buffer(text, len) : NULL;
      if (!doc || anvil_has_errors(doc)) {
         not_loading++;
      }
      anvil_dispose(doc);
      anvil_builder_dispose(b);
   }
   TestBit.is_true(accepted > 100 && refused > 100, "RT14: the attribute sweep hits both outcomes");
   TestBit.is_equal_int(0, not_loading, "RT14: every attribute value the builder accepts loads");
}

/* ---------------------------------------------------------------------- *
 * RT13 - a blob's tag survives write -> read -> write
 * ---------------------------------------------------------------------- */
static void test_rt13_blob_tags_round_trip(void) {
   anvil_document original = anvil_load(fixture_path("f04_blobs.anvl"));
   TestBit.is_not_null(original, "RT13: f04_blobs.anvl loaded");
   if (!original) {
      return;
   }
   anvil_writer copy = copy_document(original, "f04_blobs.anvl");
   anvil_dispose(original);
   TestBit.is_not_null(copy, "RT13: copied");
   if (!copy) {
      return;
   }
   size_t len = 0;
   const char *text = anvil_writer_data(copy, &len);
   TestBit.is_true(text && strstr(text, "created := @date`2026-07-07`;") != NULL,
                   "RT13: @date tag survives the copy");
   TestBit.is_true(text && strstr(text, "dom_elem := @sel`#input-date`;") != NULL,
                   "RT13: @sel tag survives the copy");
   TestBit.is_true(text && strstr(text, "raw := `some content`;") != NULL,
                   "RT13: an untagged blob stays untagged");
   anvil_writer_dispose(copy);
}

/* BR-2610-anvl-parser-001: the writer's bare-word rule only reserves the EXACT literals, so a word
 * that merely begins with one is accepted and must read back as BARE (it used to fail to load). */
static void test_rt_literal_prefixed_bare_words(void) {
   const char *words[] = {"nullable", "nulls", "truely", "falsey", "nullx"};
   for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
      anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
      TestBit.is_true(anvil_writer_statement(w, "x", NULL) && anvil_writer_bare(w, words[i]) &&
                         anvil_writer_finish(w), words[i]);
      size_t len = 0;
      const char *data = anvil_writer_data(w, &len);
      anvil_document doc = data ? anvil_load_buffer(data, len) : NULL;
      anvil_statement st = doc ? anvil_document_find_statement(doc, "x") : NULL;
      anvil_value v = st ? anvil_statement_get_value(st) : NULL;
      char text[32] = {0};
      if (v) {
         anvil_value_get_text(v, text, sizeof(text));
      }
      TestBit.is_true(doc && !anvil_has_errors(doc) && v && anvil_value_get_type(v) == ANVIL_VALUE_BARE &&
                         strcmp(text, words[i]) == 0, "round-trips as BARE");
      if (doc) {
         anvil_dispose(doc);
      }
      anvil_writer_dispose(w);
   }
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   anvil_writer_statement(w, "x", NULL);
   TestBit.is_false(anvil_writer_bare(w, "null"), "the exact literal null is still rejected as a bare word");
   TestBit.is_false(anvil_writer_bare(w, "true"), "and true");
   TestBit.is_false(anvil_writer_bare(w, "false"), "and false");
   anvil_writer_dispose(w);
}

int main(void) {
   TestBit.run_ex("RT01_scalars", NULL, test_rt01_scalars, th);
   TestBit.run_ex("RT02_string_escapes", NULL, test_rt02_string_escapes, th);
   TestBit.run_ex("RT03_edge_tokens", NULL, test_rt03_edge_tokens, th);
   TestBit.run_ex("RT04_int_and_double", NULL, test_rt04_int_and_double, th);
   TestBit.run_ex("RT05_collections", NULL, test_rt05_collections, th);
   TestBit.run_ex("RT06_attributes_and_base", NULL, test_rt06_attributes_and_base, th);
   TestBit.run_ex("RT07_varref", NULL, test_rt07_varref, th);
   TestBit.run_ex("RT08_amp", NULL, test_rt08_amp, th);
   TestBit.run_ex("RT09_include", NULL, test_rt09_include, th);
   TestBit.run_ex("RT09b_include_and_module_attribute", NULL, test_rt09b_include_and_module_attribute, th);
   TestBit.run_ex("RT10_fixture_fixed_point", NULL, test_rt10_fixture_fixed_point, th);
   TestBit.run_ex("RT13_blob_tags_round_trip", NULL, test_rt13_blob_tags_round_trip, th);
   TestBit.run_ex("RT12_duplicate_names_match_reader", NULL, test_rt12_duplicate_names_match_reader,
                  th);
   TestBit.run_ex("RT14_writer_sweep", NULL, test_rt14_writer_sweep, th);
   TestBit.run_ex("RT14_builder_sweep", NULL, test_rt14_builder_sweep, th);
   TestBit.run_ex("RT14_builder_bases", NULL, test_rt14_builder_bases, th);
   TestBit.run_ex("RT14_anonymous_block_base", NULL, test_rt14_anonymous_block_base, th);
   TestBit.run_ex("RT14_builder_attribute_values", NULL, test_rt14_builder_attribute_values, th);
   TestBit.run_ex("RT11_accepted_candidates_read_back", NULL, test_rt11_accepted_candidates_read_back,
                  th);
   TestBit.run_ex("RT_LITERAL_PREFIXED_BARE_WORDS", NULL, test_rt_literal_prefixed_bare_words, th);

   return TestBit.report();
}
