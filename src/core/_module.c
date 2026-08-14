/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * module.c - Module implementation for Anvil
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * Created: 2026-07-22
 * File: src/core/module.c
 * ----------------------------------------------------------------------- *
 */

// LEGACY (2026-08-09): pre-refactor implementation kept for comparison.
// Do not compile. Active implementation is in src/core/module.c.

 #include "internal/module.h"
 #include "types.h"
 #include "utils.h"
 #include "std.h"
// ----------------------------------------------------------------- 
#include <sigma/memory.h>

 static ssize_t mod_size = sizeof(struct anvl_mod_t);

/* ----------------------------------------------------------------- *
 * anvl_mod management                                                *
 * ----------------------------------------------------------------- */
bool mod_init(AnvlMod *out_mod) {
  // initialize locals to avoid compiler tics
  AnvlMod mod = NULL;

  // do we have a valid pointer?
  if (!out_mod)
    goto error;

  // clear the output pointer to NULL in case of failure
  *out_mod = NULL;

  // allocate memory to a local mod variable
  mod = Allocator.alloc(mod_size);
  if (!mod)
    goto error;

  // zero out the allocated memory
  memset(mod, 0, mod_size);

  // initialize the mod's context
  if (!mod_ctx_init(&mod->context)) {
    goto error;
  }

  // happy path - assign the output pointer to the newly created mod
  (*out_mod) = mod;
  return true;

error:
  // cleanup in case of error
  if (mod) {
    Allocator.dispose(mod);
  }
  if (out_mod) {
    *out_mod = NULL;
  }

  return false;
}
void mod_dispose(AnvlMod *mod) {
  // guard against null variable
  if (!mod || !(*mod))
    return;

  // dereference to local variable
  AnvlMod m = *mod;

  // dispose of internal mambers
  mod_ctx_dispose(m->context);
  m->context = NULL;

  // free the memory
  Allocator.dispose(m);

  // clear the caller's pointer to avoid dangling pointer
  *mod = NULL;
}
void mod_ctx_set_parser(anvl_ctx ctx, parser parser) {
  if (!ctx || !parser)
    return;

  ctx->parser = parser;
}
/* ----------------------------------------------------------------- *
 * anvl_ctx management                                                *
 * ----------------------------------------------------------------- */
bool mod_ctx_init(anvl_ctx *out_ctx) {
  *out_ctx = NULL;
  ssize_t size = sizeof(struct anvl_mod_ctx_t);
  anvl_ctx ctx = Allocator.alloc(size);
  if (ctx) {
    memset(ctx, 0, size);
    ctx->docs = List.new(5, sizeof(anvl_doc));
    ctx->errors = List.new(5, sizeof(anvl_error_state));

    if (!ctx->docs || !ctx->errors)
      goto error;

    goto exit;
  }

error:
  List.dispose(ctx->docs);
  List.dispose(ctx->errors);
  ctx->docs = NULL;
  ctx->errors = NULL;
  Allocator.dispose(ctx);
  ctx = NULL;

exit:
  *out_ctx = ctx;
  return ctx != NULL;
}
void mod_ctx_dispose(anvl_ctx ctx) {
  if (!ctx)
    return;

  if (ctx->docs) {
    mod_docs_clear(ctx->docs);
    mod_errs_clear(ctx->errors);
    ctx->docs = NULL;
    ctx->errors = NULL;
  }

  Allocator.dispose(ctx);
}
void mod_ctx_add_doc(anvl_ctx ctx, anvl_doc doc) {
  if (!ctx || !doc)
    return;

  List.append(ctx->docs, doc);
  doc->context = ctx; // set the context for the document
}

 