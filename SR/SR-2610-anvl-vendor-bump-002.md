# SR-2610-anvl-vendor-bump-002: re-bump every binding to today's native `main`, drop the workarounds, and add the decoded attribute accessor

**ID:** SR-2610-anvl-vendor-bump-002
**Type:** Service Request (dependency maintenance + small binding surface)
**Owner:** ANVL Bindings agent (anvil.node, anvil.wasm, anvil.net, anvil.py, anvil.java)
**Filed:** 2026-10-26
**Filed by:** anvil (planning)
**Requested by:** rivet, flywire (downstream consumers)
**Status:** open
**Blocks:** SR-2610-anvl-distribution-001 (the v0.10.0-rc release ships this pin), and Rivet's importer-workaround retirement
**Continues:** SR-2610-anvl-vendor-bump-001
**Tags:** bindings, vendor, rivet-readiness

---

## Summary

The bindings pin `vendor/anvil` at `f9ee783`. Native `main` (`8059560`, 2026-10-26) now carries
work none of them has:

- thread-safe, independent loads (BR-2610-anvl-registry-001)
- NUL-safe registry and schema strings (BR-2610-anvl-nul-001)
- decoded attribute values, `anvil_attribute_get_value_handle` (FR-2610-anvl-public-api-004)
- the `nullable`/`truely` bare-word fix (BR-2610-anvl-parser-001)
- real error messages and positions (BR-2610-anvl-errors-001)
- unterminated-comment errors (BR-2610-anvl-parser-002)

## Scope, per binding

1. **Bump `vendor/anvil` to native `main`.** anvil.java has no pin; it rebuilds against the new
   `libanvil.so`. Rebuild and run the full suite. anvil.wasm rebuilds on the Linode box; check that
   it still builds under emcc with `_Thread_local`, which the coder already verified once.
2. **Vtable and enum drift.** Add the appended `Attribute.get_value_handle` member to every binding
   that mirrors the `Attribute` vtable. The drift tests must catch it first (RED).
3. **Drop the identical-content workarounds:** anvil.net's xUnit `DisableTestParallelization`, the
   special exception text in `AnvilSchema.ValidateField` (.NET) and `validate_field` (Python), and
   anything else that existed only because of the global registry. Keep a test proving that two live
   documents with identical text both load.
4. **Flip the NUL todo/skipped tests** for registry and schema labels and extras to real tests.
5. **One decoded attribute accessor in all four bindings that expose attributes** (node, wasm, py,
   net; java too if it exposes attributes). It reads through `get_value_handle`, gives the decoded
   text (`status="done"` → `done`) and a typed form where the binding has one. `value` stays raw
   everywhere. Use the same name in each binding's idiom, and record the names here.
6. **Error-message regression:** a missing `;` now reads "Expected ';' to end the statement" through
   each binding's error surface (it used to read "Unknown error").

## Rules

- Do binding work in the worktrees. Park the shared checkouts on the new green `main` tips only
  when done, and report the commits.
- TDD, changelogs per binding and in anvil, and a Resolution section here.
- No push. Planning merges on the owner's go, then notifies FlyWire and Rivet with the anvil.node
  commit.

## Test cases

1. Each binding's suite is green on the new pin; vtable/enum drift tests pass with the new member.
2. Two live identical-text documents both load (every binding with a document type).
3. The decoded accessor gives `done` for `@[status="done"]`, `a"b` for `@[note="a\"b"]`, a number
   for `@[n=42]`, and null/None for a flag; `value` is unchanged (raw).
4. The NUL label and extra tests pass.
5. `x := nullable;` loads as a bare string through every binding.
