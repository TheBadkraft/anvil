# BR-2610-anvl-types-001: numeric constraints and values are read with `strtoll` — fractional and exponent forms silently truncate

**ID:** BR-2610-anvl-types-001
**Type:** Bug Report (correctness)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer, via flywire)
**Status:** resolved
**Tags:** types, schema, numeric, constraints, correctness, rivet-readiness

---

## Summary

Both `src/anvil_types.c` and `src/schema/schema.c` read every numeric constraint (`size`, `min`,
`max`) and every numeric *data* value through a private `read_numeric()` helper that copies the
value's text into a 32-byte buffer and calls `strtoll(text, NULL, 10)`. Anything that isn't a
plain decimal integer is truncated without any signal:

- a declared `min := 0.5;` is stored as `0`;
- a data value `3.7` is compared as `3`, so it passes `max := 3`;
- `5e+10` reads as `5`;
- text longer than 31 characters is cut before parsing.

Both helpers carry a comment saying no numeric accessor exists on `anvil_value`. That is no longer
true: `anvil_value_get_numeric()` (`include/anvil_flat.h`, landed under FR-2609-anvl-value-get-numeric)
returns an `anvil_numeric_t` with `is_integral` / `overflowed` flags. (It landed under the 2026-10-09
amendment to `FR-2609-anvl-public-api-001` decision #6. There's no separate ticket by that name.
Note that `is_integral` reflects the *spelling*: `3.0` reports false.)

**Implementation note (planning review):** comparisons must stay exact across the two
representations. Compare int64 to int64 directly. Compare a fractional value against an integral
bound as doubles only when the integral side is within ±2^53. Outside that range, fall back to
comparing the integral side as `long double` or via `anvil_value_get_bytes`, so a large integer
bound near 2^63 doesn't round. An `overflowed` data value's magnitude exceeds every finite
bound, so compare it by sign: a positive one violates any `max` and satisfies any `min`, and vice
versa. Never truncate it or pass it silently.

## Where

- `src/anvil_types.c` — `read_numeric()` (≈ line 100), used for `size`/`min`/`max` on type definitions.
- `src/schema/schema.c` — `read_numeric()` (≈ line 63), used for inline field-rule `size`/`min`/`max`
  (≈ lines 165–174) and for the data-side range comparison in `anvil_schema_validate` (≈ line 379).

## Reproduction

Schema field `score := { type := Numeric; max := 3; };`, data `score := 3.7;`.
Expected: one `ANVIL_SCHEMA_ERR_VALIDATION_RANGE` violation. Actual (by reading the code path):
`strtoll("3.7")` → `3`, `3 > 3` is false, so no violation is reported.

## Impact

Range checks on any non-integer numeric field are wrong, and wrong without any warning. Any consumer
that relies on AnvilSchema for write validation (FlyWire, Rivet) would accept out-of-range values.

## Expected behavior

- Constraint declarations and data values are read through `anvil_value_get_numeric()`.
- Comparisons work for integral and fractional values, including exponent form.
- A constraint declaration that isn't numeric, or that overflows, is not silently treated as 0.
  It is reported through the diagnostics FR-2610-anvl-types-001 / FR-2610-anvl-schema-004 add.
  Until those land, it is treated as "constraint not declared."
- `size` stays integral. A fractional `size` is a malformed declaration, not a truncation.

## Test cases

1. Type `{ type := Numeric; min := 0.5; }`: `anvil_type_def_get_min` reports the constraint, and a
   data value `0.2` validated against a field of that type yields one RANGE violation; `0.6` yields none.
2. Field `{ type := Numeric; max := 3; }` with data `3.7` → exactly one RANGE violation; data `3` → none.
3. Field `{ type := Numeric; max := 1e+3; }` with data `999` → none; `1001` → one RANGE violation.
4. A numeric data value whose text is longer than 31 characters is compared by its real magnitude.
5. Neither `anvil_types.c` nor `schema.c` contains a `strtoll`-based numeric reader after the fix.
6. Full unit suite green and Valgrind-clean.

## Notes

The public getters `anvil_type_def_get_min/max/size` take `long long *`. Fractional bounds need a
public API decision (widen the out-param, or add `_get_min_numeric` variants). That decision is
scoped in FR-2610-anvl-types-005. This BR fixes the truncation for integral bounds and the
data-side comparison now, and must not wait on that FR.

## Resolution

Branch `fix/BR-2610-anvl-types-001`, commit `7b1b718`. Both `read_numeric()` helpers are gone; constraints
and data go through `anvil_value_get_numeric()` with exact comparison (`include/internal/numeric_bound.h`,
defined in `anvil_types.c`). New tests: SCH13–SCH20 (`test_schema.c`), TYP11 (`test_types.c`).
All unit suites green; `make schema val` and `make types val` Valgrind-clean.

Findings for the owner:
- Test case 1 says `anvil_type_def_get_min` "reports the constraint" for `min := 0.5`. A `long long`
  getter cannot do that without truncating, so it returns **false** for a fractional bound (matches
  FR-2610-anvl-types-005's design). Schema inheritance still sees the exact bound through an internal accessor.
- An exponent with no sign (`9.3e18`) is not a numeric literal in this grammar (type mismatch); `e+18` is.
- `make fixtures test` fails to build and a bare `make test` runs nothing (pre-existing; not touched).
