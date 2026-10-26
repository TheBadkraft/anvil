# BR-2610-anvl-registry-001: loading text identical to a live document fails with an empty `Io` error, because the source registry is process-global

**ID:** BR-2610-anvl-registry-001
**Type:** Bug Report (error reporting, then a design fix)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Bindings agent's anvil.net slice-3 report
**Status:** resolved on branch `fix/BR-2610-anvl-registry-001` (not merged, not pushed; awaiting the owner's go)
**Tags:** source-registry, errors, bindings, rivet-readiness

---

## Summary

`anvil_load_buffer` (and `anvil_load`) fails when its content is byte-identical to any document
still loaded anywhere in the process. The failure surfaces as `ANVIL_ERR_IO` with an **empty
message**:

- `src/core/source_registry.c` (≈ line 89): a content-hash hit is reported as
  `ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT`, an unrelated parser code reused for it.
- `src/core/anvil_flat.c` `load_common`: when `mod_ctx_register_doc` fails, it sets
  `ANVIL_ERR_IO` and builds the detail from a context with nothing recorded, so the message is
  empty.

## Why it's a native problem, not a binding one

The registry exists to deduplicate documents **within one include graph** (the diamond-include
case, `document.c` ≈ line 93). But it's a process-global map, while every `load_common` call
creates its own `module_context` (`owns_ctx = true`). So two *independent* loads of the same text
(different handles, different contexts) collide. The same friction has now hit three bindings
independently:

1. `anvil.node` `parse()`: GC-deferred disposal raced identical-text loads (fixed in the binding
   by converting eagerly; see the `anvil-primitives-not-policy-bindings` precedent).
2. `anvil.net` tests: xUnit cross-class parallelism loading identical literals flaked (fixed in
   the binding by disabling parallelization, `02bf32f`).
3. `anvil.net` `AnvilSchema.ValidateField(text, …)`: fails while the caller holds an
   `AnvilDocument` of the same text (worked around with an explanatory exception, plus an
   `AnvilDocument` overload).

The same native constraint keeps showing up across unrelated bindings and paradigms. That's the
threshold for treating the constraint itself as the defect.

## Part 1: fix the report (do first, small)

- Add a dedicated internal code (e.g. `ANVL_ERR_SOURCE_ALREADY_LOADED`, message "a document with
  identical content is already loaded"). No reuse of the parser code, per
  `anvil-public-error-codes-specific-not-mirrored`.
- `load_common` records it, so `anvil_document_get_error`'s detail carries that message. The
  public category can stay `ANVIL_ERR_IO` (the public enum is deliberately small) unless a better
  existing category fits; record the choice.

## Part 2: scope the registry to the include graph (design; confirm with planning before coding)

Recommended: key deduplication per `module_context`, so it covers the include graph of one load,
not every load in the process. Independent root loads of identical text then just work, and the
diamond-include dedup behavior is unchanged. Before implementing, check whether anything relies on
cross-load dedup today (`grep` the registry's users, and the reasons recorded in `notes/` for
making it global), and record the finding here.

## Test cases

1. Part 1: loading text identical to a live document reports a non-empty, specific message (and
   the new internal code), not an empty `Io`.
2. Part 2: two `anvil_load_buffer` calls with identical text, both kept alive, both succeed and are
   independent (disposing one leaves the other valid). Valgrind-clean.
3. Part 2: diamond includes still load each shared document once (existing include tests unchanged).
4. Part 2: the `anvil.net` repro: `ValidateField(text)` while an `AnvilDocument` of the same text is
   alive, via a C equivalent (load, keep it, load again, validate).
5. Full unit suite green; `test_release`, `check-exports` green.
6. Changelog entry under `## [Unreleased]` → `### Fixed`.

## After this lands

The Bindings agent can drop the workarounds: anvil.net's xUnit parallelization switch, and the
special exception text in `ValidateField`.

## Part 1 Resolution

Branch `fix/BR-2610-anvl-registry-001` (not merged, not pushed: awaiting the owner's go).

- New internal code `ANVL_ERR_SOURCE_ALREADY_LOADED = 9005` (generic block), message "A document with
  identical content is already loaded", name table entry added. It replaces the reused parser code at
  all three sites that raise or branch on it: `source_registry.c` (`registry_add`), `module.c`
  (`mod_ctx_register_doc` and the diamond-include handling in `include_load_child`, which *branches* on
  this code, see below), plus the comments in `document.c`.
- `load_common` records the failure on the context (`anvl_error_set`) when `mod_ctx_register_doc` fails,
  so `make_error_detail` finds it. Public category stays `ANVIL_ERR_IO` (the public enum is
  deliberately small and nothing better fits).
- Tests: ANV44 (native: identical load reports "identical" in the detail message, category Io, first
  document untouched, same text loads again after the first is disposed), REG03 (now also asserts the
  specific code), REG08 (message and name). RED first (REG03 got 4001, REG08 and ANV44 failed on the
  message). Full unit regression green, counts unchanged except registry 9 -> 10 and native 43 -> 44;
  `test_release`, `check-exports`, `check-reader-only` green; Valgrind-clean on registry, native, module,
  header, document, schema.
- **Trap worth knowing:** `include_load_child` decides "this include is a diamond, reuse the existing
  document" by comparing the register error to the old parser code. Changing the code in only one place
  would have turned every diamond include into a load failure; all three sites move together, and the
  existing include tests (HDR13 and the CR diamond cases) are what caught-by-design cover it.

## Part 2 findings (investigation only; nothing implemented)

**Who depends on the registry, and on its being global:**

1. **The registry is not primarily a dedup mechanism. It is the Source -> owning-document back-lookup.**
   Five functions in `source.c` (`source_has_errors`, `source_set_error`, `source_get_arena`,
   `source_new_node`, and one more at ≈ line 755) call `Registry.find(src->hash)` to get from an
   `anvl_source` (all the parser is handed) to its `module_document` and from there to
   `doc->context` (error list, arena). `notes/source-identity-registry.md`'s goal says exactly this:
   "Pass `anvl_source` through the parser; let parser errors route through `Source.set_error(src, ...)`
   -> registry lookup -> context error list." The hash was chosen as the document identity, and a
   process-wide map was the simplest place to keep identity -> document.
2. **Dedup is a side effect of that identity choice.** Two documents with the same content hash can't
   both be registered, which gives the diamond-include dedup for free (`include_load_child`) and, as an
   unintended consequence, the cross-load collision. No note records a reason for *cross-load* dedup:
   the notes say "Registry is process-wide with module reference counting to support the intended
   one-module-per-process design while tolerating multiple modules in tests". The design assumed one
   module per process; independent root loads were never an intended use of the global scope.
3. **Nothing in `src/` relies on cross-load dedup.** The only intentional dedup is within one include
   graph (module.c ≈ 617). Other registry users: `doc_dispose` (evict own entry only),
   `mod_ctx_register_doc`, `Registry.release()` in module teardown, and the tests
   (`test_registry.c` REG01-REG10, `test_source.c` registry-backed `has_errors`/`set_error`,
   `Debug.stub_doc`/`dispose_doc`).
4. **The map is unsynchronized.** `source_registry.c` has no lock; `Map` has none. Concurrent loads on
   different threads race on the same global map, which is a plausible cause of the anvil.net
   xUnit-parallelism *flake* beyond the identical-text collision: two parallel loads of *different* text
   also share and mutate it. Worth confirming with a thread-sanitizer run before the design is chosen.
5. **Ordering hazard:** with a global map, disposing handle A while handle B (same content) is mid-load
   is fine today only because B's registration fails before touching A's entry (`doc_dispose` evicts only
   the entry it owns).

**What a per-context scope would take** (my recommendation, for planning to confirm):

- Give `anvl_source` a back-pointer to its `module_document` (set in `mod_ctx_register_doc`, cleared in
  `doc_dispose`); the five `Registry.find(src->hash)` calls in `source.c` become `src->doc`. This removes
  the need for a global lookup at all and is also faster.
- Move deduplication to a `map` owned by `module_context` (hash -> document), used by
  `mod_ctx_register_doc` and `include_load_child` exactly as now. Diamond behavior is unchanged.
- The global `Registry` vtable (`init/add/remove/find/count/release/clear`) then has no remaining
  production caller. Either delete it or make it a thin per-context facade. Its tests (REG01-REG10), the
  `Debug` helpers and the SRC tests that register stub documents would be rewritten, not just edited.
  That is the core-contract change: an internal API removal, no public ABI change.
- It would also make concurrent loads on separate contexts independent (no shared mutable state), which
  addresses finding 4 without adding a lock. Thread safety *within* one context stays the caller's
  responsibility, as it is today.

**Alternatives considered and rejected:** (a) a mutex around the global map fixes the race but not the
identical-text collision; (b) composite key (context pointer + hash) fails because `Source.*` only has
`src->hash` when it looks the document up, i.e. it needs the back-pointer anyway.

**Decision needed from planning:** go/no-go on the per-context redesign above (back-pointer in
`anvl_source`, per-context dedup map, retire or reduce the global `Registry`), and whether to run a TSan
check on the global map first.

## Part 2 TSan baseline (before any Part 2 code)

Harness: `test/infra/tsan_load_stress.c`, `make check-tsan` (8 threads x 200 rounds, public flat API
only, built with `-fsanitize=thread`; modes `different`, `identical`, `diamond`; `TSAN_ALLOC=source`
rebuilds `sigma.system.alloc` from `../sigma.core/src/system_alloc.c` with the sanitizer so the
allocator is not a blind spot). Baseline = Part 1 only, current main.

| mode | race reports | failed loads |
|---|---|---|
| `different` (every thread loads text no other loads), prebuilt allocator | 100 | **41** |
| `different`, allocator built from source with TSan | 109 | 16 |
| `identical` (all threads, same text, all alive) | 169 | 1379 |
| `diamond` (all threads, same file with a diamond graph) | 88 | 1590 |

Even loads of *different* text failed: the unsynchronized global map corrupts lookups under
concurrency, so this confirms the anvil.net xUnit flake is a real race and not only the
identical-text collision. Attributed by the shared object each race hit (`different`, source allocator):

| shared object | reports |
|---|---|
| the global source registry (`source_registry` pointer, its map nodes and resize buffers) | 59 |
| **the global `parser_state parser` in `parser.c`** (`state_map` list, `err_code`) and lists reachable through it | 19 global + about 20 list blocks |
| unattributed (stack / reused block addresses) | 10 |
| the allocator itself | 0 of its own: one `free` report whose other side was the parser global's list |

So: **the per-context design alone would not have made concurrent loads safe.** A second shared
structure sits under every load: `parser.c`'s process-wide `parser` (a list of in-flight parse contexts
appended and removed *by position* around every parse, plus the last-error code).
`sigma.system.alloc` shows no race of its own (checked with it instrumented), and no other sigma or
module global appeared.

## Part 2 Resolution

Same branch, in two commits so the parser change can be vetoed separately (the second one).

**Design as built (the recommendation, approved):**
- `anvl_source_t` gains `owner` (its `module_document`). The five `Source.*` lookups in `source.c`
  (`has_errors`, `set_error`, `get_arena`, `new_node`, and the fifth) read `src->owner` instead of
  `Registry.find(src->hash)`. `owner` is set when a document is registered and cleared when it is
  unregistered.
- Deduplication is `ctx->sources`, a hash -> document map owned by the `module_context`, created in
  `mod_ctx_initialize` and released after the context's documents in `mod_ctx_dispose`.
- **The global `Registry` is kept as a context-scoped facade** (my call): same name and file, so the
  call sites and tests migrate mechanically, and the include loader's diamond handling is untouched
  except for passing the context. `add(ctx, doc, &err)`, `remove(ctx, hash)`, `find(ctx, hash)`,
  `count(ctx)`, `clear(ctx)`, `init(ctx)`, `release(ctx)`. The process-wide map, its reference count and
  `Registry.init()/release()` in `mod_new`/`mod_dispose` are gone. Nothing global remains.
- `doc_dispose` unregisters from the document's own context only if it is the registered one (as
  before); `doc_unload_source` now unregisters the old content's identity first (it left a stale
  global entry before).
- **Trap, recorded:** `Map` keeps the key *pointer*, not a copy. The registry keys by
  `&doc->source->hash` (alive exactly as long as the registration), and my first version keyed by a
  stack variable, which silently broke diamond dedup.
- **Parser (separate commit):** `parser.c`'s `parser_state` loses `state_map` and `is_initialized`
  (the in-flight list existed only for `anvl_cleanup` to dispose contexts a parse had not yet disposed,
  which never happens: `anvl_parse` disposes its own on every path) and becomes `_Thread_local`, so
  `anvl_get_error()` reports the calling thread's last parse. `anvl_cleanup()` now just resets that
  code. Behavior for single-threaded callers is unchanged (all body-parse tests unchanged).

**Result (TSan, same harness, prebuilt allocator and allocator built from source):** `different`,
`identical`, `diamond`: **0 race reports, 0 failed loads** in all three modes, in both allocator
configurations.

**Tests.** Written first and RED (ANV45 identical loads independent, ANV46 diamond per graph, ANV47
identical fragments, SCH84 `ValidateField(text)` equivalent, SCH85 identical extras all readable; the
TSan harness failing in `identical` and `diamond`). Then GREEN, plus:
- `test_registry.c` rewritten for the context-scoped contract (REG00-REG09): duplicates within one
  context fail with `ANVL_ERR_SOURCE_ALREADY_LOADED`; the same hash in two contexts is fine and each
  context finds its own; remove/clear unregister and clear owners; REG09 is the *use after
  unregistered* case: every `Source.*` function that used to ask the registry returns false / error /
  NULL for a source with no owner (what `Registry.find` returning NULL meant before).
- **Fragments do go through the registry** (`anvil_parse_value_fragment` registers its document
  like any load), so FR-2610-anvl-schema-005's extras were silently NULL whenever two had identical
  text (`pooled := true` in two fields: very common). That was a latent bug, now pinned by SCH85 (one
  schema, two schemas, identical text).
- ANV44 (part 1's "identical load reports a specific message") was **retired**: the failure it
  asserted can no longer happen through the public API. Its code and message stay covered by REG03 and
  REG08, and `load_common` still records any registration failure.
- Existing suites touched only to drop the now-meaningless global `Registry.clear()` from their
  setup, and four call sites (DOC09, HDR20, CR16, `Debug`) moved to the context-scoped calls.

**Regression:** all 19 unit suites green, counts unchanged except registry 10 (REG rewritten), native 46,
schema 85; `test_release`, `check-exports`, `check-reader-only` green; Valgrind-clean on registry, source,
module, header, document, native, schema, body_amp, resolver; `make check-tsan` green in both allocator
configurations.

**For the Bindings agent:** the workarounds can go (anvil.net's xUnit parallelization switch and the
special `ValidateField` exception text); `anvil.node`'s eager-conversion in `parse()` is no longer
needed for correctness. Loads on separate threads are independent; sharing a *single* document handle
across threads is still the caller's responsibility, as before.
