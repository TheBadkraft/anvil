/*
 * Sigma Collections
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------------------------------- *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------------------------------- *
 * File: query.h
 * Description: Header file for Sigma Collections' heapless query mechanism
 *
 * Query: A uniform, allocation-free way to scan any collection type in this
 *        library — farray, parray, collection, list, map, slotarray — via a
 *        small POD cursor (sc_queryable) each type produces via its own
 *        `as_queryable` (or, for Map, `keys`/`values`). Query.next pulls one
 *        element at a time, caller-driven; Query.first is a convenience that
 *        stops at the first element satisfying a predicate. See
 *        FR-2603-sigma-collections-006 for the full design rationale.
 */
#pragma once

#include <sigma/types.h>
#include <sigma/collection.h>

struct sc_queryable;

/**
 * @brief Advances a queryable by exactly one (matching, for sparse sources)
 *        element. Implemented once per source *kind* (dense array-backed vs.
 *        sparse hash-backed) — never once per caller.
 * @param self The queryable being advanced; may mutate self->index.
 * @param out_element Receives a pointer to the yielded element.
 * @param out_index Receives the index the element was found at (may differ
 *        from self->index's prior value for sparse sources that skip slots).
 * @return true if an element was yielded; false if the source is exhausted.
 */
typedef bool (*sc_query_advance_fn)(struct sc_queryable *self, const void **out_element,
                                    usize *out_index);

/**
 * @brief A heapless, POD cursor over some collection's elements.
 *
 * Produced by a collection type's own `as_queryable` (or `keys`/`values`, for
 * Map) — never constructed by hand. Safe to pass by value; `Query.next`
 * mutates it in place via the caller's own storage.
 */
typedef struct sc_queryable {
   void *source;                 /**< The underlying handle (farray/parray/collection/map/slotarray) */
   usize element_size;           /**< Meaningful for dense sources; ignored by sparse ones */
   usize bound;                  /**< Upper bound for index — logical length for dense sources
                                       (not raw allocated capacity: a List/collection may have
                                       more capacity than appended elements), raw slot count for
                                       sparse sources (which must probe every slot, occupied or not) */
   usize index;                  /**< Current scan position — mutated by advance() */
   sc_query_advance_fn advance;  /**< The source-kind-specific stepping behavior */
} sc_queryable;

/**
 * @brief A projected view of a Map key — keys are caller-owned byte spans,
 *        not guaranteed NUL-terminated, so `Map.keys` yields this instead of
 *        a bare pointer.
 */
typedef struct sc_key_view {
   const char *ptr;
   usize len;
} sc_key_view;

/* Public interface for the Query mechanism                     */
/* ============================================================ */
typedef struct sc_query_i {
   /**
    * @brief Pull the next element from a queryable. No predicate, no
    *        allocation, no early-break imposed — "roll your own query loop."
    * @param q The queryable to advance (mutated in place).
    * @param out_element Receives a pointer to the yielded element, or
    *        undefined if this call returns false.
    * @param out_index Optional; receives the yielded element's index.
    * @return true if an element was yielded; false if exhausted.
    */
   bool (*next)(sc_queryable *q, const void **out_element, usize *out_index);
   /**
    * @brief A one-call specialization of `next`: loop internally, stop at
    *        the first element `pred` returns true for. Covers "find first"
    *        (real out_index) and "any" (caller passes NULL for out_index)
    *        with one function.
    * @param q The queryable to scan (passed by value — this call gets its
    *        own cursor, starting fresh from q's current position).
    * @param pred The predicate to test each element against.
    * @param userdata Passed through to every `pred` call.
    * @param out_index Optional; receives the matching element's index.
    * @return true if a match was found; false if the source was exhausted
    *         with no match.
    */
   bool (*first)(sc_queryable q, sc_predicate_fn pred, void *userdata, usize *out_index);
} sc_query_i;
extern const sc_query_i Query;
