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
#include <sigma/math.h>
#include <sigma/types.h>

#define ENABLED 0

static void td(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

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
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_DOC_CAP, spec->docs_cap,
                        "CR00a: docs_cap matches default");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_ERR_CAP, spec->errs_cap,
                        "CR00a: errs_cap matches default");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_MAP_CAP, spec->map_cap, "CR00a: map_cap matches default");

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
   anvl_ctx_spec custom = {
      .docs_cap = docs_cap,
      .errs_cap = errs_cap,
      .map_cap = map_cap,
   };
   context_spec spec = &custom;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = resolve_context_spec(&spec, &err_code);

   TestBit.is_equal_int(ANVL_RES_OK, res, "CR00c: resolve_context_spec returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR00c: err_code remains NONE");
   TestBit.is_not_null(spec, "CR00c: resolved spec is non-NULL");
   TestBit.is_equal_int(docs_cap, (long long)spec->docs_cap, "CR00c: docs_cap committed");
   TestBit.is_equal_int(errs_cap, (long long)spec->errs_cap, "CR00c: errs_cap committed");
   TestBit.is_equal_int(16, (long long)spec->map_cap, "CR00c: map_cap normalized and committed");
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
 * CR01a — mod_ctx_default_initialize success path
 * Commentary: verifies the contract for successful context creation
 * and the expected initialized graph state. NULL spec resolves to default
 * specification values.
 * ---------------------------------------------------------------------- */
static void test_cr01a_mod_ctx_default_init_success(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_ctx_initialize(NULL, &ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR01a: mod_ctx_initialize returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR01a: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(ctx, "CR01a: out context is allocated");
   TestBit.is_not_null(ctx->docs, "CR01a: context docs list is initialized");
   TestBit.is_not_null(ctx->errors, "CR01a: context errors list is initialized");
   TestBit.is_equal_int(0, (long long)List.size(ctx->docs), "CR01a: docs list starts empty");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_DOC_CAP, (long long)List.capacity(ctx->docs),
                        "CR01a: docs list capacity matches default");
   TestBit.is_equal_int(0, (long long)List.size(ctx->errors), "CR01a: errors list starts empty");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_ERR_CAP, (long long)List.capacity(ctx->errors),
                        "CR01a: errors list capacity matches default");

   // Debug.dispose_ctx(ctx);
   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * CR01b — mod_ctx_initialize with custom spec
 * Commentary: verifies that a custom context spec is honored and
 * that the resulting context is initialized with the expected capacities.
 * ---------------------------------------------------------------------- */
static void test_cr01b_mod_ctx_custom_spec_init(void) {
   static int docs_cap = 10;
   static int errs_cap = 15;
   static int map_cap = 16; // must be power-of-two for map

   anvl_ctx_spec custom = {
      .docs_cap = docs_cap,
      .errs_cap = errs_cap,
      .map_cap = map_cap,
   };
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_ctx_initialize(&custom, &ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR01b: mod_ctx_initialize returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR01b: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(ctx, "CR01b: out context is allocated");
   TestBit.is_not_null(ctx->docs, "CR01b: context docs list is initialized");
   TestBit.is_not_null(ctx->errors, "CR01b: context errors list is initialized");
   TestBit.is_equal_int(0, (long long)List.size(ctx->docs), "CR01b: docs list starts empty");
   TestBit.is_equal_int(docs_cap, (long long)List.capacity(ctx->docs),
                        "CR01b: docs list capacity matches custom spec");
   TestBit.is_equal_int(0, (long long)List.size(ctx->errors), "CR01b: errors list starts empty");
   TestBit.is_equal_int(errs_cap, (long long)List.capacity(ctx->errors),
                        "CR01b: errors list capacity matches custom spec");

   // sanity check: the default spec should match the new base spec after a custom spec is used
   context_spec base_spec = NULL;
   anvl_result base_res = resolve_context_spec(&base_spec, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, base_res,
                        "CR01b: resolve_context_spec returns ANVL_RES_OK for base spec");
   TestBit.is_equal_int(docs_cap, (long long)base_spec->docs_cap,
                        "CR01b: base spec docs_cap matches custom spec");
   TestBit.is_equal_int(errs_cap, (long long)base_spec->errs_cap,
                        "CR01b: base spec errs_cap matches custom spec");
   TestBit.is_equal_int(map_cap, (long long)base_spec->map_cap,
                        "CR01b: base spec map_cap matches custom spec");

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
 * CR05a — mod_initialize success path
 * Commentary: verifies module allocation, attached context creation with
 * default spec, and default root state.
 * ---------------------------------------------------------------------- */
static void test_cr05a_mod_init_success(void) {
   AnvlMod mod = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_initialize(&mod, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR05a: mod_initialize returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR05a: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(mod, "CR05a: module object is allocated");
   TestBit.is_not_null(mod->context, "CR05a: module context is initialized");
   TestBit.is_true(mod->root == NULL, "CR05a: module root is NULL before first load");
   TestBit.is_not_null(mod->context->docs, "CR05a: module context docs list exists");
   TestBit.is_not_null(mod->context->errors, "CR05a: module context errors list exists");

   module_context ctx = mod->context;
   TestBit.is_equal_int(0, (long long)List.size(ctx->docs), "CR05a: docs list starts empty");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_DOC_CAP, (long long)List.capacity(ctx->docs),
                        "CR05a: docs list capacity matches default");
   TestBit.is_equal_int(0, (long long)List.size(ctx->errors), "CR05a: errors list starts empty");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_ERR_CAP, (long long)List.capacity(ctx->errors),
                        "CR05a: errors list capacity matches default");

   Debug.dispose_mod(mod);
}
/* ---------------------------------------------------------------------- *
 * CR05b — mod_initialize success path w/ custom context spec
 * Commentary: verifies module allocation, attached context creation with
 * custom spec, and default root state.
 * ---------------------------------------------------------------------- */
static void test_cr05b_mod_init_success(void) {
   static int docs_cap = 15;
   static int errs_cap = 20;
   static int map_cap = 15;
   anvl_ctx_spec custom = {
      .docs_cap = docs_cap,
      .errs_cap = errs_cap,
      .map_cap = map_cap,
   };

   context_spec spec = &custom;
   anvl_err_code err_code = ANVL_ERR_NONE;

   // set the default context
   (void)resolve_context_spec(&spec, &err_code);

   AnvlMod mod = NULL;
   // initialize the module
   anvl_result res = mod_initialize(&mod, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR05b: mod_initialize returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR05b: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(mod, "CR05b: module object is allocated");
   TestBit.is_not_null(mod->context, "CR05b: module context is initialized");
   TestBit.is_true(mod->root == NULL, "CR05b: module root is NULL before first load");
   TestBit.is_not_null(mod->context->docs, "CR05b: module context docs list exists");
   TestBit.is_not_null(mod->context->errors, "CR05b: module context errors list exists");

   module_context ctx = mod->context;
   TestBit.is_equal_int(0, (long long)List.size(ctx->docs), "CR05b: docs list starts empty");
   TestBit.is_equal_int(docs_cap, (long long)List.capacity(ctx->docs),
                        "CR05b: docs list capacity matches default");
   TestBit.is_equal_int(0, (long long)List.size(ctx->errors), "CR05b: errors list starts empty");
   TestBit.is_equal_int(errs_cap, (long long)List.capacity(ctx->errors),
                        "CR05b: errors list capacity matches default");

   Debug.dispose_mod(mod);
}
/* ---------------------------------------------------------------------- *
 * CR05c — mod_new minimum member initialization
 * Commentary: verifies that the module object is allocated and that
 * the context remains uninitialized.
 * ---------------------------------------------------------------------- */
static void test_cr05c_mod_new_success(void) {
   AnvlMod mod = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_new(&mod, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR05c: mod_initialize returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR05c: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(mod, "CR05c: module object is allocated");
   TestBit.is_null(mod->context, "CR05c: module context is NULL");
   TestBit.is_null(mod->root, "CR05c: module root is NULL");

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
/* ----------------------------------------------------------------------
 * CR09 — mod_attach_context: both mod & context are valid
 * Commentary: attach valid context to intialized (new) module
 * ---------------------------------------------------------------------- */
static void test_cr09_mod_attach_context_success(void) {
   AnvlMod mod = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_new(&mod, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR09: mod_new returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR09: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(mod, "CR09: module object is allocated");
   TestBit.is_null(mod->context, "CR09: module context is NULL");

   module_context ctx = NULL;
   res = mod_ctx_initialize(NULL, &ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR09: mod_ctx_initialize returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR09: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(ctx, "CR09: context object is allocated");

   res = mod_attach_context(&mod, ctx);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR09: mod_attach_context returns ANVL_RES_OK");
   TestBit.is_true(mod->context == ctx,
                   "CR09: module context pointer matches attached context pointer");

   Debug.dispose_mod(mod);
}
/* ---------------------------------------------------------------------- *
 * CR09a — mod_attach_context negative: NULL mod pointer
 * Commentary: attach should fail when module pointer destination is NULL.
 * ---------------------------------------------------------------------- */
static void test_cr09a_mod_attach_context_null_mod_ptr(void) {
   module_context ctx = (module_context)(uintptr_t)0x1;
   anvl_result res = mod_attach_context(NULL, ctx);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "CR09a: mod_attach_context returns ANVL_RES_ERR for NULL mod pointer");
}
/* ---------------------------------------------------------------------- *
 * CR09b — mod_attach_context negative: NULL module handle
 * Commentary: attach should fail when module handle is NULL.
 * ---------------------------------------------------------------------- */
static void test_cr09b_mod_attach_context_null_mod_handle(void) {
   AnvlMod mod = NULL;
   module_context ctx = (module_context)(uintptr_t)0x1;
   anvl_result res = mod_attach_context(&mod, ctx);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "CR09b: mod_attach_context returns ANVL_RES_ERR for NULL module handle");
}
/* ---------------------------------------------------------------------- *
 * CR09c — mod_attach_context negative: NULL context
 * Commentary: attach should fail when context is NULL.
 * ---------------------------------------------------------------------- */
static void test_cr09c_mod_attach_context_null_ctx(void) {
   AnvlMod mod = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_new(&mod, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR09c: mod_new returns ANVL_RES_OK");
   TestBit.is_not_null(mod, "CR09c: module object is allocated");

   res = mod_attach_context(&mod, NULL);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "CR09c: mod_attach_context returns ANVL_RES_ERR for NULL context");
   TestBit.is_null(mod->context, "CR09c: module context remains NULL after failed attach");

   Debug.dispose_mod(mod);
}
/* ---------------------------------------------------------------------- *
 * CR09d — mod_attach_context self-attach is no-op success
 * Commentary: attaching the currently assigned context should succeed
 * without replacing the pointer.
 * ---------------------------------------------------------------------- */
static void test_cr09d_mod_attach_context_self_attach_noop(void) {
   AnvlMod mod = NULL;
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_new(&mod, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR09d: mod_new returns ANVL_RES_OK");
   TestBit.is_not_null(mod, "CR09d: module object is allocated");

   res = mod_ctx_initialize(NULL, &ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR09d: mod_ctx_initialize returns ANVL_RES_OK");
   TestBit.is_not_null(ctx, "CR09d: context object is allocated");

   res = mod_attach_context(&mod, ctx);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR09d: first attach returns ANVL_RES_OK");
   TestBit.is_true(mod->context == ctx, "CR09d: first attach sets context pointer");

   res = mod_attach_context(&mod, ctx);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR09d: self-attach returns ANVL_RES_OK");
   TestBit.is_true(mod->context == ctx,
                   "CR09d: self-attach preserves the existing context pointer");

   Debug.dispose_mod(mod);
}
/* ---------------------------------------------------------------------- *
 * CR10 — mod_ctx_clear_docs disposes docs and list
 * Commentary: validates batch cleanup behavior for owned docs list.
 * ---------------------------------------------------------------------- */
static void test_cr10_mod_ctx_clear_docs(void) {
   list docs = List.new(2, sizeof(module_document));
   TestBit.is_not_null(docs, "CR10: docs list allocated");

   module_document d1 = Debug.stub_doc(NULL);
   module_document d2 = Debug.stub_doc(NULL);
   TestBit.is_not_null(d1, "CR10: first stub doc allocated");
   TestBit.is_not_null(d2, "CR10: second stub doc allocated");

   if (!docs || !d1 || !d2) {
      if (d1)
         Allocator.dispose(d1);
      if (d2)
         Allocator.dispose(d2);
      if (docs)
         List.dispose(docs);
      TestBit.fail("CR10: setup failed");
      return;
   }

   TestBit.is_equal_int(0, (long long)List.append(docs, (object)d1),
                        "CR10: append first doc succeeds");
   TestBit.is_equal_int(0, (long long)List.append(docs, (object)d2),
                        "CR10: append second doc succeeds");
   TestBit.is_equal_int(2, (long long)List.size(docs), "CR10: docs list contains two entries");

   mod_ctx_clear_docs(docs);
}
/* ---------------------------------------------------------------------- *
 * CR11 — mod_ctx_clear_errs disposes errors and list
 * Commentary: validates batch cleanup behavior for accumulated
 * parser/runtime error state.
 * ---------------------------------------------------------------------- */
static void test_cr11_mod_ctx_clear_errs(void) {
   list errs = List.new(2, sizeof(anvl_error));
   TestBit.is_not_null(errs, "CR11: error list allocated");

   anvl_error e1 = Debug.stub_err(ANVL_ERR_INVALID_ARGUMENT);
   anvl_error e2 = Debug.stub_err(ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
   TestBit.is_not_null(e1, "CR11: first error state allocated");
   TestBit.is_not_null(e2, "CR11: second error state allocated");

   if (!errs || !e1 || !e2) {
      if (e1)
         Allocator.dispose(e1);
      if (e2)
         Allocator.dispose(e2);
      if (errs)
         List.dispose(errs);
      TestBit.fail("CR11: setup failed");
      return;
   }

   TestBit.is_equal_int(0, (long long)List.append(errs, (object)e1),
                        "CR11: append first error succeeds");
   TestBit.is_equal_int(0, (long long)List.append(errs, (object)e2),
                        "CR11: append second error succeeds");
   TestBit.is_equal_int(2, (long long)List.size(errs), "CR11: error list contains two entries");

   mod_ctx_clear_errs(errs);
}
/* ---------------------------------------------------------------------- *
 * CR13 — mod_ctx_set_parser stores parser on context
 * Commentary: codifies parser attachment behavior for the context
 * lifecycle API (may be RED until implemented).
 * ---------------------------------------------------------------------- */
static void test_cr13_mod_ctx_set_parser(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("CR13: setup context failed");
      return;
   }

   TestBit.is_true(ctx->parser == NULL, "CR13: parser starts NULL");

   anvl_parser p = (anvl_parser)(uintptr_t)0x1;
   mod_ctx_set_parser(ctx, p);

   TestBit.is_true(ctx->parser == p, "CR13: parser pointer is assigned to context");

   Debug.dispose_ctx(ctx);
}
/* ---------------------------------------------------------------------- *
 * CR14 — mod_ctx_dispose handles both NULL and populated contexts
 * Commentary: explicit lifecycle disposal contract for context-
 * level ownership (may be RED until implemented).
 * ---------------------------------------------------------------------- */
static void test_cr14_mod_ctx_dispose(void) {
   // Negative/no-op variant: disposing NULL should not crash.
   mod_ctx_dispose(NULL);

   // Positive variant: disposing a populated context should be safe.
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("CR14: setup context failed");
      return;
   }

   module_document doc = Debug.stub_doc(NULL);
   anvl_error err = Debug.stub_err(ANVL_ERR_MEMORY_ALLOC_FAILED);
   if (!doc || !err) {
      if (doc)
         Debug.dispose_doc(doc);
      if (err)
         Debug.dispose_err(err);
      Debug.dispose_ctx(ctx);
      TestBit.fail("CR14: setup allocations failed");
      return;
   }

   TestBit.is_equal_int(0, (long long)List.append(ctx->docs, (object)doc),
                        "CR14: append doc succeeds");
   TestBit.is_equal_int(0, (long long)List.append(ctx->errors, (object)err),
                        "CR14: append error succeeds");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * CR15 — mod_dispose handles both NULL and initialized modules
 * Commentary: explicit lifecycle disposal contract for module-level
 * ownership (may be RED until implemented).
 * ---------------------------------------------------------------------- */
static void test_cr15_mod_dispose(void) {
   // Negative/no-op variant: disposing NULL module pointer should not crash.
   mod_dispose(NULL);

   // Positive variant: initialized module should be disposed and nulled.
   AnvlMod mod = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_initialize(&mod, &err_code) || !mod) {
      TestBit.fail("CR15: setup module failed");
      return;
   }

   mod_dispose(&mod);
   TestBit.is_true(mod == NULL, "CR15: mod_dispose nulls caller module pointer");
}

/* ---------------------------------------------------------------------- *
 * CR16 — mod_dispose releases the source registry
 * Commentary: disposing the module should release the global source
 * registry reference; when the reference count reaches zero, the registry
 * is cleared and registered documents are no longer findable.
 * ---------------------------------------------------------------------- */
static void test_cr16_mod_dispose_releases_registry(void) {
   AnvlMod mod = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_initialize(&mod, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR16: mod_initialize returns OK");
   TestBit.is_not_null(mod, "CR16: module allocated");

   module_document doc = Debug.stub_doc(NULL);
   Source.create(&doc->source, &err_code);
   const char *buffer = "name := test\n";
   Source.from_buffer(&doc->source, buffer, strlen(buffer), &err_code);
   uint64_t hash = Source.hash(doc->source);

   res = mod_ctx_register_doc(mod->context, doc, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR16: register doc returns OK");
   TestBit.is_equal_int(1, (long long)Registry.count(), "CR16: registry contains one document");

   mod_dispose(&mod);
   TestBit.is_true(mod == NULL, "CR16: mod_dispose nulls caller module pointer");
   TestBit.is_equal_int(0, (long long)Registry.count(),
                        "CR16: registry is empty after module disposal");
   TestBit.is_null(Registry.find(hash), "CR16: document no longer findable after disposal");
}
/* ---------------------------------------------------------------------- *
 * CR17 — mod_ctx_create_arena creates a usable, shared arena
 * Commentary: the arena is shared across every document in ctx->docs (see
 * notes/document-body-parse.md "Arena-backed allocation"), not per-document —
 * this test only checks that the context ends up with a working bump
 * allocator, not that individual documents draw from it correctly (that's
 * Source.get_arena's job, tested in test_source.c).
 * ---------------------------------------------------------------------- */
static void test_cr17_mod_ctx_create_arena(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_ctx_initialize(NULL, &ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR17: mod_ctx_initialize returns OK");
   TestBit.is_not_null(ctx, "CR17: context allocated");
   if (!ctx) {
      return;
   }
   TestBit.is_null(ctx->arena, "CR17: arena is NULL before creation");

   res = mod_ctx_create_arena(ctx, 1024, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR17: create_arena returns OK");
   TestBit.is_not_null(ctx->arena, "CR17: arena allocated on the context");

   if (ctx->arena) {
      void *p = ctx->arena->alloc(ctx->arena, 64);
      TestBit.is_not_null(p, "CR17: arena hands out usable memory");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * CR18 — mod_ctx_dispose releases the arena
 * Commentary: TestBit can't assert "the memory was freed" directly (ctx
 * itself is gone after dispose) — the real signal for this one is
 * `make module val` reporting 0 bytes in use at exit. The in-test
 * assertions only cover that dispose doesn't crash, with and without an
 * arena present, and that using the arena before disposal works normally.
 * ---------------------------------------------------------------------- */
static void test_cr18_mod_ctx_dispose_releases_arena(void) {
   // Variant 1: context with a populated, used arena.
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("CR18: setup context failed");
      return;
   }

   TestBit.is_equal_int(ANVL_RES_OK, mod_ctx_create_arena(ctx, 256, &err_code),
                        "CR18: create_arena returns OK");
   if (ctx->arena) {
      // Allocate enough to span more than one chained block, so disposal has
      // to walk the whole chain, not just free a single block.
      for (usize i = 0; i < 8; i++) {
         (void)ctx->arena->alloc(ctx->arena, 512);
      }
   }

   mod_ctx_dispose(ctx); // real leak-freedom signal is `make module val`, not an assertion here

   // Variant 2: a context whose arena was never created — dispose must still
   // be a safe no-op on the arena field.
   module_context ctx2 = NULL;
   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx2, &err_code) || !ctx2) {
      TestBit.fail("CR18: setup second context failed");
      return;
   }
   TestBit.is_null(ctx2->arena, "CR18: second context's arena is NULL (never created)");
   mod_ctx_dispose(ctx2); // must not crash on a NULL arena
   TestBit.is_true(true, "CR18: dispose with no arena created completes without crashing");
}
/* ---------------------------------------------------------------------- *
 * CR19 — mod_ctx_initialize creates the statements/values node indexes
 * Commentary: see notes/document-body-parse.md "Arena node iteration". These
 * lists hold pointers into the arena, populated by Source.new_node; here we
 * only lock in that mod_ctx_initialize/mod_ctx_dispose manage their lifetime
 * correctly, independent of Source.new_node's own (still RED) behavior.
 * ---------------------------------------------------------------------- */
static void test_cr19_mod_ctx_node_indexes_created(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   TestBit.is_equal_int(ANVL_RES_OK, mod_ctx_initialize(NULL, &ctx, &err_code),
                        "CR19: mod_ctx_initialize returns OK");
   TestBit.is_not_null(ctx, "CR19: context allocated");
   if (!ctx) {
      return;
   }

   TestBit.is_not_null(ctx->statements, "CR19: statements index created");
   TestBit.is_not_null(ctx->values, "CR19: values index created");
   TestBit.is_equal_int(0, (long long)List.size(ctx->statements),
                        "CR19: statements index starts empty");
   TestBit.is_equal_int(0, (long long)List.size(ctx->values),
                        "CR19: values index starts empty");

   mod_ctx_dispose(ctx); // real leak-freedom signal is `make module val`, not an assertion here
}
/* ---------------------------------------------------------------------- *
 * CR20 — mod_ctx_arena_size_hint applies the multiplier/floor heuristic
 * Commentary: see notes/document-body-parse.md "Initial sizing heuristic".
 * Pure arithmetic — no context needed. RED until the stub (src/core/module.c)
 * is replaced with max(sum * ANVL_ARENA_SIZE_MULTIPLIER, ANVL_ARENA_MIN_SIZE).
 * ---------------------------------------------------------------------- */
static void test_cr20_mod_ctx_arena_size_hint(void) {
   // Small sum: multiplier result lands under the floor, floor wins.
   usize small_sum = 100;
   TestBit.is_equal_int((long long)ANVL_ARENA_MIN_SIZE,
                        (long long)mod_ctx_arena_size_hint(small_sum),
                        "CR20: small sum is floored to ANVL_ARENA_MIN_SIZE");

   // Large sum: multiplier result exceeds the floor, multiplier wins.
   usize large_sum = 40000;
   TestBit.is_equal_int((long long)(large_sum * ANVL_ARENA_SIZE_MULTIPLIER),
                        (long long)mod_ctx_arena_size_hint(large_sum),
                        "CR20: large sum uses sum * ANVL_ARENA_SIZE_MULTIPLIER");

   // Boundary: sum * multiplier lands exactly on the floor.
   usize boundary_sum = ANVL_ARENA_MIN_SIZE / ANVL_ARENA_SIZE_MULTIPLIER;
   TestBit.is_equal_int((long long)ANVL_ARENA_MIN_SIZE,
                        (long long)mod_ctx_arena_size_hint(boundary_sum),
                        "CR20: sum landing exactly on the floor returns the floor");

   // Degenerate: zero sum still returns the floor, never zero.
   TestBit.is_equal_int((long long)ANVL_ARENA_MIN_SIZE, (long long)mod_ctx_arena_size_hint(0),
                        "CR20: zero sum is floored to ANVL_ARENA_MIN_SIZE");
}
/* ---------------------------------------------------------------------- *
 * CR21 — full sequencing: header scan through arena creation, one real
 * multi-document import graph, no orchestration function exists yet so this
 * test chains the real pieces manually, exactly as the eventual production
 * sequencing point (mod_load_imports -> mod_ctx_arena_size_hint ->
 * mod_ctx_create_arena) will. RED until CR20's stub is implemented — nothing
 * else in this chain needs to change for this test to go GREEN.
 * ---------------------------------------------------------------------- */
static void test_cr21_full_sequencing_arena_ready(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_import_diamond.anvl", &ctx);
   TestBit.is_not_null(root, "CR21: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_scan_header(root, &err_code),
                        "CR21: root header scan returns OK");

   usize size_hint = 0;
   TestBit.is_equal_int(ANVL_RES_OK, mod_load_imports(ctx, root, &size_hint, &err_code),
                        "CR21: load imports returns OK");
   TestBit.is_equal_int(3, (long long)List.size(ctx->docs),
                        "CR21: three documents registered (root + base + types)");
   TestBit.is_true(size_hint > 0, "CR21: size hint is non-zero after loading the import graph");

   usize capacity = mod_ctx_arena_size_hint(size_hint);
   TestBit.is_equal_int(ANVL_RES_OK, mod_ctx_create_arena(ctx, capacity, &err_code),
                        "CR21: create_arena returns OK using the computed size hint");
   TestBit.is_not_null(ctx->arena, "CR21: arena is ready — every document loaded, sized, allocated");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * CR22 — the arena is genuinely usable at this point, from any document in
 * the graph, not just the root: allocate a statement via the root's source
 * and a value via a child document's source, both landing on the same
 * shared ctx->statements/ctx->values indexes. This is the concrete proof
 * that "one arena, shared across the whole import graph" (the design
 * decision behind putting the arena on module_context, not module_document)
 * actually holds. RED until CR20's stub is implemented.
 * ---------------------------------------------------------------------- */
static void test_cr22_ready_for_parser_new_node(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_import_diamond.anvl", &ctx);
   TestBit.is_not_null(root, "CR22: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   doc_scan_header(root, &err_code);

   usize size_hint = 0;
   mod_load_imports(ctx, root, &size_hint, &err_code);

   usize capacity = mod_ctx_arena_size_hint(size_hint);
   mod_ctx_create_arena(ctx, capacity, &err_code);
   TestBit.is_not_null(ctx->arena, "CR22: arena ready before allocating any nodes");
   if (!ctx->arena) {
      mod_ctx_dispose(ctx);
      return;
   }

   module_document child = NULL;
   List.get(ctx->docs, 1, (object *)&child);
   TestBit.is_not_null(child, "CR22: a child document is available from the loaded import graph");

   err_code = ANVL_ERR_NONE;
   void *stmt = Source.new_node(root->source, ANVL_NODE_STATEMENT, &err_code);
   TestBit.is_not_null(stmt, "CR22: statement node allocated via the root document's source");

   err_code = ANVL_ERR_NONE;
   void *value = child ? Source.new_node(child->source, ANVL_NODE_VALUE, &err_code) : NULL;
   TestBit.is_not_null(value, "CR22: value node allocated via a child document's source");

   TestBit.is_equal_int(1, (long long)List.size(ctx->statements),
                        "CR22: root's statement lands on the context's shared statements index");
   TestBit.is_equal_int(1, (long long)List.size(ctx->values),
                        "CR22: child's value lands on the same shared context's values index");

   mod_ctx_dispose(ctx);
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

   TestBit.run_ex("test_cr01a_mod_ctx_default_init_success", NULL,
                  test_cr01a_mod_ctx_default_init_success, td);
   TestBit.run_ex("test_cr01b_mod_ctx_custom_spec_init", NULL, test_cr01b_mod_ctx_custom_spec_init,
                  td);

   TestBit.run_ex("CR02_mod_ctx_init_null_out_ctx", NULL, test_cr02_mod_ctx_init_null_out_ctx, td);
   TestBit.run_ex("CR03_mod_ctx_init_null_out_err", NULL, test_cr03_mod_ctx_init_null_out_err, td);
   TestBit.run_ex("CR04_mod_ctx_init_null_outputs", NULL, test_cr04_mod_ctx_init_null_outputs, td);

   TestBit.run_ex("CR05a_mod_init_success", NULL, test_cr05a_mod_init_success, td);
   TestBit.run_ex("CR05b_mod_init_success", NULL, test_cr05b_mod_init_success, td);
   TestBit.run_ex("CR05c_mod_new_success", NULL, test_cr05c_mod_new_success, td);

   TestBit.run_ex("CR06_mod_init_null_out_mod", NULL, test_cr06_mod_init_null_out_mod, td);
   TestBit.run_ex("CR07_mod_init_null_out_err", NULL, test_cr07_mod_init_null_out_err, td);
   TestBit.run_ex("CR08_mod_init_null_outputs", NULL, test_cr08_mod_init_null_outputs, td);

   TestBit.run_ex("CR09_mod_attach_context_success", NULL, test_cr09_mod_attach_context_success,
                  td);
   TestBit.run_ex("CR09a_mod_attach_context_null_mod_ptr", NULL,
                  test_cr09a_mod_attach_context_null_mod_ptr, td);
   TestBit.run_ex("CR09b_mod_attach_context_null_mod_handle", NULL,
                  test_cr09b_mod_attach_context_null_mod_handle, td);
   TestBit.run_ex("CR09c_mod_attach_context_null_ctx", NULL, test_cr09c_mod_attach_context_null_ctx,
                  td);
   TestBit.run_ex("CR09d_mod_attach_context_self_attach_noop", NULL,
                  test_cr09d_mod_attach_context_self_attach_noop, td);

   TestBit.run_ex("CR10_mod_ctx_clear_docs", NULL, test_cr10_mod_ctx_clear_docs, td);
   TestBit.run_ex("CR11_mod_ctx_clear_errs", NULL, test_cr11_mod_ctx_clear_errs, td);

   TestBit.run_ex("CR13_mod_ctx_set_parser", NULL, test_cr13_mod_ctx_set_parser, td);
   TestBit.run_ex("CR14_mod_ctx_dispose", NULL, test_cr14_mod_ctx_dispose, td);
   TestBit.run_ex("CR15_mod_dispose", NULL, test_cr15_mod_dispose, td);
   TestBit.run_ex("CR16_mod_dispose_releases_registry", NULL,
                  test_cr16_mod_dispose_releases_registry, td);
   TestBit.run_ex("CR17_mod_ctx_create_arena", NULL, test_cr17_mod_ctx_create_arena, td);
   TestBit.run_ex("CR18_mod_ctx_dispose_releases_arena", NULL,
                  test_cr18_mod_ctx_dispose_releases_arena, td);
   TestBit.run_ex("CR19_mod_ctx_node_indexes_created", NULL,
                  test_cr19_mod_ctx_node_indexes_created, td);
   TestBit.run_ex("CR20_mod_ctx_arena_size_hint", NULL, test_cr20_mod_ctx_arena_size_hint, td);
   TestBit.run_ex("CR21_full_sequencing_arena_ready", NULL, test_cr21_full_sequencing_arena_ready,
                  td);
   TestBit.run_ex("CR22_ready_for_parser_new_node", NULL, test_cr22_ready_for_parser_new_node, td);

   return TestBit.report();
}
