# AnvilSchema

Status: **implemented.** `@[schema]` loading, required-field presence, type-kind validation,
nested `Object` and `Array` shapes with full-path violations, `nullable`, `integer`, `format` and
the `Blob` primitive with `tag`, exact numeric bounds, `size`/`min`/`max`/`values` (including a
constraint inherited from a resolved `types.X` custom type), validation of a bare object value
(`anvil_schema_validate_value`), load diagnostics, read-back introspection and extension keys.
Every violation is collected in one pass; validation never stops at the first.

This page is the introduction, then a short tour of what a schema can say today
([jump to the tour](#a-schema-end-to-end)). The detail behind each rule lives in
[`types-reference.md`](../types-reference.md), the reference for the type vocabulary and the
schema behaviour that depends on it; the C declarations are in `include/anvil_schema.h`, shipped
in the [native library download](/download). See also the
[Language Reference](../language-reference.md#10-schema) and [Getting Started](../getting-started.md).

## The question nobody asks about their schema tool, until it's too late

Every format that's succeeded long enough eventually needs a way to say "this document must
look like *this*." XML got there. JSON got there. Every one of them solved it the same way:
by inventing something new.

XML got XSD — its own W3C specification, its own grammar, its own processor, entirely separate
from the XML parser that reads the document XSD is describing. Want to transform that document
instead of just validating it? That's XSLT — a *third* specification, its own declarative
language, its own processor again. Three technologies, three grammars, three things to learn,
none of which the original XML parser has ever heard of.

JSON's story is the same shape, just later. JSON shipped with no validation concept at all —
`{"age": 42}` is exactly as valid as `{"age": "not a number"}` as far as any `JSON.parse()` in
the world is concerned. JSON Schema arrived years afterward, an external specification — written
*as* JSON, which is almost a joke — that no JSON parser has ever been taught to read. Validating
against it means reaching for a second library entirely, maintained by a different project, on a
different release schedule, with no structural guarantee it even stays in step with whatever
`JSON.parse()` itself considers valid JSON.

Nobody asks "wait, why does validating my data require an entirely separate piece of software
from the thing that reads my data?" They just accept it, because every format they've ever used
works this way. It's not a law of nature. It's just what happens when schema gets bolted on
*after* the format already shipped without one.

## What's actually different here

A `.schema.anvl` file is parsed by the exact same `anvil_load()` call, running the exact same
grammar, as any other `.anvl` file on earth. There is no second parser. There is no second
specification. We checked, directly, against the real parser, before writing a single line of
schema-specific code: every piece of grammar a schema file needs — attributes on any statement at
any nesting depth, ordinary objects, includes — was already there, fully implemented and tested,
before this design thread even started. Zero new grammar was required. Not "we kept it minimal."
Zero.

That's not a coincidence and it's not a clever trick. It's what falls out of building one
language, one grammar, one parser in the first place, instead of a format plus a growing pile of
things bolted onto it later. A schema document doesn't need special syntax because it was never
going to need a special *anything* — it's just data, describing other data, the same way every
other ANVL document describes whatever it describes.

## AnvilSchema is a consumer, not a feature

Here's the part worth sitting with: Anvil Native doesn't have schema support built in. It never
will, exactly. What it has is a public API — read a document's statements, read their values,
read their attributes, walk what they include — available in full to *any* consumer, for *any*
purpose. AnvilSchema is what you get when that generic API gets pointed at one specific job:
reading a document that says `@[schema]` and checking another document's shape against it.

Nothing about that job is privileged. Nothing about it required Anvil Native to change its own
rules, add a special code path, or make an exception for schema's sake. AnvilSchema is a
demonstration of what the primitives already support, not a capability someone had to go add.
That's the whole point of building primitives instead of policy: the library's job stops at
"parse correctly and expose what's there, generically, to anyone" — what any particular consumer
*does* with that is entirely up to them, using tools no more special than the ones AnvilSchema
itself uses.

## This is one demonstration, not the whole story

AnvilSchema is the first real answer to "what can you build on top of ANVL that ANVL itself
never had to know about." It won't be the last, and it isn't the deepest example available
either — a scripting language (AnvilScript) is next on the list, and it's built on the exact same
premise: nothing special, just the primitives, aimed at a different job. We haven't come close to
scratching the surface of what "one parser, generic primitives, extend however you need" actually
makes possible. AnvilSchema is just the first proof.

## A schema, end to end

A schema is an ordinary `.anvl` document that carries the `@[schema]` module attribute. Every
top-level statement in it is one field rule, without exception:

```anvl
#!aml
include "common.types.anvl";          // a @[types] file: its types become types.<Name>
@[schema]

id        := { type := Numeric; required := true; integer := true; min := 1; };
title     := { type := String; required := true; size := 80; };
status    := { type := enum; values := [ open, closed ]; };
closed_at := { type := String; format := iso8601_datetime; nullable := true; };
owner     := { type := types.Person; };
links     := {
   type := Array;
   min_items := 0;
   max_items := 10;
   items := { type := Object; fields := { target := { type := String; required := true; }; }; };
};
```

`include` lines come first in the header, before the `@[schema]` attribute. Load it with `anvil_schema_load(doc)` and check a data document with `anvil_schema_validate`. The
schema copies everything it needs out of the document at load time, so it stays usable after the
schema document is disposed. A malformed schema is still returned, never rejected: what is wrong
with it is reported as diagnostics (below), and the caller decides whether that is fatal. Load the
data document yourself and check `anvil_has_errors` before validating it.

### Rules

- **Shape.** An `Object` rule carries `fields := { name := { <rule> }; ... }`; an `Array` rule
  carries `items := { <rule> }` for every element, plus `min_items` and `max_items`. Rules nest to
  any depth, and a `@[types]` `Object` type may carry `fields` too, reusable by any schema field
  that references it. A schema is closed: a data field the schema does not declare is an
  `UNKNOWN_FIELD` violation, at every depth. Violations name the full path, for example
  `links[2].target`; a top-level field is just its name.
- **`nullable`.** A declared type rejects an explicit `null` unless the rule says
  `nullable := true`; a `null` then skips every other constraint. An absent field is governed only
  by `required`, so `required := true; nullable := true;` means "must be present, may be null".
- **`integer`.** `integer := true` requires a numeric value to be whole, judged by its value, not
  how it is written: `2`, `2.0` and `1e+3` pass; `2.5` fails.
- **Exact bounds.** `min`, `max` and the values they are compared with are exact. They are never
  truncated or rounded, so `min := 0.5` means 0.5 (it once behaved as 0), and an integer near 2^63
  does not round. A bound that is not a valid number is a `MALFORMED_CONSTRAINT` diagnostic and is
  treated as not declared, never as 0.
- **Strings and blobs.** `type := String` accepts a quoted string, a bare word or a backtick blob.
  `type := Blob` accepts only a blob, and may declare `tag := png;`, in which case the data blob's
  `@tag` must match exactly (`ANVIL_SCHEMA_ERR_VALIDATION_TAG` otherwise). `size` counts bytes,
  and for a blob, payload bytes. An `enum` does not accept a blob.
- **`format`.** A string can be required to match `iso8601_date`, `iso8601_datetime` or `uuid`
  (see the table in [`types-reference.md`](../types-reference.md#formats)). The set is closed, and
  checked by hand-written validators: no regular-expression engine, no time-zone database. A
  mismatch is `ANVIL_SCHEMA_ERR_VALIDATION_FORMAT`.

### `iso8601_datetime` is the strict RFC 3339 profile

The form is `YYYY-MM-DDTHH:MM:SS[.fraction](Z|+HH:MM|-HH:MM)`, checked strictly by the library:

| Accepted | Rejected | Why |
|---|---|---|
| `2026-10-29T12:30:00Z` | `2026-10-29 12:30:00Z` | the separator must be an upper-case `T`, not a space |
| `2026-10-29T12:30:00.123+05:30` | `2026-10-29T12:30:00+05` | the offset is `Z` or a full `+HH:MM` / `-HH:MM`, never a short `+HH` |
| `2026-12-31T23:59:60Z` | `2026-10-29T12:30:00` | an offset is required, so no timestamp is ambiguous; seconds may be 60 (a leap second) |
| | `2026-10-29t12:30:00z` | `T` and `Z` are upper case |

The date part gets the same real calendar check as `iso8601_date` (month length, leap years). A
space separator and a short `+HH` offset are what PostgreSQL's default text output produces, so a
timestamp read from a database must be **normalized to RFC 3339 before it is validated**; the
schema will not accept it as written.

### Validating a value, not a document

`anvil_schema_validate_value(schema, object_value, &opts)` checks an object value (a request body
such as `set := { ... };`) as if its statements were a document's top level. The options struct
(set its `size` to `sizeof`) has two switches:

- `skip_required`: do not enforce required-presence (patch semantics); every other check still
  applies to each field that is present.
- `allow_unknown`: do not report fields the schema does not declare.

Both default to off, which is exactly `anvil_schema_validate`'s behaviour. A value that is not an
object yields one `TYPE_MISMATCH` violation with an empty field path. Like `anvil_schema_validate`,
each call replaces the violations of the previous call on that schema, so to validate the elements
of `set := [ {...}, {...} ]` read the violations between calls.

### Diagnostics

Something wrong with the schema itself (as opposed to a violation, which is something wrong with
data) is a diagnostic. A schema with diagnostics still loads and still validates, with one
exception that fails closed: a field whose `type` is an `UNKNOWN_TYPE` accepts no value. Every
*present* value of that field, at any depth (top level, a `fields` member, an array `items` rule),
is an `ANVIL_SCHEMA_ERR_VALIDATION_UNRESOLVED_TYPE` violation, an explicit `null` included even
with `nullable := true`; `skip_required` and `allow_unknown` do not excuse it. An absent field is
judged only by `required`. Other weakened rules simply check less. `anvil_schema_get_diagnostic_count` / `get_diagnostic` list them in the
order found: the type registry's own diagnostics first (name collisions, unknown base types,
malformed constraints, malformed enum values), then the schema's: `UNKNOWN_TYPE` (a typo such as
`types.Prority`), `UNKNOWN_FORMAT`, `MALFORMED_CONSTRAINT`, `RULE_NOT_OBJECT`, `SELF_REFERENCE`
(a type whose `fields` refer back to itself) and `CONFLICTING_CONSTRAINTS` (`min` greater than
`max`). A well-formed schema has zero diagnostics, which makes a good CI gate. Diagnostics are
copies, readable until the schema is disposed.

### Reading a schema back

A loaded schema can be introspected, read-only. `anvil_schema_get_field_count` / `get_field` /
`find_field` list the top-level rules in declaration order, and the `anvil_schema_field_get_*`
accessors report each rule's effective settings: its kind, the `type :=` as written
(`types.Id32`), `required`, `nullable`, `integer`, and every constraint, with anything the rule
left out filled in from its `types.X`. A constraint getter returns whether it was actually
declared, not a default, so a rule with no `min` and one whose `min` is 0 are distinguishable.
Nested rules are reached through `get_nested` / `find_nested` (an Object's `fields`) and
`get_items` (an Array's element rule).

**Extension keys.** A rule key the schema does not consume (a consumer's own annotation, such as
FlyWire's `pooled` or `mask`) is never a diagnostic. It is exposed as an extra
(`get_extra_count`, `get_extra_name`, `get_extra_value`), in declaration order, on a rule at any
depth. Each value is a copy owned by the schema, readable with the ordinary `anvil_value`
accessors and valid until the schema is disposed, so it survives disposing the source document.
A value that contains a `$variable` reference cannot be copied and reads as NULL.

### Guarantees

- **Text is read in full, NULs included.** Enum labels, `values` membership, long names and
  messages are compared and returned by length, never as C strings, and no fixed-size buffer
  truncates them. A label or an extension value with an embedded NUL survives intact, so read it
  with the returned length (the usual two-call `get_value(NULL, 0)`, then allocate and copy). A
  NUL inside a name that is compared as an identifier (`type`, `format`, `tag`, `required`,
  `nullable`, `integer`) is a malformed value: it is diagnosed and treated as not declared, never
  truncated to a prefix that could match. A statement name or bare word cannot contain a NUL at
  all; the core rejects such a document at load.
- **Thread safety.** Independent loads are safe: any number of threads may call `anvil_load`,
  `anvil_load_buffer` and `anvil_parse_value_fragment` at the same time, on the same text or on
  different text, because each load owns its state and shares nothing with other loads. This
  covers loading a schema and its data. Sharing **one** handle (a document, a schema, a type
  registry) across threads is the caller's job: a handle and everything obtained from it must not
  be used on several threads at once unless the caller serializes it, and must not be used during
  or after its disposal. A schema's violation list is replaced by every validate call, so validate
  with one schema from one thread at a time. The writer and builder are likewise single-thread.
