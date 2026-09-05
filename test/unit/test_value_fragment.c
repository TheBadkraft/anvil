/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_value_fragment.c - Unit tests for Value Fragment parsing          *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_value_fragment.c                                  *
 * ---------------------------------------------------------------------- *
 * Two layers, mirroring test_attribute_grammar.c's dual-level shape:     *
 * VFP* tests anvl_parse_value_fragment (src/core/parser.c) directly,      *
 * isolating the parsing mechanism from the public wrapper; VF* tests the *
 * public anvil_parse_value_fragment/anvil_document_get_fragment_value    *
 * surface (src/core/anvil_flat.c) that Node-binding-prep's gap #3 asked  *
 * for — a standalone entry point with no enclosing document/statement    *
 * context and no VarRef support at any nesting depth (there's no         *
 * identifier map here to resolve one against), matching anvil.js's own   *
 * parseRawValueCore precedent. See notes/public-api.md.                  *
 * ********************************************************************** */

#include "anvil.h"
#include "anvil_flat.h"
#include "anvil_types.h"
#include "types.h"
#include "internal/constants.h"
#include "internal/module.h"
#include "internal/parser.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "testbit.h"
#include "std.h"
// ----------------
#include "../utilities/debug.h"
#include "../utilities/helpers.h"
#include <sigma/list.h>
#include <sigma/types.h>
#include <string.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

/* A fragment has no header — just register the buffer and create an arena
 * sized off its own length, skipping doc_scan_header/mod_load_imports
 * entirely (mirrors what anvil_parse_value_fragment itself will do). */
static module_document setup_fragment_doc(const char *buffer, module_context *out_ctx) {
   module_document doc = setup_registered_doc(buffer, out_ctx);
   if (!doc) {
      return NULL;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   usize capacity = mod_ctx_arena_size_hint(strlen(buffer));
   mod_ctx_create_arena(*out_ctx, capacity, &err_code);
   return doc;
}

/* ======================================================================
 * Internal layer — VFP* (anvl_parse_value_fragment, src/core/parser.c)
 * ====================================================================== */

/* VFP01 — a bare numeric scalar parses, no terminator required */
static void test_vfp01_bare_numeric_scalar(void) {
   module_context ctx = NULL;
   module_document doc = setup_fragment_doc("42", &ctx);
   TestBit.is_not_null(doc, "VFP01: fragment document ready");
   if (!doc) {
      return;
   }
   anvl_value value = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, anvl_parse_value_fragment(doc, &value, &err_code),
                        "VFP01: fragment parses");
   TestBit.is_not_null(value, "VFP01: value returned");
   if (value) {
      TestBit.is_equal_int(ANVL_VALUE_NUMERIC, (long long)value->type, "VFP01: value is NUMERIC");
      TestBit.is_true(slice_equals(value->text, "42"), "VFP01: text is '42'");
   }
   mod_ctx_dispose(ctx);
}
/* VFP02 — a trailing ';' is tolerated, not required */
static void test_vfp02_trailing_semicolon_tolerated(void) {
   module_context ctx = NULL;
   module_document doc = setup_fragment_doc("42;", &ctx);
   TestBit.is_not_null(doc, "VFP02: fragment document ready");
   if (!doc) {
      return;
   }
   anvl_value value = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, anvl_parse_value_fragment(doc, &value, &err_code),
                        "VFP02: fragment with trailing ';' parses");
   TestBit.is_not_null(value, "VFP02: value returned");
   mod_ctx_dispose(ctx);
}
/* VFP03 — trailing content after the value (beyond an optional ';') is a hard error */
static void test_vfp03_trailing_content_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_fragment_doc("42 extra", &ctx);
   TestBit.is_not_null(doc, "VFP03: fragment document ready");
   if (!doc) {
      return;
   }
   anvl_value value = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, anvl_parse_value_fragment(doc, &value, &err_code),
                        "VFP03: trailing content rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "VFP03: err_code is UNEXPECTED_TOKEN");
   mod_ctx_dispose(ctx);
}
/* VFP04 — a top-level '$var' is rejected: no identifier map exists to resolve it against */
static void test_vfp04_top_level_varref_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_fragment_doc("$var", &ctx);
   TestBit.is_not_null(doc, "VFP04: fragment document ready");
   if (!doc) {
      return;
   }
   anvl_value value = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, anvl_parse_value_fragment(doc, &value, &err_code),
                        "VFP04: '$var' rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_FRAGMENT, err_code,
                        "VFP04: err_code is VARREF_NOT_ALLOWED_IN_FRAGMENT");
   mod_ctx_dispose(ctx);
}
/* VFP05 — a '$var' nested inside an array is rejected too: the flat ctx->values index
 * reaches every value node regardless of nesting depth, no tree walk needed here. */
static void test_vfp05_nested_varref_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_fragment_doc("[1, $var, 3]", &ctx);
   TestBit.is_not_null(doc, "VFP05: fragment document ready");
   if (!doc) {
      return;
   }
   anvl_value value = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, anvl_parse_value_fragment(doc, &value, &err_code),
                        "VFP05: nested '$var' rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_FRAGMENT, err_code,
                        "VFP05: err_code is VARREF_NOT_ALLOWED_IN_FRAGMENT");
   mod_ctx_dispose(ctx);
}
/* VFP06 — an empty (whitespace-only) fragment is not a value */
static void test_vfp06_empty_fragment_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_fragment_doc("   ", &ctx);
   TestBit.is_not_null(doc, "VFP06: fragment document ready");
   if (!doc) {
      return;
   }
   anvl_value value = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, anvl_parse_value_fragment(doc, &value, &err_code),
                        "VFP06: empty fragment rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_VALUE, err_code,
                        "VFP06: err_code is EXPECTED_VALUE");
   mod_ctx_dispose(ctx);
}

/* ======================================================================
 * Public layer — VF* (anvil_parse_value_fragment / anvil_document_get_-
 * fragment_value, src/core/anvil_flat.c)
 * ====================================================================== */

/* VF01 — numeric scalar */
static void test_vf01_numeric_scalar(void) {
   anvil_document doc = anvil_parse_value_fragment("42", 2);
   TestBit.is_not_null(doc, "VF01: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "VF01: no errors");
   anvil_value value = anvil_document_get_fragment_value(doc);
   TestBit.is_not_null(value, "VF01: fragment value present");
   if (value) {
      TestBit.is_equal_int(ANVIL_VALUE_NUMERIC, anvil_value_get_type(value),
                           "VF01: value is ANVIL_VALUE_NUMERIC");
      char buf[16] = {0};
      size_t len = anvil_value_get_text(value, buf, sizeof(buf));
      TestBit.is_equal_int(2, (long long)len, "VF01: text length is 2");
      TestBit.is_true(strcmp(buf, "42") == 0, "VF01: text is '42'");
   }
   anvil_dispose(doc);
}
/* VF02 — string scalar, escapes resolved same as any other STRING value */
static void test_vf02_string_scalar(void) {
   const char *src = "\"hello\\nworld\"";
   anvil_document doc = anvil_parse_value_fragment(src, strlen(src));
   TestBit.is_not_null(doc, "VF02: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "VF02: no errors");
   anvil_value value = anvil_document_get_fragment_value(doc);
   TestBit.is_not_null(value, "VF02: fragment value present");
   if (value) {
      TestBit.is_equal_int(ANVIL_VALUE_STRING, anvil_value_get_type(value),
                           "VF02: value is ANVIL_VALUE_STRING");
      char buf[32] = {0};
      anvil_value_get_text(value, buf, sizeof(buf));
      TestBit.is_true(strcmp(buf, "hello\nworld") == 0, "VF02: escape resolved to real newline");
   }
   anvil_dispose(doc);
}
/* VF03 — bool scalar */
static void test_vf03_bool_scalar(void) {
   anvil_document doc = anvil_parse_value_fragment("true", 4);
   TestBit.is_not_null(doc, "VF03: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "VF03: no errors");
   anvil_value value = anvil_document_get_fragment_value(doc);
   TestBit.is_not_null(value, "VF03: fragment value present");
   if (value) {
      TestBit.is_equal_int(ANVIL_VALUE_BOOL, anvil_value_get_type(value), "VF03: value is ANVIL_VALUE_BOOL");
   }
   anvil_dispose(doc);
}
/* VF04 — null scalar */
static void test_vf04_null_scalar(void) {
   anvil_document doc = anvil_parse_value_fragment("null", 4);
   TestBit.is_not_null(doc, "VF04: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "VF04: no errors");
   anvil_value value = anvil_document_get_fragment_value(doc);
   TestBit.is_not_null(value, "VF04: fragment value present");
   if (value) {
      TestBit.is_equal_int(ANVIL_VALUE_NULL, anvil_value_get_type(value), "VF04: value is ANVIL_VALUE_NULL");
   }
   anvil_dispose(doc);
}
/* VF05 — array of numeric scalars */
static void test_vf05_array(void) {
   const char *src = "[1,2,3]";
   anvil_document doc = anvil_parse_value_fragment(src, strlen(src));
   TestBit.is_not_null(doc, "VF05: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "VF05: no errors");
   anvil_value value = anvil_document_get_fragment_value(doc);
   TestBit.is_not_null(value, "VF05: fragment value present");
   if (value) {
      TestBit.is_equal_int(ANVIL_VALUE_ARRAY, anvil_value_get_type(value), "VF05: value is ANVIL_VALUE_ARRAY");
      TestBit.is_equal_int(3, (long long)anvil_value_get_count(value), "VF05: three elements");
      anvil_value elem0 = anvil_value_get_element(value, 0);
      TestBit.is_not_null(elem0, "VF05: element 0 retrieved");
      if (elem0) {
         char buf[8] = {0};
         anvil_value_get_text(elem0, buf, sizeof(buf));
         TestBit.is_true(strcmp(buf, "1") == 0, "VF05: element 0 text is '1'");
      }
   }
   anvil_dispose(doc);
}
/* VF06 — tuple */
static void test_vf06_tuple(void) {
   const char *src = "(1,2)";
   anvil_document doc = anvil_parse_value_fragment(src, strlen(src));
   TestBit.is_not_null(doc, "VF06: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "VF06: no errors");
   anvil_value value = anvil_document_get_fragment_value(doc);
   TestBit.is_not_null(value, "VF06: fragment value present");
   if (value) {
      TestBit.is_equal_int(ANVIL_VALUE_TUPLE, anvil_value_get_type(value), "VF06: value is ANVIL_VALUE_TUPLE");
      TestBit.is_equal_int(2, (long long)anvil_value_get_count(value), "VF06: two elements");
   }
   anvil_dispose(doc);
}
/* VF07 — object, enumerated via anvil_value_get_statement same as any other OBJECT value */
static void test_vf07_object(void) {
   const char *src = "{ a := 1; }";
   anvil_document doc = anvil_parse_value_fragment(src, strlen(src));
   TestBit.is_not_null(doc, "VF07: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "VF07: no errors");
   anvil_value value = anvil_document_get_fragment_value(doc);
   TestBit.is_not_null(value, "VF07: fragment value present");
   if (value) {
      TestBit.is_equal_int(ANVIL_VALUE_OBJECT, anvil_value_get_type(value),
                           "VF07: value is ANVIL_VALUE_OBJECT");
      TestBit.is_equal_int(1, (long long)anvil_value_get_count(value), "VF07: one field");
      anvil_statement field = anvil_value_get_statement(value, 0);
      TestBit.is_not_null(field, "VF07: field statement retrieved");
      if (field) {
         char name[8] = {0};
         anvil_statement_get_name(field, name, sizeof(name));
         TestBit.is_true(strcmp(name, "a") == 0, "VF07: field name is 'a'");
      }
   }
   anvil_dispose(doc);
}
/* VF08 — surrounding whitespace is tolerated */
static void test_vf08_surrounding_whitespace(void) {
   anvil_document doc = anvil_parse_value_fragment("  42  ", 6);
   TestBit.is_not_null(doc, "VF08: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "VF08: no errors");
   anvil_value value = anvil_document_get_fragment_value(doc);
   TestBit.is_not_null(value, "VF08: fragment value present");
   anvil_dispose(doc);
}
/* VF09 — trailing content after the value is a hard error, surfaced as ANVIL_ERR_SYNTAX */
static void test_vf09_trailing_content_rejected(void) {
   anvil_document doc = anvil_parse_value_fragment("42 extra", 8);
   TestBit.is_not_null(doc, "VF09: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "VF09: has errors");
   TestBit.is_equal_int(ANVIL_ERR_SYNTAX, anvil_get_error(doc), "VF09: error category is SYNTAX");
   TestBit.is_null(anvil_document_get_fragment_value(doc), "VF09: no fragment value on failure");
   anvil_dispose(doc);
}
/* VF10 — a top-level VarRef is rejected via the public surface too */
static void test_vf10_top_level_varref_rejected(void) {
   anvil_document doc = anvil_parse_value_fragment("$var", 4);
   TestBit.is_not_null(doc, "VF10: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "VF10: has errors");
   TestBit.is_equal_int(ANVIL_ERR_SYNTAX, anvil_get_error(doc), "VF10: error category is SYNTAX");
   TestBit.is_null(anvil_document_get_fragment_value(doc), "VF10: no fragment value on failure");
   anvil_dispose(doc);
}
/* VF11 — a nested VarRef is rejected via the public surface too */
static void test_vf11_nested_varref_rejected(void) {
   const char *src = "[1, $var]";
   anvil_document doc = anvil_parse_value_fragment(src, strlen(src));
   TestBit.is_not_null(doc, "VF11: document handle allocated");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "VF11: has errors");
   TestBit.is_equal_int(ANVIL_ERR_SYNTAX, anvil_get_error(doc), "VF11: error category is SYNTAX");
   anvil_dispose(doc);
}
/* VF12 — anvil_document_get_fragment_value on an ordinary anvil_load'd document (not a
 * fragment) is safely NULL, not a crash */
static void test_vf12_non_fragment_document_returns_null(void) {
   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "VF12: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "VF12: no errors");
   TestBit.is_null(anvil_document_get_fragment_value(doc),
                   "VF12: ordinary document has no fragment value");
   anvil_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("VFP01_bare_numeric_scalar", NULL, test_vfp01_bare_numeric_scalar, th);
   TestBit.run_ex("VFP02_trailing_semicolon_tolerated", NULL,
                  test_vfp02_trailing_semicolon_tolerated, th);
   TestBit.run_ex("VFP03_trailing_content_rejected", NULL, test_vfp03_trailing_content_rejected, th);
   TestBit.run_ex("VFP04_top_level_varref_rejected", NULL, test_vfp04_top_level_varref_rejected, th);
   TestBit.run_ex("VFP05_nested_varref_rejected", NULL, test_vfp05_nested_varref_rejected, th);
   TestBit.run_ex("VFP06_empty_fragment_rejected", NULL, test_vfp06_empty_fragment_rejected, th);

   TestBit.run_ex("VF01_numeric_scalar", NULL, test_vf01_numeric_scalar, th);
   TestBit.run_ex("VF02_string_scalar", NULL, test_vf02_string_scalar, th);
   TestBit.run_ex("VF03_bool_scalar", NULL, test_vf03_bool_scalar, th);
   TestBit.run_ex("VF04_null_scalar", NULL, test_vf04_null_scalar, th);
   TestBit.run_ex("VF05_array", NULL, test_vf05_array, th);
   TestBit.run_ex("VF06_tuple", NULL, test_vf06_tuple, th);
   TestBit.run_ex("VF07_object", NULL, test_vf07_object, th);
   TestBit.run_ex("VF08_surrounding_whitespace", NULL, test_vf08_surrounding_whitespace, th);
   TestBit.run_ex("VF09_trailing_content_rejected", NULL, test_vf09_trailing_content_rejected, th);
   TestBit.run_ex("VF10_top_level_varref_rejected", NULL, test_vf10_top_level_varref_rejected, th);
   TestBit.run_ex("VF11_nested_varref_rejected", NULL, test_vf11_nested_varref_rejected, th);
   TestBit.run_ex("VF12_non_fragment_document_returns_null", NULL,
                  test_vf12_non_fragment_document_returns_null, th);

   return TestBit.report();
}
