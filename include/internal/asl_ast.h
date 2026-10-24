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
 * asl_ast.h - AnvlScript body AST node type (WITH_ASL)                   *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: include/internal/asl_ast.h                                       *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * The tree produced by parsing a function body's own AnvlScript grammar  *
 * (src/asl/asl_body_parser.c) -- never built by src/core/asl_parser.c,   *
 * which only ever captures the body as a raw, unparsed slice. See        *
 * notes/anvilscript-design.md's "Runtime AST Nodes" section -- this is a *
 * deliberately trimmed-down first slice of that sketch (5 kinds, enough  *
 * for 'var x = 42; return x;'), grown as real grammar is added, not the  *
 * full ~20-kind enum from day one.                                       *
 *                                                                        *
 * `struct asl_ast_node` is forward-declared (as an opaque pointer target)*
 * in include/internal/module.h's anvl_value_t.function.ast field, which  *
 * is why the struct tag here must be exactly `asl_ast_node`, matching    *
 * that forward reference -- the `asl_ast_node_t`/`asl_ast_node` (plain   *
 * struct / pointer) typedefs below are this file's own naming on top,   *
 * in the ordinary identifier namespace, which doesn't collide with the   *
 * tag namespace the struct keyword itself uses.                         *
 * ********************************************************************** */
#pragma once

#include "internal/module.h"

#if WITH_ASL

typedef enum {
   ASL_STMT_BLOCK,
   ASL_STMT_VAR_DECL,
   ASL_STMT_ASSIGN,
   ASL_STMT_RETURN,
   ASL_EXPR_LITERAL,
   ASL_EXPR_IDENTIFIER,
   ASL_EXPR_BINARY,
   ASL_EXPR_UNARY,
} asl_ast_kind;

struct asl_ast_node {
   asl_ast_kind kind;
   anvl_slice span; // full source span of this node, within the function's own body slice

   union {
      // ASL_STMT_BLOCK: a statement list. Fixed-size, arena-allocated array -- a bump
      // allocator can't grow an allocation in place, so the parser collects statements into
      // a scratch heap list first and copies into this array once the count is known.
      struct {
         struct asl_ast_node **stmts;
         usize count;
      } block;

      // ASL_STMT_VAR_DECL ('var name = value;') and ASL_STMT_ASSIGN ('name = value;', no
      // 'var') share this same shape -- only `kind` tells them apart. Declare-before-assign
      // (notes/anvilscript-design.md "Variable scope and mutability") is a name-resolution
      // concern, not represented here: the parser accepts 'name = value;' regardless of
      // whether 'name' was ever declared.
      struct {
         anvl_slice name;
         struct asl_ast_node *value;
      } assign;

      // ASL_STMT_RETURN ('return value;').
      struct {
         struct asl_ast_node *value;
      } return_stmt;

      // ASL_EXPR_LITERAL. `value` is a plain anvl_value (numeric only, this slice) --
      // arena-allocated directly by the body parser, not via Source.new_node (there's no
      // live anvl_source for a function body parsed independently of the original
      // document parse), and never registered in any ctx->values-style flat index -- it
      // needs no individual disposal, the whole tree is reclaimed when the arena is.
      struct {
         anvl_value value;
      } literal;

      // ASL_EXPR_IDENTIFIER. Name resolution (local variable, parameter, or outer scope) is
      // the evaluator's concern, not the parser's -- this node only ever carries the name.
      struct {
         anvl_slice name;
      } identifier;

      // ASL_EXPR_BINARY ('left op right'). `op` is the raw operator span ("+" or "-", this
      // slice) rather than a separate operator-kind enum -- one less enum to keep in sync
      // with the scanner for now; worth revisiting once the operator set grows past
      // arithmetic (comparison, logic) and a switch on raw text starts to feel wrong.
      struct {
         anvl_slice op;
         struct asl_ast_node *left;
         struct asl_ast_node *right;
      } binary;

      // ASL_EXPR_UNARY ('-x', '!x', '+x'). `op` is the raw operator span, same convention as
      // ASL_EXPR_BINARY. Prefix, right-recursive on itself (not a precedence tier of its own
      // the way binary operators are) -- '!-x' parses as !(-(x)), '--x' as -(-(x)); there is
      // no separate decrement operator at the expression level, only the statement-level
      // '++'/'--' forms asl_body_parse_stmt already desugars.
      struct {
         anvl_slice op;
         struct asl_ast_node *operand;
      } unary;
   };
};
typedef struct asl_ast_node asl_ast_node_t;
typedef asl_ast_node_t *asl_ast_node;

/**
 * @brief Parses a function body's own AnvlScript grammar -- everything
 * src/core/asl_parser.c deliberately left as an opaque slice. A pure
 * function: given the body span and an arena to allocate from, it has no
 * dependency on the original document parse having a live anvl_source
 * still around (lazily-compiled functions are parsed long after that
 * parse finished) -- the caller decides when this runs, this function
 * only decides how.
 *
 * This slice's grammar: `var identifier = expr;` and `return expr;`,
 * where `expr` is a bare identifier or an integer literal -- nothing
 * else yet (no operators, no other literal kinds, no control flow). See
 * notes/anvilscript-design.md's full EBNF for where this is headed.
 *
 * @param body The function's body span (src/core/asl_parser.c's own
 * `function.body`), not including the delimiting '{'/'}'.
 * @param arena The context's bump arena (every node in the returned tree
 * is allocated from it; nothing here is heap-allocated or individually
 * disposed).
 * @param out_err_code Set to the failure's error code, or ANVL_ERR_NONE
 * on success.
 * @return The root ASL_STMT_BLOCK node, or NULL on failure.
 */
asl_ast_node asl_parse_body(anvl_slice body, bump_allocator arena, anvl_err_code *out_err_code);

#endif
