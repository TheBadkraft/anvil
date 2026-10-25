# FR-2610-anvl-types-004: `format` constraint for String — ISO-8601 date, ISO-8601 date-time, UUID

**ID:** FR-2610-anvl-types-004
**Type:** Feature Request (native library)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** resolved
**Tags:** types, schema, constraints, datetime, uuid, rivet-readiness

---

## Summary

The type vocabulary has no way to say "this string is a timestamp" or "this string is a UUID."
The primitives are `Numeric`, `String`, `Bool`, `Object`, `Tuple`, `Array`, `enum`. A
`Timestamp := { type := String; }` type documents intent, but it can't reject
`created_at := "yesterday";`.

## Motivation

Every ticket record carries `created_at`/`updated_at`/`due_at`, and most IDs crossing system
boundaries are UUIDs. Without a lexical check, malformed timestamps reach Postgres and fail
there, as a 500 instead of a schema violation.

## Design

Primitives, not policy: a *small, closed* set of well-known lexical formats, checked by
hand-written validators (no regex engine; the core stays dependency-free).

- New String constraint `format := <identifier>`, inherited from a resolved type like
  `size`/`min`/`max`.
- Initial set:
  - `iso8601_date`: `YYYY-MM-DD`, with a real calendar check (month 01–12, day valid for the month, leap years).
  - `iso8601_datetime`: RFC 3339 profile, `YYYY-MM-DDTHH:MM:SS[.fraction](Z|±HH:MM)`. The offset is
    mandatory, so a timestamp is never ambiguous.
  - `uuid`: 8-4-4-4-12 hex, case-insensitive, any version.
- Unknown `format` names are a load-time diagnostic (FR-2610-anvl-schema-004 /
  FR-2610-anvl-types-001), not a silent pass.
- New violation category `ANVIL_SCHEMA_ERR_VALIDATION_FORMAT` (appended).
- Public getter `anvil_type_def_get_format(def, buf, buflen)`, same convention as the other getters.
- Explicitly out of scope: arbitrary `pattern :=` regexes, and time-zone-database awareness.

## Test cases

1. `iso8601_date`: `2026-02-28` passes; `2026-02-29` fails (not a leap year); `2028-02-29` passes; `2026-13-01` fails.
2. `iso8601_datetime`: `2026-10-25T14:03:00Z` and `2026-10-25T14:03:00.123-05:00` pass;
   `2026-10-25 14:03:00Z` (space) fails; `2026-10-25T14:03:00` (no offset) fails.
3. `uuid`: `0f8fad5b-d9cb-469f-a165-70867728950e` passes; uppercase form passes; 35-char form fails.
4. Each failure yields exactly one FORMAT violation naming the field.
5. A type `Timestamp := { type := String; format := iso8601_datetime; }` passes the format on
   to a field declared `type := types.Timestamp`.
6. `format := nope` yields a load diagnostic, not a violation on every data document.
7. Valgrind-clean; full suite green.

## Dependencies

FR-2610-anvl-schema-004 (diagnostics) for unknown-format reporting; can land first and treat
unknown formats as "not declared" until then.

## Resolution

Branch `feat/FR-2610-anvl-types-004`, commit `1ed0aec`, merged to `main`. Tests TYP32-TYP33 (`test_types.c`, 42 table cases on
the validators plus the declaration/getter) and SCH68-SCH73 (`test_schema.c`); fixtures
`schema_format_*`. Full unit regression green with unchanged counts elsewhere, `test_release`
green, Valgrind-clean on schema, types, anvil_native, anvil_vtable, document, resolver. Docs:
`docs/types-reference.md` "Formats" (and the site mirror).

Decisions:
- The validators live in `anvil_types.c` behind `include/internal/format.h` (no new source file,
  so no Makefile churn) and are shared with `schema.c`.
- RFC 3339 strictness: `T` and `Z` are upper case only (a space or lower case fails), the offset is
  mandatory, and seconds may be `60` (a leap second, which Postgres also accepts).
- An unknown name on a *type* is `MALFORMED_CONSTRAINT` (the registry has no UNKNOWN_FORMAT category;
  the schema one is reserved for schema fields) and arrives in the schema's list via pass-through.
- `format` is checked on STRING, BARE and BLOB data (a blob's payload bytes); a payload with an
  embedded NUL is invalid. Numbers are not a format target.
- The public type-side getter is `anvil_type_def_get_format` as specified; the internal
  `anvil_type_def_format` is the const-pointer form schema.c uses.
