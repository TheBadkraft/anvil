# FR-2603-sigma-collections-006: heapless, early-break, predicate-driven scan

**ID:** FR-2603-sigma-collections-006
**Type:** Feature Request
**Owner:** sigma.collections
**Filed:** 2026-09-04
**Status:** implemented (2026-09-04) — see Resolution below; `chore/sigma-subset-workspace` branch, awaiting merge/review by the regular anvil agent
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

## Resolution (2026-09-04, expanded) — supersedes "Requested API" above

Reviewed against the actual codebase (`sigma.anvil`, the sigma subset workspace) before implementation, across several rounds of design review. This section is authoritative over "Requested API" for whoever implements this — it changed shape twice during review, and this is the final form.

### Why the original signature changed

`Collections.predicate_scan(const void *base, usize stride, usize count, ...)` would've required exposing a raw base pointer publicly, which no type in this codebase does. It also hard-coded "stop at the first match" into the primitive itself, which turned out to be the wrong layer for that decision — see below. Both problems dissolve once the actual foundation is built correctly: a heapless, resumable cursor, unified across every collection type this subset has (not just `farray`/`parray`).

### The core abstraction: `sc_queryable` + `Query`

A **new public header, `include/sigma/query.h`**, and a new top-level vtable, `Query` — chosen over `Find` because this is meant to be a growing namespace of query verbs, not one operation. Its foundation is a heapless, POD cursor that any collection type can produce, and one generic engine that walks it:

```c
// include/sigma/collection.h — shared; farray.h/parray.h already include it
typedef bool (*sc_predicate_fn)(const void *element, usize index, void *userdata);

// include/sigma/query.h
// Advances *self by exactly one matching element. Implemented once per source
// *kind* (dense array-backed vs. sparse hash-backed) — see "Producers" below —
// never once per caller. This is the one seam that lets Map/SlotArray (which
// must skip empty/tombstone slots) and farray/parray/collection/list (which
// don't) share the exact same Query.next/Query.first on top.
typedef bool (*sc_query_advance_fn)(struct sc_queryable *self, const void **out_element);

typedef struct sc_queryable {
    void *source;                  // the underlying handle (farray/parray/collection/map/slotarray)
    usize element_size;            // meaningful for dense sources; ignored by sparse ones
    usize index;                   // current scan position — mutated by advance()
    sc_query_advance_fn advance;   // the behavior — see Producers below
} sc_queryable;

typedef struct sc_query_i {
    // The real primitive: pull the next element, caller decides whether to keep going.
    // No predicate, no allocation, no early-break imposed — "roll your own query loop."
    bool (*next)(sc_queryable *q, const void **out_element, usize *out_index);

    // A one-call specialization of `next`: loop internally, stop at the first match.
    // Covers "find first" (real out_index) and "any" (caller ignores out_index) in
    // one function, exactly as the original FR's rationale intended.
    bool (*first)(sc_queryable q, sc_predicate_fn pred, void *userdata, usize *out_index);
} sc_query_i;
extern const sc_query_i Query;
```

`Query.first` is implemented purely in terms of `Query.next` — it is not a separate mechanism:

```c
bool query_first(sc_queryable q, sc_predicate_fn pred, void *userdata, usize *out_index) {
    const void *elem; usize idx;
    while (Query.next(&q, &elem, &idx)) {
        if (pred(elem, idx, userdata)) { if (out_index) *out_index = idx; return true; }
    }
    return false;
}
```

### Producers — who can become a `sc_queryable`, and how

**Dense sources** (`farray`, `parray`, `collection`, `list`) — all reduce to the internal `sc_array_base` (`{handle, bucket, end}`), either directly (`farray`/`parray` *are* one) or by embedding it as their literal first struct member (`collection`), which is what makes casting to it valid. `list` is the one exception — `struct sc_list { collection coll; ... }` holds its collection *by pointer*, not embedded, so it can't be cast directly; it reaches the same place through its own already-existing `collection`, at zero extra allocation cost. All four share one `advance` implementation (`array_base_query_advance`, internal) — indexes forward, reads via the existing `array_base_get_element_ptr`, never skips anything.

```c
sc_queryable FArray.as_queryable(farray arr, usize stride);
sc_queryable PArray.as_queryable(parray arr);
sc_queryable Collections.as_queryable(collection coll);
sc_queryable List.as_queryable(list lst);   // reaches into lst's own existing collection — O(1), no allocation
```

**Sparse sources** (`map`, `slotarray`) — neither reduces to `sc_array_base` at all (different struct shape entirely), and more importantly, a dense walk would be *wrong* for them, not just inapplicable: both reuse freed slots, so raw index-order walking would hand a predicate garbage/tombstone entries. This is exactly why `SparseIterator` already exists as a distinct thing from `Iterator` in this codebase — `Map`/`SlotArray` already implement the skip-empty-slots contract that mechanism uses (`sc_sparse_i`: `is_empty_slot`/`capacity`/`get_at`). Each gets its own `advance` implementation that wraps that same existing logic — no new scanning behavior, just a second, generic-shaped front door onto code that already exists:

```c
sc_queryable Map.as_queryable(map m);   // yields const map_entry * (key + key_len + value together)
sc_queryable Map.keys(map m);           // yields const sc_key_view * ({ptr, len} — keys aren't NUL-terminated)
sc_queryable Map.values(map m);         // yields const addr *
sc_queryable SlotArray.as_queryable(slotarray sa);
```

`Map.keys`/`Map.values` mirror what `dictionary.Keys`/`dictionary.Values` give you in .NET, and cost almost nothing beyond `Map.as_queryable` itself — same slot walk, just narrower projection. `values()` is free (a value is already a fixed-size `addr`); `keys()` needs one small wrinkle — a key is `{ptr, len}` (not NUL-terminated, per `map.h`'s own contract), so its `advance` copies that pair into a small scratch field carried on the cursor and yields a pointer to it, rather than a bare pointer that would silently drop the length:

```c
typedef struct { const char *ptr; usize len; } sc_key_view;
```

### What this looks like to a caller

The motivating case (`AMP06` in `test_body_amp.c`) today pays for a `collection` view and an `iterator` object just to walk a list of statements and stop at one:

```c
// Before — two allocations for what's really a query
collection view = FArray.as_collection(doc->body, sizeof(anvl_statement));
iterator it = Collections.create_iterator(view);
while (Iterator.next(it)) {
    anvl_statement stmt = *(anvl_statement *)Iterator.current(it);
    if (slice_equals(stmt->name, "third")) { /* found it */ break; }
}
Iterator.dispose(it);
Collections.dispose(view);
```

```c
// After — zero allocations, and it reads as what it actually is: a query
static bool is_named_third(const void *element, usize index, void *userdata) {
    anvl_statement stmt = *(anvl_statement *)element;
    return slice_equals(stmt->name, "third");
}

sc_queryable q = FArray.as_queryable(doc->body, sizeof(anvl_statement));
usize idx;
if (Query.first(q, is_named_third, NULL, &idx)) {
    // found at idx — nothing to dispose, nothing was allocated
}
```

"Any element satisfies this?" is the same call, ignoring the index:

```c
if (Query.first(FArray.as_queryable(doc->body, sizeof(anvl_statement)), is_named_third, NULL, NULL)) {
    // at least one exists
}
```

Rolling your own loop instead of using a predicate — `Query.next` directly, exactly like a hand-written `for` loop, just heapless and uniform across every collection type:

```c
sc_queryable names = Map.keys(symbol_table);
const sc_key_view *k; usize i;
while (Query.next(&names, (const void **)&k, &i)) {
    printf("%.*s\n", (int)k->len, k->ptr);
    if (should_stop_here(k)) break;   // caller decides — nothing is imposed
}
```

Finding an entry in a `Map` by its *value*, something `Map.get`'s key lookup can't do at all today:

```c
static bool value_over_100(const void *element, usize index, void *userdata) {
    return ((const map_entry *)element)->value > 100;
}
usize idx;
if (Query.first(Map.as_queryable(symbol_table), value_over_100, NULL, &idx)) { /* ... */ }
```

### Explicitly out of scope

A real LINQ-style **deferred, composable** query layer — `Where(...).Select(...).OrderBy(...)` building a plan before anything executes — is a different, much bigger thing (effectively a small query-expression compiler), and not something this FR builds. `Query.next`/`Query.first` plus per-type `.as_queryable()` cover the actual need (uniform, heapless, predicated-or-manual scanning over anything in this subset) without it. Nothing here forecloses building that layer later on top of `.as_queryable()` as its entry point, if it's ever worth doing — it just isn't part of this FR.

### Relationship to FR-008 / FR-009

**FR-2603-sigma-collections-008 is superseded by this FR.** It proposed a `Collections`-level convenience for callers already holding a `collection` — `Collections.as_queryable(collection)` above *is* that, just properly named and homed under `Query` rather than being a one-off `Collections` method. Recommend closing FR-008 as absorbed once this lands.

**FR-2603-sigma-collections-009 narrows, rather than disappearing.** `Query.first` already covers "any" (ignore `out_index`); `Query.next` in a caller-side loop already covers "give me every match" (the original motivation for a `Query.where`) with no new code needed. What's left for FR-009, if it's ever wanted, is genuinely new: the deferred/composable chain library described above — that's the only thing not already covered by what this FR delivers.

### Test coverage

New test files, this subset's `TestBit` convention (not `sigma.collections`' `sigtest`/`Assert`, which doesn't apply here) — `test/sigma/test_farray.c`, `test/sigma/test_parray.c` (both currently have zero coverage in this subset), plus `Query`/`as_queryable` coverage across every producer above:

- **Dense** (`farray`/`parray`/`collection`/`list`, via `Query.next`/`Query.first`): first-match found, no-match exhausts the full scan, early-break via `Query.first` actually stops before scanning remaining elements (call-counting predicate), empty collection, `out_index` correctness on match and no-match, NULL-argument invariants.
- **Sparse** (`map`/`slotarray`): the same matrix, plus — empty/tombstone slots are correctly skipped and never reach the predicate (the one behavior that's actually new/risky here, not shared with the dense path); `Map.keys`/`Map.values` project the right field; a removed-then-reinserted slot doesn't produce a stale or duplicate yield.
