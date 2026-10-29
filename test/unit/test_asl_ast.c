/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_asl_ast.c - Unit tests for the AnvlScript body/AST parser         *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_asl_ast.c                                        *
 * ---------------------------------------------------------------------- *
 * First slice only: 'var identifier = integer_literal;' and              *
 * 'return expr;' where expr is a bare identifier or integer literal --  *
 * see include/internal/asl_ast.h. Built with WITH_ASL=1 (see Makefile). *
 * ********************************************************************** */

#include "anvil.h"
#include "types.h"
#include "internal/asl_ast.h"
#include "internal/constants.h"
#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "testbit.h"
#include "std.h"
// ----------------
#include "../utilities/debug.h"
#include "../utilities/helpers.h"
#include <sigma/list.h>
#include <sigma/types.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
}

/* Loads a fixture through header-scan + include-loading, ready for doc_parse_body. */
static module_document setup_asl_doc(const char *fixture_name, module_context *out_ctx) {
   module_document doc = setup_registered_file(fixture_name, out_ctx);
   if (!doc) {
      return NULL;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   if (ANVL_RES_OK != doc_scan_header(doc, &err_code)) {
      return doc;
   }
   usize size_hint = 0;
   (void)mod_load_includes(*out_ctx, doc, &size_hint, &err_code);
   usize capacity = mod_ctx_arena_size_hint(size_hint);
   mod_ctx_create_arena(*out_ctx, capacity, &err_code);
   return doc;
}

/* ---------------------------------------------------------------------- *
 * ASLAST01 — 'var x = 42; return x;' parses into a 2-statement block:
 * ASL_STMT_VAR_DECL binding 'x' to a NUMERIC literal, then ASL_STMT_RETURN
 * referencing 'x' by name.
 * ---------------------------------------------------------------------- */
static void test_aslast01_var_and_return(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_var_return.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST01: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST01: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST01: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST01: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST01: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }

   TestBit.is_equal_int(ASL_STMT_BLOCK, (long long)root->kind, "ASLAST01: root is ASL_STMT_BLOCK");
   TestBit.is_equal_int(2, (long long)root->block.count, "ASLAST01: two statements in the block");

   struct asl_ast_node *var_decl = root->block.stmts[0];
   TestBit.is_not_null(var_decl, "ASLAST01: first statement retrieved");
   if (var_decl) {
      TestBit.is_equal_int(ASL_STMT_VAR_DECL, (long long)var_decl->kind,
                           "ASLAST01: first statement is ASL_STMT_VAR_DECL");
      TestBit.is_true(Source.slice_equals(var_decl->assign.name, "x"),
                      "ASLAST01: var_decl name is 'x'");
      TestBit.is_not_null(var_decl->assign.value, "ASLAST01: var_decl has a value expression");
      if (var_decl->assign.value) {
         TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)var_decl->assign.value->kind,
                              "ASLAST01: var_decl value is ASL_EXPR_LITERAL");
         anvl_value literal = var_decl->assign.value->literal.value;
         TestBit.is_not_null(literal, "ASLAST01: literal value allocated");
         if (literal) {
            TestBit.is_equal_int(ANVL_VALUE_NUMERIC, (long long)literal->type,
                                 "ASLAST01: literal type is NUMERIC");
            TestBit.is_true(Source.slice_equals(literal->text, "42"),
                            "ASLAST01: literal text is '42'");
         }
      }
   }

   struct asl_ast_node *return_stmt = root->block.stmts[1];
   TestBit.is_not_null(return_stmt, "ASLAST01: second statement retrieved");
   if (return_stmt) {
      TestBit.is_equal_int(ASL_STMT_RETURN, (long long)return_stmt->kind,
                           "ASLAST01: second statement is ASL_STMT_RETURN");
      TestBit.is_not_null(return_stmt->return_stmt.value,
                          "ASLAST01: return_stmt has a value expression");
      if (return_stmt->return_stmt.value) {
         TestBit.is_equal_int(ASL_EXPR_IDENTIFIER, (long long)return_stmt->return_stmt.value->kind,
                              "ASLAST01: return_stmt value is ASL_EXPR_IDENTIFIER");
         TestBit.is_true(Source.slice_equals(return_stmt->return_stmt.value->identifier.name, "x"),
                         "ASLAST01: return_stmt references 'x'");
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST02 — 'var x = 1 + 2 - 3; return x;' — binary '+'/'-' chain
 * left-associatively: (1 + 2) - 3, not 1 + (2 - 3).
 * ---------------------------------------------------------------------- */
static void test_aslast02_additive_left_associative(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_additive.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST02: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST02: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST02: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST02: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST02: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }

   struct asl_ast_node *var_decl = root->block.stmts[0];
   TestBit.is_not_null(var_decl, "ASLAST02: var_decl retrieved");
   if (!var_decl) {
      mod_ctx_dispose(ctx);
      return;
   }

   struct asl_ast_node *outer = var_decl->assign.value;
   TestBit.is_not_null(outer, "ASLAST02: outer expression retrieved");
   if (outer) {
      TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)outer->kind,
                           "ASLAST02: outer expression is ASL_EXPR_BINARY");
      TestBit.is_true(Source.slice_equals(outer->binary.op, "-"), "ASLAST02: outer op is '-'");

      struct asl_ast_node *inner = outer->binary.left;
      TestBit.is_not_null(inner, "ASLAST02: outer.left retrieved");
      if (inner) {
         TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)inner->kind,
                              "ASLAST02: outer.left is itself ASL_EXPR_BINARY ('1 + 2')");
         TestBit.is_true(Source.slice_equals(inner->binary.op, "+"), "ASLAST02: inner op is '+'");
         TestBit.is_not_null(inner->binary.left, "ASLAST02: inner.left retrieved");
         TestBit.is_not_null(inner->binary.right, "ASLAST02: inner.right retrieved");
         if (inner->binary.left && inner->binary.right) {
            TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)inner->binary.left->kind,
                                 "ASLAST02: inner.left is a literal");
            TestBit.is_true(Source.slice_equals(inner->binary.left->literal.value->text, "1"),
                            "ASLAST02: inner.left is '1'");
            TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)inner->binary.right->kind,
                                 "ASLAST02: inner.right is a literal");
            TestBit.is_true(Source.slice_equals(inner->binary.right->literal.value->text, "2"),
                            "ASLAST02: inner.right is '2'");
         }
      }

      struct asl_ast_node *right = outer->binary.right;
      TestBit.is_not_null(right, "ASLAST02: outer.right retrieved");
      if (right) {
         TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)right->kind,
                              "ASLAST02: outer.right is a literal");
         TestBit.is_true(Source.slice_equals(right->literal.value->text, "3"),
                         "ASLAST02: outer.right is '3'");
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST03 — 'var x = 2 + 3 * 4; return x;' — '*' binds tighter than
 * '+': this must parse as 2 + (3 * 4), not (2 + 3) * 4.
 * ---------------------------------------------------------------------- */
static void test_aslast03_multiplicative_precedence(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_multiplicative.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST03: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST03: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST03: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST03: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST03: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }

   struct asl_ast_node *var_decl = root->block.stmts[0];
   TestBit.is_not_null(var_decl, "ASLAST03: var_decl retrieved");
   if (!var_decl) {
      mod_ctx_dispose(ctx);
      return;
   }

   struct asl_ast_node *outer = var_decl->assign.value;
   TestBit.is_not_null(outer, "ASLAST03: outer expression retrieved");
   if (outer) {
      TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)outer->kind,
                           "ASLAST03: outer expression is ASL_EXPR_BINARY");
      TestBit.is_true(Source.slice_equals(outer->binary.op, "+"), "ASLAST03: outer op is '+'");

      struct asl_ast_node *left = outer->binary.left;
      TestBit.is_not_null(left, "ASLAST03: outer.left retrieved");
      if (left) {
         TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)left->kind,
                              "ASLAST03: outer.left is a literal (not nested further)");
         TestBit.is_true(Source.slice_equals(left->literal.value->text, "2"),
                         "ASLAST03: outer.left is '2'");
      }

      struct asl_ast_node *right = outer->binary.right;
      TestBit.is_not_null(right, "ASLAST03: outer.right retrieved");
      if (right) {
         TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)right->kind,
                              "ASLAST03: outer.right is itself ASL_EXPR_BINARY ('3 * 4')");
         TestBit.is_true(Source.slice_equals(right->binary.op, "*"), "ASLAST03: inner op is '*'");
         TestBit.is_not_null(right->binary.left, "ASLAST03: inner.left retrieved");
         TestBit.is_not_null(right->binary.right, "ASLAST03: inner.right retrieved");
         if (right->binary.left && right->binary.right) {
            TestBit.is_true(Source.slice_equals(right->binary.left->literal.value->text, "3"),
                            "ASLAST03: inner.left is '3'");
            TestBit.is_true(Source.slice_equals(right->binary.right->literal.value->text, "4"),
                            "ASLAST03: inner.right is '4'");
         }
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST04 — 'var x = 1; x = 2; return x;' — plain reassignment (no
 * 'var') is a distinct ASL_STMT_ASSIGN, same shape as ASL_STMT_VAR_DECL.
 * ---------------------------------------------------------------------- */
static void test_aslast04_plain_assignment(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_assign.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST04: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST04: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST04: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST04: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST04: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }

   TestBit.is_equal_int(3, (long long)root->block.count, "ASLAST04: three statements");

   struct asl_ast_node *assign = root->block.stmts[1];
   TestBit.is_not_null(assign, "ASLAST04: second statement retrieved");
   if (assign) {
      TestBit.is_equal_int(ASL_STMT_ASSIGN, (long long)assign->kind,
                           "ASLAST04: second statement is ASL_STMT_ASSIGN");
      TestBit.is_true(Source.slice_equals(assign->assign.name, "x"), "ASLAST04: assigns to 'x'");
      TestBit.is_not_null(assign->assign.value, "ASLAST04: assign has a value expression");
      if (assign->assign.value) {
         TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)assign->assign.value->kind,
                              "ASLAST04: assign value is ASL_EXPR_LITERAL");
         TestBit.is_true(Source.slice_equals(assign->assign.value->literal.value->text, "2"),
                         "ASLAST04: assign value is '2'");
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST05 — 'x += 3; x++;' both desugar to ASL_STMT_ASSIGN wrapping a
 * synthesized ASL_EXPR_BINARY ('x = x + 3;' / 'x = x + 1;') -- no new
 * node kinds, per notes/anvilscript-design.md's roadmap item #1.
 * ---------------------------------------------------------------------- */
static void test_aslast05_compound_assign_and_increment(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_compound.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST05: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST05: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST05: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST05: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST05: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }

   TestBit.is_equal_int(4, (long long)root->block.count, "ASLAST05: four statements");

   // x += 3;
   struct asl_ast_node *compound = root->block.stmts[1];
   TestBit.is_not_null(compound, "ASLAST05: 'x += 3' statement retrieved");
   if (compound) {
      TestBit.is_equal_int(ASL_STMT_ASSIGN, (long long)compound->kind,
                           "ASLAST05: 'x += 3' is ASL_STMT_ASSIGN");
      TestBit.is_true(Source.slice_equals(compound->assign.name, "x"),
                      "ASLAST05: 'x += 3' assigns to 'x'");
      struct asl_ast_node *bin = compound->assign.value;
      TestBit.is_not_null(bin, "ASLAST05: 'x += 3' has a value expression");
      if (bin) {
         TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)bin->kind,
                              "ASLAST05: 'x += 3' value is ASL_EXPR_BINARY");
         TestBit.is_true(Source.slice_equals(bin->binary.op, "+"), "ASLAST05: desugared op is '+'");
         TestBit.is_not_null(bin->binary.left, "ASLAST05: desugared left retrieved");
         TestBit.is_not_null(bin->binary.right, "ASLAST05: desugared right retrieved");
         if (bin->binary.left && bin->binary.right) {
            TestBit.is_equal_int(ASL_EXPR_IDENTIFIER, (long long)bin->binary.left->kind,
                                 "ASLAST05: desugared left is ASL_EXPR_IDENTIFIER");
            TestBit.is_true(Source.slice_equals(bin->binary.left->identifier.name, "x"),
                            "ASLAST05: desugared left is 'x'");
            TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)bin->binary.right->kind,
                                 "ASLAST05: desugared right is ASL_EXPR_LITERAL");
            TestBit.is_true(Source.slice_equals(bin->binary.right->literal.value->text, "3"),
                            "ASLAST05: desugared right is '3'");
         }
      }
   }

   // x++;
   struct asl_ast_node *inc = root->block.stmts[2];
   TestBit.is_not_null(inc, "ASLAST05: 'x++' statement retrieved");
   if (inc) {
      TestBit.is_equal_int(ASL_STMT_ASSIGN, (long long)inc->kind,
                           "ASLAST05: 'x++' is ASL_STMT_ASSIGN");
      TestBit.is_true(Source.slice_equals(inc->assign.name, "x"), "ASLAST05: 'x++' assigns to 'x'");
      struct asl_ast_node *bin = inc->assign.value;
      TestBit.is_not_null(bin, "ASLAST05: 'x++' has a value expression");
      if (bin) {
         TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)bin->kind,
                              "ASLAST05: 'x++' value is ASL_EXPR_BINARY");
         TestBit.is_true(Source.slice_equals(bin->binary.op, "+"),
                         "ASLAST05: 'x++' desugared op is '+'");
         TestBit.is_not_null(bin->binary.right, "ASLAST05: 'x++' desugared right retrieved");
         if (bin->binary.right) {
            TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)bin->binary.right->kind,
                                 "ASLAST05: 'x++' desugared right is ASL_EXPR_LITERAL");
            TestBit.is_true(Source.slice_equals(bin->binary.right->literal.value->text, "1"),
                            "ASLAST05: 'x++' desugared right is '1'");
         }
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST06 — comparison/equality/logical precedence, all in one pass:
 *   'x = 1 < 2 == 0;'     -> (1 < 2) == 0            (comparison > equality)
 *   'y = 3 <= 4;'         -> '<=' is one operator, not '<' + dangling '='
 *   'z = 1 == 1 && 2 == 2;' -> (1 == 1) && (2 == 2)  (equality > logical-and)
 * ---------------------------------------------------------------------- */
static void test_aslast06_comparison_equality_logical_precedence(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_comparison.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST06: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST06: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST06: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST06: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST06: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }
   TestBit.is_equal_int(4, (long long)root->block.count, "ASLAST06: four statements");

   // x = 1 < 2 == 0  -->  (1 < 2) == 0
   struct asl_ast_node *x = root->block.stmts[0]->assign.value;
   TestBit.is_not_null(x, "ASLAST06: x's value retrieved");
   if (x) {
      TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)x->kind, "ASLAST06: x is ASL_EXPR_BINARY");
      TestBit.is_true(Source.slice_equals(x->binary.op, "=="), "ASLAST06: x's outer op is '=='");
      struct asl_ast_node *lt = x->binary.left;
      TestBit.is_not_null(lt, "ASLAST06: x's left retrieved");
      if (lt) {
         TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)lt->kind,
                              "ASLAST06: x's left is itself ASL_EXPR_BINARY ('1 < 2')");
         TestBit.is_true(Source.slice_equals(lt->binary.op, "<"), "ASLAST06: inner op is '<'");
      }
   }

   // y = 3 <= 4  -->  op must be '<=', not '<'
   struct asl_ast_node *y = root->block.stmts[1]->assign.value;
   TestBit.is_not_null(y, "ASLAST06: y's value retrieved");
   if (y) {
      TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)y->kind, "ASLAST06: y is ASL_EXPR_BINARY");
      TestBit.is_true(Source.slice_equals(y->binary.op, "<="), "ASLAST06: y's op is '<=', not '<'");
   }

   // z = 1 == 1 && 2 == 2  -->  (1 == 1) && (2 == 2)
   struct asl_ast_node *z = root->block.stmts[2]->assign.value;
   TestBit.is_not_null(z, "ASLAST06: z's value retrieved");
   if (z) {
      TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)z->kind, "ASLAST06: z is ASL_EXPR_BINARY");
      TestBit.is_true(Source.slice_equals(z->binary.op, "&&"), "ASLAST06: z's outer op is '&&'");
      TestBit.is_not_null(z->binary.left, "ASLAST06: z's left retrieved");
      TestBit.is_not_null(z->binary.right, "ASLAST06: z's right retrieved");
      if (z->binary.left && z->binary.right) {
         TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)z->binary.left->kind,
                              "ASLAST06: z's left is ASL_EXPR_BINARY ('1 == 1')");
         TestBit.is_true(Source.slice_equals(z->binary.left->binary.op, "=="),
                         "ASLAST06: z's left op is '=='");
         TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)z->binary.right->kind,
                              "ASLAST06: z's right is ASL_EXPR_BINARY ('2 == 2')");
         TestBit.is_true(Source.slice_equals(z->binary.right->binary.op, "=="),
                         "ASLAST06: z's right op is '=='");
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST07 — 'var x = -5; var y = !1; var z = +3;' — unary prefix
 * operators each produce an ASL_EXPR_UNARY wrapping the operand literal.
 * ---------------------------------------------------------------------- */
static void test_aslast07_unary_operators(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_unary.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST07: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST07: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST07: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST07: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST07: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }
   TestBit.is_equal_int(4, (long long)root->block.count, "ASLAST07: four statements");

   static const char *const names[] = {"x", "y", "z"};
   static const char *const ops[] = {"-", "!", "+"};
   static const char *const operands[] = {"5", "1", "3"};
   for (int i = 0; i < 3; i++) {
      struct asl_ast_node *value = root->block.stmts[i]->assign.value;
      TestBit.is_not_null(value, "ASLAST07: value retrieved");
      if (!value) {
         continue;
      }
      TestBit.is_equal_int(ASL_EXPR_UNARY, (long long)value->kind,
                           "ASLAST07: value is ASL_EXPR_UNARY");
      TestBit.is_true(Source.slice_equals(value->unary.op, ops[i]), "ASLAST07: op matches");
      TestBit.is_not_null(value->unary.operand, "ASLAST07: operand retrieved");
      if (value->unary.operand) {
         TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)value->unary.operand->kind,
                              "ASLAST07: operand is ASL_EXPR_LITERAL");
         TestBit.is_true(Source.slice_equals(value->unary.operand->literal.value->text, operands[i]),
                         "ASLAST07: operand text matches");
      }
      (void)names;
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST08 — 'var x = -2 * 3;' — unary binds tighter than multiplicative:
 * (-2) * 3, not -(2 * 3). 'var y = --5;' — chained unary: -(-(5)).
 * ---------------------------------------------------------------------- */
static void test_aslast08_unary_precedence_and_chaining(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_unary_precedence.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST08: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST08: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST08: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST08: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST08: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }

   // x = -2 * 3  -->  (-2) * 3
   struct asl_ast_node *x = root->block.stmts[0]->assign.value;
   TestBit.is_not_null(x, "ASLAST08: x's value retrieved");
   if (x) {
      TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)x->kind, "ASLAST08: x is ASL_EXPR_BINARY");
      TestBit.is_true(Source.slice_equals(x->binary.op, "*"), "ASLAST08: x's op is '*'");
      struct asl_ast_node *left = x->binary.left;
      TestBit.is_not_null(left, "ASLAST08: x's left retrieved");
      if (left) {
         TestBit.is_equal_int(ASL_EXPR_UNARY, (long long)left->kind,
                              "ASLAST08: x's left is ASL_EXPR_UNARY, not folded into '*'");
         TestBit.is_true(Source.slice_equals(left->unary.op, "-"), "ASLAST08: x's left op is '-'");
         TestBit.is_not_null(left->unary.operand, "ASLAST08: x's left operand retrieved");
         if (left->unary.operand) {
            TestBit.is_true(Source.slice_equals(left->unary.operand->literal.value->text, "2"),
                            "ASLAST08: x's left operand is '2'");
         }
      }
      struct asl_ast_node *right = x->binary.right;
      TestBit.is_not_null(right, "ASLAST08: x's right retrieved");
      if (right) {
         TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)right->kind,
                              "ASLAST08: x's right is a plain literal");
         TestBit.is_true(Source.slice_equals(right->literal.value->text, "3"),
                         "ASLAST08: x's right is '3'");
      }
   }

   // y = --5  -->  -(-(5))
   struct asl_ast_node *y = root->block.stmts[1]->assign.value;
   TestBit.is_not_null(y, "ASLAST08: y's value retrieved");
   if (y) {
      TestBit.is_equal_int(ASL_EXPR_UNARY, (long long)y->kind, "ASLAST08: y is ASL_EXPR_UNARY");
      TestBit.is_true(Source.slice_equals(y->unary.op, "-"), "ASLAST08: y's outer op is '-'");
      struct asl_ast_node *inner = y->unary.operand;
      TestBit.is_not_null(inner, "ASLAST08: y's inner operand retrieved");
      if (inner) {
         TestBit.is_equal_int(ASL_EXPR_UNARY, (long long)inner->kind,
                              "ASLAST08: y's inner operand is itself ASL_EXPR_UNARY");
         TestBit.is_true(Source.slice_equals(inner->unary.op, "-"), "ASLAST08: y's inner op is '-'");
         TestBit.is_not_null(inner->unary.operand, "ASLAST08: y's innermost operand retrieved");
         if (inner->unary.operand) {
            TestBit.is_true(Source.slice_equals(inner->unary.operand->literal.value->text, "5"),
                            "ASLAST08: y's innermost operand is '5'");
         }
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST09 — 'var x = (1 + 2) * 3;' — grouping overrides precedence:
 * (1 + 2) * 3, not 1 + (2 * 3) (contrast with ASLAST03's un-grouped case).
 * ---------------------------------------------------------------------- */
static void test_aslast09_grouping_overrides_precedence(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_grouping.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST09: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST09: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST09: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST09: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST09: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }

   struct asl_ast_node *x = root->block.stmts[0]->assign.value;
   TestBit.is_not_null(x, "ASLAST09: x's value retrieved");
   if (x) {
      TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)x->kind, "ASLAST09: x is ASL_EXPR_BINARY");
      TestBit.is_true(Source.slice_equals(x->binary.op, "*"), "ASLAST09: x's outer op is '*'");
      struct asl_ast_node *left = x->binary.left;
      TestBit.is_not_null(left, "ASLAST09: x's left retrieved");
      if (left) {
         TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)left->kind,
                              "ASLAST09: x's left is itself ASL_EXPR_BINARY ('1 + 2'), grouping held");
         TestBit.is_true(Source.slice_equals(left->binary.op, "+"), "ASLAST09: x's left op is '+'");
      }
      struct asl_ast_node *right = x->binary.right;
      TestBit.is_not_null(right, "ASLAST09: x's right retrieved");
      if (right) {
         TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)right->kind,
                              "ASLAST09: x's right is a plain literal ('3', ungrouped)");
         TestBit.is_true(Source.slice_equals(right->literal.value->text, "3"),
                         "ASLAST09: x's right is '3'");
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST10 — 'var x = -(1 + 2);' — grouping composes with unary.
 * 'var y = ((3));' — nested grouping unwraps transparently to the bare
 * literal, leaving no trace of the parens in the tree.
 * ---------------------------------------------------------------------- */
static void test_aslast10_grouping_with_unary_and_nesting(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_grouping_nested.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST10: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST10: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST10: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_not_null(root, "ASLAST10: body parses to a non-NULL root");
   TestBit.is_equal_int(ANVL_ERR_NONE, ast_err, "ASLAST10: ast_err is ANVL_ERR_NONE");
   if (!root) {
      mod_ctx_dispose(ctx);
      return;
   }

   // x = -(1 + 2)
   struct asl_ast_node *x = root->block.stmts[0]->assign.value;
   TestBit.is_not_null(x, "ASLAST10: x's value retrieved");
   if (x) {
      TestBit.is_equal_int(ASL_EXPR_UNARY, (long long)x->kind, "ASLAST10: x is ASL_EXPR_UNARY");
      TestBit.is_true(Source.slice_equals(x->unary.op, "-"), "ASLAST10: x's op is '-'");
      struct asl_ast_node *operand = x->unary.operand;
      TestBit.is_not_null(operand, "ASLAST10: x's operand retrieved");
      if (operand) {
         TestBit.is_equal_int(ASL_EXPR_BINARY, (long long)operand->kind,
                              "ASLAST10: x's operand is the grouped ASL_EXPR_BINARY ('1 + 2')");
         TestBit.is_true(Source.slice_equals(operand->binary.op, "+"),
                         "ASLAST10: x's operand op is '+'");
      }
   }

   // y = ((3))  -->  bare literal, parens leave no node of their own
   struct asl_ast_node *y = root->block.stmts[1]->assign.value;
   TestBit.is_not_null(y, "ASLAST10: y's value retrieved");
   if (y) {
      TestBit.is_equal_int(ASL_EXPR_LITERAL, (long long)y->kind,
                           "ASLAST10: y is a bare ASL_EXPR_LITERAL, nested parens unwrapped");
      TestBit.is_true(Source.slice_equals(y->literal.value->text, "3"), "ASLAST10: y is '3'");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASLAST11 — 'var x = (1 + 2;' — a grouped expression missing its
 * closing ')' fails parsing cleanly (NULL root, a real error code), not
 * a crash and not a silently-accepted tree.
 * ---------------------------------------------------------------------- */
static void test_aslast11_unterminated_grouping_fails_cleanly(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_grouping_unterminated.anvl", &ctx);
   TestBit.is_not_null(doc, "ASLAST11: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASLAST11: signature parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASLAST11: 'foo' statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_err_code ast_err = ANVL_ERR_NONE;
   asl_ast_node root = asl_parse_body(stmt->value->function.body, ctx->arena, &ast_err);
   TestBit.is_null(root, "ASLAST11: unterminated group yields a NULL root");
   TestBit.is_true(ast_err != ANVL_ERR_NONE, "ASLAST11: ast_err is a real error code");

   mod_ctx_dispose(ctx);
}

int main(void) {
   TestBit.run_ex("ASLAST01_var_and_return", NULL, test_aslast01_var_and_return, th);
   TestBit.run_ex("ASLAST02_additive_left_associative", NULL,
                  test_aslast02_additive_left_associative, th);
   TestBit.run_ex("ASLAST03_multiplicative_precedence", NULL,
                  test_aslast03_multiplicative_precedence, th);
   TestBit.run_ex("ASLAST04_plain_assignment", NULL, test_aslast04_plain_assignment, th);
   TestBit.run_ex("ASLAST05_compound_assign_and_increment", NULL,
                  test_aslast05_compound_assign_and_increment, th);
   TestBit.run_ex("ASLAST06_comparison_equality_logical_precedence", NULL,
                  test_aslast06_comparison_equality_logical_precedence, th);
   TestBit.run_ex("ASLAST07_unary_operators", NULL, test_aslast07_unary_operators, th);
   TestBit.run_ex("ASLAST08_unary_precedence_and_chaining", NULL,
                  test_aslast08_unary_precedence_and_chaining, th);
   TestBit.run_ex("ASLAST09_grouping_overrides_precedence", NULL,
                  test_aslast09_grouping_overrides_precedence, th);
   TestBit.run_ex("ASLAST10_grouping_with_unary_and_nesting", NULL,
                  test_aslast10_grouping_with_unary_and_nesting, th);
   TestBit.run_ex("ASLAST11_unterminated_grouping_fails_cleanly", NULL,
                  test_aslast11_unterminated_grouping_fails_cleanly, th);

   return TestBit.report();
}
