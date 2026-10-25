/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_anvil_vtable.c - Unit tests for the vtable convenience layer      *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_anvil_vtable.c                                    *
 * ---------------------------------------------------------------------- *
 * Every vtable field must be the *exact same function* as its flat       *
 * counterpart (pointer identity, not just matching behavior) — this is   *
 * the mechanical drift check notes/public-api.md calls for. One          *
 * end-to-end smoke test proves the vtable alone (never naming a flat     *
 * function) is genuinely usable on its own.                              *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_types.h"
#include "anvil_vtable.h"
#include "internal/source_registry.h"
#include "testbit.h"
// ----------------
#include "../utilities/helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

/* ---------------------------------------------------------------------- *
 * VT01 — every Anvil vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt01_anvil_matches_flat(void) {
   TestBit.is_true(Anvil.load == anvil_load, "VT01: Anvil.load is anvil_load");
   TestBit.is_true(Anvil.load_buffer == anvil_load_buffer, "VT01: Anvil.load_buffer is anvil_load_buffer");
   TestBit.is_true(Anvil.dispose == anvil_dispose, "VT01: Anvil.dispose is anvil_dispose");
   TestBit.is_true(Anvil.has_errors == anvil_has_errors,
                   "VT01: Anvil.has_errors is anvil_has_errors");
   TestBit.is_true(Anvil.get_error == anvil_get_error, "VT01: Anvil.get_error is anvil_get_error");
   TestBit.is_true(Anvil.get_version == anvil_get_version,
                   "VT01: Anvil.get_version is anvil_get_version");
   TestBit.is_true(Anvil.parse_value_fragment == anvil_parse_value_fragment,
                   "VT01: Anvil.parse_value_fragment is anvil_parse_value_fragment");
}
/* ---------------------------------------------------------------------- *
 * VT02 — every Statement vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt02_statement_matches_flat(void) {
   TestBit.is_true(Statement.get_value == anvil_statement_get_value,
                   "VT02: Statement.get_value is anvil_statement_get_value");
   TestBit.is_true(Statement.get_name == anvil_statement_get_name,
                   "VT02: Statement.get_name is anvil_statement_get_name");
   TestBit.is_true(Statement.get_attribute_count == anvil_statement_get_attribute_count,
                   "VT02: Statement.get_attribute_count is anvil_statement_get_attribute_count");
   TestBit.is_true(Statement.get_attribute == anvil_statement_get_attribute,
                   "VT02: Statement.get_attribute is anvil_statement_get_attribute");
   TestBit.is_true(Statement.find_attribute == anvil_statement_find_attribute,
                   "VT02: Statement.find_attribute is anvil_statement_find_attribute");
}
/* ---------------------------------------------------------------------- *
 * VT03 — every Value vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt03_value_matches_flat(void) {
   TestBit.is_true(Value.get_type == anvil_value_get_type, "VT03: Value.get_type is anvil_value_get_type");
   TestBit.is_true(Value.get_text == anvil_value_get_text, "VT03: Value.get_text is anvil_value_get_text");
   TestBit.is_true(Value.get_count == anvil_value_get_count,
                   "VT03: Value.get_count is anvil_value_get_count");
   TestBit.is_true(Value.get_element == anvil_value_get_element,
                   "VT03: Value.get_element is anvil_value_get_element");
   TestBit.is_true(Value.get_statement == anvil_value_get_statement,
                   "VT03: Value.get_statement is anvil_value_get_statement");
   TestBit.is_true(Value.find_statement == anvil_value_find_statement,
                   "VT03: Value.find_statement is anvil_value_find_statement");
   TestBit.is_true(Value.get_numeric == anvil_value_get_numeric,
                   "VT03: Value.get_numeric is anvil_value_get_numeric");
   TestBit.is_true(Value.get_bytes == anvil_value_get_bytes,
                   "VT03: Value.get_bytes is anvil_value_get_bytes");
   TestBit.is_true(Value.get_blob_tag == anvil_value_get_blob_tag,
                   "VT03: Value.get_blob_tag is anvil_value_get_blob_tag");
}
/* ---------------------------------------------------------------------- *
 * VT05 — every Document vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt05_document_matches_flat(void) {
   TestBit.is_true(Document.get_attribute_count == anvil_document_get_attribute_count,
                   "VT05: Document.get_attribute_count is anvil_document_get_attribute_count");
   TestBit.is_true(Document.get_attribute == anvil_document_get_attribute,
                   "VT05: Document.get_attribute is anvil_document_get_attribute");
   TestBit.is_true(Document.find_attribute == anvil_document_find_attribute,
                   "VT05: Document.find_attribute is anvil_document_find_attribute");
   TestBit.is_true(Document.find_statement == anvil_document_find_statement,
                   "VT05: Document.find_statement is anvil_document_find_statement");
   TestBit.is_true(Document.get_fragment_value == anvil_document_get_fragment_value,
                   "VT05: Document.get_fragment_value is anvil_document_get_fragment_value");
   TestBit.is_true(Document.get_statements == anvil_document_get_statements,
                   "VT05: Document.get_statements is anvil_document_get_statements");
   TestBit.is_true(Document.get_includes == anvil_document_get_includes,
                   "VT05: Document.get_includes is anvil_document_get_includes");
   TestBit.is_true(Document.get_error == anvil_document_get_error,
                   "VT05: Document.get_error is anvil_document_get_error");
}
/* ---------------------------------------------------------------------- *
 * VT08 — every StatementIterator vtable field is pointer-identical to its
 * flat counterpart
 * ---------------------------------------------------------------------- */
static void test_vt08_statement_iterator_matches_flat(void) {
   TestBit.is_true(StatementIterator.next == anvil_statement_iterator_next,
                   "VT08: StatementIterator.next is anvil_statement_iterator_next");
   TestBit.is_true(StatementIterator.dispose == anvil_statement_iterator_dispose,
                   "VT08: StatementIterator.dispose is anvil_statement_iterator_dispose");
}
/* ---------------------------------------------------------------------- *
 * VT09 — every DocumentIterator vtable field is pointer-identical to its
 * flat counterpart
 * ---------------------------------------------------------------------- */
static void test_vt09_document_iterator_matches_flat(void) {
   TestBit.is_true(DocumentIterator.next == anvil_document_iterator_next,
                   "VT09: DocumentIterator.next is anvil_document_iterator_next");
   TestBit.is_true(DocumentIterator.dispose == anvil_document_iterator_dispose,
                   "VT09: DocumentIterator.dispose is anvil_document_iterator_dispose");
}
/* ---------------------------------------------------------------------- *
 * VT07 — every Error vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt07_error_matches_flat(void) {
   TestBit.is_true(Error.get_category == anvil_error_get_category,
                   "VT07: Error.get_category is anvil_error_get_category");
   TestBit.is_true(Error.get_message == anvil_error_get_message,
                   "VT07: Error.get_message is anvil_error_get_message");
   TestBit.is_true(Error.get_line == anvil_error_get_line,
                   "VT07: Error.get_line is anvil_error_get_line");
   TestBit.is_true(Error.get_column == anvil_error_get_column,
                   "VT07: Error.get_column is anvil_error_get_column");
}
/* ---------------------------------------------------------------------- *
 * VT06 — every Attribute vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt06_attribute_matches_flat(void) {
   TestBit.is_true(Attribute.get_key == anvil_attribute_get_key,
                   "VT06: Attribute.get_key is anvil_attribute_get_key");
   TestBit.is_true(Attribute.get_value == anvil_attribute_get_value,
                   "VT06: Attribute.get_value is anvil_attribute_get_value");
}
/* ---------------------------------------------------------------------- *
 * VT04 — end-to-end smoke test using only the vtable, never a flat
 * function name, proving the vtable alone is genuinely usable
 * ---------------------------------------------------------------------- */
static void test_vt04_vtable_only_smoke_test(void) {
   anvil_document doc = Anvil.load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "VT04: document loaded via the vtable");
   if (!doc) {
      return;
   }
   TestBit.is_false(Anvil.has_errors(doc), "VT04: no errors, via the vtable");

   anvil_statement name = Document.find_statement(doc, "name");
   TestBit.is_not_null(name, "VT04: 'name' statement found via the vtable");
   anvil_value name_val = Statement.get_value(name);
   TestBit.is_equal_int(ANVIL_VALUE_BARE, Value.get_type(name_val),
                        "VT04: 'name' is ANVIL_VALUE_BARE, via the vtable");

   Anvil.dispose(doc);

   anvil_document fragment = Anvil.parse_value_fragment("42", 2);
   TestBit.is_not_null(fragment, "VT04: fragment document allocated via the vtable");
   if (fragment) {
      TestBit.is_false(Anvil.has_errors(fragment), "VT04: fragment has no errors, via the vtable");
      anvil_value fragment_val = Document.get_fragment_value(fragment);
      TestBit.is_equal_int(ANVIL_VALUE_NUMERIC, Value.get_type(fragment_val),
                           "VT04: fragment value is ANVIL_VALUE_NUMERIC, via the vtable");
      Anvil.dispose(fragment);
   }
}


/* ---------------------------------------------------------------------- *
 * VT10 (BR-2610-anvl-vtable-001) - Document.get_path
 * ---------------------------------------------------------------------- */
static void test_vt10_document_get_path(void) {
   TestBit.is_true(Document.get_path == anvil_document_get_path,
                   "VT10: Document.get_path is anvil_document_get_path");
   if (!Document.get_path) {
      return;
   }
   anvil_document file_doc = anvil_load(fixture_path("types_basic.anvl"));
   TestBit.is_not_null(file_doc, "VT10: file document loaded");
   if (file_doc) {
      char via_vt[512] = {0}, via_flat[512] = {0};
      size_t a = Document.get_path(file_doc, via_vt, sizeof(via_vt));
      size_t b = anvil_document_get_path(file_doc, via_flat, sizeof(via_flat));
      TestBit.is_true(a == b && a > 0 && strcmp(via_vt, via_flat) == 0 &&
                         strstr(via_vt, "types_basic.anvl") != NULL,
                      "VT10: same path through the vtable as through the flat call");
      TestBit.is_equal_int((long long)a, (long long)Document.get_path(file_doc, NULL, 0),
                           "VT10: the length query matches");
      anvil_dispose(file_doc);
   }
   const char *src = "#!aml\nx := 1;\n";
   anvil_document buf_doc = anvil_load_buffer(src, strlen(src));
   if (buf_doc) {
      char via_vt[64] = {0}, via_flat[64] = {0};
      size_t a = Document.get_path(buf_doc, via_vt, sizeof(via_vt));
      size_t b = anvil_document_get_path(buf_doc, via_flat, sizeof(via_flat));
      TestBit.is_true(a == b && strcmp(via_vt, via_flat) == 0,
                      "VT10: a buffer document reads the same through both");
      anvil_dispose(buf_doc);
   }
   TestBit.is_equal_int(0, (long long)Document.get_path(NULL, NULL, 0), "VT10: NULL document");
}

/* ---------------------------------------------------------------------- *
 * VT11 (BR-2610-anvl-vtable-001) - completeness: every function declared in anvil_flat.h has a
 * vtable member (pointer-identical), unless it is listed flat-only below with a reason. Adding
 * a flat function without either fails here.
 * ---------------------------------------------------------------------- */
struct vt_entry {
   const char *flat_name;
   const void *flat_fn;
   const void *vtable_member; // NULL only for a flat-only entry
   const char *flat_only_reason;
};

#define VT_MEMBER(flat, member) {#flat, (const void *)flat, (const void *)(member), NULL}
#define VT_FLAT_ONLY(flat, why) {#flat, (const void *)flat, NULL, why}


// Every function name declared in the header: a line starting in column 0 whose first '(' is
// preceded by an anvil_* identifier (comments, typedefs, preprocessor lines never match).
static size_t header_function_names(const char *header_text, char names[][96], size_t max) {
   size_t count = 0;
   const char *line = header_text;
   while (*line && count < max) {
      const char *eol = strchr(line, '\n');
      size_t len = eol ? (size_t)(eol - line) : strlen(line);
      char first = len ? line[0] : ' ';
      if ((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || first == '_') {
         const char *paren = memchr(line, '(', len);
         if (paren) {
            const char *end = paren;
            const char *start = end;
            while (start > line && ((start[-1] >= 'a' && start[-1] <= 'z') ||
                                    (start[-1] >= '0' && start[-1] <= '9') || start[-1] == '_')) {
               start--;
            }
            size_t n = (size_t)(end - start);
            if (n > 6 && strncmp(start, "anvil_", 6) == 0 && n < 96) {
               memcpy(names[count], start, n);
               names[count][n] = '\0';
               count++;
            }
         }
      }
      line += len + (eol ? 1 : 0);
   }
   return count;
}

static char *read_whole_file(const char *path) {
   FILE *f = fopen(path, "rb");
   if (!f) {
      return NULL;
   }
   fseek(f, 0, SEEK_END);
   long n = ftell(f);
   fseek(f, 0, SEEK_SET);
   char *text = malloc((size_t)n + 1);
   if (text) {
      size_t got = fread(text, 1, (size_t)n, f);
      text[got] = '\0';
   }
   fclose(f);
   return text;
}

static void test_vt11_vtables_cover_every_flat_function(void) {
const struct vt_entry VT_TABLE[] = { // automatic: vtable members are not constant expressions
   VT_MEMBER(anvil_load, Anvil.load),
   VT_MEMBER(anvil_load_buffer, Anvil.load_buffer),
   VT_MEMBER(anvil_parse_value_fragment, Anvil.parse_value_fragment),
   VT_MEMBER(anvil_dispose, Anvil.dispose),
   VT_MEMBER(anvil_has_errors, Anvil.has_errors),
   VT_MEMBER(anvil_get_error, Anvil.get_error),
   VT_MEMBER(anvil_get_version, Anvil.get_version),
   VT_MEMBER(anvil_document_get_error, Document.get_error),
   VT_MEMBER(anvil_error_get_category, Error.get_category),
   VT_MEMBER(anvil_error_get_message, Error.get_message),
   VT_MEMBER(anvil_error_get_line, Error.get_line),
   VT_MEMBER(anvil_error_get_column, Error.get_column),
   VT_MEMBER(anvil_document_find_statement, Document.find_statement),
   VT_MEMBER(anvil_document_get_statements, Document.get_statements),
   VT_MEMBER(anvil_statement_iterator_next, StatementIterator.next),
   VT_MEMBER(anvil_statement_iterator_dispose, StatementIterator.dispose),
   VT_MEMBER(anvil_document_get_includes, Document.get_includes),
   VT_MEMBER(anvil_document_iterator_next, DocumentIterator.next),
   VT_MEMBER(anvil_document_iterator_dispose, DocumentIterator.dispose),
   VT_MEMBER(anvil_statement_get_value, Statement.get_value),
   VT_MEMBER(anvil_statement_get_name, Statement.get_name),
   VT_MEMBER(anvil_value_get_type, Value.get_type),
   VT_MEMBER(anvil_value_get_bytes, Value.get_bytes),
   VT_MEMBER(anvil_value_get_blob_tag, Value.get_blob_tag),
   VT_MEMBER(anvil_value_get_text, Value.get_text),
   VT_MEMBER(anvil_value_get_numeric, Value.get_numeric),
   VT_MEMBER(anvil_value_get_count, Value.get_count),
   VT_MEMBER(anvil_value_get_element, Value.get_element),
   VT_MEMBER(anvil_value_get_statement, Value.get_statement),
   VT_MEMBER(anvil_value_find_statement, Value.find_statement),
   VT_MEMBER(anvil_document_get_fragment_value, Document.get_fragment_value),
   VT_MEMBER(anvil_document_get_attribute_count, Document.get_attribute_count),
   VT_MEMBER(anvil_document_get_attribute, Document.get_attribute),
   VT_MEMBER(anvil_document_find_attribute, Document.find_attribute),
   VT_MEMBER(anvil_document_get_path, Document.get_path),
   VT_MEMBER(anvil_statement_get_attribute_count, Statement.get_attribute_count),
   VT_MEMBER(anvil_statement_get_attribute, Statement.get_attribute),
   VT_MEMBER(anvil_statement_find_attribute, Statement.find_attribute),
   VT_MEMBER(anvil_attribute_get_key, Attribute.get_key),
   VT_MEMBER(anvil_attribute_get_value, Attribute.get_value),
   // Flat-only functions go here as VT_FLAT_ONLY(anvil_x, "why a binding doesn't need it").
};
   char *header = read_whole_file("../../include/anvil_flat.h");
   TestBit.is_not_null(header, "VT11: anvil_flat.h readable");
   if (!header) {
      return;
   }
   static char names[256][96];
   size_t found = header_function_names(header, names, 256);
   free(header);
   TestBit.is_true(found > 30, "VT11: the header scan finds the flat API");

   size_t table_len = sizeof(VT_TABLE) / sizeof(VT_TABLE[0]);
   for (size_t i = 0; i < found; i++) {
      const struct vt_entry *entry = NULL;
      for (size_t j = 0; j < table_len; j++) {
         if (strcmp(VT_TABLE[j].flat_name, names[i]) == 0) {
            entry = &VT_TABLE[j];
         }
      }
      if (!entry) {
         printf("    VT11: %s is declared in anvil_flat.h but has no vtable member and is not "
                "listed flat-only\n", names[i]);
      }
      TestBit.is_true(entry != NULL, names[i]);
      if (entry && !entry->flat_only_reason) {
         TestBit.is_true(entry->vtable_member != NULL && entry->vtable_member == entry->flat_fn,
                         names[i]);
      }
   }
   // The table must not name a function the header no longer declares.
   for (size_t j = 0; j < table_len; j++) {
      bool declared = false;
      for (size_t i = 0; i < found; i++) {
         declared = declared || strcmp(VT_TABLE[j].flat_name, names[i]) == 0;
      }
      TestBit.is_true(declared, VT_TABLE[j].flat_name);
   }
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("VT01_anvil_matches_flat", NULL, test_vt01_anvil_matches_flat, th);
   TestBit.run_ex("VT02_statement_matches_flat", NULL, test_vt02_statement_matches_flat, th);
   TestBit.run_ex("VT03_value_matches_flat", NULL, test_vt03_value_matches_flat, th);
   TestBit.run_ex("VT04_vtable_only_smoke_test", NULL, test_vt04_vtable_only_smoke_test, th);
   TestBit.run_ex("VT05_document_matches_flat", NULL, test_vt05_document_matches_flat, th);
   TestBit.run_ex("VT06_attribute_matches_flat", NULL, test_vt06_attribute_matches_flat, th);
   TestBit.run_ex("VT07_error_matches_flat", NULL, test_vt07_error_matches_flat, th);
   TestBit.run_ex("VT08_statement_iterator_matches_flat", NULL,
                  test_vt08_statement_iterator_matches_flat, th);
   TestBit.run_ex("VT09_document_iterator_matches_flat", NULL,
                  test_vt09_document_iterator_matches_flat, th);
   TestBit.run_ex("VT10_document_get_path", NULL, test_vt10_document_get_path, th);
   TestBit.run_ex("VT11_vtables_cover_every_flat_function", NULL,
                  test_vt11_vtables_cover_every_flat_function, th);

   return TestBit.report();
}
