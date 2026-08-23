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
static bool parse_identifier(anvl_source, anvl_slice *);
static bool parse_value(anvl_source, anvl_value *);
static bool parse_scalar_value(anvl_source, anvl_value *);
static bool parse_numeric_literal(anvl_source, anvl_value *);
static void parse_error(anvl_source, anvl_err_code);

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

   if (!source) {
      parser.err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

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
   usize pos = Source.position(ctx->src);
   ctx->start = Time.now(); // Record the start time of parsing

   while (!Source.is_eof(ctx->src)) {
      // we still have to have this check here, too
      if (Source.match_length(ctx->src, "#!", 2) == 2) {
         parse_error(ctx->src, ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS);
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
   return pos == length; // Return true if all source was consumed, false otherwise

error:
   return false;
}
static bool parse_statement(anvl_source src, anvl_statement *out_stmt) {
   // Parse statement
   *out_stmt = Source.new_node(src, ANVL_NODE_STATEMENT, &parser.err_code);
   if (!*out_stmt) {
      parse_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false; // Failed to allocate statement node
   }

   // - identifier
   if (!parse_identifier(src, &(*out_stmt)->name)) {
      parse_error(src, ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
      return false;
   }
   // - consume whitespace
   Source.skip_whitespace_and_comments(src);

   // - assignment operator
   if (!Source.match_operator(src, ":=", 2)) {
      parse_error(src, ANVL_ERR_PARSER_EXPECTED_ASSIGN);
      return false;
   }
   // - consume assignment operator
   Source.consume(src, 2);
   // - consume whitespace
   Source.skip_whitespace_and_comments(src);

   // - value
   if (!parse_value(src, &(*out_stmt)->value)) {
      parse_error(src, ANVL_ERR_PARSER_EXPECTED_VALUE);
      return false;
   }
   // - consume whitespace
   Source.skip_whitespace_and_comments(src);

   return true;
}
static bool parse_identifier(anvl_source src, anvl_slice *out_identifier) {
   // initialize the slice
   Source.init_slice(src, out_identifier);
   out_identifier->start = NULL;
   out_identifier->end = NULL;

   if (!Source.is_identifier_start(Source.peek(src))) {
      parse_error(src, ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
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

   return true;
}
static bool parse_value(anvl_source src, anvl_value *out_value) {
   *out_value = Source.new_node(src, ANVL_NODE_VALUE, &parser.err_code);
   if (!*out_value) {
      parse_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false; // Failed to allocate value node
   }

   // 1. check for scalar value - numeric, string or blob, boolean, null
   if (!parse_scalar_value(src, out_value)) {
      parse_error(src, ANVL_ERR_PARSER_EXPECTED_VALUE);
      return false; // Failed to parse scalar value
   }

   // x. end of statement terminator `;` is required for all dialects.
   if (Source.peek(src) != ';') {
      parse_error(src, ANVL_ERR_PARSER_UNTERMINATED_STATEMENT);
      return false;
   }
   Source.consume(src, 1);
   Source.skip_whitespace_and_comments(src);

   return true;
}
static bool parse_scalar_value(anvl_source src, anvl_value *out_value) {
   // 1. numeric literal - integer, float, hex, exponential, etc. (`parse_numeric_literal` handles
   // all numeric formats)
   if (parse_numeric_literal(src, out_value)) {
      return true; // Successfully parsed numeric literal
   }
   // [TODO] 2. string literal - quoted string with optional escape sequences
   // [TODO] 3. blob literal - tagged content in text span
   // [TODO] 4. boolean literal - `true` or `false`
   // [TODO] 5. null literal - `null` keyword

   return false; // Failed to parse scalar value
}

static bool parse_numeric_literal(anvl_source src, anvl_value *out_value) {
   // does it make sense to have error codes here? are there errors we can specifically detect for
   // numeric literals? maybe invalid characters in the number, or overflow? for now, let's just
   // return false if we can't parse a valid number
   if (Source.is_digit(Source.peek(src))) {
      const char *start = Source.at(src);
      while (Source.is_digit(Source.peek(src))) {
         Source.consume(src, 1);
      }
      (*out_value)->type = ANVL_VALUE_NUMERIC;

      Source.init_slice(src, &(*out_value)->text);
      (*out_value)->text.start = start;
      (*out_value)->text.end = Source.at(src);
      return true;
   }

   return false;
}
static void parse_error(anvl_source src, anvl_err_code code) {
   parser.err_code = code;
   // Optionally log the error with source position information
   usize line = Source.line(src);
   usize col = Source.column(src);
   const char *err_msg = anvl_error_code_message(code);

   Source.set_error(src, code, line, col, err_msg, &parser.err_code);
}