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
}

/* Parses every document currently registered on ctx, root and includes alike. */
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
 * RSV02 — duplicate top-level name across a merged include
 * (resolver_dup_include.anvl declares 'shared' too)
 * ---------------------------------------------------------------------- */
static void test_rsv02_duplicate_name_across_include(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "include \"../fixtures/resolver_dup_include.anvl\";\n"
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
 * RSV07 - a VarRef targeting a block-form (OBJECT_BLOCK) statement resolves
 * to that statement's object value, like any other target (BR-2610-anvl-resolver-001):
 * a block is read-only (it cannot be a base), and a reference is a read.
 * ---------------------------------------------------------------------- */
static void test_rsv07_varref_to_block_form_resolves(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "foo {\n"
                                       "   x := 1;\n"
                                       "};\n"
                                       "alias := $foo;\n"
                                       "chain := $alias;\n"
                                       "o := { inner := $foo; deeper := { r := $alias; }; };\n"
                                       "p { q := $foo; };\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV07: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV07: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "RSV07: resolve returns OK");

   anvl_statement foo = NULL, alias = NULL, chain = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&foo);
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&alias);
   FArray.get(doc->body, 2, sizeof(anvl_statement), (object *)&chain);
   TestBit.is_true(foo && alias && chain && foo->value && alias->value && chain->value,
                   "RSV07: statements retrieved");
   if (foo && alias && chain && foo->value && alias->value && chain->value) {
      TestBit.is_true(alias->value->varref.resolved == foo->value,
                      "RSV07: alias resolves to the block's object value");
      TestBit.is_true(chain->value->varref.resolved == foo->value,
                      "RSV07: a chain through a reference to a block resolves to the block's value");
      TestBit.is_equal_int(ANVL_VALUE_OBJECT, foo->value->type, "RSV07: that value is an object");
   }
   // every reference in the document, at any depth, resolved
   usize unresolved = 0, refs = 0;
   for (usize i = 0; i < List.size(ctx->values); i++) {
      anvl_value v = NULL;
      List.get(ctx->values, i, (object *)&v);
      if (v && v->type == ANVL_VALUE_VARREF) {
         refs++;
         unresolved += v->varref.resolved ? 0 : 1;
      }
   }
   TestBit.is_equal_int(5, (long long)refs, "RSV07: five references in the document");
   TestBit.is_equal_int(0, (long long)unresolved, "RSV07: none left unresolved");

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
 * RSV15 — BR-2611-anvl-resolver-002: a reference that leads back to a
 * container holding it is a hard error, unlike a plain VarRef chain cycle
 * ---------------------------------------------------------------------- */
static void test_rsv15_reference_cycle_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "a := { x := $b; };\n"
                                       "b := { y := $a; };\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV15: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV15: body parses");

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_resolve_context(ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "RSV15: resolve returns ERR");
   TestBit.is_equal_int(ANVL_ERR_RESOLVER_REFERENCE_CYCLE, err_code,
                        "RSV15: err_code reports REFERENCE_CYCLE");
   TestBit.is_true(List.size(ctx->errors) > 0, "RSV15: the error is recorded");
   if (List.size(ctx->errors) > 0) {
      anvl_error e = anvl_error_get(ctx->errors, 0);
      TestBit.is_equal_int(ANVL_ERR_RESOLVER_REFERENCE_CYCLE, e ? (long long)e->code : -1,
                           "RSV15: the recorded code");
      TestBit.is_equal_int(2, e ? (long long)e->line : -1, "RSV15: the first reference, line 2 (after the shebang)");
      TestBit.is_equal_int(13, e ? (long long)e->column : -1, "RSV15: column 13");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * BR-2611-anvl-resolver-003 and FR-2611-anvl-resolver-001 part 2: the merge and the reference chase are linear.
 * RSV16-RSV20 pin what the resolver returns (they hold before and after the rewrite); RSV21-RSV24 bound the time.
 * ---------------------------------------------------------------------- */
#include <stdio.h>
#include <time.h>

static double now_secs(void) {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Valgrind runs the client about 50 times slower: the sizes shrink and the budgets grow, so a run under it checks memory
 * only and a native run checks the time. */
static bool under_valgrind(void) {
   const char *pre = getenv("LD_PRELOAD");
   return pre && strstr(pre, "vgpreload");
}

static anvl_statement stmt_named(module_context ctx, const char *name) {
   addr val = 0;
   if (!ctx->identifiers || !Map.get(ctx->identifiers, name, strlen(name), &val)) {
      return NULL;
   }
   return (anvl_statement)val;
}

/* The field names of a statement's merged list, comma separated, in list order. */
static void field_names(anvl_statement stmt, char *out, usize cap) {
   out[0] = 0;
   if (!stmt || !stmt->value || stmt->value->type != ANVL_VALUE_OBJECT) {
      snprintf(out, cap, "<none>");
      return;
   }
   list fields = stmt->value->object.statements;
   usize n = List.size(fields), at = 0;
   for (usize i = 0; i < n && at + 80 < cap; i++) {
      anvl_statement f = NULL;
      List.get(fields, i, (object *)&f);
      at += (usize)snprintf(out + at, cap - at, "%s%.*s", i ? "," : "", (int)Source.slice_length(f->name), f->name.start);
   }
}

static void expect_fields(module_context ctx, const char *stmt, const char *expected, const char *what) {
   char got[512];
   field_names(stmt_named(ctx, stmt), got, sizeof got);
   TestBit.is_equal_str(expected, got, what);
}

/* RSV16 - the merged field order and the field chosen for a name: own fields first, then each ancestor nearest first;
 * a name already present is skipped. A base may be declared after the statements deriving from it, and a statement may
 * derive from one that is itself derived. */
static void test_rsv16_merge_order_and_overrides(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "e : d := { e1 := 1; a := 5; };\n"
                                       "d : c := { d1 := 1; b := 4; };\n"
                                       "c : b := { c1 := 1; a := 3; };\n"
                                       "b : a := { b1 := 1; x := 2; b := 2; };\n"
                                       "a := { a1 := 1; x := 1; a := 1; b := 1; };\n"
                                       "lonely := { q := 1; };\n"
                                       "dbl : lonely := { q := 9; };\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV16: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV16: body parses");
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, mod_resolve_context(ctx, &err_code), "RSV16: resolve returns OK");
   expect_fields(ctx, "a", "a1,x,a,b", "RSV16: the root is untouched");
   expect_fields(ctx, "b", "b1,x,b,a1,a", "RSV16: b: own, then a's a1 and a (x and b are overridden)");
   expect_fields(ctx, "c", "c1,a,b1,x,b,a1", "RSV16: c: own, then b's, then a's");
   expect_fields(ctx, "d", "d1,b,c1,a,b1,x,a1", "RSV16: d: own, then c's");
   expect_fields(ctx, "e", "e1,a,d1,b,c1,b1,x,a1", "RSV16: e, four deep");
   expect_fields(ctx, "dbl", "q", "RSV16: a field overridden by name is not duplicated");
   /* the inherited field is the ancestor's own statement (aliased), not a copy */
   anvl_statement d = stmt_named(ctx, "d"), c = stmt_named(ctx, "c");
   anvl_statement f = NULL;
   List.get(d->value->object.statements, 2, (object *)&f);
   anvl_statement g = NULL;
   List.get(c->value->object.statements, 0, (object *)&g);
   TestBit.is_true(f == g, "RSV16: d's inherited c1 is c's own statement");
   anvl_statement t = stmt_named(ctx, "dbl"), l = stmt_named(ctx, "lonely");
   List.get(t->value->object.statements, 0, (object *)&f);
   List.get(l->value->object.statements, 0, (object *)&g);
   TestBit.is_true(f != g, "RSV16: dbl keeps its own q");
   mod_ctx_dispose(ctx);
}

/* RSV17 - a base on a nested statement stays 4022, wherever it is written. */
static void test_rsv17_nested_base_stays_4022(void) {
   const char *docs[] = {"#!aml\nb := { x := 1; };\na := { n : b := { y := 1; }; };\n",
                         "#!aml\nb := { x := 1; };\na := [ { n : b := { y := 1; }; } ];\n"};
   for (int i = 0; i < 2; i++) {
      module_context ctx = NULL;
      module_document doc = setup_amp_doc(docs[i], &ctx);
      TestBit.is_not_null(doc, "RSV17: document loaded");
      if (!doc) {
         continue;
      }
      anvl_err_code err_code = ANVL_ERR_NONE;
      (void)doc_parse_body(doc, &err_code);
      TestBit.is_equal_int(ANVL_ERR_PARSER_NESTED_BASE, err_code, "RSV17: err_code reports NESTED_BASE");
      mod_ctx_dispose(ctx);
   }
}

static void expect_ref_cycle_at(const char *src, long line, long col, const char *what) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc(src, &ctx);
   TestBit.is_not_null(doc, what);
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), what);
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, mod_resolve_context(ctx, &err_code), what);
   TestBit.is_equal_int(ANVL_ERR_RESOLVER_REFERENCE_CYCLE, err_code, what);
   anvl_error e = List.size(ctx->errors) ? anvl_error_get(ctx->errors, 0) : NULL;
   TestBit.is_equal_int(line, e ? (long long)e->line : -1, what);
   TestBit.is_equal_int(col, e ? (long long)e->column : -1, what);
   mod_ctx_dispose(ctx);
}

/* RSV18 - 4054 is reported at the first on-cycle `$` in source order, also when the cycle runs through inherited fields
 * and when other references, earlier in the text, are not on a cycle. */
static void test_rsv18_reference_cycle_position(void) {
   /* line 2 holds a harmless reference; the cycle a -> b -> a is first closed at line 4's `$b` */
   expect_ref_cycle_at("#!aml\n"
                       "t := 1; p := { v := $t; };\n"
                       "z := { n := $p; };\n"
                       "a := { x := $b; };\n"
                       "b := { y := $a; };\n",
                       4, 13, "RSV18: first on-cycle reference");
   /* the cycle is closed through a field the derived statement inherits: s derives from base, whose field holds the reference */
   expect_ref_cycle_at("#!aml\n"
                       "base := { r := $s; };\n"
                       "s : base := { w := 1; };\n",
                       2, 16, "RSV18: through an inherited field");
   /* two cycles; the one written first by position is reported, though the later one is built from earlier statements */
   expect_ref_cycle_at("#!aml\n"
                       "m := { k := $n; };\n"
                       "n := { k := $m; };\n"
                       "a := { x := $b; };\n"
                       "b := { y := $a; };\n",
                       2, 13, "RSV18: the earlier of two cycles");
   /* a chain of derived statements, every level holding the reference to the last */
   expect_ref_cycle_at("#!aml\n"
                       "last : mid := { z := 1; };\n"
                       "mid : root := { m := 1; };\n"
                       "root := { r := $last; };\n",
                       4, 16, "RSV18: through a three-deep chain");
}

/* RSV19 - a plain chain resolves to the value at its end for every reference on it; a plain cycle, a chain into a
 * cycle and a chain to a missing name stay unresolved (null); the order of the statements does not matter. */
static void test_rsv19_plain_chain_ends(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!aml\n"
                                       "r0 := $r1;\n"
                                       "r1 := $r2;\n"
                                       "r2 := $r3;\n"
                                       "r3 := { v := 1; };\n"
                                       "r4 := $r2;\n"
                                       "c1 := $c2;\n"
                                       "c2 := $c1;\n"
                                       "in := $c1;\n"
                                       "self := $self;\n"
                                       "gone := $nowhere;\n"
                                       "viagone := $gone;\n"
                                       "late := $r0;\n"
                                       "holder := { a := $r0; b := $r4; c := $c1; d := $in; e := $viagone; f := $late; };\n",
                                       &ctx);
   TestBit.is_not_null(doc, "RSV19: document loaded");
   if (!doc) {
      return;
   }
   TestBit.is_true(parse_all_docs(ctx), "RSV19: body parses");
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, mod_resolve_context(ctx, &err_code), "RSV19: resolve returns OK");
   anvl_value end = stmt_named(ctx, "r3")->value;
   const char *names[] = {"r0", "r1", "r2", "r4", "late"};
   for (int i = 0; i < 5; i++) {
      anvl_statement s = stmt_named(ctx, names[i]);
      TestBit.is_true(s && s->value->type == ANVL_VALUE_VARREF && s->value->varref.resolved == end,
                      "RSV19: a plain chain resolves to the value at its end");
   }
   const char *nulls[] = {"c1", "c2", "in", "self", "gone", "viagone"};
   for (int i = 0; i < 6; i++) {
      anvl_statement s = stmt_named(ctx, nulls[i]);
      TestBit.is_true(s && s->value->type == ANVL_VALUE_VARREF && s->value->varref.resolved == NULL,
                      "RSV19: a cycle, a chain into one or a missing name stays unresolved");
   }
   list fields = stmt_named(ctx, "holder")->value->object.statements;
   bool expect_end[] = {true, true, false, false, false, true};
   for (usize i = 0; i < List.size(fields); i++) {
      anvl_statement f = NULL;
      List.get(fields, i, (object *)&f);
      TestBit.is_true((f->value->varref.resolved == end) == expect_end[i],
                      "RSV19: a reference inside an object resolves like a top-level one");
   }
   mod_ctx_dispose(ctx);
}

/* RSV20 - an inheritance cycle is 4050 however it is reached: a tail into the cycle, a self base. */
static void test_rsv20_inheritance_cycles(void) {
   const char *docs[] = {"#!aml\nt : a := { x := 1; };\na : b := { y := 1; };\nb : a := { z := 1; };\n",
                         "#!aml\ns : s := { x := 1; };\n",
                         "#!aml\nok : root := { x := 1; };\nroot := { r := 1; };\nu : v := { x := 1; };\nv : w := { x := 1; };\nw : u := { x := 1; };\n"};
   for (int i = 0; i < 3; i++) {
      module_context ctx = NULL;
      module_document doc = setup_amp_doc(docs[i], &ctx);
      TestBit.is_not_null(doc, "RSV20: document loaded");
      if (!doc) {
         continue;
      }
      TestBit.is_true(parse_all_docs(ctx), "RSV20: body parses");
      anvl_err_code err_code = ANVL_ERR_NONE;
      TestBit.is_equal_int(ANVL_RES_ERR, mod_resolve_context(ctx, &err_code), "RSV20: resolve returns ERR");
      TestBit.is_equal_int(ANVL_ERR_RESOLVER_CYCLE_DETECTED, err_code, "RSV20: err_code reports CYCLE_DETECTED");
      mod_ctx_dispose(ctx);
   }
}

/* The time of one resolve of a generated document, in seconds, and the result. */
static double time_resolve(const char *src, module_context *out_ctx, bool *ok) {
   module_document doc = setup_amp_doc(src, out_ctx);
   *ok = doc && parse_all_docs(*out_ctx);
   if (!*ok) {
      return 0;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   double t0 = now_secs();
   anvl_result res = mod_resolve_context(*out_ctx, &err_code);
   double dt = now_secs() - t0;
   *ok = res == ANVL_RES_OK;
   return dt;
}

#define BUDGET(secs) (under_valgrind() ? (secs) * 60.0 : (secs))
#define SCALE(n) (under_valgrind() ? (n) / 4 : (n))

static char *grow(char *buf, usize *cap, usize *at, usize need) {
   if (*at + need + 1 > *cap) {
      *cap = (*at + need + 1) * 2;
      buf = realloc(buf, *cap);
   }
   return buf;
}

#define APPEND(...) \
   do { \
      char tmp_[160]; \
      int n_ = snprintf(tmp_, sizeof tmp_, __VA_ARGS__); \
      buf = grow(buf, &cap, &at, (usize)n_); \
      memcpy(buf + at, tmp_, (usize)n_ + 1); \
      at += (usize)n_; \
   } while (0)

/* RSV21 - BR-2611-anvl-resolver-003: a 400-deep chain b_i : b_(i-1), one own field each (13 s before). */
static void test_rsv21_deep_chain_merge_is_linear(void) {
   int n = SCALE(400);
   usize cap = 4096, at = 0;
   char *buf = malloc(cap);
   buf[0] = 0;
   APPEND("#!aml\nb0 := { f0 := 0; };\n");
   for (int i = 1; i < n; i++) {
      APPEND("b%d : b%d := { f%d := %d; };\n", i, i - 1, i, i);
   }
   module_context ctx = NULL;
   bool ok = false;
   double dt = time_resolve(buf, &ctx, &ok);
   TestBit.is_true(ok, "RSV21: resolves");
   char last_name[32];
   snprintf(last_name, sizeof last_name, "b%d", n - 1);
   anvl_statement last = ok ? stmt_named(ctx, last_name) : NULL;
   TestBit.is_equal_int(n, last ? (long long)List.size(last->value->object.statements) : -1,
                        "RSV21: the last statement holds every field of the chain");
   TestBit.is_true(dt < BUDGET(3.0), "RSV21: a 400-deep chain merges within 3 s");
   if (ctx) {
      mod_ctx_dispose(ctx);
   }
   free(buf);
}

/* RSV22 - a base with 1500 reference fields inherited by 1500 statements (9.7 s before). */
static void test_rsv22_wide_base_many_derived_is_linear(void) {
   int n = SCALE(1500);
   usize cap = 4096, at = 0;
   char *buf = malloc(cap);
   buf[0] = 0;
   APPEND("#!aml\nt := 1;\nbase := {\n");
   for (int i = 0; i < n; i++) {
      APPEND("r%d := $t;\n", i);
   }
   APPEND("};\n");
   for (int i = 0; i < n; i++) {
      APPEND("d%d : base := { p := %d; };\n", i, i);
   }
   module_context ctx = NULL;
   bool ok = false;
   double dt = time_resolve(buf, &ctx, &ok);
   TestBit.is_true(ok, "RSV22: resolves");
   char last_name[32];
   snprintf(last_name, sizeof last_name, "d%d", n - 1);
   anvl_statement last = ok ? stmt_named(ctx, last_name) : NULL;
   TestBit.is_equal_int(n + 1, last ? (long long)List.size(last->value->object.statements) : -1,
                        "RSV22: a derived statement holds its own field and every base field");
   TestBit.is_true(dt < BUDGET(4.0), "RSV22: 1500 x 1500 merges within 4 s");
   if (ctx) {
      mod_ctx_dispose(ctx);
   }
   free(buf);
}

/* RSV23 - a 300-deep chain with a 300-reference field at its root (4.0 s before). */
static void test_rsv23_chain_with_reference_root_is_linear(void) {
   int n = SCALE(300);
   usize cap = 4096, at = 0;
   char *buf = malloc(cap);
   buf[0] = 0;
   APPEND("#!aml\nt := 1;\nc0 := { refs := [ ");
   for (int i = 0; i < n; i++) {
      APPEND("$t, ");
   }
   APPEND("1 ]; };\n");
   for (int i = 1; i < n; i++) {
      APPEND("c%d : c%d := { m%d := %d; };\n", i, i - 1, i, i);
   }
   module_context ctx = NULL;
   bool ok = false;
   double dt = time_resolve(buf, &ctx, &ok);
   TestBit.is_true(ok, "RSV23: resolves");
   char last_name[32];
   snprintf(last_name, sizeof last_name, "c%d", n - 1);
   anvl_statement last = ok ? stmt_named(ctx, last_name) : NULL;
   TestBit.is_equal_int(n, last ? (long long)List.size(last->value->object.statements) : -1,
                        "RSV23: the last statement holds the root field and every level's own");
   TestBit.is_true(dt < BUDGET(2.0), "RSV23: the chain with a reference root merges within 2 s");
   if (ctx) {
      mod_ctx_dispose(ctx);
   }
   free(buf);
}

/* RSV24 - FR-2611-anvl-resolver-001 part 2: a plain chain a0 := $a1; ... of 20000 statements (5.3 s before). */
static void test_rsv24_plain_chain_is_linear(void) {
   int n = SCALE(20000);
   usize cap = 4096, at = 0;
   char *buf = malloc(cap);
   buf[0] = 0;
   APPEND("#!aml\n");
   for (int i = 0; i + 1 < n; i++) {
      APPEND("a%d := $a%d;\n", i, i + 1);
   }
   APPEND("a%d := { v := 1; };\n", n - 1);
   module_context ctx = NULL;
   bool ok = false;
   double dt = time_resolve(buf, &ctx, &ok);
   TestBit.is_true(ok, "RSV24: resolves");
   anvl_statement first = ok ? stmt_named(ctx, "a0") : NULL;
   char last_name[32];
   snprintf(last_name, sizeof last_name, "a%d", n - 1);
   anvl_statement last = ok ? stmt_named(ctx, last_name) : NULL;
   TestBit.is_true(first && last && first->value->varref.resolved == last->value,
                   "RSV24: the first reference resolves to the value at the end");
   TestBit.is_true(dt < BUDGET(1.5), "RSV24: a plain chain of 20000 resolves within 1.5 s");
   if (ctx) {
      mod_ctx_dispose(ctx);
   }
   free(buf);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("RSV01_duplicate_name_same_document", NULL,
                  test_rsv01_duplicate_name_same_document, th);
   TestBit.run_ex("RSV02_duplicate_name_across_include", NULL,
                  test_rsv02_duplicate_name_across_include, th);
   TestBit.run_ex("RSV03_varref_resolves_single_hop", NULL,
                  test_rsv03_varref_resolves_single_hop, th);
   TestBit.run_ex("RSV04_varref_chain_resolves_to_final_value", NULL,
                  test_rsv04_varref_chain_resolves_to_final_value, th);
   TestBit.run_ex("RSV05_varref_cycle_resolves_to_null", NULL,
                  test_rsv05_varref_cycle_resolves_to_null, th);
   TestBit.run_ex("RSV06_varref_missing_target_resolves_to_null", NULL,
                  test_rsv06_varref_missing_target_resolves_to_null, th);
   TestBit.run_ex("RSV07_varref_to_block_form_resolves", NULL,
                  test_rsv07_varref_to_block_form_resolves, th);
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

   TestBit.run_ex("RSV15_reference_cycle_rejected", NULL, test_rsv15_reference_cycle_rejected, th);

   TestBit.run_ex("RSV16_merge_order_and_overrides", NULL, test_rsv16_merge_order_and_overrides, th);
   TestBit.run_ex("RSV17_nested_base_stays_4022", NULL, test_rsv17_nested_base_stays_4022, th);
   TestBit.run_ex("RSV18_reference_cycle_position", NULL, test_rsv18_reference_cycle_position, th);
   TestBit.run_ex("RSV19_plain_chain_ends", NULL, test_rsv19_plain_chain_ends, th);
   TestBit.run_ex("RSV20_inheritance_cycles", NULL, test_rsv20_inheritance_cycles, th);
   TestBit.run_ex("RSV21_deep_chain_merge_is_linear", NULL, test_rsv21_deep_chain_merge_is_linear, th);
   TestBit.run_ex("RSV22_wide_base_many_derived_is_linear", NULL, test_rsv22_wide_base_many_derived_is_linear, th);
   TestBit.run_ex("RSV23_chain_with_reference_root_is_linear", NULL,
                  test_rsv23_chain_with_reference_root_is_linear, th);
   TestBit.run_ex("RSV24_plain_chain_is_linear", NULL, test_rsv24_plain_chain_is_linear, th);

   return TestBit.report();
}
