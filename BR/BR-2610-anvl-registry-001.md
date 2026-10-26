# BR-2610-anvl-registry-001: loading text identical to a live document fails with an empty `Io` error, because the source registry is process-global

**ID:** BR-2610-anvl-registry-001
**Type:** Bug Report (error reporting, then a design fix)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Bindings agent's anvil.net slice-3 report
**Status:** part 1 done (branch `fix/BR-2610-anvl-registry-001`, not merged); part 2 awaiting planning's decision
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
