/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * asl.h — anvil.asl: Anvil Script Language (ASL) parser + evaluator
 * ------------------------------------------------------------------
 * Ported from: AslParser.cs + AslEvaluator.cs (Anvil.Net v0.4.0)
 *
 * Provides a lightweight scripting layer embedded in Anvil documents.
 * A function body is a brace-delimited block of statements that can
 * read locals, parameters, external $references, and call module
 * functions.  No closures, no heap-allocated call frames.
 *
 * Component: post-parse optional (anvil.asl.o)
 *
 * Design notes:
 *   - No exceptions — errors set via anvl_error_set(), return false/NULL
 *   - Scope is FLAT per invocation (no parent-chain closures)
 *   - String values are heap-owned; caller frees via Asl.value_free()
 *   - List / Map items are recursively heap-owned
 *   - Maximum call depth: ASL_MAX_DEPTH (32 by default)
 * ------------------------------------------------------------------
 */
#pragma once

#include "errors.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Forward typedefs                                                   */
/* ------------------------------------------------------------------ */
typedef struct asl_value_t asl_value_t;
typedef struct asl_node_t asl_node_t;
typedef struct asl_module_t asl_module_t;

/* ------------------------------------------------------------------ */
/* Value kind                                                         */
/* ------------------------------------------------------------------ */
typedef enum {
   ASL_NULL = 0,
   ASL_INT = 1,
   ASL_FLOAT = 2,
   ASL_STRING = 3,
   ASL_BOOL = 4,
   ASL_TUPLE = 5,
   ASL_LIST = 6,
   ASL_MAP = 7,
} asl_value_kind_t;

/* ------------------------------------------------------------------ */
/* Value                                                              */
/* string_value, list_items, map_keys/map_values are heap-owned       */
/* (allocated via Allocator.alloc).  Use Asl.value_free() to release. */
/* ------------------------------------------------------------------ */
struct asl_value_t {
   asl_value_kind_t kind;
   int64_t long_value;  /* ASL_INT  */
   double double_value; /* ASL_FLOAT */
   char *string_value;  /* ASL_STRING — heap-owned, NUL-terminated */
   int bool_value;      /* ASL_BOOL: 0=false, 1=true */

   /* ASL_LIST */
   asl_value_t *list_items; /* heap-owned array */
   int list_count;

   /* ASL_MAP */
   char **map_keys;         /* heap-owned: array of heap-owned strings */
   asl_value_t *map_values; /* heap-owned array parallel to map_keys */
   int map_count;
};

/* Convenience initialiser — zero-initialised ASL_NULL value */
#define ASL_VALUE_NULL ((asl_value_t){0})

/* ------------------------------------------------------------------ */
/* Node kind                                                          */
/* ------------------------------------------------------------------ */
typedef enum {
   ASL_NODE_BLOCK = 0,
   ASL_NODE_ASSIGN = 1,
   ASL_NODE_RETURN = 2,
   ASL_NODE_IF = 3,
   ASL_NODE_FOR = 4,
   ASL_NODE_BREAK = 5,
   ASL_NODE_CONTINUE = 6,
   ASL_NODE_LIST_LITERAL = 7,
   ASL_NODE_BINARY_EXPR = 8,
   ASL_NODE_UNARY_EXPR = 9,
   ASL_NODE_IDENT = 10,
   ASL_NODE_INT_LITERAL = 11,
   ASL_NODE_FLOAT_LITERAL = 12,
   ASL_NODE_STRING_LITERAL = 13,
   ASL_NODE_CALL = 14,          /* callee + args; callee may be MEMBER_ACCESS */
   ASL_NODE_MEMBER_ACCESS = 15, /* object.member — object is children[0]       */
   ASL_NODE_VAR_REF = 16,       /* $identifier — external lookup               */
   ASL_NODE_NULL_LITERAL = 17,
   ASL_NODE_BOOL_LITERAL = 18,
} asl_node_kind_t;

/* ------------------------------------------------------------------ */
/* Binary / unary operator                                            */
/* ------------------------------------------------------------------ */
typedef enum {
   ASL_OP_NONE = 0,
   ASL_OP_ADD = 1,
   ASL_OP_SUB = 2,
   ASL_OP_MUL = 3,
   ASL_OP_DIV = 4,
   ASL_OP_MOD = 5,
   ASL_OP_EQ = 6,
   ASL_OP_NOT_EQ = 7,
   ASL_OP_LT = 8,
   ASL_OP_GT = 9,
   ASL_OP_LT_EQ = 10,
   ASL_OP_GE = 11,
   ASL_OP_AND = 12,
   ASL_OP_OR = 13,
   ASL_OP_NEG = 14, /* unary minus */
   ASL_OP_NOT = 15, /* unary !     */
} asl_op_t;

/* ------------------------------------------------------------------ */
/* AST node                                                           */
/* children[] is a heap-owned array of heap-owned node pointers.      */
/* Children semantics:                                                */
/*   BLOCK        → [stmt0, stmt1, …]                                 */
/*   ASSIGN       → [lhs-IDENT, rhs-expr]                             */
/*   RETURN       → [expr]                                            */
/*   IF           → [cond, then-BLOCK, (else-BLOCK)?]                 */
/*   FOR          → [init-ASSIGN?, cond-expr?, step-expr?, body-BLOCK] */
/*   BINARY_EXPR  → [left, right]                                     */
/*   UNARY_EXPR   → [operand]                                         */
/*   CALL         → [callee, arg0, arg1, …]                           */
/*   MEMBER_ACCESS→ [object]  (+member via member_start/member_length)*/
/*   LIST_LITERAL → [elem0, elem1, …]                                 */
/*   IDENT/INT/FLOAT/STRING/VAR_REF/NULL/BOOL → no children           */
/* ------------------------------------------------------------------ */
struct asl_node_t {
   asl_node_kind_t kind;
   asl_op_t op;

   /* Source span for identifiers and string literals */
   int start;
   int length;

   /* Member name span (MEMBER_ACCESS, CALL-via-member) */
   int member_start;
   int member_length;

   /* Literal values */
   int64_t int_value;
   double float_value;
   int bool_value; /* ASL_NODE_BOOL_LITERAL: 0 or 1 */

   /* Children */
   asl_node_t **children;
   int child_count;
};

/* ------------------------------------------------------------------ */
/* Parameter span (one entry per declared parameter)                 */
/* ------------------------------------------------------------------ */
typedef struct {
   int start;
   int length;
} asl_param_span_t;

/* ------------------------------------------------------------------ */
/* Function metadata (captured by the host during document parsing)  */
/* --                                                                 */
/* name_start / name_length — span of the function name in src       */
/* param_count / param_spans — declared parameters                   */
/* body_start / body_length — span of the '{…}' body block in src    */
/* ------------------------------------------------------------------ */
typedef struct {
   int name_start;
   int name_length;
   int param_count;
   asl_param_span_t *param_spans; /* heap-owned, param_count entries */
   int body_start;
   int body_length;
} asl_function_meta_t;

/* ------------------------------------------------------------------ */
/* Module dispatch                                                    */
/* Registered by the host; called for  module.fn(args…)  expressions */
/* ------------------------------------------------------------------ */
struct asl_module_t {
   const char *name;
   asl_value_t (*call)(const char *fn,
                       asl_value_t *args, int argc,
                       void *userdata);
   void *userdata;
};

/* ------------------------------------------------------------------ */
/* External $key lookup callback                                      */
/* Called (after local scope check) for  $identifier  expressions.   */
/* Set *out and return true when found; return false when not found.  */
/* ------------------------------------------------------------------ */
typedef bool (*asl_ext_lookup_cb)(const char *key,
                                  asl_value_t *out,
                                  void *userdata);

/* ------------------------------------------------------------------ */
/* Bare function dispatch callback                                    */
/* Called for unqualified calls:  foo(args…) inside a script.         */
/* Return true when the function was found; false → result is ASL_NULL */
/* ------------------------------------------------------------------ */
typedef bool (*asl_fn_dispatch_cb)(const char *name,
                                   asl_value_t *args, int argc,
                                   asl_value_t *out,
                                   void *userdata);

/* ------------------------------------------------------------------ */
/* Asl vtable                                                         */
/* ------------------------------------------------------------------ */
typedef struct anvl_asl_i {
   /* Parse a function body and return the AST root (BLOCK node).
    * The body range [body_start+1 .. body_start+body_length-1) is parsed
    * (outer braces stripped).
    * Returns NULL + sets ANVL_ERR_ASL_PARSE_ERROR on failure.
    * Caller owns the returned tree; free with Asl.node_free(). */
   asl_node_t *(*parse)(const asl_function_meta_t *meta, const char *src);

   /* Execute a function and write the result to *out.
    * args / argc bind the declared parameter names in order.
    * Returns true on success; false + sets ANVL_ERR_ASL_* on failure.
    * *out may contain heap-owned data (strings/lists/maps); the caller
    * is responsible for calling Asl.value_free(out) afterwards. */
   bool (*exec)(const asl_function_meta_t *meta,
                const char *src,
                const asl_value_t *args, int argc,
                asl_value_t *out,
                asl_module_t *modules, int module_count,
                asl_ext_lookup_cb ext_lookup, void *ext_ud,
                asl_fn_dispatch_cb fn_dispatch, void *fn_ud);

   /* Recursively free an AST node and all its children. */
   void (*node_free)(asl_node_t *node);

   /* Free heap-owned data inside a value (string_value, list_items,
    * map_keys, map_values).  Does NOT free the asl_value_t struct itself
    * (which is typically stack- or array-allocated). */
   void (*value_free)(asl_value_t *val);
} anvl_asl_i;

extern const anvl_asl_i Asl;
