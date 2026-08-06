/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_module.c - Unit tests for module/context management               *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_module.c                                          *
 * ********************************************************************** */

#include "anvil.h"
#include "types.h"
#include "internal/module.h"
#include "internal/source.h"
#include "testbit.h"
#include "std.h"
// ----------------
#include "../utilities/debug.h"
#include <sigma/list.h>
#include <sigma/types.h>

#define ENABLED 0

static void td(void) {}

static anvl_ctx_spec ANVL_CTX_DEFAULTS = {
   .docs_cap = 5,
   .errs_cap = 5,
   .map_cap = 5,
   .strict_namespace = false,
};

/* ---------------------------------------------------------------------- *
 * CR00 — mod_ctx_spec_resolution
 * Commentary: verifies that the default context spec is used when NULL is
 * provided to mod_ctx_initialize().
 * ---------------------------------------------------------------------- */
static void test_cr00a_mod_ctx_spec_default_resolution(void) {
   // return the default spec when NULL is provided
   context_spec spec = NULL;
   context_spec dup_spec = NULL;

   anvl_result exp_res = ANVL_RES_OK;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = resolve_context_spec(&spec, &err_code);
   TestBit.is_not_null(spec, "CR00a: resolve_context_spec returns non-NULL spec for NULL input");
   TestBit.is_equal_int(ANVL_CTX_DEFAULTS.docs_cap, spec->docs_cap,
                        "CR00a: docs_cap matches default");
   TestBit.is_equal_int(ANVL_CTX_DEFAULTS.errs_cap, spec->errs_cap,
                        "CR00a: errs_cap matches default");
   TestBit.is_equal_int(ANVL_CTX_DEFAULTS.map_cap, spec->map_cap, "CR00a: map_cap matches default");
   TestBit.is_equal_int(ANVL_CTX_DEFAULTS.strict_namespace, spec->strict_namespace,
                        "CR00a: strict_namespace matches default");

   // verify that the returned spec pointer is the same as the default spec
   res = resolve_context_spec(&dup_spec, &err_code);
   TestBit.is_equal_int((long long)dup_spec, (long long)spec,
                        "CR00a: returns pointer to default spec");
   TestBit.is_equal_int(exp_res, res, "CR00a: returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR00a: err_code == ANVL_ERR_NONE");
}
static void test_cr00b_mod_ctx_spec_null_spec_resolution(void) {
   // return an error when the spec_ptr itself is NULL
   anvl_result res = resolve_context_spec(NULL, NULL);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "CR00b: resolve_context_spec returns ANVL_RES_ERR for NULL spec_ptr");
}
/* ---------------------------------------------------------------------- *
 * CR00c — resolve_context_spec full override commits base
 * ---------------------------------------------------------------------- */
static void test_cr00c_mod_ctx_spec_full_override_commits_base(void) {
   static int docs_cap = 15;
   static int errs_cap = 20;
   static int map_cap = 15;
   static bool strict_namespace = true;
   anvl_ctx_spec custom = {
      .docs_cap = docs_cap,
      .errs_cap = errs_cap,
      .map_cap = map_cap,
      .strict_namespace = strict_namespace,
   };
   context_spec spec = &custom;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = resolve_context_spec(&spec, &err_code);

   TestBit.is_equal_int(ANVL_RES_OK, res, "CR00c: resolve_context_spec returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR00c: err_code remains NONE");
   TestBit.is_not_null(spec, "CR00c: resolved spec is non-NULL");
   TestBit.is_equal_int(docs_cap, (long long)spec->docs_cap, "CR00c: docs_cap committed");
   TestBit.is_equal_int(errs_cap, (long long)spec->errs_cap, "CR00c: errs_cap committed");
   TestBit.is_equal_int(map_cap, (long long)spec->map_cap, "CR00c: map_cap committed");
   TestBit.is_equal_int(strict_namespace, (long long)spec->strict_namespace,
                        "CR00c: strict_namespace committed");
}
/* ---------------------------------------------------------------------- *
 * CR00d — resolve_context_spec partial override preserves non-overridden
 * ---------------------------------------------------------------------- */
static void test_cr00d_mod_ctx_spec_partial_override(void) {
   context_spec before = NULL;
   context_spec after = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   // effectively a `set_context_spec` operation
   (void)resolve_context_spec(&before, &err_code);

   anvl_ctx_spec partial = {
      .docs_cap = before->docs_cap + 5, // override
      // .errs_cap = 0,                    preserve
      // .map_cap = 0,                     preserve
      .strict_namespace = !before->strict_namespace,
   };

   after = &partial;
   anvl_result res = resolve_context_spec(&after, &err_code);

   TestBit.is_equal_int(ANVL_RES_OK, res, "CR00d: resolve_context_spec returns OK");
   TestBit.is_equal_int((long long)(partial.docs_cap), (long long)after->docs_cap,
                        "CR00d: docs_cap updated");
   TestBit.is_equal_int((long long)before->errs_cap, (long long)after->errs_cap,
                        "CR00d: errs_cap preserved");
   TestBit.is_equal_int((long long)before->map_cap, (long long)after->map_cap,
                        "CR00d: map_cap preserved");
   TestBit.is_equal_int((long long)partial.strict_namespace, (long long)after->strict_namespace,
                        "CR00d: strict_namespace updated");
}
/* ---------------------------------------------------------------------- *
 * CR00f — resolve_context_spec allows NULL out_err_code
 * ---------------------------------------------------------------------- */
static void test_cr00e_mod_ctx_spec_null_out_err(void) {
   context_spec spec = NULL;
   anvl_result res = resolve_context_spec(&spec, NULL);

   TestBit.is_equal_int(ANVL_RES_OK, res, "CR00f: resolve_context_spec returns OK");
   TestBit.is_not_null(spec, "CR00f: resolved spec is non-NULL");
}

/* ---------------------------------------------------------------------- *
 * CR01 — mod_ctx_default_initialize success path
 * Commentary: verifies the contract for successful context creation
 * and the expected initialized graph state.
 * ---------------------------------------------------------------------- */
static void test_cr01_mod_ctx_default_init_success(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_ctx_initialize(NULL, &ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR01: mod_ctx_initialize returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR01: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(ctx, "CR01: out context is allocated");
   TestBit.is_not_null(ctx->docs, "CR01: context docs list is initialized");
   TestBit.is_not_null(ctx->errors, "CR01: context errors list is initialized");
   TestBit.is_not_null(ctx->doc_map, "CR01: context doc_map is initialized");
   TestBit.is_equal_int(0, (long long)List.size(ctx->docs), "CR01: docs list starts empty");
   TestBit.is_equal_int(0, (long long)List.size(ctx->errors), "CR01: errors list starts empty");
   TestBit.is_equal_int(0, (long long)Map.count(ctx->doc_map), "CR01: doc_map starts empty");

   // Debug.dispose_ctx(ctx);
   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * CR02 — mod_ctx_initialize negative: out_ctx is NULL
 * Commentary: invalid destination pointer should fail fast and
 * emit ANVL_ERR_INVALID_ARGUMENT.
 * ---------------------------------------------------------------------- */
static void test_cr02_mod_ctx_init_null_out_ctx(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_ctx_initialize(NULL, NULL, &err_code);

   TestBit.is_equal_int(ANVL_RES_ERR, res, "CR02: mod_ctx_initialize returns ANVL_RES_ERR");
   TestBit.is_equal_int(ANVL_ERR_INVALID_ARGUMENT, err_code,
                        "CR02: null out_ctx maps to ANVL_ERR_INVALID_ARGUMENT");
}
/* ---------------------------------------------------------------------- *
 * CR03 — mod_ctx_initialize negative: out_err_code is NULL
 * Commentary: caller must provide an error sink for deterministic
 * failure reporting.
 * ---------------------------------------------------------------------- */
static void test_cr03_mod_ctx_init_null_out_err(void) {
   module_context ctx = NULL;
   anvl_result res = mod_ctx_initialize(NULL, &ctx, NULL);

   TestBit.is_equal_int(ANVL_RES_ERR, res, "CR03: mod_ctx_initialize returns ANVL_RES_ERR");
   TestBit.is_true(ctx == NULL, "CR03: out_ctx remains NULL when out_err_code is NULL");
}
/* ---------------------------------------------------------------------- *
 * CR04 — mod_ctx_initialize negative: both outputs are NULL
 * Commentary: fully invalid invocation should return ANVL_RES_ERR.
 * ---------------------------------------------------------------------- */
static void test_cr04_mod_ctx_init_null_outputs(void) {
   anvl_result res = mod_ctx_initialize(NULL, NULL, NULL);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "CR04: mod_ctx_initialize rejects null outputs");
}
/* ---------------------------------------------------------------------- *
 * CR05 — mod_initialize success path
 * Commentary: verifies module allocation, attached context creation,
 * and default root state.
 * ---------------------------------------------------------------------- */
static void test_cr05_mod_init_success(void) {
   AnvlMod mod = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_initialize(&mod, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR05: mod_initialize returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR05: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(mod, "CR05: module object is allocated");
   TestBit.is_not_null(mod->context, "CR05: module context is initialized");
   TestBit.is_true(mod->root == NULL, "CR05: module root is NULL before first load");
   TestBit.is_not_null(mod->context->docs, "CR05: module context docs list exists");
   TestBit.is_not_null(mod->context->errors, "CR05: module context errors list exists");

   Debug.dispose_mod(mod);
}
/* ---------------------------------------------------------------------- *
 * CR06 — mod_initialize negative: out_mod is NULL
 * Commentary: invalid destination should be rejected with a clear
 * argument error code.
 * ---------------------------------------------------------------------- */
static void test_cr06_mod_init_null_out_mod(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_initialize(NULL, &err_code);

   TestBit.is_equal_int(ANVL_RES_ERR, res, "CR06: mod_initialize returns ANVL_RES_ERR");
   TestBit.is_equal_int(ANVL_ERR_INVALID_ARGUMENT, err_code,
                        "CR06: null out_mod maps to ANVL_ERR_INVALID_ARGUMENT");
}
/* ---------------------------------------------------------------------- *
 * CR07 — mod_initialize negative: out_err_code is NULL
 * Commentary: required error sink is missing; call must fail and
 * not leak partial module state.
 * ---------------------------------------------------------------------- */
static void test_cr07_mod_init_null_out_err(void) {
   AnvlMod mod = (AnvlMod)(uintptr_t)0x1;
   anvl_result res = mod_initialize(&mod, NULL);

   TestBit.is_equal_int(ANVL_RES_ERR, res, "CR07: mod_initialize returns ANVL_RES_ERR");
   TestBit.is_true(mod == NULL, "CR07: out_mod is reset to NULL on failure");
}
/* ---------------------------------------------------------------------- *
 * CR08 — mod_initialize negative: both outputs are NULL
 * Commentary: fully invalid invocation should fail predictably.
 * ---------------------------------------------------------------------- */
static void test_cr08_mod_init_null_outputs(void) {
   anvl_result res = mod_initialize(NULL, NULL);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "CR08: mod_initialize rejects null outputs");
}
/* ---------------------------------------------------------------------- *
 * CR09 — mod_ctx_clear_docs disposes docs and list
 * Commentary: validates batch cleanup behavior for owned docs list.
 * ---------------------------------------------------------------------- */
static void test_cr09_mod_ctx_clear_docs(void) {
   list docs = List.new(2, sizeof(module_document));
   TestBit.is_not_null(docs, "CR09: docs list allocated");

   module_document d1 = Debug.stub_doc(NULL, NULL);
   module_document d2 = Debug.stub_doc(NULL, NULL);
   TestBit.is_not_null(d1, "CR09: first stub doc allocated");
   TestBit.is_not_null(d2, "CR09: second stub doc allocated");

   if (!docs || !d1 || !d2) {
      if (d1)
         Allocator.dispose(d1);
      if (d2)
         Allocator.dispose(d2);
      if (docs)
         List.dispose(docs);
      TestBit.fail("CR09: setup failed");
      return;
   }

   TestBit.is_equal_int(0, (long long)List.append(docs, (object)d1),
                        "CR09: append first doc succeeds");
   TestBit.is_equal_int(0, (long long)List.append(docs, (object)d2),
                        "CR09: append second doc succeeds");
   TestBit.is_equal_int(2, (long long)List.size(docs), "CR09: docs list contains two entries");

   mod_ctx_clear_docs(docs);
}
/* ---------------------------------------------------------------------- *
 * CR10 — mod_ctx_clear_errs disposes errors and list
 * Commentary: validates batch cleanup behavior for accumulated
 * parser/runtime error state.
 * ---------------------------------------------------------------------- */
static void test_cr10_mod_ctx_clear_errs(void) {
   list errs = List.new(2, sizeof(anvl_error));
   TestBit.is_not_null(errs, "CR10: error list allocated");

   anvl_error e1 = Debug.stub_err(ANVL_ERR_INVALID_ARGUMENT);
   anvl_error e2 = Debug.stub_err(ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
   TestBit.is_not_null(e1, "CR10: first error state allocated");
   TestBit.is_not_null(e2, "CR10: second error state allocated");

   if (!errs || !e1 || !e2) {
      if (e1)
         Allocator.dispose(e1);
      if (e2)
         Allocator.dispose(e2);
      if (errs)
         List.dispose(errs);
      TestBit.fail("CR10: setup failed");
      return;
   }

   TestBit.is_equal_int(0, (long long)List.append(errs, (object)e1),
                        "CR10: append first error succeeds");
   TestBit.is_equal_int(0, (long long)List.append(errs, (object)e2),
                        "CR10: append second error succeeds");
   TestBit.is_equal_int(2, (long long)List.size(errs), "CR10: error list contains two entries");

   mod_ctx_clear_errs(errs);
}
/* ---------------------------------------------------------------------- *
 * CR11 — mod_ctx_add_doc appends to context docs
 * Commentary: codifies the intended behavior for future context API
 * completion (currently may fail in RED state if unimplemented).
 * ---------------------------------------------------------------------- */
static void test_cr11_mod_ctx_add_doc(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("CR11: setup context failed");
      return;
   }

   module_document doc = Debug.stub_doc(NULL, NULL);
   TestBit.is_not_null(doc, "CR11: stub doc allocated");
   if (!doc) {
      Debug.dispose_ctx(ctx);
      return;
   }

   mod_ctx_add_doc(ctx, doc);

   TestBit.is_equal_int(1, (long long)List.size(ctx->docs), "CR11: docs list size increments to 1");

   module_document out = NULL;
   TestBit.is_equal_int(0, (long long)List.get(ctx->docs, 0, (object *)&out),
                        "CR11: list.get returns OK for first inserted doc");
   TestBit.is_true(out == doc, "CR11: stored doc pointer matches inserted doc pointer");

   Debug.dispose_ctx(ctx);
}
/* ---------------------------------------------------------------------- *
 * CR12 — mod_ctx_set_parser stores parser on context
 * Commentary: codifies parser attachment behavior for the context
 * lifecycle API (may be RED until implemented).
 * ---------------------------------------------------------------------- */
static void test_cr12_mod_ctx_set_parser(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("CR12: setup context failed");
      return;
   }

   TestBit.is_true(ctx->parser == NULL, "CR12: parser starts NULL");

   anvl_parser p = (anvl_parser)(uintptr_t)0x1;
   mod_ctx_set_parser(ctx, p);

   TestBit.is_true(ctx->parser == p, "CR12: parser pointer is assigned to context");

   Debug.dispose_ctx(ctx);
}
/* ---------------------------------------------------------------------- *
 * CR13 — mod_ctx_dispose handles both NULL and populated contexts
 * Commentary: explicit lifecycle disposal contract for context-
 * level ownership (may be RED until implemented).
 * ---------------------------------------------------------------------- */
static void test_cr13_mod_ctx_dispose(void) {
   // Negative/no-op variant: disposing NULL should not crash.
   mod_ctx_dispose(NULL);

   // Positive variant: disposing a populated context should be safe.
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("CR13: setup context failed");
      return;
   }

   module_document doc = Debug.stub_doc(NULL, NULL);
   anvl_error err = Debug.stub_err(ANVL_ERR_MEMORY_ALLOC_FAILED);
   if (!doc || !err) {
      if (doc)
         Debug.dispose_doc(doc);
      if (err)
         Debug.dispose_err(err);
      Debug.dispose_ctx(ctx);
      TestBit.fail("CR13: setup allocations failed");
      return;
   }

   TestBit.is_equal_int(0, (long long)List.append(ctx->docs, (object)doc),
                        "CR13: append doc succeeds");
   TestBit.is_equal_int(0, (long long)List.append(ctx->errors, (object)err),
                        "CR13: append error succeeds");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * CR14 — mod_dispose handles both NULL and initialized modules
 * Commentary: explicit lifecycle disposal contract for module-level
 * ownership (may be RED until implemented).
 * ---------------------------------------------------------------------- */
static void test_cr14_mod_dispose(void) {
   // Negative/no-op variant: disposing NULL module pointer should not crash.
   mod_dispose(NULL);

   // Positive variant: initialized module should be disposed and nulled.
   AnvlMod mod = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_initialize(&mod, &err_code) || !mod) {
      TestBit.fail("CR14: setup module failed");
      return;
   }

   mod_dispose(&mod);
   TestBit.is_true(mod == NULL, "CR14: mod_dispose nulls caller module pointer");
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("CR00a_mod_ctx_spec_default_resolution", NULL,
                  test_cr00a_mod_ctx_spec_default_resolution, td);
   TestBit.run_ex("CR00b_mod_ctx_spec_null_err_resolution", NULL,
                  test_cr00b_mod_ctx_spec_null_spec_resolution, td);
   TestBit.run_ex("CR00c_mod_ctx_spec_full_override_commits_base", NULL,
                  test_cr00c_mod_ctx_spec_full_override_commits_base, td);
   TestBit.run_ex("CR00d_mod_ctx_spec_partial_override", NULL,
                  test_cr00d_mod_ctx_spec_partial_override, td);
   TestBit.run_ex("CR00e_mod_ctx_spec_null_out_err", NULL, test_cr00e_mod_ctx_spec_null_out_err,
                  td);

#if ENABLED
   TestBit.run_ex("CR01_mod_ctx_init_success", NULL, test_cr01_mod_ctx_init_success, td);
   TestBit.run_ex("CR02_mod_ctx_init_null_out_ctx", NULL, test_cr02_mod_ctx_init_null_out_ctx, td);
   TestBit.run_ex("CR03_mod_ctx_init_null_out_err", NULL, test_cr03_mod_ctx_init_null_out_err, td);
   TestBit.run_ex("CR04_mod_ctx_init_null_outputs", NULL, test_cr04_mod_ctx_init_null_outputs, td);

   TestBit.run_ex("CR05_mod_init_success", NULL, test_cr05_mod_init_success, td);
   TestBit.run_ex("CR06_mod_init_null_out_mod", NULL, test_cr06_mod_init_null_out_mod, td);
   TestBit.run_ex("CR07_mod_init_null_out_err", NULL, test_cr07_mod_init_null_out_err, td);
   TestBit.run_ex("CR08_mod_init_null_outputs", NULL, test_cr08_mod_init_null_outputs, td);

   TestBit.run_ex("CR09_mod_ctx_clear_docs", NULL, test_cr09_mod_ctx_clear_docs, td);
   TestBit.run_ex("CR10_mod_ctx_clear_errs", NULL, test_cr10_mod_ctx_clear_errs, td);
   TestBit.run_ex("CR11_mod_ctx_add_doc", NULL, test_cr11_mod_ctx_add_doc, td);
   TestBit.run_ex("CR12_mod_ctx_set_parser", NULL, test_cr12_mod_ctx_set_parser, td);
   TestBit.run_ex("CR13_mod_ctx_dispose", NULL, test_cr13_mod_ctx_dispose, td);
   TestBit.run_ex("CR14_mod_dispose", NULL, test_cr14_mod_dispose, td);
#endif

   return TestBit.report();
}

/* Manual lifecycle helpers now live in test/utilities/debug.c */
