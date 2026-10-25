# FR-2610-anvl-schema-003: `nullable` — an explicit `null` is a legal value for a declared field

**ID:** FR-2610-anvl-schema-003
**Type:** Feature Request (native library)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet, flywire (downstream consumers)
**Status:** resolved
**Tags:** schema, null, nullable, rivet-readiness

---

## Summary

A field with a declared `type` that receives `note := null;` fails `kind_matches()`
(`ANVIL_VALUE_NULL` matches no kind), and yields TYPE_MISMATCH. There's no way to declare "this
field may be explicitly null." "Absent" and "null" are different things. FlyWire already keeps
them apart on the write path: an omitted column gets SQL `DEFAULT`, while an explicit `null`
binds NULL. AnvilSchema can't express that difference.

## Motivation

Ticket fields like `assignee_id`, `due_at` and `resolved_at` are legitimately NULL in Postgres.
A patch that *clears* the assignee sends `assignee_id := null;`, and that must validate.

## Design

- New rule key `nullable := true` (default false). Inherits from a resolved type like other constraints.
- Semantics:
  - Absent field → governed by `required` only (unchanged).
  - Present with `null` → valid iff `nullable`; otherwise TYPE_MISMATCH (unchanged behavior, but
    with the clearer message "field is not nullable").
  - Present with `null` and `nullable` → no other constraint (`size`, `values`, ...) is evaluated.
- `required := true; nullable := true;` is legal: the field must be present, and may be null.
- Applies at every depth once FR-2610-anvl-schema-001 lands (nested fields, array items).

## Test cases

1. `{ type := Numeric; }` with `x := null;` → one TYPE_MISMATCH ("not nullable").
2. `{ type := Numeric; nullable := true; }` with `x := null;` → 0 violations.
3. `{ type := String; nullable := true; values := [a, b]; }` with `null` → 0 violations (values not evaluated).
4. `{ required := true; nullable := true; }`: absent → one REQUIRED; `null` → 0.
5. `nullable` inherited from a `@[types]` definition applies to a referencing field.
6. Valgrind-clean; full suite green.

## Dependencies

None.

## Resolution

Branch `feat/FR-2610-anvl-schema-003`, merged to `main`. Tests SCH51–SCH57 (`test_schema.c`) and TYP28
(`test_types.c`); fixtures `schema_nullable_*`. Full unit regression green with unchanged counts
elsewhere, `test_release` green, Valgrind-clean on schema, types, anvil_native, anvil_vtable,
document, resolver. Docs: `docs/types-reference.md` "Nullable values" (and the site mirror).

Decisions:
- A field's explicit `nullable := false` overrides a type's `true`: the rule is "inherit only what
  the field didn't declare", so declared-ness is tracked, not just the value (`anvil_type_def_nullable`
  returns the value and whether it was declared; internal accessor, like `fields_text`).
- A field with **no declared type** accepts `null` either way (nothing for null to violate), which
  keeps the existing behavior; the ticket's TYPE_MISMATCH applies to a field with a declared type.
- A malformed `nullable` (neither `true` nor `false`) is `MALFORMED_CONSTRAINT` on both the type and
  the schema side and is treated as not declared.
- Public introspection of `nullable` is FR-2610-anvl-schema-005's job.
