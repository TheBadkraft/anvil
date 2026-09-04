/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_resolver.c - Unit tests for the Resolution phase (phase 4)        *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_resolver.c                                        *
 * ---------------------------------------------------------------------- *
 * mod_resolve_context: identifier-map construction and duplicate-name    *
 * detection, '$identifier' VarRef resolution (chain-following, cycle/    *
 * missing/anonymous-target all non-error), and 'base' target legality    *
 * (missing base, anonymous-object base both hard errors). See            *
 * notes/resolution-phase.md.                                             *
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

/* Parses every document currently registered on ctx, root and imports alike. */
static bool parse_all_docs(module_context ctx) {
   usize count = List.size(ctx->docs);
   for (usize i = 0; i < count; i++) {
      module_document doc = NULL;
      List.get(ctx->docs, i, (object *)&doc);
      if (!doc) {
         continue;
      }
      anvl_err_code err_code = ANVL_ERR_NONE;
      if (ANVL_RES_OK != doc_parse_body(doc, &err_code)) {
         return false;
      }
   }
   return true;
}

/* ---------------------------------------------------------------------- *
 * RSV01 — duplicate top-level name within a single document
 * ---------------------------------------------------------------------- */
static void test_rsv01_duplicate_name_same_document(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "shared := 1;\n"
                                       "shared := 2;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV01: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV01: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "RSV01: resolve returns ERR");
   TestBit.is_equal_int(ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER, err_code,
                        "RSV01: err_code reports DUPLICATE_IDENTIFIER");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV02 — duplicate top-level name across a merged import
 * (resolver_dup_import.anvl declares 'shared' too)
 * ---------------------------------------------------------------------- */
static void test_rsv02_duplicate_name_across_import(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "import \"../fixtures/resolver_dup_import.anvl\";\n"
                                       "shared := 2;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV02: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_equal_int(2, (long long)List.size(ctx->docs), "RSV02: two documents registered");
   TestBit.is_true(parse_all_docs(ctx), "RSV02: both bodies parse");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "RSV02: resolve returns ERR");
   TestBit.is_equal_int(ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER, err_code,
                        "RSV02: err_code reports DUPLICATE_IDENTIFIER");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV03 — '$identifier' VarRef resolves to a concrete value (single hop)
 * ---------------------------------------------------------------------- */
static void test_rsv03_varref_resolves_single_hop(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "name := 42;\n"
                                       "alias := $name;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV03: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV03: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "RSV03: resolve returns OK");

   anvl_statement name_stmt = NULL;
   anvl_statement alias_stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&name_stmt);
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&alias_stmt);
   TestBit.is_not_null(name_stmt, "RSV03: 'name' statement retrieved");
   TestBit.is_not_null(alias_stmt, "RSV03: 'alias' statement retrieved");
   if (name_stmt && alias_stmt && alias_stmt->value) {
      TestBit.is_not_null(alias_stmt->value->varref.resolved, "RSV03: alias resolved is set");
      TestBit.is_true(alias_stmt->value->varref.resolved == name_stmt->value,
                      "RSV03: alias resolves to name's own value node");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV04 — VarRef chain resolves to the final concrete value, not one hop
 * ---------------------------------------------------------------------- */
static void test_rsv04_varref_chain_resolves_to_final_value(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "a := 1;\n"
                                       "b := $a;\n"
                                       "c := $b;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV04: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV04: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "RSV04: resolve returns OK");

   anvl_statement a_stmt = NULL;
   anvl_statement c_stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&a_stmt);
   FArray.get(doc->body, 2, sizeof(anvl_statement), (object *)&c_stmt);
   TestBit.is_not_null(a_stmt, "RSV04: 'a' statement retrieved");
   TestBit.is_not_null(c_stmt, "RSV04: 'c' statement retrieved");
   if (a_stmt && c_stmt && c_stmt->value) {
      TestBit.is_true(c_stmt->value->varref.resolved == a_stmt->value,
                      "RSV04: c resolves straight through to a's value, not to b's VARREF");
      if (c_stmt->value->varref.resolved) {
         TestBit.is_equal_int(ANVL_VALUE_NUMERIC,
                              (long long)c_stmt->value->varref.resolved->type,
                              "RSV04: final resolved value is NUMERIC");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV05 — VarRef cycle resolves to null, not an error
 * ---------------------------------------------------------------------- */
static void test_rsv05_varref_cycle_resolves_to_null(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "a := $b;\n"
                                       "b := $a;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV05: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV05: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "RSV05: resolve returns OK (cycle is not an error)");

   anvl_statement a_stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&a_stmt);
   TestBit.is_not_null(a_stmt, "RSV05: 'a' statement retrieved");
   if (a_stmt && a_stmt->value) {
      TestBit.is_null(a_stmt->value->varref.resolved, "RSV05: a's resolved is NULL");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV06 — VarRef to a missing target resolves to null, not an error
 * ---------------------------------------------------------------------- */
static void test_rsv06_varref_missing_target_resolves_to_null(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "alias := $nope;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV06: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV06: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "RSV06: resolve returns OK (missing target is not an error)");

   anvl_statement alias_stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&alias_stmt);
   TestBit.is_not_null(alias_stmt, "RSV06: 'alias' statement retrieved");
   if (alias_stmt && alias_stmt->value) {
      TestBit.is_null(alias_stmt->value->varref.resolved, "RSV06: alias's resolved is NULL");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV07 — VarRef targeting an anonymous (OBJECT_BLOCK) statement resolves
 * to null, not an error — no value of its own to alias
 * ---------------------------------------------------------------------- */
static void test_rsv07_varref_to_anonymous_resolves_to_null(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "foo {\n"
                                       "   x := 1;\n"
                                       "};\n"
                                       "alias := $foo;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV07: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV07: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res,
                        "RSV07: resolve returns OK (anonymous target is not an error)");

   anvl_statement alias_stmt = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&alias_stmt);
   TestBit.is_not_null(alias_stmt, "RSV07: 'alias' statement retrieved");
   if (alias_stmt && alias_stmt->value) {
      TestBit.is_null(alias_stmt->value->varref.resolved, "RSV07: alias's resolved is NULL");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV08 — 'base' naming nothing declared is a hard error
 * ---------------------------------------------------------------------- */
static void test_rsv08_missing_base_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "derived : nope := {\n"
                                       "   y := 2;\n"
                                       "};\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV08: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV08: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "RSV08: resolve returns ERR");
   TestBit.is_equal_int(ANVL_ERR_RESOLVER_MISSING_BASE, err_code,
                        "RSV08: err_code reports MISSING_BASE");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV09 — 'base' naming an anonymous (OBJECT_BLOCK) statement is a hard
 * error — anonymous objects are immutable, cannot be inherited from
 * ---------------------------------------------------------------------- */
static void test_rsv09_anonymous_base_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "foo {\n"
                                       "   x := 1;\n"
                                       "};\n"
                                       "derived : foo := {\n"
                                       "   y := 2;\n"
                                       "};\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV09: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV09: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "RSV09: resolve returns ERR");
   TestBit.is_equal_int(ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS, err_code,
                        "RSV09: err_code reports CANNOT_INHERIT_FROM_ANONYMOUS");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV10 — 'base' naming a valid (ASSIGN, object-valued) statement resolves
 * cleanly — the positive case, proving the check doesn't false-positive
 * ---------------------------------------------------------------------- */
static void test_rsv10_valid_base_resolves(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "foo := {\n"
                                       "   x := 1;\n"
                                       "};\n"
                                       "derived : foo := {\n"
                                       "   y := 2;\n"
                                       "};\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV10: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV10: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "RSV10: resolve returns OK");
   TestBit.is_false(doc_has_errors(doc), "RSV10: document reports no errors");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV11 — inheritance merges base's fields into derived's own field list,
 * in place; derived's own same-named field wins over base's
 * ---------------------------------------------------------------------- */
static void test_rsv11_merge_appends_and_overrides(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "base := {\n"
                                       "   x := 1;\n"
                                       "   y := 2;\n"
                                       "};\n"
                                       "derived : base := {\n"
                                       "   y := 20;\n"
                                       "   z := 3;\n"
                                       "};\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV11: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV11: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "RSV11: resolve returns OK");

   anvl_statement derived = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&derived);
   TestBit.is_not_null(derived, "RSV11: 'derived' statement retrieved");
   if (derived && derived->value) {
      list fields = derived->value->object.statements;
      TestBit.is_equal_int(3, (long long)List.size(fields),
                           "RSV11: derived has own y/z plus inherited x — three fields");

      anvl_statement f = NULL;
      List.get(fields, 0, (object *)&f);
      TestBit.is_true(f && slice_equals(f->name, "y"), "RSV11: first field is own 'y'");
      if (f && f->value) {
         TestBit.is_true(slice_equals(f->value->text, "20"),
                         "RSV11: derived's own 'y' (20) wins, not base's (2)");
      }

      f = NULL;
      List.get(fields, 2, (object *)&f);
      TestBit.is_true(f && slice_equals(f->name, "x"),
                      "RSV11: third field is inherited 'x', appended after derived's own");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV12 — transitive inheritance (c : b : a) merges through the whole
 * chain, not just the immediate base
 * ---------------------------------------------------------------------- */
static void test_rsv12_transitive_merge(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "a := {\n"
                                       "   x := 1;\n"
                                       "};\n"
                                       "b : a := {\n"
                                       "   y := 2;\n"
                                       "};\n"
                                       "c : b := {\n"
                                       "   z := 3;\n"
                                       "};\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV12: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV12: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "RSV12: resolve returns OK");

   anvl_statement c_stmt = NULL;
   FArray.get(doc->body, 2, sizeof(anvl_statement), (object *)&c_stmt);
   TestBit.is_not_null(c_stmt, "RSV12: 'c' statement retrieved");
   if (c_stmt && c_stmt->value) {
      list fields = c_stmt->value->object.statements;
      TestBit.is_equal_int(3, (long long)List.size(fields),
                           "RSV12: c has its own z, plus y (from b) and x (from a) — three fields");

      bool has_x = false, has_y = false, has_z = false;
      usize count = List.size(fields);
      for (usize i = 0; i < count; i++) {
         anvl_statement f = NULL;
         List.get(fields, i, (object *)&f);
         if (!f) {
            continue;
         }
         if (slice_equals(f->name, "x")) {
            has_x = true;
         }
         if (slice_equals(f->name, "y")) {
            has_y = true;
         }
         if (slice_equals(f->name, "z")) {
            has_z = true;
         }
      }
      TestBit.is_true(has_x && has_y && has_z, "RSV12: c has x, y, and z");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV13 — OBJECT_BLOCK-form derived ('derived : base { ... };') merges
 * from an ASSIGN-form base the same way the ASSIGN-form derived does
 * ---------------------------------------------------------------------- */
static void test_rsv13_object_block_derived_merges(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "base := {\n"
                                       "   x := 1;\n"
                                       "};\n"
                                       "derived : base {\n"
                                       "   y := 2;\n"
                                       "};\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV13: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV13: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "RSV13: resolve returns OK");

   anvl_statement derived = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&derived);
   TestBit.is_not_null(derived, "RSV13: 'derived' statement retrieved");
   if (derived) {
      TestBit.is_equal_int(2, (long long)List.size(derived->body),
                           "RSV13: derived's body has own y plus inherited x");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * RSV14 — an inheritance cycle (a : b; b : a;) is a hard error, unlike a
 * VarRef cycle
 * ---------------------------------------------------------------------- */
static void test_rsv14_inheritance_cycle_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "a : b := {\n"
                                       "   p := 1;\n"
                                       "};\n"
                                       "b : a := {\n"
                                       "   q := 2;\n"
                                       "};\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV14: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV14: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "RSV14: resolve returns ERR");
   TestBit.is_equal_int(ANVL_ERR_RESOLVER_CYCLE_DETECTED, err_code,
                        "RSV14: err_code reports CYCLE_DETECTED");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("RSV01_duplicate_name_same_document", NULL,
                  test_rsv01_duplicate_name_same_document, th);
   TestBit.run_ex("RSV02_duplicate_name_across_import", NULL,
                  test_rsv02_duplicate_name_across_import, th);
   TestBit.run_ex("RSV03_varref_resolves_single_hop", NULL,
                  test_rsv03_varref_resolves_single_hop, th);
   TestBit.run_ex("RSV04_varref_chain_resolves_to_final_value", NULL,
                  test_rsv04_varref_chain_resolves_to_final_value, th);
   TestBit.run_ex("RSV05_varref_cycle_resolves_to_null", NULL,
                  test_rsv05_varref_cycle_resolves_to_null, th);
   TestBit.run_ex("RSV06_varref_missing_target_resolves_to_null", NULL,
                  test_rsv06_varref_missing_target_resolves_to_null, th);
   TestBit.run_ex("RSV07_varref_to_anonymous_resolves_to_null", NULL,
                  test_rsv07_varref_to_anonymous_resolves_to_null, th);
   TestBit.run_ex("RSV08_missing_base_rejected", NULL, test_rsv08_missing_base_rejected, th);
   TestBit.run_ex("RSV09_anonymous_base_rejected", NULL, test_rsv09_anonymous_base_rejected, th);
   TestBit.run_ex("RSV10_valid_base_resolves", NULL, test_rsv10_valid_base_resolves, th);
   TestBit.run_ex("RSV11_merge_appends_and_overrides", NULL,
                  test_rsv11_merge_appends_and_overrides, th);
   TestBit.run_ex("RSV12_transitive_merge", NULL, test_rsv12_transitive_merge, th);
   TestBit.run_ex("RSV13_object_block_derived_merges", NULL,
                  test_rsv13_object_block_derived_merges, th);
   TestBit.run_ex("RSV14_inheritance_cycle_rejected", NULL,
                  test_rsv14_inheritance_cycle_rejected, th);

   return TestBit.report();
}
