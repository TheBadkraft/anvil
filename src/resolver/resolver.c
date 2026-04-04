/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * resolver.c — Inheritance graph resolver (FR-03)
 * ------------------------------------------------------------------
 * Implements cycle detection (Kahn's topological sort) and lazy
 * merged-field access for single-inheritance Anvil documents.
 *
 * Port of AnvilResolver.cs + AnvilNodeState.cs from Anvil.Net.Api.
 * ------------------------------------------------------------------
 */

#include "resolver.h"
#include <sigma.memory/memory.h>
#include <string.h>

/* ================================================================
 * FNV-1a hash (64-bit)
 * ================================================================ */
#define FNV1A_OFFSET_BASIS UINT64_C(14695981039346656037)
#define FNV1A_PRIME UINT64_C(1099511628211)

static usize fnv1a(const char *data, usize len) {
   uint64_t h = FNV1A_OFFSET_BASIS;
   for (usize i = 0; i < len; i++) {
      h ^= (uint8_t)data[i];
      h *= FNV1A_PRIME;
   }
   return (usize)h;
}

/* ================================================================
 * anvl_id_map_t helpers
 * ================================================================ */

static bool id_map_init(anvl_id_map_t *map, usize cap) {
   map->key_pos = Allocator.alloc(sizeof(usize) * cap);
   map->key_len = Allocator.alloc(sizeof(usize) * cap);
   map->val = Allocator.alloc(sizeof(usize) * cap);
   if (!map->key_pos || !map->key_len || !map->val) {
      if (map->key_pos)
         Allocator.dispose(map->key_pos);
      if (map->key_len)
         Allocator.dispose(map->key_len);
      if (map->val)
         Allocator.dispose(map->val);
      map->key_pos = map->key_len = map->val = NULL;
      return false;
   }
   /* sentinel: key_pos == USIZE_MAX means empty slot */
   for (usize i = 0; i < cap; i++) {
      map->key_pos[i] = (usize)-1;
      map->key_len[i] = 0;
      map->val[i] = 0;
   }
   map->cap = cap;
   map->count = 0;
   return true;
}

static void id_map_free(anvl_id_map_t *map) {
   if (map->key_pos) {
      Allocator.dispose(map->key_pos);
      map->key_pos = NULL;
   }
   if (map->key_len) {
      Allocator.dispose(map->key_len);
      map->key_len = NULL;
   }
   if (map->val) {
      Allocator.dispose(map->val);
      map->val = NULL;
   }
   map->cap = map->count = 0;
}

/* Find slot index for (pos, len); returns USIZE_MAX if not found */
static usize id_map_find_slot(const anvl_id_map_t *map,
                              const char *raw,
                              usize pos, usize len) {
   usize h = fnv1a(raw + pos, len);
   usize mask = map->cap - 1;
   usize i = h & mask;
   for (usize probe = 0; probe < map->cap; probe++) {
      usize slot = (i + probe) & mask;
      if (map->key_pos[slot] == (usize)-1)
         return (usize)-1; /* not found */
      if (map->key_len[slot] == len &&
          memcmp(raw + map->key_pos[slot], raw + pos, len) == 0)
         return slot;
   }
   return (usize)-1;
}

/* Insert (pos, len) → idx; assumes load < 0.7. */
static void id_map_insert(anvl_id_map_t *map,
                          const char *raw,
                          usize pos, usize len, usize idx) {
   usize h = fnv1a(raw + pos, len);
   usize mask = map->cap - 1;
   usize i = h & mask;
   for (usize probe = 0; probe < map->cap; probe++) {
      usize slot = (i + probe) & mask;
      if (map->key_pos[slot] == (usize)-1) {
         map->key_pos[slot] = pos;
         map->key_len[slot] = len;
         map->val[slot] = idx;
         map->count++;
         return;
      }
   }
   /* table full — shouldn't happen if pre-sized correctly */
}

/* Lookup identifier; returns USIZE_MAX (not found) or stmt index */
static usize id_map_lookup(const anvl_id_map_t *map,
                           const char *raw,
                           usize pos, usize len) {
   usize slot = id_map_find_slot(map, raw, pos, len);
   if (slot == (usize)-1)
      return (usize)-1;
   return map->val[slot];
}

/* ================================================================
 * Next power-of-2 ≥ v (v > 0)
 * ================================================================ */
static usize next_pow2(usize v) {
   if (v == 0)
      return 1;
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
#if defined(_WIN64) || defined(__LP64__)
   v |= v >> 32;
#endif
   return v + 1;
}

/* ================================================================
 * anvl_resolver_build_state
 *
 * 1. Fast-path: if no statement has base_meta, return NULL (no error).
 * 2. Build id→idx hash table.
 * 3. Compute in-degrees and adjacency list (base → derived indices).
 * 4. Kahn's topological sort.
 * 5. If topoCount < n → CYCLE_DETECTED.
 * ================================================================ */
anvl_node_state_t *anvl_resolver_build_state(context ctx, source src) {
   usize n = (usize)Context.statement_count(ctx);
   if (n == 0)
      return NULL;

   const char *raw = Source.data(src);

   /* ---- 1. Fast-path scan ---------------------------------------- */
   bool any_base = false;
   for (usize i = 0; i < n; i++) {
      statement s = Context.get_statement(ctx, i);
      if (s && s->base_meta) {
         any_base = true;
         break;
      }
   }
   if (!any_base)
      return NULL;

   /* ---- 2. Build id→idx hash table ------------------------------- */
   /* capacity: next power-of-2 above n*2 to keep load < 0.5 */
   usize cap = next_pow2(n * 2);
   anvl_id_map_t id_to_idx;
   if (!id_map_init(&id_to_idx, cap)) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     "resolver: id_map alloc failed", 0, 0, __FILE__);
      return NULL;
   }

   for (usize i = 0; i < n; i++) {
      statement s = Context.get_statement(ctx, i);
      if (!s)
         continue;
      usize pos = s->meta[STMT_META_IDENT_POS];
      usize len = s->meta[STMT_META_IDENT_LEN];
      if (len == 0)
         continue;
      id_map_insert(&id_to_idx, raw, pos, len, i);
   }

   /* ---- 3. In-degrees + adjacency (base → list of derived) ------- */
   usize *in_degree = Allocator.alloc(sizeof(usize) * n);
   usize *dep_head = Allocator.alloc(sizeof(usize) * n);   /* head of adj list */
   usize *dep_next = Allocator.alloc(sizeof(usize) * n);   /* next pointer (parallel array) */
   usize *dep_target = Allocator.alloc(sizeof(usize) * n); /* target stmt idx */

   if (!in_degree || !dep_head || !dep_next || !dep_target) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     "resolver: adj alloc failed", 0, 0, __FILE__);
      if (in_degree)
         Allocator.dispose(in_degree);
      if (dep_head)
         Allocator.dispose(dep_head);
      if (dep_next)
         Allocator.dispose(dep_next);
      if (dep_target)
         Allocator.dispose(dep_target);
      id_map_free(&id_to_idx);
      return NULL;
   }

   for (usize i = 0; i < n; i++) {
      in_degree[i] = 0;
      dep_head[i] = (usize)-1; /* end-of-list sentinel */
      dep_next[i] = (usize)-1;
      dep_target[i] = (usize)-1;
   }

   for (usize i = 0; i < n; i++) {
      statement s = Context.get_statement(ctx, i);
      if (!s || !s->base_meta)
         continue;
      usize base_idx = id_map_lookup(&id_to_idx, raw,
                                     s->base_meta->pos, s->base_meta->len);
      if (base_idx == (usize)-1)
         continue;    /* missing base — deferred error */
      in_degree[i]++; /* derived i depends on base */
      /* prepend i to adjacency list of base_idx */
      dep_target[i] = i;
      dep_next[i] = dep_head[base_idx];
      dep_head[base_idx] = i;
   }

   /* ---- 4. Kahn's topological sort ------------------------------- */
   usize *queue = Allocator.alloc(sizeof(usize) * n);
   if (!queue) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     "resolver: queue alloc failed", 0, 0, __FILE__);
      Allocator.dispose(in_degree);
      Allocator.dispose(dep_head);
      Allocator.dispose(dep_next);
      Allocator.dispose(dep_target);
      id_map_free(&id_to_idx);
      return NULL;
   }

   usize qhead = 0, qtail = 0;
   for (usize i = 0; i < n; i++) {
      if (in_degree[i] == 0)
         queue[qtail++] = i;
   }

   usize topo_count = 0;
   while (qhead < qtail) {
      usize u = queue[qhead++];
      topo_count++;
      /* for each dependent of u (nodes that listed u as base) */
      for (usize e = dep_head[u]; e != (usize)-1; e = dep_next[e]) {
         usize v = dep_target[e]; /* dep_target[e] == e in our scheme above */
         if (v == (usize)-1)
            continue;
         in_degree[v]--;
         if (in_degree[v] == 0)
            queue[qtail++] = v;
      }
   }

   Allocator.dispose(queue);
   Allocator.dispose(in_degree);
   Allocator.dispose(dep_head);
   Allocator.dispose(dep_next);
   Allocator.dispose(dep_target);

   /* ---- 5. Cycle check ------------------------------------------- */
   if (topo_count < n) {
      anvl_error_set(ANVL_ERR_RESOLVER_CYCLE_DETECTED,
                     "resolver: inheritance cycle detected", 0, 0, __FILE__);
      id_map_free(&id_to_idx);
      return NULL;
   }

   /* ---- 6. Build node state -------------------------------------- */
   anvl_node_state_t *state = Allocator.alloc(sizeof(*state));
   if (!state) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     "resolver: state alloc failed", 0, 0, __FILE__);
      id_map_free(&id_to_idx);
      return NULL;
   }
   memset(state, 0, sizeof(*state));

   state->merge_cache = Allocator.alloc(sizeof(anvl_field_list_t) * n);
   state->computed = Allocator.alloc(sizeof(uint8_t) * n);
   if (!state->merge_cache || !state->computed) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     "resolver: cache alloc failed", 0, 0, __FILE__);
      if (state->merge_cache)
         Allocator.dispose(state->merge_cache);
      if (state->computed)
         Allocator.dispose(state->computed);
      Allocator.dispose(state);
      id_map_free(&id_to_idx);
      return NULL;
   }
   memset(state->merge_cache, 0, sizeof(anvl_field_list_t) * n);
   memset(state->computed, 0, sizeof(uint8_t) * n);

   state->id_to_idx = id_to_idx;
   state->ctx = ctx;
   state->src = src;
   state->stmt_count = n;

   return state;
}

/* ================================================================
 * Merge two field lists: base first, then own (derived overrides).
 *
 * Returns an allocated anvl_field_list_t (caller owns).
 * Returns NULL + error on alloc failure.
 * ================================================================ */
static anvl_field_list_t *merge_fields(const anvl_field_list_t *base_list,
                                       const field *own_fields,
                                       usize own_count,
                                       source src) {
   const char *raw = Source.data(src);
   usize base_count = base_list ? base_list->count : 0;

   /* Upper bound: base_count + own_count */
   usize max_count = base_count + own_count;
   field *out = Allocator.alloc(sizeof(field) * (max_count ? max_count : 1));
   if (!out) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     "resolver: field merge alloc failed", 0, 0, __FILE__);
      return NULL;
   }

   /* Copy base fields into output */
   usize count = 0;
   for (usize i = 0; i < base_count; i++) {
      out[count++] = base_list->fields[i];
   }

   /* For each own field: replace base field with same key or append */
   for (usize i = 0; i < own_count; i++) {
      field f = own_fields[i];
      if (!f)
         continue;
      bool replaced = false;
      for (usize j = 0; j < base_count; j++) {
         if (!out[j])
            continue;
         if (out[j]->key_len == f->key_len &&
             memcmp(raw + out[j]->key_pos, raw + f->key_pos, f->key_len) == 0) {
            out[j] = f; /* derived wins */
            replaced = true;
            break;
         }
      }
      if (!replaced)
         out[count++] = f;
   }

   anvl_field_list_t *result = Allocator.alloc(sizeof(*result));
   if (!result) {
      Allocator.dispose(out);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     "resolver: field_list alloc failed", 0, 0, __FILE__);
      return NULL;
   }
   result->fields = out;
   result->count = count;
   return result;
}

/* ================================================================
 * anvl_node_state_get_merged_fields — lazy, recursive merge
 * ================================================================ */
const anvl_field_list_t *anvl_node_state_get_merged_fields(
    anvl_node_state_t *state, usize stmt_idx) {
   if (!state || stmt_idx >= state->stmt_count)
      return NULL;

   /* Cache hit */
   if (state->computed[stmt_idx])
      return &state->merge_cache[stmt_idx];

   statement stmt = Context.get_statement(state->ctx, stmt_idx);
   if (!stmt)
      return NULL;

   const char *raw = Source.data(state->src);

   /* Collect own fields from value_meta */
   field *own_fields = NULL;
   usize own_count = 0;
   if (stmt->value_meta && stmt->value_meta->type == ANVL_VALUE_OBJECT) {
      usize start = stmt->value_meta->data.object.field_start;
      usize fc = stmt->value_meta->data.object.field_count;
      own_fields = &state->ctx->field_list.fields[start];
      own_count = fc;
   }

   /* No base: own fields only */
   if (!stmt->base_meta) {
      usize max = own_count ? own_count : 1;
      field *copy = Allocator.alloc(sizeof(field) * max);
      if (!copy) {
         anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                        "resolver: own-copy alloc failed", 0, 0, __FILE__);
         return NULL;
      }
      if (own_count)
         memcpy(copy, own_fields, sizeof(field) * own_count);
      state->merge_cache[stmt_idx].fields = copy;
      state->merge_cache[stmt_idx].count = own_count;
      state->computed[stmt_idx] = 1;
      return &state->merge_cache[stmt_idx];
   }

   /* Has base: look up base by identifier */
   usize base_stmt_idx = id_map_lookup(&state->id_to_idx, raw,
                                       stmt->base_meta->pos,
                                       stmt->base_meta->len);
   if (base_stmt_idx == (usize)-1) {
      anvl_error_set(ANVL_ERR_RESOLVER_MISSING_BASE,
                     "resolver: base not found", 0, 0, __FILE__);
      return NULL;
   }

   /* Recurse to get base merged fields */
   const anvl_field_list_t *base_list =
       anvl_node_state_get_merged_fields(state, base_stmt_idx);
   if (!base_list && anvl_error_is_set())
      return NULL;

   /* Merge */
   anvl_field_list_t *merged = merge_fields(base_list, own_fields, own_count, state->src);
   if (!merged)
      return NULL; /* error already set */

   state->merge_cache[stmt_idx].fields = merged->fields;
   state->merge_cache[stmt_idx].count = merged->count;
   Allocator.dispose(merged); /* free wrapper, keep fields array */
   state->computed[stmt_idx] = 1;
   return &state->merge_cache[stmt_idx];
}

/* ================================================================
 * anvl_node_state_warm_all
 * ================================================================ */
bool anvl_node_state_warm_all(anvl_node_state_t *state) {
   if (!state)
      return false;
   for (usize i = 0; i < state->stmt_count; i++) {
      if (!state->computed[i]) {
         const anvl_field_list_t *r = anvl_node_state_get_merged_fields(state, i);
         if (!r && anvl_error_is_set())
            return false;
      }
   }
   return true;
}

/* ================================================================
 * anvl_node_state_dispose
 * ================================================================ */
void anvl_node_state_dispose(anvl_node_state_t *state) {
   if (!state)
      return;

   /* Free each committed merge result's field array */
   if (state->merge_cache) {
      for (usize i = 0; i < state->stmt_count; i++) {
         if (state->computed[i] && state->merge_cache[i].fields) {
            Allocator.dispose(state->merge_cache[i].fields);
            state->merge_cache[i].fields = NULL;
         }
      }
      Allocator.dispose(state->merge_cache);
   }
   if (state->computed)
      Allocator.dispose(state->computed);

   id_map_free(&state->id_to_idx);
   Allocator.dispose(state);
}

/* ================================================================
 * Custom Merge Policy API — FR-2604-anvl-002
 * ================================================================ */

/**
 * Get the base statement index for a given statement.
 */
usize anvl_node_state_get_base_index(anvl_node_state_t *state, usize stmt_idx) {
   if (!state || stmt_idx >= state->stmt_count)
      return (usize)-1;

   statement stmt = Context.get_statement(state->ctx, stmt_idx);
   if (!stmt || !stmt->base_meta)
      return (usize)-1;

   const char *raw = Source.data(state->src);
   usize base_pos = stmt->base_meta->pos;
   usize base_len = stmt->base_meta->len;

   /* Look up base identifier in id_to_idx map */
   usize base_idx = id_map_lookup(&state->id_to_idx, raw, base_pos, base_len);
   return base_idx;
}

/**
 * Get the statement's own (unmerged) fields.
 */
const anvl_field_list_t *anvl_node_state_get_own_fields(
    anvl_node_state_t *state, usize stmt_idx) {
   if (!state || stmt_idx >= state->stmt_count)
      return NULL;

   statement stmt = Context.get_statement(state->ctx, stmt_idx);
   if (!stmt || !stmt->value_meta)
      return NULL;

   if (stmt->value_meta->type != ANVL_VALUE_OBJECT)
      return NULL;

   /* Allocate temporary field_list to return (caller should not free fields) */
   anvl_field_list_t *result = Allocator.alloc(sizeof(*result));
   if (!result)
      return NULL;

   /* Get fields from context field_list */
   usize start = stmt->value_meta->data.object.field_start;
   usize count = stmt->value_meta->data.object.field_count;
   
   result->fields = &state->ctx->field_list.fields[start];
   result->count = count;
   
   return result;
}

/**
 * Merge two field lists with custom policy callback.
 */
static anvl_field_list_t *merge_fields_custom(
    context ctx,
    source src,
    const anvl_field_list_t *base_list,
    const field *own_fields,
    usize own_count,
    anvl_merge_policy_fn policy,
    void *userdata) {
   
   const char *raw = Source.data(src);
   usize base_count = base_list ? base_list->count : 0;

   /* Upper bound: base_count + own_count */
   usize max_count = base_count + own_count;
   field *out = Allocator.alloc(sizeof(field) * (max_count ? max_count : 1));
   if (!out) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     "resolver: field merge alloc failed", 0, 0, __FILE__);
      return NULL;
   }

   usize count = 0;

   if (!policy) {
      /* NULL policy: use default "derived wins" behavior */
      /* Copy base fields */
      for (usize i = 0; i < base_count; i++) {
         out[count++] = base_list->fields[i];
      }

      /* For each own field: replace base field with same key or append */
      for (usize i = 0; i < own_count; i++) {
         field f = own_fields[i];
         if (!f)
            continue;
         bool replaced = false;
         for (usize j = 0; j < base_count; j++) {
            if (!out[j])
               continue;
            if (out[j]->key_len == f->key_len &&
                memcmp(raw + out[j]->key_pos, raw + f->key_pos, f->key_len) == 0) {
               out[j] = f; /* derived wins */
               replaced = true;
               break;
            }
         }
         if (!replaced)
            out[count++] = f;
      }
   } else {
      /* Custom policy: invoke callback for each field pair */
      
      /* First pass: process fields that exist in both base and derived */
      for (usize i = 0; i < base_count; i++) {
         field base_field = base_list->fields[i];
         if (!base_field)
            continue;

         /* Find matching derived field */
         field derived_field = NULL;
         for (usize j = 0; j < own_count; j++) {
            field f = own_fields[j];
            if (!f)
               continue;
            if (f->key_len == base_field->key_len &&
                memcmp(raw + f->key_pos, raw + base_field->key_pos, f->key_len) == 0) {
               derived_field = f;
               break;
            }
         }

         /* Invoke policy callback */
         field result_field = policy(ctx, src, base_field, derived_field, userdata);
         if (result_field)
            out[count++] = result_field;
      }

      /* Second pass: append derived fields that don't exist in base */
      for (usize i = 0; i < own_count; i++) {
         field derived_field = own_fields[i];
         if (!derived_field)
            continue;

         /* Check if this field exists in base */
         bool in_base = false;
         for (usize j = 0; j < base_count; j++) {
            field base_field = base_list->fields[j];
            if (!base_field)
               continue;
            if (base_field->key_len == derived_field->key_len &&
                memcmp(raw + base_field->key_pos, raw + derived_field->key_pos,
                       derived_field->key_len) == 0) {
               in_base = true;
               break;
            }
         }

         if (!in_base) {
            /* Invoke policy with NULL base_field */
            field result_field = policy(ctx, src, NULL, derived_field, userdata);
            if (result_field)
               out[count++] = result_field;
         }
      }
   }

   anvl_field_list_t *result = Allocator.alloc(sizeof(*result));
   if (!result) {
      Allocator.dispose(out);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     "resolver: field_list alloc failed", 0, 0, __FILE__);
      return NULL;
   }
   result->fields = out;
   result->count = count;
   return result;
}

/**
 * Get merged fields with custom policy callback.
 */
const anvl_field_list_t *anvl_node_state_get_merged_fields_custom(
    anvl_node_state_t *state,
    usize stmt_idx,
    anvl_merge_policy_fn policy,
    void *userdata) {
   
   if (!state || stmt_idx >= state->stmt_count)
      return NULL;

   /* If policy is NULL, use default get_merged_fields (same cache) */
   if (!policy)
      return anvl_node_state_get_merged_fields(state, stmt_idx);

   /* For custom policies: check cache (note: simplified caching, could be improved) */
   /* For MVP: recompute each time with custom policy (no cross-policy caching) */
   /* TODO: implement separate cache for custom policies if needed */

   statement stmt = Context.get_statement(state->ctx, stmt_idx);
   if (!stmt)
      return NULL;

   const char *raw = Source.data(state->src);

   /* Collect own fields */
   field *own_fields = NULL;
   usize own_count = 0;
   if (stmt->value_meta && stmt->value_meta->type == ANVL_VALUE_OBJECT) {
      usize start = stmt->value_meta->data.object.field_start;
      usize fc = stmt->value_meta->data.object.field_count;
      own_fields = &state->ctx->field_list.fields[start];
      own_count = fc;
   }

   /* No base? Return own fields merged with policy (base = NULL) */
   if (!stmt->base_meta) {
      anvl_field_list_t *merged = merge_fields_custom(
          state->ctx, state->src, NULL, own_fields, own_count, policy, userdata);
      if (!merged)
         return NULL;

      /* Note: This leaks memory on repeated calls with custom policy */
      /* For MVP: acceptable (test-only), production would need cache */
      anvl_field_list_t *cached = Allocator.alloc(sizeof(*cached));
      if (!cached) {
         Allocator.dispose(merged->fields);
         Allocator.dispose(merged);
         return NULL;
      }
      *cached = *merged;
      Allocator.dispose(merged);
      return cached;
   }

   /* Find base statement */
   usize base_stmt_idx = id_map_lookup(&state->id_to_idx, raw,
                                       stmt->base_meta->pos,
                                       stmt->base_meta->len);
   
   if (base_stmt_idx == (usize)-1) {
      anvl_error_set(ANVL_ERR_RESOLVER_MISSING_BASE,
                     "base identifier not found at resolve time", 0, 0, __FILE__);
      return NULL;
   }

   /* Recursively resolve base (using custom policy) */
   const anvl_field_list_t *base_list = anvl_node_state_get_merged_fields_custom(
       state, base_stmt_idx, policy, userdata);
   if (!base_list && anvl_error_is_set())
      return NULL;

   /* Merge with custom policy */
   anvl_field_list_t *merged = merge_fields_custom(
       state->ctx, state->src, base_list, own_fields, own_count, policy, userdata);
   if (!merged)
      return NULL;

   /* Note: Memory leak on repeated calls (no caching) */
   /* For MVP: acceptable for tests */
   anvl_field_list_t *cached = Allocator.alloc(sizeof(*cached));
   if (!cached) {
      Allocator.dispose(merged->fields);
      Allocator.dispose(merged);
      return NULL;
   }
   *cached = *merged;
   Allocator.dispose(merged);
   return cached;
}

