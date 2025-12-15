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
 * parser.c - Parser implementation for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: src/core/parser.c
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "errors.h"
#include <sigcore/memory.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Parser Context Structure                                          */
/* ------------------------------------------------------------------ */
typedef struct {
   context ctx;
   source src;
} parser_ctx;

/* ------------------------------------------------------------------ */
/* Forward Declarations                                              */
/* ------------------------------------------------------------------ */
static void parser_error(anvl_error_code code, usize line, usize col);
static bool parse_source(parser_ctx *p);
static bool parse_statement(parser_ctx *p);
static bool parse_value(parser_ctx *p, usize *start, usize *len, anvl_value_type *type);
static bool parse_attribute_block(parser_ctx *p);

/* ------------------------------------------------------------------ */
/* Public Parser Interface                                           */
/* ------------------------------------------------------------------ */
bool anvl_parse(context ctx) {
   if (!ctx || !ctx->source) {
      return false;
   }

   parser_ctx p = {
       .ctx = ctx,
       .src = ctx->source};

   // Clear any existing errors
   anvl_error_clear();

   return parse_source(&p);
}

/* ------------------------------------------------------------------ */
/* Parser Implementation                                             */
/* ------------------------------------------------------------------ */
static void parser_error(anvl_error_code code, usize line, usize col) {
   anvl_error_set(code, anvl_error_code_message(code), line, col, __FILE__);
}

static bool parse_source(parser_ctx *p) {
   // Skip initial whitespace and comments
   Source.skip_whitespace_and_comments(p->src);

   // Parse module attributes
   while (Source.match_length(p->src, "@[", 2) == 2) {
      if (!parse_attribute_block(p)) {
         return false;
      }
      Source.skip_whitespace_and_comments(p->src);
   }

   // Parse statements
   while (!Source.is_eof(p->src)) {
      if (!parse_statement(p)) {
         return false;
      }
      Source.skip_whitespace_and_comments(p->src);
   }

   return true;
}

static bool parse_statement(parser_ctx *p) {
   usize start_pos = Source.position(p->src);

   // Parse identifier
   usize ident_start = Source.position(p->src);
   usize ident_len = 0;

   // Skip whitespace
   Source.skip_whitespace_and_comments(p->src);

   // Check for identifier
   if (!Source.is_identifier_start(Source.peek(p->src))) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, Source.line(p->src), Source.column(p->src));
      return false;
   }

   // Read identifier
   while (!Source.is_eof(p->src) && Source.is_identifier_part(Source.peek(p->src))) {
      Source.consume(p->src, 1);
      ident_len++;
   }

   if (ident_len == 0) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, Source.line(p->src), Source.column(p->src));
      return false;
   }

   // Skip whitespace
   Source.skip_whitespace_and_comments(p->src);

   // Check for assignment operator first
   if (Source.match_operator(p->src, ":=", 2) == 2) {
      Source.consume(p->src, 2);
      Source.skip_whitespace_and_comments(p->src);
   } else {
      // Check for inheritance (optional)
      usize base_len = 0;
      if (Source.match_length(p->src, ":", 1) == 1) {
         Source.consume(p->src, 1);
         Source.skip_whitespace_and_comments(p->src);

         // Read base identifier
         while (!Source.is_eof(p->src) && Source.is_identifier_part(Source.peek(p->src))) {
            Source.consume(p->src, 1);
            base_len++;
         }

         if (base_len == 0) {
            parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, Source.line(p->src), Source.column(p->src));
            return false;
         }

         Source.skip_whitespace_and_comments(p->src);
      }

      // Parse attributes (optional)
      // TODO: Implement attribute parsing

      // Expect assignment operator
      if (Source.match_operator(p->src, ":=", 2) != 2) {
         parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, Source.line(p->src), Source.column(p->src));
         return false;
      }
      Source.consume(p->src, 2);

      Source.skip_whitespace_and_comments(p->src);
   }

   // Parse value
   usize value_start, value_len;
   anvl_value_type value_type;
   if (!parse_value(p, &value_start, &value_len, &value_type)) {
      return false;
   }

   // Create statement
   statement stmt = Memory.alloc(sizeof(struct anvl_statement), true);
   stmt->type = ANVL_STMT_ASSN;
   stmt->stmt_len = Source.position(p->src) - start_pos;
   stmt->ident_pos = ident_start;
   stmt->ident_len = ident_len;
   stmt->attribs_len = 0; // TODO: attributes
   stmt->attribs_count = 0;
   stmt->value_type = value_type;
   stmt->value_pos = value_start;
   stmt->value_len = value_len;

   // Add to context
   if (!Context.add_statement(p->ctx, stmt)) {
      Memory.dispose(stmt);
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, Source.line(p->src), Source.column(p->src));
      return false;
   }

   return true;
}

static bool parse_value(parser_ctx *p, usize *start, usize *len, anvl_value_type *type) {
   *start = Source.position(p->src);

   // Check for different value types
   if (Source.match_operator(p->src, "{", 1) == 1) {
      *type = ANVL_VALUE_OBJECT;
      // Parse object
      Source.consume(p->src, 1); // consume '{'

      // For objects, store fields in field list
      usize field_start_index = Context.field_count(p->ctx);
      usize field_count = 0;

      // Skip whitespace after '{'
      Source.skip_whitespace_and_comments(p->src);

      // Check for empty object
      if (Source.match_operator(p->src, "}", 1) == 1) {
         Source.consume(p->src, 1);
      } else {
         // Parse object fields
         while (true) {
            // Parse field key
            usize key_start = Source.position(p->src);
            usize key_len = 0;

            // Skip whitespace
            Source.skip_whitespace_and_comments(p->src);

            // Check for identifier start
            if (!Source.is_identifier_start(Source.peek(p->src))) {
               parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, Source.line(p->src), Source.column(p->src));
               return false;
            }

            // Read key identifier
            while (!Source.is_eof(p->src) && Source.is_identifier_part(Source.peek(p->src))) {
               Source.consume(p->src, 1);
               key_len++;
            }

            if (key_len == 0) {
               parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, Source.line(p->src), Source.column(p->src));
               return false;
            }

            // Skip whitespace after key
            Source.skip_whitespace_and_comments(p->src);

            // Expect ':='
            if (Source.match_operator(p->src, ":=", 2) != 2) {
               parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, Source.line(p->src), Source.column(p->src));
               return false;
            }
            Source.consume(p->src, 2);

            // Skip whitespace after ':='
            Source.skip_whitespace_and_comments(p->src);

            // Parse field value
            usize value_start, value_len;
            anvl_value_type value_type;
            if (!parse_value(p, &value_start, &value_len, &value_type)) {
               return false;
            }

            // Create field entry
            field fld = Memory.alloc(sizeof(struct anvl_field), true);
            fld->key_pos = key_start;
            fld->key_len = key_len;
            fld->value_pos = value_start;
            fld->value_len = value_len;
            fld->value_type = value_type;

            if (!Context.add_field(p->ctx, fld)) {
               Memory.dispose(fld);
               parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, Source.line(p->src), Source.column(p->src));
               return false;
            }

            field_count++;

            // Skip whitespace after value
            Source.skip_whitespace_and_comments(p->src);

            // Check for comma or end
            if (Source.match_operator(p->src, ",", 1) == 1) {
               Source.consume(p->src, 1);
               Source.skip_whitespace_and_comments(p->src);
            } else if (Source.match_operator(p->src, "}", 1) == 1) {
               Source.consume(p->src, 1);
               break;
            } else {
               parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES, Source.line(p->src), Source.column(p->src));
               return false;
            }
         }
      }

      // For objects, return the field info instead of source positions
      *start = field_start_index; // starting index in field list
      *len = field_count;         // number of fields
      return true;
   } else if (Source.match_operator(p->src, "[", 1) == 1) {
      *type = ANVL_VALUE_ARRAY;
      // Parse array
      Source.consume(p->src, 1); // consume '['

      // For now, just find the matching ']'
      usize element_start_index = Context.value_count(p->ctx);
      usize element_count = 0;

      // Skip whitespace after '['
      Source.skip_whitespace_and_comments(p->src);

      // Check for empty array
      if (Source.match_operator(p->src, "]", 1) == 1) {
         Source.consume(p->src, 1);
      } else {
         // Parse array elements
         while (true) {
            // Parse element value
            usize elem_start, elem_len;
            anvl_value_type elem_type;
            if (!parse_value(p, &elem_start, &elem_len, &elem_type)) {
               return false;
            }

            // Create value entry for this element
            value elem_value = Memory.alloc(sizeof(struct anvl_value), true);
            elem_value->type = elem_type;
            elem_value->pos = elem_start;
            elem_value->len = elem_len;

            if (!Context.add_value(p->ctx, elem_value)) {
               Memory.dispose(elem_value);
               parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, Source.line(p->src), Source.column(p->src));
               return false;
            }

            element_count++;

            // Skip whitespace after element
            Source.skip_whitespace_and_comments(p->src);

            // Check for comma or end
            if (Source.match_operator(p->src, ",", 1) == 1) {
               Source.consume(p->src, 1);
               Source.skip_whitespace_and_comments(p->src);
            } else if (Source.match_operator(p->src, "]", 1) == 1) {
               Source.consume(p->src, 1);
               break;
            } else {
               parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY, Source.line(p->src), Source.column(p->src));
               return false;
            }
         }
      }

      // For arrays, return the element info instead of source positions
      *start = element_start_index; // starting index in value list
      *len = element_count;         // number of elements
      // For now, just mark the entire array as parsed
      return true;
   } else if (Source.match_operator(p->src, "(", 1) == 1) {
      *type = ANVL_VALUE_TUPLE;
      // Parse tuple
      Source.consume(p->src, 1); // consume '('

      // For tuples, store elements in value list
      usize element_start_index = Context.value_count(p->ctx);
      usize element_count = 0;

      // Skip whitespace after '('
      Source.skip_whitespace_and_comments(p->src);

      // Check for empty tuple
      if (Source.match_operator(p->src, ")", 1) == 1) {
         Source.consume(p->src, 1);
      } else {
         // Parse tuple elements
         while (true) {
            // Parse element value
            usize elem_start, elem_len;
            anvl_value_type elem_type;
            if (!parse_value(p, &elem_start, &elem_len, &elem_type)) {
               return false;
            }

            // Create value entry for this element
            value elem_value = Memory.alloc(sizeof(struct anvl_value), true);
            elem_value->type = elem_type;
            elem_value->pos = elem_start;
            elem_value->len = elem_len;

            if (!Context.add_value(p->ctx, elem_value)) {
               Memory.dispose(elem_value);
               parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, Source.line(p->src), Source.column(p->src));
               return false;
            }

            element_count++;

            // Skip whitespace after element
            Source.skip_whitespace_and_comments(p->src);

            // Check for comma or end
            if (Source.match_operator(p->src, ",", 1) == 1) {
               Source.consume(p->src, 1);
               Source.skip_whitespace_and_comments(p->src);
            } else if (Source.match_operator(p->src, ")", 1) == 1) {
               Source.consume(p->src, 1);
               break;
            } else {
               parser_error(ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE, Source.line(p->src), Source.column(p->src));
               return false;
            }
         }
      }

      // For tuples, return the element info instead of source positions
      *start = element_start_index; // starting index in value list
      *len = element_count;         // number of elements
      return true;
   } else if (Source.match_operator(p->src, "\"", 1) == 1) {
      *type = ANVL_VALUE_SCALAR;
      // Parse string content
      Source.consume(p->src, 1); // consume opening quote

      bool found_closing_quote = false;
      while (!Source.is_eof(p->src)) {
         char c = Source.peek(p->src);
         if (c == '"') {
            // Found closing quote
            Source.consume(p->src, 1);
            found_closing_quote = true;
            break;
         } else if (c == '\\') {
            // Handle escape sequence
            Source.consume(p->src, 1); // consume backslash
            if (!Source.is_eof(p->src)) {
               Source.consume(p->src, 1); // consume escaped character
            }
         } else {
            Source.consume(p->src, 1);
         }
      }

      // Check if we found the closing quote
      if (!found_closing_quote) {
         parser_error(ANVL_ERR_PARSER_UNTERMINATED_STRING, Source.line(p->src), Source.column(p->src));
         return false;
      }
   } else if (Source.match_operator(p->src, "`", 1) == 1) {
      *type = ANVL_VALUE_BLOB;
      // TODO: Parse blob
      parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, Source.line(p->src), Source.column(p->src));
      return false;
   } else {
      // Parse scalar (number, boolean, null, identifier)
      *type = ANVL_VALUE_SCALAR;

      // Skip to next whitespace or operator
      while (!Source.is_eof(p->src)) {
         char c = Source.peek(p->src);
         if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
             c == ',' || c == '}' || c == ']' || c == ')') {
            break;
         }
         Source.consume(p->src, 1);
      }
   }

   *len = Source.position(p->src) - *start;
   return true;
}

static bool parse_attribute_block(parser_ctx *p) {
   // Consume "@["
   Source.consume(p->src, 2);
   Source.skip_whitespace_and_comments(p->src);

   // Check for empty attribute block
   if (Source.match_length(p->src, "]", 1) == 1) {
      parser_error(ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK, Source.line(p->src), Source.column(p->src));
      return false;
   }

   while (!Source.is_eof(p->src)) {
      // Parse attribute key
      usize key_start = Source.position(p->src);
      usize key_len = 0;

      // Read identifier (may contain dots)
      while (!Source.is_eof(p->src) &&
             (Source.is_identifier_part(Source.peek(p->src)) || Source.peek(p->src) == '.')) {
         Source.consume(p->src, 1);
         key_len++;
      }

      if (key_len == 0) {
         parser_error(ANVL_ERR_PARSER_INVALID_ATTRIBUTE, Source.line(p->src), Source.column(p->src));
         return false;
      }

      usize value_start = 0;
      usize value_len = 0;

      Source.skip_whitespace_and_comments(p->src);

      // Check for optional value
      if (Source.match_length(p->src, "=", 1) == 1) {
         Source.consume(p->src, 1);
         Source.skip_whitespace_and_comments(p->src);

         // Parse scalar value
         value_start = Source.position(p->src);
         anvl_value_type value_type;
         if (!parse_value(p, &value_start, &value_len, &value_type)) {
            return false;
         }

         // Attributes must have scalar values
         if (value_type != ANVL_VALUE_SCALAR) {
            parser_error(ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE, Source.line(p->src), Source.column(p->src));
            return false;
         }
      }

      // Create attribute
      attribute attr = Memory.alloc(sizeof(struct anvl_attribute), true);
      attr->key_pos = key_start;
      attr->key_len = key_len;
      attr->value_pos = value_start;
      attr->value_len = value_len;

      // Add to context
      if (!Context.add_attribute(p->ctx, attr)) {
         Memory.dispose(attr);
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, Source.line(p->src), Source.column(p->src));
         return false;
      }

      Source.skip_whitespace_and_comments(p->src);

      // Check for end of attribute block
      if (Source.match_length(p->src, "]", 1) == 1) {
         Source.consume(p->src, 1);
         break;
      }

      // Expect comma
      if (Source.match_length(p->src, ",", 1) != 1) {
         parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES, Source.line(p->src), Source.column(p->src));
         return false;
      }
      Source.consume(p->src, 1);
      Source.skip_whitespace_and_comments(p->src);
   }

   return true;
}
