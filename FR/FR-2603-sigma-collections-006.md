# FR-2603-sigma-collections-006: heapless, early-break, predicate-driven scan

**ID:** FR-2603-sigma-collections-006
**Type:** Feature Request
**Owner:** sigma.collections
**Filed:** 2026-09-04
**Status:** open
**Requested by:** Anvil (downstream consumer — `anvil/src/core/parser.c`, `document.c`, and the upcoming Resolution phase)
**Tags:** sigma-collections, iterator, query, zero-allocation

---

## Summary

Add a heapless, early-break, predicate-driven scan primitive to `sigma.collections` — a leaner alternative to `Iterator`/`Collections.create_iterator` for one-off query-shaped access (find-first-match, any-match) over a contiguous run of elements. Takes a base pointer, a stride, a count, and a predicate function with the ability to stop scanning as soon as it's satisfied — no `Allocator.alloc` at all, for either a collection view or an iterator object.

The repo owner recalls building something like this before — mechanically equivalent to .NET's `IEnumerator` pattern — predating a major Sigma refactor, and it was never re-implemented afterward. That prior version is not readily available (would take significant effort to locate, if it still exists at all), so it should **not** block finalizing this FR — the signature below is a fresh proposal to be judged on its own merits, not a placeholder awaiting comparison against unrecoverable prior art.

---

## Background

Raised while showcasing the existing generic `Iterator` in Anvil's own `test_body_amp.c` (`AMP06`) — a plain ordered walk over an `farray` view of a statement list via `Collections.create_iterator`. That walk works fine, but it always pays two allocations up front (a `collection` view, then an `iterator` object) regardless of how soon a match is found, and has no filtering or early-exit of its own — a caller manually loops and manually `break`s after already paying for both allocations.

For a one-off "find the first match" or "does any element satisfy this" query, that's strictly more work than the query needs. A predicate-scan taking the raw base pointer/stride/count directly could answer the same question with a single function call and zero heap traffic.

**Complements, not replaces, `Map`-backed lookups.** `sigma.collections` already has `Map`/`SparseIterator` for repeated lookups against a stable keyspace — that's the right tool when the same keys get looked up over and over (O(1) amortized vs. even a zero-allocation O(n) scan). This request is for the other shape of access: an arbitrary, possibly one-off predicate over a contiguous run, where there's no known key to hand a `Map`, or the query condition isn't a key-equality test at all. Anvil's own internal code always reaches for `Map` where lookups repeat (its upcoming Resolution phase does exactly that for `$identifier`/`base` lookups); this primitive is for everything else — consumers of a collection who have some arbitrary query shape and no allocation budget to spare for it.

---

## Requested API

Proposed addition to `sigma.collections`, alongside the existing `Iterator`/`Collections` API (exact naming/placement — `collections.h` vs. a new header — left to the repo owner):

```c
// Return true to stop the scan at this element (it matched); false to keep scanning.
typedef bool (*sc_predicate_fn)(const void *element, usize index, void *userdata);

// Scans `count` elements of `stride` bytes each, starting at `base`, calling `pred` for
// each one in order. Stops at the first element `pred` returns true for (a match), or
// after all `count` elements have been scanned (no match). No allocation of any kind —
// no collection view, no iterator object.
//
// Returns true if a match was found; if `out_index` is non-NULL, it receives the
// matching element's index (undefined on a false return).
bool Collections.predicate_scan(const void *base, usize stride, usize count,
                                 sc_predicate_fn pred, void *userdata,
                                 usize *out_index);
```

`predicate_scan` covers both "find first match" (a real match) and "does any element satisfy this" (caller ignores `out_index`, just checks the `bool` return) with one call. A `void *userdata` parameter lets the predicate close over caller state without needing a capturing closure (matching plain-C convention already used elsewhere in `sigma.collections`, e.g. `map_entry` iteration).

---

## Rationale

- **Allocation-zero query access is the actual win**, not just fewer lines at the call site — a real cost reduction for hot or one-off query paths, not just ergonomics.
- **Strictly additive.** Doesn't replace `Iterator`/`Collections.create_iterator` (still the right tool for a full, stateful, resumable walk) or `Map` (still the right tool for repeated keyed lookups) — fills the gap between them.
- **General-purpose, not Anvil-specific.** Lives in `sigma.collections` alongside the existing collection-access primitives; Anvil is the motivating and current caller but the mechanism has no dependency on anything Anvil-shaped.
- **Low implementation risk.** No new storage type, no lifetime/ownership questions — a single stateless scanning function over caller-owned memory.

---

## Resolution (2026-09-04) — supersedes "Requested API" above

Reviewed against the actual codebase (`sigma.anvil`, the sigma subset workspace) before implementation. Two things changed from the original proposal; this section is authoritative over "Requested API" for whoever implements this.

### 1. Serve both `farray` and `parray`, via the shared `array_base` layer — not a free `Collections` function

The original signature (`Collections.predicate_scan(const void *base, usize stride, usize count, ...)`) would require exposing a raw base pointer publicly, which neither `FArray` nor `PArray` do today — both are opaque handles. It's also unnecessary: `farray` and `parray` are both already castable to the internal `sc_array_base` (`{handle, bucket, end}` — see `include/sigma/internal/array_base.h`), and every existing operation (`capacity`, `clear`, `set`, `get`, `remove`, `compact`) is already implemented once at that shared layer, then re-exposed per type as a one-line cast+delegate in `farray.c`/`parray.c`. This scan primitive should follow that exact, already-established pattern instead of introducing a new public raw-pointer capability.

(`parray` itself was header-only in this subset with no `.c`/build wiring — ported from `sigma.collections` as a prerequisite for this FR, along with its `slotarray` dependency. Both are now implemented, tested-green against the existing suite, and wired into `test/sigma/Makefile`.)

### 2. Early-break is caller-driven via a resumable cursor, not baked into the primitive

The original proposal hard-codes "stop at the first match" into the primitive itself. Reconsidered during review: `sc_array_base` is POD with no heap state, so a scan *position* is just one more index alongside it — a resumable cursor costs nothing extra over a fixed-stop function, and building the fixed-stop version first only means rebuilding it under a cursor later. Build the cursor as the actual foundation now.

**A new `Query` vtable is introduced as the forward-looking home for this family** (`Query` over `Find` — this needs to keep growing past one verb: `first`, later `any`/`where`/`count`, is a namespace, not an operation). Its natural input is `collection` (the codebase's existing type-erased handle), matching how `Collections.create_iterator` already works — but that means anything under `Query` pays for a `collection` view when the caller doesn't already have one, which reopens the exact allocation this FR exists to eliminate. So the scope split is deliberate:

- **This FR (FR-006) delivers only the true zero-allocation path** — `FArray.select_first` / `PArray.select_first`, called directly against a raw `farray`/`parray` handle, no `collection` involved. This is the concrete fix for the motivating AMP06 case.
- **`Query` itself — `Query.first(collection, ...)` and beyond — is explicitly out of scope for this FR.** `Query.first` is FR-2603-sigma-collections-008's job (already filed, scoped as the `collection`-holding convenience this FR doesn't cover); the broader `any`/`where`/`count` family is FR-2603-sigma-collections-009 (to be filed). Both build on the exact same internal engine this FR lands — no reimplementation, just a second and third front door onto it.

### Revised shape to implement

```c
// include/sigma/collection.h — shared, both farray.h and parray.h already include it
typedef bool (*sc_predicate_fn)(const void *element, usize index, void *userdata);

// include/sigma/internal/array_base.h — internal, not public
typedef struct {
    const sc_array_base *arr;
    usize element_size;
    usize index;          // next position to examine
} sc_scan_cursor;

bool array_base_scan_next(sc_scan_cursor *cur, sc_predicate_fn pred, void *userdata,
                           const void **out_element, usize *out_index);
```

`FArray.select_first(farray arr, usize stride, sc_predicate_fn pred, void *userdata, usize *out_index)` and `PArray.select_first(parray arr, sc_predicate_fn pred, void *userdata, usize *out_index)` (no `stride` — `PArray`'s element size is always `sizeof(addr)`, matching its existing no-stride convention) each build a cursor over their own `sc_array_base` cast and call `array_base_scan_next` exactly once, discarding the cursor — this is what makes `select_first` a trivial specialization of the cursor rather than separate logic.

The cursor type itself stays internal for this FR — not part of the public surface yet. Whether FR-009's `Query.where` exposes it directly (a walkable enumerator) or wraps it behind a callback is that FR's design question, not this one's.

**Test coverage** (new: this subset has zero existing tests for `farray`, and now `parray`) — `test/sigma/test_farray.c` and `test/sigma/test_parray.c`, following this subset's `TestBit` convention (not `sigma.collections`' `sigtest`/`Assert` framework, which doesn't apply here): first-match found, no-match exhausts the full scan, early-break actually stops before scanning remaining elements (assert via a call-counting predicate), empty collection, `out_index` correctness on both match and no-match, NULL-argument invariants (`arr`/`pred` NULL).
