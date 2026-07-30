/* *********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium,    *
 * is strictly prohibited without express written permission from the      *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * anvil.c - Implementation of Anvil public API                            *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-30                                                     *
 * File: src/core/anvil.c                                                  *
 * *********************************************************************** */

#include "anvil.h"
#include "types.h"

static const char *anvl_get_version(void) {
   static char version[32];
   snprintf(version, sizeof(version), "%d.%d.%d+%d-%s", ANVL_VERSION_MAJOR, ANVL_VERSION_MINOR,
            ANVL_VERSION_PATCH, ANVL_BUILD, ANVL_VERSION_TAG);
   return version;
}

// interface vtable
const anvl_i Anvl = {
   .get_version = anvl_get_version,
};