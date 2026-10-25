# BR-2610-anvl-schema-001: fixed-size stack buffers in `schema.c` silently truncate type names, rule keys and enum comparisons

**ID:** BR-2610-anvl-schema-001
**Type:** Bug Report (correctness, low severity)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** resolved
**Tags:** schema, buffers, truncation, correctness, rivet-readiness

---

## Summary

`src/schema/schema.c` reads text through fixed-size stack buffers and never checks the
length `anvil_value_get_text` / `anvil_statement_get_name` return (both report the *full* length,
snprintf-style):

| Site | Buffer | Effect when exceeded |
|---|---|---|
| `read_field_rule`, rule key name | `char fname[16]` | keys of 16+ chars are compared truncated |
| `read_field_rule`, `type :=` value | `char text[64]` | a long type name resolves by its 63-char prefix, possibly to a different type |
| `read_field_rule`, `required :=` | `char text[8]` | fine for `true`/`false`; anything else is compared truncated |
| `anvil_schema_validate`, `values` membership | `char text[128]` | a data value over 127 chars is compared by its prefix |
| `read_numeric` | `char text[32]` | covered by BR-2610-anvl-types-001 |
| `struct anvil_schema_field_t.name`, set in `anvil_schema_load` | `char name[ANVIL_SCHEMA_NAME_MAX]` (65) | a declared field name of 65+ chars is stored truncated *(added in planning review)* |
| `anvil_schema_validate`, unknown-field pass | `char dname[ANVIL_SCHEMA_NAME_MAX]` | a 65+-char *undeclared* data field whose first 64 chars match a declared field is not reported as UNKNOWN_FIELD *(added in planning review)* |
| violation `field`/`message` storage | `ANVIL_SCHEMA_MSG_MAX` (128) | check it: FR-2610-anvl-schema-001's dotted/indexed paths will exceed any fixed size, so violation fields should be allocated to length *(added in planning review)* |

## Reproduction

A `@[types]` doc defining two types whose names share the same first 63 characters but differ
after that. A schema field referencing the second name resolves to whichever the truncated
63-char prefix finds.

A field with `values := [ <128-char label> ];` never accepts the matching data value, because
the data side is truncated to 127 chars and the stored label is not.

## Impact

Low today: the current rule keys are short. But it's silent, and it conflicts with the project's
own "no silent truncation" stance (the public API returns full lengths precisely so callers can
size buffers).

## Expected behavior

Every read either sizes its buffer from the returned length (query with `NULL, 0`, then allocate),
or detects `returned_len >= sizeof(buf)` and treats the input as non-matching or malformed. It
never compares a truncated prefix.

## Test cases

1. A type name of 80 characters resolves correctly. A second type sharing its first 63 characters
   doesn't capture the reference.
2. `values := [ <200-char label> ]` accepts the identical 200-char data value, and rejects one that
   differs only at character 150.
3. A rule key longer than 15 characters (e.g. a FlyWire-style extension key) isn't mistaken for a
   shorter known key.
4. Valgrind-clean; full suite green.

## Resolution

Branch `fix/BR-2610-anvl-schema-001`, commit `2f3f0af`, merged to `main`. Every site in the table is fixed, plus the
matching sites in `anvil_types.c` (type-def name, rule key, `type :=` text). Reads go through
`include/internal/text_alloc.h` (query the full length, allocate exactly); field names, type names and
violation `field`/`message` are allocated to length, so FR-2610-anvl-schema-001's paths need no further
change here. Tests: SCH21–SCH24 (`test_schema.c`), TYP12 (`test_types.c`); fixtures
`schema_long_types.anvl`, `schema_long_consumer.anvl`.

Findings:
- Test case 3 (long rule key mistaken for a short one) could not fail before the fix: a clipped
  prefix of a longer key never equals a shorter known key. SCH23 stands as a regression guard.
- Also fixed in passing: `collect_type_defs` leaked the `values` list of a def whose `type :=` was
  unrecognised.
- `anvil_type_def_*` has no public name getter, so TYP12 checks long names via `find` and kind only.
- Regression: all 19 unit suites and `test_release` pass with unchanged counts (plus the new tests);
  Valgrind clean on schema, types, anvil_native, anvil_vtable, document, resolver.
