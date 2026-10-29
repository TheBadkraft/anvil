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
 * internal/source_registry.h - Global source hash -> document registry   *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * Created: 2026-08-14                                                    *
 * File: include/internal/source_registry.h                               *
 * ---------------------------------------------------------------------- *
 * Description:                                                           *
 * This header defines a process-wide registry mapping source content     *
 * hashes to module_document identities. The registry enables source-     *
 * centric lookups: any code with a source hash can recover the owning    *
 * document and its context without threading context pointers through    *
 * every layer.                                                           *
 * ********************************************************************** */
#pragma once

#include "types.h"
#include "errors.h"
// ----------------
#include <sigma/types.h>

/* ---------------------------------------------------------------------- *
 * Source registry interface
 * ----------------------------------------------------------------------
 * Context-scoped (BR-2610-anvl-registry-001). Each module_context owns a hash -> document map
 * (ctx->sources) that deduplicates documents within ONE load's include graph; independent loads
 * have independent contexts and never see each other. There is no process-global state: a source
 * reaches its document through anvl_source_t::owner, which `add` sets and `remove` clears.
 * ---------------------------------------------------------------------- */
typedef struct anvl_source_registry_i {
   /**
    * @brief Register a document in `ctx` under its source hash and make it its source's owner.
    * @param ctx The context whose include graph the document belongs to.
    * @param doc The document to register. Must have a loaded source with a non-zero hash.
    * @param[out] out_err_code ANVL_ERR_SOURCE_ALREADY_LOADED if `ctx` already holds a document
    * with this content hash (a diamond include, to the caller that handles it), or another code.
    * @return `ANVL_RES_OK` on success; otherwise `ANVL_RES_ERR`.
    */
   anvl_result (*add)(module_context ctx, module_document doc, anvl_err_code *out_err_code);
   /**
    * @brief Unregister the document under `hash` in `ctx` and clear its source's owner.
    * @details Silently no-ops if the hash is not present.
    */
   void (*remove)(module_context ctx, uint64_t hash);
   /**
    * @brief Look up a document of `ctx` by source hash. NULL if not found.
    */
   module_document (*find)(module_context ctx, uint64_t hash);
   /**
    * @brief The number of documents registered in `ctx`.
    */
   usize (*count)(module_context ctx);
   /**
    * @brief Unregister every document of `ctx` (does not dispose them) and clear their owners.
    */
   void (*clear)(module_context ctx);
   /**
    * @brief Create (`init`) or dispose (`release`) ctx's map. Called by the context's own
    * lifecycle; `release` runs after the context's documents are disposed and touches nothing
    * else.
    */
   bool (*init)(module_context ctx);
   void (*release)(module_context ctx);
} anvl_source_registry_i;
extern const anvl_source_registry_i Registry;
