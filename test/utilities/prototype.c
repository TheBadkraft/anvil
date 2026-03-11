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
 * prototype.c - Prototype statement buffer implementation
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-15
 * File: test/utilities/prototype.c
 * ------------------------------------------------------------------
 */

#include "prototype.h"
#include <sigma.memory/memory.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Statement Buffer Allocation/Free                                   */
/* ------------------------------------------------------------------ */
stmt_buffer stmt_buffer_alloc(void) {
   void *_r = Allocator.alloc(sizeof(struct stmt_buffer));
   if (_r) memset(_r, 0, sizeof(struct stmt_buffer));
   return _r;
}

void stmt_buffer_free(stmt_buffer sb) {
   if (!sb)
      return;

   // Free sub-buffers
   void *ptr_base = PTR_BASE(sb);
   if (ptr_base) {
      Allocator.dispose(ptr_base);
   }
   void *ptr_attribs = PTR_ATTRIBS(sb);
   if (ptr_attribs) {
      Allocator.dispose(ptr_attribs);
   }
   void *ptr_value = PTR_VALUE(sb);
   if (ptr_value) {
      Allocator.dispose(ptr_value);
   }

   Allocator.dispose(sb);
}

/* ------------------------------------------------------------------ */
/* Header Setters                                                    */
/* ------------------------------------------------------------------ */
void stmt_buffer_set_header(stmt_buffer sb, anvl_stmt_type type, usize stmt_start, usize stmt_len,
                            usize ident_pos, usize ident_len, anvl_value_type value_type) {
   if (!sb)
      return;

   sb->buffer[0] = (usize)type;
   sb->buffer[1] = stmt_start;
   sb->buffer[2] = stmt_len;
   sb->buffer[3] = ident_pos;
   sb->buffer[4] = ident_len;
   sb->buffer[7] = (usize)value_type;
}

/* ------------------------------------------------------------------ */
/* Sub-Buffer Setters                                                */
/* ------------------------------------------------------------------ */
void stmt_buffer_set_base(stmt_buffer sb, usize pos, usize len) {
   if (!sb)
      return;

   void *old_ptr = PTR_BASE(sb);
   if (old_ptr) {
      Allocator.dispose(old_ptr);
   }

   base_buffer *bb = Allocator.alloc(sizeof(base_buffer));
   if (bb) memset(bb, 0, sizeof(base_buffer));
   bb->pos = pos;
   bb->len = len;
   sb->buffer[5] = (usize)(uintptr_t)bb;
}

void stmt_buffer_set_attribs(stmt_buffer sb, usize count, usize len, usize *pairs) {
   if (!sb || count == 0)
      return;

   void *old_ptr = PTR_ATTRIBS(sb);
   if (old_ptr) {
      Allocator.dispose(old_ptr);
   }

   // Allocate for count, len, and 2*count pairs
   usize total_size = sizeof(attribs_buffer) + (2 * count - 1) * sizeof(usize);
   attribs_buffer *ab = Allocator.alloc(total_size);
   if (ab) memset(ab, 0, total_size);
   ab->count = count;
   ab->len = len;
   memcpy(ab->pairs, pairs, 2 * count * sizeof(usize));
   sb->buffer[6] = (usize)(uintptr_t)ab;
}

void stmt_buffer_set_value(stmt_buffer sb, usize pos, usize len) {
   if (!sb)
      return;

   void *old_ptr = PTR_VALUE(sb);
   if (old_ptr) {
      Allocator.dispose(old_ptr);
   }

   value_buffer *vb = Allocator.alloc(sizeof(value_buffer));
   if (vb) memset(vb, 0, sizeof(value_buffer));
   vb->pos = pos;
   vb->len = len;
   sb->buffer[8] = (usize)(uintptr_t)vb;
}