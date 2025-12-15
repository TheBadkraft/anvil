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
   // Fully contiguous buffer: 9 usize elements
   usize buffer[9];
};

/* ------------------------------------------------------------------ */
/* Buffer Access Macros                                              */
/* ------------------------------------------------------------------ */
#define STMT_TYPE(sb) ((anvl_stmt_type)(sb)->buffer[0])
#define STMT_START(sb) ((sb)->buffer[1])
#define STMT_LEN(sb) ((sb)->buffer[2])
#define IDENT_POS(sb) ((sb)->buffer[3])
#define IDENT_LEN(sb) ((sb)->buffer[4])
#define PTR_BASE(sb) ((void *)(uintptr_t)(sb)->buffer[5])
#define PTR_ATTRIBS(sb) ((void *)(uintptr_t)(sb)->buffer[6])
#define VALUE_TYPE(sb) ((anvl_value_type)(sb)->buffer[7])
#define PTR_VALUE(sb) ((void *)(uintptr_t)(sb)->buffer[8])

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

// Accessor functions
static inline anvl_stmt_type stmt_buffer_get_type(stmt_buffer sb) { return STMT_TYPE(sb); }
static inline usize stmt_buffer_get_start(stmt_buffer sb) { return STMT_START(sb); }
static inline usize stmt_buffer_get_len(stmt_buffer sb) { return STMT_LEN(sb); }
static inline usize stmt_buffer_get_ident_pos(stmt_buffer sb) { return IDENT_POS(sb); }
static inline usize stmt_buffer_get_ident_len(stmt_buffer sb) { return IDENT_LEN(sb); }
static inline void *stmt_buffer_get_ptr_base(stmt_buffer sb) { return PTR_BASE(sb); }
static inline void *stmt_buffer_get_ptr_attribs(stmt_buffer sb) { return PTR_ATTRIBS(sb); }
static inline anvl_value_type stmt_buffer_get_value_type(stmt_buffer sb) { return VALUE_TYPE(sb); }
static inline void *stmt_buffer_get_ptr_value(stmt_buffer sb) { return PTR_VALUE(sb); }