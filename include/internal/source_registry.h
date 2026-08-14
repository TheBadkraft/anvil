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
 * ---------------------------------------------------------------------- */
typedef struct anvl_source_registry_i {
   /**
    * @brief Idempotently initialize the global source registry.
    * @details Safe to call multiple times. Normally invoked from `mod_new()` so
    * the registry exists before any document is registered.
    */
   void (*init)(void);
   /**
    * @brief Register a document under its source hash.
    * @param doc The document to register. Must have a loaded source with a non-zero hash.
    * @param[out] out_err_code Pointer to the error code if registration fails.
    * @return Anvl result: `ANVL_RES_OK` on success; otherwise `ANVL_RES_ERR`.
    * @details This function adds the document to the global registry keyed by
    * `Source.hash(doc->source)`. If the hash is already registered, the call fails.
    */
   anvl_result (*add)(module_document doc, anvl_err_code *out_err_code);
   /**
    * @brief Remove a document from the registry by source hash.
    * @param hash The source hash to unregister.
    * @details Silently no-ops if the hash is not present.
    */
   void (*remove)(uint64_t hash);
   /**
    * @brief Look up a document by source hash.
    * @param hash The source hash to search for.
    * @return The registered document, or NULL if not found.
    */
   module_document (*find)(uint64_t hash);
   /**
    * @brief Return the number of registered documents.
    */
   usize (*count)(void);
   /**
    * @brief Release one module reference to the registry.
    * @details Decrements the module reference count and clears the registry
    * when the count reaches zero. Normally invoked from `mod_dispose()`.
    */
   void (*release)(void);
   /**
    * @brief Clear every entry from the registry unconditionally.
    * @details This does not dispose of the registered documents. Intended for
    * tests and explicit reset scenarios; module code should prefer `release()`.
    */
   void (*clear)(void);
} anvl_source_registry_i;
extern const anvl_source_registry_i Registry;
