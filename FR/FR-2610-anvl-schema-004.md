# FR-2610-anvl-schema-004: schema load diagnostics — a malformed schema is reported, not silently weakened

**ID:** FR-2610-anvl-schema-004
**Type:** Feature Request (native library / public API)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** open — not started
**Tags:** schema, diagnostics, public-api, rivet-readiness

---

## Summary

`read_field_rule` documents the current behavior itself: "An unrecognized type name is silently
left with no kind to check against … no error-reporting design exists yet for a malformed
*schema* field itself." So a typo (`type := types.Prority;`) turns a type-checked field into an
unchecked one, and nothing reports it. The same holds for non-numeric `size`/`min`/`max`, a
`values` that isn't an array, and a top-level statement that isn't an object (it's skipped at
`anvil_schema_load`, ≈ line 220).

## Motivation

A validator that weakens silently is worse than none, because callers trust it. Rivet's CI
needs a "schema is well-formed" gate that fails loudly.

## Design

Mirror FR-2610-anvl-types-001's shape so the two read the same:

- `size_t anvil_schema_get_diagnostic_count(anvil_schema)` and
  `anvil_schema_diagnostic anvil_schema_get_diagnostic(anvil_schema, size_t)`, with accessors for
  category, field path, and message. Diagnostics are produced at `anvil_schema_load` and live as
  long as the schema handle.
- Categories (stable, public, appended-only): `UNKNOWN_TYPE`, `MALFORMED_CONSTRAINT`,
  `RULE_NOT_OBJECT`, `UNKNOWN_FORMAT` (FR-2610-anvl-types-004), `SELF_REFERENCE`
  (FR-2610-anvl-schema-001), `CONFLICTING_CONSTRAINTS` (e.g. `min` > `max`).
- Unknown rule keys are **not** diagnostics. They're legitimate consumer extensions (FlyWire's
  `pooled`, `mask`), and FR-2610-anvl-schema-005 exposes them. "Primitives over policy": the
  schema reads what it knows and passes on the rest.
- `anvil_schema_load` still returns a handle when diagnostics exist. The caller decides whether
  to treat them as fatal.
- The type registry's own diagnostics (FR-2610-anvl-types-001) for the registry the schema built
  from its includes are passed through into this list, so one call gives the full picture.

## Test cases

1. `type := types.Prority` (undefined) → one UNKNOWN_TYPE diagnostic at that field path; the
   schema still loads.
2. `size := abc` → one MALFORMED_CONSTRAINT.
3. A top-level `bad := 5;` in a `@[schema]` doc → one RULE_NOT_OBJECT naming `bad`.
4. `min := 10; max := 1;` → one CONFLICTING_CONSTRAINTS.
5. `pooled := true;` → 0 diagnostics.
6. A well-formed schema → 0 diagnostics (regression across every existing schema fixture).
7. Type-registry collision diagnostics from the schema's includes appear in the schema's list.
8. Valgrind-clean; full suite green.

## Dependencies

Soft: FR-2610-anvl-types-001 (pass-through of registry diagnostics).
