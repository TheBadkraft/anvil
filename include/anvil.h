/* *********************************************************************** *
 * Copyright (c) 2025, 2026 Quantum Override. All rights reserved.         *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium, is *
 * strictly prohibited without express written permission from the         *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * anvil.h - Public API for Anvil                                          *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2025-12-03                                                     *
 * File: include/anvil.h                                                   *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * This header defines the public API for the Anvil library - including    * 
 * the module interface, which provides functionality for loading and      *
 * managing AML/ASL code.                                                  *
 * *********************************************************************** */
#pragma once

#include "constants.h"
// #include "context.h"
#include "errors.h"
// #include "operators.h"
// #include "symbols.h"
#include "types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ----------------------------------------------------------------- *
 * Anvil Interface                                                   *
 * ----------------------------------------------------------------- *
 * Retired: the old Module/AnvlMod-based load/parse/dispose surface  *
 * this file used to declare (unimplemented stub, only ever referenced *
 * by the already-disabled test_fixtures.c) — superseded by the real  *
 * public ABI in anvil_types.h/anvil_flat.h (anvil_load, anvil_dispose, *
 * anvil_has_errors, anvil_get_error). See notes/public-api.md.        *
 * get_version stays here — genuinely implemented, and test_version.c  *
 * (which runs after every test suite) depends on it.                 *
 * ----------------------------------------------------------------- */
typedef struct anvl_i {
   const char *(*get_version)(void);
} anvl_i;
extern const anvl_i Anvl;