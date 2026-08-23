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
 * constants.h - Constants for Anvil                                       *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2025-12-14                                                     *
 * File: include/constants.h                                               *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * This file contains constants for Anvil                                  *
 * ----------------------------------------------------------------------- */
#pragma once

#include "std.h"
// -------------------------
#include <sigma/types.h>

/* ----------------------------------------------------------------------- *
 * Versioning                                                              *
 * Update this by script for each release                                  *
 * ----------------------------------------------------------------------- */
#define ANVL_VERSION_MAJOR 0
#define ANVL_VERSION_MINOR 7
#define ANVL_VERSION_PATCH 0
#define ANVL_VERSION_TAG "alpha"
#define ANVL_VERSION_STR "0.7.0-alpha"

/* ----------------------------------------------------------------------- *
 * Constants                                                               *
 * ----------------------------------------------------------------------- */
#define ANVL_SHEBANG_LEN 5
#define ANVL_EXT_LEN 4
#define ANVL_SHEBANG_AML "#!aml"
#define ANVL_SHEBANG_AMP "#!amp"
#define ANVL_SHEBANG_ASL "#!asl"
#define ANVL_EXT_AML ".aml"
#define ANVL_EXT_AMP ".amp"
#define ANVL_EXT_ASL ".asl"
#define ANVL_EXT_ANVL ".anvl"

/* ----------------------------------------------------------------------- *
 * Arena sizing — see notes/document-body-parse.md "Initial sizing heuristic"
 * capacity = max(sum(Source.length() across ctx->docs) * ANVL_ARENA_SIZE_MULTIPLIER,
 *                ANVL_ARENA_MIN_SIZE)
 * ----------------------------------------------------------------------- */
// Working multiplier on summed source length to estimate body-parse arena capacity.
// A minimal statement (`name := 42;`, 11 bytes) produces ~170 bytes of arena nodes —
// over 15x its own length — so raw byte count alone badly undercounts. Chosen for
// being cheap, not precise; replace with a measured value once real parsing exists
// to instrument against.
#define ANVL_ARENA_SIZE_MULTIPLIER 2
// Floor for the initial arena block. Allocator.create_bump(initial) uses a non-zero
// `initial` verbatim — it does not floor to its own default — so a small document's
// multiplied estimate could still land under a sensible minimum without this.
// Currently matches sigma's own bump-allocator default (DEFAULT_BLOCK_SZ,
// src/sigma/memory.c) but is Anvil's own named setting, not a re-export of it.
#define ANVL_ARENA_MIN_SIZE (64u * 1024u)