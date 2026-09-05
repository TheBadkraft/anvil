/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_attribute_grammar.c - Invariant tests for attribute key/value      *
 * character-set acceptance, at both module- and statement-level          *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_attribute_grammar.c                               *
 * ---------------------------------------------------------------------- *
 * Characterizes, rather than changes, current behavior: header_scan_-    *
 * attributes (src/core/document.c, module-level '@[...]') and            *
 * parse_attribute_list (src/core/parser.c, statement-level '@[...]')     *
 * both scan attribute KEYS with the strict Source.is_identifier_start/   *
 * is_identifier_part pair (alpha/digit/underscore, no leading digit) —   *
 * confirmed byte-for-byte identical between the two call sites. Attribute*
 * VALUES, by contrast, are scanned permissively: any byte up to an       *
 * unquoted ',' or ']'. Every ATG_M* (module) case has a matching ATG_S*  *
 * (statement) case so the two levels can be compared side by side. No    *
 * grammar change is made here — see notes/public-api.md's open question  *
 * on whether attribute keys should stay this strict, loosen towards      *
 * is_bare_literal_part, or move the other way.                          *
 * ********************************************************************** */

#include "anvil.h"
#include "types.h"
#include "internal/constants.h"
#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "testbit.h"
#include "std.h"
// ----------------
#include "../utilities/debug.h"
#include "../utilities/helpers.h"
#include <sigma/list.h>
#include <sigma/types.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

/* Loads a buffer through header-scan + import-loading + arena creation,
 * ready for doc_parse_body — the buffer-based counterpart to
 * test_body_aml.c's setup_aml_doc, needed here since every statement-level
 * case is a short inline buffer rather than a fixture file. */
static module_document setup_attr_stmt_doc(const char *buffer, module_context *out_ctx) {
   module_document doc = setup_registered_doc(buffer, out_ctx);
   if (!doc) {
      return NULL;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   if (ANVL_RES_OK != doc_scan_header(doc, &err_code)) {
      return doc;
   }
   usize size_hint = 0;
   (void)mod_load_imports(*out_ctx, doc, &size_hint, &err_code);
   usize capacity = mod_ctx_arena_size_hint(size_hint);
   mod_ctx_create_arena(*out_ctx, capacity, &err_code);
   return doc;
}

/* ======================================================================
 * Module-level ('@[...]' in the header) — ATG_M*
 * ====================================================================== */

/* ATG_M01 — baseline: alpha + digit + underscore key is accepted */
static void test_atg_m01_baseline_identifier_key(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[valid_key1]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M01: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_scan_header(doc, &err_code),
                        "ATG_M01: 'valid_key1' key accepted");
   TestBit.is_equal_int(1, (long long)List.size(doc->header->attributes),
                        "ATG_M01: one attribute captured");
   mod_ctx_dispose(ctx);
}
/* ATG_M02 — leading underscore is accepted (is_identifier_start includes '_') */
static void test_atg_m02_leading_underscore_key(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[_leading]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M02: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_scan_header(doc, &err_code),
                        "ATG_M02: '_leading' key accepted");
   mod_ctx_dispose(ctx);
}
/* ATG_M03 — leading digit is rejected outright (is_identifier_start excludes digits) */
static void test_atg_m03_leading_digit_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[1bad]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M03: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_scan_header(doc, &err_code),
                        "ATG_M03: '1bad' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_INVALID_IDENTIFIER, err_code,
                        "ATG_M03: err_code is INVALID_IDENTIFIER (nothing consumed yet)");
   mod_ctx_dispose(ctx);
}
/* ATG_M04 — hyphen mid-key rejected: key scan truncates at the hyphen, then the
 * hyphen is neither '=', ',' nor ']' */
static void test_atg_m04_hyphen_in_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[has-hyphen]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M04: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_scan_header(doc, &err_code),
                        "ATG_M04: 'has-hyphen' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ATG_M04: err_code is UNEXPECTED_TOKEN (truncated key, then stray '-')");
   mod_ctx_dispose(ctx);
}
/* ATG_M05 — dot mid-key rejected, same shape as the hyphen case */
static void test_atg_m05_dot_in_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[has.dot]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M05: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_scan_header(doc, &err_code),
                        "ATG_M05: 'has.dot' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ATG_M05: err_code is UNEXPECTED_TOKEN");
   mod_ctx_dispose(ctx);
}
/* ATG_M06 — colon mid-key rejected */
static void test_atg_m06_colon_in_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[has:colon]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M06: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_scan_header(doc, &err_code),
                        "ATG_M06: 'has:colon' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ATG_M06: err_code is UNEXPECTED_TOKEN");
   mod_ctx_dispose(ctx);
}
/* ATG_M07 — slash mid-key rejected */
static void test_atg_m07_slash_in_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[has/slash]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M07: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_scan_header(doc, &err_code),
                        "ATG_M07: 'has/slash' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ATG_M07: err_code is UNEXPECTED_TOKEN");
   mod_ctx_dispose(ctx);
}
/* ATG_M08 — mixed-case key is accepted (alpha check is not case-sensitive) */
static void test_atg_m08_mixed_case_key_accepted(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[CamelCase]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M08: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_scan_header(doc, &err_code),
                        "ATG_M08: 'CamelCase' key accepted");
   mod_ctx_dispose(ctx);
}
/* ATG_M09 — single-character key is accepted */
static void test_atg_m09_single_char_key_accepted(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[x]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M09: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_scan_header(doc, &err_code),
                        "ATG_M09: 'x' key accepted");
   mod_ctx_dispose(ctx);
}
/* ATG_M10 — a hyphenated VALUE is accepted even though the same bytes in a KEY
 * are rejected (ATG_M04): value scanning is permissive (anything up to an
 * unquoted ',' or ']'), independent of the strict key grammar. */
static void test_atg_m10_hyphenated_value_accepted(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("@[key=has-hyphen-value]\nname := test\n", &ctx);
   TestBit.is_not_null(doc, "ATG_M10: registered document allocated");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_scan_header(doc, &err_code),
                        "ATG_M10: hyphenated value accepted");
   anvl_attribute attr = NULL;
   List.get(doc->header->attributes, 0, (object *)&attr);
   TestBit.is_not_null(attr, "ATG_M10: attribute retrieved");
   if (attr) {
      TestBit.is_true(slice_equals(attr->value, "has-hyphen-value"),
                      "ATG_M10: value text is 'has-hyphen-value' verbatim");
   }
   mod_ctx_dispose(ctx);
}

/* ======================================================================
 * Statement-level ('@[...]' on a statement) — ATG_S*, same shape as ATG_M*
 * ====================================================================== */

/* ATG_S01 — baseline: alpha + digit + underscore key is accepted */
static void test_atg_s01_baseline_identifier_key(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[valid_key1] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S01: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_parse_body(doc, &err_code),
                        "ATG_S01: 'valid_key1' key accepted");
   mod_ctx_dispose(ctx);
}
/* ATG_S02 — leading underscore is accepted */
static void test_atg_s02_leading_underscore_key(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[_leading] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S02: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_parse_body(doc, &err_code),
                        "ATG_S02: '_leading' key accepted");
   mod_ctx_dispose(ctx);
}
/* ATG_S03 — leading digit is rejected */
static void test_atg_s03_leading_digit_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[1bad] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S03: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_parse_body(doc, &err_code),
                        "ATG_S03: '1bad' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_INVALID_IDENTIFIER, err_code,
                        "ATG_S03: err_code is INVALID_IDENTIFIER (matches ATG_M03)");
   mod_ctx_dispose(ctx);
}
/* ATG_S04 — hyphen mid-key rejected */
static void test_atg_s04_hyphen_in_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[has-hyphen] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S04: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_parse_body(doc, &err_code),
                        "ATG_S04: 'has-hyphen' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ATG_S04: err_code is UNEXPECTED_TOKEN (matches ATG_M04)");
   mod_ctx_dispose(ctx);
}
/* ATG_S05 — dot mid-key rejected */
static void test_atg_s05_dot_in_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[has.dot] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S05: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_parse_body(doc, &err_code),
                        "ATG_S05: 'has.dot' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ATG_S05: err_code is UNEXPECTED_TOKEN (matches ATG_M05)");
   mod_ctx_dispose(ctx);
}
/* ATG_S06 — colon mid-key rejected */
static void test_atg_s06_colon_in_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[has:colon] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S06: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_parse_body(doc, &err_code),
                        "ATG_S06: 'has:colon' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ATG_S06: err_code is UNEXPECTED_TOKEN (matches ATG_M06)");
   mod_ctx_dispose(ctx);
}
/* ATG_S07 — slash mid-key rejected */
static void test_atg_s07_slash_in_key_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[has/slash] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S07: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, doc_parse_body(doc, &err_code),
                        "ATG_S07: 'has/slash' key rejected");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ATG_S07: err_code is UNEXPECTED_TOKEN (matches ATG_M07)");
   mod_ctx_dispose(ctx);
}
/* ATG_S08 — mixed-case key is accepted */
static void test_atg_s08_mixed_case_key_accepted(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[CamelCase] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S08: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_parse_body(doc, &err_code),
                        "ATG_S08: 'CamelCase' key accepted");
   mod_ctx_dispose(ctx);
}
/* ATG_S09 — single-character key is accepted */
static void test_atg_s09_single_char_key_accepted(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[x] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S09: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_parse_body(doc, &err_code),
                        "ATG_S09: 'x' key accepted");
   mod_ctx_dispose(ctx);
}
/* ATG_S10 — a hyphenated VALUE is accepted, same asymmetry as ATG_M10 */
static void test_atg_s10_hyphenated_value_accepted(void) {
   module_context ctx = NULL;
   module_document doc = setup_attr_stmt_doc("server @[key=has-hyphen-value] := \"prod-1\";\n", &ctx);
   TestBit.is_not_null(doc, "ATG_S10: document loaded");
   if (!doc) {
      return;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_parse_body(doc, &err_code),
                        "ATG_S10: hyphenated value accepted");
   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ATG_S10: 'server' statement retrieved");
   if (stmt && stmt->attributes) {
      anvl_attribute attr = NULL;
      List.get(stmt->attributes, 0, (object *)&attr);
      TestBit.is_not_null(attr, "ATG_S10: attribute retrieved");
      if (attr) {
         TestBit.is_true(slice_equals(attr->value, "has-hyphen-value"),
                         "ATG_S10: value text is 'has-hyphen-value' verbatim");
      }
   }
   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("ATG_M01_baseline_identifier_key", NULL, test_atg_m01_baseline_identifier_key, th);
   TestBit.run_ex("ATG_M02_leading_underscore_key", NULL, test_atg_m02_leading_underscore_key, th);
   TestBit.run_ex("ATG_M03_leading_digit_key_rejected", NULL,
                  test_atg_m03_leading_digit_key_rejected, th);
   TestBit.run_ex("ATG_M04_hyphen_in_key_rejected", NULL, test_atg_m04_hyphen_in_key_rejected, th);
   TestBit.run_ex("ATG_M05_dot_in_key_rejected", NULL, test_atg_m05_dot_in_key_rejected, th);
   TestBit.run_ex("ATG_M06_colon_in_key_rejected", NULL, test_atg_m06_colon_in_key_rejected, th);
   TestBit.run_ex("ATG_M07_slash_in_key_rejected", NULL, test_atg_m07_slash_in_key_rejected, th);
   TestBit.run_ex("ATG_M08_mixed_case_key_accepted", NULL, test_atg_m08_mixed_case_key_accepted, th);
   TestBit.run_ex("ATG_M09_single_char_key_accepted", NULL, test_atg_m09_single_char_key_accepted, th);
   TestBit.run_ex("ATG_M10_hyphenated_value_accepted", NULL,
                  test_atg_m10_hyphenated_value_accepted, th);

   TestBit.run_ex("ATG_S01_baseline_identifier_key", NULL, test_atg_s01_baseline_identifier_key, th);
   TestBit.run_ex("ATG_S02_leading_underscore_key", NULL, test_atg_s02_leading_underscore_key, th);
   TestBit.run_ex("ATG_S03_leading_digit_key_rejected", NULL,
                  test_atg_s03_leading_digit_key_rejected, th);
   TestBit.run_ex("ATG_S04_hyphen_in_key_rejected", NULL, test_atg_s04_hyphen_in_key_rejected, th);
   TestBit.run_ex("ATG_S05_dot_in_key_rejected", NULL, test_atg_s05_dot_in_key_rejected, th);
   TestBit.run_ex("ATG_S06_colon_in_key_rejected", NULL, test_atg_s06_colon_in_key_rejected, th);
   TestBit.run_ex("ATG_S07_slash_in_key_rejected", NULL, test_atg_s07_slash_in_key_rejected, th);
   TestBit.run_ex("ATG_S08_mixed_case_key_accepted", NULL, test_atg_s08_mixed_case_key_accepted, th);
   TestBit.run_ex("ATG_S09_single_char_key_accepted", NULL, test_atg_s09_single_char_key_accepted, th);
   TestBit.run_ex("ATG_S10_hyphenated_value_accepted", NULL,
                  test_atg_s10_hyphenated_value_accepted, th);

   return TestBit.report();
}
