# FR-2610-anvl-types-003: decide the Blob primitive — and make schema type-checking agree with it

**ID:** FR-2610-anvl-types-003
**Type:** Feature Request (native library / design decision)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** resolved
**Tags:** types, schema, blob, primitives, rivet-readiness

---

## Summary

`docs/types-reference.md` leaves an open question: does Blob become its own primitive, or stay
folded into `String` ("structurally, today, a blob is already a string with a tag attribute")?
Today the code has picked neither in a usable way. `schema.c`'s `kind_matches()` (≈ line 297)
accepts only `ANVIL_VALUE_STRING` and `ANVIL_VALUE_BARE` for `String`, and rejects
`ANVIL_VALUE_BLOB`. No `Blob` primitive exists to declare instead. So a field holding a
backtick blob can't be type-checked at all, except by leaving `type :=` off.

## Motivation

Ticket descriptions, comment bodies and attachments are the natural blob use case. Multi-line
text with no escaping is the reason blobs exist. FlyWire's AMP framing also carries binary
payloads as tagged blobs. Rivet's schema needs to say "this field is long text" and "this field
is a tagged binary payload," and have both validate.

## Design (proposal — decision is part of this FR)

Recommended: **both**.

1. `String` accepts `ANVIL_VALUE_BLOB` as well as quoted and bare strings. This matches the
   documented structural model ("a blob is a string with a tag"), and makes `type := String`
   mean "textual content" whatever the spelling.
2. Add a `Blob` primitive (`ANVIL_TYPE_BLOB`) that accepts *only* blob values. It gets an
   optional `tag := <identifier>` constraint (data blob's `@tag` must equal it, read via
   `anvil_value_get_blob_tag`), and `size` measures the payload byte length via
   `anvil_value_get_bytes`.
3. New violation category `ANVIL_SCHEMA_ERR_VALIDATION_TAG` for a tag mismatch (appended to the enum;
   existing values unchanged).

Alternative to record if rejected: String-only (point 1 alone). That's simpler, but no field
can then require "binary payload with tag X."

## Test cases

1. Field `{ type := String; }` with data ``body := `multi-line text`;`` → no violation.
2. Field `{ type := String; size := 5; }` with a 6-byte blob → one SIZE violation.
3. Field `{ type := Blob; }` with data `"quoted"` → one TYPE_MISMATCH; with a blob → none.
4. Field `{ type := Blob; tag := png; }` with an `@png` blob → none; with an untagged or `@jpg` blob → one TAG violation.
5. `anvil_type_def_get_kind` returns `ANVIL_TYPE_BLOB` for `X := { type := Blob; };` in a `@[types]` doc.
6. Existing String validations for quoted and bare values are unchanged (regression).
7. `docs/types-reference.md` primitives table lists the decision; the open question is removed.

## Dependencies

None. Binding exposure follows via FR-2610-anvl-bindings-001.

## Resolution

Decision **D1: both** (recorded in `notes/roadmap-2610-rivet-readiness.md`). Branch
`feat/FR-2610-anvl-types-003`, merged to `main`. Tests SCH62–SCH67 (`test_schema.c`) and TYP31
(`test_types.c`); fixtures `schema_blob_*`. Full unit regression green with unchanged counts elsewhere,
`test_release` green, Valgrind-clean on schema, types, anvil_native, anvil_vtable, document, resolver.
Docs: `docs/types-reference.md` primitives table and Blob paragraph (and the site mirror).

Notes:
- `ANVIL_TYPE_BLOB` is appended after `ANVIL_TYPE_ENUM`; `ANVIL_SCHEMA_ERR_VALIDATION_TAG` after
  `ITEM_COUNT`. No existing value moved. Bindings that mirror `anvil_type_kind` need the new value
  (FR-2610-anvl-bindings-001).
- `size` on a blob is measured with `anvil_value_get_bytes`, on every other value with
  `anvil_value_get_text`, so a String field given a blob counts bytes, as the ticket specifies.
- A `tag` constraint is checked whenever the data value is a blob and the rule declares a tag
  (not only under `type := Blob`); a non-scalar `tag` is a `MALFORMED_CONSTRAINT` diagnostic.
- The type's tag is held internally (`anvil_type_def_tag`); public introspection of `tag` is
  FR-2610-anvl-schema-005's job.
