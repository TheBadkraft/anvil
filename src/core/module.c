/* *********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium,    *
 * is strictly prohibited without express written permission from the      *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * module.c - Module implementation for Anvil                              *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2025-07-22                                                     *
 * File: src/core/module.c                                                 *
 * *********************************************************************** */

#include "internal/module.h"
#include "types.h"
#include "constants.h"
#include "std.h"

static ssize_t mod_size = sizeof(struct anvl_mod_t);
static ssize_t ctx_size = sizeof(struct anvl_mod_ctx_t);

/* ----------------------------------------------------------------------- *
 * AnvlMod management                                                      *
 * ----------------------------------------------------------------------- */
anvl_result mod_initialize(AnvlMod *out_mod, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   AnvlMod mod = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!out_mod || !out_err_code) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   *out_mod = NULL;

   mod = Allocator.alloc(mod_size);
   if (!mod) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(mod, 0, mod_size);

   // initialize module context here
   if (ANVL_RES_OK != mod_ctx_initialize(&mod->context, &err_code)) {
      goto error;
   }
   *out_mod = mod;
   return ANVL_RES_OK;

error:
   Allocator.dispose(mod);
   mod = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (out_mod) {
      *out_mod = mod;
   }
   return ANVL_RES_ERR;
}

/* ----------------------------------------------------------------------- *
 * module_context management                                               *
 * ----------------------------------------------------------------------- */
anvl_result mod_ctx_initialize(module_context *out_ctx, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   module_context ctx = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!out_ctx || !out_err_code) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   *out_ctx = NULL;

   ctx = Allocator.alloc(ctx_size);
   if (!ctx) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(ctx, 0, ctx_size);

   ctx->docs = List.new(5, sizeof(module_document));
   ctx->errors = List.new(5, sizeof(anvl_error));
   if (!ctx->docs || !ctx->errors) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   *out_ctx = ctx;
   *out_err_code = err_code;
   return ANVL_RES_OK;

error:
   if (ctx) {
      List.dispose(ctx->docs);
      List.dispose(ctx->errors);
      ctx->docs = NULL;
      ctx->errors = NULL;
      Allocator.dispose(ctx);
   }
   ctx = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (out_ctx) {
      *out_ctx = ctx;
   }
   return ANVL_RES_ERR;
}
void mod_ctx_clear_docs(list docs) {
   // iterate through the list and dispose of each module_document
   for (usize i = 0; i < List.size(docs); i++) {
      module_document doc = NULL;
      List.get(docs, i, (object *)&doc);
      if (doc) {
         doc_dispose(doc);
      }
   }
   List.dispose(docs);
}
void mod_ctx_clear_errs(list errors) {
   for (usize i = 0; i < List.size(errors); i++) {
      anvl_error err = NULL;
      List.get(errors, i, (object *)&err);
      if (err) {
         Allocator.dispose(err);
      }
   }
   List.dispose(errors);
}