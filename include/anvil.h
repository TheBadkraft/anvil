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
 * Module Interface                                                  *
 * ----------------------------------------------------------------- */
typedef struct anvl_mod_i {
   bool (*parse)(AnvlMod mod);
} anvl_mod_i;
extern const anvl_mod_i Module;

/* ----------------------------------------------------------------- *
 * Anvil Interface                                                   *
 * ----------------------------------------------------------------- */
typedef struct anvl_i {
   AnvlMod (*load)(const char *filepath);
   // AnvlMod (*read)(const char *source, usize len);
   void (*dispose)(AnvlMod mod);
   const char *(*get_root_path)(AnvlMod mod);
   // void (*cleanup)(void);
   // Error handling - from root error state can be queried across multiple source objects
   bool (*has_errors)(AnvlMod mod);
   // const anvl_error_state *(*error_get)(void);
   void (*error_clear)(AnvlMod mod);
   // Version
   const char *(*get_version)(void);
} anvl_i;
extern const anvl_i Anvl;