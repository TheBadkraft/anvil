# FR-2603-sigma-collections-010: version-counter mutation detection for Query/Iterator/SparseIterator

**ID:** FR-2603-sigma-collections-010
**Type:** Feature Request
**Owner:** sigma.collections
**Filed:** 2026-09-04
**Status:** open — approved in principle (not speculative like FR-009; scheduling only, not a "should we" question)
**Requested by:** self-filed during FR-2603-sigma-collections-006 review
**Tags:** sigma-collections, query, iterator, safety, invalidation

---

## Summary

Enforce, at negligible runtime cost, the invariant FR-006 documented but left unenforced: don't mutate a collection while something is scanning it. Give every mutable collection struct a `usize version` counter, incremented only by *structural* mutations (ones that can reallocate/move storage — not plain value overwrites). `sc_queryable` (and, for consistency, `iterator`/`sparse_iterator`) captures the version at construction and checks it on each step, catching the bug at the moment it happens instead of leaving it as silent undefined behavior.

Modeled directly on how .NET does this in `List<T>`/`Dictionary<K,V>` — see FR-006 review discussion for the full comparison. The short version: it's not the "expensive lock" and not the "compiled-out debug check" extremes considered during that review — it's a single integer compare per step, always on, in every build, because that's genuinely all it costs.

---

## Background

FR-006 landed `Query`/`sc_queryable` and documented (`query.h`'s INVARIANT block, plus a `@warning` on every `as_queryable`/`keys`/`values`) that mutating a source while a queryable over it is live is undefined. That same hazard already existed, undocumented-as-enforcement, for this library's `Iterator`/`SparseIterator` — `Map.create_iterator`'s doc comment already states the rule in prose ("Any set/remove operation can invalidate iterator traversal state") with nothing checking it.

Investigated during FR-006's review: .NET's `List<T>`/`Dictionary<K,V>` solve exactly this with one private counter field, bumped on structural mutation, checked once per `MoveNext()`. Not compiled out in release — the cost is low enough that there's no reason to hide it behind a debug flag. This FR proposes the same shape here.

---

## Which structs actually need a counter

Not "every mutating operation on every type" — only ones that can **reallocate or relocate storage**, mirroring .NET's own rule that a plain value overwrite (`list[i] = x`) doesn't bump `_version`, only `Add`/`Insert`/`Remove`/`Clear` do:

- **`sc_collection`** (`internal/collections.h`) — `collection_grow` reallocates `array.bucket` and disposes the old one; `collection_add`/`collection_remove`/`collection_clear` change `length`. This is the one that matters most: it's what backs `List`, so `list_append`/`list_insert_at`/`list_remove_at`/`list_clear` all route through it already.
- **`struct sc_map_s`** (`map.c`) — `map_resize` (triggered by `map_set` crossing the load factor) reallocates `buckets` entirely; `map_remove` tombstones a slot in place (no reallocation, but changes what's occupied, which sparse traversal cares about).

**Not needed:** `farray`/`parray`. Neither ever reallocates after construction — `set`/`get`/`remove` mutate value content at a fixed capacity only. `SlotArray.add`/`remove_at` reuse existing slots in its underlying `parray` without resizing it either. None of these can produce a dangling pointer or a shifted-out-from-under-you index the way `collection`/`map` can.

---

## Requested Shape (open questions, not a final spec)

```c
// added to struct sc_collection and struct sc_map_s
usize version;   // starts at 0; incremented by every structural mutator
```

`sc_queryable` (and `struct iterator_s`/`struct sparse_iterator_s`) gains a `usize captured_version` field, set when constructed. Each `Query.next`/`Iterator.next`/`SparseIterator.next` call compares it against the live source's current version before stepping.

**Open, not decided by this filing:**

1. **What happens on mismatch.** No exceptions in C. Candidates: a distinct return signal so "invalidated" isn't confused with "exhausted" (both currently just look like `false`) — e.g. a separate `Query.i` accessor (`bool (*is_valid)(const sc_queryable *q)`) the caller can check after a loop ends, or a reserved out_index sentinel, or an `assert`/`abort` (matching "fail loud during development," closer to .NET's throw, but a much harder failure mode for production code than this library's existing error-code idioms).
2. **Whether `Iterator`/`SparseIterator` are in scope here or split into their own FR.** Recommend keeping them together with `Query` — same hazard, same fix, and it would be an inconsistent half-fix to protect `Query` while leaving `Map.create_iterator`'s documented-only warning exactly as unenforced as it is today.
3. **Whether `map_remove`'s tombstone (no reallocation, but changes occupancy) should count as a version bump.** A live `sc_queryable`/`sparse_iterator` mid-walk wouldn't segfault from a tombstone (no memory moved), but could still see a since-removed entry or skip a since-added one — arguably still worth flagging, even though it's not the memory-safety-grade hazard `collection_grow`/`map_resize` are.

---

## Rationale

- **Cheap enough to always run.** One `usize` compare per traversal step, no allocation, no locking — the .NET precedent proves this doesn't need a debug-only escape hatch.
- **Fixes a real, already-documented gap.** FR-006's INVARIANT note and `Map.create_iterator`'s existing doc comment both currently rely entirely on the caller reading and remembering prose. This makes the failure loud and immediate instead of silent and eventually-a-segfault.
- **Precisely scoped.** Only `sc_collection` and `struct sc_map_s` need the field — `farray`/`parray`/`slotarray` structurally can't produce this hazard, so they're untouched.

## Non-goals

- Not a guarantee of safety under concurrent mutation from another thread — this library is documented thread-compatible, not thread-safe, elsewhere (e.g. `map.h`), and this FR doesn't change that. It catches the single-threaded "I mutated while I was still iterating" bug, same as .NET's version does.
- Not retroactively changing `Query.next`/`Iterator.next`/`SparseIterator.next`'s existing return-value contract without deciding open question 1 first.
