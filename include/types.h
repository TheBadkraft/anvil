/* *********************************************************************** *
 * Copyright (c) 2025 Quantum Override. All rights reserved.               *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium,    *
 * is strictly prohibited without express written permission from the      *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * types.h - Type, enum, and value definitions for Anvil                   *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2025-12-14                                                     *
 * File: include/types.h                                                   *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * This file contains type, enum, and value definitions for Anvil          *
 * *********************************************************************** */
#pragma once

#include "std.h"
// -------------------------
#include <sigma/types.h>

/* ----------------------------------------------------------------------- *
 * Dialect: AML vs ASL vs AMP vs Aurora                                    *
 * ----------------------------------------------------------------------- */
typedef enum {
   ANVL_DIALECT_AML,   // Anvil Markup Language (full features)
   ANVL_DIALECT_ASL,   // Anvil Script Language (future)
   ANVL_DIALECT_AMP,   // Anvil Messaging Protocol (scalars + blobs only)
   ANVL_DIALECT_ERROR, // Error state for dialect detection
} anvl_dialect;

/* ----------------------------------------------------------------------- *
 * Result codes                                                            *
 * ----------------------------------------------------------------------- */
typedef enum {
   ANVL_RES_OK,       // Operation completed successfully
   ANVL_RES_ERR,      // Operation failed with an error
   ANVL_RES_EOF,      // End of file or input reached
   ANVL_RES_NOOP,     // No operation performed (e.g., no-op for certain conditions)
   ANVL_RES_DEFERRED, // operation deferred to another handler
   ANVL_RES_UNKNOWN,  // Unknown result state
} anvl_result;

/* ----------------------------------------------------------------------- *
 * typedefs - Opaque Handles                                               *
 * ----------------------------------------------------------------------- */
typedef struct anvl_mod_ctx_t *module_context;
typedef struct anvl_mod_doc_t *module_document;
typedef struct anvl_node_t *anvl_node;

/* ----------------------------------------------------------------------- *
 * typedefs - Error state                                                  *
 * ----------------------------------------------------------------------- */
typedef struct anvl_error_state *anvl_error;

/* ----------------------------------------------------------------------- *
 * anvl_mod_t *AnvilMod                                                    *
 * ----------------------------------------------------------------------- */
typedef struct anvl_mod_t *AnvlMod;
struct anvl_mod_t {
   module_context context; // NULL until first doc loaded
   module_document root;   // first document loaded (context->docs[0])
};