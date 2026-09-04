# FR-2603-sigma-collections-008: Collections-level scan convenience for existing `collection` views

**ID:** FR-2603-sigma-collections-008
**Type:** Feature Request
**Owner:** sigma.collections
**Filed:** 2026-09-04
**Status:** closed — superseded by FR-2603-sigma-collections-006
**Requested by:** Anvil (raised during FR-2603-sigma-collections-006 review)
**Tags:** sigma-collections, iterator, query, zero-allocation, collection

---

## Superseded (2026-09-04)

FR-006's own review expanded to cover exactly this case, properly named and homed: `Collections.as_queryable(collection)` (see FR-006's "Resolution" section, "Producers" — dense sources) is this request's `Collections.<scan>` entry point, verbatim in intent. No separate implementation needed — closing this in favor of FR-006, which now ships it directly.

---

## Summary

Expose FR-2603-sigma-collections-006's heapless predicate-scan mechanism at the `Collections` (i.e. `collection`-level) API too, for callers who already hold a `collection` view — via `FArray.as_collection`/`PArray.as_collection`/`to_collection`, or any other API that only ever hands back a `collection` — and want to query it without paying for a full `Iterator` allocation on top.

**Naming is intentionally left open here** — FR-006 is, as of this filing, still resolving its own function/type names (see its "prior implementation" discussion and the resumable-cursor question raised during its review). This request should inherit whatever primitive and naming convention FR-006 lands on; treat every name below as illustrative, not proposed.

---

## Background

FR-006's primary target is the true zero-allocation path: `FArray.<scan>(...)` / `PArray.<scan>(...)` called directly against a raw `farray`/`parray` handle, with no `collection` involved at all. That covers the motivating AMP06-style case, where the caller controls the whole call site and can skip `create_view` entirely.

It doesn't cover a second, narrower case: code that already holds a `collection` — because it was constructed for other reasons, or because it came back from an API whose contract is "you get a `collection`, not a raw array handle" — and wants to run the same kind of query over it. Today that code's only option is `Collections.create_iterator`, paying a second allocation (the iterator object) on top of whatever already produced the `collection`.

Since `sc_collection` embeds `sc_array_base` directly (`internal/collections.h`), the same shared low-level primitive FR-006 introduces at the `array_base` layer can back a `Collections`-level entry point as a thin, mechanical wrapper — no new scanning logic, just another one-line delegation in the same style `Collections.create_view`/`create_iterator` already use.

---

## Requested API

Illustrative only, pending FR-006's naming:

```c
bool Collections.<scan>(collection coll, sc_predicate_fn pred, void *userdata, usize *out_index);
```

Delegates to the same shared primitive `FArray.<scan>`/`PArray.<scan>` delegate to, cast against `coll`'s embedded `sc_array_base`.

---

## Rationale

- **Low risk, mechanical.** No new scanning behavior — purely a second entry point onto FR-006's shared primitive, for a caller shape FR-006 itself doesn't cover.
- **Doesn't duplicate `Iterator`.** Still doesn't replace `Collections.create_iterator` for a full stateful/resumable walk — same non-goal FR-006 states, just extended to the `collection`-holding case.
- **Genuinely secondary.** FR-006's `FArray`/`PArray` entry points already satisfy the zero-allocation goal for any caller that controls its own call site. This exists for the narrower "I already have a `collection`, not a raw handle" case.

## Non-goals

- Not a mandate to change how `collection` views are constructed, or to add allocator/lifetime behavior — purely a read-only scan entry point.
- Not to be started before FR-006 finalizes its primitive's name and shape — this FR should be revisited (and likely renamed throughout) once that lands.
