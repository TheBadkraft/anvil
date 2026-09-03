/*
 * Copyright (c) 2025-26 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * parser.c - Implementation of Anvil parser
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * Created: 2025-12-13
 * Refactor: 2026-07-17
 * File: src/core/parser.c
 * ----------------------------------------------------------------------- *
 * Refactoring Note: This file is the refactored parser implementation.
 * It supersedes the original implementation in src/core/_parser.c, which
 * is now considered obsolete. The refactored parser improves maintainability,
 * readability, and performance while preserving the original functionality.
 * The original file is retained for reference and historical purposes.
 *
 * This refactored parser is designed to be more modular, with clearer
 * separation of concerns, improved error handling, and enhanced support for
 * future language features. It is recommended to use this refactored version.
 *
 * The original license and copyright information from the original file is
 * retained here for historical context, but the refactored code is subject
 * to the same proprietary license as the rest of the Anvil project.
 */

#include "internal/module.h"
#include "internal/parser.h"
#include "internal/source.h"
#include "types.h"
#include "errors.h"
#include "std.h"
// ----------------
#include <sigma/list.h>
#include <sigma/time.h>

// Parser state structure
typedef struct {
   // list of parser_state objects, one for each document being parsed
   list state_map;         // List of parser_ctx for each document being parsed
   anvl_err_code err_code; // Error code for the parser state
   bool is_initialized;    // Flag to indicate if the parser has been initialized
} parser_state;
typedef struct {
   anvl_source src; // The source being parsed
   usize start;     // Timestamp start
   usize end;       // Timestamp end
   list statements; // Top-level statements accumulated so far, in parse order; handed off to
                    // Source.finish_body at the end of parse_source (success or failure) and
                    // set back to NULL once ownership transfers. Still owned here (and disposed
                    // by parser_dispose_ctx) only if parse_source never reaches that hand-off.
} parser_ctx;
typedef parser_ctx *context;

// Global parser state
static parser_state parser = {0};

// constant static data for reserved literals
static const char *reserved_literals[] = {ANVL_KEYWORD_TRUE, ANVL_KEYWORD_FALSE, ANVL_KEYWORD_NULL};
static const usize reserved_lengths[] = {ANVL_KEYWORD_TRUE_LEN, ANVL_KEYWORD_FALSE_LEN,
                                         ANVL_KEYWORD_NULL_LEN};

// Optional instrumentation hook — parser depends only on the callback
// signature (see internal/parser.h), never on a specific consumer.
static anvl_parse_hook_fn hook_fn = NULL;
static void *hook_userdata = NULL;

static const usize ptr_size = sizeof(void *);

/* ----------------------------------------------------------------- *
 * Parser Function Declarations
 * ----------------------------------------------------------------- */
static bool parser_init(void);
static bool parser_create_ctx(anvl_source, context *);
static void parser_dispose_ctx(context *);
static bool parse_source(context);
static bool parse_statement(anvl_source, anvl_statement *);
static bool parse_statement_list(anvl_source, list *);
static bool parse_attribute_list(anvl_source, list *);
static void dispose_attribute_list(list);
static bool parse_identifier(anvl_source, anvl_slice *);
static bool parse_value(anvl_source, anvl_value *);
static bool parse_scalar_value(anvl_source, anvl_value *);
static bool parse_reserved_literal(anvl_source, anvl_value *);
static bool parse_numeric_literal(anvl_source, anvl_value *);
static bool parse_string_literal(anvl_source, anvl_value *);
static bool parse_blob_literal(anvl_source, anvl_value *);
static bool parse_bare_literal(anvl_source, anvl_value *);
static bool parse_array(anvl_source, anvl_value *);
static bool parse_tuple(anvl_source, anvl_value *);
static bool parse_object_value(anvl_source, anvl_value *);
static bool is_value_boundary(anvl_source);
static void skip_same_line_whitespace(anvl_source);
static void parser_set_error(anvl_source, anvl_err_code);

/* ----------------------------------------------------------------- *
 * Parser Function Implementations
 * ----------------------------------------------------------------- */
// Parse the given source and return the result
anvl_result anvl_parse(anvl_source source) {
   // immediately set up the parser context and state for the given document
   if (!parser_init()) {
      goto error; // Failed to initialize parser
   }

   context ctx = NULL;
   if (!parser_create_ctx(source, &ctx)) {
      goto error; // Failed to create parser context
   }

   // Note: The parser context creation already ensures that the source is valid.
   // if (!source) {
   //    parser.err_code = ANVL_ERR_INVALID_ARGUMENT;
   //    goto error;
   // }

   // tracking the state for the given source in the parser's state map
   usize idx = List.size(parser.state_map);
   (void)List.append(parser.state_map, ctx);
   anvl_result res = parse_source(ctx) ? ANVL_RES_OK : ANVL_RES_ERR;

   // Fire the instrumentation hook regardless of success/failure — how fast
   // we fail on bad input is as meaningful as how fast we succeed.
   if (hook_fn) {
      anvl_parse_metrics_t metrics = {
         .bytes = Source.length(ctx->src),
         .elapsed_ns = Time.elapsed(ctx->start, ctx->end),
      };
      hook_fn(&metrics, hook_userdata);
   }

   parser_dispose_ctx(&ctx);
   List.remove(parser.state_map, idx); // Clean up the state map after parsing

   return res;

error:
   return ANVL_RES_ERR;
}
// Clean up the parser state and free any allocated resources
void anvl_cleanup(void) {
   if (!parser.is_initialized) {
      return; // Nothing to clean up
   }

   // Dispose of all parser contexts in the state map
   usize count = List.size(parser.state_map);
   for (usize i = 0; i < count; i++) {
      parser_ctx *ctx = NULL;
      List.get(parser.state_map, i, (object *)&ctx);
      if (ctx) {
         parser_dispose_ctx(&ctx);
      }
   }

   List.dispose(parser.state_map);
   parser.state_map = NULL;
   parser.is_initialized = false;
}
// Get the last error code from the parser state
anvl_err_code anvl_get_error(void) { return parser.err_code; }
// Register (or clear, with fn == NULL) the instrumentation hook
void anvl_parser_set_hook(anvl_parse_hook_fn fn, void *userdata) {
   hook_fn = fn;
   hook_userdata = userdata;
}
// Clear any registered instrumentation hook
void anvl_parser_clear_hook(void) {
   hook_fn = NULL;
   hook_userdata = NULL;
}

/* ----------------------------------------------------------------- *
 * Internal Parser Function Implementations
 * ----------------------------------------------------------------- */
// Initialize the parser state if not already initialized
static bool parser_init(void) {
   if (parser.is_initialized) {
      return true; // Already initialized
   }

   // Initialize any global parser state here if needed
   parser.state_map = List.new(4, ptr_size); // Initial capacity of 4, element size of pointer
   parser.is_initialized = parser.state_map != NULL;
   parser.err_code = parser.is_initialized ? ANVL_ERR_NONE : ANVL_ERR_PARSER_INITIALIZATION_FAILED;

   return parser.is_initialized;
}
// Create a new parser context for the given source and return true on success, false on failure
static bool parser_create_ctx(anvl_source source, context *out_ctx) {
   if (!source || !out_ctx) {
      parser.err_code = ANVL_ERR_INVALID_ARGUMENT;
      return false;
   }

   parser_ctx *ctx = Allocator.alloc(sizeof(parser_ctx));
   if (!ctx) {
      parser.err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      return false;
   }

   ctx->src = source;
   ctx->start = Source.position(source);
   ctx->end = 0; // Will be set after parsing
   ctx->statements = List.new(4, sizeof(anvl_statement));
   if (!ctx->statements) {
      parser.err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      Allocator.dispose(ctx);
      return false;
   }
   *out_ctx = ctx;

   return true;
}
// dispose of the parser context and free associated resources
static void parser_dispose_ctx(context *ctx) {
   if (!ctx || !*ctx) {
      return;
   }
   // Only still owned here if parse_source never handed it off to Source.finish_body
   // (finish_body takes ownership and this gets set back to NULL when that happens).
   if ((*ctx)->statements) {
      List.dispose((*ctx)->statements);
   }
   Allocator.dispose(*ctx);
   *ctx = NULL;
}
// Parse the source using the given parser context and return true on success, false on failure
static bool parse_source(context ctx) {
   if (!ctx || !ctx->src) {
      parser.err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   usize length = Source.length(ctx->src);
   ctx->start = Time.now(); // Record the start time of parsing

   while (!Source.is_eof(ctx->src)) {
      // we still have to have this check here, too
      if (Source.match_length(ctx->src, ANVL_TOK_SHEBANG_PREFIX, ANVL_TOK_SHEBANG_PREFIX_LEN) ==
          ANVL_TOK_SHEBANG_PREFIX_LEN) {
         parser_set_error(ctx->src, ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS);
         ctx->end = Time.now(); // Record the end time even on failure, for accurate instrumentation
         // Freeze whatever was already accumulated before this failure — partial
         // results stay inspectable on doc->body, not silently dropped.
         Source.finish_body(ctx->src, ctx->statements, &parser.err_code);
         ctx->statements = NULL; // ownership transferred to finish_body regardless of its result
         return false;
      }
      // TODO: we need to check for illegal keywords after statements
      anvl_statement stmt = NULL;
      if (!parse_statement(ctx->src, &stmt)) {
         ctx->end = Time.now(); // Record the end time even on failure, for accurate instrumentation
         Source.finish_body(ctx->src, ctx->statements, &parser.err_code);
         ctx->statements = NULL;
         return false;
      }
      // Statement finished successfully — accumulate it for this document's body.
      List.append(ctx->statements, stmt);
   }

   ctx->end = Time.now(); // Record the end time of parsing
   Source.finish_body(ctx->src, ctx->statements, &parser.err_code);
   ctx->statements = NULL;

   // Return true if all source was consumed, false otherwise
   return Source.position(ctx->src) == length;

error:
   return false;
}
// Skip same-line whitespace only (space/tab) — never a newline, never a comment. Used at the
// two points in the grammar where a statement's value must begin and end on the same line:
// right after `:=`, before the value's leading character is examined, and right after the
// value, before its own terminating `;` (see parse_value). Crossing a line — or treating
// `//`/`/*` as a comment — in either gap would reopen the same leading-`/`-vs-comment
// ambiguity already worked through for UNC-style bare literals (notes/document-body-parse.md
// "Bare literal grammar"); refusing to cross a line at all removes the ambiguity outright,
// since a `//` immediately after `:=` can now only ever be the value itself, never a comment
// deferring the value to a later line.
static void skip_same_line_whitespace(anvl_source src) {
   while (Source.peek(src) == ' ' || Source.peek(src) == '\t') {
      Source.consume(src, 1);
   }
}
// Dispose an attribute list built by parse_attribute_list. Each anvl_attribute is its own
// heap allocation (not arena-owned — attributes aren't a Source.new_node kind, unlike
// statements/values), so disposing the list container alone would leak every attribute in
// it; this frees each one first. Safe to call on a partially-built list from an error path.
static void dispose_attribute_list(list attrs) {
   if (!attrs) {
      return;
   }
   usize count = List.size(attrs);
   for (usize i = 0; i < count; i++) {
      anvl_attribute attr = NULL;
      List.get(attrs, i, (object *)&attr);
      Allocator.dispose(attr);
   }
   List.dispose(attrs);
}
// Parse a statement-level attribute list '@[key, key=value, ...]', with the opening '@['
// already confirmed present (not yet consumed) by the caller. Mirrors header_scan_attributes's
// shape (src/core/document.c) for module-level attributes, adapted to build and return a
// fresh list per statement rather than appending into a pre-existing document-level one. No
// trailing comma before ']' — matches header_scan_attributes's own behavior, not a separate
// design choice.
static bool parse_attribute_list(anvl_source src, list *out_attributes) {
   Source.consume(src, 2); // "@["
   Source.skip_whitespace_and_comments(src);

   list attrs = List.new(4, ptr_size);
   if (!attrs) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false;
   }

   while (true) {
      Source.skip_whitespace_and_comments(src);

      if (!Source.is_identifier_start(Source.peek(src))) {
         parser_set_error(src, ANVL_ERR_PARSER_INVALID_IDENTIFIER);
         dispose_attribute_list(attrs);
         return false;
      }

      const char *key_start = Source.at(src);
      Source.consume(src, 1);
      while (Source.is_identifier_part(Source.peek(src))) {
         Source.consume(src, 1);
      }
      const char *key_end = Source.at(src);

      anvl_attribute attr = Allocator.alloc(sizeof(anvl_attribute_t));
      if (!attr) {
         parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
         dispose_attribute_list(attrs);
         return false;
      }
      Source.init_slice(src, &attr->key);
      attr->key.start = key_start;
      attr->key.end = key_end;

      Source.skip_whitespace_and_comments(src);

      // optional '=value' — flag attribute (empty .value) when absent
      if (Source.peek(src) == '=') {
         Source.consume(src, 1);
         Source.skip_whitespace_and_comments(src);
         const char *value_start = Source.at(src);
         bool in_string = false;
         while (!Source.is_eof(src)) {
            char c = Source.peek(src);
            if (!in_string && (c == ',' || c == ANVL_TOK_RBRACKET)) {
               break;
            }
            if (c == ANVL_TOK_QUOTE) {
               in_string = !in_string;
            }
            Source.consume(src, 1);
         }
         Source.init_slice(src, &attr->value);
         attr->value.start = value_start;
         attr->value.end = Source.at(src);
      }

      List.append(attrs, attr);

      Source.skip_whitespace_and_comments(src);

      if (Source.peek(src) == ',') {
         Source.consume(src, 1);
         continue;
      }
      if (Source.peek(src) == ANVL_TOK_RBRACKET) {
         Source.consume(src, 1);
         break;
      }

      parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
      dispose_attribute_list(attrs);
      return false;
   }

   *out_attributes = attrs;
   return true;
}
// Parse a `{ statements }` body, recursively — shared by ASSIGN's object-typed value
// (parse_object_value, `:= { ... }`) and the direct OBJECT_BLOCK statement form
// (`ident { ... }`, in parse_statement). Consumes the opening and closing braces. An empty
// body ('{}') is a parse error, same "no collection/container may be empty" invariant as
// array/tuple (see the value-tree design comment in include/internal/module.h). Each nested
// anvl_statement is arena-owned via parse_statement's own Source.new_node call, same as any
// top-level one — only this function's own `list` container needs disposing on an error path.
static bool parse_statement_list(anvl_source src, list *out_statements) {
   Source.consume(src, 1); // '{'
   Source.skip_whitespace_and_comments(src);

   if (Source.peek(src) == ANVL_TOK_RBRACE) {
      parser_set_error(src, ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED);
      return false;
   }

   list statements = List.new(4, ptr_size);
   if (!statements) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false;
   }

   while (!Source.is_eof(src) && Source.peek(src) != ANVL_TOK_RBRACE) {
      anvl_statement stmt = NULL;
      if (!parse_statement(src, &stmt)) {
         List.dispose(statements);
         return false;
      }
      List.append(statements, stmt);
   }

   if (Source.peek(src) != ANVL_TOK_RBRACE) {
      parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE);
      List.dispose(statements);
      return false;
   }
   Source.consume(src, 1); // '}'

   *out_statements = statements;
   return true;
}
// Parse a single statement from the source
static bool parse_statement(anvl_source src, anvl_statement *out_stmt) {
   // Parse statement
   *out_stmt = Source.new_node(src, ANVL_NODE_STATEMENT, &parser.err_code);
   if (!*out_stmt) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false; // Failed to allocate statement node
   }

   // - identifier
   if (!parse_identifier(src, &(*out_stmt)->name)) {
      parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
      return false;
   }
   // - consume whitespace
   Source.skip_whitespace_and_comments(src);

   // - optional inheritance base ': base' — ':' not immediately followed by '=' (that's the
   // assignment operator, checked below). AML-only; AMP forbids inheritance entirely (AMP11).
   // `base` reuses parse_identifier, so a keyword there is rejected the same as anywhere else.
   if (Source.peek(src) == ':' && Source.peek_offset(src, 1) != '=') {
      if (Source.dialect(src) == ANVL_DIALECT_AMP) {
         parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
         return false;
      }
      Source.consume(src, 1); // ':'
      Source.skip_whitespace_and_comments(src);
      if (!parse_identifier(src, &(*out_stmt)->base)) {
         parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
         return false;
      }
      Source.skip_whitespace_and_comments(src);
   }

   // - optional statement-level attributes '@[...]' — AML-only; AMP forbids them entirely
   // (see AMP15). Matches header_scan_attributes's own AMP-gate (src/core/document.c), and
   // is checked before consuming anything so an AMP document errors cleanly, not partially.
   if (Source.match_length(src, "@[", 2) == 2) {
      if (Source.dialect(src) == ANVL_DIALECT_AMP) {
         parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
         return false;
      }
      if (!parse_attribute_list(src, &(*out_stmt)->attributes)) {
         return false; // parse_attribute_list already set its own error
      }
      Source.skip_whitespace_and_comments(src);
   }

   // - dispatch: ':=' (ASSIGN) or a direct '{' (OBJECT_BLOCK) — both accept the same optional
   // base/attributes already parsed above. AMP forbids OBJECT_BLOCK entirely (AMP12), checked
   // before attempting to parse it, matching the reject-before-parsing shape used throughout
   // this file (arrays/tuples' AMP-element rejection, attributes' and base's own AMP gates).
   if (Source.match_token(src, ANVL_TOK_ASSIGN, ANVL_TOK_ASSIGN_LEN)) {
      // - consume assignment operator
      Source.consume(src, ANVL_TOK_ASSIGN_LEN);
      // - consume same-line whitespace only — see skip_same_line_whitespace's doc comment
      skip_same_line_whitespace(src);

      // - base implies object-shaped, deterministically: if a base was captured, ':=' must be
      // followed immediately by '{' — checked here, before attempting to parse any value at
      // all, not as a post-hoc type check performed after generically parsing one (see notes/
      // document-body-parse.md "Resolved questions" #3).
      if (!Source.slice_is_empty((*out_stmt)->base) && Source.peek(src) != ANVL_TOK_LBRACE) {
         parser_set_error(src, ANVL_ERR_PARSER_INHERITANCE_REQUIRES_OBJECT);
         return false;
      }

      // - value
      if (!parse_value(src, &(*out_stmt)->value)) {
         parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_VALUE);
         return false;
      }
      // - consume whitespace
      Source.skip_whitespace_and_comments(src);
   } else if (Source.peek(src) == ANVL_TOK_LBRACE) {
      if (Source.dialect(src) == ANVL_DIALECT_AMP) {
         parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
         return false;
      }
      (*out_stmt)->kind = ANVL_STMT_OBJECT_BLOCK;
      if (!parse_statement_list(src, &(*out_stmt)->body)) {
         return false; // parse_statement_list already set its own error
      }
      // - end of statement terminator ';' is still required after the closing '}'
      skip_same_line_whitespace(src);
      if (Source.peek(src) != ANVL_TOK_STMT_TERMINATOR) {
         parser_set_error(src, ANVL_ERR_PARSER_UNTERMINATED_STATEMENT);
         return false;
      }
      Source.consume(src, 1);
      Source.skip_whitespace_and_comments(src);
   } else {
      parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_ASSIGN);
      return false;
   }

   return true;
}
// Parse an identifier
static bool parse_identifier(anvl_source src, anvl_slice *out_identifier) {
   // initialize the slice
   Source.init_slice(src, out_identifier);
   out_identifier->start = NULL;
   out_identifier->end = NULL;

   if (!Source.is_identifier_start(Source.peek(src))) {
      parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
      return false;
   }
   // Consume the identifier
   const char *id_start = Source.at(src);
   Source.consume(src, 1);
   while (Source.is_identifier_part(Source.peek(src))) {
      Source.consume(src, 1);
   }
   // build the identifier slice
   out_identifier->start = id_start;
   out_identifier->end = Source.at(src);

   if (Source.is_keyword(*out_identifier)) {
      parser_set_error(src, ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD);
      return false;
   }

   return true;
}
// Parse a value from the source
static bool parse_value(anvl_source src, anvl_value *out_value) {
   *out_value = Source.new_node(src, ANVL_NODE_VALUE, &parser.err_code);
   if (!*out_value) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false; // Failed to allocate value node
   }

   // Value dispatch: scalar, then array/tuple/object by leading symbol. Only the final `else`
   // sets a generic error — every other branch either succeeds or has already set its own,
   // more specific error via parser_set_error internally, so there's exactly one call site for
   // ANVL_ERR_PARSER_EXPECTED_VALUE and no risk of a generic error silently overwriting a
   // specific one that already fired.
   if (parse_scalar_value(src, out_value)) {
      // fall through to terminator check
   } else if (Source.peek(src) == ANVL_TOK_LBRACKET) {
      if (!parse_array(src, out_value)) {
         return false; // array already set its own error
      }
   } else if (Source.peek(src) == ANVL_TOK_LPAREN) {
      if (!parse_tuple(src, out_value)) {
         return false; // tuple already set its own error
      }
   } else if (Source.peek(src) == ANVL_TOK_LBRACE) {
      // AMP forbids an object-typed ASSIGN value entirely (AMP16) — rejected on sight, same
      // shape as every other AMP-forbidden construct in this file.
      if (Source.dialect(src) == ANVL_DIALECT_AMP) {
         parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
         return false;
      }
      if (!parse_object_value(src, out_value)) {
         return false; // object already set its own error
      }
   } else {
      parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_VALUE);
      return false; // Not a scalar, not a collection start — genuinely not a value
   }

   // 3. end of statement terminator `;` is required for all dialects. Only same-line
   // whitespace (space/tab) is allowed between the value and the terminator — not a newline,
   // and not a comment. Deliberate, not an oversight: a comment belongs after `;`, never
   // before it — extending comment-skipping into this gap would reopen, in a new grammar
   // position, the same leading-`/`-vs-comment ambiguity already worked through for UNC-style
   // bare literals (see notes/document-body-parse.md "Bare literal grammar").
   skip_same_line_whitespace(src);
   if (Source.peek(src) != ANVL_TOK_STMT_TERMINATOR) {
      parser_set_error(src, ANVL_ERR_PARSER_UNTERMINATED_STATEMENT);
      return false;
   }
   Source.consume(src, 1);
   Source.skip_whitespace_and_comments(src);

   return true;
}
// True if `c` is a legitimate value boundary — the character immediately following a
// completed literal, not itself part of it. See notes/document-body-parse.md "generalize the
// decline-and-rewind boundary check" TODO: parse_numeric_literal uses this to tell a genuinely
// finished number apart from a numeric-shaped prefix of a longer bare-literal token (`09-02-2026`).
static bool is_value_boundary(anvl_source src) {
   if (Source.is_eof(src)) {
      return true;
   }
   char c = Source.peek(src);
   return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ANVL_TOK_STMT_TERMINATOR ||
          c == ',' || c == ANVL_TOK_RBRACKET || c == ANVL_TOK_RPAREN || c == ANVL_TOK_RBRACE;
}
// Parse a scalar value
static bool parse_scalar_value(anvl_source src, anvl_value *out_value) {
   // 1. check for reserved literals - true, false, null
   if (parse_reserved_literal(src, out_value)) {
      return true; // Successfully parsed reserved literal
   }
   // 2. numeric literal - integer, float, hex, exponential, etc. (`parse_numeric_literal` handles
   // all numeric formats)
   if (parse_numeric_literal(src, out_value)) {
      return true; // Successfully parsed numeric literal
   }
   // 3. string literal - quoted string with optional escape sequences
   if (parse_string_literal(src, out_value)) {
      return true; // Successfully parsed string literal
   }
   // 4. blob literal - tagged content in text span
   if (parse_blob_literal(src, out_value)) {
      return true; // Successfully parsed blob literal
   }
   // 5. bare literal - unquoted string (identifier) that is not a reserved keyword
   if (parse_bare_literal(src, out_value)) {
      return true; // Successfully parsed bare literal
   }

   return false; // Failed to parse scalar value
}
// Parse a reserved literal (true, false, null)
static bool parse_reserved_literal(anvl_source src, anvl_value *out_value) {
   const char *start = Source.at(src);
   for (usize i = 0; i < sizeof(reserved_literals) / sizeof(reserved_literals[0]); i++) {
      if (Source.match_token(src, reserved_literals[i], reserved_lengths[i]) ==
          reserved_lengths[i]) {
         // Found a reserved literal
         *out_value = Source.new_node(src, ANVL_NODE_VALUE, &parser.err_code);
         if (!*out_value) {
            parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
            return false; // Failed to allocate value node
         }
         Source.consume(src, reserved_lengths[i]);

         // set value type & initialize the value slice
         (*out_value)->type = (i == 0) || (i == 1) ? ANVL_VALUE_BOOL : ANVL_VALUE_NULL;
         Source.init_slice(src, &(*out_value)->text);
         (*out_value)->text.start = start;
         (*out_value)->text.end = Source.at(src);

         return true; // Successfully parsed reserved literal
      }
   }

   return false; // Not a reserved literal
}
// Parse a numeric literal
static bool parse_numeric_literal(anvl_source src, anvl_value *out_value) {
   // We need to check for a leading digit to determine if it's a numeric literal
   // or a '-' sign for a negative number. If it's a digit, we can parse it as a numeric literal.
   // If it's a '-' sign, we need to check if the next character is a digit to determine if it's a
   // negative number.
   bool negative = Source.peek(src) == '-';
   // dec point counter valid values are 0 or 1
   char first_digit = negative ? Source.peek_offset(src, 1) : Source.peek(src);
   if (!Source.is_digit(first_digit)) {
      return false; // Not a numeric literal
   }

   // cache source position
   usize save_pos = Source.position(src);
   usize save_line = Source.line(src);
   usize save_col = Source.column(src);

   const char *start = Source.at(src);
   if (negative) {
      // Consume the '-' sign
      Source.consume(src, 1);
   }
   // consume the digit we already confirmed above
   Source.consume(src, 1);
   // consume digits and at most one decimal point
   while (Source.is_digit(Source.peek(src))) {
      Source.consume(src, 1);
   }
   if (Source.peek(src) == ANVL_TOK_DOT) {
      // Consume the decimal point
      Source.consume(src, 1);
      // now consume digits after the decimal point
      while (Source.is_digit(Source.peek(src))) {
         Source.consume(src, 1);
      }
      // if a 2nd decimal point is found, roll back to cached position and return false
      if (Source.peek(src) == ANVL_TOK_DOT) {
         Source.set_position(src, save_pos, save_line, save_col);
         return false; // Invalid numeric literal (multiple decimal points)
      }
   }

   // optional exponent part (e.g., `e+10`, `E-5`) — sign is mandatory
   if (Source.peek(src) == 'e' || Source.peek(src) == 'E') {
      char sign = Source.peek_offset(src, 1);
      if (sign == '+' || sign == '-') {
         // committed: 'e'/'E' plus a sign is unambiguous, so a missing digit
         // here is a real malformed exponent, not "maybe something else"
         if (!Source.is_digit(Source.peek_offset(src, 2))) {
            parser_set_error(src, ANVL_ERR_PARSER_INVALID_EXPONENT);
            return false;
         }
         // Consume the 'e' or 'E' and the sign
         Source.consume(src, 2);
         // Consume the exponent digits
         while (Source.is_digit(Source.peek(src))) {
            Source.consume(src, 1);
         }
      }
   }

   // Generalized decline-and-rewind (see is_value_boundary): what looked like a complete
   // number so far might actually just be the numeric-shaped prefix of a longer bare-literal
   // token (`09-02-2026`) — if what follows isn't a legitimate boundary, this was never a
   // number to begin with, so decline the whole span rather than truncating it.
   if (!is_value_boundary(src)) {
      Source.set_position(src, save_pos, save_line, save_col);
      return false;
   }

   // set value type & initialize the value slice
   (*out_value)->type = ANVL_VALUE_NUMERIC;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = Source.at(src);

   return true; // Successfully parsed numeric literal
}
// Parse a string literal
static bool parse_string_literal(anvl_source src, anvl_value *out_value) {
   // Check for opening quote
   if (Source.peek(src) != ANVL_TOK_QUOTE) {
      return false; // Not a string literal
   }

   // Consume the opening quote and record the start position of the string content
   Source.consume(src, 1);
   const char *start = Source.at(src);

   // Parse the string content until the closing quote or EOF
   while (!Source.is_eof(src) && Source.peek(src) != ANVL_TOK_QUOTE) {
      // Handle escape sequences (e.g., \n, \t, \", etc.)
      if (Source.peek(src) == ANVL_TOK_ESCAPE) {
         Source.consume(src, 1); // Consume the backslash
         if (!Source.is_eof(src)) {
            Source.consume(src, 1); // Consume the escaped character
         }
      } else {
         Source.consume(src, 1); // Consume regular character
      }
   }

   // Check for closing quote
   if (Source.peek(src) != ANVL_TOK_QUOTE) {
      parser_set_error(src, ANVL_ERR_PARSER_UNTERMINATED_STRING);
      return false; // Unterminated string literal
   }

   // Consume the closing quote
   Source.consume(src, 1);

   // set value type & initialize the value slice
   (*out_value)->type = ANVL_VALUE_STRING;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = Source.at(src);

   return true; // Successfully parsed string literal
}
// Parse a blob literal (tagged content in text span)
static bool parse_blob_literal(anvl_source src, anvl_value *out_value) {
   const char *tag_start = NULL;
   const char *tag_end = NULL;
   bool has_tag = false;
   // Check for opening tag (e.g., `@tag`) - NOT REQUIRED
   if (Source.peek(src) == ANVL_TOK_ATTRIB) {
      has_tag = true;
      // capture the tag for the blob literal
      Source.consume(src, 1); // Consume the '@' symbol
      if (Source.is_identifier_start(Source.peek(src))) {
         tag_start = Source.at(src);
         while (Source.is_identifier_part(Source.peek(src)) &&
                (Source.at(src) - tag_start) < 31) { // Max 31 chars
            Source.consume(src, 1);
         }
         tag_end = Source.at(src);
      } else {
         // invalid tag, set an error
         parser_set_error(src, ANVL_ERR_PARSER_INVALID_BLOB_TAG);
      }
   }

   // Not a blob at all (no tag, no backtick) — decline cleanly, nothing consumed yet.
   // A tag *was* found but isn't followed by a backtick — that's a real error, not a
   // decline: committing to `@tag` already disambiguates this as trying to be a blob.
   if (Source.peek(src) != ANVL_TOK_BACKTICK) {
      if (has_tag) {
         parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_BACKTICK);
      }
      return false;
   }

   // Consume the opening backtick and record the start position of the blob content
   Source.consume(src, 1);
   const char *start = Source.at(src);

   // Parse the blob content until the closing brace or EOF
   while (!Source.is_eof(src) && Source.peek(src) != ANVL_TOK_BACKTICK) {
      Source.consume(src, 1); // Consume blob content character
   }

   // Check for closing brace
   if (Source.peek(src) != ANVL_TOK_BACKTICK) {
      parser_set_error(src, ANVL_ERR_PARSER_UNTERMINATED_BLOB);
      return false; // Unterminated blob literal
   }

   // Consume the closing backtick
   Source.consume(src, 1);

   // set value type & initialize the value slice
   (*out_value)->type = ANVL_VALUE_BLOB;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = Source.at(src);
   Source.init_slice(src, &(*out_value)->blob.tag);
   (*out_value)->blob.tag.start = tag_start;
   (*out_value)->blob.tag.end = tag_end;

   return true; // Successfully parsed blob literal
}
// Parse a bare literal (unquoted string that is not a reserved keyword). See
// notes/document-body-parse.md "Bare literal grammar". Leading character must be
// identifier-start-shaped, '.', '/', or a digit — a digit only ever reaches here because
// parse_numeric_literal (tried earlier in parse_scalar_value) already declined it, never as
// a fresh leading-digit case of its own. '$' is deliberately not a valid leading character:
// it's reserved for a future VarRef sigil, even though VarRef dispatch doesn't exist yet.
static bool parse_bare_literal(anvl_source src, anvl_value *out_value) {
   char first = Source.peek(src);
   bool valid_start = Source.is_identifier_start(first) || first == ANVL_TOK_DOT || first == '/' ||
                      Source.is_digit(first);
   if (!valid_start) {
      return false; // Not a bare literal
   }

   const char *start = Source.at(src);
   Source.consume(src, 1);
   while (Source.is_bare_literal_part(Source.peek(src))) {
      Source.consume(src, 1);
   }

   // '@'/'`' immediately after bare-literal content is invalid, not a silent terminator —
   // both are blob-dispatch leading symbols, and were explicitly decided *not* to be legal
   // mid-token characters (see notes/document-body-parse.md "Bare literal grammar"). Checking
   // this here, rather than letting the loop above simply stop and leave '@'/'`' unconsumed,
   // gives a precise error instead of silently truncating to a shorter "successful" literal
   // and letting the leftover '@'/'`' trip a confusing, unrelated failure downstream (e.g. a
   // generic "expected ';'" that doesn't name the actual cause).
   char next = Source.peek(src);
   if (next == ANVL_TOK_ATTRIB || next == ANVL_TOK_BACKTICK) {
      parser_set_error(src, ANVL_ERR_PARSER_INVALID_BARE_LITERAL);
      return false;
   }

   anvl_slice text = {0};
   Source.init_slice(src, &text);
   text.start = start;
   text.end = Source.at(src);

   // RESERVED keywords (import/using/vars) are illegal as a bare value, same as they are as
   // an identifier — true/false/null can never reach here, since parse_reserved_literal
   // already intercepts and classifies them earlier in parse_scalar_value's dispatch chain.
   if (Source.is_keyword(text)) {
      parser_set_error(src, ANVL_ERR_PARSER_VALUE_IS_KEYWORD);
      return false;
   }

   // set value type & initialize the value slice
   (*out_value)->type = ANVL_VALUE_IDENTIFIER;
   (*out_value)->text = text;

   return true; // Successfully parsed bare literal
}
// Parse an array value. Elements are comma-separated (trailing comma allowed), and the list
// itself owns its `list items` directly — no per-element pos/len bookkeeping needed the way
// the legacy parser (src/core/_parser.c:733-833) tracked it, since every anvl_value already
// carries its own `.text` slice from whichever literal parser produced it. AMP restricts
// elements to scalars only, rejected at the leading symbol on sight — not by generically
// parsing a nested structure and checking its type afterward, matching the JS reference
// parser's parseAmpElement (see notes/document-body-parse.md "Dialect scope"). AML's future
// any-value (including nested collections) element grammar isn't implemented yet, so this
// only ever calls parse_scalar_value, never the full parse_value, for each element.
static bool parse_array(anvl_source src, anvl_value *out_value) {
   const char *start = Source.at(src);
   // consume the array start token
   Source.consume(src, 1);
   Source.skip_whitespace_and_comments(src);

   // Check for empty array (INVALID)
   if (Source.peek(src) == ANVL_TOK_RBRACKET) {
      parser_set_error(src, ANVL_ERR_PARSER_ARRAY_CANNOT_BE_EMPTY);
      return false;
   }

   list items = List.new(4, sizeof(anvl_value));
   if (!items) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false;
   }

   while (!Source.is_eof(src) && Source.peek(src) != ANVL_TOK_RBRACKET) {
      // AMP forbids nested collections as array elements — reject on sight of the leading
      // symbol, before wasting an attempt on parse_scalar_value (which could never produce
      // one anyway, but the point is not to imply it was even considered).
      char lead = Source.peek(src);
      if (Source.dialect(src) == ANVL_DIALECT_AMP &&
          (lead == ANVL_TOK_LBRACE || lead == ANVL_TOK_LBRACKET || lead == ANVL_TOK_LPAREN)) {
         parser_set_error(src, ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR);
         List.dispose(items);
         return false;
      }

      anvl_value elem = Source.new_node(src, ANVL_NODE_VALUE, &parser.err_code);
      if (!elem) {
         parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
         List.dispose(items);
         return false;
      }
      // this is an AMP restriction: only scalar values are allowed in arrays
      if (!parse_scalar_value(src, &elem)) {
         parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_VALUE);
         List.dispose(items);
         return false;
      }
      List.append(items, elem);

      Source.skip_whitespace_and_comments(src);

      if (Source.peek(src) == ',') {
         Source.consume(src, 1);
         Source.skip_whitespace_and_comments(src);
         continue; // may land on ']' now (trailing comma) — loop condition handles it
      }
      if (Source.peek(src) == ANVL_TOK_RBRACKET) {
         break;
      }

      parser_set_error(src, ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY);
      List.dispose(items);
      return false;
   }

   if (Source.peek(src) != ANVL_TOK_RBRACKET) {
      parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE);
      List.dispose(items);
      return false;
   }
   Source.consume(src, 1); // consume ']'

   // set value type & initialize the value slice
   (*out_value)->type = ANVL_VALUE_ARRAY;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = Source.at(src);
   (*out_value)->collection.items = items;

   return true; // Successfully parsed array
}
// Parse a tuple value. Same element-parsing shape as parse_array (see its own doc comment) —
// elements via parse_scalar_value, AMP rejects a nested collection element on sight of its
// leading symbol, reusing ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR (matching the legacy parser's
// own choice, src/core/_parser.c:876-880, to reuse the array error code for tuple's identical
// AMP restriction rather than a separate TUPLE-specific one). Unlike an array, a tuple
// requires at least two elements — empty and single-element tuples are both parse errors,
// since a one-element tuple isn't meaningfully positional (checked before consuming the
// closing ')', matching legacy's own ordering). AML's eventual "a tuple element may be any
// value, including a nested array/object/tuple" is deferred until object/array-value dispatch
// exists elsewhere — element parsing here is scalar-only for now, same as array.
static bool parse_tuple(anvl_source src, anvl_value *out_value) {
   const char *start = Source.at(src);
   // consume the tuple start token
   Source.consume(src, 1);
   Source.skip_whitespace_and_comments(src);

   // Check for empty tuple (INVALID)
   if (Source.peek(src) == ANVL_TOK_RPAREN) {
      parser_set_error(src, ANVL_ERR_PARSER_EMPTY_TUPLE_NOT_ALLOWED);
      return false;
   }

   list items = List.new(4, sizeof(anvl_value));
   if (!items) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false;
   }

   while (!Source.is_eof(src) && Source.peek(src) != ANVL_TOK_RPAREN) {
      // AMP forbids nested collections as tuple elements — same reject-on-sight
      // treatment as parse_array; see its own doc comment for why.
      char lead = Source.peek(src);
      if (Source.dialect(src) == ANVL_DIALECT_AMP &&
          (lead == ANVL_TOK_LBRACE || lead == ANVL_TOK_LBRACKET || lead == ANVL_TOK_LPAREN)) {
         parser_set_error(src, ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR);
         List.dispose(items);
         return false;
      }

      anvl_value elem = Source.new_node(src, ANVL_NODE_VALUE, &parser.err_code);
      if (!elem) {
         parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
         List.dispose(items);
         return false;
      }
      if (!parse_scalar_value(src, &elem)) {
         parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_VALUE);
         List.dispose(items);
         return false;
      }
      List.append(items, elem);

      Source.skip_whitespace_and_comments(src);

      if (Source.peek(src) == ',') {
         Source.consume(src, 1);
         Source.skip_whitespace_and_comments(src);
         continue; // may land on ')' now (trailing comma) — loop condition handles it
      }
      if (Source.peek(src) == ANVL_TOK_RPAREN) {
         break;
      }

      parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE);
      List.dispose(items);
      return false;
   }

   if (Source.peek(src) != ANVL_TOK_RPAREN) {
      parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE);
      List.dispose(items);
      return false;
   }

   // minimum 2 elements — checked before consuming ')', matching legacy's own ordering
   if (List.size(items) < 2) {
      parser_set_error(src, ANVL_ERR_PARSER_TUPLE_TOO_FEW_ELEMENTS);
      List.dispose(items);
      return false;
   }
   Source.consume(src, 1); // consume ')'

   // set value type & initialize the value slice
   (*out_value)->type = ANVL_VALUE_TUPLE;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = Source.at(src);
   (*out_value)->collection.items = items;

   return true; // Successfully parsed tuple
}
// Parse an object-typed value ('name := { statements };') — the exact same recursive
// statement-list shape as the direct OBJECT_BLOCK statement form (see parse_statement_list's
// own doc comment), just reached through ':=' instead of directly. No 'PAIR'/key-value form —
// object.statements is a nested statement list, same as OBJECT_BLOCK's own body.
static bool parse_object_value(anvl_source src, anvl_value *out_value) {
   const char *start = Source.at(src);

   list statements = NULL;
   if (!parse_statement_list(src, &statements)) {
      return false; // parse_statement_list already set its own error
   }

   // set value type & initialize the value slice
   (*out_value)->type = ANVL_VALUE_OBJECT;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = Source.at(src);
   (*out_value)->object.statements = statements;

   return true; // Successfully parsed object value
}
// Parse an error and record it in the source's error state
static void parser_set_error(anvl_source src, anvl_err_code code) {
   parser.err_code = code;
   // Optionally log the error with source position information
   usize line = Source.line(src);
   usize col = Source.column(src);
   const char *err_msg = anvl_error_code_message(code);

   Source.set_error(src, code, line, col, err_msg, &parser.err_code);
}