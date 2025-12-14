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
   ANVL_STMT_ASSN, // assignment statement
   ANVL_STMT_FUNC  // function statement (future use)
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
/* Attribute Structure                                               */
/* ------------------------------------------------------------------ */
typedef struct anvl_attribute *attribute;
struct anvl_attribute {
   usize key_pos;   // key position in source
   usize key_len;   // key length
   usize value_pos; // value position (0 if no value)
   usize value_len; // value length (0 if no value)
};

/* ------------------------------------------------------------------ */
/* Statement Structure                                               */
/* ------------------------------------------------------------------ */
typedef struct anvl_statement *statement;
struct anvl_statement {
   anvl_stmt_type type; // statement type
   usize stmt_len;      // total statement length

   // Identifier info
   usize ident_pos; // identifier position in source
   usize ident_len; // identifier length

   // Attributes (optional)
   usize attribs_len;   // total attributes length (0 if none)
   usize attribs_count; // number of attributes
   // Followed by attribs_count pairs of (pos, len) for each attribute

   // Value info
   anvl_value_type value_type; // type of value
   usize value_pos;            // value position in source
   usize value_len;            // value length
};