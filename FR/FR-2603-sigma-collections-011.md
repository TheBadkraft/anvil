# FR-2603-sigma-collections-011: arena-backed stack primitive for AnvilScript runtime

**ID:** FR-2603-sigma-collections-011
**Type:** Feature Request
**Owner:** sigma.collections
**Filed:** 2026-09-06
**Status:** implemented (2026-09-07) — see Resolution below; `chore/fr-007-fr-011-arena-collections` branch, awaiting merge/review by the regular anvil agent
**Requested by:** Anvil (AnvilScript runtime, `src/core/anvil.c`, future ASL evaluator)
**Tags:** sigma-collections, stack, arena, parray, anvilscript

---

## Summary

Provide an **arena-backed stack** primitive that the AnvilScript runtime can use for function-call state: argument passing, return-value passing, and statement-location bookmarks. The stack is backed by `PArray` (or a new thin wrapper) but constructed against a caller-supplied allocator/arena so the entire stack is reclaimed in bulk when the ScriptEngine context is disposed. This request builds directly on FR-2603-sigma-collections-007 (per-instance allocator override) and exposes stack semantics that Sigma has not previously implemented.

## Background

AnvilScript is designing a lightweight, in-process scripting runtime. Function calls need a deterministic, low-overhead way to:

1. Push arguments before a call.
2. Pop them into parameters inside the callee.
3. Push a return value (if any) back to the caller.
4. Optionally record a statement-location pointer so the engine can later implement cooperative constructs such as `yield`.

Sigma already has `PArray` (pointer array) and `SlotArray` (sparse reusable handles), but neither exposes explicit push/pop stack semantics. `PArray` is the natural substrate because the stack is dense, ordered, and pointer-sized. The missing piece is the ability to bind that `PArray` to an arena allocator (per FR-007) and a small stack-shaped API on top.

### Why this is not just "use PArray directly"

`PArray` today allocates through the global `Allocator` facade. An AnvilScript evaluation context may live only as long as a single function call or a short host callback; allocating and then individually freeing every stack frame's scratch space would defeat the arena-based lifetime model the rest of the runtime uses. FR-007 gives `List`/`Collection` a per-instance allocator override; the same mechanism is needed for `PArray` (and ideally the other array types) so the ScriptEngine can say: "this entire call stack lives in the module/context arena and disappears with it."

### Relationship to FR-007

FR-007 adds per-instance allocator override for `List`/`Collection`. This FR extends that idea to `PArray` and adds explicit stack operations. The two FRs can be reviewed and implemented independently, but the ScriptEngine stack cannot be arena-backed until at least the `PArray` override half is available.

## Requested Changes

### 1. Per-instance allocator override for `PArray`

Add an opt-in constructor alongside `PArray.new`:

```c
parray (*new_with_allocator)(usize capacity, sc_alloc_use_t *use);
```

Behavior mirrors FR-007:

- `use == NULL` → use the global `Allocator` facade, exactly as today.
- `use != NULL` → allocate/free/resize through the supplied `sc_alloc_use_t`.
- Growth of an arena-backed `parray` must **orphan** the old bucket rather than dispose it, because arena allocators cannot free individual prior allocations.

### 2. Stack API over `PArray`

Introduce a thin stack interface. Exact naming/placement is left to the repo owner; one option is a new `Stack` interface backed by `parray`:

```c
typedef struct sc_stack_i {
    stack (*new_with_allocator)(usize capacity, sc_alloc_use_t *use);
    void (*dispose)(stack s);

    void (*push)(stack s, object value);
    object (*pop)(stack s);
    object (*peek)(stack s);
    bool (*is_empty)(stack s);
    usize (*count)(stack s);
    void (*clear)(stack s);

    // Optional: save/restore a cursor for coroutine/yield-style bookmarks.
    usize (*mark)(stack s);
    void (*restore)(stack s, usize mark);
} sc_stack_i;
extern const sc_stack_i Stack;
```

Or, if preferred, add the operations directly to `sc_parray_i` as `push`/`pop`/`peek`. The key semantic requirements are:

- `push` appends to the logical top.
- `pop` removes and returns the top element.
- `peek` returns the top without removing it.
- All operations are O(1) amortized.
- The underlying storage participates in the same allocator-override/growth rules as `PArray`.

### 3. Statement-location bookmark support

For future `yield`-style behavior, the stack should support a lightweight cursor:

- `mark()` returns the current stack depth.
- `restore(mark)` truncates the stack back to that depth (popping everything pushed after it).

This lets the ScriptEngine freeze a call frame's stack state at a statement boundary and resume later by restoring the mark, without allocating a separate continuation object.

## Benefits

- **Eliminates per-call disposal overhead** in the AnvilScript runtime. Arguments, locals, return values, and temporary expression results all live on an arena-bound stack and are reclaimed when the evaluation context ends.
- **Deterministic lifetime** tied to the ScriptEngine context, matching the arena model already used for parsed AST nodes.
- **Foundation for `yield`/`resume`** without a separate coroutine stack copy; the existing stack plus a depth marker is sufficient for simple cooperative suspension.
- **Low implementation risk** if it reuses `PArray` internals and the same override/growth pattern proposed by FR-007.

## Rationale

AnvilScript needs Sigma collections to feel native to its arena-based execution model. A stack is the first collection shape the runtime itself depends on for control flow; without arena backing, the runtime would spend as much code managing stack lifetime as it does evaluating expressions. This FR keeps Sigma in control of the collection primitive while giving AnvilScript exactly the semantics it needs.

## Related

- FR-2603-sigma-collections-007: opt-in per-instance allocator override for List/Collection.
- `notes/anvilscript-design.md` § "Sigma collections and the ASL runtime".

---

## Resolution (2026-09-07)

Implemented as a new `Stack`/`stack` (`stack.h`/`stack.c`), taking the FR's own stated alternative rather than adding `push`/`pop`/`peek` to `sc_parray_i` directly, and landing on `collection` rather than raw `PArray` as its foundation. Two things changed from the original request during review; this section is authoritative over "Requested Changes" for whoever reviews this.

**`PArray.new_with_allocator` (item 1) was dropped, not deferred.** Investigated during review: neither `FArray` nor `PArray` have ever had a growth mechanism in this codebase — `grow` exists exactly once, on `collection` (`collection_grow`, consumed by `collection_add`), and `List` already gets its own growth purely by composing over `collection`, never by asking its backing array to grow itself. `PArray.new_with_allocator` was requested specifically to arm the stack; once the stack composes over `collection` instead of raw `parray`, that motivation is gone — a caller-supplied allocator's initial-bucket-only benefit, with no growth to protect, wasn't judged worth building speculatively. If a real consumer wants a fixed-capacity, arena-backed `PArray` on its own merits later, that's a smaller, separately-motivated FR.

**Reference semantics — the actual reason `PArray` was named in the original request — come from `collection` for free.** `collection_new`/`collection_new_with_allocator` default to `array.handle[0] == 'P'` (only `FArray.to_collection` ever overrides it to `'F'`), and `collection_add`'s `'P'` branch already stores the pushed pointer by value (`memcpy(dest, &ptr, stride)`) rather than copying pointee data. So `Stack` gets exactly the pointer/reference semantics `PArray` would have given it, plus `collection`'s already-implemented, already-TDD-covered arena-aware growth (orphan-on-grow, skip-release-on-dispose, from FR-007) at zero new growth code.

```c
struct sc_stack { collection coll; };   // identical relationship List already has to collection
```

`Stack.new`/`new_with_allocator` mirror `List`'s exactly (`new_with_allocator(capacity, NULL)` == `new`). `push`/`is_empty`/`count`/`clear` are direct one-line delegations to `collection_add`/`collection_get_length`/`collection_clear`. `pop`/`peek` read the slot at `length - 1` (returning `NULL` on an empty stack, matching the FR's own proposed `object`-returning signatures); `pop` additionally truncates via `collection_set_length(length - 1)`. `mark`/`restore` are `collection_get_length`/`collection_set_length` directly — an O(1) truncate with **no zeroing of the discarded region**, matching "without allocating a separate continuation object": a real stack pointer just moves, it doesn't clear memory behind it. A `restore` to a depth greater than the current one is a no-op rather than an error.

**Test coverage** — `test/sigma/test_stack.c`, this subset's `TestBit` convention: LIFO push/pop order, peek non-mutation, pop/peek-on-empty return `NULL`, `is_empty`/`count`/`clear`, `mark`/`restore` (including restoring then pushing again, proving the truncate is real and the slot is reused — not just a cosmetic count), plus the same three allocator-override tests FR-007 established the pattern for (`new_with_allocator(NULL)` == default path, growth orphans instead of releasing, dispose skips releasing when overridden), reusing FR-007's `fake_arena.h` test double as-is — no new test infrastructure needed.
