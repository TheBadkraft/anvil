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
#include <sigma/allocator.h>
#include <sigma/map.h>
#include <sigma/memory.h>

// Initial capacity for the global source registry. Must be a power-of-two
// to satisfy the Sigma map contract.
#define SOURCE_REGISTRY_INITIAL_CAP 64

// Process-wide registry map: hash (uint64_t) -> module_document.
static map source_registry = NULL;

// Number of live modules referencing the registry. The registry is cleared
// when this count reaches zero.
static usize module_ref_count = 0;

static void registry_ensure_initialized(void) {
   if (!source_registry) {
      source_registry = Map.new(SOURCE_REGISTRY_INITIAL_CAP);
   }
}

static void registry_init(void) {
   registry_ensure_initialized();
   module_ref_count++;
}

static void registry_release(void) {
   if (module_ref_count == 0) {
      return;
   }

   module_ref_count--;
   if (module_ref_count == 0 && source_registry) {
      Map.dispose(source_registry);
      source_registry = NULL;
   }
}

static anvl_result registry_add(module_document doc, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!doc || !doc->source) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   uint64_t *hash = &doc->source->hash;
   if (*hash == 0) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   registry_ensure_initialized();
   if (!source_registry) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   if (Map.has(source_registry, (const char *)hash, sizeof(*hash))) {
      err_code = ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT;
      goto error;
   }

   if (Map.set(source_registry, (const char *)hash, sizeof(*hash), (addr)doc) != 0) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}

static void registry_remove(uint64_t hash) {
   if (!source_registry || hash == 0) {
      return;
   }

   Map.remove(source_registry, (const char *)&hash, sizeof(hash));
}

static module_document registry_find(uint64_t hash) {
   if (!source_registry || hash == 0) {
      return NULL;
   }

   addr found = 0;
   if (Map.get(source_registry, (const char *)&hash, sizeof(hash), &found) == 0) {
      return NULL;
   }

   return (module_document)found;
}

static usize registry_count(void) {
   if (!source_registry) {
      return 0;
   }

   return Map.count(source_registry);
}

static void registry_clear(void) {
   if (source_registry) {
      Map.dispose(source_registry);
      source_registry = NULL;
   }
   module_ref_count = 0;
}

const anvl_source_registry_i Registry = {
   .init = registry_init,
   .add = registry_add,
   .remove = registry_remove,
   .find = registry_find,
   .count = registry_count,
   .release = registry_release,
   .clear = registry_clear,
};
