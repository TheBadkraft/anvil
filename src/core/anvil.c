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

void anvl_format_version(char *buf, size_t cap, int major, int minor, int patch, const char *tag,
                         int build) {
   if (!buf || cap == 0) {
      return;
   }
   // semver: the pre-release tag comes BEFORE the build metadata, and an unknown build is omitted.
   int n = snprintf(buf, cap, "%d.%d.%d", major, minor, patch);
   if (tag && *tag && n > 0 && (size_t)n < cap) {
      n += snprintf(buf + n, cap - (size_t)n, "-%s", tag);
   }
   if (build > 0 && n > 0 && (size_t)n < cap) {
      snprintf(buf + n, cap - (size_t)n, "+%d", build);
   }
}

static const char *anvl_get_version(void) {
   static char version[64];
   anvl_format_version(version, sizeof(version), ANVL_VERSION_MAJOR, ANVL_VERSION_MINOR,
                       ANVL_VERSION_PATCH, ANVL_VERSION_TAG, ANVL_BUILD);
   return version;
}

// interface vtable
const anvl_i Anvl = {
   .get_version = anvl_get_version,
};