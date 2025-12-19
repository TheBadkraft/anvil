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
 * types.h - Type, enum, and value definitions for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: include/types.h
 * ------------------------------------------------------------------
 * Description:
 * This file contains type, enum, and value definitions for Anvil
 * ------------------------------------------------------------------
 */
#pragma once

#include <sigcore/types.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Versioning                                                         */
/* Update this by script for each release                             */
/* ------------------------------------------------------------------ */
#define ANVIL_API_VERSION_MAJOR 0
#define ANVIL_API_VERSION_MINOR 1
#define ANVIL_API_VERSION_PATCH 0
#define ANVIL_BUILD 0               // how to update auto-magically?
#define ANVIL_BUILD_CANDIDATE "dev" // dev, alpha, beta, rc, stable

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */
#define ANVL_SHEBANG_LEN 5
#define ANVL_EXT_LEN 4
#define ANVL_SHEBANG_AML "#!aml"
/* ------------------------------------------------------------------ */
/* Statement Types                                                   */
/* ------------------------------------------------------------------ */
typedef enum {
   ANVL_STMT_ASSN, // assignment statement (all current statements)
   ANVL_STMT_FUNC, // function statement (AnvilScript, future)
   ANVL_STMT_MSSG  // message statement (AMPP, future)
} anvl_stmt_type;

/* ------------------------------------------------------------------ */
/* Value Types (for expanded VALUE descriptor)                       */
/* ------------------------------------------------------------------ */
typedef enum {
   ANVL_VALUE_SCALAR,
   ANVL_VALUE_OBJECT,
   ANVL_VALUE_ARRAY,
   ANVL_VALUE_TUPLE,
   ANVL_VALUE_BLOB
} anvl_value_type;

/* ------------------------------------------------------------------ */
/* Statement Metadata Indices                                        */
/* ------------------------------------------------------------------ */
#define STMT_META_TYPE 0       // ASSN, FUNC, or MSSG
#define STMT_META_RESERVED_1 1 // reserved for future use
#define STMT_META_IDENT_POS 2  // identifier position in source
#define STMT_META_IDENT_LEN 3  // identifier length
#define STMT_META_BASE_IDX 4   // index into base_meta (0 = none, for inheritance metadata)
#define STMT_META_ATTR_IDX 5   // index into attr_meta (0 = none)
#define STMT_META_RESERVED_6 6 // reserved for future use
#define STMT_META_VALUE_IDX 7  // index into value_meta
#define STMT_META_RESERVED_8 8 // reserved for future use

/* ------------------------------------------------------------------ */
/* Forward declarations for recursive structures                      */
/* ------------------------------------------------------------------ */
typedef struct anvl_statement *statement;
typedef struct anvl_field *field;
typedef struct anvl_value *value;
typedef struct anvl_attribute *attribute;

/* ------------------------------------------------------------------ */
/* Attribute Structure                                               */
/* ------------------------------------------------------------------ */
struct anvl_attribute {
   usize key_pos;   // key position in source
   usize key_len;   // key length
   usize value_pos; // value position (0 if no value)
   usize value_len; // value length (0 if no value)
};

/* ------------------------------------------------------------------ */
/* Base Metadata Structure (for single inheritance)                  */
/* ------------------------------------------------------------------ */
struct anvl_base_meta {
   usize pos; // position of base class name in source
   usize len; // length of base class name
};

/* ------------------------------------------------------------------ */
/* Attribute Metadata Structure (for statement-level attributes)     */
/* ------------------------------------------------------------------ */
struct anvl_attr_meta {
   usize pos;       // position in source
   usize len;       // length
   usize value_pos; // value position (0 if no value)
   usize value_len; // value length (0 if no value)
};

/* ================================================================
 * Value Metadata Structure (for detailed value information)
 *
 * Value metadata is SELF-CONTAINED and stored per statement.
 *
 * For scalars: pos/len in source only
 * For arrays/tuples: pos/len in source + array of element_meta
 * For objects: pos/len in source + field_list indices
 * For blobs: pos/len in source (tag stored as attribute, not in value metadata)
 * ================================================================ */
struct anvl_element_meta {
   anvl_value_type type; // type of this element
   usize pos;            // position in source
   usize len;            // length in source
};

struct anvl_value_meta {
   anvl_value_type type; // scalar, object, array, tuple, blob
   usize pos;            // position in source
   usize len;            // length in source
   union {
      // For objects: field information
      struct {
         usize field_count;
         usize field_start; // index into context->field_list
      } object;
      // For arrays/tuples: element details
      struct {
         usize element_count;
         struct anvl_element_meta *elements; // array of element metadata (pos/len/type for each)
      } collection;
   } data;
};

/* ------------------------------------------------------------------ */
/* Statement Structure (with directional metadata)                    */
/* ========================================================================
 * Each statement is SELF-CONTAINED with its own meta-buffers:
 * - meta[9]: fixed metadata indices
 * - base_meta: inheritance info (single base, nullable)
 * - attr_meta: array of statement-level attributes
 * - value_meta: detailed value information
 * ======================================================================== */
/* ------------------------------------------------------------------ */
struct anvl_statement {
   // Fixed metadata buffer with indices
   usize meta[9]; // metadata buffer with directional indices
                  // meta[0] = type (ASSN/FUNC/MSSG)
                  // meta[1] = reserved
                  // meta[2] = ident_pos
                  // meta[3] = ident_len
                  // meta[4] = base_meta index (0=none, 1=present)
                  // meta[5] = attr_meta count (number of attributes)
                  // meta[6] = reserved
                  // meta[7] = value_meta index (0=none, 1=present)
                  // meta[8] = reserved

   // Self-contained decorator buffers
   struct anvl_base_meta *base_meta;   // NULL if no inheritance
   struct anvl_attr_meta *attr_meta;   // NULL if no attributes
   struct anvl_value_meta *value_meta; // detailed value info
};

/* ------------------------------------------------------------------ */
/* Field Structure (for object key-value pairs)                      */
/* ------------------------------------------------------------------ */
struct anvl_field {
   usize key_pos;      // key position in source
   usize key_len;      // key length
   usize attrib_start; // start index in context->attr_list (zero-copy)
   usize attrib_count; // number of attributes

   // Field value (nested)
   value val;
};

/* ------------------------------------------------------------------ */
/* Value Structure (recursive for nested structures)                */
/* ------------------------------------------------------------------ */
struct anvl_value {
   anvl_value_type type; // value type
   union {
      // Scalar values (string, number, etc.)
      struct {
         usize pos;
         usize len;
      } scalar;

      // Object values - store indices into context->field_list
      struct {
         usize field_start; // start index in context->field_list
         usize field_count; // number of fields
      } object;

      // Array/Tuple values - store indices into context->value_list
      struct {
         usize element_start;    // start index in context->value_list
         usize element_count;    // number of elements
         void *_elem_types_temp; // TEMPORARY: element types buffer (used during parsing only)
      } collection;
   } data;
};