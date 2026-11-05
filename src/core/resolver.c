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
 * Runs once, after every document in the include graph has been through  *
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Builds ctx->identifiers: top-level statement name -> anvl_statement, across every document
// in ctx->docs. Only doc->body (a document's own top-level list) is walked, never the flat
// ctx->statements index — that index also carries nested statements, which are never valid
// resolution targets. A duplicate top-level name anywhere in the context — same document or
// across a merged include — is a hard error (see notes/resolution-phase.md "Duplicate names are
// a hard error, full stop").
// Line and column (both 1-based, counted from the start of the file, the way the parser reports
// them) of a pointer into one of the context's source buffers. False if it points into none.
static bool locate_in_sources(module_context ctx, const char *p, usize *line, usize *col,
                              const char **file) {
   for (usize d = 0, n = List.size(ctx->docs); d < n; d++) {
      module_document doc = NULL;
      List.get(ctx->docs, d, (object *)&doc);
      if (!doc || !doc->source) {
         continue;
      }
      const char *base = (const char *)Source.data(doc->source);
      usize len = Source.length(doc->source);
      if (!base || p < base || p > base + len) {
         continue;
      }
      usize l = 1, c = 1;
      for (const char *q = base; q < p; q++) {
         if (*q == '\n') {
            l++;
            c = 1;
         } else {
            c++;
         }
      }
      *line = l;
      *col = c;
      if (file) {
         *file = doc->filepath;
      }
      return true;
   }
   return false;
}

static anvl_result build_identifiers(module_context ctx, anvl_err_code *out_err_code) {
   if (!ctx->identifiers) {
      ctx->identifiers = Map.new(ANVL_CTX_DEFAULT_MAP_CAP);
      if (!ctx->identifiers) {
         anvl_error_set(ctx->errors, ANVL_ERR_MEMORY_ALLOC_FAILED, 0, 0, NULL, NULL);
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
            continue; // VARS/IMPORT - no declared name, never a resolvable target
         }
         if (Map.has(ctx->identifiers, stmt->name.start, name_len)) {
            // Report the SECOND occurrence (this statement), and name the first one's line.
            usize line = 0, col = 0, first_line = 0, first_col = 0;
            const char *file = NULL;
            (void)locate_in_sources(ctx, stmt->name.start, &line, &col, &file);
            addr first = 0;
            Map.get(ctx->identifiers, stmt->name.start, name_len, &first);
            bool have_first = first &&
               locate_in_sources(ctx, ((anvl_statement)first)->name.start, &first_line, &first_col, NULL);
            char msg[256];
            if (have_first) {
               snprintf(msg, sizeof(msg),
                        "Duplicate top-level identifier '%.*s' (first declared at line %zu)",
                        (int)(name_len > 100 ? 100 : name_len), stmt->name.start, first_line);
            } else {
               snprintf(msg, sizeof(msg), "Duplicate top-level identifier '%.*s'",
                        (int)(name_len > 100 ? 100 : name_len), stmt->name.start);
            }
            anvl_error_set_message(ctx->errors, ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER, line, col,
                                   file, msg, NULL);
            *out_err_code = ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER;
            return ANVL_RES_ERR;
         }
         Map.set(ctx->identifiers, stmt->name.start, name_len, (addr)stmt);
      }
   }
   return ANVL_RES_OK;
}

// Validates every `base` in the context (via the flat ctx->statements index) against
// ctx->identifiers. Only a top-level statement can have a base — the parser rejects one on a nested
// statement (ANVL_ERR_PARSER_NESTED_BASE) — so a base here is always a top-level one, and a base can
// never name a statement that contains the one using it. A base naming nothing in the map is ANVL_ERR_RESOLVER_MISSING_BASE; a
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
         anvl_error_set(ctx->errors, ANVL_ERR_RESOLVER_MISSING_BASE, 0, 0, NULL, NULL);
         *out_err_code = ANVL_ERR_RESOLVER_MISSING_BASE;
         return ANVL_RES_ERR;
      }
      anvl_statement base_stmt = (anvl_statement)val;
      if (base_stmt->kind == ANVL_STMT_OBJECT_BLOCK) {
         anvl_error_set(ctx->errors, ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS, 0, 0, NULL,
                        NULL);
         *out_err_code = ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS;
         return ANVL_RES_ERR;
      }
      // A base must itself be an object: a scalar, string, array, tuple, blob, VarRef or function
      // has no fields to merge, so inheriting from one is the inheritance error (4020), reported
      // at the base name in the deriving statement.
      if (!base_stmt->value || base_stmt->value->type != ANVL_VALUE_OBJECT) {
         usize line = 0, col = 0;
         const char *file = NULL;
         (void)locate_in_sources(ctx, stmt->base.start, &line, &col, &file);
         anvl_error_set(ctx->errors, ANVL_ERR_PARSER_INHERITANCE_REQUIRES_OBJECT, line, col, file,
                        NULL);
         *out_err_code = ANVL_ERR_PARSER_INHERITANCE_REQUIRES_OBJECT;
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

   anvl_error_set(ctx->errors, ANVL_ERR_RESOLVER_CYCLE_DETECTED, 0, 0, NULL, NULL);
   *out_err_code = ANVL_ERR_RESOLVER_CYCLE_DETECTED;
   return ANVL_RES_ERR;
}

// Runs merge_inherited_fields for every statement in the context that has a non-empty base (only
// top-level statements do; the parser rejects a base on a nested one).
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
// name, i.e. a cycle. Returns NULL for a missing target or a cycle — neither is an error for a
// VarRef, only an unset `.varref.resolved`. A block-form (OBJECT_BLOCK) target resolves like any
// other: the parser gives it an OBJECT value of its own, and a reference is a read (a block cannot
// be a base, but it can be read — BR-2610-anvl-resolver-001).
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
      if (!stmt->value) {
         return NULL; // nothing to alias
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
static usize resolve_varrefs(module_context ctx) {
   usize resolved = 0;
   usize value_count = List.size(ctx->values);
   for (usize i = 0; i < value_count; i++) {
      anvl_value v = NULL;
      List.get(ctx->values, i, (object *)&v);
      if (!v || v->type != ANVL_VALUE_VARREF) {
         continue;
      }
      v->varref.resolved = resolve_varref_chain(ctx, v->varref.target);
      resolved += v->varref.resolved != NULL;
   }
   return resolved;
}

/* ---------------------------------------------------------------------- *
 * Reference cycles (BR-2611-anvl-resolver-002)
 *
 * A `$` reference that leads back to a container holding it is an error. Treat the resolved document as a
 * graph. A walker standing on a value reaches (1) a container's own members (object fields, array and tuple
 * elements), (2) the fields it inherits from a base, which merge_inherited_fields aliased into the member list
 * so they are members too, and (3) through a resolved `$` reference, the target statement's value. A cycle in
 * that graph is the error. A plain reference chain cycle (`a := $b; b := $a;`) is not part of it: it stays
 * unresolved (`.resolved` NULL), so a walker has no edge to follow.
 *
 * The graph collapses to the top-level statements. A reference always resolves to the value of one top-level
 * statement (the end of its chain), so a cycle must run through such values: statement S has an edge to
 * statement T for every resolved reference in S's walkable subtree (members and inherited fields, but not
 * through another reference) whose final target is T's value. A cycle exists exactly when some edge S -> T has
 * T able to reach S again (T == S included): both ends then lie in one strongly connected component.
 *
 * WHICH REFERENCE IS REPORTED. The error is at the `$` of one reference, chosen by this rule: among all the
 * references that lie on a cycle (a reference counts when some statement S whose walkable subtree holds it,
 * its own or through an inherited field, has the edge S -> T on a cycle), the first in source order, the
 * documents taken in load order (ctx->docs: the root is document 0, then the includes in the pre-order of the
 * dump's `doc` records) and, inside a document, by byte offset. The rule does not depend on how the search
 * visits the statements.
 *
 * Cost: time and memory O(size of the document + the merged member lists). Every reference is visited once for its
 * statement and once for each inherited field that holds it; a field a statement inherits from a base is a vertex of
 * its own (the same field value is in the member list of every statement that derives from it), holding the references
 * of its subtree once, and a deriving statement has one edge to it, never a copy of its references. The vertices are the
 * statements and those fields; the edges are the references (at most twice) and one per inherited field per deriving
 * statement, so at most the document plus the merged member count. The components are found iteratively (Tarjan);
 * nothing recurses, so a chain of 100000 statements is as safe as one of three. The counts are 32 bits and checked: a
 * graph of 4 billion vertices or edges is refused as out of memory, never wrapped. Documents with no resolved
 * reference skip the pass.
 * ---------------------------------------------------------------------- */
_Thread_local usize anvl_resolver_last_edges;

#define RC_NONE UINT32_MAX

typedef struct {
   anvl_value value; // a top-level statement's value
   uint32_t doc;     // index in ctx->docs
   bool derived;     // the statement has a base: its member list holds aliased fields of its bases
} rc_node;

typedef struct {
   anvl_value ref;  // the VarRef value (its text.start is the `$`), or NULL for an edge from a statement to a field it inherits
   uint32_t from;   // graph index of the statement, or of the inherited field, that holds the reference
   uint32_t target; // graph index of the statement whose value the reference resolves to, or of the inherited field
} rc_edge;

typedef struct {
   anvl_value key; // the value of a field of a statement that has a base
   uint32_t node;  // its graph index (a vertex of its own, holding the references of its subtree), RC_NONE if it holds none
} rc_cached;

typedef struct {
   module_context ctx;
   rc_node *nodes;
   uint32_t n;      // statements; the vertices after them are inherited fields
   uint32_t total;  // statements + field vertices
   uint32_t *adj;   // edge indices grouped by `from`; start[v] .. start[v + 1] are the edges out of vertex v
   uint32_t *table;  // open addressing: a concrete (non-reference) value -> its node index + 1, 0 for a free slot
   usize table_cap;  // a power of two
   uint32_t *start;  // total + 1 offsets into adj
   rc_edge *edges;
   usize edge_count, edge_cap;
   uint32_t *comp;   // node -> strongly connected component
   anvl_value best;  // the reference to report so far
   usize best_doc;
   rc_cached *cache; // open addressing: a field's value -> its vertex, so an aliased field is walked once
   usize cache_cap, cache_count;
} rc_graph;

static usize rc_hash(const void *p, usize cap) {
   return (usize)((((unsigned long long)(usize)p >> 4) * 0x9E3779B97F4A7C15ull) >> 24) & (cap - 1);
}

// The node whose value is `value`, RC_NONE if none. A resolved reference points at the concrete value that the end
// of its chain of references holds (resolve_varref_chain), which is the value of one top-level statement.
static uint32_t rc_node_of(const rc_graph *g, const void *value) {
   for (usize at = rc_hash(value, g->table_cap); g->table[at]; at = (at + 1) & (g->table_cap - 1)) {
      if (g->nodes[g->table[at] - 1].value == value) {
         return g->table[at] - 1;
      }
   }
   return RC_NONE;
}

// Index in ctx->docs of the document whose source buffer holds p. `hint` is tried first.
static usize rc_doc_of(module_context ctx, const char *p, usize hint) {
   usize count = List.size(ctx->docs);
   for (usize k = 0; k < count; k++) {
      usize d = k == 0 ? hint : (k - 1 < hint ? k - 1 : k);
      module_document doc = NULL;
      List.get(ctx->docs, d, (object *)&doc);
      const char *base = doc && doc->source ? (const char *)Source.data(doc->source) : NULL;
      if (base && p >= base && p <= base + Source.length(doc->source)) {
         return d;
      }
   }
   return RC_NONE;
}

static bool rc_add_edge(rc_graph *g, uint32_t from, anvl_value ref, uint32_t target) {
   if (g->edge_count >= UINT32_MAX - 1) {
      return false; // the counts are 32 bits: a graph this size is refused as out of memory, never wrapped
   }
   if (g->edge_count == g->edge_cap) {
      usize cap = g->edge_cap ? g->edge_cap * 2 : 64;
      rc_edge *bigger = realloc(g->edges, cap * sizeof *bigger);
      if (!bigger) {
         return false;
      }
      g->edges = bigger;
      g->edge_cap = cap;
   }
   g->edges[g->edge_count++] = (rc_edge){ref, from, target};
   return true;
}

// Adds an edge for each resolved reference in the walkable subtree of node i (members and elements at any depth,
// inherited fields; not through another reference). `stack` is a reusable scratch; false means out of memory.
static bool rc_scan_value(rc_graph *g, uint32_t from, anvl_value root, anvl_value **stack, usize *cap) {
   usize top = 0;
   (*stack)[top++] = root;
   while (top > 0) {
      anvl_value v = (*stack)[--top];
      list items = NULL;
      bool is_object = false;
      switch (v->type) {
      case ANVL_VALUE_VARREF: {
         uint32_t t = v->varref.resolved ? rc_node_of(g, v->varref.resolved) : RC_NONE;
         if (t != RC_NONE && !rc_add_edge(g, from, v, t)) {
            return false;
         }
         continue;
      }
      case ANVL_VALUE_ARRAY:
      case ANVL_VALUE_TUPLE:
         items = v->collection.items;
         break;
      case ANVL_VALUE_OBJECT:
         items = v->object.statements;
         is_object = true;
         break;
      default:
         continue;
      }
      usize count = items ? List.size(items) : 0;
      if (top + count > *cap) {
         usize grown = (top + count) * 2;
         anvl_value *bigger = realloc(*stack, grown * sizeof **stack);
         if (!bigger) {
            return false;
         }
         *stack = bigger;
         *cap = grown;
      }
      for (usize k = 0; k < count; k++) {
         object item = NULL;
         List.get(items, k, (object *)&item);
         anvl_value child = !item ? NULL : is_object ? ((anvl_statement)item)->value : (anvl_value)item;
         if (child) {
            (*stack)[top++] = child;
         }
      }
   }
   return true;
}

// The slot for field value v: the one holding it, or the empty one where it belongs. NULL when the table is empty.
static rc_cached *rc_cache_slot(const rc_graph *g, anvl_value v) {
   if (!g->cache_cap) {
      return NULL;
   }
   usize at = rc_hash(v, g->cache_cap);
   while (g->cache[at].key && g->cache[at].key != v) {
      at = (at + 1) & (g->cache_cap - 1);
   }
   return &g->cache[at];
}

static bool rc_cache_put(rc_graph *g, anvl_value v, uint32_t node) {
   if (g->cache_count * 2 >= g->cache_cap) {
      rc_cached *old = g->cache;
      usize old_cap = g->cache_cap;
      g->cache_cap = old_cap ? old_cap * 2 : 64;
      g->cache = calloc(g->cache_cap, sizeof *g->cache);
      if (!g->cache) {
         g->cache = old;
         g->cache_cap = old_cap;
         return false;
      }
      for (usize k = 0; k < old_cap; k++) {
         if (old[k].key) {
            *rc_cache_slot(g, old[k].key) = old[k];
         }
      }
      free(old);
   }
   *rc_cache_slot(g, v) = (rc_cached){v, node};
   g->cache_count++;
   return true;
}

// Adds the edges of statement i. A statement with a base lists fields it inherited, aliased: the same field value is in
// the member list of every statement that derives from it. Each such field is a vertex of its own, holding the
// references of its subtree (walked once); a deriving statement has one edge to it, not a copy of its references. A
// field with no reference has no vertex.
static bool rc_scan(rc_graph *g, uint32_t i, anvl_value **stack, usize *cap) {
   anvl_value root = g->nodes[i].value;
   if (!g->nodes[i].derived || root->type != ANVL_VALUE_OBJECT || !root->object.statements) {
      return rc_scan_value(g, i, root, stack, cap);
   }
   list fields = root->object.statements;
   for (usize k = 0, n = List.size(fields); k < n; k++) {
      anvl_statement f = NULL;
      List.get(fields, k, (object *)&f);
      if (!f || !f->value) {
         continue;
      }
      rc_cached *c = rc_cache_slot(g, f->value);
      uint32_t node = RC_NONE;
      if (c && c->key == f->value) {
         node = c->node;
      } else {
         usize before = g->edge_count;
         if (g->total >= UINT32_MAX - 2) {
            return false;
         }
         node = g->total++;
         if (!rc_scan_value(g, node, f->value, stack, cap)) {
            return false;
         }
         if (g->edge_count == before) { // no reference in it: no vertex
            g->total--;
            node = RC_NONE;
         }
         if (!rc_cache_put(g, f->value, node)) {
            return false;
         }
      }
      if (node != RC_NONE && !rc_add_edge(g, i, NULL, node)) {
         return false;
      }
   }
   return true;
}

// Fills g->comp with the strongly connected components: Tarjan's algorithm, iterative (an explicit call stack), so a
// chain of 100000 statements does not recurse. Two nodes are in one component exactly when each can reach the other.
static bool rc_components(rc_graph *g) {
   usize n = g->total;
   uint32_t *mem = calloc(n ? 5 * n : 1, sizeof *mem);
   if (!mem) {
      return false;
   }
   uint32_t *order = mem, *low = mem + n, *cursor = mem + 2 * n, *stack = mem + 3 * n, *calls = mem + 4 * n;
   uint32_t next_order = 1, next_comp = 0;
   usize sp = 0;
   for (usize i = 0; i < n; i++) {
      g->comp[i] = RC_NONE;
   }
   for (uint32_t root = 0; root < n; root++) {
      usize depth = 0;
      if (order[root]) {
         continue;
      }
      order[root] = low[root] = next_order++;
      cursor[root] = g->start[root];
      calls[depth++] = stack[sp++] = root;
      while (depth > 0) {
         uint32_t v = calls[depth - 1];
         if (cursor[v] < g->start[v + 1]) {
            uint32_t w = g->edges[g->adj[cursor[v]++]].target;
            if (!order[w]) {
               order[w] = low[w] = next_order++;
               cursor[w] = g->start[w];
               calls[depth++] = stack[sp++] = w;
            } else if (g->comp[w] == RC_NONE && order[w] < low[v]) { // w is on Tarjan's stack
               low[v] = order[w];
            }
            continue;
         }
         if (low[v] == order[v]) {
            for (uint32_t w = RC_NONE; w != v;) {
               g->comp[w = stack[--sp]] = next_comp;
            }
            next_comp++;
         }
         depth--;
         if (depth > 0 && low[v] < low[calls[depth - 1]]) {
            low[calls[depth - 1]] = low[v];
         }
      }
   }
   free(mem);
   return true;
}

// ANVL_RES_ERR with *out_err_code set when a reference leads back to a container holding it, or on running out of
// memory. The error is at the `$` of the first reference that lies on a cycle: the documents in load order, then the
// byte offset in a document.
static anvl_result check_reference_cycles(module_context ctx, anvl_err_code *out_err_code) {
   rc_graph g = {.ctx = ctx};
   anvl_value *stack = NULL;
   usize stack_cap = 64;
   usize capacity = 0, doc_count = List.size(ctx->docs);
   for (usize d = 0; d < doc_count; d++) {
      module_document doc = NULL;
      List.get(ctx->docs, d, (object *)&doc);
      capacity += doc && doc->body ? (usize)FArray.capacity(doc->body, sizeof(anvl_statement)) : 0;
   }
   for (g.table_cap = 16; g.table_cap < capacity * 2;) {
      g.table_cap *= 2;
   }
   g.nodes = malloc((capacity ? capacity : 1) * sizeof *g.nodes);
   g.table = calloc(g.table_cap, sizeof *g.table);
   stack = malloc(stack_cap * sizeof *stack);
   bool ok = g.nodes && g.table && stack;
   for (usize d = 0; ok && d < doc_count; d++) {
      module_document doc = NULL;
      List.get(ctx->docs, d, (object *)&doc);
      for (usize i = 0; doc && doc->body && i < (usize)FArray.capacity(doc->body, sizeof(anvl_statement)); i++) {
         anvl_statement stmt = NULL;
         FArray.get(doc->body, (int)i, sizeof(anvl_statement), (object *)&stmt);
         if (!stmt || !stmt->value || Source.slice_length(stmt->name) == 0) {
            continue;
         }
         g.nodes[g.n] = (rc_node){stmt->value, (uint32_t)d, Source.slice_length(stmt->base) != 0};
         if (stmt->value->type != ANVL_VALUE_VARREF) { // a reference resolves to a concrete value, never to a reference
            usize at = rc_hash(stmt->value, g.table_cap);
            while (g.table[at]) {
               at = (at + 1) & (g.table_cap - 1);
            }
            g.table[at] = g.n + 1;
         }
         g.n++;
      }
   }
   g.total = g.n;
   for (uint32_t i = 0; ok && i < g.n; i++) {
      ok = rc_scan(&g, i, &stack, &stack_cap);
   }
   // group the edges by the vertex they leave (counting sort), then find the components of statements and inherited fields
   g.start = ok ? calloc((usize)g.total + 1, sizeof *g.start) : NULL;
   g.adj = ok ? malloc((g.edge_count ? g.edge_count : 1) * sizeof *g.adj) : NULL;
   g.comp = ok ? malloc((g.total ? g.total : 1) * sizeof *g.comp) : NULL;
   ok = ok && g.start && g.adj && g.comp;
   for (usize e = 0; ok && e < g.edge_count; e++) {
      g.start[g.edges[e].from + 1]++;
   }
   for (usize v = 0; ok && v < g.total; v++) {
      g.start[v + 1] += g.start[v];
   }
   for (usize e = 0; ok && e < g.edge_count; e++) {
      g.adj[g.start[g.edges[e].from]++] = (uint32_t)e;
   }
   for (usize v = g.total; ok && v > 0; v--) { // the fill moved every start[v] to the end of its group: shift back
      g.start[v] = g.start[v - 1];
   }
   if (ok) {
      g.start[0] = 0;
      ok = rc_components(&g);
   }
   // a reference whose holder (its statement, or the inherited field that holds it) is in the component of its target
   // closes a cycle (a self edge included)
   for (usize e = 0; ok && e < g.edge_count; e++) {
      anvl_value ref = g.edges[e].ref;
      if (!ref || g.comp[g.edges[e].from] != g.comp[g.edges[e].target]) {
         continue;
      }
      usize d = rc_doc_of(ctx, ref->text.start, g.edges[e].from < g.n ? g.nodes[g.edges[e].from].doc : 0);
      if (!g.best || d < g.best_doc || (d == g.best_doc && ref->text.start < g.best->text.start)) {
         g.best = ref;
         g.best_doc = d;
      }
   }
   free(stack);
   free(g.nodes);
   free(g.table);
   free(g.start);
   free(g.adj);
   free(g.comp);
   free(g.edges);
   free(g.cache);
   anvl_resolver_last_edges = g.edge_count;
   if (!ok) {
      anvl_error_set(ctx->errors, ANVL_ERR_MEMORY_ALLOC_FAILED, 0, 0, NULL, NULL);
      *out_err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      return ANVL_RES_ERR;
   }
   if (!g.best) {
      return ANVL_RES_OK;
   }
   usize line = 0, col = 0;
   const char *file = NULL;
   (void)locate_in_sources(ctx, g.best->text.start, &line, &col, &file);
   anvl_error_set(ctx->errors, ANVL_ERR_RESOLVER_REFERENCE_CYCLE, line, col, file, NULL);
   *out_err_code = ANVL_ERR_RESOLVER_REFERENCE_CYCLE;
   return ANVL_RES_ERR;
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

   // A reference that leads back to a container holding it is an error. With no resolved reference there is
   // nothing to follow, so the check is skipped.
   if (resolve_varrefs(ctx) > 0 && ANVL_RES_OK != check_reference_cycles(ctx, &err_code)) {
      if (out_err_code) {
         *out_err_code = err_code;
      }
      return ANVL_RES_ERR;
   }

   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_OK;
}
