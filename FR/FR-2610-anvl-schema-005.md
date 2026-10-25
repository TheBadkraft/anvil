# FR-2610-anvl-schema-005: schema introspection — read a loaded schema's field rules, including consumer extension keys

**ID:** FR-2610-anvl-schema-005
**Type:** Feature Request (native library / public API)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** flywire, rivet (downstream consumers)
**Status:** resolved
**Tags:** schema, introspection, public-api, flywire, rivet-readiness

---

## Summary

`anvil_schema` is opaque: a consumer can validate against it, but can't ask what fields it
declares, in what order, or with what constraints. FlyWire's `SchemaRegistry` needs exactly
that to compute fixed byte offsets and record size from a `.meta.anvl` file. It also reads its
own extension keys (`pooled`, `mask`). So today FlyWire parses the schema document itself, in
parallel with AnvilSchema, which is the "second validator" problem `docs/schema/README.md`
argues against.

## Motivation

Unblocks FR-2610-flywire-010 (FlyWire validates through AnvilSchema), and lets Rivet generate
API docs and client forms from the one schema.

## Design

- `size_t anvil_schema_get_field_count(anvil_schema)` and
  `anvil_schema_field anvil_schema_get_field(anvil_schema, size_t index)` in declaration order;
  `anvil_schema_find_field(anvil_schema, const char *name)`.
- Per-field accessors: name, resolved kind (`anvil_type_kind`), resolved type name (if `types.X`),
  `required`, `nullable`, and each constraint with the existing "returns whether declared" convention
  (`size`, `min`/`max` numeric form, value count/value by index, `format`, `tag`).
- Nested rules (FR-2610-anvl-schema-001): `anvil_schema_field_get_nested_count/get_nested` for
  Object `fields`, and `anvil_schema_field_get_items` for Array `items`.
- **Extension keys:** `anvil_schema_field_get_extra_count` / `get_extra(index)` returns each rule
  key the schema didn't consume, as a name plus an `anvil_value`. Because those values belong to
  the source document, the handle copies them into a schema-owned representation at load time, so
  the existing "independent of the source document once loaded" guarantee holds.
- Read-only. No mutation API.

## Test cases

1. A schema with fields `a, b, c` reports count 3, in that order, and `find_field("b")` returns index 1's handle.
2. Inline and inherited constraints read back identically to what validation enforces.
3. A FlyWire-style rule `{ type := types.Int32; pooled := true; mask := [1, 2, 4]; }` exposes
   `pooled` and `mask` as extras, with readable values.
4. Introspection still works after the source document is disposed (Valgrind-clean, no use-after-free).
5. Nested `fields`/`items` are reachable once FR-2610-anvl-schema-001 lands.
6. Full suite green.

## Dependencies

Soft: FR-2610-anvl-schema-001, FR-2610-anvl-schema-003, FR-2610-anvl-types-004 (their constraints
gain accessors here as they land).

## Resolution

Branch `feat/FR-2610-anvl-schema-005`, merged to `main`. Tests SCH74-SCH80 (`test_schema.c`); fixtures
`schema_introspect_*`. Full unit regression green with unchanged counts elsewhere, `test_release`
and `make check-reader-only` green, Valgrind-clean on schema, types, anvil_native, anvil_vtable,
document, resolver (SCH79/SCH80 read everything after the source document is disposed). Docs:
`docs/types-reference.md` "Reading a schema back" (and the site mirror).

Decisions:
- Accessors report the *effective* rule (inline plus inherited from `types.X`), so what is read back
  is what validation enforces; SCH76 checks that agreement directly.
- `anvil_schema_field_get_type_name` returns the `type :=` as written (`types.Id32`), which is how a
  consumer like FlyWire learns which custom type a field uses.
- **Extras** are copied into schema-owned fragment documents (`anvil_parse_value_fragment`), so they
  are real `anvil_value`s, as the ticket asked, and independent of the source document. A string or
  blob is re-wrapped from its exact bytes (the public API strips delimiters), a collection from its
  raw span, so quoting and escapes survive (SCH80). A value containing a `$variable` reference can't
  be parsed standalone and reads as NULL (the name is still listed).
- Any key not consumed counts as an extra, on every rule at every depth, including an Array's
  `items` rule. The `fields`/`items` containers and every known constraint key are consumed.
- Read-only; the top-level list holds only the `{ ... }` rules (a stray non-rule statement is a
  diagnostic, not a field).
- Vtable exposure belongs to FR-2610-anvl-public-api-003 (Bindings agent).
