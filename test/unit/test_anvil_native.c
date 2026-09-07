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
 * ANV18 — anvil_load_buffer parses a clean in-memory document identically
 * to a file load
 * ---------------------------------------------------------------------- */
static void test_anv18_load_buffer_clean(void) {
   const char *src = "#!aml\n\nname := David;\n";
   anvil_document doc = anvil_load_buffer(src, strlen(src));
   TestBit.is_not_null(doc, "ANV18: document loaded from a buffer");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "ANV18: no errors");

   anvil_statement name = anvil_statement_get(doc, "name");
   TestBit.is_not_null(name, "ANV18: 'name' statement found");
   anvil_value name_val = anvil_statement_get_value(name);
   TestBit.is_equal_int(ANVIL_VALUE_IDENTIFIER, anvil_value_get_type(name_val),
                        "ANV18: 'name' is ANVIL_VALUE_IDENTIFIER");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV19 — anvil_load_buffer reports ANVIL_ERR_SYNTAX for a body syntax
 * error, same categorization as the file-based path
 * ---------------------------------------------------------------------- */
static void test_anv19_load_buffer_syntax_error(void) {
   const char *src = "#!aml\n\nlabel {\n   field := 1;\n";
   anvil_document doc = anvil_load_buffer(src, strlen(src));
   TestBit.is_not_null(doc, "ANV19: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV19: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_SYNTAX, anvil_get_error(doc),
                        "ANV19: error category is ANVIL_ERR_SYNTAX");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV20 — anvil_load_buffer(NULL, 0) is safe and reports an error, not a
 * crash
 * ---------------------------------------------------------------------- */
static void test_anv20_load_buffer_null_source(void) {
   anvil_document doc = anvil_load_buffer(NULL, 0);
   TestBit.is_not_null(doc, "ANV20: document handle still returned for a NULL buffer");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV20: errors recorded");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV21 — anvil_load_buffer honors `length` exactly, not a NUL terminator
 * ---------------------------------------------------------------------- */
static void test_anv21_load_buffer_respects_length(void) {
   const char *src = "#!aml\n\nname := David;\nJUNK JUNK JUNK THIS IS NOT VALID {{{";
   size_t valid_len = strlen("#!aml\n\nname := David;\n");
   anvil_document doc = anvil_load_buffer(src, valid_len);
   TestBit.is_not_null(doc, "ANV21: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc),
                    "ANV21: no errors — the trailing junk past `length` was never read");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV22 — module-level attribute enumeration (anvil_doc_attributes.anvl:
 * @[doc_level] @[is_mixed=true] @[has_var_refs] — 3 attributes, first a
 * flag, second key=value, across 3 separate @[...] blocks)
 * ---------------------------------------------------------------------- */
static void test_anv22_document_attribute_enumeration(void) {
   anvil_document doc = anvil_load(fixture_path("anvil_doc_attributes.anvl"));
   TestBit.is_not_null(doc, "ANV22: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "ANV22: no errors");
   TestBit.is_equal_int(3, (long long)anvil_document_get_attribute_count(doc),
                        "ANV22: 3 module-level attributes");

   anvil_attribute first = anvil_document_get_attribute(doc, 0);
   TestBit.is_not_null(first, "ANV22: first attribute retrieved");
   char buf[64] = {0};
   anvil_attribute_get_key(first, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("doc_level", buf), "ANV22: first attribute key is 'doc_level'");
   TestBit.is_equal_int(0, (long long)anvil_attribute_get_value(first, buf, sizeof(buf)),
                        "ANV22: first attribute is a flag (no value)");

   anvil_attribute second = anvil_document_get_attribute(doc, 1);
   TestBit.is_not_null(second, "ANV22: second attribute retrieved");
   anvil_attribute_get_key(second, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("is_mixed", buf), "ANV22: second attribute key is 'is_mixed'");
   anvil_attribute_get_value(second, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("true", buf), "ANV22: second attribute value is 'true'");

   TestBit.is_null(anvil_document_get_attribute(doc, 3),
                   "ANV22: index 3 is out of bounds (only 3 attributes, 0-2)");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV23 — module-level attribute lookup by key
 * ---------------------------------------------------------------------- */
static void test_anv23_document_find_attribute(void) {
   anvil_document doc = anvil_load(fixture_path("anvil_doc_attributes.anvl"));
   TestBit.is_not_null(doc, "ANV23: document loaded");
   if (!doc) {
      return;
   }

   anvil_attribute found = anvil_document_find_attribute(doc, "is_mixed");
   TestBit.is_not_null(found, "ANV23: 'is_mixed' found by key");
   char buf[64] = {0};
   anvil_attribute_get_value(found, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("true", buf), "ANV23: 'is_mixed' value is 'true'");

   TestBit.is_null(anvil_document_find_attribute(doc, "nonexistent"),
                   "ANV23: a nonexistent key is not found");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV24 — a document with no module-level attributes reports 0
 * ---------------------------------------------------------------------- */
static void test_anv24_document_no_attributes(void) {
   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "ANV24: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_equal_int(0, (long long)anvil_document_get_attribute_count(doc),
                        "ANV24: 0 module-level attributes");
   TestBit.is_null(anvil_document_find_attribute(doc, "anything"),
                   "ANV24: find_attribute on an attribute-less document is NULL");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV25 — statement-level attribute enumeration and lookup
 * (body_assign_attrs.anvl: server @[active, env=production] := "prod-1";)
 * ---------------------------------------------------------------------- */
static void test_anv25_statement_attribute_enumeration(void) {
   anvil_document doc = anvil_load(fixture_path("body_assign_attrs.anvl"));
   TestBit.is_not_null(doc, "ANV25: document loaded");
   if (!doc) {
      return;
   }

   anvil_statement server = anvil_statement_get(doc, "server");
   TestBit.is_not_null(server, "ANV25: 'server' statement found");
   TestBit.is_equal_int(2, (long long)anvil_statement_get_attribute_count(server),
                        "ANV25: 2 statement-level attributes");

   anvil_attribute first = anvil_statement_get_attribute(server, 0);
   char buf[64] = {0};
   anvil_attribute_get_key(first, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("active", buf), "ANV25: first attribute key is 'active'");
   TestBit.is_equal_int(0, (long long)anvil_attribute_get_value(first, buf, sizeof(buf)),
                        "ANV25: 'active' is a flag (no value)");

   anvil_attribute env = anvil_statement_find_attribute(server, "env");
   TestBit.is_not_null(env, "ANV25: 'env' found by key");
   anvil_attribute_get_value(env, buf, sizeof(buf));
   TestBit.is_true(0 == strcmp("production", buf), "ANV25: 'env' value is 'production'");

   TestBit.is_null(anvil_statement_find_attribute(server, "nonexistent"),
                   "ANV25: a nonexistent key is not found");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV26 — NULL safety across every attribute accessor
 * ---------------------------------------------------------------------- */
static void test_anv26_attribute_null_safety(void) {
   TestBit.is_equal_int(0, (long long)anvil_document_get_attribute_count(NULL),
                        "ANV26: document_get_attribute_count(NULL) is 0");
   TestBit.is_null(anvil_document_get_attribute(NULL, 0),
                   "ANV26: document_get_attribute(NULL, ...) is NULL");
   TestBit.is_null(anvil_document_find_attribute(NULL, "x"),
                   "ANV26: document_find_attribute(NULL, ...) is NULL");
   TestBit.is_equal_int(0, (long long)anvil_statement_get_attribute_count(NULL),
                        "ANV26: statement_get_attribute_count(NULL) is 0");
   TestBit.is_null(anvil_statement_get_attribute(NULL, 0),
                   "ANV26: statement_get_attribute(NULL, ...) is NULL");
   TestBit.is_null(anvil_statement_find_attribute(NULL, "x"),
                   "ANV26: statement_find_attribute(NULL, ...) is NULL");
   TestBit.is_equal_int(0, (long long)anvil_attribute_get_key(NULL, NULL, 0),
                        "ANV26: attribute_get_key(NULL, ...) is 0");
   TestBit.is_equal_int(0, (long long)anvil_attribute_get_value(NULL, NULL, 0),
                        "ANV26: attribute_get_value(NULL, ...) is 0");
}
/* ---------------------------------------------------------------------- *
 * ANV27 — a document's top-level statements are visitable, in order, via
 * a heapless iterator (f01_bare_literal.anvl: 'val' then 'name')
 * ---------------------------------------------------------------------- */
static void test_anv27_document_statement_iteration(void) {
   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "ANV27: document loaded");
   if (!doc) {
      return;
   }
   anvil_statement_iterator it = anvil_document_get_statements(doc);
   TestBit.is_not_null(it, "ANV27: iterator created");
   if (it) {
      anvil_statement stmt = NULL;

      TestBit.is_true(anvil_statement_iterator_next(it, &stmt), "ANV27: first next succeeds");
      if (stmt) {
         char name[8] = {0};
         anvil_statement_get_name(stmt, name, sizeof(name));
         TestBit.is_true(strcmp(name, "val") == 0, "ANV27: first statement is 'val'");
      }

      stmt = NULL;
      TestBit.is_true(anvil_statement_iterator_next(it, &stmt), "ANV27: second next succeeds");
      if (stmt) {
         char name[8] = {0};
         anvil_statement_get_name(stmt, name, sizeof(name));
         TestBit.is_true(strcmp(name, "name") == 0, "ANV27: second statement is 'name'");
      }

      TestBit.is_false(anvil_statement_iterator_next(it, &stmt),
                       "ANV27: third next exhausts the iterator");

      anvil_statement_iterator_dispose(it);
   }
   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV28 — statement iteration is safe on NULL/failed documents, and
 * dispose(NULL) is a no-op
 * ---------------------------------------------------------------------- */
static void test_anv28_statement_iteration_null_safety(void) {
   TestBit.is_null(anvil_document_get_statements(NULL), "ANV28: get_statements(NULL) is NULL");
   TestBit.is_false(anvil_statement_iterator_next(NULL, NULL),
                    "ANV28: iterator_next(NULL, NULL) is false");
   anvil_statement_iterator_dispose(NULL); // must not crash

   // doc->body is frozen on both success AND failure (partial results stay inspectable —
   // see source_finish_body/parse_source's own design) — so a failed parse still gets a real,
   // usable iterator here, just one that immediately exhausts if nothing was captured before
   // the failure, matching that same "partial results, not silently dropped" philosophy rather
   // than reporting NULL for an entirely different reason (NULL is reserved for "no document" /
   // "never reached a successful body parse at all", e.g. a document that never got as far as
   // doc_parse_body).
   anvil_document failed = anvil_load(fixture_path("body_err_unterminated_block.anvl"));
   TestBit.is_not_null(failed, "ANV28: failed-parse document handle still returned");
   if (failed) {
      anvil_statement_iterator it = anvil_document_get_statements(failed);
      TestBit.is_not_null(it, "ANV28: a failed parse still yields a usable (empty) iterator");
      if (it) {
         anvil_statement stmt = NULL;
         TestBit.is_false(anvil_statement_iterator_next(it, &stmt),
                          "ANV28: the failed document's body captured no statements");
         anvil_statement_iterator_dispose(it);
      }
      anvil_dispose(failed);
   }
}
/* ---------------------------------------------------------------------- *
 * ANV29 — a syntax error's diagnostic detail carries a real message and a
 * real (non-zero) source position, beyond the stable ANVIL_ERR_SYNTAX
 * category alone
 * ---------------------------------------------------------------------- */
static void test_anv29_error_detail_syntax(void) {
   anvil_document doc = anvil_load(fixture_path("body_err_unterminated_block.anvl"));
   TestBit.is_not_null(doc, "ANV29: document handle returned despite the error");
   if (!doc) {
      return;
   }
   anvil_error err = anvil_document_get_error(doc);
   TestBit.is_not_null(err, "ANV29: error detail present");
   if (err) {
      TestBit.is_equal_int(ANVIL_ERR_SYNTAX, anvil_error_get_category(err),
                           "ANV29: error category matches anvil_get_error(doc)");
      char msg[128] = {0};
      size_t len = anvil_error_get_message(err, msg, sizeof(msg));
      TestBit.is_true(len > 0, "ANV29: message is non-empty");
      TestBit.is_true(anvil_error_get_line(err) > 0, "ANV29: line is a real (non-zero) position");
      TestBit.is_true(anvil_error_get_column(err) > 0,
                      "ANV29: column is a real (non-zero) position");
   }
   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV30 — an I/O failure has no meaningful source position: the error
 * detail is either absent, or reports 0/0 rather than a stale/bogus value
 * ---------------------------------------------------------------------- */
static void test_anv30_error_detail_io_has_no_position(void) {
   anvil_document doc = anvil_load(fixture_path("anvil_nonexistent_file_xyz.anvl"));
   TestBit.is_not_null(doc, "ANV30: document handle returned despite the error");
   if (!doc) {
      return;
   }
   anvil_error err = anvil_document_get_error(doc);
   if (err) {
      TestBit.is_equal_int(0, (long long)anvil_error_get_line(err),
                           "ANV30: I/O failure has no meaningful line");
      TestBit.is_equal_int(0, (long long)anvil_error_get_column(err),
                           "ANV30: I/O failure has no meaningful column");
   } else {
      TestBit.is_true(true, "ANV30: no error detail at all is also an acceptable outcome for I/O");
   }
   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV31 — a clean document has no error detail; NULL handles are safe
 * ---------------------------------------------------------------------- */
static void test_anv31_error_detail_null_safety(void) {
   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "ANV31: document loaded");
   if (doc) {
      TestBit.is_null(anvil_document_get_error(doc), "ANV31: clean document has no error detail");
      anvil_dispose(doc);
   }

   TestBit.is_null(anvil_document_get_error(NULL), "ANV31: get_error(NULL) is NULL");
   TestBit.is_equal_int(ANVIL_OK, anvil_error_get_category(NULL),
                        "ANV31: get_category(NULL) is ANVIL_OK");
   TestBit.is_equal_int(0, (long long)anvil_error_get_message(NULL, NULL, 0),
                        "ANV31: get_message(NULL, ...) is 0");
   TestBit.is_equal_int(0, (long long)anvil_error_get_line(NULL), "ANV31: get_line(NULL) is 0");
   TestBit.is_equal_int(0, (long long)anvil_error_get_column(NULL), "ANV31: get_column(NULL) is 0");
}

/* ---------------------------------------------------------------------- *
 * ANV32 — a document's direct imports are visitable via a heapless
 * iterator (f13_import.anvl imports f01_bare_literal.anvl), and the
 * yielded handle is a fully usable anvil_document in its own right
 * ---------------------------------------------------------------------- */
static void test_anv32_document_import_iteration(void) {
   anvil_document doc = anvil_load(fixture_path("f13_import.anvl"));
   TestBit.is_not_null(doc, "ANV32: document loaded");
   if (!doc) {
      return;
   }

   anvil_document_iterator it = anvil_document_get_imports(doc);
   TestBit.is_not_null(it, "ANV32: import iterator created");
   if (it) {
      anvil_document imported = NULL;
      TestBit.is_true(anvil_document_iterator_next(it, &imported), "ANV32: first next succeeds");
      if (imported) {
         anvil_statement_iterator sit = anvil_document_get_statements(imported);
         TestBit.is_not_null(sit, "ANV32: imported document's own statements are visitable");
         if (sit) {
            anvil_statement stmt = NULL;
            TestBit.is_true(anvil_statement_iterator_next(sit, &stmt),
                            "ANV32: imported doc's first statement");
            if (stmt) {
               char name[8] = {0};
               anvil_statement_get_name(stmt, name, sizeof(name));
               TestBit.is_true(strcmp(name, "val") == 0,
                               "ANV32: imported doc's first statement is 'val'");
            }
            stmt = NULL;
            TestBit.is_true(anvil_statement_iterator_next(sit, &stmt),
                            "ANV32: imported doc's second statement");
            if (stmt) {
               char name[8] = {0};
               anvil_statement_get_name(stmt, name, sizeof(name));
               TestBit.is_true(strcmp(name, "name") == 0,
                               "ANV32: imported doc's second statement is 'name'");
            }
            anvil_statement_iterator_dispose(sit);
         }
         anvil_dispose(imported); // caller's own responsibility — safe, shares owner's context
      }

      imported = NULL;
      TestBit.is_false(anvil_document_iterator_next(it, &imported),
                       "ANV32: second next exhausts the iterator (one direct import)");

      anvil_document_iterator_dispose(it);
   }

   // The importing document's own top-level statements are unaffected — still just 'alias'.
   anvil_statement_iterator root_it = anvil_document_get_statements(doc);
   TestBit.is_not_null(root_it, "ANV32: root document's own statements still visitable");
   if (root_it) {
      anvil_statement stmt = NULL;
      TestBit.is_true(anvil_statement_iterator_next(root_it, &stmt), "ANV32: root's first statement");
      if (stmt) {
         char name[8] = {0};
         anvil_statement_get_name(stmt, name, sizeof(name));
         TestBit.is_true(strcmp(name, "alias") == 0, "ANV32: root's own statement is 'alias'");
      }
      anvil_statement_iterator_dispose(root_it);
   }

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV33 — import iteration is safe on NULL, a document with zero direct
 * imports still yields a real (immediately-exhausted) iterator, and
 * disposing an imported-document handle is a harmless no-op
 * ---------------------------------------------------------------------- */
static void test_anv33_import_iteration_null_safety(void) {
   TestBit.is_null(anvil_document_get_imports(NULL), "ANV33: get_imports(NULL) is NULL");
   TestBit.is_false(anvil_document_iterator_next(NULL, NULL),
                    "ANV33: iterator_next(NULL, NULL) is false");
   anvil_document_iterator_dispose(NULL); // must not crash

   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl")); // has zero imports
   TestBit.is_not_null(doc, "ANV33: document with no imports loaded");
   if (doc) {
      anvil_document_iterator it = anvil_document_get_imports(doc);
      TestBit.is_not_null(it, "ANV33: a document with zero imports still yields a usable iterator");
      if (it) {
         anvil_document imported = NULL;
         TestBit.is_false(anvil_document_iterator_next(it, &imported),
                          "ANV33: zero direct imports means immediate exhaustion");
         anvil_document_iterator_dispose(it);
      }
      anvil_dispose(doc);
   }

   // Disposing a yielded imported-document handle directly must be a harmless no-op — it must
   // never tear down the shared context the owning document (still in use below) depends on.
   anvil_document owner = anvil_load(fixture_path("f13_import.anvl"));
   TestBit.is_not_null(owner, "ANV33: owning document loaded");
   if (owner) {
      anvil_document_iterator it = anvil_document_get_imports(owner);
      if (it) {
         anvil_document imported = NULL;
         if (anvil_document_iterator_next(it, &imported) && imported) {
            anvil_dispose(imported); // must not corrupt owner's shared context
         }
         anvil_document_iterator_dispose(it);
      }
      TestBit.is_false(anvil_has_errors(owner),
                       "ANV33: owner's context survives disposing an imported handle directly");
      anvil_dispose(owner);
   }
}

/* ---------------------------------------------------------------------- *
 * ANV34 — anvil_statement_get_value now works for a bare OBJECT_BLOCK
 * statement ('label { ... };'), not just ASSIGN-with-object-value
 * ('label := { ... };') — the two forms are indistinguishable through
 * this accessor, matching their already-equal internal treatment
 * everywhere else (base/attributes/inheritance)
 * ---------------------------------------------------------------------- */
static void test_anv34_object_block_get_value(void) {
   anvil_document doc = anvil_load(fixture_path("body_block_namespace.anvl"));
   TestBit.is_not_null(doc, "ANV34: document loaded");
   if (!doc) {
      return;
   }

   anvil_statement label = anvil_statement_get(doc, "label");
   TestBit.is_not_null(label, "ANV34: 'label' statement found");
   if (label) {
      anvil_value val = anvil_statement_get_value(label);
      TestBit.is_not_null(val, "ANV34: OBJECT_BLOCK statement now has a value");
      if (val) {
         TestBit.is_equal_int(ANVIL_VALUE_OBJECT, anvil_value_get_type(val),
                              "ANV34: value kind is OBJECT");
         TestBit.is_equal_int(2, (long long)anvil_value_get_count(val),
                              "ANV34: two nested fields");

         anvil_statement field0 = anvil_value_get_statement(val, 0);
         TestBit.is_not_null(field0, "ANV34: first nested field retrieved");
         if (field0) {
            char name[8] = {0};
            anvil_statement_get_name(field0, name, sizeof(name));
            TestBit.is_true(strcmp(name, "field") == 0, "ANV34: first field is 'field'");
         }

         anvil_statement field1 = anvil_value_get_statement(val, 1);
         TestBit.is_not_null(field1, "ANV34: second nested field retrieved");
         if (field1) {
            char name[8] = {0};
            anvil_statement_get_name(field1, name, sizeof(name));
            TestBit.is_true(strcmp(name, "other") == 0, "ANV34: second field is 'other'");
         }
      }
   }

   anvil_dispose(doc);
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
   TestBit.run_ex("ANV18_load_buffer_clean", NULL, test_anv18_load_buffer_clean, th);
   TestBit.run_ex("ANV19_load_buffer_syntax_error", NULL, test_anv19_load_buffer_syntax_error, th);
   TestBit.run_ex("ANV20_load_buffer_null_source", NULL, test_anv20_load_buffer_null_source, th);
   TestBit.run_ex("ANV21_load_buffer_respects_length", NULL, test_anv21_load_buffer_respects_length,
                  th);
   TestBit.run_ex("ANV22_document_attribute_enumeration", NULL,
                  test_anv22_document_attribute_enumeration, th);
   TestBit.run_ex("ANV23_document_find_attribute", NULL, test_anv23_document_find_attribute, th);
   TestBit.run_ex("ANV24_document_no_attributes", NULL, test_anv24_document_no_attributes, th);
   TestBit.run_ex("ANV25_statement_attribute_enumeration", NULL,
                  test_anv25_statement_attribute_enumeration, th);
   TestBit.run_ex("ANV26_attribute_null_safety", NULL, test_anv26_attribute_null_safety, th);
   TestBit.run_ex("ANV27_document_statement_iteration", NULL,
                  test_anv27_document_statement_iteration, th);
   TestBit.run_ex("ANV28_statement_iteration_null_safety", NULL,
                  test_anv28_statement_iteration_null_safety, th);
   TestBit.run_ex("ANV29_error_detail_syntax", NULL, test_anv29_error_detail_syntax, th);
   TestBit.run_ex("ANV30_error_detail_io_has_no_position", NULL,
                  test_anv30_error_detail_io_has_no_position, th);
   TestBit.run_ex("ANV31_error_detail_null_safety", NULL, test_anv31_error_detail_null_safety, th);
   TestBit.run_ex("ANV32_document_import_iteration", NULL, test_anv32_document_import_iteration, th);
   TestBit.run_ex("ANV33_import_iteration_null_safety", NULL,
                  test_anv33_import_iteration_null_safety, th);
   TestBit.run_ex("ANV34_object_block_get_value", NULL, test_anv34_object_block_get_value, th);

   return TestBit.report();
}
