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
 * anvil_types.h - Public ABI types for Anvil Native                      *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Opaque handles and stable enums shared by every public Anvil header.   *
 * Never includes anything from an internal header — nothing in here is   *
 * permitted to depend on, or leak, an internal representation. See       *
 * notes/public-api.md for the full design.                              *
 * ********************************************************************** */
#pragma once

#include <stdbool.h>

/**
 * @brief Opaque handle to a loaded, fully-resolved Anvil document.
 *
 * Not thread-safe: a single anvil_document must only be used from the
 * thread that obtained it, unless the caller provides its own external
 * synchronization. Never dereferenced by the caller — every operation on
 * it goes through an accessor function.
 */
typedef struct anvil_document_t *anvil_document;

/**
 * @brief Stable, deliberately small public error category.
 *
 * One category per document-pipeline phase, plus the cross-cutting
 * infrastructure cases. Distinct from (and never numerically tied to) the
 * internal anvl_err_code enum, which carries ~90 granular parser/resolver
 * codes that are free to change as the implementation evolves without
 * that ever being a public ABI break. Full detail (message text, line/
 * column) is reachable through separate accessors, not this enum.
 */
typedef enum {
   ANVIL_OK = 0,
   ANVIL_ERR_IO,                // couldn't read the source at all
   ANVIL_ERR_HEADER,            // phase 1 - shebang/import/attribute scan
   ANVIL_ERR_IMPORT,            // phase 2 - import graph loading
   ANVIL_ERR_SYNTAX,            // phase 3 - body/grammar
   ANVIL_ERR_RESOLVE,           // phase 4 - identifier/base/inheritance resolution
   ANVIL_ERR_MEMORY,            // allocation failure
   ANVIL_ERR_INVALID_ARGUMENT,  // bad call into the API itself
} anvil_err_code;
