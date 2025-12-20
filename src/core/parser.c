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
 * parser.c - Implementation of Anvil parser
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-13
 * File: src/core/parser.c
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "context_internal.h"
#include "errors.h"
#include "source_internal.h"

#include <sigcore/memory.h>
#include <stdio.h>
#include <string.h>

typedef struct {
   context ctx;
   source src;
} parser_ctx;

// Forward declarations
static void parser_error(anvl_error_code code, source s);
static void dispose_value_tree(value v);
static void dispose_statement(statement stmt);
static bool parse_source(parser_ctx *p);
static bool parse_statement(parser_ctx *p, statement stmt);
static value parse_value(parser_ctx *p);
static value parse_object(parser_ctx *p);
static value parse_array(parser_ctx *p);
static value parse_tuple(parser_ctx *p);
static value parse_blob(parser_ctx *p);
static bool parse_attribute_block(parser_ctx *p, usize *out_start, usize *out_count);
static bool parse_scalar_value(parser_ctx *p, usize *start, usize *len, anvl_value_type *type);
static bool read_identifier(parser_ctx *p, usize *pos, usize *len);

bool anvl_parse(context ctx) {
   if (!ctx || !ctx->source) {
      return false;
   }

   anvl_error_clear();

   parser_ctx p = {
       .ctx = ctx,
       .src = ctx->source,
   };

   return parse_source(&p);
}

static void parser_error(anvl_error_code code, source s) {
   anvl_error_set(code, anvl_error_code_message(code), si_line(s), si_column(s), __FILE__);
}

static void dispose_value_tree(value v) {
   // All values are owned by context->value_list, not the parser
   // This function kept for API compatibility but does nothing
   (void)v;
}

static void dispose_statement(statement stmt) {
   // Free statements that failed to parse before being added to context list
   // Statements successfully added to context->stmt_list are freed via Context.dispose()
   if (stmt) {
      Memory.dispose(stmt);
   }
}

static bool parse_source(parser_ctx *p) {
   si_skip_whitespace_and_comments(p->src);

   // Module attributes (prefix only)
   // AMP dialect does not allow module-level attributes
   if (Source.dialect(p->src) == ANVL_DIALECT_AMP) {
      if (si_match_length(p->src, "@[", 2) == 2) {
         parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, p->src);
         return false;
      }
   }
   while (si_match_length(p->src, "@[", 2) == 2) {
      if (!parse_attribute_block(p, NULL, NULL))
         return false;
      si_skip_whitespace_and_comments(p->src);
   }

   while (!si_is_eof(p->src)) {
      // Shebang is illegal after statements
      if (si_match_length(p->src, "#!", 2) == 2) {
         parser_error(ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS, p->src);
         return false;
      }

      statement stmt = ci_new_statement(p->ctx, ANVL_STMT_ASSN);
      if (!stmt) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, p->src);
         return false;
      }

      if (!parse_statement(p, stmt)) {
         dispose_statement(stmt);
         return false;
      }

      ci_add_statement(p->ctx, stmt);
      si_skip_whitespace_and_comments(p->src);
   }

   si_skip_whitespace_and_comments(p->src);
   if (si_match_length(p->src, "@[", 2) == 2) {
      parser_error(ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES, p->src);
      return false;
   }

   return true;
}

static bool parse_statement(parser_ctx *p, statement stmt) {
   source s = p->src;

   si_skip_whitespace_and_comments(s);

   // Parse identifier (field name)
   usize ident_pos = 0, ident_len = 0;
   if (!read_identifier(p, &ident_pos, &ident_len))
      return false;

   si_skip_whitespace_and_comments(s);

   // Parse optional inheritance base FIRST (before attributes)
   // Check for ':' that is NOT part of ':=' (assignment operator)
   struct anvl_base_meta *base_meta = NULL;
   usize base_pos = 0, base_len = 0;
   if (si_match_length(s, ":", 1) == 1 && si_peek_offset(s, 1) != '=') {
      // AMP dialect does not allow inheritance (bare : operator)
      if (Source.dialect(s) == ANVL_DIALECT_AMP) {
         parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
         return false;
      }
      si_consume(s, 1);
      si_skip_whitespace_and_comments(s);
      if (!read_identifier(p, &base_pos, &base_len))
         return false;
      // Allocate base_meta and store inheritance information
      base_meta = ci_new_base_meta();
      if (!base_meta) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
         return false;
      }
      base_meta->pos = base_pos;
      base_meta->len = base_len;
      si_skip_whitespace_and_comments(s);
   }

   // Parse optional attributes (only valid on complex types, validated after parsing value)
   struct anvl_attr_meta *attr_meta = NULL;
   usize attrib_start = p->ctx->attr_list.count;
   while (si_match_length(s, "@[", 2) == 2) {
      // AMP dialect does not allow attributes
      if (Source.dialect(s) == ANVL_DIALECT_AMP) {
         parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
         if (base_meta)
            Memory.dispose(base_meta);
         return false;
      }
      if (!parse_attribute_block(p, NULL, NULL))
         return false;
      si_skip_whitespace_and_comments(s);
   }
   usize attrib_count = p->ctx->attr_list.count - attrib_start;
   if (attrib_count > 0) {
      // Allocate attr_meta array and populate with attribute information
      attr_meta = ci_new_attr_meta_array(attrib_count);
      if (!attr_meta) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
         if (base_meta)
            Memory.dispose(base_meta);
         return false;
      }
      // TODO: Populate attr_meta array from ctx->attr_list
      // Strategy: Copy attribute metadata from context's attr_list to local attr_meta array
      // Attributes were added in parse_attribute_block() and stored in ctx->attr_list
      // We have attrib_start index (where attributes for this statement start)
      // Need to copy: key_pos, key_len, value_pos, value_len to attr_meta entries
      // This allows statement queries without accessing global context
      for (usize i = 0; i < attrib_count; i++) {
         attribute attr = ci_get_attribute(p->ctx, attrib_start + i);
         if (!attr) {
            parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
            Memory.dispose(attr_meta);
            if (base_meta)
               Memory.dispose(base_meta);
            return false;
         }
         attr_meta[i].pos = attr->key_pos;
         attr_meta[i].len = attr->key_len;
         attr_meta[i].value_pos = attr->value_pos;
         attr_meta[i].value_len = attr->value_len;
      }
   }

   // Expect assignment operator
   if (si_match_length(s, ":=", 2) != 2) {
      if (base_meta)
         Memory.dispose(base_meta);
      if (attr_meta)
         Memory.dispose(attr_meta);
      parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, s);
      return false;
   }
   si_consume(s, 2);
   si_skip_whitespace_and_comments(s);

   // Track value position and parse value
   usize value_start_pos = si_position(s);
   value val = parse_value(p);
   usize value_end_pos = si_position(s);
   if (!val) {
      if (base_meta)
         Memory.dispose(base_meta);
      if (attr_meta)
         Memory.dispose(attr_meta);
      return false;
   }

   // Validate: attributes only valid on complex types
   if (attr_meta && val->type != ANVL_VALUE_OBJECT && val->type != ANVL_VALUE_ARRAY && val->type != ANVL_VALUE_TUPLE) {
      if (base_meta)
         Memory.dispose(base_meta);
      Memory.dispose(attr_meta);
      Memory.dispose(val);
      parser_error(ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE, s);
      return false;
   }

   // Allocate value_meta for this statement's value
   struct anvl_value_meta *value_meta = ci_new_value_meta(val->type);
   if (!value_meta) {
      if (base_meta)
         Memory.dispose(base_meta);
      if (attr_meta)
         Memory.dispose(attr_meta);
      Memory.dispose(val);
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return false;
   }

   // Populate value_meta with value information
   value_meta->pos = value_start_pos;
   value_meta->len = value_end_pos - value_start_pos;

   // For collection types, allocate element_meta array and copy element count
   if (val->type == ANVL_VALUE_ARRAY || val->type == ANVL_VALUE_TUPLE) {
      usize elem_count = val->data.collection.element_count;
      value_meta->data.collection.element_count = elem_count;

      if (elem_count > 0) {
         struct anvl_element_meta *elem_meta = ci_new_element_meta_array(elem_count);
         if (!elem_meta) {
            if (base_meta)
               Memory.dispose(base_meta);
            if (attr_meta)
               Memory.dispose(attr_meta);
            Memory.dispose(value_meta);
            parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
            return false;
         }

         // Populate element_meta array from temporary elem_types stored in val
         anvl_value_type *elem_types = (anvl_value_type *)val->data.collection._elem_types_temp;
         if (elem_types) {
            for (usize i = 0; i < elem_count; i++) {
               elem_meta[i].type = elem_types[i];
               // TODO: Store pos/len for each element when available
               // Strategy: Track element source positions during parse_array/parse_tuple
               // Currently, we only have elem_types from temporary buffer
               // Need to capture start/end position for each element as it's parsed
               // Would require position tracking in array/tuple parsing loops
            }
            // Clean up temporary elem_types array
            Memory.dispose(elem_types);
            val->data.collection._elem_types_temp = NULL;
         }

         value_meta->data.collection.elements = elem_meta;
      }
   } else if (val->type == ANVL_VALUE_OBJECT) {
      usize field_count = val->data.object.field_count;
      value_meta->data.object.field_count = field_count;
      // TODO: Allocate and populate field metadata
      // Strategy: Create an array of field metadata structs in value_meta
      // Would need to iterate val->data.object fields and extract pos/len info
      // Currently fields store key_pos, key_len + field values
      // Deferred until field metadata structure is finalized
   }

   // Fill statement metadata buffer with directional indices
   // NOTE: Always ASSN - inheritance is metadata (base_meta), not type
   stmt->meta[STMT_META_TYPE] = (usize)ANVL_STMT_ASSN;
   stmt->meta[STMT_META_RESERVED_1] = 0; // reserved
   stmt->meta[STMT_META_IDENT_POS] = ident_pos;
   stmt->meta[STMT_META_IDENT_LEN] = ident_len;
   stmt->meta[STMT_META_BASE_IDX] = base_meta ? 1 : 0;
   stmt->meta[STMT_META_ATTR_IDX] = attrib_count;
   stmt->meta[STMT_META_RESERVED_6] = 0; // reserved
   stmt->meta[STMT_META_VALUE_IDX] = 1;  // Indicates value_meta is present
   stmt->meta[STMT_META_RESERVED_8] = 0; // reserved

   // Attach self-contained meta-buffers to statement
   stmt->base_meta = base_meta;
   stmt->attr_meta = attr_meta;
   stmt->value_meta = value_meta;

   // Dispose the temporary value object (metadata is now in value_meta)
   Memory.dispose(val);

   si_skip_whitespace_and_comments(s);
   if (si_match_length(s, ",", 1) == 1) {
      si_consume(s, 1);
   }

   return true;
}

static value parse_value(parser_ctx *p) {
   source s = p->src;

   // AMP dialect validation: no complex types allowed
   anvl_dialect dialect = Source.dialect(s);
   if (dialect == ANVL_DIALECT_AMP) {
      if (si_match_length(s, "{", 1) == 1) {
         parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
         return NULL;
      }
      if (si_match_length(s, "[", 1) == 1) {
         parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
         return NULL;
      }
      if (si_match_length(s, "(", 1) == 1) {
         parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
         return NULL;
      }
   }

   if (si_match_length(s, "{", 1) == 1)
      return parse_object(p);
   if (si_match_length(s, "[", 1) == 1)
      return parse_array(p);
   if (si_match_length(s, "(", 1) == 1)
      return parse_tuple(p);
   if (si_match_length(s, "@", 1) == 1 || si_match_length(s, "`", 1) == 1)
      return parse_blob(p);

   usize start = 0, len = 0;
   anvl_value_type type = ANVL_VALUE_SCALAR;
   if (!parse_scalar_value(p, &start, &len, &type))
      return NULL;

   value v = ci_new_value(p->ctx, type);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }
   v->data.scalar.pos = start;
   v->data.scalar.len = len;
   return v;
}

/* ========================================================================
 * parse_blob() - Parse blob values
 *
 * Two encoding strategies based on dialect:
 *
 * Array-based (AML, ASL, AURORA):
 *   Blobs with tags like @json`...` are parsed as 2-element collections:
 *    [0] Tag metadata: pos/len of @identifier in source (type=BLOB)
 *    [1] Content: pos/len of backtick content (type=BLOB)
 *
 *   Bare blobs like `...` are parsed as 1-element collections:
 *    [0] Content: pos/len of backtick content (type=BLOB)
 *
 * Scalar-based (AMP):
 *   All blobs are stored as VALUE_BLOB scalars with encoded metadata:
 *    - position: source position of backtick content
 *    - length: (tag_length << 56) | content_length
 *     * Upper 8 bits = tag length
 *     * Lower 56 bits = content length
 *
 * ======================================================================== */
static value parse_blob(parser_ctx *p) {
   source s = p->src;
   anvl_dialect dialect = Source.dialect(s);

   /* Track tag information for encoding */
   uint8_t tag_len = 0;

   /* Check for optional @tag */
   if (si_match_length(s, "@", 1) == 1) {
      si_consume(s, 1); /* consume @ */

      while (!si_is_eof(s) && Source.is_identifier_part(si_peek(s))) {
         si_consume(s, 1);
         tag_len++;
      }

      if (tag_len == 0) { /* Only @ with no identifier */
         parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, s);
         return NULL;
      }

      /* BLOB ENCODING RULE: No whitespace allowed between tag and opening backtick
       * This ensures tag position is deterministic (immediately precedes content)
       */
      if (si_peek(s) != '`') {
         parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
         return NULL;
      }
   }

   /* BLOB content (between backticks) */
   if (si_peek(s) != '`') {
      parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
      return NULL;
   }
   si_consume(s, 1); /* consume opening backtick */

   usize content_pos = si_position(s);
   usize content_len = 0;

   while (!si_is_eof(s)) {
      if (si_peek(s) == '`')
         break;
      si_consume(s, 1);
      content_len++;
   }

   if (si_peek(s) != '`') {
      parser_error(ANVL_ERR_PARSER_UNTERMINATED_BLOB, s);
      return NULL;
   }
   si_consume(s, 1); /* consume closing backtick */

   /* Create blob value based on dialect */
   if (dialect == ANVL_DIALECT_AMP) {
      /* AMP: Scalar blob with encoded tag length in length field */
      value blob = ci_new_value(p->ctx, ANVL_VALUE_BLOB);
      if (!blob) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
         return NULL;
      }
      blob->data.scalar.pos = content_pos;
      blob->data.scalar.len = blob_encode_length(tag_len, content_len);
      return blob;
   } else {
      /* Array-based (AML, ASL, AURORA): Collection with tag and content as separate elements */
      value blob_collection = ci_new_value(p->ctx, ANVL_VALUE_ARRAY);
      if (!blob_collection) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
         return NULL;
      }

      /* Allocate for up to 2 elements (tag + content) */
      anvl_value_type *elem_types = Memory.alloc(sizeof(anvl_value_type) * 2, true);
      if (!elem_types) {
         Memory.dispose(blob_collection);
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
         return NULL;
      }

      usize element_count = 0;

      /* Element [0]: Tag metadata (if tag present) */
      if (tag_len > 0) {
         elem_types[0] = ANVL_VALUE_BLOB;
         element_count = 1;
      }

      /* Element [0 or 1]: Content */
      elem_types[element_count] = ANVL_VALUE_BLOB;
      element_count++;

      /* Set up collection metadata */
      blob_collection->data.collection.element_start = 0;
      blob_collection->data.collection.element_count = element_count;
      blob_collection->data.collection._elem_types_temp = elem_types;

      return blob_collection;
   }
}

static value parse_object(parser_ctx *p) {
   source s = p->src;

   // AMP dialect validation: objects not allowed
   if (Source.dialect(s) == ANVL_DIALECT_AMP) {
      parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
      return NULL;
   }

   si_consume(s, 1); // consume '{'
   si_skip_whitespace_and_comments(s);

   if (si_peek(s) == '}') {
      parser_error(ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED, s);
      return NULL;
   }

   value v = ci_new_value(p->ctx, ANVL_VALUE_OBJECT);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   // Track where fields start in the context pool
   usize field_start = p->ctx->field_list.count;
   usize field_count = 0;

   while (!si_is_eof(s) && si_peek(s) != '}') {
      usize key_pos = 0, key_len = 0;
      if (!read_identifier(p, &key_pos, &key_len)) {
         dispose_value_tree(v);
         return NULL;
      }

      si_skip_whitespace_and_comments(s);

      usize attrib_start = p->ctx->attr_list.count;
      while (si_match_length(s, "@[", 2) == 2) {
         if (!parse_attribute_block(p, NULL, NULL)) {
            dispose_value_tree(v);
            return NULL;
         }
         si_skip_whitespace_and_comments(s);
      }
      usize attrib_count = p->ctx->attr_list.count - attrib_start;

      if (si_match_length(s, ":=", 2) != 2) {
         dispose_value_tree(v);
         parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, s);
         return NULL;
      }
      si_consume(s, 2);
      si_skip_whitespace_and_comments(s);

      value field_val = parse_value(p);
      if (!field_val) {
         dispose_value_tree(v);
         return NULL;
      }

      // Create field and add to context pool
      field f = ci_new_field(p->ctx);
      f->key_pos = key_pos;
      f->key_len = key_len;
      f->val = field_val;
      f->attrib_start = attrib_start;
      f->attrib_count = attrib_count;
      ci_add_field(p->ctx, f);
      field_count++;

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, ",", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
      } else if (si_match_length(s, "}", 1) == 1) {
         break;
      } else {
         dispose_value_tree(v);
         parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES, s);
         return NULL;
      }
   }

   if (si_match_length(s, "}", 1) != 1) {
      dispose_value_tree(v);
      parser_error(ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE, s);
      return NULL;
   }

   si_consume(s, 1);

   v->data.object.field_start = field_start;
   v->data.object.field_count = field_count;
   return v;
}

static value parse_array(parser_ctx *p) {
   source s = p->src;

   // AMP dialect validation: arrays not allowed
   if (Source.dialect(s) == ANVL_DIALECT_AMP) {
      parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
      return NULL;
   }

   si_consume(s, 1); // consume '['
   si_skip_whitespace_and_comments(s);

   if (si_peek(s) == ']') {
      parser_error(ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED, s);
      return NULL;
   }

   value v = ci_new_value(p->ctx, ANVL_VALUE_ARRAY);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   // Build a temporary array to track element types
   anvl_value_type *elem_types = Memory.alloc(sizeof(anvl_value_type) * 256, true);
   if (!elem_types) {
      Memory.dispose(v);
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   usize element_count = 0;

   while (!si_is_eof(s) && si_peek(s) != ']') {
      value elem = parse_value(p);
      if (!elem) {
         Memory.dispose(elem_types);
         return NULL;
      }

      // Store element type for later use in element_meta population
      if (element_count < 256) {
         elem_types[element_count] = elem->type;
      }
      element_count++;

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, ",", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
      } else if (si_match_length(s, "]", 1) == 1) {
         break;
      } else {
         parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY, s);
         Memory.dispose(elem_types);
         return NULL;
      }
   }

   if (si_match_length(s, "]", 1) != 1) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE, s);
      Memory.dispose(elem_types);
      return NULL;
   }

   si_consume(s, 1);

   v->data.collection.element_start = 0; // Will be populated by value_meta
   v->data.collection.element_count = element_count;
   // Store element types pointer temporarily (will be used in parse_statement to populate element_meta)
   v->data.collection._elem_types_temp = elem_types;
   return v;
}

static value parse_tuple(parser_ctx *p) {
   source s = p->src;

   // AMP dialect validation: tuples not allowed
   if (Source.dialect(s) == ANVL_DIALECT_AMP) {
      parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
      return NULL;
   }

   si_consume(s, 1); // consume '('
   si_skip_whitespace_and_comments(s);

   value v = ci_new_value(p->ctx, ANVL_VALUE_TUPLE);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   // Build a temporary array to track element types
   anvl_value_type *elem_types = Memory.alloc(sizeof(anvl_value_type) * 256, true);
   if (!elem_types) {
      Memory.dispose(v);
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   usize element_count = 0;

   while (!si_is_eof(s) && si_peek(s) != ')') {
      value elem = parse_value(p);
      if (!elem) {
         Memory.dispose(elem_types);
         return NULL;
      }

      // Store element type for later use in element_meta population
      if (element_count < 256) {
         elem_types[element_count] = elem->type;
      }
      element_count++;

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, ",", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
      } else if (si_peek(s) == ')') {
         break;
      } else {
         if (si_is_eof(s))
            parser_error(ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE, s);
         else
            parser_error(ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE, s);
         Memory.dispose(elem_types);
         return NULL;
      }
   }

   if (si_match_length(s, ")", 1) != 1) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE, s);
      Memory.dispose(elem_types);
      return NULL;
   }

   si_consume(s, 1);

   v->data.collection.element_start = 0; // Will be populated by value_meta
   v->data.collection.element_count = element_count;
   // Store element types pointer temporarily (will be used in parse_statement to populate element_meta)
   v->data.collection._elem_types_temp = elem_types;
   return v;
}

static bool parse_scalar_value(parser_ctx *p, usize *start, usize *len, anvl_value_type *type) {
   source s = p->src;
   *start = si_position(s);
   *type = ANVL_VALUE_SCALAR;

   char first = si_peek(s);
   if (first == '{' || first == '[' || first == '(') {
      parser_error(ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE, s);
      return false;
   }

   if (si_match_length(s, "\"", 1) == 1) {
      si_consume(s, 1); // opening quote
      bool closed = false;
      while (!si_is_eof(s)) {
         char c = si_peek(s);
         if (c == '"') {
            si_consume(s, 1);
            closed = true;
            break;
         } else if (c == '\\') {
            si_consume(s, 1);
            if (!si_is_eof(s))
               si_consume(s, 1);
         } else {
            si_consume(s, 1);
         }
      }
      if (!closed) {
         parser_error(ANVL_ERR_PARSER_UNTERMINATED_STRING, s);
         return false;
      }
   } else {
      while (!si_is_eof(s)) {
         char c = si_peek(s);
         if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == '}' || c == ']' || c == ')')
            break;
         si_consume(s, 1);
      }
   }

   *len = si_position(s) - *start;
   return true;
}

static bool parse_attribute_block(parser_ctx *p, usize *out_start, usize *out_count) {
   source s = p->src;
   si_consume(s, 2); // consume "@["
   si_skip_whitespace_and_comments(s);

   if (si_match_length(s, "]", 1) == 1) {
      parser_error(ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK, s);
      return false;
   }

   usize start = p->ctx->attr_list.count;

   while (!si_is_eof(s)) {
      usize key_pos = si_position(s);
      usize key_len = 0;
      while (!si_is_eof(s) && (Source.is_identifier_part(si_peek(s)) || si_peek(s) == '.')) {
         si_consume(s, 1);
         key_len++;
      }
      if (key_len == 0) {
         parser_error(ANVL_ERR_PARSER_INVALID_ATTRIBUTE, s);
         return false;
      }

      usize value_pos = 0, value_len = 0;
      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, "=", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
         value_pos = si_position(s);
         anvl_value_type vt;
         if (!parse_scalar_value(p, &value_pos, &value_len, &vt))
            return false;
         if (vt != ANVL_VALUE_SCALAR) {
            parser_error(ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE, s);
            return false;
         }
      }

      attribute attr = ci_new_attribute(p->ctx);
      if (!attr) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
         return false;
      }
      attr->key_pos = key_pos;
      attr->key_len = key_len;
      attr->value_pos = value_pos;
      attr->value_len = value_len;
      ci_add_attribute(p->ctx, attr);

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, "]", 1) == 1) {
         si_consume(s, 1);
         break;
      }
      if (si_match_length(s, ",", 1) != 1) {
         parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES, s);
         return false;
      }
      si_consume(s, 1);
      si_skip_whitespace_and_comments(s);
   }

   if (out_start)
      *out_start = start;
   if (out_count)
      *out_count = p->ctx->attr_list.count - start;
   return true;
}

static bool read_identifier(parser_ctx *p, usize *pos, usize *len) {
   source s = p->src;
   if (!Source.is_identifier_start(si_peek(s))) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, s);
      return false;
   }
   *pos = si_position(s);
   *len = 0;
   while (!si_is_eof(s) && Source.is_identifier_part(si_peek(s))) {
      si_consume(s, 1);
      (*len)++;
   }
   if (*len == 0) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, s);
      return false;
   }
   return true;
}
