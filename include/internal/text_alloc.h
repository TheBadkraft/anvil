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
 * internal/text_alloc.h - full-length text reads for types and schema    *
 * ---------------------------------------------------------------------- *
 * The public getters return the FULL length (snprintf-style), so these    *
 * query it first and allocate exactly - never a fixed buffer that could   *
 * silently compare a truncated prefix. Release with Allocator.dispose.    *
 * ********************************************************************** */

#pragma once

#include "anvil_flat.h"
#include <sigma.core/allocator.h>
#include <string.h>

/// A fresh NUL-terminated copy of s, or NULL on allocation failure.
static inline char *anvil_text_dup(const char *s) {
   size_t len = strlen(s);
   char *copy = Allocator.alloc(len + 1);
   if (copy) {
      memcpy(copy, s, len + 1);
   }
   return copy;
}

/// A statement's full name, or NULL on allocation failure.
static inline char *anvil_name_alloc(anvil_statement stmt) {
   size_t len = anvil_statement_get_name(stmt, NULL, 0);
   char *name = Allocator.alloc(len + 1);
   if (name) {
      anvil_statement_get_name(stmt, name, len + 1);
   }
   return name;
}

/// A value's full text, or NULL on allocation failure.
static inline char *anvil_text_alloc(anvil_value val) {
   size_t len = anvil_value_get_text(val, NULL, 0);
   char *text = Allocator.alloc(len + 1);
   if (text) {
      anvil_value_get_text(val, text, len + 1);
   }
   return text;
}
