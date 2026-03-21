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
#include "vars.h"

#include <sigma.memory/memory.h>
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
static bool parse_vars_block(parser_ctx *p);
static value parse_varref(parser_ctx *p);
static value parse_interp_string(parser_ctx *p);
static bool parse_import_decl(parser_ctx *p);

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
   // All statements live in the context parse arena; no individual free needed.
   (void)stmt;
}

static bool parse_source(parser_ctx *p) {
   si_skip_whitespace_and_comments(p->src);

   // import declarations — must precede module attributes, vars, and all statements
   // import is not allowed in AMP dialect
   while (si_match_length(p->src, "import", 6) == 6 &&
          !Source.is_identifier_part(si_peek_offset(p->src, 6))) {
      if (Source.dialect(p->src) == ANVL_DIALECT_AMP) {
         parser_error(ANVL_ERR_IMPORT_AMP_FORBIDDEN, p->src);
         return false;
      }
      if (!parse_import_decl(p))
         return false;
      si_skip_whitespace_and_comments(p->src);
   }

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

   // vars block must come before all statements (optional; at most one)
   // Only trigger if "vars" is followed by '{' (after whitespace) — not ':='
   if (si_match_length(p->src, "vars", 4) == 4 &&
       !Source.is_identifier_part(si_peek_offset(p->src, 4))) {
      // Peek past 'vars' and any whitespace to look for '{'
      usize la = 4;
      while (!si_is_eof_offset(p->src, la)) {
         char lc = si_peek_offset(p->src, la);
         if (lc != ' ' && lc != '\t' && lc != '\r' && lc != '\n')
            break;
         la++;
      }
      if (!si_is_eof_offset(p->src, la) && si_peek_offset(p->src, la) == '{') {
         if (!parse_vars_block(p))
            return false;
         si_skip_whitespace_and_comments(p->src);
      }
   }

   while (!si_is_eof(p->src)) {
      // Shebang is illegal after statements
      if (si_match_length(p->src, "#!", 2) == 2) {
         parser_error(ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS, p->src);
         return false;
      }

      // vars block is illegal after statements (or if already seen)
      // Only check when "vars" is followed by '{' (real block, not vars-as-identifier)
      if (si_match_length(p->src, "vars", 4) == 4 &&
          !Source.is_identifier_part(si_peek_offset(p->src, 4))) {
         usize la = 4;
         while (!si_is_eof_offset(p->src, la)) {
            char lc = si_peek_offset(p->src, la);
            if (lc != ' ' && lc != '\t' && lc != '\r' && lc != '\n')
               break;
            la++;
         }
         if (!si_is_eof_offset(p->src, la) && si_peek_offset(p->src, la) == '{') {
            parser_error(p->ctx->vars_list.parsed
                             ? ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED
                             : ANVL_ERR_VARS_NOT_FIRST,
                         p->src);
            return false;
         }
      }

      // import after statements is illegal
      if (si_match_length(p->src, "import", 6) == 6 &&
          !Source.is_identifier_part(si_peek_offset(p->src, 6))) {
         parser_error(ANVL_ERR_IMPORT_NOT_FIRST, p->src);
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
      base_meta = ci_new_base_meta(p->ctx);
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
         return false;
      }
      if (!parse_attribute_block(p, NULL, NULL))
         return false;
      si_skip_whitespace_and_comments(s);
   }
   usize attrib_count = p->ctx->attr_list.count - attrib_start;
   if (attrib_count > 0) {
      // Allocate attr_meta array and populate with attribute information
      attr_meta = ci_new_attr_meta_array(p->ctx, attrib_count);
      if (!attr_meta) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
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
      return false;
   }

   // Validate: attributes only valid on complex types
   if (attr_meta && val->type != ANVL_VALUE_OBJECT && val->type != ANVL_VALUE_ARRAY && val->type != ANVL_VALUE_TUPLE) {
      parser_error(ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE, s);
      return false;
   }

   // Allocate value_meta for this statement's value
   struct anvl_value_meta *value_meta = ci_new_value_meta(p->ctx, val->type);
   if (!value_meta) {
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
         struct anvl_element_meta *elem_meta = ci_new_element_meta_array(p->ctx, elem_count);
         if (!elem_meta) {
            parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
            return false;
         }

         // Populate element_meta from the temporary tracking array (type + pos + len)
         struct anvl_element_meta *elem_temp =
             (struct anvl_element_meta *)val->data.collection._elem_types_temp;
         if (elem_temp) {
            for (usize i = 0; i < elem_count; i++) {
               elem_meta[i].type = elem_temp[i].type;
               elem_meta[i].pos = elem_temp[i].pos;
               elem_meta[i].len = elem_temp[i].len;
            }
            val->data.collection._elem_types_temp = NULL;
         }

         value_meta->data.collection.elements = elem_meta;
      }
   } else if (val->type == ANVL_VALUE_OBJECT) {
      value_meta->data.object.field_start = val->data.object.field_start;
      value_meta->data.object.field_count = val->data.object.field_count;
   } else if (val->type == ANVL_VALUE_VARREF) {
      // Override pos/len to identifier span (not including the '$' sigil)
      value_meta->pos = val->data.scalar.pos;
      value_meta->len = val->data.scalar.len;
   } else if (val->type == ANVL_VALUE_INTERP_STRING) {
      // Transfer segment ownership from temporary value to value_meta
      value_meta->data.interp_string.segment_count = val->data.interp.segment_count;
      value_meta->data.interp_string.segments = val->data.interp.segments_temp;
      val->data.interp.segments_temp = NULL; // ownership transferred
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

   // val was a temporary value object; its metadata is now in value_meta.
   // The arena owns val; no individual free needed.

   si_skip_whitespace_and_comments(s);
   if (si_match_length(s, ",", 1) == 1) {
      si_consume(s, 1);
   }

   return true;
}

static value parse_value(parser_ctx *p) {
   source s = p->src;

   // AMP dialect validation: objects not allowed
   anvl_dialect dialect = Source.dialect(s);
   if (dialect == ANVL_DIALECT_AMP) {
      if (si_match_length(s, "{", 1) == 1) {
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

   // VarRef ($identifier) or interpolated string ($"...")
   if (si_peek(s) == '$') {
      char next = si_peek_offset(s, 1);
      if (next == '"')
         return parse_interp_string(p);
      if (next == '`') {
         // '$' before backtick is invalid — blobs are always verbatim
         parser_error(ANVL_ERR_VARS_INVALID_VARREF, s);
         return NULL;
      }
      return parse_varref(p);
   }

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
      struct anvl_element_meta *elem_temp = p->ctx->arena->alloc(p->ctx->arena, sizeof(struct anvl_element_meta) * 2);
      if (elem_temp)
         memset(elem_temp, 0, sizeof(struct anvl_element_meta) * 2);
      if (!elem_temp) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
         return NULL;
      }

      usize element_count = 0;

      /* Element [0]: Tag metadata (if tag present) — pos/len are 0 (not needed by serializer) */
      if (tag_len > 0) {
         elem_temp[0].type = ANVL_VALUE_BLOB;
         element_count = 1;
      }

      /* Element [0 or 1]: Content — pos/len are 0 (serializer uses value_meta->pos/len) */
      elem_temp[element_count].type = ANVL_VALUE_BLOB;
      element_count++;

      /* Set up collection metadata */
      blob_collection->data.collection.element_start = 0;
      blob_collection->data.collection.element_count = element_count;
      blob_collection->data.collection._elem_types_temp = elem_temp;

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
      }
      /* Otherwise: field was newline-separated — continue to next field.
         si_skip_whitespace_and_comments already consumed any newlines, so
         the loop condition (peek != '}') will terminate correctly. */
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

   // Grow-by-doubling temp buffer: start small, never over-allocate.
   // Old buffers are orphaned in the arena (reclaimed at context dispose).
   // No frames used — parse_array is recursive (frames don't stack on one scope).
   usize elem_cap = 8;
   struct anvl_element_meta *elem_temp = p->ctx->arena->alloc(p->ctx->arena, sizeof(struct anvl_element_meta) * elem_cap);
   if (!elem_temp) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   usize element_count = 0;

   while (!si_is_eof(s) && si_peek(s) != ']') {
      usize elem_start = si_position(s);
      value elem = parse_value(p);
      usize elem_end = si_position(s);
      if (!elem) {
         return NULL;
      }

      // AMP: array elements must be scalar or blob
      if (Source.dialect(s) == ANVL_DIALECT_AMP &&
          elem->type != ANVL_VALUE_SCALAR &&
          elem->type != ANVL_VALUE_BLOB) {
         parser_error(ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR, s);
         return NULL;
      }

      // Grow buffer if full: alloc new (old stays in arena, harmless)
      if (element_count == elem_cap) {
         elem_cap *= 2;
         struct anvl_element_meta *grown = p->ctx->arena->alloc(p->ctx->arena, sizeof(struct anvl_element_meta) * elem_cap);
         if (!grown) {
            parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
            return NULL;
         }
         memcpy(grown, elem_temp, sizeof(struct anvl_element_meta) * element_count);
         elem_temp = grown;
      }

      elem_temp[element_count].type = elem->type;
      elem_temp[element_count].pos = elem_start;
      elem_temp[element_count].len = elem_end - elem_start;
      element_count++;

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, ",", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
      } else if (si_match_length(s, "]", 1) == 1) {
         break;
      } else {
         parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY, s);
         return NULL;
      }
   }

   if (si_match_length(s, "]", 1) != 1) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE, s);
      return NULL;
   }

   si_consume(s, 1);

   v->data.collection.element_start = 0;
   v->data.collection.element_count = element_count;
   v->data.collection._elem_types_temp = elem_temp;
   return v;
}

static value parse_tuple(parser_ctx *p) {
   source s = p->src;

   si_consume(s, 1); // consume '('
   si_skip_whitespace_and_comments(s);

   value v = ci_new_value(p->ctx, ANVL_VALUE_TUPLE);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   // Grow-by-doubling temp buffer: start small, never over-allocate.
   // Old buffers are orphaned in the arena (reclaimed at context dispose).
   // No frames used — parse_tuple is recursive (frames don't stack on one scope).
   usize elem_cap = 8;
   struct anvl_element_meta *elem_temp = p->ctx->arena->alloc(p->ctx->arena, sizeof(struct anvl_element_meta) * elem_cap);
   if (!elem_temp) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   usize element_count = 0;

   while (!si_is_eof(s) && si_peek(s) != ')') {
      usize elem_start = si_position(s);
      value elem = parse_value(p);
      usize elem_end = si_position(s);
      if (!elem) {
         return NULL;
      }

      // AMP: tuple elements must be scalar or blob
      if (Source.dialect(s) == ANVL_DIALECT_AMP &&
          elem->type != ANVL_VALUE_SCALAR &&
          elem->type != ANVL_VALUE_BLOB) {
         parser_error(ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR, s);
         return NULL;
      }

      // Grow buffer if full: alloc new (old stays in arena, harmless)
      if (element_count == elem_cap) {
         elem_cap *= 2;
         struct anvl_element_meta *grown = p->ctx->arena->alloc(p->ctx->arena, sizeof(struct anvl_element_meta) * elem_cap);
         if (!grown) {
            parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
            return NULL;
         }
         memcpy(grown, elem_temp, sizeof(struct anvl_element_meta) * element_count);
         elem_temp = grown;
      }

      elem_temp[element_count].type = elem->type;
      elem_temp[element_count].pos = elem_start;
      elem_temp[element_count].len = elem_end - elem_start;
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
         return NULL;
      }
   }

   if (si_match_length(s, ")", 1) != 1) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE, s);
      return NULL;
   }

   si_consume(s, 1);

   v->data.collection.element_start = 0;
   v->data.collection.element_count = element_count;
   v->data.collection._elem_types_temp = elem_temp;
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
   // Guard reserved keyword: 'vars'
   if (*len == 4 && strncmp(si_data(s) + *pos, "vars", 4) == 0) {
      parser_error(ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD, s);
      return false;
   }
   return true;
}

/* ========================================================================
 * parse_vars_block() - Parse a vars { } block
 *
 * Grammar: vars { key := value, ... }
 *
 * Constraints (enforced here):
 *   - Must appear before all statements (enforced in parse_source)
 *   - At most one vars block per source (enforced in parse_source)
 *   - No duplicate keys
 *   - No interpolated string values (interp strings not allowed in vars block)
 *
 * Populates ctx->vars_list.entries; sets ctx->vars_list.parsed = true.
 * ======================================================================== */
static bool parse_vars_block(parser_ctx *p) {
   source s = p->src;

   si_consume(s, 4); // consume "vars"
   si_skip_whitespace_and_comments(s);

   if (si_match_length(s, "{", 1) != 1) {
      parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
      return false;
   }
   si_consume(s, 1); // consume '{'
   si_skip_whitespace_and_comments(s);

   p->ctx->vars_list.parsed = true;

   while (!si_is_eof(s) && si_peek(s) != '}') {
      // Parse key
      usize key_pos = 0, key_len = 0;
      if (!read_identifier(p, &key_pos, &key_len))
         return false;

      si_skip_whitespace_and_comments(s);

      // Check for duplicate key
      const char *src_data = si_data(s);
      for (usize i = 0; i < p->ctx->vars_list.count; i++) {
         struct anvl_vars_entry *e = &p->ctx->vars_list.entries[i];
         if (e->key_len == key_len &&
             strncmp(src_data + e->key_pos, src_data + key_pos, key_len) == 0) {
            parser_error(ANVL_ERR_VARS_DUPLICATE_KEY, s);
            return false;
         }
      }

      // Expect ':='
      if (si_match_length(s, ":=", 2) != 2) {
         parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, s);
         return false;
      }
      si_consume(s, 2);
      si_skip_whitespace_and_comments(s);

      // Parse value
      usize value_start = si_position(s);
      value val = parse_value(p);
      usize value_end = si_position(s);
      if (!val)
         return false;

      // Interpolated strings are not allowed as vars block values
      if (val->type == ANVL_VALUE_INTERP_STRING) {
         parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
         return false;
      }

      // Clean up temporary internal data for collection types
      if ((val->type == ANVL_VALUE_ARRAY || val->type == ANVL_VALUE_TUPLE) &&
          val->data.collection._elem_types_temp) {
         val->data.collection._elem_types_temp = NULL;
      }

      // Determine value span: for VARREF, store the identifier span (not '$')
      usize vpos = (val->type == ANVL_VALUE_VARREF) ? val->data.scalar.pos : value_start;
      usize vlen = (val->type == ANVL_VALUE_VARREF) ? val->data.scalar.len
                                                    : (value_end - value_start);

      ci_add_vars_entry(p->ctx, (struct anvl_vars_entry){
                                    .key_pos = key_pos,
                                    .key_len = key_len,
                                    .value_pos = vpos,
                                    .value_len = vlen,
                                    .value_type = val->type,
                                });
      // val lives in the arena; no individual free needed.

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, ",", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
      }
   }

   if (si_match_length(s, "}", 1) != 1) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE, s);
      return false;
   }
   si_consume(s, 1); // consume '}'

   return true;
}

/* ========================================================================
 * parse_import_decl() - Parse a single import declaration
 *
 * Grammar:  import "path/to/file.aml" [as alias]
 *
 * Called from parse_source() before module attributes, vars, and statements.
 * The "import" keyword has already been confirmed (but not consumed) by the
 * caller.  All positions/lengths are byte offsets into the source buffer.
 * ======================================================================== */
static bool parse_import_decl(parser_ctx *p) {
   source s = p->src;
   si_consume(s, 6); // consume "import"
   si_skip_whitespace_and_comments(s);

   // Expect opening '"'
   if (si_peek(s) != '"') {
      parser_error(ANVL_ERR_PARSER_UNEXPECTED_CHAR, s);
      return false;
   }
   si_consume(s, 1); // opening '"'

   usize path_pos = si_position(s);
   bool closed = false;
   while (!si_is_eof(s)) {
      char c = si_peek(s);
      if (c == '"') {
         closed = true;
         break;
      }
      if (c == '\\') {
         si_consume(s, 1);
         if (!si_is_eof(s))
            si_consume(s, 1);
      } else {
         si_consume(s, 1);
      }
   }
   usize path_len = si_position(s) - path_pos;
   if (!closed) {
      parser_error(ANVL_ERR_PARSER_UNTERMINATED_STRING, s);
      return false;
   }
   si_consume(s, 1); // closing '"'
   si_skip_whitespace_and_comments(s);

   // Optional:  as <alias>
   usize alias_pos = 0;
   usize alias_len = 0;
   if (si_match_length(s, "as", 2) == 2 &&
       !Source.is_identifier_part(si_peek_offset(s, 2))) {
      si_consume(s, 2); // consume "as"
      si_skip_whitespace_and_comments(s);
      if (!Source.is_identifier_start(si_peek(s))) {
         parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, s);
         return false;
      }
      alias_pos = si_position(s);
      while (!si_is_eof(s) && Source.is_identifier_part(si_peek(s))) {
         si_consume(s, 1);
         alias_len++;
      }
   }

   ci_add_import_decl(p->ctx, (struct anvl_import_decl){
                                  .path_pos = path_pos,
                                  .path_len = path_len,
                                  .alias_pos = alias_pos,
                                  .alias_len = alias_len,
                              });
   return true;
}

/* ========================================================================
 * parse_varref() - Parse $identifier
 *
 * Consumes the '$' sigil and reads the following identifier.
 * Returns a ANVL_VALUE_VARREF value with scalar.pos/len = identifier span.
 * ======================================================================== */
static value parse_varref(parser_ctx *p) {
   source s = p->src;
   si_consume(s, 1); // consume '$'

   if (!Source.is_identifier_start(si_peek(s))) {
      parser_error(ANVL_ERR_VARS_INVALID_VARREF, s);
      return NULL;
   }

   usize ident_pos = si_position(s);
   usize ident_len = 0;
   while (!si_is_eof(s) && Source.is_identifier_part(si_peek(s))) {
      si_consume(s, 1);
      ident_len++;
   }

   value v = ci_new_value(p->ctx, ANVL_VALUE_VARREF);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }
   v->data.scalar.pos = ident_pos;
   v->data.scalar.len = ident_len;
   return v;
}

/* ========================================================================
 * parse_interp_string() - Parse $"…{ref}…"
 *
 * Grammar:
 *   $"  ( literal_char | '{{' | '}}' | '{' identifier '}' )*  "
 *
 * Segments:
 *   - Literal span: is_ref=false, pos=source_start, len=source_length
 *   - '{{' escape:  is_ref=false, pos=first_brace_pos, len=1 (emits single '{')
 *   - '}}' escape:  is_ref=false, pos=first_brace_pos, len=1 (emits single '}')
 *   - Ref hole:     is_ref=true,  pos=identifier_start, len=identifier_length
 *
 * Returns ANVL_VALUE_INTERP_STRING with interp.segments_temp set.
 * Caller (parse_statement) must transfer ownership to value_meta.
 * ======================================================================== */
static value parse_interp_string(parser_ctx *p) {
   source s = p->src;
   si_consume(s, 1); // consume '$'
   si_consume(s, 1); // consume '"'

   // Dynamic segment array (allocated from context parse arena)
   usize cap = 16;
   struct anvl_interp_segment *segs =
       p->ctx->arena->alloc(p->ctx->arena, sizeof(struct anvl_interp_segment) * cap);
   if (!segs) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }
   memset(segs, 0, sizeof(struct anvl_interp_segment) * cap);
   usize nseg = 0;

   // Helper: grow segment array if needed (old buffer left in arena; new buffer allocated)
#define SEGS_PUSH(is_r, p_, l_)                                                                \
   do {                                                                                        \
      if (nseg >= cap) {                                                                       \
         usize newcap_ = cap * 2;                                                              \
         struct anvl_interp_segment *nb_ =                                                     \
             p->ctx->arena->alloc(p->ctx->arena, sizeof(struct anvl_interp_segment) * newcap_);              \
         if (!nb_) {                                                                           \
            parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);                                \
            return NULL;                                                                       \
         }                                                                                     \
         memset(nb_, 0, sizeof(struct anvl_interp_segment) * newcap_);                         \
         memcpy(nb_, segs, sizeof(struct anvl_interp_segment) * nseg);                         \
         segs = nb_;                                                                           \
         cap = newcap_;                                                                        \
      }                                                                                        \
      segs[nseg++] = (struct anvl_interp_segment){.is_ref = (is_r), .pos = (p_), .len = (l_)}; \
   } while (0)

   usize lit_start = si_position(s);
   usize lit_len = 0;
   bool closed = false;

   while (!si_is_eof(s)) {
      char ch = si_peek(s);

      if (ch == '"') {
         // Closing quote: flush any pending literal, then done
         if (lit_len > 0)
            SEGS_PUSH(false, lit_start, lit_len);
         si_consume(s, 1);
         closed = true;
         break;
      }

      if (ch == '{') {
         if (si_peek_offset(s, 1) == '{') {
            // '{{' → escaped literal '{'
            if (lit_len > 0)
               SEGS_PUSH(false, lit_start, lit_len);
            SEGS_PUSH(false, si_position(s), 1); // only first '{'
            si_consume(s, 2);
            lit_start = si_position(s);
            lit_len = 0;
            continue;
         }
         // '{identifier}' — reference hole
         if (lit_len > 0)
            SEGS_PUSH(false, lit_start, lit_len);
         si_consume(s, 1); // consume '{'
         if (!Source.is_identifier_start(si_peek(s))) {
            parser_error(ANVL_ERR_VARS_INVALID_VARREF, s);
            return NULL;
         }
         usize ipos = si_position(s);
         usize ilen = 0;
         while (!si_is_eof(s) && Source.is_identifier_part(si_peek(s))) {
            si_consume(s, 1);
            ilen++;
         }
         if (si_peek(s) != '}') {
            parser_error(ANVL_ERR_VARS_UNTERMINATED_INTERP, s);
            return NULL;
         }
         si_consume(s, 1); // consume '}'
         SEGS_PUSH(true, ipos, ilen);
         lit_start = si_position(s);
         lit_len = 0;
         continue;
      }

      if (ch == '}' && si_peek_offset(s, 1) == '}') {
         // '}}' → escaped literal '}'
         if (lit_len > 0)
            SEGS_PUSH(false, lit_start, lit_len);
         SEGS_PUSH(false, si_position(s), 1); // only first '}'
         si_consume(s, 2);
         lit_start = si_position(s);
         lit_len = 0;
         continue;
      }

      // Ordinary character — accumulate into literal span
      if (lit_len == 0)
         lit_start = si_position(s);
      si_consume(s, 1);
      lit_len++;
   }

#undef SEGS_PUSH

   if (!closed) {
      parser_error(ANVL_ERR_VARS_UNTERMINATED_INTERP, s);
      return NULL;
   }

   value v = ci_new_value(p->ctx, ANVL_VALUE_INTERP_STRING);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }
   v->data.interp.segment_count = nseg;
   v->data.interp.segments_temp = segs;
   return v;
}
