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
 * prototype.h - Prototype statement buffer structures
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-15
 * File: test/utilities/prototype.h
 * ------------------------------------------------------------------
 */

#pragma once

#include "anvil.h"
#include <sigcore/memory.h>

/* ------------------------------------------------------------------ */
/* Statement Buffer Structure                                        */
/* ------------------------------------------------------------------ */
typedef struct stmt_buffer *stmt_buffer;
struct stmt_buffer {
    // Header - contiguous block
    anvl_stmt_type type;       // statement type
    usize stmt_start;          // statement start position in source
    usize stmt_len;            // total statement length
    usize ident_pos;           // identifier position
    usize ident_len;           // identifier length
    void *ptr_base;            // pointer to base buffer (NULL if none)
    void *ptr_attribs;         // pointer to attribs buffer (NULL if none)
    anvl_value_type value_type; // value type
    void *ptr_value;           // pointer to value buffer (never NULL)
};

/* ------------------------------------------------------------------ */
/* Sub-Buffer Formats                                                */
/* ------------------------------------------------------------------ */
// Base buffer: [pos, len]
typedef struct base_buffer {
    usize pos;
    usize len;
} base_buffer;

// Attribs buffer: [count, len, pos1, len1, pos2, len2, ...]
typedef struct attribs_buffer {
    usize count;
    usize len;
    usize pairs[1]; // flexible array for pos/len pairs
} attribs_buffer;

// Value buffer: [pos, len] (simplified for prototype)
typedef struct value_buffer {
    usize pos;
    usize len;
} value_buffer;

/* ------------------------------------------------------------------ */
/* Prototype Interface                                               */
/* ------------------------------------------------------------------ */
stmt_buffer stmt_buffer_alloc(void);
void stmt_buffer_free(stmt_buffer sb);
void stmt_buffer_set_header(stmt_buffer sb, anvl_stmt_type type, usize stmt_start, usize stmt_len,
                           usize ident_pos, usize ident_len, anvl_value_type value_type);
void stmt_buffer_set_base(stmt_buffer sb, usize pos, usize len);
void stmt_buffer_set_attribs(stmt_buffer sb, usize count, usize len, usize *pairs);
void stmt_buffer_set_value(stmt_buffer sb, usize pos, usize len);