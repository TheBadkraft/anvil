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
 * anvil.h - Public API for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-03
 * File: include/anvil.h
 * ------------------------------------------------------------------
 * Description:
 * This header defines the public API for the Anvil library, which
 * provides functionality for loading and managing AML/ASL code.
 */
#pragma once

/* Version identifier */
#define ANVIL_VERSION "v0.4.5-alpha"

#include "constants.h"
#include "context.h"
#include "errors.h"
#include "operators.h"
#include "symbols.h"
#include "types.h"
#include <sigma.core/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Opaque root handle – will be created by Anvil.load                 */
/* ------------------------------------------------------------------ */
typedef struct anvl_root_t *root;

/* ------------------------------------------------------------------ */
/* Parser Interface                                                  */
/* ------------------------------------------------------------------ */
bool anvl_parse(context ctx);

/* ------------------------------------------------------------------ */
/* The global Anvil interface                                         */
/* ------------------------------------------------------------------ */
typedef struct anvl_i {
   root (*load)(const char *filepath);
   root (*read)(const char *source, usize len);
   void (*dispose)(root root);
   void (*cleanup)(void);
   // Error handling
   bool (*error_is_set)(void);
   const anvl_error_state *(*error_get)(void);
   void (*error_clear)(void);
} anvl_i;
extern const anvl_i Anvil;