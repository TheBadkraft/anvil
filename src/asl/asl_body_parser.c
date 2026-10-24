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
 * asl_body_parser.c - AnvlScript function-body parser (WITH_ASL)         *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: src/asl/asl_body_parser.c                                        *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * See include/internal/asl_ast.h. Deliberately not built on anvl_source: *
 * a function body is a different grammar (operators, keywords, no AML   *
 * statement forms) scanned independently of the original document       *
 * parse, often long after it finished (lazy compilation). asl_cursor_t   *
 * is this file's own minimal position tracker over the raw body slice.  *
 * Only ever compiled when WITH_ASL=1 -- excluded at the Makefile level  *
 * (ASL_SRCS), not by an internal #if, since nothing calls this function  *
 * at all in a WITH_ASL=0 build.                                         *
 * ********************************************************************** */
#include "internal/asl_ast.h"
#include "internal/source.h"
#include "constants.h"
#include "errors.h"

#include <system_alloc.h>
#include <string.h>

typedef struct {
   const char *data; // original document buffer start -- propagated into every slice this
                      // file constructs, so Source.slice_equals/slice_length (which require
                      // a non-NULL .data to report correctly) work on them.
   const char *pos;
   const char *end;
} asl_cursor_t;

static bool asl_body_is_eof(const asl_cursor_t *cur) {
   return cur->pos >= cur->end;
}

static char asl_body_peek(const asl_cursor_t *cur) {
   return asl_body_is_eof(cur) ? '\0' : *cur->pos;
}

static char asl_body_peek_offset(const asl_cursor_t *cur, usize offset) {
   return (cur->pos + offset < cur->end) ? cur->pos[offset] : '\0';
}

static void asl_body_consume(asl_cursor_t *cur, usize n) {
   cur->pos += n;
   if (cur->pos > cur->end) {
      cur->pos = cur->end;
   }
}

// Same whitespace/comment shape as asl_scan_body (src/core/asl_parser.c) -- deliberately not
// shared, since that one walks an anvl_source and this one walks a raw asl_cursor_t; a real
// scanner abstraction unifying the two isn't worth building for this much logic yet.
static void asl_body_skip_ws(asl_cursor_t *cur) {
   while (!asl_body_is_eof(cur)) {
      char c = asl_body_peek(cur);
      if (c == ' ' || c == '\t' || c == '\r' || c == ANVL_TOK_NEWLINE) {
         asl_body_consume(cur, 1);
         continue;
      }
      if (c == '/' && asl_body_peek_offset(cur, 1) == '/') {
         while (!asl_body_is_eof(cur) && asl_body_peek(cur) != ANVL_TOK_NEWLINE) {
            asl_body_consume(cur, 1);
         }
         continue;
      }
      if (c == '/' && asl_body_peek_offset(cur, 1) == '*') {
         asl_body_consume(cur, 2);
         while (!asl_body_is_eof(cur) &&
                !(asl_body_peek(cur) == '*' && asl_body_peek_offset(cur, 1) == '/')) {
            asl_body_consume(cur, 1);
         }
         if (!asl_body_is_eof(cur)) {
            asl_body_consume(cur, 2);
         }
         continue;
      }
      break;
   }
}

// Reuses Source.is_identifier_start/is_identifier_part directly -- pure character
// classifiers (take a bare char, no anvl_source involved), so there's nothing to duplicate.
static bool asl_body_scan_identifier(asl_cursor_t *cur, anvl_slice *out) {
   if (!Source.is_identifier_start(asl_body_peek(cur))) {
      return false;
   }
   const char *start = cur->pos;
   asl_body_consume(cur, 1);
   while (!asl_body_is_eof(cur) && Source.is_identifier_part(asl_body_peek(cur))) {
      asl_body_consume(cur, 1);
   }
   out->data = cur->data;
   out->start = start;
   out->end = cur->pos;
   return true;
}

// Bare non-negative integers only, this slice -- no floats/hex/exponents yet (parser.c's
// own parse_numeric_literal handles those, but it's anvl_source-bound; this scanner grows
// its own literal forms as real tests ask for them, same as everything else in this file).
static bool asl_body_scan_integer(asl_cursor_t *cur, anvl_slice *out) {
   if (!(asl_body_peek(cur) >= '0' && asl_body_peek(cur) <= '9')) {
      return false;
   }
   const char *start = cur->pos;
   while (!asl_body_is_eof(cur) && asl_body_peek(cur) >= '0' && asl_body_peek(cur) <= '9') {
      asl_body_consume(cur, 1);
   }
   out->data = cur->data;
   out->start = start;
   out->end = cur->pos;
   return true;
}

// True if `word` appears at the cursor and isn't just a prefix of a longer identifier
// ("variant" must not match "var"). Does not consume -- the caller consumes exactly
// strlen(word) bytes itself once it's committed to this keyword, mirroring how
// Source.match_token (parser.c) works.
static bool asl_body_match_word(const asl_cursor_t *cur, const char *word) {
   usize len = strlen(word);
   if ((usize)(cur->end - cur->pos) < len) {
      return false;
   }
   if (memcmp(cur->pos, word, len) != 0) {
      return false;
   }
   return !Source.is_identifier_part(asl_body_peek_offset(cur, len));
}

static struct asl_ast_node *asl_body_alloc_node(bump_allocator arena, asl_ast_kind kind) {
   struct asl_ast_node *node = arena->alloc(arena, sizeof(struct asl_ast_node));
   if (node) {
      node->kind = kind;
   }
   return node;
}

static struct asl_ast_node *asl_body_parse_expr(asl_cursor_t *cur, bump_allocator arena,
                                                 anvl_err_code *out_err);

// primary := identifier | integer_literal | "(" expr ")"   (this slice only -- no other
// literal kinds yet; notes/anvilscript-design.md's full grammar is much larger). Grouping
// recurses all the way back up to asl_body_parse_expr -- full precedence applies again
// inside the parens -- and returns the inner node directly, leaving no node of its own in
// the tree; the parens exist only to override precedence while parsing, never to be seen
// again afterward.
static struct asl_ast_node *asl_body_parse_primary(asl_cursor_t *cur, bump_allocator arena,
                                                    anvl_err_code *out_err) {
   asl_body_skip_ws(cur);

   if (asl_body_peek(cur) == '(') {
      asl_body_consume(cur, 1);
      struct asl_ast_node *inner = asl_body_parse_expr(cur, arena, out_err);
      if (!inner) {
         return NULL;
      }
      asl_body_skip_ws(cur);
      if (asl_body_peek(cur) != ')') {
         *out_err = ANVL_ERR_PARSER_EXPECTED_VALUE;
         return NULL;
      }
      asl_body_consume(cur, 1);
      return inner;
   }

   anvl_slice ident;
   if (asl_body_scan_identifier(cur, &ident)) {
      struct asl_ast_node *node = asl_body_alloc_node(arena, ASL_EXPR_IDENTIFIER);
      if (!node) {
         *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
         return NULL;
      }
      node->span = ident;
      node->identifier.name = ident;
      return node;
   }

   anvl_slice digits;
   if (asl_body_scan_integer(cur, &digits)) {
      anvl_value value = arena->alloc(arena, sizeof(anvl_value_t));
      if (!value) {
         *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
         return NULL;
      }
      value->type = ANVL_VALUE_NUMERIC;
      value->text = digits;

      struct asl_ast_node *node = asl_body_alloc_node(arena, ASL_EXPR_LITERAL);
      if (!node) {
         *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
         return NULL;
      }
      node->span = digits;
      node->literal.value = value;
      return node;
   }

   *out_err = ANVL_ERR_PARSER_EXPECTED_VALUE;
   return NULL;
}

// unary := ( "-" | "!" | "+" ) unary | primary   (prefix, right-recursive on itself rather
// than a precedence tier built from asl_body_parse_binary_level -- there's no left operand
// to fold against, just a chain of prefixes. Binds tighter than multiplicative, which is
// why it's multiplicative's own operand parser now, replacing bare primary.)
static struct asl_ast_node *asl_body_parse_unary(asl_cursor_t *cur, bump_allocator arena,
                                                  anvl_err_code *out_err) {
   asl_body_skip_ws(cur);

   char c = asl_body_peek(cur);
   if (c == '-' || c == '!' || c == '+') {
      const char *op_start = cur->pos;
      asl_body_consume(cur, 1);

      struct asl_ast_node *operand = asl_body_parse_unary(cur, arena, out_err);
      if (!operand) {
         return NULL;
      }

      struct asl_ast_node *node = asl_body_alloc_node(arena, ASL_EXPR_UNARY);
      if (!node) {
         *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
         return NULL;
      }
      node->unary.op.data = cur->data;
      node->unary.op.start = op_start;
      node->unary.op.end = op_start + 1;
      node->unary.operand = operand;
      return node;
   }

   return asl_body_parse_primary(cur, arena, out_err);
}

typedef struct asl_ast_node *(*asl_operand_parser_fn)(asl_cursor_t *, bump_allocator,
                                                       anvl_err_code *);

// Length of the operator in the NULL-terminated `ops` array that matches at the cursor's
// current position, or 0 if none do. Checked in array order -- callers list longer/more
// specific forms first (e.g. "<=" before "<") so a two-byte operator is never mistaken for
// a one-byte prefix of itself with the second byte left dangling.
static usize asl_body_match_operator(const asl_cursor_t *cur, const char *const *ops) {
   for (usize i = 0; ops[i]; i++) {
      usize len = strlen(ops[i]);
      if ((usize)(cur->end - cur->pos) >= len && memcmp(cur->pos, ops[i], len) == 0) {
         return len;
      }
   }
   return 0;
}

// Generic left-associative binary-operator precedence tier: parses one operand via
// `parse_operand`, then repeatedly looks for any operator in `ops` (NULL-terminated, longer
// forms first) and folds into ASL_EXPR_BINARY nodes, left-associative. Every precedence tier
// in this grammar -- multiplicative, additive, and now comparison/equality/logical below --
// is exactly this loop; only the operand parser and the operator set change between tiers,
// which is why this exists as one shared function instead of a copy per tier.
static struct asl_ast_node *asl_body_parse_binary_level(asl_cursor_t *cur, bump_allocator arena,
                                                         anvl_err_code *out_err,
                                                         asl_operand_parser_fn parse_operand,
                                                         const char *const *ops) {
   struct asl_ast_node *left = parse_operand(cur, arena, out_err);
   if (!left) {
      return NULL;
   }

   while (true) {
      asl_body_skip_ws(cur);
      usize op_len = asl_body_match_operator(cur, ops);
      if (op_len == 0) {
         break;
      }
      const char *op_start = cur->pos;
      asl_body_consume(cur, op_len);
      asl_body_skip_ws(cur);

      struct asl_ast_node *right = parse_operand(cur, arena, out_err);
      if (!right) {
         return NULL;
      }

      struct asl_ast_node *node = asl_body_alloc_node(arena, ASL_EXPR_BINARY);
      if (!node) {
         *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
         return NULL;
      }
      node->binary.op.data = cur->data;
      node->binary.op.start = op_start;
      node->binary.op.end = op_start + op_len;
      node->binary.left = left;
      node->binary.right = right;
      left = node; // left-associative: the new node becomes the left operand for the next
                   // iteration, so 'a + b - c' nests as (a + b) - c, not a + (b - c).
   }

   return left;
}

// multiplicative := unary , { ( "*" | "/" | "%" ) , unary }   (binds tighter than additive
// below, by being the thing additive's own operands are built from rather than bare unary --
// the standard precedence-climbing trick. unary itself sits between this and primary.)
static struct asl_ast_node *asl_body_parse_multiplicative(asl_cursor_t *cur, bump_allocator arena,
                                                           anvl_err_code *out_err) {
   static const char *const ops[] = {"*", "/", "%", NULL};
   return asl_body_parse_binary_level(cur, arena, out_err, asl_body_parse_unary, ops);
}

// additive := multiplicative , { ( "+" | "-" ) , multiplicative }   (no unary minus yet --
// '-x' is a later slice.)
static struct asl_ast_node *asl_body_parse_additive(asl_cursor_t *cur, bump_allocator arena,
                                                     anvl_err_code *out_err) {
   static const char *const ops[] = {"+", "-", NULL};
   return asl_body_parse_binary_level(cur, arena, out_err, asl_body_parse_multiplicative, ops);
}

// comparison := additive , { ( "<=" | ">=" | "<" | ">" ) , additive }   ("<="/">=" listed
// first -- see asl_body_match_operator's own doc comment on why order matters here.
static struct asl_ast_node *asl_body_parse_comparison(asl_cursor_t *cur, bump_allocator arena,
                                                       anvl_err_code *out_err) {
   static const char *const ops[] = {"<=", ">=", "<", ">", NULL};
   return asl_body_parse_binary_level(cur, arena, out_err, asl_body_parse_additive, ops);
}

// equality := comparison , { ( "==" | "!=" ) , comparison }
static struct asl_ast_node *asl_body_parse_equality(asl_cursor_t *cur, bump_allocator arena,
                                                     anvl_err_code *out_err) {
   static const char *const ops[] = {"==", "!=", NULL};
   return asl_body_parse_binary_level(cur, arena, out_err, asl_body_parse_comparison, ops);
}

// logical_and := equality , { "&&" , equality }
static struct asl_ast_node *asl_body_parse_logical_and(asl_cursor_t *cur, bump_allocator arena,
                                                        anvl_err_code *out_err) {
   static const char *const ops[] = {"&&", NULL};
   return asl_body_parse_binary_level(cur, arena, out_err, asl_body_parse_equality, ops);
}

// expr := logical_or
// logical_or := logical_and , { "||" , logical_and }
static struct asl_ast_node *asl_body_parse_expr(asl_cursor_t *cur, bump_allocator arena,
                                                 anvl_err_code *out_err) {
   static const char *const ops[] = {"||", NULL};
   return asl_body_parse_binary_level(cur, arena, out_err, asl_body_parse_logical_and, ops);
}

// Shared tail for both ASL_STMT_VAR_DECL and ASL_STMT_ASSIGN -- '= expr ;' is identical
// between 'var name = expr;' and plain 'name = expr;', only `kind` and whether a leading
// 'var' keyword was consumed differ. `name` is already scanned by the caller.
static struct asl_ast_node *asl_body_parse_assign_tail(asl_cursor_t *cur, bump_allocator arena,
                                                        anvl_err_code *out_err, anvl_slice name,
                                                        asl_ast_kind kind) {
   asl_body_skip_ws(cur);

   if (asl_body_peek(cur) != '=') {
      *out_err = ANVL_ERR_PARSER_EXPECTED_ASSIGN;
      return NULL;
   }
   asl_body_consume(cur, 1);

   struct asl_ast_node *value = asl_body_parse_expr(cur, arena, out_err);
   if (!value) {
      return NULL;
   }
   asl_body_skip_ws(cur);

   if (asl_body_peek(cur) != ANVL_TOK_STMT_TERMINATOR) {
      *out_err = ANVL_ERR_PARSER_UNTERMINATED_STATEMENT;
      return NULL;
   }
   asl_body_consume(cur, 1);

   struct asl_ast_node *node = asl_body_alloc_node(arena, kind);
   if (!node) {
      *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
      return NULL;
   }
   node->assign.name = name;
   node->assign.value = value;
   return node;
}

// Builds 'name = name OP rhs;' as an ASL_STMT_ASSIGN wrapping a synthesized ASL_EXPR_BINARY
// -- the desugaring every compound-assignment ('+=' etc.) and increment/decrement ('++'/'--')
// form reduces to, so the evaluator (once built) needs no special-casing for any of them,
// only ordinary ASL_STMT_ASSIGN/ASL_EXPR_BINARY handling it already needs regardless.
// `op_start` must point at the real operator character in the source (the '+' of '+=', or
// of '++') -- reused directly as the synthesized binary node's own operator span, exactly
// like every other binary operator in this file captures it.
static struct asl_ast_node *asl_body_build_compound_assign(asl_cursor_t *cur, bump_allocator arena,
                                                            anvl_err_code *out_err, anvl_slice name,
                                                            const char *op_start,
                                                            struct asl_ast_node *rhs) {
   struct asl_ast_node *lhs_ref = asl_body_alloc_node(arena, ASL_EXPR_IDENTIFIER);
   if (!lhs_ref) {
      *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
      return NULL;
   }
   lhs_ref->span = name;
   lhs_ref->identifier.name = name;

   struct asl_ast_node *binary = asl_body_alloc_node(arena, ASL_EXPR_BINARY);
   if (!binary) {
      *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
      return NULL;
   }
   binary->binary.op.data = cur->data;
   binary->binary.op.start = op_start;
   binary->binary.op.end = op_start + 1;
   binary->binary.left = lhs_ref;
   binary->binary.right = rhs;

   struct asl_ast_node *node = asl_body_alloc_node(arena, ASL_STMT_ASSIGN);
   if (!node) {
      *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
      return NULL;
   }
   node->assign.name = name;
   node->assign.value = binary;
   return node;
}

// Synthesizes an ASL_EXPR_LITERAL node for the implicit '1' in 'x++;'/'x--;' -- there is no
// literal '1' token in the source to point a span at (only '++'/'--'), so unlike every other
// literal in this file, .text here is a static string, not a source slice. Harmless: nothing
// downstream needs a synthesized sugar node to trace back to a real source position.
static struct asl_ast_node *asl_body_make_one_literal(bump_allocator arena, anvl_err_code *out_err) {
   static const char one[] = "1";

   anvl_value value = arena->alloc(arena, sizeof(anvl_value_t));
   if (!value) {
      *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
      return NULL;
   }
   value->type = ANVL_VALUE_NUMERIC;
   value->text.data = one;
   value->text.start = one;
   value->text.end = one + 1;

   struct asl_ast_node *node = asl_body_alloc_node(arena, ASL_EXPR_LITERAL);
   if (!node) {
      *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
      return NULL;
   }
   node->literal.value = value;
   return node;
}

// stmt := "var" identifier "=" expr ";"
//       | identifier "=" expr ";"
//       | identifier ( "+=" | "-=" | "*=" | "/=" | "%=" ) expr ";"
//       | identifier ( "++" | "--" ) ";"
//       | "return" expr ";"
static struct asl_ast_node *asl_body_parse_stmt(asl_cursor_t *cur, bump_allocator arena,
                                                 anvl_err_code *out_err) {
   asl_body_skip_ws(cur);

   if (asl_body_match_word(cur, "var")) {
      asl_body_consume(cur, 3);
      asl_body_skip_ws(cur);

      anvl_slice name;
      if (!asl_body_scan_identifier(cur, &name)) {
         *out_err = ANVL_ERR_PARSER_EXPECTED_IDENTIFIER;
         return NULL;
      }
      return asl_body_parse_assign_tail(cur, arena, out_err, name, ASL_STMT_VAR_DECL);
   }

   if (asl_body_match_word(cur, "return")) {
      asl_body_consume(cur, 6);

      struct asl_ast_node *value = asl_body_parse_expr(cur, arena, out_err);
      if (!value) {
         return NULL;
      }
      asl_body_skip_ws(cur);

      if (asl_body_peek(cur) != ANVL_TOK_STMT_TERMINATOR) {
         *out_err = ANVL_ERR_PARSER_UNTERMINATED_STATEMENT;
         return NULL;
      }
      asl_body_consume(cur, 1);

      struct asl_ast_node *node = asl_body_alloc_node(arena, ASL_STMT_RETURN);
      if (!node) {
         *out_err = ANVL_ERR_MEMORY_ALLOC_FAILED;
         return NULL;
      }
      node->return_stmt.value = value;
      return node;
   }

   anvl_slice name;
   if (asl_body_scan_identifier(cur, &name)) {
      asl_body_skip_ws(cur);
      char c = asl_body_peek(cur);
      char c2 = asl_body_peek_offset(cur, 1);

      if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '%') && c2 == '=') {
         const char *op_start = cur->pos;
         asl_body_consume(cur, 2); // e.g. "+="
         asl_body_skip_ws(cur);

         struct asl_ast_node *rhs = asl_body_parse_expr(cur, arena, out_err);
         if (!rhs) {
            return NULL;
         }
         asl_body_skip_ws(cur);
         if (asl_body_peek(cur) != ANVL_TOK_STMT_TERMINATOR) {
            *out_err = ANVL_ERR_PARSER_UNTERMINATED_STATEMENT;
            return NULL;
         }
         asl_body_consume(cur, 1);

         return asl_body_build_compound_assign(cur, arena, out_err, name, op_start, rhs);
      }

      if ((c == '+' && c2 == '+') || (c == '-' && c2 == '-')) {
         const char *op_start = cur->pos;
         asl_body_consume(cur, 2); // "++" or "--"
         asl_body_skip_ws(cur);
         if (asl_body_peek(cur) != ANVL_TOK_STMT_TERMINATOR) {
            *out_err = ANVL_ERR_PARSER_UNTERMINATED_STATEMENT;
            return NULL;
         }
         asl_body_consume(cur, 1);

         struct asl_ast_node *one = asl_body_make_one_literal(arena, out_err);
         if (!one) {
            return NULL;
         }
         return asl_body_build_compound_assign(cur, arena, out_err, name, op_start, one);
      }

      return asl_body_parse_assign_tail(cur, arena, out_err, name, ASL_STMT_ASSIGN);
   }

   *out_err = ANVL_ERR_PARSER_UNEXPECTED_TOKEN;
   return NULL;
}

asl_ast_node asl_parse_body(anvl_slice body, bump_allocator arena, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!arena) {
      if (out_err_code) {
         *out_err_code = ANVL_ERR_INVALID_ARGUMENT;
      }
      return NULL;
   }

   asl_cursor_t cur = {.data = body.data, .pos = body.start, .end = body.end};

   // Scratch, heap-backed while the statement count is unknown; copied into a fixed-size
   // arena array once parsing finishes -- a bump allocator can't grow an allocation in
   // place, so an unknown-count array always needs this two-pass shape.
   list scratch = List.new(4, sizeof(void *));
   if (!scratch) {
      if (out_err_code) {
         *out_err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      }
      return NULL;
   }

   asl_body_skip_ws(&cur);
   while (!asl_body_is_eof(&cur)) {
      struct asl_ast_node *stmt = asl_body_parse_stmt(&cur, arena, &err_code);
      if (!stmt) {
         List.dispose(scratch);
         if (out_err_code) {
            *out_err_code = err_code;
         }
         return NULL;
      }
      List.append(scratch, stmt);
      asl_body_skip_ws(&cur);
   }

   usize count = List.size(scratch);
   struct asl_ast_node **stmts = NULL;
   if (count > 0) {
      stmts = arena->alloc(arena, count * sizeof(struct asl_ast_node *));
      if (!stmts) {
         List.dispose(scratch);
         if (out_err_code) {
            *out_err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
         }
         return NULL;
      }
      for (usize i = 0; i < count; i++) {
         struct asl_ast_node *stmt = NULL;
         List.get(scratch, i, (object *)&stmt);
         stmts[i] = stmt;
      }
   }
   List.dispose(scratch);

   struct asl_ast_node *block = arena->alloc(arena, sizeof(struct asl_ast_node));
   if (!block) {
      if (out_err_code) {
         *out_err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      }
      return NULL;
   }
   block->kind = ASL_STMT_BLOCK;
   block->span = body;
   block->block.stmts = stmts;
   block->block.count = count;

   return block;
}
