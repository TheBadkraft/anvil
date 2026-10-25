# BR-2610-anvl-vtable-001: `anvil_document_get_path` is missing from the `Document` vtable, and nothing checks vtable completeness

**ID:** BR-2610-anvl-vtable-001
**Type:** Bug Report (public ABI)
**Owner:** anvl, worked by the ANVL coder agent (roadmap addendum A2)
**Filed:** 2026-10-25
**Filed by:** anvil (planning), from the coder agent's finding in FR-2610-anvl-types-001
**Status:** resolved
**Blocks:** the `anvil.py`/`anvil.net`/`anvil.java` bindings reaching a document's path
**Tags:** abi, vtable, document, rivet-readiness

---

## Summary

FR-2610-anvl-types-001 added `anvil_document_get_path` (`include/anvil_flat.h:405`) to the flat API
only. `anvil_document_i` in `include/anvil_vtable.h` has no matching member. The vtable contract
is that a binding binding one struct "can never drift from the flat API", and this breaks it:
`anvil.py`, `anvil.net` and `anvil.java` bind the `Document` vtable and can't reach the function.

Nothing caught it. `test_anvil_vtable.c`'s VT01–VT0n check that every *existing* member is
pointer-identical to its flat function. They don't check that every flat function *has* a member.

## Expected behavior

- `get_path` is appended **last** in `anvil_document_i`, the same way `get_numeric`/`get_bytes`/
  `get_blob_tag` were appended last to `anvil_value_i`, so existing member offsets don't move.
- A completeness test: every function declared in `anvil_flat.h` is reachable through the core
  vtables (`Anvil`, `Document`, `Statement`, `Value`, ...), except functions explicitly listed as
  flat-only with a reason. A new flat function without a member fails the suite.

## Test cases

1. `Document.get_path == anvil_document_get_path`; it returns the same path through the vtable
   as through the flat call for a file-loaded document, and 0 for a buffer-loaded one, matching
   the flat behavior.
2. The completeness test passes, and fails if `get_path` is removed from the vtable (verify once
   by hand).
3. Existing VT tests unchanged; full unit suite green; `test_release` green; Valgrind-clean.
4. Changelog entry under `## [Unreleased]` → `### Fixed`.

## Notes

Do this together with BR-2610-anvl-visibility-001 (addendum A1). Both are ABI-completeness gaps,
and both need closing before the Bindings agent's py/net/java slices bind the native surface.
FR-2610-anvl-public-api-003 (`Schema`/`TypeRegistry` vtables) should reuse the same completeness
test for its two headers.

## Resolution

Branch `fix/BR-2610-anvl-vtable-001`, commit `1c34198`, merged to `main`. Tests VT10 and VT11 in `test_anvil_vtable.c`.
Unit suites green (counts unchanged elsewhere), `test_release`, `check-exports`, `check-reader-only`
green, Valgrind-clean on vtable, native, schema, types.

- `get_path` appended last in `anvil_document_i` and wired in `anvil_vtable.c`.
- **VT11, the completeness test:** reads `include/anvil_flat.h`, extracts every declared `anvil_*`
  function, and requires each to appear in a table of (flat function, vtable member) pairs, checked
  pointer-identical, or in the flat-only list (empty today; each entry needs a reason). It also fails
  if the table names a function the header no longer declares. Verified by hand: deleting the
  `get_path` row makes VT11 fail naming `anvil_document_get_path`; removing the member from the
  struct is a compile error. The table is built inside the test because vtable members are not
  constant expressions in C. FR-2610-anvl-public-api-003 should add its two headers the same way.
- **Test case 1 correction:** the ticket says a buffer-loaded document returns 0 from `get_path`. It
  doesn't: `anvil_load_buffer` registers the fixed label `<buffer>`, and that is what the flat call
  returns (documented in FR-2610-anvl-types-001's header comment). VT10 asserts the vtable result equals
  the flat result for both file and buffer documents, which is the actual contract.
