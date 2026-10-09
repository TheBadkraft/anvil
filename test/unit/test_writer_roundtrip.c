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
#include "anvil_writer.h"
#include "internal/source_registry.h"
#include "testbit.h"
// ----------------
#include "../utilities/helpers.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
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
   const char *bares[] = {"1e5",     "007x", "a-b",        "/usr/bin", "a:b",  "x$y",
                          ".hidden", "_x",   "Truthy",     "1.5.2",    "09-02-2026", "//unc"};
   const size_t bare_count = sizeof bares / sizeof *bares - 1; // "//unc" is rejected by the writer
   const char *numerics[] = {"0", "-0", "007", "3.14", "-1285", "1.7976931348623157e+308", "5e-07",
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

/* ---------------------------------------------------------------------- *
 * RT10 - fixed point over every real fixture
 * ---------------------------------------------------------------------- *
 * Copies a loaded document back out through the writer using only the
 * public reader API, then repeats on the result: the second copy must be
 * byte-identical to the first (the writer's output is canonical), and
 * must parse without errors. Blob tags and inheritance bases aren't
 * exposed by the reader API, so blobs copy untagged and an inherited
 * object copies as its merged fields.
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
   case ANVIL_VALUE_BLOB:
      ok = anvil_writer_blob(w, NULL, text, need);
      break;
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
         char *text1 = strdup(anvil_writer_data(first, &len1));
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
   const char *attr_alpha = "ab1=,]\" $-.";
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
   TestBit.run_ex("RT10_fixture_fixed_point", NULL, test_rt10_fixture_fixed_point, th);
   TestBit.run_ex("RT11_accepted_candidates_read_back", NULL, test_rt11_accepted_candidates_read_back,
                  th);
   return TestBit.report();
}
