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
