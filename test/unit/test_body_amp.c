/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_body_amp.c - Unit tests for AMP-legal body parsing                *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_body_amp.c                                        *
 * ---------------------------------------------------------------------- *
 * AMP is a strict grammar subset of AML: everything proven here also     *
 * holds in AML, so test_body_aml.c never re-proves scalar/array          *
 * assignment. This suite carries both the AMP-legal positive cases and   *
 * the AMP-illegal negative (rejection) cases, since those rejections are *
 * exactly what makes it a subset. See notes/document-body-parse.md.      *
 *                                                                        *
 * RED-state note: doc_parse_body (src/core/document.c) is currently a    *
 * stub that always returns ANVL_RES_ERR without recording a document     *
 * error. Every test below is expected to fail until the real parser is  *
 * implemented.                                                           *
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

/* Loads an AMP buffer through header-scan + import-loading, ready for doc_parse_body. */
static module_document setup_amp_doc(const char *buffer, module_context *out_ctx) {
   module_document doc = setup_registered_doc(buffer, out_ctx);
   if (!doc) {
      return NULL;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   if (ANVL_RES_OK != doc_scan_header(doc, &err_code)) {
      return doc;
   }
   (void)mod_load_imports(*out_ctx, doc, &err_code);
   return doc;
}

/* ---------------------------------------------------------------------- *
 * AMP00 — empty body
 * ---------------------------------------------------------------------- */
static void test_amp00_empty_body(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n", &ctx);
   TestBit.is_not_null(doc, "AMP00: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP00: parse body returns OK");
   TestBit.is_not_null(doc->body, "AMP00: body list allocated");
   if (doc->body) {
      TestBit.is_equal_int(0, (long long)List.size(doc->body), "AMP00: body list is empty");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP01 — integer assignment: positive, negative, UINT64_MAX-scale
 * ---------------------------------------------------------------------- */
static void test_amp01_integer_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "age := 42;\n"
                                       "negint := -1285;\n"
                                       "large := 18446744073709551615;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP01: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP01: parse body returns OK");
   TestBit.is_equal_int(3, (long long)List.size(doc->body), "AMP01: three statements captured");

   anvl_doc_statement stmt = NULL;
   List.get(doc->body, 0, (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP01: first statement retrieved");
   if (stmt) {
      TestBit.is_equal_int(ANVL_STMT_ASSIGN, (long long)stmt->kind, "AMP01: first statement is ASSIGN");
      TestBit.is_not_null(stmt->value, "AMP01: first statement has a value");
      if (stmt->value) {
         TestBit.is_equal_int(ANVL_VALUE_INTEGER, (long long)stmt->value->type,
                              "AMP01: first value is INTEGER");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP02 — float assignment: negative and scientific-notation
 * ---------------------------------------------------------------------- */
static void test_amp02_float_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "decimal := -3.402823466;\n"
                                       "exp := 1.7976931348623157e+308;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP02: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP02: parse body returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->body), "AMP02: two statements captured");

   anvl_doc_statement stmt = NULL;
   List.get(doc->body, 0, (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP02: first statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_FLOAT, (long long)stmt->value->type,
                           "AMP02: first value is FLOAT");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP03 — string assignment
 * ---------------------------------------------------------------------- */
static void test_amp03_string_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "name := \"hello\";\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP03: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP03: parse body returns OK");
   TestBit.is_equal_int(1, (long long)List.size(doc->body), "AMP03: one statement captured");

   anvl_doc_statement stmt = NULL;
   List.get(doc->body, 0, (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP03: statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_STRING, (long long)stmt->value->type,
                           "AMP03: value is STRING");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP04 — blob assignment: tagged and untagged
 * ---------------------------------------------------------------------- */
static void test_amp04_blob_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "created := @date`2026-07-07`;\n"
                                       "raw := `payload`;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP04: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP04: parse body returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->body), "AMP04: two statements captured");

   anvl_doc_statement tagged = NULL;
   List.get(doc->body, 0, (object *)&tagged);
   TestBit.is_not_null(tagged, "AMP04: tagged statement retrieved");
   if (tagged && tagged->value) {
      TestBit.is_equal_int(ANVL_VALUE_BLOB, (long long)tagged->value->type,
                           "AMP04: tagged value is BLOB");
      TestBit.is_false(Source.slice_is_empty(tagged->value->tag), "AMP04: tagged blob has a tag");
   }

   anvl_doc_statement untagged = NULL;
   List.get(doc->body, 1, (object *)&untagged);
   TestBit.is_not_null(untagged, "AMP04: untagged statement retrieved");
   if (untagged && untagged->value) {
      TestBit.is_equal_int(ANVL_VALUE_BLOB, (long long)untagged->value->type,
                           "AMP04: untagged value is BLOB");
      TestBit.is_true(Source.slice_is_empty(untagged->value->tag),
                      "AMP04: untagged blob has an empty tag");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP05 — scalar array assignment
 * ---------------------------------------------------------------------- */
static void test_amp05_scalar_array_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "tags := [1, 2, 3];\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP05: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP05: parse body returns OK");
   TestBit.is_equal_int(1, (long long)List.size(doc->body), "AMP05: one statement captured");

   anvl_doc_statement stmt = NULL;
   List.get(doc->body, 0, (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP05: statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_ARRAY, (long long)stmt->value->type, "AMP05: value is ARRAY");
      TestBit.is_equal_int(3, (long long)List.size(stmt->value->collection.items),
                           "AMP05: array has three elements");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP06 — multiple statements captured in order
 * ---------------------------------------------------------------------- */
static void test_amp06_multiple_statements_in_order(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "first := 1;\n"
                                       "second := 2;\n"
                                       "third := 3;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP06: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP06: parse body returns OK");
   TestBit.is_equal_int(3, (long long)List.size(doc->body), "AMP06: three statements captured");

   const char *expected_names[3] = {"first", "second", "third"};
   for (usize i = 0; i < 3; i++) {
      anvl_doc_statement stmt = NULL;
      List.get(doc->body, i, (object *)&stmt);
      TestBit.is_not_null(stmt, "AMP06: statement retrieved");
      if (stmt) {
         TestBit.is_true(slice_equals(stmt->name, expected_names[i]),
                         "AMP06: statement name matches expected order");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP07 — missing value after ':=' reports error
 * ---------------------------------------------------------------------- */
static void test_amp07_missing_value_after_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "name := ;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP07: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP07: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP07: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP08 — unterminated array reports error
 * ---------------------------------------------------------------------- */
static void test_amp08_unterminated_array(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "tags := [1, 2, 3;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP08: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP08: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP08: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP09 — invalid blob tag reports error
 * ---------------------------------------------------------------------- */
static void test_amp09_invalid_blob_tag(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "bad := @1abc`content`;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP09: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP09: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP09: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP10 — identifier followed by neither ':=' nor '{' reports error
 * ---------------------------------------------------------------------- */
static void test_amp10_bare_identifier_statement(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "name;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP10: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP10: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP10: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP11 — ':base' on ASSIGN rejected in AMP
 * ---------------------------------------------------------------------- */
static void test_amp11_base_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "base := 1;\n"
                                       "derived : base := 2;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP11: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP11: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP11: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP12 — ANVL_STMT_OBJECT_BLOCK rejected in all three forms
 * ---------------------------------------------------------------------- */
static void test_amp12_object_block_rejected(void) {
   const char *buffers[3] = {
      "#!amp\n"
      "label {\n"
      "   field := 1;\n"
      "};\n",
      "#!amp\n"
      "config @[attrib] {\n"
      "   addr := 1;\n"
      "};\n",
      "#!amp\n"
      "base := 1;\n"
      "derived : base {\n"
      "   override := 2;\n"
      "};\n",
   };

   for (usize i = 0; i < 3; i++) {
      module_context ctx = NULL;
      module_document doc = setup_amp_doc(buffers[i], &ctx);
      TestBit.is_not_null(doc, "AMP12: document loaded");
      if (!doc) {
         continue;
      }

      anvl_err_code err_code = ANVL_ERR_NONE;
      anvl_result res = doc_parse_body(doc, &err_code);
      TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP12: object-block form rejected");
      TestBit.is_true(doc_has_errors(doc), "AMP12: document reports an error");

      mod_ctx_dispose(ctx);
   }
}
/* ---------------------------------------------------------------------- *
 * AMP13 — 'vars { ... }' rejected in AMP
 * ---------------------------------------------------------------------- */
static void test_amp13_vars_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "vars {\n"
                                       "   host := 1;\n"
                                       "};\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP13: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP13: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP13: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP14 — 'using "path";' rejected in AMP
 * ---------------------------------------------------------------------- */
static void test_amp14_using_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "using \"somewhere\";\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP14: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP14: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP14: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP15 — statement-level '@[...]' attributes rejected in AMP
 * ---------------------------------------------------------------------- */
static void test_amp15_statement_attributes_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "server @[active] := 1;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP15: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP15: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP15: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP16 — object-typed ASSIGN value rejected in AMP
 * ---------------------------------------------------------------------- */
static void test_amp16_object_value_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "config := { host := 1; };\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP16: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP16: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP16: document reports an error");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("AMP00_empty_body", NULL, test_amp00_empty_body, th);
   TestBit.run_ex("AMP01_integer_assign", NULL, test_amp01_integer_assign, th);
   TestBit.run_ex("AMP02_float_assign", NULL, test_amp02_float_assign, th);
   TestBit.run_ex("AMP03_string_assign", NULL, test_amp03_string_assign, th);
   TestBit.run_ex("AMP04_blob_assign", NULL, test_amp04_blob_assign, th);
   TestBit.run_ex("AMP05_scalar_array_assign", NULL, test_amp05_scalar_array_assign, th);
   TestBit.run_ex("AMP06_multiple_statements_in_order", NULL,
                  test_amp06_multiple_statements_in_order, th);
   TestBit.run_ex("AMP07_missing_value_after_assign", NULL,
                  test_amp07_missing_value_after_assign, th);
   TestBit.run_ex("AMP08_unterminated_array", NULL, test_amp08_unterminated_array, th);
   TestBit.run_ex("AMP09_invalid_blob_tag", NULL, test_amp09_invalid_blob_tag, th);
   TestBit.run_ex("AMP10_bare_identifier_statement", NULL, test_amp10_bare_identifier_statement, th);
   TestBit.run_ex("AMP11_base_rejected", NULL, test_amp11_base_rejected, th);
   TestBit.run_ex("AMP12_object_block_rejected", NULL, test_amp12_object_block_rejected, th);
   TestBit.run_ex("AMP13_vars_rejected", NULL, test_amp13_vars_rejected, th);
   TestBit.run_ex("AMP14_using_rejected", NULL, test_amp14_using_rejected, th);
   TestBit.run_ex("AMP15_statement_attributes_rejected", NULL,
                  test_amp15_statement_attributes_rejected, th);
   TestBit.run_ex("AMP16_object_value_rejected", NULL, test_amp16_object_value_rejected, th);

   return TestBit.report();
}
