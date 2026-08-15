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

#include "internal/constants.h"
#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "types.h"
#include "constants.h"
#include "std.h"
#include <sigma/math.h>
#include <sigma/strings.h>

static ssize_t mod_size = sizeof(struct anvl_mod_t);
static ssize_t ctx_size = sizeof(struct anvl_mod_ctx_t);

/* ----------------------------------------------------------------------- *
 * Context Specification
 * ----------------------------------------------------------------------- */
static const anvl_ctx_spec ANVL_CTX_DEFAULTS = {
   .docs_cap = ANVL_CTX_DEFAULT_DOC_CAP,
   .errs_cap = ANVL_CTX_DEFAULT_ERR_CAP,
   .map_cap = ANVL_CTX_DEFAULT_MAP_CAP, // must be power-of-two for map
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
   usize normalized_map_cap = 0;
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

   if (Math.normalize_pow_2_min_checked(candidate.map_cap, 8, &normalized_map_cap) != SC_MATH_OK) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   candidate.map_cap = normalized_map_cap;

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
anvl_result mod_initialize(AnvlMod *out_mod, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   AnvlMod mod = NULL;

   module_context ctx;
   // we treat a NULL ctx_spec as the user saying they do not want a copy returned
   context_spec spec = NULL;
   if (mod_new(out_mod ? &mod : NULL, out_err_code) == ANVL_RES_OK &&
       mod_ctx_initialize(spec, &ctx, out_err_code) == ANVL_RES_OK &&
       mod_attach_context(&mod, ctx) == ANVL_RES_OK) {

      mod->root = NULL;
      *out_mod = mod;
   } else {
      err_code = out_err_code ? *out_err_code : ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   if (!mod) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   return ANVL_RES_OK;

error: {
   mod_dispose(&mod);

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (out_mod) {
      *out_mod = mod;
   }
   return ANVL_RES_ERR;
}
}
anvl_result mod_new(AnvlMod *out_mod, anvl_err_code *out_err_code) {
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
   mod->root = NULL;

   // Ensure the process-wide source registry exists before any document is
   // registered. The registry is cleared in mod_dispose().
   Registry.init();

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
void mod_dispose(AnvlMod *mod_ptr) {
   if (!mod_ptr || !*mod_ptr) {
      return;
   }
   AnvlMod mod = *mod_ptr;
   if (mod->context) {
      mod_ctx_dispose(mod->context);
   }

   // Release this module's reference to the global source registry. When the
   // reference count reaches zero, the registry is cleared. Documents
   // unregister themselves individually during context disposal.
   Registry.release();

   Allocator.dispose(mod);
   *mod_ptr = NULL;
}
anvl_result mod_attach_context(AnvlMod *mod, module_context ctx) {
   module_context old_ctx = NULL;
   if (!mod || !*mod || !ctx) {
      goto error;
   }

   if ((*mod)->context == ctx) {
      return ANVL_RES_OK;
   }

   old_ctx = (*mod)->context;
   (*mod)->context = ctx;

   if (old_ctx) {
      mod_ctx_dispose(old_ctx);
   }

   return ANVL_RES_OK;

error:
   return ANVL_RES_ERR;
}
#endif

/* ----------------------------------------------------------------------- *
 * Context management                                                      *
 * ----------------------------------------------------------------------- */
#if 1 // module_context management
// if context_spec is NULL, use default values
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

   if (ANVL_RES_OK != resolve_context_spec(&ctx_spec, &err_code) || !ctx_spec) {
      goto error;
   }

   ctx = Allocator.alloc(ctx_size);
   if (!ctx) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(ctx, 0, ctx_size);

   ctx->docs = List.new(ctx_spec->docs_cap, sizeof(module_document));
   ctx->errors = List.new(ctx_spec->errs_cap, sizeof(anvl_error));
   if (!ctx->docs || !ctx->errors) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   *out_ctx = ctx;
   *out_err_code = err_code;
   return ANVL_RES_OK;

error: {
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
}
void mod_ctx_dispose(module_context ctx) {
   if (!ctx) {
      return;
   }
   mod_ctx_clear_docs(ctx->docs);
   mod_ctx_clear_errs(ctx->errors);
   ctx->docs = NULL;
   ctx->errors = NULL;

   Allocator.dispose(ctx);
}
anvl_result mod_ctx_register_doc(module_context ctx, module_document doc, const char *filepath,
                                 anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!ctx || !doc || !doc->source) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   if (Source.hash(doc->source) == 0) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   if (Registry.find(Source.hash(doc->source)) != NULL) {
      err_code = ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT;
      goto error;
   }

   doc->context = ctx;
   if (filepath) {
      if (doc->filepath) {
         String.dispose(doc->filepath);
      }
      doc->filepath = String.copy((string)filepath);
      if (!doc->filepath) {
         err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
         goto error;
      }
   }

   if (Registry.add(doc, &err_code) != ANVL_RES_OK) {
      goto error;
   }

   List.append(ctx->docs, (object)doc);

   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}
void mod_ctx_set_parser(module_context ctx, anvl_parser parser) {
   // make sure we have a valid context and document
   if (!ctx || !parser) {
      return;
   }
   ctx->parser = parser;
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

void mod_ctx_add_doc(module_context ctx, module_document doc) {
   if (!ctx || !doc) {
      return;
   }

   List.append(ctx->docs, (object)doc);
}
#endif