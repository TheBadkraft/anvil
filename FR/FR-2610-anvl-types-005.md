# FR-2610-anvl-types-005: Numeric refinements — `integer` constraint and fractional `min`/`max` in the public API

**ID:** FR-2610-anvl-types-005
**Type:** Feature Request (native library / public API)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** resolved
**Tags:** types, schema, numeric, public-api, rivet-readiness

---

## Summary

`Numeric` is one kind covering integers and floats. There is no way to declare "whole numbers
only," so `priority := 2.5;` passes a `{ type := Numeric; min := 1; max := 4; }` field.
Separately, the public getters `anvil_type_def_get_min/max` take `long long *`, so a fractional
bound can't be represented in the API even after BR-2610-anvl-types-001 fixes the reader.

## Motivation

Ticket numbers, counts, ordinals and foreign keys are integral. The Postgres columns behind them
are `integer`/`bigint`, so a fractional value fails at the database instead of in validation.

## Design

- New constraint `integer := true` on Numeric fields and types: the data value must satisfy
  `anvil_value_get_numeric(v).is_integral`. Inherits like other constraints.
- Fractional bounds: add `bool anvil_type_def_get_min_numeric(def, anvil_numeric_t *out)` and
  `_max_numeric`. Keep the existing `long long` getters unchanged (an ABI-stable surface). They
  return false when the declared bound isn't integral, so they never truncate.
- Range comparison in schema uses the numeric form throughout.
- Violation for non-integral data under `integer := true`: reuse
  `ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH`, with a distinct message ("value must be an integer").
  No new category is needed.

## Test cases

1. `{ type := Numeric; integer := true; }`: `2` passes; `2.5` → one violation; `2.0` → decide and
   document. **Correction (2026-10-25, planning review):** `anvil_numeric_t.is_integral` reflects the
   *spelling*, not the value (`include/anvil_types.h`: "the source text had no '.' and no
   exponent"), so `2.0` and `1e3` report `is_integral = false`. Decision needed:
   *spelling-based* (`integer` ⇔ `is_integral`, so `2.0` fails) or *value-based* (`is_integral ||
   (!overflowed && as_double == trunc(as_double))`, so `2.0` and `1e3` pass). Recommended:
   value-based, because the constraint is about the number. Either way, pin it in a test.
2. `min := 0.25;`: `anvil_type_def_get_min_numeric` returns true with 0.25; `anvil_type_def_get_min`
   returns false.
3. `min := 1;`: both getters return true with 1.
4. Data `0.2` against `min := 0.25` → one RANGE violation.
5. `integer` passes from a `@[types]` definition on to a field that references it.
6. Valgrind-clean; full suite green.

## Dependencies

BR-2610-anvl-types-001 (must land first).

## Resolution

Branch `feat/FR-2610-anvl-types-005`, merged to `main`. Tests TYP29–TYP30 (`test_types.c`) and SCH58–SCH61
(`test_schema.c`); fixtures `schema_integer_*`. Full unit regression green with unchanged counts
elsewhere, `test_release` green, Valgrind-clean on schema, types, anvil_native, anvil_vtable, document,
resolver. Docs: `docs/types-reference.md` Constraints section (and the site mirror).

Decisions:
- **D2 (value-based) as decided in the roadmap**, pinned by SCH58 (`2.0`, `1e+3`, `1.5e+3` pass;
  `2.5`, `1e-1`, `0.5` fail). A value beyond double range, or an integral literal beyond int64, counts
  as whole (a double that large has no fractional bits). `is_integral` is deliberately not used.
- The internal `anvil_type_def_numeric_min/max` added under BR-2610-anvl-types-001 are gone: the
  schema now reads fractional bounds through the new public `_numeric` getters. The `long long`
  getters were already non-truncating (BR-001); their header docs are corrected here.
- `integer` follows the same declared-ness rule as `nullable`: a field's own value, `true` or
  `false`, wins over the type's. Applies only to NUMERIC data values; a null (nullable) or a
  non-numeric value is not an integer-check target.
- Not done: a `CONFLICTING_CONSTRAINTS` diagnostic for `integer := true` with bounds that admit no
  whole number (e.g. `min := 0.2; max := 0.8`). Cheap to add later if wanted.
- Vtable exposure of the new getters belongs to FR-2610-anvl-public-api-003 (Bindings agent).
