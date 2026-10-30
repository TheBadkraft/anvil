/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 *                                                                        *
 * This software is proprietary and confidential. Unauthorized copying,   *
 * distribution, modification, or use of this software, via any medium,   *
 * is strictly prohibited without express written permission from the     *
 * copyright holder.                                                      *
 *                                                                        *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * asl_parser.c - AnvlScript function declaration signature (WITH_ASL)    *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: src/core/asl_parser.c                                            *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * See include/internal/asl_parser.h. Parses the signature only: name,    *
 * optional attributes (parse_statement's own concern), the full          *
 * parameter list (plain identifiers, each with an optional literal       *
 * default or a trailing '[]' parameter-array marker on the last one),    *
 * and the body's raw span (brace-depth-aware; string/comment content     *
 * doesn't miscount). The body's own AnvlScript grammar (statements,      *
 * expressions) is never parsed here -- that's a separate component, see  *
 * notes/anvilscript-design.md's "ANVL/AnvlScript boundary".              *
 * ********************************************************************** */
#include "internal/asl_parser.h"

#if WITH_ASL

#include "internal/parser.h"
#include "internal/source.h"
#include "constants.h"
#include "errors.h"

#include <string.h>

// True if two slices have identical content. Small, local duplicate of resolver.c's own
// slices_equal (a two-line length+memcmp check) rather than exposing that one too -- unlike
// parser_set_error/parse_identifier, there's no real shared rule here worth single-sourcing.
static bool asl_slices_equal(anvl_slice a, anvl_slice b) {
   usize len = Source.slice_length(a);
   if (len != Source.slice_length(b)) {
      return false;
   }
   return len == 0 || memcmp(a.start, b.start, len) == 0;
}

// Only the anvl_asl_param_t wrapper is disposed here, never param->default_value -- that's an
// arena-allocated anvl_value (parsed via parse_scalar_value the same way any other scalar
// value is), already indexed in ctx->values and released by the existing value-tree pass
// (src/core/module.c) or by the arena itself on an early-return before that node was even
// reached. Disposing it here too would be a double-free.
static void asl_dispose_params(list params) {
   if (!params) {
      return;
   }
   usize count = List.size(params);
   for (usize i = 0; i < count; i++) {
      anvl_asl_param param = NULL;
      List.get(params, i, (object *)&param);
      Allocator.dispose(param);
   }
   List.dispose(params);
}

// Parses a function declaration's full parameter list, both delimiters included -- from the
// opening '(' through the matching ')'. Mirrors parse_attribute_list's own shape in parser.c
// (owns both of its delimiters start to finish, not split between it and its caller), so
// asl_parse_function_decl can treat "the parameter list" as one step, the same way
// parse_statement treats "the attribute list" as one call to parse_attribute_list.
//
// Plain identifier parameters, with an optional literal default -- a trailing '[]' vararg
// marker (notes/anvilscript-design.md's EBNF) is a later slice. *out_params is set to NULL
// for zero parameters (matching anvl_statement_t.attributes' own "NULL means none"
// convention), or a heap-allocated list of anvl_asl_param on success. On failure, *out_params
// is left untouched -- the caller never needs to dispose anything itself, this function
// cleans up after its own failures entirely.
static bool asl_parse_param_list(anvl_source src, list *out_params) {
   Source.consume(src, 1); // '(' -- caller only peeked
   Source.skip_whitespace_and_comments(src);

   if (Source.peek(src) == ANVL_TOK_RPAREN) {
      Source.consume(src, 1); // ')'
      *out_params = NULL;
      return true;
   }

   list params = List.new(4, sizeof(void *));
   if (!params) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false;
   }

   while (true) {
      Source.skip_whitespace_and_comments(src);

      anvl_slice name;
      if (!parse_identifier(src, &name)) {
         parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
         asl_dispose_params(params);
         return false;
      }

      // Validate parameter uniqueness as each name is collected -- O(n^2) over the
      // already-seen list, same tradeoff as attribute-key dedup elsewhere in this file's AML
      // counterpart: a function's own parameter count is small enough that this isn't worth
      // a Map just to make the check O(1).
      usize seen = List.size(params);
      for (usize i = 0; i < seen; i++) {
         anvl_asl_param other = NULL;
         List.get(params, i, (object *)&other);
         if (other && asl_slices_equal(other->name, name)) {
            parser_set_error(src, ANVL_ERR_ASL_DUPLICATE_PARAMETER);
            asl_dispose_params(params);
            return false;
         }
      }

      anvl_asl_param param = Allocator.alloc(sizeof(anvl_asl_param_t));
      if (!param) {
         parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
         asl_dispose_params(params);
         return false;
      }
      param->name = name;
      param->default_value = NULL;
      param->is_vararg = false;

      // A parameter is either a default literal ('= value') or a parameter-array marker
      // ('[]') -- mutually exclusive alternatives in the grammar, never both.
      Source.skip_whitespace_and_comments(src);
      if (Source.peek(src) == '=') {
         Source.consume(src, 1);
         Source.skip_whitespace_and_comments(src);

         // parse_scalar_value (like every sub-parser it dispatches to) only ever writes into
         // an already-allocated *out_value -- it never allocates one itself. The
         // ANVL-grammar caller (parse_value, parser.c) always pre-allocates via
         // Source.new_node first; this is that same precondition, met here too.
         anvl_err_code default_err_code;
         param->default_value = Source.new_node(src, ANVL_NODE_VALUE, &default_err_code);
         if (!param->default_value) {
            parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
            Allocator.dispose(param);
            asl_dispose_params(params);
            return false;
         }
         if (!parse_scalar_value(src, &param->default_value)) {
            parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_VALUE);
            Allocator.dispose(param);
            asl_dispose_params(params);
            return false;
         }
      } else if (Source.peek(src) == ANVL_TOK_LBRACKET) {
         // Bare '[]' only -- nothing may appear between the brackets (no size, no content).
         Source.consume(src, 1); // '['
         Source.skip_whitespace_and_comments(src);
         if (Source.peek(src) != ANVL_TOK_RBRACKET) {
            parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
            Allocator.dispose(param);
            asl_dispose_params(params);
            return false;
         }
         Source.consume(src, 1); // ']'
         param->is_vararg = true;
      }

      List.append(params, param);

      Source.skip_whitespace_and_comments(src);
      if (Source.peek(src) == ',') {
         // "A final parameter may be marked as a parameter-array" -- a vararg that isn't
         // last is rejected here, before even looking at what follows the comma.
         if (param->is_vararg) {
            parser_set_error(src, ANVL_ERR_ASL_VARARG_NOT_LAST);
            asl_dispose_params(params);
            return false;
         }
         Source.consume(src, 1);
         continue;
      }
      break;
   }

   Source.skip_whitespace_and_comments(src);
   if (Source.peek(src) != ANVL_TOK_RPAREN) {
      parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
      asl_dispose_params(params);
      return false;
   }
   Source.consume(src, 1); // ')'

   *out_params = params;
   return true;
}

// Scans a function body from the opening '{' through its matching '}' (both consumed),
// producing the raw, untrimmed interior as *out_body. Brace-depth-aware -- a nested '{'/'}'
// doesn't end the scan early -- and skips string-literal and comment content without
// counting braces inside them. Never parses the body's own AnvlScript grammar (statements,
// expressions); that's a separate, later component (the ScriptEngine's own body/AST parser,
// not this file -- see notes/anvilscript-design.md's "ANVL/AnvlScript boundary").
static bool asl_scan_body(anvl_source src, anvl_slice *out_body) {
   Source.consume(src, 1); // '{'

   // Leading whitespace/comments are skipped before body.start is captured, not after --
   // otherwise incidental whitespace in an empty '{ }' would count as real content (see
   // ASL06's own empty-body test).
   Source.skip_whitespace_and_comments(src);

   anvl_slice body;
   Source.init_slice(src, &body);
   body.start = Source.at(src);

   int depth = 1;
   bool in_string = false;
   while (!Source.is_eof(src) && depth > 0) {
      char c = Source.peek(src);

      if (in_string) {
         if (c == ANVL_TOK_ESCAPE) {
            Source.consume(src, 1);
            if (!Source.is_eof(src)) {
               Source.consume(src, 1); // the escaped character itself
            }
            continue;
         }
         if (c == ANVL_TOK_QUOTE) {
            in_string = false;
         }
         Source.consume(src, 1);
         continue;
      }

      if (c == ANVL_TOK_QUOTE) {
         in_string = true;
         Source.consume(src, 1);
         continue;
      }
      if (c == '/' && Source.peek_offset(src, 1) == '/') {
         while (!Source.is_eof(src) && Source.peek(src) != ANVL_TOK_NEWLINE) {
            Source.consume(src, 1);
         }
         continue;
      }
      if (c == '/' && Source.peek_offset(src, 1) == '*') {
         Source.consume(src, 2); // '/*'
         while (!Source.is_eof(src) &&
                !(Source.peek(src) == '*' && Source.peek_offset(src, 1) == '/')) {
            Source.consume(src, 1);
         }
         if (!Source.is_eof(src)) {
            Source.consume(src, 2); // '*/'
         }
         continue;
      }
      if (c == ANVL_TOK_LBRACE) {
         depth++;
         Source.consume(src, 1);
         continue;
      }
      if (c == ANVL_TOK_RBRACE) {
         depth--;
         if (depth == 0) {
            break; // stop *before* consuming the matching '}' -- that's body.end
         }
         Source.consume(src, 1);
         continue;
      }
      Source.consume(src, 1);
   }

   if (depth != 0) {
      parser_set_error(src, ANVL_ERR_ASL_UNTERMINATED_BODY);
      return false;
   }

   body.end = Source.at(src);
   Source.consume(src, 1); // '}'
   *out_body = body;
   return true;
}

bool asl_parse_function_decl(anvl_source src, anvl_statement *out_stmt) {
   // Function declarations are AnvlScript-exclusive (docs/dialect-ownership-matrix.md:
   // "function declarations: AML No / ASL Yes (owner) / AMP No"). Reuses
   // ANVL_ERR_PARSER_UNEXPECTED_TOKEN, matching every other dialect gate already in
   // parser.c (base/attributes/OBJECT_BLOCK's own AMP-rejection).
   if (Source.dialect(src) != ANVL_DIALECT_ASL) {
      parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
      return false;
   }

   (*out_stmt)->kind = ANVL_STMT_FUNCTION;

   list params = NULL;
   if (!asl_parse_param_list(src, &params)) {
      return false; // asl_parse_param_list already set its own error and disposed itself
   }
   Source.skip_whitespace_and_comments(src);

   if (!Source.match_token(src, ANVL_TOK_ARROW, ANVL_TOK_ARROW_LEN)) {
      parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
      asl_dispose_params(params);
      return false;
   }
   Source.consume(src, ANVL_TOK_ARROW_LEN);
   Source.skip_whitespace_and_comments(src);

   if (Source.peek(src) != ANVL_TOK_LBRACE) {
      parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
      asl_dispose_params(params);
      return false;
   }

   anvl_slice body;
   if (!asl_scan_body(src, &body)) {
      asl_dispose_params(params);
      return false; // asl_scan_body already set its own error
   }

   anvl_err_code err_code;
   (*out_stmt)->value = Source.new_node(src, ANVL_NODE_VALUE, &err_code);
   if (!(*out_stmt)->value) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      asl_dispose_params(params);
      return false;
   }
   (*out_stmt)->value->type = ANVL_VALUE_FUNCTION;
   (*out_stmt)->value->text = body; // step 1: mirrors the body span; widen to the full
                                    // '(' ... '}' signature once something actually needs it.
   (*out_stmt)->value->function.params = params;
   (*out_stmt)->value->function.body_start = body.start;
   (*out_stmt)->value->function.body_end = body.end;
   (*out_stmt)->value->function.ast = NULL; // lazily compiled by the ScriptEngine, not here

   Source.skip_whitespace_and_comments(src);
   if (Source.peek(src) != ANVL_TOK_STMT_TERMINATOR) {
      parser_set_error(src, ANVL_ERR_PARSER_UNTERMINATED_STATEMENT);
      return false;
   }
   Source.consume(src, 1); // ';'
   Source.skip_whitespace_and_comments(src);

   return true;
}

#endif
