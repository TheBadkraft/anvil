/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_builder_roundtrip.c - The builder's output through the real parser *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_builder_roundtrip.c                               *
 * ---------------------------------------------------------------------- *
 * Three checks: a hand-built document reads back as built; every real    *
 * fixture, copied into a builder through the public reader API, emits    *
 * text that re-copies to itself; and random trees built through the      *
 * builder emit byte-for-byte what the streaming writer emits for the     *
 * same structure, and parse. Links reader + writer add-on.               *
 * ********************************************************************** */

#include "anvil_builder.h"
#include "anvil_flat.h"
#include "anvil_types.h"
#include "anvil_writer.h"
#include "internal/source_registry.h"
#include "testbit.h"
// ----------------
#include "../utilities/helpers.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

static char *text_of(anvil_value v) {
   size_t need = anvil_value_get_text(v, NULL, 0);
   char *buf = malloc(need + 1);
   anvil_value_get_text(v, buf, need + 1);
   return buf;
}

/* ---------------------------------------------------------------------- *
 * RB01 - a hand-built document reads back as built
 * ---------------------------------------------------------------------- */
static void test_rb01_hand_built(void) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   anvil_builder_attribute(b, "doclevel", NULL);
   anvil_builder_add(b, NULL, "name", anvil_builder_string(b, "Dav\"id\n", 7));
   anvil_node tags = anvil_builder_array(b);
   anvil_builder_append(b, tags, anvil_builder_bare(b, "alpha"));
   anvil_builder_append(b, tags, anvil_builder_int(b, 2));
   anvil_builder_add(b, NULL, "tags", tags);
   anvil_node base = anvil_builder_object(b);
   anvil_builder_add(b, base, "x", anvil_builder_int(b, 1));
   anvil_builder_add(b, NULL, "base", base);
   anvil_node derived = anvil_builder_object(b);
   anvil_builder_add(b, derived, "y", anvil_builder_int(b, 2));
   anvil_member dm = anvil_builder_add(b, NULL, "derived", derived);
   anvil_builder_set_base(b, dm, "base");
   anvil_builder_member_attribute(b, dm, "env", "production");
   anvil_builder_add(b, NULL, "alias", anvil_builder_varref(b, "name"));

   size_t len = 0;
   const char *text = anvil_builder_emit(b, &len);
   TestBit.is_not_null(text, "RB01: emitted");
   anvil_document doc = text ? anvil_load_buffer(text, len) : NULL;
   TestBit.is_not_null(doc, "RB01: loaded");
   if (doc) {
      TestBit.is_false(anvil_has_errors(doc), "RB01: reader accepts the builder's output");
      TestBit.is_not_null(anvil_document_find_attribute(doc, "doclevel"), "RB01: module attribute");
      anvil_statement name = anvil_document_find_statement(doc, "name");
      char *got = name ? text_of(anvil_statement_get_value(name)) : NULL;
      TestBit.is_equal_str("Dav\"id\n", got ? got : "", "RB01: string with escapes");
      free(got);
      anvil_statement alias = anvil_document_find_statement(doc, "alias");
      got = alias ? text_of(anvil_statement_get_value(alias)) : NULL;
      TestBit.is_equal_str("Dav\"id\n", got ? got : "", "RB01: varref resolves");
      free(got);
      anvil_statement tags_stmt = anvil_document_find_statement(doc, "tags");
      TestBit.is_equal_int(2, tags_stmt ? (long long)anvil_value_get_count(anvil_statement_get_value(tags_stmt)) : -1,
                           "RB01: array length");
      anvil_statement d = anvil_document_find_statement(doc, "derived");
      TestBit.is_equal_int(2, d ? (long long)anvil_value_get_count(anvil_statement_get_value(d)) : -1,
                           "RB01: derived inherits base's field");
      TestBit.is_not_null(d ? anvil_statement_find_attribute(d, "env") : NULL, "RB01: statement attribute");
      anvil_dispose(doc);
   }
   anvil_builder_dispose(b);
}

/* ---------------------------------------------------------------------- *
 * RB02 - fixed point over every real fixture, via the builder
 * ---------------------------------------------------------------------- *
 * Blob tags and inheritance bases aren't readable through the reader API,
 * so blobs go in untagged and an inherited object goes in as its merged
 * fields (same limits as test_writer_roundtrip.c's RT10).
 */
static anvil_node node_from_value(anvil_builder b, anvil_value v);

static bool fill_object(anvil_builder b, anvil_node obj, anvil_value v) {
   size_t n = anvil_value_get_count(v);
   for (size_t i = 0; i < n; i++) {
      anvil_statement s = anvil_value_get_statement(v, i);
      char name[256];
      anvil_statement_get_name(s, name, sizeof name);
      anvil_node child = node_from_value(b, anvil_statement_get_value(s));
      anvil_member m = child ? anvil_builder_add(b, obj, name, child) : NULL;
      if (!m) {
         return false;
      }
      size_t attrs = anvil_statement_get_attribute_count(s);
      for (size_t a = 0; a < attrs; a++) {
         anvil_attribute attr = anvil_statement_get_attribute(s, a);
         char key[128], val[256];
         anvil_attribute_get_key(attr, key, sizeof key);
         size_t vlen = anvil_attribute_get_value(attr, val, sizeof val);
         if (!anvil_builder_member_attribute(b, m, key, vlen ? val : NULL)) {
            return false;
         }
      }
   }
   return true;
}

static anvil_node node_from_value(anvil_builder b, anvil_value v) {
   anvil_value_type kind = anvil_value_get_type(v);
   size_t need = anvil_value_get_text(v, NULL, 0);
   char *text = text_of(v);
   anvil_node node = NULL;
   switch (kind) {
   case ANVIL_VALUE_NULL:
      node = anvil_builder_null(b);
      break;
   case ANVIL_VALUE_BOOL:
      node = anvil_builder_bool(b, strcmp(text, "true") == 0);
      break;
   case ANVIL_VALUE_NUMERIC:
      node = anvil_builder_numeric(b, text);
      break;
   case ANVIL_VALUE_STRING:
      node = anvil_builder_string(b, text, need);
      break;
   case ANVIL_VALUE_BARE:
      node = anvil_builder_bare(b, text);
      break;
   case ANVIL_VALUE_BLOB:
      node = anvil_builder_blob(b, NULL, text, need);
      break;
   case ANVIL_VALUE_ARRAY:
   case ANVIL_VALUE_TUPLE: {
      node = kind == ANVIL_VALUE_ARRAY ? anvil_builder_array(b) : anvil_builder_tuple(b);
      size_t n = anvil_value_get_count(v);
      for (size_t i = 0; node && i < n; i++) {
         anvil_node element = node_from_value(b, anvil_value_get_element(v, i));
         if (!element || !anvil_builder_append(b, node, element)) {
            node = NULL;
         }
      }
      break;
   }
   case ANVIL_VALUE_OBJECT:
      node = anvil_builder_object(b);
      if (node && !fill_object(b, node, v)) {
         node = NULL;
      }
      break;
   }
   free(text);
   return node;
}

// Copies `doc` into a new builder, emits it, and returns the text (malloc'd), or NULL.
static char *emit_copy(anvil_document doc, size_t *out_len, const char *label) {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   bool ok = b != NULL;
   size_t attrs = anvil_document_get_attribute_count(doc);
   for (size_t a = 0; ok && a < attrs; a++) {
      anvil_attribute attr = anvil_document_get_attribute(doc, a);
      char key[128], val[256];
      anvil_attribute_get_key(attr, key, sizeof key);
      size_t vlen = anvil_attribute_get_value(attr, val, sizeof val);
      ok = anvil_builder_attribute(b, key, vlen ? val : NULL);
   }
   anvil_statement_iterator it = anvil_document_get_statements(doc);
   anvil_statement s = NULL;
   while (ok && anvil_statement_iterator_next(it, &s)) {
      char name[256];
      anvil_statement_get_name(s, name, sizeof name);
      anvil_node value = node_from_value(b, anvil_statement_get_value(s));
      anvil_member m = value ? anvil_builder_add(b, NULL, name, value) : NULL;
      ok = m != NULL;
      size_t sattrs = anvil_statement_get_attribute_count(s);
      for (size_t a = 0; ok && a < sattrs; a++) {
         anvil_attribute attr = anvil_statement_get_attribute(s, a);
         char key[128], val[256];
         anvil_attribute_get_key(attr, key, sizeof key);
         size_t vlen = anvil_attribute_get_value(attr, val, sizeof val);
         ok = anvil_builder_member_attribute(b, m, key, vlen ? val : NULL);
      }
   }
   anvil_statement_iterator_dispose(it);
   char *copy = NULL;
   const char *text = ok ? anvil_builder_emit(b, out_len) : NULL;
   if (text) {
      copy = strdup(text);
   } else if (b) {
      fprintf(stderr, "[%s] builder rejected a copied document: %s\n", label,
              anvil_writer_error_message(anvil_builder_get_error(b)));
   }
   anvil_builder_dispose(b);
   return copy;
}

static void test_rb02_fixture_fixed_point(void) {
   DIR *dir = opendir("../../test/fixtures");
   TestBit.is_not_null(dir, "RB02: fixtures directory opened");
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
         continue;
      }
      size_t len1 = 0;
      char *text1 = emit_copy(original, &len1, entry->d_name);
      TestBit.is_not_null(text1, entry->d_name);
      // Disposed before the re-read: identical source text is rejected as a registry duplicate.
      anvil_dispose(original);
      if (text1) {
         anvil_document reread = anvil_load_buffer(text1, len1);
         TestBit.is_true(reread && !anvil_has_errors(reread), entry->d_name);
         if (reread && !anvil_has_errors(reread)) {
            size_t len2 = 0;
            char *text2 = emit_copy(reread, &len2, entry->d_name);
            bool same = text2 && len1 == len2 && memcmp(text1, text2, len1) == 0;
            if (!same) {
               fprintf(stderr, "[%s] not a fixed point:\n--- first\n%s\n--- second\n%s\n", entry->d_name,
                       text1, text2 ? text2 : "(null)");
            }
            TestBit.is_true(same, entry->d_name);
            free(text2);
         }
         if (reread) {
            anvil_dispose(reread);
         }
         free(text1);
         copied++;
      }
   }
   closedir(dir);
   TestBit.is_true(copied >= 20, "RB02: at least 20 real fixtures round-tripped through the builder");
}

/* ---------------------------------------------------------------------- *
 * RB03 - random trees: builder output == streaming-writer output
 * ---------------------------------------------------------------------- *
 * The same seeded random choices drive two generators, one streaming into
 * a writer and one building a tree; they must produce identical text, and
 * the reader must accept it.
 */
static unsigned rng_state;
static unsigned rng(void) {
   rng_state = rng_state * 1664525u + 1013904223u;
   return rng_state >> 8;
}

static const char *STRINGS[] = {"plain", "with \"quotes\"", "back\\slash", "line\nbreak", "", "tab\there"};

static void gen_writer(anvil_writer w, int depth);
static void gen_object_writer(anvil_writer w, int depth) {
   anvil_writer_begin_object(w);
   unsigned fields = 1 + rng() % 3;
   for (unsigned i = 0; i < fields; i++) {
      char name[8];
      snprintf(name, sizeof name, "f%u", i);
      anvil_writer_statement(w, name, NULL);
      gen_writer(w, depth + 1);
   }
   anvil_writer_end_object(w);
}
static void gen_writer(anvil_writer w, int depth) {
   unsigned kind = rng() % 8;
   if (depth >= 3 && kind >= 5) {
      kind = rng() % 5;
   }
   switch (kind) {
   case 0:
      anvil_writer_int(w, (int)(rng() % 2000) - 1000);
      break;
   case 1: {
      char word[16];
      snprintf(word, sizeof word, "w%u", rng() % 100);
      anvil_writer_bare(w, word);
      break;
   }
   case 2: {
      const char *s = STRINGS[rng() % 6];
      anvil_writer_string(w, s, strlen(s));
      break;
   }
   case 3:
      anvil_writer_bool(w, rng() % 2);
      break;
   case 4:
      anvil_writer_null(w);
      break;
   case 5:
   case 6: {
      bool tuple = kind == 6;
      tuple ? anvil_writer_begin_tuple(w) : anvil_writer_begin_array(w);
      unsigned count = (tuple ? 2 : 1) + rng() % 3;
      for (unsigned i = 0; i < count; i++) {
         gen_writer(w, depth + 1);
      }
      tuple ? anvil_writer_end_tuple(w) : anvil_writer_end_array(w);
      break;
   }
   default:
      gen_object_writer(w, depth);
      break;
   }
}

static anvil_node gen_builder(anvil_builder b, int depth);
static anvil_node gen_object_builder(anvil_builder b, int depth) {
   anvil_node obj = anvil_builder_object(b);
   unsigned fields = 1 + rng() % 3;
   for (unsigned i = 0; i < fields; i++) {
      char name[8];
      snprintf(name, sizeof name, "f%u", i);
      anvil_builder_add(b, obj, name, gen_builder(b, depth + 1));
   }
   return obj;
}
static anvil_node gen_builder(anvil_builder b, int depth) {
   unsigned kind = rng() % 8;
   if (depth >= 3 && kind >= 5) {
      kind = rng() % 5;
   }
   switch (kind) {
   case 0:
      return anvil_builder_int(b, (int)(rng() % 2000) - 1000);
   case 1: {
      char word[16];
      snprintf(word, sizeof word, "w%u", rng() % 100);
      return anvil_builder_bare(b, word);
   }
   case 2: {
      const char *s = STRINGS[rng() % 6];
      return anvil_builder_string(b, s, strlen(s));
   }
   case 3:
      return anvil_builder_bool(b, rng() % 2);
   case 4:
      return anvil_builder_null(b);
   case 5:
   case 6: {
      bool tuple = kind == 6;
      anvil_node c = tuple ? anvil_builder_tuple(b) : anvil_builder_array(b);
      unsigned count = (tuple ? 2 : 1) + rng() % 3;
      for (unsigned i = 0; i < count; i++) {
         anvil_builder_append(b, c, gen_builder(b, depth + 1));
      }
      return c;
   }
   default:
      return gen_object_builder(b, depth);
   }
}

static void test_rb03_random_trees(void) {
   int mismatches = 0, rejected = 0;
   for (unsigned seed = 1; seed <= 500; seed++) {
      rng_state = seed;
      anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
      unsigned statements = 1 + rng() % 5;
      for (unsigned i = 0; i < statements; i++) {
         char name[8];
         snprintf(name, sizeof name, "s%u", i);
         anvil_writer_statement(w, name, NULL);
         gen_writer(w, 0);
      }
      bool wrote = anvil_writer_finish(w);

      rng_state = seed;
      anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
      statements = 1 + rng() % 5;
      for (unsigned i = 0; i < statements; i++) {
         char name[8];
         snprintf(name, sizeof name, "s%u", i);
         anvil_builder_add(b, NULL, name, gen_builder(b, 0));
      }
      size_t built_len = 0;
      const char *built = anvil_builder_emit(b, &built_len);

      size_t wrote_len = 0;
      const char *streamed = wrote ? anvil_writer_data(w, &wrote_len) : NULL;
      if (!wrote || !built || !streamed || built_len != wrote_len || memcmp(built, streamed, wrote_len) != 0) {
         mismatches++;
         fprintf(stderr, "[seed %u] builder and writer disagree\n--- writer\n%s\n--- builder\n%s\n", seed,
                 streamed ? streamed : "(null)", built ? built : "(null)");
      } else {
         th();
         anvil_document doc = anvil_load_buffer(built, built_len);
         if (!doc || anvil_has_errors(doc)) {
            rejected++;
            fprintf(stderr, "[seed %u] reader rejected:\n%s\n", seed, built);
         }
         if (doc) {
            anvil_dispose(doc);
         }
      }
      anvil_writer_dispose(w);
      anvil_builder_dispose(b);
   }
   TestBit.is_equal_int(0, mismatches, "RB03: builder output equals writer output for 500 random trees");
   TestBit.is_equal_int(0, rejected, "RB03: reader accepts all 500 random trees");
}

int main(void) {
   TestBit.run_ex("RB01_hand_built", NULL, test_rb01_hand_built, th);
   TestBit.run_ex("RB02_fixture_fixed_point", NULL, test_rb02_fixture_fixed_point, th);
   TestBit.run_ex("RB03_random_trees", NULL, test_rb03_random_trees, th);
   return TestBit.report();
}
