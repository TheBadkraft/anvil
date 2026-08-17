/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_body_aml.c - Unit tests for AML-only body parsing                 *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_body_aml.c                                        *
 * ---------------------------------------------------------------------- *
 * Only what AML adds beyond AMP: tuple, object-as-value, static          *
 * references, OBJECT_BLOCK (namespace / immutable / inheritance), and    *
 * import. Scalar/array assignment is already proven by test_body_amp.c   *
 * and behaves identically here, so it isn't re-tested. See               *
 * notes/document-body-parse.md.                                          *
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

/* Loads a fixture through header-scan + import-loading, ready for doc_parse_body. */
static module_document setup_aml_doc(const char *fixture_name, module_context *out_ctx) {
   module_document doc = setup_registered_file(fixture_name, out_ctx);
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
 * AML00 — tuple assignment (f06_tuple.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml00_tuple_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("f06_tuple.anvl", &ctx);
   TestBit.is_not_null(doc, "AML00: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML00: parse body returns OK");
   TestBit.is_equal_int(1, (long long)List.size(doc->body), "AML00: one statement captured");

   anvl_doc_statement stmt = NULL;
   List.get(doc->body, 0, (object *)&stmt);
   TestBit.is_not_null(stmt, "AML00: statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_TUPLE, (long long)stmt->value->type, "AML00: value is TUPLE");
      TestBit.is_equal_int(2, (long long)List.size(stmt->value->collection.items),
                           "AML00: tuple has two elements");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML01 — object-as-value assignment (f07_object.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml01_object_value_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("f07_object.anvl", &ctx);
   TestBit.is_not_null(doc, "AML01: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML01: parse body returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->body), "AML01: two statements captured");

   anvl_doc_statement stmt = NULL;
   List.get(doc->body, 0, (object *)&stmt);
   TestBit.is_not_null(stmt, "AML01: 'config' statement retrieved");
   if (stmt) {
      TestBit.is_equal_int(ANVL_STMT_ASSIGN, (long long)stmt->kind, "AML01: statement is ASSIGN");
      TestBit.is_not_null(stmt->value, "AML01: statement has a value");
      if (stmt->value) {
         TestBit.is_equal_int(ANVL_VALUE_OBJECT, (long long)stmt->value->type,
                              "AML01: value is OBJECT");
         TestBit.is_equal_int(2, (long long)List.size(stmt->value->object.statements),
                              "AML01: object value has two nested statements (host, port)");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML02 — static value reference (f11_static_ref.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml02_static_value_reference(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("f11_static_ref.anvl", &ctx);
   TestBit.is_not_null(doc, "AML02: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML02: parse body returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->body), "AML02: two statements captured");

   anvl_doc_statement derived = NULL;
   List.get(doc->body, 1, (object *)&derived);
   TestBit.is_not_null(derived, "AML02: 'derived_var' statement retrieved");
   if (derived && derived->value) {
      TestBit.is_equal_int(ANVL_VALUE_IDENTIFIER, (long long)derived->value->type,
                           "AML02: value is IDENTIFIER");
      TestBit.is_true(slice_equals(derived->value->text, "base_var"),
                      "AML02: identifier text references 'base_var'");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML03 — inheritance via the ASSIGN form (body_assign_inherit.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml03_inherit_via_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_assign_inherit.anvl", &ctx);
   TestBit.is_not_null(doc, "AML03: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML03: parse body returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->body), "AML03: two statements captured");

   anvl_doc_statement derived = NULL;
   List.get(doc->body, 1, (object *)&derived);
   TestBit.is_not_null(derived, "AML03: 'derived' statement retrieved");
   if (derived) {
      TestBit.is_equal_int(ANVL_STMT_ASSIGN, (long long)derived->kind, "AML03: statement is ASSIGN");
      TestBit.is_false(Source.slice_is_empty(derived->base), "AML03: base is set");
      TestBit.is_not_null(derived->value, "AML03: statement has a value");
      if (derived->value) {
         TestBit.is_equal_int(ANVL_VALUE_OBJECT, (long long)derived->value->type,
                              "AML03: value is OBJECT");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML04 — base with a non-object value reports a deterministic parse
 * error (body_err_base_non_object.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml04_base_requires_object_value(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_err_base_non_object.anvl", &ctx);
   TestBit.is_not_null(doc, "AML04: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AML04: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AML04: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML05 — namespace/scoping container (body_block_namespace.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml05_namespace_block(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_block_namespace.anvl", &ctx);
   TestBit.is_not_null(doc, "AML05: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML05: parse body returns OK");
   TestBit.is_equal_int(1, (long long)List.size(doc->body), "AML05: one statement captured");

   anvl_doc_statement stmt = NULL;
   List.get(doc->body, 0, (object *)&stmt);
   TestBit.is_not_null(stmt, "AML05: 'label' statement retrieved");
   if (stmt) {
      TestBit.is_equal_int(ANVL_STMT_OBJECT_BLOCK, (long long)stmt->kind,
                           "AML05: statement is OBJECT_BLOCK");
      TestBit.is_true(Source.slice_is_empty(stmt->base), "AML05: base is empty");
      TestBit.is_null(stmt->attributes, "AML05: attributes is NULL");
      TestBit.is_not_null(stmt->body, "AML05: nested body list allocated");
      if (stmt->body) {
         TestBit.is_equal_int(2, (long long)List.size(stmt->body),
                              "AML05: two nested statements (field, other)");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML06 — immutable object with attributes, no base (f12_immutable_object.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml06_immutable_object_block(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("f12_immutable_object.anvl", &ctx);
   TestBit.is_not_null(doc, "AML06: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML06: parse body returns OK");
   TestBit.is_equal_int(1, (long long)List.size(doc->body), "AML06: one statement captured");

   anvl_doc_statement stmt = NULL;
   List.get(doc->body, 0, (object *)&stmt);
   TestBit.is_not_null(stmt, "AML06: 'config' statement retrieved");
   if (stmt) {
      TestBit.is_equal_int(ANVL_STMT_OBJECT_BLOCK, (long long)stmt->kind,
                           "AML06: statement is OBJECT_BLOCK");
      TestBit.is_true(Source.slice_is_empty(stmt->base), "AML06: base is empty");
      TestBit.is_not_null(stmt->attributes, "AML06: attributes captured");
      if (stmt->attributes) {
         TestBit.is_equal_int(1, (long long)List.size(stmt->attributes),
                              "AML06: one attribute captured");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML07 — inheritance with override via the block form (body_block_inherit.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml07_inherit_via_block(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_block_inherit.anvl", &ctx);
   TestBit.is_not_null(doc, "AML07: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML07: parse body returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->body), "AML07: two statements captured");

   anvl_doc_statement derived = NULL;
   List.get(doc->body, 1, (object *)&derived);
   TestBit.is_not_null(derived, "AML07: 'derived' statement retrieved");
   if (derived) {
      TestBit.is_equal_int(ANVL_STMT_OBJECT_BLOCK, (long long)derived->kind,
                           "AML07: statement is OBJECT_BLOCK");
      TestBit.is_false(Source.slice_is_empty(derived->base), "AML07: base is set");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML08 — base and attributes together (body_block_inherit_attrs.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml08_base_and_attributes(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_block_inherit_attrs.anvl", &ctx);
   TestBit.is_not_null(doc, "AML08: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML08: parse body returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->body), "AML08: two statements captured");

   anvl_doc_statement derived = NULL;
   List.get(doc->body, 1, (object *)&derived);
   TestBit.is_not_null(derived, "AML08: 'derived' statement retrieved");
   if (derived) {
      TestBit.is_false(Source.slice_is_empty(derived->base), "AML08: base is set");
      TestBit.is_not_null(derived->attributes, "AML08: attributes captured");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML09 — unterminated object-block reports error (body_err_unterminated_block.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml09_unterminated_object_block(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_err_unterminated_block.anvl", &ctx);
   TestBit.is_not_null(doc, "AML09: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AML09: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AML09: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML10 — bare 'name : base;' with nothing after reports error
 * (body_err_bare_base.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml10_bare_base_statement(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_err_bare_base.anvl", &ctx);
   TestBit.is_not_null(doc, "AML10: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AML10: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AML10: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML11 — import + static reference into the flat merged namespace
 * (f13_import.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml11_import_and_static_reference(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("f13_import.anvl", &ctx);
   TestBit.is_not_null(doc, "AML11: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_equal_int(1, (long long)List.size(doc->header->imports),
                        "AML11: one import captured at header scan");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML11: parse body returns OK");
   TestBit.is_equal_int(1, (long long)List.size(doc->body), "AML11: one statement captured");

   anvl_doc_statement alias = NULL;
   List.get(doc->body, 0, (object *)&alias);
   TestBit.is_not_null(alias, "AML11: 'alias' statement retrieved");
   if (alias && alias->value) {
      TestBit.is_equal_int(ANVL_VALUE_IDENTIFIER, (long long)alias->value->type,
                           "AML11: value is IDENTIFIER");
      TestBit.is_true(slice_equals(alias->value->text, "name"),
                      "AML11: identifier text references 'name'");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("AML00_tuple_assign", NULL, test_aml00_tuple_assign, th);
   TestBit.run_ex("AML01_object_value_assign", NULL, test_aml01_object_value_assign, th);
   TestBit.run_ex("AML02_static_value_reference", NULL, test_aml02_static_value_reference, th);
   TestBit.run_ex("AML03_inherit_via_assign", NULL, test_aml03_inherit_via_assign, th);
   TestBit.run_ex("AML04_base_requires_object_value", NULL,
                  test_aml04_base_requires_object_value, th);
   TestBit.run_ex("AML05_namespace_block", NULL, test_aml05_namespace_block, th);
   TestBit.run_ex("AML06_immutable_object_block", NULL, test_aml06_immutable_object_block, th);
   TestBit.run_ex("AML07_inherit_via_block", NULL, test_aml07_inherit_via_block, th);
   TestBit.run_ex("AML08_base_and_attributes", NULL, test_aml08_base_and_attributes, th);
   TestBit.run_ex("AML09_unterminated_object_block", NULL, test_aml09_unterminated_object_block, th);
   TestBit.run_ex("AML10_bare_base_statement", NULL, test_aml10_bare_base_statement, th);
   TestBit.run_ex("AML11_import_and_static_reference", NULL,
                  test_aml11_import_and_static_reference, th);

   return TestBit.report();
}
