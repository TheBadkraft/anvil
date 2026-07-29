/* *********************************************************************** *
 * Copyright (c) 2025 Quantum Override. All rights reserved.            *
 *                                                                      *
 * This software is proprietary and confidential. Unauthorized copying, *
 * distribution, modification, or use of this software, via any medium, *
 * is strictly prohibited without express written permission from the   *
 * copyright holder.                                                    *
 *                                                                      *
 * SPDX-License-Identifier: Proprietary                                 *
 * ----------------------------------------------------------------------- *
 * types.h - Type, enum, and value definitions for Anvil                *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                     *
 * Created: 2025-12-14                                                  *
 * File: include/types.h                                                *
 * ----------------------------------------------------------------------- *
 * Description:                                                         *
 * This file contains type, enum, and value definitions for Anvil       *
 * *********************************************************************** */
#pragma once

#include "std.h"
// -----------------------------------------------------------------
#include <sigma/types.h>

/* ----------------------------------------------------------------- *
 * typedefs - Opaque Handles                                          *
 * ----------------------------------------------------------------- */
typedef struct anvl_mod_ctx_t *anvl_ctx;
typedef struct anvl_doc_t *anvl_doc;
typedef struct anvl_node_t *anvl_node;

/* ----------------------------------------------------------------- *
 * typedefs - Error state                                             *
 * ----------------------------------------------------------------- */
typedef struct anvl_error_state *anvl_err;

/* ----------------------------------------------------------------- *
 * anvl_mod_t *AnvilMod                                               *
 * ----------------------------------------------------------------- */
typedef struct anvl_mod_t *AnvlMod;
struct anvl_mod_t {
   anvl_ctx context;  // NULL until first doc loaded
   anvl_doc root; // root points to the first document loaded into the module (context->docs[0])
};