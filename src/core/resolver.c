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
 * resolver.c - Resolution phase (phase 4): $identifier VarRef and        *
 * base/inheritance target validation                                    *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: src/core/resolver.c                                              *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Runs once, after every document in the import graph has been through   *
 * a successful doc_parse_body — not per-document, order-independent.     *
 * See notes/resolution-phase.md for the full design.                     *
 * ********************************************************************** */

#include "internal/constants.h"
#include "internal/module.h"
#include "internal/source.h"
// ----------------
#include <sigma/farray.h>
#include <sigma/list.h>
#include <sigma/map.h>
#include <sigma/types.h>
#include <string.h>

// Builds ctx->identifiers: top-level statement name -> anvl_statement, across every document
// in ctx->docs. Only doc->body (a document's own top-level list) is walked, never the flat
// ctx->statements index — that index also carries nested statements, which are never valid
// resolution targets. A duplicate top-level name anywhere in the context — same document or
// across a merged import — is a hard error (see notes/resolution-phase.md "Duplicate names are
// a hard error, full stop").
static anvl_result build_identifiers(module_context ctx, anvl_err_code *out_err_code) {
   if (!ctx->identifiers) {
      ctx->identifiers = Map.new(ANVL_CTX_DEFAULT_MAP_CAP);
      if (!ctx->identifiers) {
         anvl_error_set(ctx->errors, ANVL_ERR_MEMORY_ALLOC_FAILED, 0, 0, __FILE__, NULL);
         *out_err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
         return ANVL_RES_ERR;
      }
   }

   usize doc_count = List.size(ctx->docs);
   for (usize d = 0; d < doc_count; d++) {
      module_document doc = NULL;
      List.get(ctx->docs, d, (object *)&doc);
      if (!doc || !doc->body) {
         continue;
      }

      int stmt_count = FArray.capacity(doc->body, sizeof(anvl_statement));
      for (int i = 0; i < stmt_count; i++) {
         anvl_statement stmt = NULL;
         FArray.get(doc->body, i, sizeof(anvl_statement), (object *)&stmt);
         if (!stmt) {
            continue;
         }
         usize name_len = Source.slice_length(stmt->name);
         if (name_len == 0) {
            continue; // VARS/USING - no declared name, never a resolvable target
         }
         if (Map.has(ctx->identifiers, stmt->name.start, name_len)) {
            anvl_error_set(ctx->errors, ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER, 0, 0, __FILE__,
                           NULL);
            *out_err_code = ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER;
            return ANVL_RES_ERR;
         }
         Map.set(ctx->identifiers, stmt->name.start, name_len, (addr)stmt);
      }
   }
   return ANVL_RES_OK;
}

// Validates every `base` in the context (any nesting depth, via the flat ctx->statements
// index — base is legal on a nested statement too, not just a top-level one) against
// ctx->identifiers. A base naming nothing in the map is ANVL_ERR_RESOLVER_MISSING_BASE; a
// base naming an anonymous (ANVL_STMT_OBJECT_BLOCK) statement is
// ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS — anonymous objects are immutable and inline, with no
// life of their own independent of the one statement that declares them, so nothing can
// inherit from one (see notes/resolution-phase.md, open question #1).
static anvl_result validate_bases(module_context ctx, anvl_err_code *out_err_code) {
   usize stmt_count = List.size(ctx->statements);
   for (usize i = 0; i < stmt_count; i++) {
      anvl_statement stmt = NULL;
      List.get(ctx->statements, i, (object *)&stmt);
      if (!stmt) {
         continue;
      }
      usize base_len = Source.slice_length(stmt->base);
      if (base_len == 0) {
         continue;
      }

      addr val = 0;
      if (!Map.get(ctx->identifiers, stmt->base.start, base_len, &val)) {
         anvl_error_set(ctx->errors, ANVL_ERR_RESOLVER_MISSING_BASE, 0, 0, __FILE__, NULL);
         *out_err_code = ANVL_ERR_RESOLVER_MISSING_BASE;
         return ANVL_RES_ERR;
      }
      anvl_statement base_stmt = (anvl_statement)val;
      if (base_stmt->kind == ANVL_STMT_OBJECT_BLOCK) {
         anvl_error_set(ctx->errors, ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS, 0, 0, __FILE__,
                        NULL);
         *out_err_code = ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS;
         return ANVL_RES_ERR;
      }
   }
   return ANVL_RES_OK;
}

// True if two slices have identical content.
static bool slices_equal(anvl_slice a, anvl_slice b) {
   usize len_a = Source.slice_length(a);
   usize len_b = Source.slice_length(b);
   if (len_a != len_b) {
      return false;
   }
   if (len_a == 0) {
      return true;
   }
   return memcmp(a.start, b.start, len_a) == 0;
}

// The list of a statement's own, as-parsed fields (nested statements) — the OBJECT_BLOCK body,
// or an ASSIGN statement's object-typed value's statements. NULL for anything else — a
// non-object-shaped statement never has a non-empty base to merge in the first place, per
// body-parse's own "base implies object-shaped" invariant (document-body-parse.md, Resolved
// Questions #3).
static list own_fields(anvl_statement stmt) {
   if (stmt->kind == ANVL_STMT_OBJECT_BLOCK) {
      return stmt->body;
   }
   if (stmt->value && stmt->value->type == ANVL_VALUE_OBJECT) {
      return stmt->value->object.statements;
   }
   return NULL;
}

// True if `fields` already has a statement named `name`.
static bool fields_has_name(list fields, anvl_slice name) {
   usize count = List.size(fields);
   for (usize i = 0; i < count; i++) {
      anvl_statement s = NULL;
      List.get(fields, i, (object *)&s);
      if (s && slices_equal(s->name, name)) {
         return true;
      }
   }
   return false;
}

// Merges inherited fields into `stmt`'s own field list, in place — appending an *aliased*
// pointer (never a copy) for each of an ancestor's fields not already present by name in
// `stmt`'s current set, walking the base chain nearest-ancestor-first so a closer override
// always wins over a farther one, and a transitive chain (`c : b : a`) falls out naturally
// without needing recursion. Mutating `stmt`'s own list directly, rather than building a
// separate merged view, was a deliberate call: two representations of the same field (an
// original and a shadow copy in some other list) is exactly the risk to avoid, not something
// to introduce here — after this runs, `stmt`'s own field list *is* its complete, correct
// field set.
//
// Cycle-safe via the same hop-counter technique resolve_varref_chain uses: since `base` is a
// single slice per statement, the "inherits from" relation never branches, so with N distinct
// names in `identifiers`, a non-cyclic chain can't need more than N hops — exceeding that is a
// cycle. Unlike a VarRef, an inheritance cycle is a hard error here (ANVL_ERR_RESOLVER_CYCLE_
// DETECTED) — a cyclic chain can't produce a sensible merged object at all, matching the
// legacy anvl.bak resolver's own policy on inheritance cycles specifically.
static anvl_result merge_inherited_fields(module_context ctx, anvl_statement stmt,
                                          anvl_err_code *out_err_code) {
   list fields = own_fields(stmt);
   if (!fields) {
      return ANVL_RES_OK;
   }

   usize max_hops = Map.count(ctx->identifiers) + 1;
   anvl_slice current_base = stmt->base;

   for (usize hop = 0; hop < max_hops; hop++) {
      usize len = Source.slice_length(current_base);
      addr val = 0;
      if (!Map.get(ctx->identifiers, current_base.start, len, &val)) {
         return ANVL_RES_OK; // unreachable - validate_bases already confirmed this exists
      }
      anvl_statement ancestor = (anvl_statement)val;
      list ancestor_fields = own_fields(ancestor);
      if (ancestor_fields) {
         usize count = List.size(ancestor_fields);
         for (usize i = 0; i < count; i++) {
            anvl_statement field = NULL;
            List.get(ancestor_fields, i, (object *)&field);
            if (field && !fields_has_name(fields, field->name)) {
               List.append(fields, field);
            }
         }
      }
      if (Source.slice_length(ancestor->base) == 0) {
         return ANVL_RES_OK; // reached the root of the chain
      }
      current_base = ancestor->base;
   }

   anvl_error_set(ctx->errors, ANVL_ERR_RESOLVER_CYCLE_DETECTED, 0, 0, __FILE__, NULL);
   *out_err_code = ANVL_ERR_RESOLVER_CYCLE_DETECTED;
   return ANVL_RES_ERR;
}

// Runs merge_inherited_fields for every statement in the context (any nesting depth) that has
// a non-empty base.
static anvl_result merge_all_inheritance(module_context ctx, anvl_err_code *out_err_code) {
   usize stmt_count = List.size(ctx->statements);
   for (usize i = 0; i < stmt_count; i++) {
      anvl_statement stmt = NULL;
      List.get(ctx->statements, i, (object *)&stmt);
      if (!stmt || Source.slice_length(stmt->base) == 0) {
         continue;
      }
      if (ANVL_RES_OK != merge_inherited_fields(ctx, stmt, out_err_code)) {
         return ANVL_RES_ERR;
      }
   }
   return ANVL_RES_OK;
}

// Chases a VarRef target to its final concrete value, following a chain of VarRefs
// (`a := $b; b := $c;` resolves all the way to c's value, not just one hop to b's).
// Cycle-safe without a visited set: with N distinct top-level names in ctx->identifiers, any
// non-cyclic chase terminates (hits a concrete value or a miss) within N hops, since each hop
// consumes one distinct name — so a chase still going after N+1 hops must have revisited a
// name, i.e. a cycle. Returns NULL for a missing target, a cycle, or a target that names an
// anonymous (OBJECT_BLOCK) statement (no value of its own to alias) — none of those are
// errors for a VarRef, only an unset `.varref.resolved`.
static anvl_value resolve_varref_chain(module_context ctx, anvl_slice target) {
   usize max_hops = Map.count(ctx->identifiers) + 1;
   anvl_slice current = target;

   for (usize hop = 0; hop < max_hops; hop++) {
      usize len = Source.slice_length(current);
      addr val = 0;
      if (!Map.get(ctx->identifiers, current.start, len, &val)) {
         return NULL; // missing target
      }
      anvl_statement stmt = (anvl_statement)val;
      if (stmt->kind == ANVL_STMT_OBJECT_BLOCK || !stmt->value) {
         return NULL; // anonymous object - no value to alias
      }
      if (stmt->value->type != ANVL_VALUE_VARREF) {
         return stmt->value; // final concrete value
      }
      current = stmt->value->varref.target; // keep chasing
   }
   return NULL; // exceeded max_hops without terminating - a cycle
}

// Finds every ANVL_VALUE_VARREF in one flat pass over ctx->values (already indexes every
// anvl_value at any nesting depth - see mod_ctx_dispose's own use of the same index) and sets
// .varref.resolved on each. Never fails - see resolve_varref_chain.
static void resolve_varrefs(module_context ctx) {
   usize value_count = List.size(ctx->values);
   for (usize i = 0; i < value_count; i++) {
      anvl_value v = NULL;
      List.get(ctx->values, i, (object *)&v);
      if (!v || v->type != ANVL_VALUE_VARREF) {
         continue;
      }
      v->varref.resolved = resolve_varref_chain(ctx, v->varref.target);
   }
}

anvl_result mod_resolve_context(module_context ctx, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!ctx) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      if (out_err_code) {
         *out_err_code = err_code;
      }
      return ANVL_RES_ERR;
   }

   if (ANVL_RES_OK != build_identifiers(ctx, &err_code)) {
      if (out_err_code) {
         *out_err_code = err_code;
      }
      return ANVL_RES_ERR;
   }

   if (ANVL_RES_OK != validate_bases(ctx, &err_code)) {
      if (out_err_code) {
         *out_err_code = err_code;
      }
      return ANVL_RES_ERR;
   }

   if (ANVL_RES_OK != merge_all_inheritance(ctx, &err_code)) {
      if (out_err_code) {
         *out_err_code = err_code;
      }
      return ANVL_RES_ERR;
   }

   resolve_varrefs(ctx);

   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_OK;
}
