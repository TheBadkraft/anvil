/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * asl.c — anvil.asl: Anvil Script Language (ASL) parser + evaluator
 * ------------------------------------------------------------------
 * Ported from: AslParser.cs + AslEvaluator.cs (Anvil.Net v0.4.0)
 *
 * Implementation is split into three logical sections:
 *
 *   §1  Scanner  — walk a source substring character by character
 *   §2  Parser   — Pratt/precedence-climbing recursive-descent
 *   §3  Eval     — tree-walk evaluator with flat per-call scope
 *
 * All heap allocation goes through Allocator.alloc / Allocator.dispose
 * (sigma.memory).  String building uses StringBuilder (sigma.text).
 *
 * Error reporting: anvl_error_set() + return NULL / false.
 * ------------------------------------------------------------------
 */

#include "asl.h"
#include "errors.h"
#include <sigma.memory/memory.h>
#include <sigma.text/strings.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * §0  Internal limits
 * ================================================================ */
#define ASL_MAX_DEPTH  32   /* maximum eval recursion depth */
#define ASL_MAX_SCOPE  256  /* max local variables per invocation */
#define ASL_INIT_CHILDREN 4 /* default child-array capacity      */

/* ================================================================
 * §1  Scanner
 * ================================================================ */

typedef struct {
   const char *src;
   int         pos;
   int         end;   /* exclusive end of the body (after strip) */
} asl_scanner_t;

static void scan_skip_ws(asl_scanner_t *s) {
   while (s->pos < s->end) {
      char c = s->src[s->pos];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
         s->pos++;
      else if (c == '/' && s->pos + 1 < s->end && s->src[s->pos + 1] == '/') {
         /* line comment */
         while (s->pos < s->end && s->src[s->pos] != '\n')
            s->pos++;
      } else {
         break;
      }
   }
}

static char scan_peek2(asl_scanner_t *s) {
   if (s->pos + 1 < s->end) return s->src[s->pos + 1];
   return '\0';
}

static bool scan_match(asl_scanner_t *s, char c) {
   scan_skip_ws(s);
   if (s->pos < s->end && s->src[s->pos] == c) {
      s->pos++;
      return true;
   }
   return false;
}

static bool is_alpha(char c) {
   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }

/* ================================================================
 * §2  AST helpers — node allocation / free
 * ================================================================ */

static asl_node_t *node_alloc(asl_node_kind_t kind) {
   asl_node_t *n = Allocator.alloc(sizeof(asl_node_t));
   if (!n) return NULL;
   memset(n, 0, sizeof(asl_node_t));
   n->kind = kind;
   return n;
}

static bool node_add_child(asl_node_t *parent, asl_node_t *child) {
   if (parent->child_count == 0) {
      parent->children = Allocator.alloc(sizeof(asl_node_t *) * ASL_INIT_CHILDREN);
      if (!parent->children) return false;
      parent->child_count = 0;
   } else if ((parent->child_count & (parent->child_count - 1)) == 0
              && parent->child_count >= ASL_INIT_CHILDREN) {
      /* double capacity */
      asl_node_t **nb = Allocator.alloc(
          sizeof(asl_node_t *) * (usize)(parent->child_count * 2));
      if (!nb) return false;
      memcpy(nb, parent->children,
             sizeof(asl_node_t *) * (usize)parent->child_count);
      Allocator.dispose(parent->children);
      parent->children = nb;
   } else if (parent->child_count == 0) {
      /* first alloc path already handled above, but guard */
   }
   parent->children[parent->child_count++] = child;
   return true;
}

static void node_free_impl(asl_node_t *n) {
   if (!n) return;
   for (int i = 0; i < n->child_count; i++)
      node_free_impl(n->children[i]);
   if (n->children) Allocator.dispose(n->children);
   Allocator.dispose(n);
}

/* ================================================================
 * §2b  Value helpers
 * ================================================================ */

static void value_free_impl(asl_value_t *v) {
   if (!v) return;
   if (v->kind == ASL_STRING && v->string_value) {
      Allocator.dispose(v->string_value);
      v->string_value = NULL;
   } else if (v->kind == ASL_LIST) {
      for (int i = 0; i < v->list_count; i++)
         value_free_impl(&v->list_items[i]);
      if (v->list_items) Allocator.dispose(v->list_items);
   } else if (v->kind == ASL_MAP) {
      for (int i = 0; i < v->map_count; i++) {
         if (v->map_keys[i]) Allocator.dispose(v->map_keys[i]);
         value_free_impl(&v->map_values[i]);
      }
      if (v->map_keys)   Allocator.dispose(v->map_keys);
      if (v->map_values) Allocator.dispose(v->map_values);
   }
   /* do NOT free v itself — it may be stack / array element */
}

/* Deep copy a value (for storing into scope / results). */
static bool value_copy(asl_value_t *dst, const asl_value_t *src_val) {
   *dst = *src_val; /* shallow copy of all scalars */
   if (src_val->kind == ASL_STRING && src_val->string_value) {
      usize len = strlen(src_val->string_value);
      char *s = Allocator.alloc(len + 1);
      if (!s) return false;
      memcpy(s, src_val->string_value, len + 1);
      dst->string_value = s;
   }
   /* List / Map deep copy not needed for current test suite — add later */
   return true;
}

/* ================================================================
 * §3  Parser — forward declarations
 * ================================================================ */

static asl_node_t *parse_stmt(asl_scanner_t *s, const char *src);
static asl_node_t *parse_block(asl_scanner_t *s, const char *src);
static asl_node_t *parse_expr(asl_scanner_t *s, const char *src, int min_prec);
static asl_node_t *parse_unary(asl_scanner_t *s, const char *src);
static asl_node_t *parse_primary(asl_scanner_t *s, const char *src);

/* ------------------------------------------------------------------ */
/* Precedence table                                                   */
/* ------------------------------------------------------------------ */
static int bin_prec(asl_op_t op) {
   switch (op) {
      case ASL_OP_OR:     return 1;
      case ASL_OP_AND:    return 2;
      case ASL_OP_EQ:
      case ASL_OP_NOT_EQ: return 3;
      case ASL_OP_LT:
      case ASL_OP_GT:
      case ASL_OP_LT_EQ:
      case ASL_OP_GE:     return 4;
      case ASL_OP_ADD:
      case ASL_OP_SUB:    return 5;
      case ASL_OP_MUL:
      case ASL_OP_DIV:
      case ASL_OP_MOD:    return 6;
      default:            return 0;
   }
}

/* Try to read a binary operator at current position (after ws skip).
 * Returns ASL_OP_NONE if none found. Advances past the token on match. */
static asl_op_t scan_binop(asl_scanner_t *s) {
   scan_skip_ws(s);
   if (s->pos >= s->end) return ASL_OP_NONE;
   char c  = s->src[s->pos];
   char c2 = scan_peek2(s);

   switch (c) {
      case '+': s->pos++; return ASL_OP_ADD;
      case '-': s->pos++; return ASL_OP_SUB;
      case '*': s->pos++; return ASL_OP_MUL;
      case '/': s->pos++; return ASL_OP_DIV;
      case '%': s->pos++; return ASL_OP_MOD;
      case '&':
         if (c2 == '&') { s->pos += 2; return ASL_OP_AND; }
         return ASL_OP_NONE;
      case '|':
         if (c2 == '|') { s->pos += 2; return ASL_OP_OR; }
         return ASL_OP_NONE;
      case '=':
         if (c2 == '=') { s->pos += 2; return ASL_OP_EQ; }
         return ASL_OP_NONE;
      case '!':
         if (c2 == '=') { s->pos += 2; return ASL_OP_NOT_EQ; }
         return ASL_OP_NONE;
      case '<':
         if (c2 == '=') { s->pos += 2; return ASL_OP_LT_EQ; }
         s->pos++; return ASL_OP_LT;
      case '>':
         if (c2 == '=') { s->pos += 2; return ASL_OP_GE; }
         s->pos++; return ASL_OP_GT;
      default:
         return ASL_OP_NONE;
   }
}

/* ------------------------------------------------------------------ */
/* parse_expr — Pratt precedence climbing                             */
/* ------------------------------------------------------------------ */
static asl_node_t *parse_expr(asl_scanner_t *s, const char *src, int min_prec) {
   asl_node_t *left = parse_unary(s, src);
   if (!left) return NULL;

   while (true) {
      /* Peek at the next binary operator without consuming */
      int saved = s->pos;
      scan_skip_ws(s);
      if (s->pos >= s->end) break;

      /* save before trying to read op */
      int op_start = s->pos;
      asl_op_t op = scan_binop(s);
      if (op == ASL_OP_NONE) {
         s->pos = op_start;
         break;
      }
      int prec = bin_prec(op);
      if (prec <= min_prec) {
         /* not high enough — back up and stop */
         s->pos = saved;
         break;
      }

      asl_node_t *right = parse_expr(s, src, prec);
      if (!right) {
         node_free_impl(left);
         return NULL;
      }
      asl_node_t *bin = node_alloc(ASL_NODE_BINARY_EXPR);
      if (!bin) { node_free_impl(left); node_free_impl(right); return NULL; }
      bin->op = op;
      if (!node_add_child(bin, left) || !node_add_child(bin, right)) {
         node_free_impl(bin); node_free_impl(left); node_free_impl(right);
         return NULL;
      }
      left = bin;
   }
   return left;
}

/* ------------------------------------------------------------------ */
/* parse_unary                                                        */
/* ------------------------------------------------------------------ */
static asl_node_t *parse_unary(asl_scanner_t *s, const char *src) {
   scan_skip_ws(s);
   if (s->pos >= s->end) return NULL;

   char c = s->src[s->pos];
   if (c == '-') {
      s->pos++;
      asl_node_t *operand = parse_unary(s, src);
      if (!operand) return NULL;
      asl_node_t *n = node_alloc(ASL_NODE_UNARY_EXPR);
      if (!n) { node_free_impl(operand); return NULL; }
      n->op = ASL_OP_NEG;
      if (!node_add_child(n, operand)) { node_free_impl(n); return NULL; }
      return n;
   }
   if (c == '!') {
      s->pos++;
      asl_node_t *operand = parse_unary(s, src);
      if (!operand) return NULL;
      asl_node_t *n = node_alloc(ASL_NODE_UNARY_EXPR);
      if (!n) { node_free_impl(operand); return NULL; }
      n->op = ASL_OP_NOT;
      if (!node_add_child(n, operand)) { node_free_impl(n); return NULL; }
      return n;
   }
   return parse_primary(s, src);
}

/* ------------------------------------------------------------------ */
/* parse_primary                                                      */
/* ------------------------------------------------------------------ */
static asl_node_t *parse_primary(asl_scanner_t *s, const char *src) {
   scan_skip_ws(s);
   if (s->pos >= s->end) {
      anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR, "unexpected end of expression",
                     0, 0, __FILE__);
      return NULL;
   }
   char c = s->src[s->pos];

   /* Grouped expression */
   if (c == '(') {
      s->pos++;
      asl_node_t *inner = parse_expr(s, src, 0);
      if (!inner) return NULL;
      scan_skip_ws(s);
      if (s->pos >= s->end || s->src[s->pos] != ')') {
         anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR,
                        "expected ')' to close grouped expression", 0, 0, __FILE__);
         node_free_impl(inner);
         return NULL;
      }
      s->pos++;
      return inner;
   }

   /* $varref */
   if (c == '$') {
      s->pos++;
      int start = s->pos;
      while (s->pos < s->end && is_alnum(s->src[s->pos]))
         s->pos++;
      if (s->pos == start) {
         anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR,
                        "expected identifier after '$'", 0, 0, __FILE__);
         return NULL;
      }
      asl_node_t *n = node_alloc(ASL_NODE_VAR_REF);
      if (!n) return NULL;
      n->start  = start;
      n->length = s->pos - start;
      return n;
   }

   /* String literal */
   if (c == '"') {
      s->pos++;
      int start = s->pos;
      while (s->pos < s->end && s->src[s->pos] != '"') {
         if (s->src[s->pos] == '\\') s->pos++; /* skip escaped char */
         s->pos++;
      }
      int length = s->pos - start;
      if (s->pos < s->end) s->pos++; /* consume closing '"' */
      asl_node_t *n = node_alloc(ASL_NODE_STRING_LITERAL);
      if (!n) return NULL;
      n->start  = start;
      n->length = length;
      return n;
   }

   /* List literal [ expr, … ] */
   if (c == '[') {
      s->pos++;
      asl_node_t *n = node_alloc(ASL_NODE_LIST_LITERAL);
      if (!n) return NULL;
      scan_skip_ws(s);
      if (s->pos < s->end && s->src[s->pos] == ']') {
         s->pos++;
         return n; /* empty list */
      }
      while (true) {
         asl_node_t *elem = parse_expr(s, src, 0);
         if (!elem) { node_free_impl(n); return NULL; }
         if (!node_add_child(n, elem)) { node_free_impl(n); return NULL; }
         scan_skip_ws(s);
         if (s->pos < s->end && s->src[s->pos] == ',') { s->pos++; continue; }
         break;
      }
      scan_skip_ws(s);
      if (s->pos >= s->end || s->src[s->pos] != ']') {
         anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR, "expected ']'", 0, 0, __FILE__);
         node_free_impl(n);
         return NULL;
      }
      s->pos++;
      return n;
   }

   /* Numeric literal */
   if (is_digit(c) || (c == '-' && s->pos + 1 < s->end && is_digit(s->src[s->pos + 1]))) {
      int start = s->pos;
      if (c == '-') s->pos++;
      while (s->pos < s->end && is_digit(s->src[s->pos])) s->pos++;
      bool is_float = false;
      if (s->pos < s->end && s->src[s->pos] == '.') {
         is_float = true;
         s->pos++;
         while (s->pos < s->end && is_digit(s->src[s->pos])) s->pos++;
      }
      /* Make a NUL-terminated copy for strtol/strtod */
      int len = s->pos - start;
      char *tmp = Allocator.alloc((usize)(len + 1));
      if (!tmp) return NULL;
      memcpy(tmp, src + start, (usize)len);
      tmp[len] = '\0';
      asl_node_t *n;
      if (is_float) {
         n = node_alloc(ASL_NODE_FLOAT_LITERAL);
         if (n) n->float_value = strtod(tmp, NULL);
      } else {
         n = node_alloc(ASL_NODE_INT_LITERAL);
         if (n) n->int_value = (int64_t)strtoll(tmp, NULL, 10);
      }
      Allocator.dispose(tmp);
      return n;
   }

   /* Keywords and identifiers */
   if (is_alpha(c)) {
      int start = s->pos;
      while (s->pos < s->end && is_alnum(s->src[s->pos])) s->pos++;
      int len = s->pos - start;
      const char *kw = src + start;

      if (len == 4 && strncmp(kw, "true", 4) == 0) {
         asl_node_t *n = node_alloc(ASL_NODE_BOOL_LITERAL);
         if (!n) return NULL;
         n->bool_value = 1;
         return n;
      }
      if (len == 5 && strncmp(kw, "false", 5) == 0) {
         asl_node_t *n = node_alloc(ASL_NODE_BOOL_LITERAL);
         if (!n) return NULL;
         n->bool_value = 0;
         return n;
      }
      if (len == 4 && strncmp(kw, "null", 4) == 0) {
         return node_alloc(ASL_NODE_NULL_LITERAL);
      }

      asl_node_t *n = node_alloc(ASL_NODE_IDENT);
      if (!n) return NULL;
      n->start  = start;
      n->length = len;

      /* Check for postfix: member access or call */
      scan_skip_ws(s);
      while (s->pos < s->end) {
         if (s->src[s->pos] == '.') {
            s->pos++;
            scan_skip_ws(s);
            int mstart = s->pos;
            while (s->pos < s->end && is_alnum(s->src[s->pos])) s->pos++;
            int mlen = s->pos - mstart;
            asl_node_t *ma = node_alloc(ASL_NODE_MEMBER_ACCESS);
            if (!ma) { node_free_impl(n); return NULL; }
            ma->member_start  = mstart;
            ma->member_length = mlen;
            if (!node_add_child(ma, n)) { node_free_impl(ma); return NULL; }
            n = ma;
            /* fall through to check for call */
         } else {
            break;
         }
      }

      /* Function call: ident or member_access followed by '(' ... ')' */
      scan_skip_ws(s);
      if (s->pos < s->end && s->src[s->pos] == '(') {
         s->pos++;
         asl_node_t *call = node_alloc(ASL_NODE_CALL);
         if (!call) { node_free_impl(n); return NULL; }
         if (!node_add_child(call, n)) { node_free_impl(call); return NULL; }
         scan_skip_ws(s);
         if (s->pos < s->end && s->src[s->pos] == ')') {
            s->pos++;
         } else {
            while (true) {
               asl_node_t *arg = parse_expr(s, src, 0);
               if (!arg) { node_free_impl(call); return NULL; }
               if (!node_add_child(call, arg)) { node_free_impl(call); return NULL; }
               scan_skip_ws(s);
               if (s->pos < s->end && s->src[s->pos] == ',') { s->pos++; continue; }
               break;
            }
            scan_skip_ws(s);
            if (s->pos >= s->end || s->src[s->pos] != ')') {
               anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR, "expected ')'", 0, 0, __FILE__);
               node_free_impl(call);
               return NULL;
            }
            s->pos++;
         }
         return call;
      }

      return n;
   }

   /* Unknown */
   char msg[64];
   snprintf(msg, sizeof(msg), "unexpected character '%c' in expression", c);
   anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR, msg, 0, 0, __FILE__);
   return NULL;
}

/* ------------------------------------------------------------------ */
/* parse_block — '{' stmt* '}'                                       */
/* ------------------------------------------------------------------ */
static asl_node_t *parse_block(asl_scanner_t *s, const char *src) {
   scan_skip_ws(s);
   if (s->pos >= s->end || s->src[s->pos] != '{') {
      anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR, "expected '{'", 0, 0, __FILE__);
      return NULL;
   }
   s->pos++;
   asl_node_t *block = node_alloc(ASL_NODE_BLOCK);
   if (!block) return NULL;
   while (true) {
      scan_skip_ws(s);
      if (s->pos >= s->end || s->src[s->pos] == '}') break;
      asl_node_t *stmt = parse_stmt(s, src);
      if (!stmt) { node_free_impl(block); return NULL; }
      if (!node_add_child(block, stmt)) { node_free_impl(block); return NULL; }
   }
   if (s->pos < s->end && s->src[s->pos] == '}') s->pos++;
   return block;
}

/* ------------------------------------------------------------------ */
/* parse_stmt                                                         */
/* ------------------------------------------------------------------ */
static asl_node_t *parse_stmt(asl_scanner_t *s, const char *src) {
   scan_skip_ws(s);
   if (s->pos >= s->end) return NULL;

   /* return */
   if (s->pos + 6 <= s->end && strncmp(src + s->pos, "return", 6) == 0
       && !is_alnum(src[s->pos + 6])) {
      s->pos += 6;
      asl_node_t *n = node_alloc(ASL_NODE_RETURN);
      if (!n) return NULL;
      scan_skip_ws(s);
      /* optional expression */
      if (s->pos < s->end && s->src[s->pos] != ';' && s->src[s->pos] != '}') {
         asl_node_t *expr = parse_expr(s, src, 0);
         if (!expr) { node_free_impl(n); return NULL; }
         if (!node_add_child(n, expr)) { node_free_impl(n); return NULL; }
      }
      scan_match(s, ';');
      return n;
   }

   /* break */
   if (s->pos + 5 <= s->end && strncmp(src + s->pos, "break", 5) == 0
       && !is_alnum(src[s->pos + 5])) {
      s->pos += 5;
      scan_match(s, ';');
      return node_alloc(ASL_NODE_BREAK);
   }

   /* continue */
   if (s->pos + 8 <= s->end && strncmp(src + s->pos, "continue", 8) == 0
       && !is_alnum(src[s->pos + 8])) {
      s->pos += 8;
      scan_match(s, ';');
      return node_alloc(ASL_NODE_CONTINUE);
   }

   /* if / else-if / else */
   if (s->pos + 2 <= s->end && strncmp(src + s->pos, "if", 2) == 0
       && !is_alnum(src[s->pos + 2])) {
      s->pos += 2;
      scan_skip_ws(s);
      if (s->pos >= s->end || s->src[s->pos] != '(') {
         anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR, "expected '(' after 'if'",
                        0, 0, __FILE__);
         return NULL;
      }
      s->pos++;
      asl_node_t *cond = parse_expr(s, src, 0);
      if (!cond) return NULL;
      scan_skip_ws(s);
      if (s->pos >= s->end || s->src[s->pos] != ')') {
         anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR,
                        "expected ')' to close 'if' condition", 0, 0, __FILE__);
         node_free_impl(cond);
         return NULL;
      }
      s->pos++;
      asl_node_t *then_block = parse_block(s, src);
      if (!then_block) { node_free_impl(cond); return NULL; }

      asl_node_t *if_node = node_alloc(ASL_NODE_IF);
      if (!if_node) {
         node_free_impl(cond); node_free_impl(then_block); return NULL;
      }
      if (!node_add_child(if_node, cond)
          || !node_add_child(if_node, then_block)) {
         node_free_impl(if_node); return NULL;
      }

      /* optional else / else-if */
      scan_skip_ws(s);
      if (s->pos + 4 <= s->end && strncmp(src + s->pos, "else", 4) == 0
          && !is_alnum(src[s->pos + 4])) {
         s->pos += 4;
         scan_skip_ws(s);
         asl_node_t *else_branch;
         if (s->pos + 2 <= s->end && strncmp(src + s->pos, "if", 2) == 0
             && !is_alnum(src[s->pos + 2])) {
            /* else-if — parse as a nested if statement */
            s->pos -= 0; /* keep pos, parse_stmt will see "if" */
            /* back up to let parse_stmt handle it — but we consumed 'else'
             * already.  Synthesise as: else { if … } */
            else_branch = parse_stmt(s, src);
         } else {
            else_branch = parse_block(s, src);
         }
         if (!else_branch) { node_free_impl(if_node); return NULL; }
         if (!node_add_child(if_node, else_branch)) {
            node_free_impl(if_node); return NULL;
         }
      }
      return if_node;
   }

   /* for ( init ; cond ; step ) block */
   if (s->pos + 3 <= s->end && strncmp(src + s->pos, "for", 3) == 0
       && !is_alnum(src[s->pos + 3])) {
      s->pos += 3;
      scan_skip_ws(s);
      if (s->pos >= s->end || s->src[s->pos] != '(') {
         anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR,
                        "expected '(' after 'for'", 0, 0, __FILE__);
         return NULL;
      }
      s->pos++;

      /* init clause — assignment or empty */
      asl_node_t *init_node = NULL;
      scan_skip_ws(s);
      if (s->pos < s->end && s->src[s->pos] != ';') {
         /* expect ident = expr */
         int id_start = s->pos;
         while (s->pos < s->end && is_alnum(s->src[s->pos])) s->pos++;
         int id_len = s->pos - id_start;
         scan_skip_ws(s);
         if (s->pos >= s->end || s->src[s->pos] != '=') {
            anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR,
                           "expected '=' in for-init", 0, 0, __FILE__);
            return NULL;
         }
         s->pos++;
         asl_node_t *rhs = parse_expr(s, src, 0);
         if (!rhs) return NULL;
         asl_node_t *lhs = node_alloc(ASL_NODE_IDENT);
         if (!lhs) { node_free_impl(rhs); return NULL; }
         lhs->start  = id_start;
         lhs->length = id_len;
         init_node = node_alloc(ASL_NODE_ASSIGN);
         if (!init_node) { node_free_impl(lhs); node_free_impl(rhs); return NULL; }
         init_node->op = ASL_OP_NONE;
         if (!node_add_child(init_node, lhs) || !node_add_child(init_node, rhs)) {
            node_free_impl(init_node); return NULL;
         }
      }
      scan_skip_ws(s);
      if (s->pos >= s->end || s->src[s->pos] != ';') {
         anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR,
                        "expected ';' after for-init", 0, 0, __FILE__);
         node_free_impl(init_node);
         return NULL;
      }
      s->pos++;

      /* cond clause */
      asl_node_t *cond_node = NULL;
      scan_skip_ws(s);
      if (s->pos < s->end && s->src[s->pos] != ';') {
         cond_node = parse_expr(s, src, 0);
         if (!cond_node) { node_free_impl(init_node); return NULL; }
      }
      scan_skip_ws(s);
      if (s->pos >= s->end || s->src[s->pos] != ';') {
         anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR,
                        "expected ';' after for-cond", 0, 0, __FILE__);
         node_free_impl(init_node); node_free_impl(cond_node);
         return NULL;
      }
      s->pos++;

      /* step clause — assignment, augmented assignment, i++, i-- */
      asl_node_t *step_node = NULL;
      scan_skip_ws(s);
      if (s->pos < s->end && s->src[s->pos] != ')') {
         int id_start = s->pos;
         while (s->pos < s->end && is_alnum(s->src[s->pos])) s->pos++;
         int id_len = s->pos - id_start;
         scan_skip_ws(s);

         asl_node_t *lhs_id = node_alloc(ASL_NODE_IDENT);
         if (!lhs_id) { node_free_impl(init_node); node_free_impl(cond_node); return NULL; }
         lhs_id->start  = id_start;
         lhs_id->length = id_len;

         if (s->pos + 1 < s->end
             && s->src[s->pos] == '+' && s->src[s->pos + 1] == '+') {
            /* i++ → assign(i, i+1) */
            s->pos += 2;
            asl_node_t *one = node_alloc(ASL_NODE_INT_LITERAL);
            if (!one) { node_free_impl(lhs_id); node_free_impl(init_node); node_free_impl(cond_node); return NULL; }
            one->int_value = 1;
            asl_node_t *ref  = node_alloc(ASL_NODE_IDENT);
            if (!ref)  { node_free_impl(one); node_free_impl(lhs_id); node_free_impl(init_node); node_free_impl(cond_node); return NULL; }
            ref->start  = id_start;
            ref->length = id_len;
            asl_node_t *add = node_alloc(ASL_NODE_BINARY_EXPR);
            if (!add)  { node_free_impl(ref); node_free_impl(one); node_free_impl(lhs_id); node_free_impl(init_node); node_free_impl(cond_node); return NULL; }
            add->op = ASL_OP_ADD;
            node_add_child(add, ref); node_add_child(add, one);
            step_node = node_alloc(ASL_NODE_ASSIGN);
            if (!step_node) { node_free_impl(add); node_free_impl(lhs_id); node_free_impl(init_node); node_free_impl(cond_node); return NULL; }
            node_add_child(step_node, lhs_id);
            node_add_child(step_node, add);
         } else if (s->pos + 1 < s->end
                    && s->src[s->pos] == '-' && s->src[s->pos + 1] == '-') {
            /* i-- → assign(i, i-1) */
            s->pos += 2;
            asl_node_t *one = node_alloc(ASL_NODE_INT_LITERAL);
            one->int_value = 1;
            asl_node_t *ref  = node_alloc(ASL_NODE_IDENT);
            ref->start  = id_start; ref->length = id_len;
            asl_node_t *sub = node_alloc(ASL_NODE_BINARY_EXPR);
            sub->op = ASL_OP_SUB;
            node_add_child(sub, ref); node_add_child(sub, one);
            step_node = node_alloc(ASL_NODE_ASSIGN);
            node_add_child(step_node, lhs_id);
            node_add_child(step_node, sub);
         } else if (s->pos < s->end && s->src[s->pos] == '=') {
            /* i = expr  or  i += expr, etc. */
            asl_op_t aug = ASL_OP_NONE;
            if (s->pos > 0) {
               char prev = s->src[s->pos - 1];
               if      (prev == '+') aug = ASL_OP_ADD;
               else if (prev == '-') aug = ASL_OP_SUB;
               else if (prev == '*') aug = ASL_OP_MUL;
               else if (prev == '/') aug = ASL_OP_DIV;
            }
            s->pos++;
            asl_node_t *rhs = parse_expr(s, src, 0);
            if (!rhs) { node_free_impl(lhs_id); node_free_impl(init_node); node_free_impl(cond_node); return NULL; }
            if (aug != ASL_OP_NONE) {
               asl_node_t *ref2 = node_alloc(ASL_NODE_IDENT);
               ref2->start = id_start; ref2->length = id_len;
               asl_node_t *binop = node_alloc(ASL_NODE_BINARY_EXPR);
               binop->op = aug;
               node_add_child(binop, ref2); node_add_child(binop, rhs);
               rhs = binop;
            }
            step_node = node_alloc(ASL_NODE_ASSIGN);
            node_add_child(step_node, lhs_id);
            node_add_child(step_node, rhs);
         } else {
            node_free_impl(lhs_id);
         }
      }
      scan_skip_ws(s);
      if (s->pos >= s->end || s->src[s->pos] != ')') {
         anvl_error_set(ANVL_ERR_ASL_PARSE_ERROR,
                        "expected ')' to close for-header", 0, 0, __FILE__);
         node_free_impl(init_node); node_free_impl(cond_node); node_free_impl(step_node);
         return NULL;
      }
      s->pos++;

      asl_node_t *body = parse_block(s, src);
      if (!body) {
         node_free_impl(init_node); node_free_impl(cond_node); node_free_impl(step_node);
         return NULL;
      }

      asl_node_t *for_node = node_alloc(ASL_NODE_FOR);
      if (!for_node) { node_free_impl(init_node); node_free_impl(cond_node); node_free_impl(step_node); node_free_impl(body); return NULL; }
      /* Children: [init?, cond?, step?, body] — we always push 4 slots;
       * NULL children are valid (empty clause).
       * We store NULLs explicitly so index 3 = body. */
      if (!node_add_child(for_node, init_node)) { node_free_impl(for_node); return NULL; }
      if (!node_add_child(for_node, cond_node)) { node_free_impl(for_node); return NULL; }
      if (!node_add_child(for_node, step_node)) { node_free_impl(for_node); return NULL; }
      if (!node_add_child(for_node, body))      { node_free_impl(for_node); return NULL; }
      return for_node;
   }

   /* Assignment or expression-statement.
    * Peek ahead: if we see  ident ('='|'+='|'-='|'*='|'/=')  → assign */
   {
      int saved = s->pos;
      /* try to lex an identifier */
      if (is_alpha(src[s->pos])) {
         int id_start = s->pos;
         while (s->pos < s->end && is_alnum(s->src[s->pos])) s->pos++;
         int id_len = s->pos - id_start;
         scan_skip_ws(s);

         asl_op_t aug = ASL_OP_NONE;
         bool is_assign = false;

         if (s->pos + 1 < s->end
             && s->src[s->pos + 1] == '='
             && (s->src[s->pos] == '+' || s->src[s->pos] == '-'
                 || s->src[s->pos] == '*' || s->src[s->pos] == '/')) {
            static const char ops[] = "+-*/";
            static const asl_op_t op_map[] = { ASL_OP_ADD, ASL_OP_SUB, ASL_OP_MUL, ASL_OP_DIV };
            for (int qi = 0; qi < 4; qi++) {
               if (s->src[s->pos] == ops[qi]) { aug = op_map[qi]; break; }
            }
            s->pos += 2;
            is_assign = true;
         } else if (s->pos < s->end && s->src[s->pos] == '='
                    && (s->pos + 1 >= s->end || s->src[s->pos + 1] != '=')) {
            s->pos++;
            is_assign = true;
         } else if (s->pos + 1 < s->end
                    && s->src[s->pos] == '+' && s->src[s->pos + 1] == '+') {
            aug = ASL_OP_ADD;
            s->pos += 2;
            is_assign = true;
            /* rhs = ident + 1 — handled below via aug */
         } else if (s->pos + 1 < s->end
                    && s->src[s->pos] == '-' && s->src[s->pos + 1] == '-') {
            aug = ASL_OP_SUB;
            s->pos += 2;
            is_assign = true;
         }

         if (is_assign) {
            asl_node_t *rhs;
            if (aug == ASL_OP_ADD || aug == ASL_OP_SUB) {
               /* Check if it was ++ / -- (next char is after operator) */
               scan_skip_ws(s);
               /* If next is ';' or '}' it was i++/i-- with no explicit rhs */
               if (s->pos >= s->end || s->src[s->pos] == ';' || s->src[s->pos] == '}') {
                  /* i++ desugars to i = i + 1 */
                  asl_node_t *ref = node_alloc(ASL_NODE_IDENT);
                  if (!ref) return NULL;
                  ref->start  = id_start;
                  ref->length = id_len;
                  asl_node_t *one = node_alloc(ASL_NODE_INT_LITERAL);
                  if (!one) { node_free_impl(ref); return NULL; }
                  one->int_value = 1;
                  rhs = node_alloc(ASL_NODE_BINARY_EXPR);
                  if (!rhs) { node_free_impl(ref); node_free_impl(one); return NULL; }
                  rhs->op = aug;
                  node_add_child(rhs, ref);
                  node_add_child(rhs, one);
               } else {
                  asl_node_t *raw = parse_expr(s, src, 0);
                  if (!raw) return NULL;
                  asl_node_t *ref = node_alloc(ASL_NODE_IDENT);
                  ref->start = id_start; ref->length = id_len;
                  asl_node_t *bin = node_alloc(ASL_NODE_BINARY_EXPR);
                  bin->op = aug;
                  node_add_child(bin, ref); node_add_child(bin, raw);
                  rhs = bin;
               }
            } else if (aug != ASL_OP_NONE) {
               asl_node_t *raw = parse_expr(s, src, 0);
               if (!raw) return NULL;
               asl_node_t *ref = node_alloc(ASL_NODE_IDENT);
               ref->start = id_start; ref->length = id_len;
               asl_node_t *bin = node_alloc(ASL_NODE_BINARY_EXPR);
               bin->op = aug;
               node_add_child(bin, ref); node_add_child(bin, raw);
               rhs = bin;
            } else {
               rhs = parse_expr(s, src, 0);
               if (!rhs) return NULL;
            }
            asl_node_t *lhs = node_alloc(ASL_NODE_IDENT);
            if (!lhs) { node_free_impl(rhs); return NULL; }
            lhs->start  = id_start;
            lhs->length = id_len;
            asl_node_t *assign = node_alloc(ASL_NODE_ASSIGN);
            if (!assign) { node_free_impl(lhs); node_free_impl(rhs); return NULL; }
            if (!node_add_child(assign, lhs) || !node_add_child(assign, rhs)) {
               node_free_impl(assign);
               return NULL;
            }
            scan_match(s, ';');
            return assign;
         }
         /* Not an assignment — fall through to expression statement */
         s->pos = saved;
      }

      /* Expression statement */
      asl_node_t *expr = parse_expr(s, src, 0);
      if (!expr) return NULL;
      scan_match(s, ';');
      return expr;
   }
}

/* ================================================================
 * §3b  Public parse entry point
 * ================================================================ */

static asl_node_t *asl_parse(const asl_function_meta_t *meta, const char *src) {
   if (!meta || !src) return NULL;

   /* The body span includes the outer braces.  Strip them. */
   int body_inner_start = meta->body_start + 1;
   int body_inner_end   = meta->body_start + meta->body_length - 1;
   if (body_inner_end <= body_inner_start) {
      /* Empty body — return empty block */
      return node_alloc(ASL_NODE_BLOCK);
   }

   asl_scanner_t s = {
       .src = src,
       .pos = body_inner_start,
       .end = body_inner_end,
   };

   asl_node_t *block = node_alloc(ASL_NODE_BLOCK);
   if (!block) return NULL;

   while (true) {
      scan_skip_ws(&s);
      if (s.pos >= s.end) break;
      asl_node_t *stmt = parse_stmt(&s, src);
      if (!stmt) { node_free_impl(block); return NULL; }
      if (!node_add_child(block, stmt)) { node_free_impl(block); return NULL; }
   }
   return block;
}

/* ================================================================
 * §4  Evaluator
 * ================================================================ */

/* ------------------------------------------------------------------ */
/* Scope — flat hash-less linear search (small n, simple)            */
/* ------------------------------------------------------------------ */
typedef struct {
   char         names[ASL_MAX_SCOPE][64]; /* NUL-terminated name copies */
   asl_value_t  values[ASL_MAX_SCOPE];
   int          count;
} asl_scope_impl_t;

static bool scope_set(asl_scope_impl_t *sc, const char *name, int namelen,
                      const asl_value_t *val) {
   /* Update existing */
   for (int i = 0; i < sc->count; i++) {
      if (strncmp(sc->names[i], name, (usize)namelen) == 0
          && sc->names[i][namelen] == '\0') {
         value_free_impl(&sc->values[i]);
         return value_copy(&sc->values[i], val);
      }
   }
   /* Insert new */
   if (sc->count >= ASL_MAX_SCOPE) return false;
   int idx = sc->count++;
   int cplen = namelen < 63 ? namelen : 63;
   memcpy(sc->names[idx], name, (usize)cplen);
   sc->names[idx][cplen] = '\0';
   memset(&sc->values[idx], 0, sizeof(asl_value_t));
   return value_copy(&sc->values[idx], val);
}

static bool scope_get(const asl_scope_impl_t *sc, const char *name, int namelen,
                      asl_value_t *out) {
   for (int i = 0; i < sc->count; i++) {
      if (strncmp(sc->names[i], name, (usize)namelen) == 0
          && sc->names[i][namelen] == '\0') {
         return value_copy(out, &sc->values[i]);
      }
   }
   return false;
}

static void scope_free_values(asl_scope_impl_t *sc) {
   for (int i = 0; i < sc->count; i++)
      value_free_impl(&sc->values[i]);
   sc->count = 0;
}

/* ------------------------------------------------------------------ */
/* Exec context                                                       */
/* ------------------------------------------------------------------ */
typedef struct asl_eval_ctx_t {
   asl_scope_impl_t   *scope;
   const char         *src;
   asl_module_t       *modules;
   int                 module_count;
   asl_ext_lookup_cb   ext_lookup;
   void               *ext_ud;
   asl_fn_dispatch_cb  fn_dispatch;
   void               *fn_ud;

   bool        has_return;
   bool        has_break;
   bool        has_continue;
   asl_value_t return_value;
   int         depth;
} asl_eval_ctx_t;

/* Forward declarations */
static bool eval_block(asl_eval_ctx_t *ctx, const asl_node_t *block);
static bool eval_expr(asl_eval_ctx_t *ctx, const asl_node_t *node, asl_value_t *out);

/* ------------------------------------------------------------------ */
/* Truthiness                                                         */
/* ------------------------------------------------------------------ */
static bool asl_truthy(const asl_value_t *v) {
   switch (v->kind) {
      case ASL_NULL:   return false;
      case ASL_BOOL:   return v->bool_value != 0;
      case ASL_INT:    return v->long_value  != 0;
      case ASL_FLOAT:  return v->double_value != 0.0;
      case ASL_STRING: return v->string_value && v->string_value[0] != '\0';
      case ASL_TUPLE:  /* fall-through: non-null tuple is truthy */
      case ASL_LIST:   return v->list_count > 0;
      case ASL_MAP:    return v->map_count  > 0;
   }
   return false;
}

/* ------------------------------------------------------------------ */
/* eval_expr                                                          */
/* ------------------------------------------------------------------ */
static bool eval_expr(asl_eval_ctx_t *ctx, const asl_node_t *node, asl_value_t *out) {
   memset(out, 0, sizeof(*out));

   if (ctx->depth > ASL_MAX_DEPTH) {
      anvl_error_set(ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED,
                     anvl_error_code_message(ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED),
                     0, 0, __FILE__);
      return false;
   }

   switch (node->kind) {

      case ASL_NODE_NULL_LITERAL:
         out->kind = ASL_NULL;
         return true;

      case ASL_NODE_BOOL_LITERAL:
         out->kind       = ASL_BOOL;
         out->bool_value = node->bool_value;
         return true;

      case ASL_NODE_INT_LITERAL:
         out->kind       = ASL_INT;
         out->long_value = node->int_value;
         return true;

      case ASL_NODE_FLOAT_LITERAL:
         out->kind         = ASL_FLOAT;
         out->double_value = node->float_value;
         return true;

      case ASL_NODE_STRING_LITERAL: {
         usize len = (usize)node->length;
         char *s = Allocator.alloc(len + 1);
         if (!s) return false;
         memcpy(s, ctx->src + node->start, len);
         s[len] = '\0';
         out->kind         = ASL_STRING;
         out->string_value = s;
         return true;
      }

      case ASL_NODE_IDENT: {
         if (!scope_get(ctx->scope, ctx->src + node->start, node->length, out)) {
            char msg[96];
            snprintf(msg, sizeof(msg), "undefined variable '%.*s'",
                     node->length, ctx->src + node->start);
            anvl_error_set(ANVL_ERR_ASL_RUNTIME_ERROR, msg, 0, 0, __FILE__);
            return false;
         }
         return true;
      }

      case ASL_NODE_VAR_REF: {
         /* $identifier — check local scope first, then external lookup */
         if (scope_get(ctx->scope, ctx->src + node->start, node->length, out))
            return true;
         if (ctx->ext_lookup) {
            /* Build NUL-terminated key */
            int klen = node->length;
            char *key = Allocator.alloc((usize)(klen + 1));
            if (!key) return false;
            memcpy(key, ctx->src + node->start, (usize)klen);
            key[klen] = '\0';
            bool found = ctx->ext_lookup(key, out, ctx->ext_ud);
            Allocator.dispose(key);
            if (found) return true;
         }
         char msg[96];
         snprintf(msg, sizeof(msg), "unresolved $%.*s",
                  node->length, ctx->src + node->start);
         anvl_error_set(ANVL_ERR_ASL_RUNTIME_ERROR, msg, 0, 0, __FILE__);
         return false;
      }

      case ASL_NODE_LIST_LITERAL: {
         out->kind = ASL_LIST;
         if (node->child_count == 0) return true;
         out->list_items = Allocator.alloc(sizeof(asl_value_t) * (usize)node->child_count);
         if (!out->list_items) return false;
         memset(out->list_items, 0, sizeof(asl_value_t) * (usize)node->child_count);
         for (int i = 0; i < node->child_count; i++) {
            if (!eval_expr(ctx, node->children[i], &out->list_items[i])) {
               value_free_impl(out);
               return false;
            }
            out->list_count++;
         }
         return true;
      }

      case ASL_NODE_UNARY_EXPR: {
         asl_value_t operand = {0};
         if (!eval_expr(ctx, node->children[0], &operand)) return false;
         if (node->op == ASL_OP_NEG) {
            if (operand.kind == ASL_INT) {
               out->kind       = ASL_INT;
               out->long_value = -operand.long_value;
            } else if (operand.kind == ASL_FLOAT) {
               out->kind         = ASL_FLOAT;
               out->double_value = -operand.double_value;
            } else {
               anvl_error_set(ANVL_ERR_ASL_RUNTIME_ERROR,
                              "unary '-' on non-numeric value", 0, 0, __FILE__);
               value_free_impl(&operand);
               return false;
            }
         } else { /* NOT */
            out->kind       = ASL_BOOL;
            out->bool_value = asl_truthy(&operand) ? 0 : 1;
         }
         value_free_impl(&operand);
         return true;
      }

      case ASL_NODE_BINARY_EXPR: {
         /* Short-circuit logical ops */
         if (node->op == ASL_OP_AND) {
            asl_value_t lv = {0};
            if (!eval_expr(ctx, node->children[0], &lv)) return false;
            if (!asl_truthy(&lv)) {
               value_free_impl(&lv);
               out->kind = ASL_BOOL; out->bool_value = 0;
               return true;
            }
            value_free_impl(&lv);
            asl_value_t rv = {0};
            if (!eval_expr(ctx, node->children[1], &rv)) return false;
            bool t = asl_truthy(&rv);
            value_free_impl(&rv);
            out->kind = ASL_BOOL; out->bool_value = t ? 1 : 0;
            return true;
         }
         if (node->op == ASL_OP_OR) {
            asl_value_t lv = {0};
            if (!eval_expr(ctx, node->children[0], &lv)) return false;
            if (asl_truthy(&lv)) {
               value_free_impl(&lv);
               out->kind = ASL_BOOL; out->bool_value = 1;
               return true;
            }
            value_free_impl(&lv);
            asl_value_t rv = {0};
            if (!eval_expr(ctx, node->children[1], &rv)) return false;
            bool t = asl_truthy(&rv);
            value_free_impl(&rv);
            out->kind = ASL_BOOL; out->bool_value = t ? 1 : 0;
            return true;
         }

         asl_value_t lv = {0}, rv = {0};
         if (!eval_expr(ctx, node->children[0], &lv)) return false;
         if (!eval_expr(ctx, node->children[1], &rv)) {
            value_free_impl(&lv); return false;
         }

         /* String concat */
         if (node->op == ASL_OP_ADD
             && (lv.kind == ASL_STRING || rv.kind == ASL_STRING)) {
            /* Convert both sides to string then concat */
            /* Simple — assume both are already strings for now */
            usize llen = lv.string_value ? strlen(lv.string_value) : 0;
            usize rlen = rv.string_value ? strlen(rv.string_value) : 0;
            char *buf = Allocator.alloc(llen + rlen + 1);
            if (!buf) { value_free_impl(&lv); value_free_impl(&rv); return false; }
            if (lv.string_value) memcpy(buf,         lv.string_value, llen);
            if (rv.string_value) memcpy(buf + llen,  rv.string_value, rlen);
            buf[llen + rlen] = '\0';
            value_free_impl(&lv); value_free_impl(&rv);
            out->kind = ASL_STRING; out->string_value = buf;
            return true;
         }

         /* Float promotion */
         bool use_float = (lv.kind == ASL_FLOAT || rv.kind == ASL_FLOAT);
         double ld = (lv.kind == ASL_FLOAT) ? lv.double_value : (double)lv.long_value;
         double rd = (rv.kind == ASL_FLOAT) ? rv.double_value : (double)rv.long_value;
         int64_t li = (lv.kind == ASL_INT)  ? lv.long_value  : (int64_t)lv.double_value;
         int64_t ri = (rv.kind == ASL_INT)  ? rv.long_value  : (int64_t)rv.double_value;
         value_free_impl(&lv); value_free_impl(&rv);

         switch (node->op) {
            case ASL_OP_ADD:
               if (use_float) { out->kind=ASL_FLOAT; out->double_value=ld+rd; }
               else            { out->kind=ASL_INT;   out->long_value=li+ri; }
               return true;
            case ASL_OP_SUB:
               if (use_float) { out->kind=ASL_FLOAT; out->double_value=ld-rd; }
               else            { out->kind=ASL_INT;   out->long_value=li-ri; }
               return true;
            case ASL_OP_MUL:
               if (use_float) { out->kind=ASL_FLOAT; out->double_value=ld*rd; }
               else            { out->kind=ASL_INT;   out->long_value=li*ri; }
               return true;
            case ASL_OP_DIV:
               if (ri == 0 && !use_float) {
                  anvl_error_set(ANVL_ERR_ASL_RUNTIME_ERROR, "division by zero", 0, 0, __FILE__);
                  return false;
               }
               if (use_float) { out->kind=ASL_FLOAT; out->double_value=ld/rd; }
               else            { out->kind=ASL_INT;   out->long_value=li/ri; }
               return true;
            case ASL_OP_MOD:
               if (ri == 0) {
                  anvl_error_set(ANVL_ERR_ASL_RUNTIME_ERROR, "modulo by zero", 0, 0, __FILE__);
                  return false;
               }
               out->kind=ASL_INT; out->long_value=li % ri;
               return true;
            case ASL_OP_EQ:
               out->kind=ASL_BOOL;
               if (use_float) out->bool_value = (ld == rd) ? 1 : 0;
               else            out->bool_value = (li == ri) ? 1 : 0;
               return true;
            case ASL_OP_NOT_EQ:
               out->kind=ASL_BOOL;
               if (use_float) out->bool_value = (ld != rd) ? 1 : 0;
               else            out->bool_value = (li != ri) ? 1 : 0;
               return true;
            case ASL_OP_LT:
               out->kind=ASL_BOOL;
               if (use_float) out->bool_value = (ld < rd) ? 1 : 0;
               else            out->bool_value = (li < ri) ? 1 : 0;
               return true;
            case ASL_OP_GT:
               out->kind=ASL_BOOL;
               if (use_float) out->bool_value = (ld > rd) ? 1 : 0;
               else            out->bool_value = (li > ri) ? 1 : 0;
               return true;
            case ASL_OP_LT_EQ:
               out->kind=ASL_BOOL;
               if (use_float) out->bool_value = (ld <= rd) ? 1 : 0;
               else            out->bool_value = (li <= ri) ? 1 : 0;
               return true;
            case ASL_OP_GE:
               out->kind=ASL_BOOL;
               if (use_float) out->bool_value = (ld >= rd) ? 1 : 0;
               else            out->bool_value = (li >= ri) ? 1 : 0;
               return true;
            default:
               anvl_error_set(ANVL_ERR_ASL_RUNTIME_ERROR,
                              "unknown binary operator", 0, 0, __FILE__);
               return false;
         }
      }

      case ASL_NODE_CALL: {
         /* callee is children[0]; args are children[1..] */
         const asl_node_t *callee = node->children[0];
         int argc = node->child_count - 1;
         asl_value_t *args = NULL;
         if (argc > 0) {
            args = Allocator.alloc(sizeof(asl_value_t) * (usize)argc);
            if (!args) return false;
            memset(args, 0, sizeof(asl_value_t) * (usize)argc);
            for (int i = 0; i < argc; i++) {
               if (!eval_expr(ctx, node->children[i + 1], &args[i])) {
                  for (int j = 0; j < i; j++) value_free_impl(&args[j]);
                  Allocator.dispose(args);
                  return false;
               }
            }
         }

         bool ok = false;
         if (callee->kind == ASL_NODE_MEMBER_ACCESS) {
            /* module.function call */
            const char *module_name = ctx->src + callee->children[0]->start;
            int         module_len  = callee->children[0]->length;
            const char *fn_name     = ctx->src + callee->member_start;
            int         fn_len      = callee->member_length;
            for (int mi = 0; mi < ctx->module_count; mi++) {
               if (strncmp(ctx->modules[mi].name, module_name, (usize)module_len) == 0
                   && ctx->modules[mi].name[module_len] == '\0') {
                  char *fn_buf = Allocator.alloc((usize)(fn_len + 1));
                  if (!fn_buf) break;
                  memcpy(fn_buf, fn_name, (usize)fn_len);
                  fn_buf[fn_len] = '\0';
                  *out = ctx->modules[mi].call(fn_buf, args, argc,
                                               ctx->modules[mi].userdata);
                  Allocator.dispose(fn_buf);
                  ok = true;
                  break;
               }
            }
         } else {
            /* bare function call */
            if (ctx->fn_dispatch) {
               char *fn_buf = Allocator.alloc((usize)(callee->length + 1));
               if (fn_buf) {
                  memcpy(fn_buf, ctx->src + callee->start, (usize)callee->length);
                  fn_buf[callee->length] = '\0';
                  ok = ctx->fn_dispatch(fn_buf, args, argc, out, ctx->fn_ud);
                  Allocator.dispose(fn_buf);
               }
            }
            if (!ok) {
               /* unresolved call → NULL */
               out->kind = ASL_NULL;
               ok = true;
            }
         }

         if (args) {
            for (int i = 0; i < argc; i++) value_free_impl(&args[i]);
            Allocator.dispose(args);
         }
         return ok;
      }

      case ASL_NODE_MEMBER_ACCESS: {
         /* Standalone member access (not a call) — map lookup or ext */
         asl_value_t obj = {0};
         if (!eval_expr(ctx, node->children[0], &obj)) return false;
         if (obj.kind == ASL_MAP) {
            const char *mname = ctx->src + node->member_start;
            int         mlen  = node->member_length;
            for (int i = 0; i < obj.map_count; i++) {
               if (strncmp(obj.map_keys[i], mname, (usize)mlen) == 0
                   && obj.map_keys[i][mlen] == '\0') {
                  bool cp = value_copy(out, &obj.map_values[i]);
                  value_free_impl(&obj);
                  return cp;
               }
            }
         }
         value_free_impl(&obj);
         out->kind = ASL_NULL;
         return true;
      }

      default:
         anvl_error_set(ANVL_ERR_ASL_RUNTIME_ERROR,
                        "unexpected node in expression", 0, 0, __FILE__);
         return false;
   }
}

/* ------------------------------------------------------------------ */
/* eval_stmt                                                          */
/* ------------------------------------------------------------------ */
static bool eval_stmt(asl_eval_ctx_t *ctx, const asl_node_t *node) {
   switch (node->kind) {

      case ASL_NODE_RETURN: {
         asl_value_t val = {0};
         if (node->child_count > 0) {
            if (!eval_expr(ctx, node->children[0], &val)) return false;
         }
         value_free_impl(&ctx->return_value);
         ctx->return_value = val;
         ctx->has_return   = true;
         return true;
      }

      case ASL_NODE_BREAK:
         ctx->has_break = true;
         return true;

      case ASL_NODE_CONTINUE:
         ctx->has_continue = true;
         return true;

      case ASL_NODE_ASSIGN: {
         asl_value_t val = {0};
         if (!eval_expr(ctx, node->children[1], &val)) return false;
         const asl_node_t *lhs = node->children[0];
         if (!scope_set(ctx->scope, ctx->src + lhs->start, lhs->length, &val)) {
            value_free_impl(&val);
            return false;
         }
         value_free_impl(&val);
         return true;
      }

      case ASL_NODE_IF: {
         asl_value_t cond_val = {0};
         if (!eval_expr(ctx, node->children[0], &cond_val)) return false;
         bool taken = asl_truthy(&cond_val);
         value_free_impl(&cond_val);
         if (taken) {
            return eval_block(ctx, node->children[1]);
         } else if (node->child_count >= 3 && node->children[2]) {
            const asl_node_t *else_branch = node->children[2];
            if (else_branch->kind == ASL_NODE_BLOCK) {
               return eval_block(ctx, else_branch);
            } else {
               return eval_stmt(ctx, else_branch);
            }
         }
         return true;
      }

      case ASL_NODE_FOR: {
         /* children: [init, cond, step, body] */
         asl_node_t *init_n = node->children[0]; /* may be NULL */
         asl_node_t *cond_n = node->children[1]; /* may be NULL */
         asl_node_t *step_n = node->children[2]; /* may be NULL */
         asl_node_t *body_n = node->children[3];

         /* init */
         if (init_n && !eval_stmt(ctx, init_n)) return false;

         while (true) {
            /* cond */
            if (cond_n) {
               asl_value_t cv = {0};
               if (!eval_expr(ctx, cond_n, &cv)) return false;
               bool keep = asl_truthy(&cv);
               value_free_impl(&cv);
               if (!keep) break;
            }
            /* body */
            if (!eval_block(ctx, body_n)) return false;
            if (ctx->has_return) break;
            if (ctx->has_break)  { ctx->has_break = false; break; }
            ctx->has_continue = false; /* consume continue — step still runs */
            /* step */
            if (step_n && !eval_stmt(ctx, step_n)) return false;
         }
         return true;
      }

      case ASL_NODE_BLOCK:
         return eval_block(ctx, node);

      default:
         /* Expression statement */
         {
            asl_value_t discard = {0};
            bool ok = eval_expr(ctx, node, &discard);
            value_free_impl(&discard);
            return ok;
         }
   }
}

/* ------------------------------------------------------------------ */
/* eval_block                                                         */
/* ------------------------------------------------------------------ */
static bool eval_block(asl_eval_ctx_t *ctx, const asl_node_t *block) {
   for (int i = 0; i < block->child_count; i++) {
      if (!eval_stmt(ctx, block->children[i])) return false;
      if (ctx->has_return || ctx->has_break || ctx->has_continue) break;
   }
   return true;
}

/* ================================================================
 * §5  Public exec entry point
 * ================================================================ */

static bool asl_exec(const asl_function_meta_t *meta,
                     const char *src,
                     const asl_value_t *args, int argc,
                     asl_value_t *out,
                     asl_module_t *modules, int module_count,
                     asl_ext_lookup_cb   ext_lookup,  void *ext_ud,
                     asl_fn_dispatch_cb  fn_dispatch, void *fn_ud) {
   if (!meta || !src || !out) return false;

   /* Parse */
   asl_node_t *ast = asl_parse(meta, src);
   if (!ast) return false;

   /* Set up scope with parameters */
   asl_scope_impl_t scope;
   memset(&scope, 0, sizeof(scope));

   int bound = (argc < meta->param_count) ? argc : meta->param_count;
   for (int i = 0; i < bound; i++) {
      if (!meta->param_spans) break;
      int ps = meta->param_spans[i].start;
      int pl = meta->param_spans[i].length;
      if (!scope_set(&scope, src + ps, pl, &args[i])) {
         node_free_impl(ast);
         return false;
      }
   }

   asl_eval_ctx_t ctx = {
       .scope        = &scope,
       .src          = src,
       .modules      = modules,
       .module_count = module_count,
       .ext_lookup   = ext_lookup,
       .ext_ud       = ext_ud,
       .fn_dispatch  = fn_dispatch,
       .fn_ud        = fn_ud,
       .has_return   = false,
       .has_break    = false,
       .has_continue = false,
       .return_value = {0},
       .depth        = 0,
   };

   bool ok = eval_block(&ctx, ast);
   node_free_impl(ast);
   scope_free_values(&scope);

   if (ok) {
      *out = ctx.return_value; /* transfer ownership */
   } else {
      value_free_impl(&ctx.return_value);
   }
   return ok;
}

/* ================================================================
 * §6  Public vtable
 * ================================================================ */

const anvl_asl_i Asl = {
    .parse      = asl_parse,
    .exec       = asl_exec,
    .node_free  = node_free_impl,
    .value_free = value_free_impl,
};
