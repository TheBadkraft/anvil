# BR-2610-anvl-registry-001: loading text identical to a live document fails with an empty `Io` error, because the source registry is process-global

**ID:** BR-2610-anvl-registry-001
**Type:** Bug Report (error reporting, then a design fix)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Bindings agent's anvil.net slice-3 report
**Status:** open
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
