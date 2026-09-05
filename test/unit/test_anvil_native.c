/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_anvil_native.c - Unit tests for the public ABI (Anvil Native)     *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_anvil_native.c                                    *
 * ---------------------------------------------------------------------- *
 * anvil_load / anvil_dispose / anvil_has_errors / anvil_get_error — the  *
 * minimal Anvil lifecycle slice. See notes/public-api.md.                *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_types.h"
#include "internal/source_registry.h"
#include "testbit.h"
// ----------------
#include "../utilities/helpers.h"
#include <sigma/allocator.h>
#include <string.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

/* ---------------------------------------------------------------------- *
 * ANV01 — a clean, error-free document loads with no errors
 * ---------------------------------------------------------------------- */
static void test_anv01_load_clean_document(void) {
   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "ANV01: document handle returned");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "ANV01: no errors recorded");
   TestBit.is_equal_int(ANVIL_OK, anvil_get_error(doc), "ANV01: error category is ANVIL_OK");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV02 — a body syntax error reports ANVIL_ERR_SYNTAX
 * ---------------------------------------------------------------------- */
static void test_anv02_body_syntax_error(void) {
   anvil_document doc = anvil_load(fixture_path("body_err_unterminated_block.anvl"));
   TestBit.is_not_null(doc, "ANV02: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV02: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_SYNTAX, anvil_get_error(doc),
                        "ANV02: error category is ANVIL_ERR_SYNTAX");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV03 — a nonexistent file reports ANVIL_ERR_IO
 * ---------------------------------------------------------------------- */
static void test_anv03_file_not_found(void) {
   anvil_document doc = anvil_load(fixture_path("anvil_nonexistent_file_xyz.anvl"));
   TestBit.is_not_null(doc, "ANV03: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV03: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_IO, anvil_get_error(doc), "ANV03: error category is ANVIL_ERR_IO");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV04 — a missing import target reports ANVIL_ERR_IMPORT
 * ---------------------------------------------------------------------- */
static void test_anv04_import_error(void) {
   anvil_document doc = anvil_load(fixture_path("hdr_import_missing.anvl"));
   TestBit.is_not_null(doc, "ANV04: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV04: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_IMPORT, anvil_get_error(doc),
                        "ANV04: error category is ANVIL_ERR_IMPORT");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV05 — a resolver-phase duplicate identifier reports ANVIL_ERR_RESOLVE
 * ---------------------------------------------------------------------- */
static void test_anv05_resolve_error(void) {
   anvil_document doc = anvil_load(fixture_path("anvil_err_resolve_duplicate.anvl"));
   TestBit.is_not_null(doc, "ANV05: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV05: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_RESOLVE, anvil_get_error(doc),
                        "ANV05: error category is ANVIL_ERR_RESOLVE");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV06 — a duplicate shebang (header-scan failure) reports ANVIL_ERR_HEADER
 * ---------------------------------------------------------------------- */
static void test_anv06_header_error(void) {
   anvil_document doc = anvil_load(fixture_path("anvil_err_header_dup_shebang.anvl"));
   TestBit.is_not_null(doc, "ANV06: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV06: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_HEADER, anvil_get_error(doc),
                        "ANV06: error category is ANVIL_ERR_HEADER");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV07 — dispose(NULL) and the accessors are safe on a NULL handle
 * ---------------------------------------------------------------------- */
static void test_anv07_null_handle_safety(void) {
   anvil_dispose(NULL); // must not crash
   TestBit.is_false(anvil_has_errors(NULL), "ANV07: has_errors(NULL) is false");
   TestBit.is_equal_int(ANVIL_ERR_INVALID_ARGUMENT, anvil_get_error(NULL),
                        "ANV07: get_error(NULL) is ANVIL_ERR_INVALID_ARGUMENT");
}
/* ---------------------------------------------------------------------- *
 * ANV08 — get_version reports a non-empty string
 * ---------------------------------------------------------------------- */
static void test_anv08_get_version(void) {
   const char *version = anvil_get_version();
   TestBit.is_not_null(version, "ANV08: version string returned");
   if (version) {
      TestBit.is_true(version[0] != '\0', "ANV08: version string is non-empty");
   }
}

/* Loads anvil_accessors.anvl — every scalar/collection kind, plus a VarRef to
 * a scalar and a VarRef to an object, for the Statement/Value accessor tests
 * below. */
static anvil_document load_accessors_doc(void) {
   return anvil_load(fixture_path("anvil_accessors.anvl"));
}

/* ---------------------------------------------------------------------- *
 * ANV09 — scalar accessors: BOOL, NUMERIC, IDENTIFIER text round-trips
 * ---------------------------------------------------------------------- */
static void test_anv09_scalar_accessors(void) {
   anvil_document doc = load_accessors_doc();
   TestBit.is_not_null(doc, "ANV09: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "ANV09: no errors");

   anvil_statement flag = anvil_statement_get(doc, "flag");
   TestBit.is_not_null(flag, "ANV09: 'flag' statement found");
   anvil_value flag_val = anvil_statement_get_value(flag);
   TestBit.is_equal_int(ANVIL_VALUE_BOOL, anvil_value_get_type(flag_val),
                        "ANV09: 'flag' is ANVIL_VALUE_BOOL");
   char buf[64] = {0};
   size_t len = anvil_value_get_text(flag_val, buf, sizeof(buf));
   TestBit.is_equal_int(4, (long long)len, "ANV09: 'flag' text length is 4");
   TestBit.is_true(0 == strcmp("true", buf), "ANV09: 'flag' text is 'true'");

   anvil_statement count = anvil_statement_get(doc, "count");
   anvil_value count_val = anvil_statement_get_value(count);
   TestBit.is_equal_int(ANVIL_VALUE_NUMERIC, anvil_value_get_type(count_val),
                        "ANV09: 'count' is ANVIL_VALUE_NUMERIC");
   anvil_value_get_text(count_val, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("42", buf), "ANV09: 'count' text is '42'");

   anvil_statement word = anvil_statement_get(doc, "word");
   anvil_value word_val = anvil_statement_get_value(word);
   TestBit.is_equal_int(ANVIL_VALUE_IDENTIFIER, anvil_value_get_type(word_val),
                        "ANV09: 'word' is ANVIL_VALUE_IDENTIFIER");
   anvil_value_get_text(word_val, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("bareword", buf), "ANV09: 'word' text is 'bareword'");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV10 — STRING accessor resolves escape sequences (real bytes, not the
 * literal backslash-n the source contains)
 * ---------------------------------------------------------------------- */
static void test_anv10_string_escape_resolution(void) {
   anvil_document doc = load_accessors_doc();
   TestBit.is_not_null(doc, "ANV10: document loaded");
   if (!doc) {
      return;
   }

   anvil_statement greeting = anvil_statement_get(doc, "greeting");
   anvil_value greeting_val = anvil_statement_get_value(greeting);
   TestBit.is_equal_int(ANVIL_VALUE_STRING, anvil_value_get_type(greeting_val),
                        "ANV10: 'greeting' is ANVIL_VALUE_STRING");

   char buf[64] = {0};
   size_t len = anvil_value_get_text(greeting_val, buf, sizeof(buf));
   TestBit.is_equal_int(11, (long long)len, "ANV10: resolved length is 11 (one byte for '\\n')");
   TestBit.is_true(0 == strcmp("hello\nworld", buf),
                   "ANV10: text has a real newline byte, not a literal backslash-n");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV11 — BLOB text is the raw content span, untouched
 * ---------------------------------------------------------------------- */
static void test_anv11_blob_accessor(void) {
   anvil_document doc = load_accessors_doc();
   TestBit.is_not_null(doc, "ANV11: document loaded");
   if (!doc) {
      return;
   }

   anvil_statement tag = anvil_statement_get(doc, "tag");
   anvil_value tag_val = anvil_statement_get_value(tag);
   TestBit.is_equal_int(ANVIL_VALUE_BLOB, anvil_value_get_type(tag_val),
                        "ANV11: 'tag' is ANVIL_VALUE_BLOB");
   char buf[64] = {0};
   anvil_value_get_text(tag_val, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("2026-01-01", buf), "ANV11: blob text is '2026-01-01'");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV12 — ARRAY/TUPLE: count and indexed element access
 * ---------------------------------------------------------------------- */
static void test_anv12_array_tuple_accessors(void) {
   anvil_document doc = load_accessors_doc();
   TestBit.is_not_null(doc, "ANV12: document loaded");
   if (!doc) {
      return;
   }

   anvil_statement items = anvil_statement_get(doc, "items");
   anvil_value items_val = anvil_statement_get_value(items);
   TestBit.is_equal_int(ANVIL_VALUE_ARRAY, anvil_value_get_type(items_val),
                        "ANV12: 'items' is ANVIL_VALUE_ARRAY");
   TestBit.is_equal_int(3, (long long)anvil_value_get_count(items_val),
                        "ANV12: 'items' has 3 elements");

   char buf[16] = {0};
   anvil_value first = anvil_value_get_element(items_val, 0);
   anvil_value_get_text(first, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("1", buf), "ANV12: items[0] text is '1'");

   anvil_value third = anvil_value_get_element(items_val, 2);
   anvil_value_get_text(third, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("3", buf), "ANV12: items[2] text is '3'");

   TestBit.is_null(anvil_value_get_element(items_val, 3), "ANV12: items[3] is out of bounds");

   anvil_statement pair = anvil_statement_get(doc, "pair");
   anvil_value pair_val = anvil_statement_get_value(pair);
   TestBit.is_equal_int(ANVIL_VALUE_TUPLE, anvil_value_get_type(pair_val),
                        "ANV12: 'pair' is ANVIL_VALUE_TUPLE");
   TestBit.is_equal_int(2, (long long)anvil_value_get_count(pair_val),
                        "ANV12: 'pair' has 2 elements");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV13 — OBJECT: count, nested statement access, nested name/value
 * ---------------------------------------------------------------------- */
static void test_anv13_object_accessors(void) {
   anvil_document doc = load_accessors_doc();
   TestBit.is_not_null(doc, "ANV13: document loaded");
   if (!doc) {
      return;
   }

   anvil_statement config = anvil_statement_get(doc, "config");
   anvil_value config_val = anvil_statement_get_value(config);
   TestBit.is_equal_int(ANVIL_VALUE_OBJECT, anvil_value_get_type(config_val),
                        "ANV13: 'config' is ANVIL_VALUE_OBJECT");
   TestBit.is_equal_int(2, (long long)anvil_value_get_count(config_val),
                        "ANV13: 'config' has 2 fields");

   anvil_statement host_stmt = anvil_value_get_statement(config_val, 0);
   TestBit.is_not_null(host_stmt, "ANV13: config's first field retrieved");
   char name_buf[32] = {0};
   anvil_statement_get_name(host_stmt, name_buf, sizeof(name_buf));
   TestBit.is_true(0 == strcmp("host", name_buf), "ANV13: first field is named 'host'");

   char text_buf[32] = {0};
   anvil_value_get_text(anvil_statement_get_value(host_stmt), text_buf, sizeof(text_buf));
   TestBit.is_true(0 == strcmp("localhost", text_buf), "ANV13: 'host' value is 'localhost'");

   TestBit.is_null(anvil_value_get_statement(config_val, 2),
                   "ANV13: config's field index 2 is out of bounds");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV14 — a resolved VarRef is transparent: get_type/get_text report the
 * target's own kind and content, never a distinct "reference" type
 * ---------------------------------------------------------------------- */
static void test_anv14_varref_transparency_scalar(void) {
   anvil_document doc = load_accessors_doc();
   TestBit.is_not_null(doc, "ANV14: document loaded");
   if (!doc) {
      return;
   }

   anvil_statement alias = anvil_statement_get(doc, "alias_count");
   anvil_value alias_val = anvil_statement_get_value(alias);
   TestBit.is_equal_int(ANVIL_VALUE_NUMERIC, anvil_value_get_type(alias_val),
                        "ANV14: alias reports NUMERIC, not a VARREF-shaped type");
   char buf[16] = {0};
   anvil_value_get_text(alias_val, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("42", buf), "ANV14: alias text is the target's '42'");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV15 — VarRef transparency extends to a non-scalar target (OBJECT):
 * count/get_statement walk straight through to the target's own fields
 * ---------------------------------------------------------------------- */
static void test_anv15_varref_transparency_object(void) {
   anvil_document doc = load_accessors_doc();
   TestBit.is_not_null(doc, "ANV15: document loaded");
   if (!doc) {
      return;
   }

   anvil_statement alias = anvil_statement_get(doc, "alias_config");
   anvil_value alias_val = anvil_statement_get_value(alias);
   TestBit.is_equal_int(ANVIL_VALUE_OBJECT, anvil_value_get_type(alias_val),
                        "ANV15: alias reports OBJECT, transparently");
   TestBit.is_equal_int(2, (long long)anvil_value_get_count(alias_val),
                        "ANV15: alias has the target's 2 fields");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV16 — buffer-sizing convention: buf=NULL/buflen=0 returns the required
 * length without writing anything; a correctly-sized second call matches
 * ---------------------------------------------------------------------- */
static void test_anv16_buffer_sizing_convention(void) {
   anvil_document doc = load_accessors_doc();
   TestBit.is_not_null(doc, "ANV16: document loaded");
   if (!doc) {
      return;
   }

   anvil_statement word = anvil_statement_get(doc, "word");
   anvil_value word_val = anvil_statement_get_value(word);

   size_t needed = anvil_value_get_text(word_val, NULL, 0);
   TestBit.is_equal_int(8, (long long)needed, "ANV16: 'bareword' needs 8 bytes");

   char *buf = Allocator.alloc(needed + 1);
   TestBit.is_not_null(buf, "ANV16: buffer allocated");
   if (buf) {
      size_t len = anvil_value_get_text(word_val, buf, needed + 1);
      TestBit.is_equal_int((long long)needed, (long long)len,
                           "ANV16: second call returns the same length");
      TestBit.is_true(0 == strcmp("bareword", buf), "ANV16: second call fills the buffer correctly");
      Allocator.dispose(buf);
   }

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV17 — NULL safety across every Statement/Value accessor
 * ---------------------------------------------------------------------- */
static void test_anv17_accessor_null_safety(void) {
   TestBit.is_null(anvil_statement_get(NULL, "x"), "ANV17: statement_get(NULL, ...) is NULL");

   anvil_document doc = load_accessors_doc();
   TestBit.is_not_null(doc, "ANV17: document loaded (for the valid-doc/NULL-name case)");
   if (doc) {
      TestBit.is_null(anvil_statement_get(doc, NULL), "ANV17: statement_get(doc, NULL) is NULL");
      anvil_dispose(doc);
   }

   TestBit.is_null(anvil_statement_get_value(NULL), "ANV17: statement_get_value(NULL) is NULL");
   TestBit.is_equal_int(0, (long long)anvil_statement_get_name(NULL, NULL, 0),
                        "ANV17: statement_get_name(NULL, ...) is 0");
   TestBit.is_equal_int(ANVIL_VALUE_NULL, anvil_value_get_type(NULL),
                        "ANV17: value_get_type(NULL) is ANVIL_VALUE_NULL");
   TestBit.is_equal_int(0, (long long)anvil_value_get_text(NULL, NULL, 0),
                        "ANV17: value_get_text(NULL, ...) is 0");
   TestBit.is_equal_int(0, (long long)anvil_value_get_count(NULL),
                        "ANV17: value_get_count(NULL) is 0");
   TestBit.is_null(anvil_value_get_element(NULL, 0), "ANV17: value_get_element(NULL, ...) is NULL");
   TestBit.is_null(anvil_value_get_statement(NULL, 0),
                   "ANV17: value_get_statement(NULL, ...) is NULL");
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("ANV01_load_clean_document", NULL, test_anv01_load_clean_document, th);
   TestBit.run_ex("ANV02_body_syntax_error", NULL, test_anv02_body_syntax_error, th);
   TestBit.run_ex("ANV03_file_not_found", NULL, test_anv03_file_not_found, th);
   TestBit.run_ex("ANV04_import_error", NULL, test_anv04_import_error, th);
   TestBit.run_ex("ANV05_resolve_error", NULL, test_anv05_resolve_error, th);
   TestBit.run_ex("ANV06_header_error", NULL, test_anv06_header_error, th);
   TestBit.run_ex("ANV07_null_handle_safety", NULL, test_anv07_null_handle_safety, th);
   TestBit.run_ex("ANV08_get_version", NULL, test_anv08_get_version, th);
   TestBit.run_ex("ANV09_scalar_accessors", NULL, test_anv09_scalar_accessors, th);
   TestBit.run_ex("ANV10_string_escape_resolution", NULL, test_anv10_string_escape_resolution, th);
   TestBit.run_ex("ANV11_blob_accessor", NULL, test_anv11_blob_accessor, th);
   TestBit.run_ex("ANV12_array_tuple_accessors", NULL, test_anv12_array_tuple_accessors, th);
   TestBit.run_ex("ANV13_object_accessors", NULL, test_anv13_object_accessors, th);
   TestBit.run_ex("ANV14_varref_transparency_scalar", NULL, test_anv14_varref_transparency_scalar,
                  th);
   TestBit.run_ex("ANV15_varref_transparency_object", NULL, test_anv15_varref_transparency_object,
                  th);
   TestBit.run_ex("ANV16_buffer_sizing_convention", NULL, test_anv16_buffer_sizing_convention, th);
   TestBit.run_ex("ANV17_accessor_null_safety", NULL, test_anv17_accessor_null_safety, th);

   return TestBit.report();
}
