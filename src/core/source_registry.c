/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 *                                                                        *
 * This software is proprietary and confidential. Unauthorized copying,   *
 * distribution, modification, or use of this software, via any medium,   *
 * is strictly prohibited without express written permission from the     *
 * copyright holder.                                                      *
 *                                                                        *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * source_registry.c - Global source hash -> document registry            *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * Created: 2026-08-14                                                    *
 * File: src/core/source_registry.c                                       *
 * ---------------------------------------------------------------------- *
 * Description:                                                           *
 * Process-wide registry mapping source content hashes to module_document *
 * identities. The registry is initialized in mod_new() and cleared when    *
 * the owning module is disposed.                                         *
 * ********************************************************************** */

#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "std.h"
// ----------------
#include <sigma.core/allocator.h>
#include <sigma/map.h>

// Initial capacity of a context's source map. Must be a power-of-two (Sigma map contract).
#define SOURCE_REGISTRY_INITIAL_CAP 16

static bool registry_init(module_context ctx) {
   if (!ctx) {
      return false;
   }
   if (!ctx->sources) {
      ctx->sources = Map.new(SOURCE_REGISTRY_INITIAL_CAP);
   }
   return ctx->sources != NULL;
}

static module_document registry_find(module_context ctx, uint64_t hash) {
   if (!ctx || !ctx->sources || hash == 0) {
      return NULL;
   }
   addr found = 0;
   if (Map.get(ctx->sources, (const char *)&hash, sizeof(hash), &found) == 0) {
      return NULL;
   }
   return (module_document)found;
}

static anvl_result registry_add(module_context ctx, module_document doc,
                                anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!ctx || !doc || !doc->source) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   uint64_t hash = doc->source->hash;
   if (hash == 0) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   if (!registry_init(ctx)) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   if (Map.has(ctx->sources, (const char *)&hash, sizeof(hash))) {
      err_code = ANVL_ERR_SOURCE_ALREADY_LOADED;
      goto error;
   }

   // The map keeps the key POINTER, not a copy: key it by the source's own hash field, which lives
   // exactly as long as the registration does (it is removed before the source is disposed).
   if (Map.set(ctx->sources, (const char *)&doc->source->hash, sizeof(hash), (addr)doc) != 0) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   doc->source->owner = doc;
   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}

static void registry_remove(module_context ctx, uint64_t hash) {
   module_document doc = registry_find(ctx, hash);
   if (!doc) {
      return;
   }
   Map.remove(ctx->sources, (const char *)&hash, sizeof(hash));
   if (doc->source && doc->source->owner == doc) {
      doc->source->owner = NULL;
   }
}

static usize registry_count(module_context ctx) {
   return ctx && ctx->sources ? Map.count(ctx->sources) : 0;
}

static void registry_clear(module_context ctx) {
   if (!ctx || !ctx->sources) {
      return;
   }
   // Clear owners first; the documents themselves are the context's list, not this map's.
   if (ctx->docs) {
      for (usize i = 0, n = List.size(ctx->docs); i < n; i++) {
         module_document d = NULL;
         List.get(ctx->docs, i, (object *)&d);
         if (d && d->source && d->source->owner == d) {
            d->source->owner = NULL;
         }
      }
   }
   Map.dispose(ctx->sources);
   ctx->sources = NULL;
}

// Dispose the map itself. Called from the context's own teardown, after its documents (whose
// doc_dispose already unregistered them), so it must not touch ctx->docs.
static void registry_release(module_context ctx) {
   if (ctx && ctx->sources) {
      Map.dispose(ctx->sources);
      ctx->sources = NULL;
   }
}

const anvl_source_registry_i Registry = {
   .add = registry_add,
   .remove = registry_remove,
   .find = registry_find,
   .count = registry_count,
   .clear = registry_clear,
   .init = registry_init,
   .release = registry_release,
};
