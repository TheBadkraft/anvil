# BR-2610-anvl-vtable-001: `anvil_document_get_path` is missing from the `Document` vtable, and nothing checks vtable completeness

**ID:** BR-2610-anvl-vtable-001
**Type:** Bug Report (public ABI)
**Owner:** anvl, worked by the ANVL coder agent (roadmap addendum A2)
**Filed:** 2026-10-25
**Filed by:** anvil (planning), from the coder agent's finding in FR-2610-anvl-types-001
**Status:** open
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
