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
#include <string.h>

/* ------------------------------------------------------------------ */
/* Statement Buffer Allocation/Free                                   */
/* ------------------------------------------------------------------ */
stmt_buffer stmt_buffer_alloc(void) {
    return Memory.alloc(sizeof(struct stmt_buffer), true);
}

void stmt_buffer_free(stmt_buffer sb) {
    if (!sb) return;

    // Free sub-buffers
    if (sb->ptr_base) {
        Memory.dispose(sb->ptr_base);
    }
    if (sb->ptr_attribs) {
        Memory.dispose(sb->ptr_attribs);
    }
    if (sb->ptr_value) {
        Memory.dispose(sb->ptr_value);
    }

    Memory.dispose(sb);
}

/* ------------------------------------------------------------------ */
/* Header Setters                                                    */
/* ------------------------------------------------------------------ */
void stmt_buffer_set_header(stmt_buffer sb, anvl_stmt_type type, usize stmt_start, usize stmt_len,
                           usize ident_pos, usize ident_len, anvl_value_type value_type) {
    if (!sb) return;

    sb->type = type;
    sb->stmt_start = stmt_start;
    sb->stmt_len = stmt_len;
    sb->ident_pos = ident_pos;
    sb->ident_len = ident_len;
    sb->value_type = value_type;
}

/* ------------------------------------------------------------------ */
/* Sub-Buffer Setters                                                */
/* ------------------------------------------------------------------ */
void stmt_buffer_set_base(stmt_buffer sb, usize pos, usize len) {
    if (!sb) return;

    if (sb->ptr_base) {
        Memory.dispose(sb->ptr_base);
    }

    base_buffer *bb = Memory.alloc(sizeof(base_buffer), true);
    bb->pos = pos;
    bb->len = len;
    sb->ptr_base = bb;
}

void stmt_buffer_set_attribs(stmt_buffer sb, usize count, usize len, usize *pairs) {
    if (!sb || count == 0) return;

    if (sb->ptr_attribs) {
        Memory.dispose(sb->ptr_attribs);
    }

    // Allocate for count, len, and 2*count pairs
    usize total_size = sizeof(attribs_buffer) + (2 * count - 1) * sizeof(usize);
    attribs_buffer *ab = Memory.alloc(total_size, true);
    ab->count = count;
    ab->len = len;
    memcpy(ab->pairs, pairs, 2 * count * sizeof(usize));
    sb->ptr_attribs = ab;
}

void stmt_buffer_set_value(stmt_buffer sb, usize pos, usize len) {
    if (!sb) return;

    if (sb->ptr_value) {
        Memory.dispose(sb->ptr_value);
    }

    value_buffer *vb = Memory.alloc(sizeof(value_buffer), true);
    vb->pos = pos;
    vb->len = len;
    sb->ptr_value = vb;
}