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
 * Status: complete and GREEN against the real parser (src/core/parser.c) *
 * — all cases pass, including object/OBJECT_BLOCK, statement            *
 * attributes, and inheritance/base, the last pieces to land.             *
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
   usize size_hint = 0;
   (void)mod_load_imports(*out_ctx, doc, &size_hint, &err_code);
   usize capacity = mod_ctx_arena_size_hint(size_hint);
   mod_ctx_create_arena(*out_ctx, capacity, &err_code);
   return doc;
}

/* ---------------------------------------------------------------------- *
 * AML00 — tuple assignment, including a nested tuple element (f06_tuple.anvl)
 * Commentary: `nested`'s middle element is itself a tuple — the case this
 * fixture's own comment block always promised ("nested elements") but
 * couldn't deliver until array/tuple elements accepted any value, not
 * just scalars (parse_collection now calls parse_value_body per element,
 * not parse_scalar_value — see notes/document-body-parse.md). AMP's
 * scalar-only element restriction (AMP20) is unaffected — this is purely
 * an AML capability.
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
   TestBit.is_equal_int(3, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML00: three statements captured");

   // coords := (10, 20); — numeric elements
   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AML00: 'coords' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_TUPLE, (long long)stmt->value->type,
                           "AML00: coords value is TUPLE");
      TestBit.is_equal_int(2, (long long)List.size(stmt->value->collection.items),
                           "AML00: coords tuple has two elements");
   }

   // mixed := (1, "two", true); — mixed scalar elements
   stmt = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AML00: 'mixed' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(3, (long long)List.size(stmt->value->collection.items),
                           "AML00: mixed tuple has three elements");
      anvl_value elem = NULL;
      List.get(stmt->value->collection.items, 1, (object *)&elem);
      if (elem) {
         TestBit.is_equal_int(ANVL_VALUE_STRING, (long long)elem->type,
                              "AML00: mixed tuple's second element is STRING");
      }
   }

   // nested := (1, (2, 3), 4); — a tuple element, nested inside a tuple
   stmt = NULL;
   FArray.get(doc->body, 2, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AML00: 'nested' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(3, (long long)List.size(stmt->value->collection.items),
                           "AML00: nested tuple has three elements");
      anvl_value elem = NULL;
      List.get(stmt->value->collection.items, 1, (object *)&elem);
      TestBit.is_not_null(elem, "AML00: nested tuple's middle element retrieved");
      if (elem) {
         TestBit.is_equal_int(ANVL_VALUE_TUPLE, (long long)elem->type,
                              "AML00: nested tuple's middle element is itself a TUPLE");
         TestBit.is_equal_int(2, (long long)List.size(elem->collection.items),
                              "AML00: the nested tuple has two elements");
      }
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
   TestBit.is_equal_int(2, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML01: two statements captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
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
   TestBit.is_equal_int(2, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML02: two statements captured");

   anvl_statement derived = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&derived);
   TestBit.is_not_null(derived, "AML02: 'derived_var' statement retrieved");
   if (derived && derived->value) {
      TestBit.is_equal_int(ANVL_VALUE_VARREF, (long long)derived->value->type,
                           "AML02: value is VARREF");
      TestBit.is_true(slice_equals(derived->value->text, "$base_var"),
                      "AML02: full text is '$base_var'");
      TestBit.is_true(slice_equals(derived->value->varref.target, "base_var"),
                      "AML02: target references 'base_var'");
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
   TestBit.is_equal_int(2, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML03: two statements captured");

   anvl_statement derived = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&derived);
   TestBit.is_not_null(derived, "AML03: 'derived' statement retrieved");
   if (derived) {
      TestBit.is_equal_int(ANVL_STMT_ASSIGN, (long long)derived->kind,
                           "AML03: statement is ASSIGN");
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
   TestBit.is_equal_int(ANVL_ERR_PARSER_INHERITANCE_REQUIRES_OBJECT, err_code,
                        "AML04: err_code reports INHERITANCE_REQUIRES_OBJECT");

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
   TestBit.is_equal_int(1, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML05: one statement captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
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
   TestBit.is_equal_int(1, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML06: one statement captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
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
   TestBit.is_equal_int(2, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML07: two statements captured");

   anvl_statement derived = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&derived);
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
   TestBit.is_equal_int(2, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML08: two statements captured");

   anvl_statement derived = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&derived);
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
   TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE, err_code,
                        "AML09: err_code reports EXPECTED_OBJECT_CLOSE");

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
   TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_ASSIGN, err_code,
                        "AML10: err_code reports EXPECTED_ASSIGN (neither ':=' nor '{' follows)");

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
   TestBit.is_equal_int(1, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML11: one statement captured");

   anvl_statement alias = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&alias);
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
 * AML12 — statement-level attributes on the ASSIGN form, no base
 * (body_assign_attrs.anvl)
 * Commentary: isolates attribute-list parsing (parse_attribute_list) from
 * object/OBJECT_BLOCK and inheritance, neither of which exists yet — none
 * of AML06/AML08 (the other attribute-bearing fixtures) can pass without
 * those too, so this is the one place attribute parsing gets verified on
 * its own. Covers both a flag attribute (no '=value') and a key=value one
 * in the same list.
 * ---------------------------------------------------------------------- */
static void test_aml12_attributes_on_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_assign_attrs.anvl", &ctx);
   TestBit.is_not_null(doc, "AML12: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML12: parse body returns OK");
   TestBit.is_equal_int(1, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML12: one statement captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AML12: 'server' statement retrieved");
   if (!stmt) {
      mod_ctx_dispose(ctx);
      return;
   }
   TestBit.is_true(Source.slice_is_empty(stmt->base), "AML12: base is empty (no inheritance)");
   TestBit.is_not_null(stmt->attributes, "AML12: attributes captured");
   if (stmt->attributes) {
      TestBit.is_equal_int(2, (long long)List.size(stmt->attributes),
                           "AML12: two attributes captured");

      anvl_attribute attr = NULL;
      List.get(stmt->attributes, 0, (object *)&attr);
      TestBit.is_not_null(attr, "AML12: first attribute retrieved");
      if (attr) {
         TestBit.is_true(slice_equals(attr->key, "active"), "AML12: first attribute key is 'active'");
         TestBit.is_true(Source.slice_is_empty(attr->value),
                         "AML12: first attribute is a flag (empty value)");
      }

      attr = NULL;
      List.get(stmt->attributes, 1, (object *)&attr);
      TestBit.is_not_null(attr, "AML12: second attribute retrieved");
      if (attr) {
         TestBit.is_true(slice_equals(attr->key, "env"), "AML12: second attribute key is 'env'");
         TestBit.is_true(slice_equals(attr->value, "production"),
                         "AML12: second attribute value is 'production'");
      }
   }
   TestBit.is_not_null(stmt->value, "AML12: statement has a value");
   if (stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_STRING, (long long)stmt->value->type,
                           "AML12: value is STRING");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML13 — nested collections: array-of-arrays and object-as-tuple-element
 * (body_nested_collections.anvl)
 * Commentary: the other two nesting combinations AML00's tuple-in-tuple
 * doesn't cover — an array element that is itself an array, and a tuple
 * element that is itself an object. All three together (this one plus
 * AML00) exercise every collection-in-collection pairing parse_collection
 * (via parse_value_body) now supports.
 * ---------------------------------------------------------------------- */
static void test_aml13_nested_collections(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_nested_collections.anvl", &ctx);
   TestBit.is_not_null(doc, "AML13: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML13: parse body returns OK");
   TestBit.is_equal_int(2, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML13: two statements captured");

   // grid := [[1, 2, 3], [4, 5, 6]];
   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AML13: 'grid' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_ARRAY, (long long)stmt->value->type,
                           "AML13: grid value is ARRAY");
      TestBit.is_equal_int(2, (long long)List.size(stmt->value->collection.items),
                           "AML13: grid has two elements");
      anvl_value row = NULL;
      List.get(stmt->value->collection.items, 0, (object *)&row);
      TestBit.is_not_null(row, "AML13: grid's first row retrieved");
      if (row) {
         TestBit.is_equal_int(ANVL_VALUE_ARRAY, (long long)row->type,
                              "AML13: grid's first row is itself an ARRAY");
         TestBit.is_equal_int(3, (long long)List.size(row->collection.items),
                              "AML13: grid's first row has three elements");
      }
   }

   // player := (Aria, { health := 100; stamina := 50; }, warrior);
   stmt = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AML13: 'player' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_TUPLE, (long long)stmt->value->type,
                           "AML13: player value is TUPLE");
      TestBit.is_equal_int(3, (long long)List.size(stmt->value->collection.items),
                           "AML13: player has three elements");
      anvl_value middle = NULL;
      List.get(stmt->value->collection.items, 1, (object *)&middle);
      TestBit.is_not_null(middle, "AML13: player's middle element retrieved");
      if (middle) {
         TestBit.is_equal_int(ANVL_VALUE_OBJECT, (long long)middle->type,
                              "AML13: player's middle element is an OBJECT");
         TestBit.is_equal_int(2, (long long)List.size(middle->object.statements),
                              "AML13: the nested object has two statements");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML14 — '$identifier' VarRef: as a statement's own value, and nested
 * inside an array element (body_varref.anvl)
 * ---------------------------------------------------------------------- */
static void test_aml14_varref_value(void) {
   module_context ctx = NULL;
   module_document doc = setup_aml_doc("body_varref.anvl", &ctx);
   TestBit.is_not_null(doc, "AML14: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AML14: parse body returns OK");
   TestBit.is_equal_int(3, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AML14: three statements captured");

   // alias := $name;
   anvl_statement stmt = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AML14: 'alias' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_VARREF, (long long)stmt->value->type,
                           "AML14: alias value is VARREF");
      TestBit.is_true(slice_equals(stmt->value->text, "$name"),
                      "AML14: alias full text is '$name'");
      TestBit.is_true(slice_equals(stmt->value->varref.target, "name"),
                      "AML14: alias target is 'name'");
   }

   // wrapped := [$name, "static"];
   stmt = NULL;
   FArray.get(doc->body, 2, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AML14: 'wrapped' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_ARRAY, (long long)stmt->value->type,
                           "AML14: wrapped value is ARRAY");
      anvl_value elem = NULL;
      List.get(stmt->value->collection.items, 0, (object *)&elem);
      TestBit.is_not_null(elem, "AML14: wrapped's first element retrieved");
      if (elem) {
         TestBit.is_equal_int(ANVL_VALUE_VARREF, (long long)elem->type,
                              "AML14: wrapped's first element is VARREF");
         TestBit.is_true(slice_equals(elem->varref.target, "name"),
                         "AML14: wrapped's first element targets 'name'");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AML15 — malformed '$' with no identifier following it (nothing, or
 * whitespace before the identifier) — ANVL_ERR_VARS_INVALID_VARREF
 * ---------------------------------------------------------------------- */
static void test_aml15_varref_missing_target_rejected(void) {
   const char *buffers[2] = {
      "#!aml\n"
      "bad := $;\n",
      "#!aml\n"
      "bad := $ name;\n",
   };

   for (usize i = 0; i < 2; i++) {
      module_context ctx = NULL;
      module_document doc = setup_amp_doc(buffers[i], &ctx);
      TestBit.is_not_null(doc, "AML15: document loaded");
      if (!doc) {
         continue;
      }

      anvl_err_code err_code = ANVL_ERR_NONE;
      anvl_result res = doc_parse_body(doc, &err_code);
      TestBit.is_equal_int(ANVL_RES_ERR, res, "AML15: parse body returns ERR");
      TestBit.is_true(doc_has_errors(doc), "AML15: document reports an error");
      TestBit.is_equal_int(ANVL_ERR_VARS_INVALID_VARREF, err_code,
                           "AML15: err_code reports INVALID_VARREF");

      mod_ctx_dispose(ctx);
   }
}
/* ---------------------------------------------------------------------- *
 * AML16 — dotted-path '$identifier.rest' rejected; AML has no dotted-path
 * VarRefs, strictly bare only — ANVL_ERR_VARS_INVALID_VARREF
 * ---------------------------------------------------------------------- */
static void test_aml16_varref_dotted_path_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "bad := $name.sub;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AML16: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AML16: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AML16: document reports an error");
   TestBit.is_equal_int(ANVL_ERR_VARS_INVALID_VARREF, err_code,
                        "AML16: err_code reports INVALID_VARREF");

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
   TestBit.run_ex("AML04_base_requires_object_value", NULL, test_aml04_base_requires_object_value,
                  th);
   TestBit.run_ex("AML05_namespace_block", NULL, test_aml05_namespace_block, th);
   TestBit.run_ex("AML06_immutable_object_block", NULL, test_aml06_immutable_object_block, th);
   TestBit.run_ex("AML07_inherit_via_block", NULL, test_aml07_inherit_via_block, th);
   TestBit.run_ex("AML08_base_and_attributes", NULL, test_aml08_base_and_attributes, th);
   TestBit.run_ex("AML09_unterminated_object_block", NULL, test_aml09_unterminated_object_block,
                  th);
   TestBit.run_ex("AML10_bare_base_statement", NULL, test_aml10_bare_base_statement, th);
   TestBit.run_ex("AML11_import_and_static_reference", NULL, test_aml11_import_and_static_reference,
                  th);
   TestBit.run_ex("AML12_attributes_on_assign", NULL, test_aml12_attributes_on_assign, th);
   TestBit.run_ex("AML13_nested_collections", NULL, test_aml13_nested_collections, th);
   TestBit.run_ex("AML14_varref_value", NULL, test_aml14_varref_value, th);
   TestBit.run_ex("AML15_varref_missing_target_rejected", NULL,
                  test_aml15_varref_missing_target_rejected, th);
   TestBit.run_ex("AML16_varref_dotted_path_rejected", NULL,
                  test_aml16_varref_dotted_path_rejected, th);

   return TestBit.report();
}
