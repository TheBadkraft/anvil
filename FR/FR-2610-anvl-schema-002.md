# FR-2610-anvl-schema-002: validate an object value, not only a document — plus a partial (patch) mode

**ID:** FR-2610-anvl-schema-002
**Type:** Feature Request (native library / public API)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet, flywire (downstream consumers)
**Status:** open — not started
**Tags:** schema, public-api, flywire, partial-update, rivet-readiness

---

## Summary

`anvil_schema_validate(schema, data_doc)` takes a whole `anvil_document` and validates its
top-level statements. Real request bodies put the record *inside* the document: FlyWire's create
is `set := { field := value; ... };`, and batch create is `set := [ {...}, {...} ];`. There's no
way to point a schema at the object value of `set`. There's also no way to say "this is a partial
update, don't enforce `required`." FlyWire's own validator has that as `requireAll: false`.

## Motivation

This is what lets FlyWire (and so Rivet) drop its separate JavaScript validator and use
AnvilSchema (FR-2610-flywire-010). Without it, AnvilSchema can't express the two most common
write operations, create-from-a-body and patch.

## Design

- `bool anvil_schema_validate_value(anvil_schema schema, anvil_value object_value, const anvil_schema_options *opts);`
  validates the statements of an OBJECT value exactly as if they were a document's top level.
  A non-object value yields one TYPE_MISMATCH violation with an empty field path.
- `anvil_schema_options` (a struct with a `size` field for forward compatibility, zero-init = today's behavior):
  - `skip_required`: required-presence checks are not enforced (patch semantics). Every other
    check still applies to every field that *is* present.
  - `allow_unknown`: unknown-field violations are suppressed. Off by default; included so a
    consumer can opt in without filtering violations by category.
- `anvil_schema_validate(schema, doc)` keeps its signature and behavior. It is defined as the
  document form with default options.
- Violation list semantics are unchanged: each call replaces the previous call's violations.

## Test cases

1. Doc `set := { status := open; title := "x"; };` with schema fields `status` (required) and
   `title`: validating the value of `set` returns true with 0 violations.
2. Same, with `status` omitted and default options → one REQUIRED; with `skip_required` → 0.
3. `skip_required` with a present-but-invalid `status := bogus;` → one VALUES violation (other checks still run).
4. `allow_unknown` with an extra field → 0 violations; default → one UNKNOWN_FIELD.
5. Passing an ARRAY value → one TYPE_MISMATCH with an empty path, no crash.
6. Validating each element of `set := [ {...}, {...} ]` in turn: each call replaces the previous
   call's violations (documented, tested).
7. `anvil_schema_validate(schema, doc)` results are identical to before (regression).
8. Valgrind-clean; full suite green.

## Dependencies

None. Composes with FR-2610-anvl-schema-001: nested rules apply equally under `validate_value`.
