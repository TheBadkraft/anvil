/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * source.c - Source interface implementation
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-13
 * File: src/core/source.c
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static anvl_dialect detect_dialect_from_source(const char *, usize);

/* Source interface implementations */
static source source_create(const char *data, usize len) {
   source src = Memory.alloc(sizeof(struct anvl_source_t), true);

   // Always allocate our own buffer - source owns its data
   if (data && len > 0) {
      char *buffer = Memory.alloc(len + 1, false); // +1 for null terminator
      memcpy(buffer, data, len);
      buffer[len] = '\0'; // null terminate
      src->buffer.bucket = buffer;
      src->buffer.end = buffer + len;
   } else {
      // Empty source
      src->buffer.bucket = NULL;
      src->buffer.end = NULL;
   }

   src->dialect = (data && len > 0) ? detect_dialect_from_source(data, len) : ANVL_DIALECT_ASL;
   src->stride = sizeof(char);
   src->pos = 0;
   src->line = 1;
   src->col = 1;
   return src;
}

/* Detect dialect from source content (shebang) */
static anvl_dialect detect_dialect_from_source(const char *source, usize len) {
   if (len >= ANVL_SHEBANG_LEN && strncmp(source, ANVL_SHEBANG_AML, ANVL_SHEBANG_LEN) == 0)
      return ANVL_DIALECT_AML;
   // if it ain't AML, it's friggin ASL ... right?
   return ANVL_DIALECT_ASL; // default
}

static void source_dispose(source self) {
   if (self->buffer.bucket) {
      Memory.dispose(self->buffer.bucket);
   }
   Memory.dispose(self);
}

static anvl_dialect source_get_dialect(source self) {
   // const char *data = (const char *)self->buffer.bucket;
   // usize len = self->buffer.end - self->buffer.bucket;

   // // Check shebang first (highest precedence)
   // anvl_dialect shebang_dialect = detect_dialect_from_source(data, len);
   // if (shebang_dialect != ANVL_DIALECT_ASL) {
   //    return shebang_dialect;
   // }

   // // Default to ASL
   // return ANVL_DIALECT_ASL;

   return self->dialect;
}

const anvl_source_i Source = {
    .create = source_create,
    .dispose = source_dispose,
    .dialect = source_get_dialect};
