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
#include "testbit.h"
#include "std.h"
// ----------------
#include "../utilities/debug.h"
#include "../utilities/helpers.h"
#include <sigma/list.h>
#include <sigma/math.h>
#include <sigma/types.h>

#define ENABLED 0

static void td(void) { (void)reset_context_spec_defaults(NULL); }

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
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_STRICT_NAMESPACE, spec->strict_namespace,
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
   TestBit.is_equal_int(16, (long long)spec->map_cap, "CR00c: map_cap normalized and committed");
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
   TestBit.is_not_null(ctx->doc_map, "CR01a: context doc_map is initialized");
   TestBit.is_equal_int(0, (long long)List.size(ctx->docs), "CR01a: docs list starts empty");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_DOC_CAP, (long long)List.capacity(ctx->docs),
                        "CR01a: docs list capacity matches default");
   TestBit.is_equal_int(0, (long long)List.size(ctx->errors), "CR01a: errors list starts empty");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_ERR_CAP, (long long)List.capacity(ctx->errors),
                        "CR01a: errors list capacity matches default");
   TestBit.is_equal_int(0, (long long)Map.count(ctx->doc_map), "CR01a: doc_map starts empty");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_MAP_CAP, (long long)Map.capacity(ctx->doc_map),
                        "CR01a: doc_map capacity matches default");

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
      .strict_namespace = true,
   };
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = mod_ctx_initialize(&custom, &ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR01b: mod_ctx_initialize returns ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR01b: err_code remains ANVL_ERR_NONE");
   TestBit.is_not_null(ctx, "CR01b: out context is allocated");
   TestBit.is_not_null(ctx->docs, "CR01b: context docs list is initialized");
   TestBit.is_not_null(ctx->errors, "CR01b: context errors list is initialized");
   TestBit.is_not_null(ctx->doc_map, "CR01b: context doc_map is initialized");
   TestBit.is_equal_int(0, (long long)List.size(ctx->docs), "CR01b: docs list starts empty");
   TestBit.is_equal_int(docs_cap, (long long)List.capacity(ctx->docs),
                        "CR01b: docs list capacity matches custom spec");
   TestBit.is_equal_int(0, (long long)List.size(ctx->errors), "CR01b: errors list starts empty");
   TestBit.is_equal_int(errs_cap, (long long)List.capacity(ctx->errors),
                        "CR01b: errors list capacity matches custom spec");
   TestBit.is_equal_int(0, (long long)Map.count(ctx->doc_map), "CR01b: doc_map starts empty");
   TestBit.is_equal_int(map_cap, (long long)Map.capacity(ctx->doc_map),
                        "CR01b: doc_map capacity matches custom spec");

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
   TestBit.is_equal_int(true, (long long)base_spec->strict_namespace,
                        "CR01b: base spec strict_namespace matches custom spec");

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
   TestBit.is_equal_int(0, (long long)Map.count(ctx->doc_map), "CR05a: doc_map starts empty");
   TestBit.is_equal_int(ANVL_CTX_DEFAULT_MAP_CAP, (long long)Map.capacity(ctx->doc_map),
                        "CR05a: doc_map capacity matches default");

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
   static bool strict_namespace = true;
   anvl_ctx_spec custom = {
      .docs_cap = docs_cap,
      .errs_cap = errs_cap,
      .map_cap = map_cap,
      .strict_namespace = strict_namespace,
   };

   context_spec spec = &custom;
   anvl_err_code err_code = ANVL_ERR_NONE;
   int exp_map_cap = Math.next_pow_2(map_cap);

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
   TestBit.is_equal_int(0, (long long)Map.count(ctx->doc_map), "CR05b: doc_map starts empty");
   TestBit.is_equal_int(exp_map_cap, (long long)Map.capacity(ctx->doc_map),
                        "CR05b: doc_map capacity matches default");

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

   module_document d1 = Debug.stub_doc(NULL, NULL);
   module_document d2 = Debug.stub_doc(NULL, NULL);
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
 * CR12 — mod_ctx_add_doc appends to context docs
 * Commentary: codifies the intended behavior for future context API
 * completion (currently may fail in RED state if unimplemented).
 * ---------------------------------------------------------------------- */
static void test_cr12_mod_ctx_add_doc(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("CR12: setup context failed");
      return;
   }

   module_document doc = Debug.stub_doc(NULL, NULL);
   TestBit.is_not_null(doc, "CR12: stub doc allocated");
   if (!doc) {
      Debug.dispose_ctx(ctx);
      return;
   }

   mod_ctx_add_doc(ctx, doc);

   TestBit.is_equal_int(1, (long long)List.size(ctx->docs), "CR12: docs list size increments to 1");

   module_document out = NULL;
   TestBit.is_equal_int(0, (long long)List.get(ctx->docs, 0, (object *)&out),
                        "CR12: list.get returns OK for first inserted doc");
   TestBit.is_true(out == doc, "CR12: stored doc pointer matches inserted doc pointer");

   Debug.dispose_ctx(ctx);
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

   module_document doc = Debug.stub_doc(NULL, NULL);
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
   TestBit.run_ex("CR12_mod_ctx_add_doc", NULL, test_cr12_mod_ctx_add_doc, td);
   TestBit.run_ex("CR13_mod_ctx_set_parser", NULL, test_cr13_mod_ctx_set_parser, td);
   TestBit.run_ex("CR14_mod_ctx_dispose", NULL, test_cr14_mod_ctx_dispose, td);
   TestBit.run_ex("CR15_mod_dispose", NULL, test_cr15_mod_dispose, td);

   return TestBit.report();
}

/* Manual lifecycle helpers now live in test/utilities/debug.c */
