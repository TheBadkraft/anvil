# FR-2603-sigma-collections-009: deferred, composable query chains over `sc_queryable`

**ID:** FR-2603-sigma-collections-009
**Type:** Feature Request
**Owner:** sigma.collections
**Filed:** 2026-09-04
**Status:** open — speculative, not committed
**Requested by:** self-filed during FR-2603-sigma-collections-006 review
**Tags:** sigma-collections, query, linq, speculative

---

## Summary

A real LINQ-style **deferred, composable** query layer on top of `sc_queryable`/`Query` (FR-006) — `Where(...).Select(...).OrderBy(...)`-shaped chains that build a plan and only execute when finally consumed, rather than running immediately. This is explicitly speculative: filed to record the idea and its scoping boundary, not as committed work.

---

## Background

FR-006 landed `Query.next` (heapless, caller-driven pull) and `Query.first` (stops at first predicate match) uniformly across every collection type in this subset — dense (`farray`/`parray`/`collection`/`list`) and sparse (`map`/`slotarray`) alike, via `.as_queryable()`.

During that review, it became clear most of what a `Query.where`/`Query.any`/`Query.count` family would add is already free with what FR-006 ships:

- **"Any element matches?"** is `Query.first(q, pred, NULL, NULL)`, ignoring `out_index` — no new function needed.
- **"Give me every match"** is a caller-side loop over `Query.next` with their own condition inline — no new function needed.
- **"Count matches"** is the same loop with a counter — no new function needed.

What's *not* covered by any of that is genuine **deferred composition** — building up `Where(pred1).Where(pred2).Select(projection)` as a value, without running any of it, then executing the whole chain once at the end (and potentially fusing/optimizing the steps together, the way real LINQ providers do). That's a materially different kind of thing: closer to a small expression-builder/compiler than a scanning primitive, and not something FR-006's `sc_queryable`/`advance` mechanism was designed to do.

---

## Requested Shape (illustrative, unscoped)

Not designed — this FR exists to name the boundary, not to specify the mechanism yet. Candidate directions, none committed:

- A `sc_queryable` wrapped in a builder that stacks predicates/projections and only materializes an `advance` chain on first `Query.next` pull (lazy, but not necessarily optimized/fused).
- A distinct type (`sc_query_expr` or similar) representing an unexecuted plan, with its own small vtable (`Where`, `Select`, `Take`, ...) that composes plans rather than running them, and a terminal `Query.run(expr, ...)` that actually walks the source once.

Either direction is a genuinely new subsystem, not an extension of `array_base`/`sc_sparse_i` the way FR-006's producers were.

---

## Non-goals (for now)

- Not committed work — filed to capture the idea while it's fresh, explicitly deferred until there's a real, concrete need (per FR-006's review: "some things simply do not translate well at lower level and should be left out of scope" — this may turn out to be one of them, or it may not; undetermined).
- Not a rewrite of `Query.next`/`Query.first`/`.as_queryable()` — whatever this becomes should sit on top of FR-006's mechanism as consumers, not change it.
- Not scoped to a specific set of chain operators (`Where`/`Select`/`OrderBy`/`Take`/...) — which operators would be included is itself an open question if this is ever picked up.
