/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * resolver.h — Inheritance graph resolver (FR-03)
 * ------------------------------------------------------------------
 * Validates the inheritance DAG (cycle detection via Kahn's
 * topological sort) and provides lazy merged-field access.
 *
 * Usage:
 *   anvl_node_state_t *state = anvl_resolver_build_state(ctx, src);
 *   if (!state) {
 *       // no inheritance OR error (check anvl_error_is_set())
 *   }
 *   field *merged = anvl_node_state_get_merged_fields(state, idx, &count);
 *   anvl_node_state_dispose(state);
 * ------------------------------------------------------------------
 */
#pragma once

#include "context.h"
#include "errors.h"
#include <sigma.core/types.h>
#include <stdbool.h>

/* ================================================================
 * anvl_id_map_t — open-addressing hash: identifier string → stmt index
 *
 * Keys are source substrings (pos+len pairs); equality check is
 * a memcmp against the source buffer.  Sentinel: key_pos == USIZE_MAX.
 * Load factor kept below 0.7; table is never resized (pre-sized for n).
 * ================================================================ */
typedef struct {
   usize *key_pos; // source position of each identifier
   usize *key_len; // length of each identifier
   usize *val;     // statement index for each slot
   usize cap;      // allocated slot count (must be power of 2)
   usize count;    // number of entries stored
} anvl_id_map_t;

/* ================================================================
 * Merged field list — array of field pointers returned by
 * anvl_node_state_get_merged_fields().
 * Caller must NOT free individual fields — they are owned by ctx.
 * The array itself is owned by the node_state and freed on dispose.
 * ================================================================ */
typedef struct {
   field *fields;
   usize count;
} anvl_field_list_t;

/* ================================================================
 * anvl_node_state_t — per-root lazy resolution state
 *
 * Mirrors the C struct described in AnvilNodeState.cs.
 * ================================================================ */
typedef struct {
   anvl_id_map_t id_to_idx;        // identifier → statement index
   anvl_field_list_t *merge_cache; // nullable per-stmt slot [n]
   uint8_t *computed;              // bitmap: 1 when merge for slot i is done
   context ctx;
   source src;
   usize stmt_count;
} anvl_node_state_t;

/* ================================================================
 * Public API
 * ================================================================ */

/**
 * Validate the inheritance DAG and build the resolver state.
 *
 * Returns a heap-allocated state on success, or NULL when:
 *   - No statement in the document has a base (fast-path; no error set).
 *   - A cycle is detected (ANVL_ERR_RESOLVER_CYCLE_DETECTED is set).
 *   - Allocation fails (ANVL_ERR_MEMORY_ALLOCATION_FAILED is set).
 *
 * The caller is responsible for calling anvl_node_state_dispose().
 */
anvl_node_state_t *anvl_resolver_build_state(context ctx, source src);

/**
 * Return the merged field list for statement at index stmtIdx.
 * Computed lazily; subsequent calls return the cached result.
 *
 * On missing base: sets ANVL_ERR_RESOLVER_MISSING_BASE and returns NULL.
 * On alloc failure: sets ANVL_ERR_MEMORY_ALLOCATION_FAILED and returns NULL.
 *
 * The returned array and its length are valid until anvl_node_state_dispose().
 */
const anvl_field_list_t *anvl_node_state_get_merged_fields(
    anvl_node_state_t *state, usize stmt_idx);

/**
 * Pre-warm all merge computations (opt-in eager resolution).
 * Returns false if any merge fails (error is set).
 */
bool anvl_node_state_warm_all(anvl_node_state_t *state);

/**
 * Release all memory owned by the state.
 */
void anvl_node_state_dispose(anvl_node_state_t *state);

/* ================================================================
 * Custom Merge Policy API — FR-2604-anvl-002
 * ================================================================ */

/**
 * Merge policy callback type — consumer-controlled field merging.
 *
 * Invoked for each field pair (base, derived) during inheritance resolution.
 * Should return:
 *   - A field pointer (new merged field, or base, or derived)
 *   - NULL to exclude the field from the result
 *
 * Parameters:
 *   ctx          - parsing context (for allocating new fields if needed)
 *   src          - source buffer (for accessing field names/values)
 *   base_field   - field from base statement (may be NULL if only in derived)
 *   derived_field- field from derived statement (may be NULL if only in base)
 *   userdata     - caller-supplied context pointer
 *
 * Examples:
 *   - Array concatenation: build new field with [base] + [derived]
 *   - Object deep merge: recursively merge nested objects
 *   - Exclusion: return NULL to omit field from result
 *   - Default: return derived_field (mimics current "derived wins")
 */
typedef field (*anvl_merge_policy_fn)(
    context ctx,
    source src,
    field base_field,
    field derived_field,
    void *userdata);

/**
 * Get the base statement index for a given statement.
 *
 * Returns:
 *   - Index of the base statement (0 <= idx < stmt_count)
 *   - USIZE_MAX if statement has no base or base not found
 */
usize anvl_node_state_get_base_index(
    anvl_node_state_t *state,
    usize stmt_idx);

/**
 * Get the statement's own (unmerged) fields.
 *
 * Returns the fields defined directly in the statement, WITHOUT
 * inheritance resolution. This is the raw field list from the AST.
 *
 * Returns NULL if statement index is out of bounds.
 */
const anvl_field_list_t *anvl_node_state_get_own_fields(
    anvl_node_state_t *state,
    usize stmt_idx);

/**
 * Return the merged field list with custom merge policy.
 *
 * Like anvl_node_state_get_merged_fields(), but invokes the caller-supplied
 * merge policy callback for each field pair during resolution.
 *
 * If policy is NULL, behaves identically to get_merged_fields() (default
 * "derived wins" semantics) and uses the same cache.
 *
 * Results are cached separately per stmt_idx for custom policies.
 * Subsequent calls with the same policy return cached results.
 *
 * Returns NULL on error (missing base, allocation failure).
 * Error code is set via anvl_error_set().
 */
const anvl_field_list_t *anvl_node_state_get_merged_fields_custom(
    anvl_node_state_t *state,
    usize stmt_idx,
    anvl_merge_policy_fn policy,
    void *userdata);

