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
 * anvil.h - Public API for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-03
 * File: include/anvil.h
 * ------------------------------------------------------------------
 * Description:
 * This header defines the public API for the Anvil library, which
 * provides functionality for loading and managing AML/ASL code.
 */
#pragma once

#include "constants.h"
#include "errors.h"
#include "operators.h"
#include "symbols.h"
#include "types.h"
#include <sigcore/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Source structure - a source interface `anvl_source_i` for interrogators */
/* ------------------------------------------------------------------ */
typedef struct anvl_source_t *source;
struct anvl_source_t {
   // farray-compatible buffer structure
   struct {
      void *bucket; // pointer to first element (raw bytes)
      void *end;    // one past allocated memory
   } buffer;
   anvl_dialect dialect;
   usize stride; // size of each element in bytes (1 for byte arrays)
   usize pos;
   usize line;
   usize col;
};
/* ------------------------------------------------------------------ */
/* Source Interface - for interrogating source content               */
/* ------------------------------------------------------------------ */
typedef struct anvl_source_i {
   source (*create)(const char *, usize);
   void (*dispose)(source self);
   anvl_dialect (*dialect)(source);

   // Position & EOF
   usize (*position)(source);
   usize (*line)(source);
   usize (*column)(source);
   bool (*is_eof)(source);
   bool (*is_eof_offset)(source, usize);

   // Character peek
   char (*peek)(source);
   char (*peek_offset)(source, usize);

   // String matching (returns length if matches, 0 if not)
   usize (*match_length)(source, const char *, usize);
   usize (*match_operator)(source, const char *, usize);

   // Character classification
   bool (*is_alpha)(char);
   bool (*is_digit)(char);
   bool (*is_hex_digit)(char);
   bool (*is_identifier_start)(char);
   bool (*is_identifier_part)(char);

   // Operator & Symbol classification
   anvl_operator (*is_operator)(source);
   anvl_symbol (*is_symbol)(source);

   // Consume
   usize (*consume)(source, usize);

   // Substring extraction
   char *(*substring)(source, usize, usize);

   // Whitespace & comments
   usize (*skip_whitespace_and_comments)(source);

   // Shebang
   bool (*is_shebang)(source);

   // Dialect parsing
   anvl_dialect (*parse_dialect)(source, anvl_dialect);

   // Position management
   void (*set_position)(source, usize, usize, usize);
   void (*reset)(source);
} anvl_source_i;
extern const anvl_source_i Source;

/* ------------------------------------------------------------------ */
/* Context – encapsulates source and state                            */
/* ------------------------------------------------------------------ */
typedef struct anvl_context_t *context;
struct anvl_context_t {
   source source;
   // List of parsed statements
   struct {
      statement *statements; // array of statement pointers
      usize count;           // number of statements
      usize capacity;        // allocated capacity
   } stmt_list;
   // List of module attributes
   struct {
      attribute *attributes; // array of attribute pointers
      usize count;           // number of attributes
      usize capacity;        // allocated capacity
   } attr_list;
};

/* ------------------------------------------------------------------ */
/* Context Builder Interface                                          */
/* ------------------------------------------------------------------ */
typedef struct anvl_ctx_builder_i {
   anvl_dialect dialect;
   source source;
   void (*set_dialect)(struct anvl_ctx_builder_i *self, anvl_dialect dialect);
   void (*set_source)(struct anvl_ctx_builder_i *self, const char *source, usize len);
   bool (*load_file)(struct anvl_ctx_builder_i *self, const char *filepath);
   context (*build)(struct anvl_ctx_builder_i *self);
   void (*dispose)(struct anvl_ctx_builder_i *self);
} anvl_ctx_builder_i;
extern anvl_ctx_builder_i CtxBuilder;

/* ------------------------------------------------------------------ */
/* Context Interface                                                  */
/* ------------------------------------------------------------------ */
typedef struct anvl_context_i {
   struct anvl_ctx_builder_i *(*get_builder)(void);
   anvl_dialect (*dialect)(context self);
   usize (*statement_count)(context self);
   statement (*get_statement)(context self, usize index);
   bool (*add_statement)(context self, statement stmt);
   usize (*attribute_count)(context self);
   attribute (*get_attribute)(context self, usize index);
   bool (*add_attribute)(context self, attribute attr);
   void (*dispose)(context self);
   // Parser
   bool (*parse)(context self);
} anvl_context_i;
extern const anvl_context_i Context;

/* ------------------------------------------------------------------ */
/* Opaque root handle – will be created by Anvil.load                 */
/* ------------------------------------------------------------------ */
typedef struct anvl_root_t *root;

/* ------------------------------------------------------------------ */
/* Parser Interface                                                  */
/* ------------------------------------------------------------------ */
bool anvl_parse(context ctx);

/* ------------------------------------------------------------------ */
/* The global Anvil interface                                         */
/* ------------------------------------------------------------------ */
typedef struct anvl_i {
   root (*load)(const char *filepath);
   root (*read)(const char *source, usize len);
   void (*dispose)(root root);
   void (*cleanup)(void);
   // Error handling
   bool (*error_is_set)(void);
   const anvl_error_state *(*error_get)(void);
   void (*error_clear)(void);
} anvl_i;
extern const anvl_i Anvil;