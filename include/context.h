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
 * context.h - Context and Source API for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-16
 * File: include/context.h
 * ------------------------------------------------------------------
 * Description:
 * This header defines the context and source APIs for Anvil.
 */
#pragma once

#include "constants.h"
#include "errors.h"
#include "operators.h"
#include "symbols.h"
#include "types.h"
#include "utils.h"
#include <sigma.core/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_NESTING 16

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

   // Consume
   usize (*consume)(source, usize);

   // Data access (for scanning without consuming)
   const char *(*data)(source);
   usize (*length)(source);

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
   // List of module attributes (top-level only)
   struct {
      attribute *attributes; // array of attribute pointers
      usize count;           // number of attributes
      usize capacity;        // allocated capacity
   } attr_list;
   // List of parsed fields (for object key-value pairs)
   // Fields store references to their values via value_meta in parent statement
   struct {
      field *fields;  // array of field pointers
      usize count;    // number of fields
      usize capacity; // allocated capacity
   } field_list;
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
/* Value Builder - for incremental value construction                */
/* ------------------------------------------------------------------ */
typedef struct anvl_value_builder_t *value_builder;
typedef struct anvl_stmt_builder_t *stmt_builder;
struct anvl_value_builder_t {
   value val;   // the value being built
   context ctx; // reference to owning context
   // Internal state for nesting
   struct {
      usize depth;
      void *values[MAX_NESTING];
   } value_stack;
   // Temporary storage for building
   struct {
      field *fields;
      usize field_count;
      usize field_capacity;
      value *values;
      usize value_count;
      usize value_capacity;
   } temp;

   // Builder methods
   void (*begin_object)(value_builder self);
   void (*field)(value_builder self, usize key_pos, usize key_len, usize attrib_start, usize attrib_count);
   void (*field_value)(value_builder self, value val);
   void (*end_object)(value_builder self);
   void (*begin_array)(value_builder self);
   void (*element)(value_builder self, value val);
   void (*end_array)(value_builder self);
   void (*scalar)(value_builder self, usize pos, usize len, anvl_value_type type);
};

/* ------------------------------------------------------------------ */
/* Attribute Builder - for incremental attribute construction        */
/* ------------------------------------------------------------------ */
typedef struct anvl_attr_builder_t *attr_builder;
struct anvl_attr_builder_t {
   context ctx; // reference to owning context
   // Current attribute being built
   usize key_pos;
   usize key_len;
   usize value_pos;
   usize value_len;
   bool has_value;

   // Builder methods
   void (*key)(attr_builder self, usize pos, usize len);
   void (*value)(attr_builder self, usize pos, usize len);
   void (*add)(attr_builder self);
};

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
   // Statement Builder
   stmt_builder (*begin_statement)(context self, anvl_stmt_type type);
   void (*end_statement)(context self, stmt_builder bldr, anvl_stmt_type type);
   // Value Builder
   value_builder (*begin_value)(context self, anvl_value_type type);
   void (*end_value)(context self, value_builder bldr);
   // Attribute Builder
   attr_builder (*begin_attribute)(context self);
   void (*end_attribute)(context self, attr_builder bldr);
} anvl_context_i;
extern const anvl_context_i Context;

/* ------------------------------------------------------------------ */
/* Statement Interface                                               */
/* ------------------------------------------------------------------ */
typedef struct anvl_statement_i {
   const char *(*identifier)(statement self, source src);
   const char *(*base)(statement self, source src);
   anvl_stmt_type (*type)(statement self);
   anvl_value_type (*value_type)(statement self);
   usize (*length)(statement self);
} anvl_statement_i;
extern const anvl_statement_i Statement;

/* ------------------------------------------------------------------ */
/* Statement Builder - for incremental statement construction        */
/* ------------------------------------------------------------------ */
struct anvl_stmt_builder_t {
   statement stmt; // the statement being built
   context ctx;    // reference to owning context
   // Internal state for nesting (e.g., current value being built)
   struct {
      usize depth;
      value values[MAX_NESTING];
   } value_stack;

   // Builder methods
   void (*identifier)(stmt_builder self, usize pos, usize len);
   void (*base)(stmt_builder self, usize pos, usize len);
   void (*begin_attributes)(stmt_builder self);
   void (*attribute_key)(stmt_builder self, usize pos, usize len);
   void (*attribute_value)(stmt_builder self, usize pos, usize len);
   void (*end_attributes)(stmt_builder self);
   value_builder (*begin_value)(stmt_builder self, anvl_value_type type);
   void (*end_value)(stmt_builder self, value_builder vbldr);
   void (*value_scalar)(stmt_builder self, usize pos, usize len, anvl_value_type type);
};