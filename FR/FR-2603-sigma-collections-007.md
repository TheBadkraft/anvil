# FR-2603-sigma-collections-007: opt-in per-instance allocator override for List/Collection

**ID:** FR-2603-sigma-collections-007
**Type:** Feature Request
**Owner:** sigma.collections
**Filed:** 2026-09-04
**Status:** open
**Requested by:** Anvil (downstream consumer — `anvil/src/core/module.c`, `parser.c`)
**Tags:** sigma-collections, allocator, arena, list, collection

---

## Summary

Give `List`/`Collection` an **opt-in, per-instance** allocator override — a second constructor (`List.new_with_allocator` / `Collections.create_with_allocator`, exact naming up to the repo owner) that binds one specific list/collection instance to a caller-supplied `sc_alloc_use_t`, alongside the existing `List.new`/`Collections.create`, which keep routing through the global `Allocator`/`Application` facade exactly as they do today. Paired with a small `collection_grow` change so growth doesn't corrupt a bump/arena-backed instance.

**This is not a re-request of FR-2603-sigma-collections-001** (closed obsolete in favor of the single, process-wide, set-once `Application.set_allocator()`). See "Why the current Application allocator can't cover this" below for why a narrower, per-instance mechanism is still needed even with that architecture in place.

---

## Background

Anvil allocates every `anvl_value`/`anvl_statement` node for a parsed document from a bump/arena allocator (`bump_allocator`, one per `module_context`, created by `mod_ctx_create_arena`) — the whole arena is released as a single block when the context is disposed, so individual node lifetime is free. But every `list` living *inside* those nodes — `.collection.items` (`ARRAY`/`TUPLE` elements), `.attributes`, `.body`/`.object.statements` (nested statement lists) — is a `sigma.collections` `list`, heap-backed via the global `Allocator`, since `List`/`Collection` has no allocator parameter anywhere in its API today.

That forces Anvil to hand-roll a manual disposal pass in `mod_ctx_dispose` for every list kind it introduces — walking its own flat node index and individually disposing each `.collection.items`/`.attributes`/`.body`, one duplicated "walk the flat index, tear down each list" pass per kind, currently three and growing. If `List`/`Collection` could be told, per instance, to allocate from the same arena the surrounding nodes already live in, those lists would be reclaimed for free along with everything else, and the manual passes would disappear entirely.

### Why the current `Application` allocator can't cover this

`sc_application_i.set_allocator()` (`sigma.core/include/sigma.core/application.h`) is process-wide and set exactly once, before any allocations, at module-init time — by design, per FR-001's resolution, which deliberately moved away from per-module hooks toward this single global mechanism. That's the right call for "which allocator does the whole process use," but it can't express Anvil's actual shape: **multiple independent `module_context`s, each with its own arena, can be alive at the same time** (concurrent parses, or a test harness holding several contexts open at once). A single global allocator — even a scoped push/pop on it — is still one allocator for the whole process at any given moment; it can't route context A's lists to arena A and context B's lists to arena B simultaneously. Only a per-*instance* override, decided at the point a specific `List`/`Collection` is constructed, can do that.

This request is deliberately narrower than FR-001's per-module hook: it changes nothing about the default path (`List.new`/`Collections.create` and every existing caller are untouched, still global-`Allocator`-backed), and adds one explicit, opt-in alternate constructor for a caller that wants a specific instance bound to a specific allocator.

### The `collection_grow` blocker

Even with an override in place, growth would still corrupt a bump/arena-backed instance today. `collection_grow` (`sigma.collections/src/collections.c:182-203`) grows by `Allocator.alloc`-ing a new, larger buffer, `memcpy`-ing the old contents over, then `Allocator.dispose`-ing the old buffer (`collection_dispose`, line 162-171, does the same on final teardown). A bump/arena allocator has no way to free one specific prior allocation — only bulk-release the whole arena — so disposing the old buffer mid-grow would corrupt it on the very first grow.

**Precedented fix**: orphan the old buffer on grow instead of disposing it, when the instance carries an arena-style override — exactly what Anvil's own legacy parser (`src/core/_parser.c`) already did by hand for its growable `elem_temp` buffers before this generation of the parser moved to plain `List.new`/`List.append` calls for simplicity ("old buffers are orphaned in the arena, reclaimed at context dispose"). The fix generalizes that hand-rolled pattern into `collection_grow` itself, conditional on whether the instance has an override bound.

---

## Requested Changes

**1. New opt-in constructors**, alongside the existing ones:

```c
list (*new_with_allocator)(usize capacity, usize elem_size, sc_alloc_use_t *use);
collection (*create_with_allocator)(usize capacity, usize stride, sc_alloc_use_t *use);
```

Internally, the instance stores the `sc_alloc_use_t *` it was constructed with (`NULL` for every existing `List.new`/`Collections.create` caller, meaning "use the global `Allocator`/`Application` facade, exactly as today").

**2. `collection_grow` / `collection_dispose` — orphan instead of dispose when overridden:**

```c
int collection_grow(collection coll) {
    ...
    void *new_buffer = coll->alloc_use
        ? coll->alloc_use->alloc(coll->stride * new_capacity)
        : Allocator.alloc(coll->stride * new_capacity);
    if (!new_buffer) return ERR;

    memcpy(new_buffer, coll->array.bucket, coll->stride * current_capacity);
    if (!coll->alloc_use) {
        Allocator.dispose(coll->array.bucket);  // arena-backed: orphan instead, do nothing here
    }
    coll->array.bucket = new_buffer;
    ...
}
```

`collection_dispose` similarly skips `Allocator.dispose(coll->array.bucket)` when `coll->alloc_use` is set — the bound allocator (the arena) owns reclaiming that memory as a whole, at its own disposal time, not `collection`'s.

**3. `List`'s own thin wrapper** (`sigma.collections/src/list.c`, wherever it delegates to `collection_*`) needs the same override threaded through its `new_with_allocator` counterpart.

---

## Rationale

- Removes an already-duplicating, purely mechanical disposal pass from Anvil's `mod_ctx_dispose` (two today, a third and fourth landing as more nested-list kinds ship) — every one of them doing the same "walk the flat node index, tear down each list" shape, for lists whose lifetime is already fully owned by the surrounding arena.
- **Strictly additive and opt-in** — zero behavior change for `List.new`/`Collections.create` or any existing caller; the override only exists for a caller that explicitly asks for it.
- **General-purpose**, not an Anvil-only hook — any `sigma.collections` consumer with the same "bump-allocate a batch of nodes, bulk-free the whole batch later" lifetime shape benefits identically. Anvil is the motivating and current-only caller, same relationship the `mmap`-backed source-loading idea (filed separately, `sigma.memory`) has to Anvil's file-loading path.
- **Not urgent.** Anvil's current manual disposal passes are correct and Valgrind-clean today — this is a quality-of-life/DRY request, not a bug fix, and can be scheduled whenever it suits `sigma.collections`' own roadmap.

## Non-goals

- Not reopening FR-2603-sigma-collections-001's per-module `alloc_use` hook — the global `Application` allocator stays the default and only mechanism for every caller that doesn't explicitly opt in to `new_with_allocator`.
- Not proposing any change to `Application.set_allocator()`'s own single-shot, process-wide semantics.
