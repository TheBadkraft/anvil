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
 * Context Specification
 * ----------------------------------------------------------------------- */
static const anvl_ctx_spec ANVL_CTX_DEFAULTS = {
   .docs_cap = 5,
   .errs_cap = 5,
   .map_cap = 5,
   .strict_namespace = false,
};
// modify base if context initialized with custom spec
static anvl_ctx_spec ANVL_CTX_BASE = ANVL_CTX_DEFAULTS; // struct copy of defaults

/* ----------------------------------------------------------------------- *
 * Internal context specification resolution functions
 * ----------------------------------------------------------------------- */
// Resolves context specification, returning the default if NULL is provided
anvl_result resolve_context_spec(context_spec *spec_ptr, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_ctx_spec candidate;
   if (out_err_code) {
      *out_err_code = err_code;
   }
   // if the spec_ptr is NULL, return an error state
   if (!spec_ptr) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   // if no spec is provided, return the default spec
   if (!*spec_ptr) {
      *spec_ptr = &ANVL_CTX_BASE;
      return ANVL_RES_OK;
   }

   /* Transactional merge target. */
   candidate = ANVL_CTX_BASE;

   if ((*spec_ptr)->docs_cap) {
      candidate.docs_cap = (*spec_ptr)->docs_cap;
   }
   if ((*spec_ptr)->errs_cap) {
      candidate.errs_cap = (*spec_ptr)->errs_cap;
   }
   if ((*spec_ptr)->map_cap) {
      candidate.map_cap = (*spec_ptr)->map_cap;
   }
   candidate.strict_namespace = (*spec_ptr)->strict_namespace;

   /* Validation gate: fail before commit. */
   if (candidate.docs_cap == 0 || candidate.errs_cap == 0 || candidate.map_cap == 0) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   ANVL_CTX_BASE = candidate; // commit
   *spec_ptr = &ANVL_CTX_BASE;
   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}

/* ----------------------------------------------------------------------- *
 * AnvlMod management                                                      *
 * ----------------------------------------------------------------------- */
#if 1 // module management
anvl_result mod_set_context(AnvlMod mod, module_context ctx) {
   if (!mod || !ctx) {
      return ANVL_RES_ERR;
   }
   mod->context = ctx;
   return ANVL_RES_OK;
}
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
   mod->context = NULL;
   *out_mod = mod;
   return ANVL_RES_OK;

error: {
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
}
anvl_result mod_init_with_context(AnvlMod *out_mod, module_context ctx,
                                  anvl_err_code *out_err_code) {
   anvl_result res = mod_initialize(out_mod, out_err_code);
   anvl_err_code err_code = (out_err_code) ? *out_err_code : ANVL_ERR_NONE;

   // assign provided context
   if (res != ANVL_RES_OK) {
      return res;
   }

   return mod_set_context(*out_mod, ctx);
}
void mod_dispose(AnvlMod *mod_ptr) {
   // stub
   (void)mod_ptr;
}
#endif

/* ----------------------------------------------------------------------- *
 * Context management                                                      *
 * ----------------------------------------------------------------------- */
#if 1 // module_context management
anvl_result mod_ctx_initialize(context_spec ctx_spec, module_context *out_ctx,
                               anvl_err_code *out_err_code) {
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
   // doc_map contract: key = namespace bytes, value = (addr)doc_identity.
   // Keys are caller-owned and must remain valid while present in the map.
   ctx->doc_map = Map.new(5);
   if (!ctx->docs || !ctx->errors || !ctx->doc_map) {
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
      Map.dispose(ctx->doc_map);
      ctx->docs = NULL;
      ctx->errors = NULL;
      ctx->doc_map = NULL;
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
void mod_ctx_dispose(module_context ctx) {
   // stub
   (void)ctx;
}
void mod_ctx_add_doc(module_context ctx, module_document doc) {
   // stub
   (void)ctx;
   (void)doc;
}
void mod_ctx_set_parser(module_context ctx, anvl_parser parser) {
   // stub
   (void)ctx;
   (void)parser;
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
#endif