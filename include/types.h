/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * types.h - Type, enum, and value definitions for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: include/types.h
 * ------------------------------------------------------------------
 * Description:
 * This file contains type, enum, and value definitions for Anvil
 * ------------------------------------------------------------------
 */
#pragma once

#include <sigcore/types.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Versioning                                                         */
/* Update this by script for each release                             */
/* ------------------------------------------------------------------ */
#define ANVIL_API_VERSION_MAJOR 0
#define ANVIL_API_VERSION_MINOR 1
#define ANVIL_API_VERSION_PATCH 0
#define ANVIL_BUILD 0               // how to update auto-magically?
#define ANVIL_BUILD_CANDIDATE "dev" // dev, alpha, beta, rc, stable

/* ------------------------------------------------------------------ */
/* Dialect: AML vs ASL                                                */
/* ------------------------------------------------------------------ */
typedef enum {
   ANVL_DIALECT_AML,
   ANVL_DIALECT_ASL
} anvl_dialect;

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */
#define ANVL_SHEBANG_LEN 5
#define ANVL_EXT_LEN 4
#define ANVL_SHEBANG_AML "#!aml"
#define ANVL_SHEBANG_ASL "#!asl"
#define ANVL_EXT_AML ".aml"
#define ANVL_EXT_ASL ".asl"