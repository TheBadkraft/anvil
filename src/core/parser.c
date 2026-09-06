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
// Array and tuple differ only in delimiter, error codes, and minimum element count — this is
// that difference, data instead of duplicated control flow (see parse_collection). too_few_err
// is only consulted when min_elements > 1 (array's own minimum of 1 is already fully covered
// by the empty-`[]` check up front, so it never needs a second, always-false check or a
// placeholder code).
typedef struct {
   char close;
   anvl_err_code empty_err;
   anvl_err_code missing_delim_err;
   anvl_err_code expected_close_err;
   usize min_elements;
   anvl_err_code too_few_err;
   anvl_value_type value_type;
} collection_spec_t;

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
static bool parse_value_body(anvl_source, anvl_value *);
static bool parse_scalar_value(anvl_source, anvl_value *);
static bool parse_reserved_literal(anvl_source, anvl_value *);
static bool parse_numeric_literal(anvl_source, anvl_value *);
static bool parse_string_literal(anvl_source, anvl_value *);
static bool parse_blob_literal(anvl_source, anvl_value *);
static bool parse_bare_literal(anvl_source, anvl_value *);
static bool parse_collection(anvl_source, anvl_value *, const collection_spec_t *);
static bool parse_array(anvl_source, anvl_value *);
static bool parse_tuple(anvl_source, anvl_value *);
static bool parse_object_value(anvl_source, anvl_value *);
static bool parse_varref(anvl_source, anvl_value *);
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
// parser_set_error's own call into Source.set_error clobbers parser.err_code back to
// ANVL_ERR_NONE as a side effect (that out-param reports whether *recording* the error
// succeeded, not the error itself — see source_set_error, src/core/source.c) — the same reason
// doc_parse_body (src/core/document.c) reads the real code back from doc->context->errors
// instead of trusting parser.err_code after a failure. Mirrors that exact pattern.
static anvl_err_code last_context_error(module_document doc) {
   if (!doc || !doc->context) {
      return ANVL_ERR_PARSER_INITIALIZATION_FAILED;
   }
   usize count = List.size(doc->context->errors);
   if (count == 0) {
      return ANVL_ERR_PARSER_INITIALIZATION_FAILED;
   }
   anvl_error last = anvl_error_get(doc->context->errors, count - 1);
   return last ? last->code : ANVL_ERR_PARSER_INITIALIZATION_FAILED;
}
// Parse a single, standalone value expression ("Value Fragment") — see internal/parser.h for
// the full contract. Reuses parse_value_body completely unchanged, so every existing grammar
// rule (empty collections, tuple arity, AMP's scalar-only element rule, AMP's object ban,
// unterminated strings, ...) is inherited for free — no second, divergent copy of that
// validation to keep in sync. Deliberately bypasses parser_init/parser.state_map entirely:
// those exist to track concurrent whole-document parses, which a single value expression has
// no need of.
anvl_result anvl_parse_value_fragment(module_document doc, anvl_value *out_value,
                                      anvl_err_code *out_err_code) {
   if (!doc || !doc->source || !doc->context || !out_value) {
      if (out_err_code) {
         *out_err_code = ANVL_ERR_INVALID_ARGUMENT;
      }
      return ANVL_RES_ERR;
   }

   anvl_source src = doc->source;
   Source.skip_whitespace_and_comments(src);

   anvl_value value = Source.new_node(src, ANVL_NODE_VALUE, &parser.err_code);
   if (!value) {
      if (out_err_code) {
         *out_err_code = parser.err_code;
      }
      return ANVL_RES_ERR;
   }

   if (!parse_value_body(src, &value)) {
      if (out_err_code) {
         *out_err_code = last_context_error(doc);
      }
      return ANVL_RES_ERR;
   }

   Source.skip_whitespace_and_comments(src);
   if (Source.peek(src) == ANVL_TOK_STMT_TERMINATOR) {
      Source.consume(src, 1); // tolerated, never required — a fragment isn't a statement
      Source.skip_whitespace_and_comments(src);
   }
   if (!Source.is_eof(src)) {
      parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
      if (out_err_code) {
         *out_err_code = last_context_error(doc);
      }
      return ANVL_RES_ERR;
   }

   // No VarRef support at any nesting depth: rather than a hand-rolled recursive walk of the
   // value tree, reuse the flat doc->context->values index — every value node allocated during
   // this parse, at any depth, lands there via Source.new_node (same mechanism module.c's own
   // disposal pass relies on to reach every nested value without recursion).
   usize value_count = List.size(doc->context->values);
   for (usize i = 0; i < value_count; i++) {
      anvl_value v = NULL;
      List.get(doc->context->values, i, (object *)&v);
      if (v && v->type == ANVL_VALUE_VARREF) {
         parser_set_error(src, ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_FRAGMENT);
         if (out_err_code) {
            *out_err_code = last_context_error(doc);
         }
         return ANVL_RES_ERR;
      }
   }

   *out_value = value;
   if (out_err_code) {
      *out_err_code = ANVL_ERR_NONE;
   }
   return ANVL_RES_OK;
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
         // results stay inspectable on doc->body, not silently dropped. A throwaway out-param
         // here, not &parser.err_code — finish_body's own out-param reports whether *this*
         // bookkeeping call succeeded, which it does even after a real parse failure, and would
         // otherwise clobber the real code parser_set_error just recorded above.
         anvl_err_code finish_err_code = ANVL_ERR_NONE;
         Source.finish_body(ctx->src, ctx->statements, &finish_err_code);
         ctx->statements = NULL; // ownership transferred to finish_body regardless of its result
         return false;
      }
      // TODO: we need to check for illegal keywords after statements
      anvl_statement stmt = NULL;
      if (!parse_statement(ctx->src, &stmt)) {
         ctx->end = Time.now(); // Record the end time even on failure, for accurate instrumentation
         // Same reasoning as the shebang branch above — preserve parse_statement's own recorded
         // error rather than letting finish_body's unrelated bookkeeping result overwrite it.
         anvl_err_code finish_err_code = ANVL_ERR_NONE;
         Source.finish_body(ctx->src, ctx->statements, &finish_err_code);
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
         if (Source.peek(src) == '$') {
            parser_set_error(src, ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_ATTRIBUTE);
            Allocator.dispose(attr);
            dispose_attribute_list(attrs);
            return false;
         }
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
// Value dispatch shared by parse_value (a statement's own value, ';'-terminated) and
// parse_collection (array/tuple elements, ','/close-delimiter-terminated) — "what kind of
// value starts here," with no opinion on what has to follow it. Does not allocate the value
// node itself (the caller already did, via Source.new_node) or check for a terminator.
// Scalar, then array/tuple/object by leading symbol. Only the final `else` sets a generic
// error — every other branch either succeeds or has already set its own, more specific error
// via parser_set_error internally, so there's exactly one call site for
// ANVL_ERR_PARSER_EXPECTED_VALUE in the whole file, shared by every caller, and no risk of a
// generic error silently overwriting a specific one that already fired.
static bool parse_value_body(anvl_source src, anvl_value *out_value) {
   if (parse_scalar_value(src, out_value)) {
      return true;
   }
   if (Source.peek(src) == ANVL_TOK_LBRACKET) {
      return parse_array(src, out_value); // array already sets its own error on failure
   }
   if (Source.peek(src) == ANVL_TOK_LPAREN) {
      return parse_tuple(src, out_value); // tuple already sets its own error on failure
   }
   if (Source.peek(src) == ANVL_TOK_LBRACE) {
      // AMP forbids an object-typed value entirely (AMP16 at top level; AMP20-style rejection
      // for a nested element is handled earlier, in parse_collection, before this is ever
      // reached for an element) — rejected on sight, same shape as every other AMP-forbidden
      // construct in this file.
      if (Source.dialect(src) == ANVL_DIALECT_AMP) {
         parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
         return false;
      }
      return parse_object_value(src, out_value); // object already sets its own error on failure
   }
   if (Source.peek(src) == '$') {
      // AMP forbids a VarRef entirely — rejected on sight, same shape as every other
      // AMP-forbidden construct in this file.
      if (Source.dialect(src) == ANVL_DIALECT_AMP) {
         parser_set_error(src, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
         return false;
      }
      return parse_varref(src, out_value); // varref already sets its own error on failure
   }
   parser_set_error(src, ANVL_ERR_PARSER_EXPECTED_VALUE);
   return false; // Not a scalar, not a collection start — genuinely not a value
}
// Parse a value from the source
static bool parse_value(anvl_source src, anvl_value *out_value) {
   *out_value = Source.new_node(src, ANVL_NODE_VALUE, &parser.err_code);
   if (!*out_value) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false; // Failed to allocate value node
   }

   if (!parse_value_body(src, out_value)) {
      return false; // parse_value_body already set the appropriate error
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

   // Capture the end position before consuming the closing quote itself, not after -
   // otherwise .text would include the closing quote as a trailing byte.
   const char *end = Source.at(src);

   // Consume the closing quote
   Source.consume(src, 1);

   // set value type & initialize the value slice
   (*out_value)->type = ANVL_VALUE_STRING;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = end;

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

   // Capture the end position before consuming the closing backtick itself, not after -
   // otherwise .text would include the closing backtick as a trailing byte.
   const char *end = Source.at(src);

   // Consume the closing backtick
   Source.consume(src, 1);

   // set value type & initialize the value slice
   (*out_value)->type = ANVL_VALUE_BLOB;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = end;
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
// it's the VarRef sigil, dispatched separately by parse_value_body (see parse_varref below).
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
// Parse a '$identifier' VarRef — a static, resolve-once reference to another statement's
// value (see notes/document-body-parse.md "$ VarRef reinstated"). Strictly bare: no dotted
// path, no call syntax — matching the JS reference parser's parseVarRef. Purely syntactic:
// the parser never checks whether the target actually exists as a declared identifier, and
// does not apply parse_identifier's keyword rejection to it either — an unmatched *or*
// keyword-shaped target both just resolve to `null` at Resolution time, not a parse error
// (see the ANVL_ERR_VARS_INVALID_VARREF doc comment, errors.h). AMP forbids '$' entirely —
// checked by the caller (parse_value_body) before this is ever reached.
static bool parse_varref(anvl_source src, anvl_value *out_value) {
   const char *start = Source.at(src);
   Source.consume(src, 1); // '$'

   if (!Source.is_identifier_start(Source.peek(src))) {
      parser_set_error(src, ANVL_ERR_VARS_INVALID_VARREF);
      return false;
   }

   const char *target_start = Source.at(src);
   Source.consume(src, 1);
   while (Source.is_identifier_part(Source.peek(src))) {
      Source.consume(src, 1);
   }
   const char *target_end = Source.at(src);

   // A dotted path after an otherwise well-formed target is a distinct, common mistake worth
   // naming precisely, rather than silently truncating to the leading segment and letting the
   // leftover '.rest' trip a confusing, unrelated failure downstream — same rationale as the
   // '@'/'`' check in parse_bare_literal above.
   if (Source.peek(src) == ANVL_TOK_DOT) {
      parser_set_error(src, ANVL_ERR_VARS_INVALID_VARREF);
      return false;
   }

   (*out_value)->type = ANVL_VALUE_VARREF;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = target_end;
   Source.init_slice(src, &(*out_value)->varref.target);
   (*out_value)->varref.target.start = target_start;
   (*out_value)->varref.target.end = target_end;

   return true;
}
// Shared skeleton for array/tuple parsing (see parse_array/parse_tuple below for the specifics
// each one plugs in). Elements are comma-separated (trailing comma allowed), parsed via
// parse_value_body — any value, not just scalar; AMP's scalar-only restriction is enforced
// right here, per element, by rejecting a nested collection on sight of its leading symbol,
// not by restricting what parse_value_body itself can produce (matching the JS reference
// parser's parseAmpElement, not the legacy parser's parse-then-check-type approach — see
// notes/document-body-parse.md "Dialect scope"). The list itself owns its `list items`
// directly — no per-element pos/len bookkeeping needed the way the legacy parser
// (src/core/_parser.c:733-833, 834-935) tracked it, since every anvl_value already carries
// its own `.text` slice from whichever parser produced it.
static bool parse_collection(anvl_source src, anvl_value *out_value, const collection_spec_t *spec) {
   const char *start = Source.at(src);
   Source.consume(src, 1); // open delimiter — already confirmed present by the caller
   Source.skip_whitespace_and_comments(src);

   if (Source.peek(src) == spec->close) {
      parser_set_error(src, spec->empty_err);
      return false;
   }

   list items = List.new(4, sizeof(anvl_value));
   if (!items) {
      parser_set_error(src, ANVL_ERR_MEMORY_ALLOC_FAILED);
      return false;
   }

   while (!Source.is_eof(src) && Source.peek(src) != spec->close) {
      // AMP forbids nested collections as array/tuple elements — reject on sight of the
      // leading symbol, before wasting an attempt on parse_value_body.
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
      if (!parse_value_body(src, &elem)) {
         List.dispose(items);
         return false; // parse_value_body already set its own error
      }
      List.append(items, elem);

      Source.skip_whitespace_and_comments(src);

      if (Source.peek(src) == ',') {
         Source.consume(src, 1);
         Source.skip_whitespace_and_comments(src);
         continue; // may land on the close delimiter now (trailing comma) — loop condition handles it
      }
      if (Source.peek(src) == spec->close) {
         break;
      }

      parser_set_error(src, spec->missing_delim_err);
      List.dispose(items);
      return false;
   }

   if (Source.peek(src) != spec->close) {
      parser_set_error(src, spec->expected_close_err);
      List.dispose(items);
      return false;
   }

   // minimum element count — checked before consuming the close delimiter, matching the
   // legacy tuple parser's own ordering.
   if (spec->min_elements > 1 && List.size(items) < spec->min_elements) {
      parser_set_error(src, spec->too_few_err);
      List.dispose(items);
      return false;
   }
   Source.consume(src, 1); // close delimiter

   // set value type & initialize the value slice
   (*out_value)->type = spec->value_type;
   Source.init_slice(src, &(*out_value)->text);
   (*out_value)->text.start = start;
   (*out_value)->text.end = Source.at(src);
   (*out_value)->collection.items = items;

   return true; // Successfully parsed collection
}
// Parse an array value ('[ elements ]'). See parse_collection for the shared shape.
static bool parse_array(anvl_source src, anvl_value *out_value) {
   static const collection_spec_t spec = {
      .close = ANVL_TOK_RBRACKET,
      .empty_err = ANVL_ERR_PARSER_ARRAY_CANNOT_BE_EMPTY,
      .missing_delim_err = ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY,
      .expected_close_err = ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE,
      .min_elements = 1,
      .too_few_err = ANVL_ERR_NONE, // unused: min_elements <= 1, see parse_collection's doc comment
      .value_type = ANVL_VALUE_ARRAY,
   };
   return parse_collection(src, out_value, &spec);
}
// Parse a tuple value ('( elements )'). See parse_collection for the shared shape. Unlike an
// array, a tuple requires at least two elements — empty and single-element tuples are both
// parse errors, since a one-element tuple isn't meaningfully positional. Reuses
// ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR for tuple's identical AMP restriction rather than a
// separate TUPLE-specific code, matching the legacy parser's own choice
// (src/core/_parser.c:876-880).
static bool parse_tuple(anvl_source src, anvl_value *out_value) {
   static const collection_spec_t spec = {
      .close = ANVL_TOK_RPAREN,
      .empty_err = ANVL_ERR_PARSER_EMPTY_TUPLE_NOT_ALLOWED,
      .missing_delim_err = ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE,
      .expected_close_err = ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE,
      .min_elements = 2,
      .too_few_err = ANVL_ERR_PARSER_TUPLE_TOO_FEW_ELEMENTS,
      .value_type = ANVL_VALUE_TUPLE,
   };
   return parse_collection(src, out_value, &spec);
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
   // Captured *before* Source.set_error runs — its own out-param reports whether *recording*
   // the error succeeded (ANVL_ERR_NONE on success, including a first-error-wins no-op inside
   // anvl_error_set), not the error itself, so the call below always leaves parser.err_code as
   // ANVL_ERR_NONE as a side effect, regardless of what's being recorded or whether this is the
   // first call or a later one. `prior` is what lets this function tell those cases apart and
   // decide what parser.err_code should actually end up holding.
   anvl_err_code prior = parser.err_code;
   bool already_recorded = prior != ANVL_ERR_NONE;

   usize line = Source.line(src);
   usize col = Source.column(src);
   const char *err_msg = anvl_error_code_message(code);

   Source.set_error(src, code, line, col, err_msg, &parser.err_code);

   // First-error-wins, mirroring anvl_error_set's own protection for ctx->errors (see its
   // comment) — parser.err_code has no such protection of its own otherwise. Without this, a
   // later, more generic fallback error from an outer caller (e.g. parse_statement's
   // ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, called unconditionally whenever parse_identifier
   // returns false, for *any* reason) would silently overwrite a more specific one an inner
   // callee already recorded moments earlier (e.g. parse_identifier's own
   // ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD) — even though ctx->errors itself already protects
   // the *recorded* error from exactly that, parser.err_code was quietly drifting out of sync
   // with it. Restoring `prior` here, not just declining to overwrite it with `code`, matters:
   // the Source.set_error call above still resets parser.err_code to ANVL_ERR_NONE as a side
   // effect on *every* call, first or not, so the first-recorded value has to be put back
   // explicitly, not merely left alone.
   parser.err_code = already_recorded ? prior : code;
}