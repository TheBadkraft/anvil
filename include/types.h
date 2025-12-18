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
   ANVL_STMT_ASSN,        // assignment statement
   ANVL_STMT_INHERITANCE, // inheritance statement
   ANVL_STMT_FUNC         // function statement (future use)
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
#define STMT_META_TYPE 0
#define STMT_META_LEN 1
#define STMT_META_IDENT_POS 2
#define STMT_META_IDENT_LEN 3
#define STMT_META_VALUE_TYPE 4

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
/* Statement Structure (simplified with direct ownership)            */
/* ------------------------------------------------------------------ */
struct anvl_statement {
   usize meta[9]; // metadata buffer
   usize base[2]; // base position and length

   // Statement attributes
   usize attrib_count;
   attribute *attributes;

   // Statement value (nested)
   value val;
};

/* ------------------------------------------------------------------ */
/* Field Structure (for object key-value pairs)                      */
/* ------------------------------------------------------------------ */
struct anvl_field {
   usize key_pos; // key position in source
   usize key_len; // key length

   // Field attributes (direct ownership)
   usize attrib_count;
   attribute *attributes;

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

      // Object values
      struct {
         usize field_count;
         field *fields;
      } object;

      // Array/Tuple values
      struct {
         usize element_count;
         value *elements;
      } collection;
   } data;
};