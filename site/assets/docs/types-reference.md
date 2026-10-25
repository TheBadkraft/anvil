# Anvil Types — Language Reference

Status: **partially implemented.** The grammar and semantics below are settled design (see
`notes/native-schema.md` for the full design record); the runtime support column in each section
says exactly what `types.c` (`src/anvil_types.c`, `include/anvil_type_registry.h`) can actually
do today versus what's designed but not yet built. Schema (`@[schema]`, validating a *data*
document's shape against a type-annotated field list) is design-only — nothing is implemented for
it yet; this reference covers types only.

## What this is

Anvil's native type system is opt-in, strong typing layered on top of AML's own default — a weak,
untyped model where `foo := 42;` is simply a numeric value with no declared shape. Types let you
name a reusable, constrained shape (`VIN`, `AssetStatus`) once and reference it by name wherever
you need it, instead of repeating the same constraints inline everywhere. Nothing about this
changes AML's own default behavior — a document that never opts in never has to think about it.

## Opting in — `@[types]`

A document that *defines* types carries the `@[types]` module attribute, at the top of the file,
before any statements:

```anvl
#!aml
@[types]

VIN := {
   type := String;
   size := 17;
};
```

`@[types]` has exactly one meaning: **every top-level statement in this document is a type
definition, no exceptions.** There's no such thing as an ordinary data statement at the top level
of a `@[types]` document — if you want plain data alongside custom types, that data goes in a
different, unattributed document.

The conventional file extension is `.types.anvl` — not enforced by the parser (nothing about the
filename is special), just a naming convention so a `.types.anvl` file is recognizable at a
glance, the same way `.schema.anvl` already is.

**Consuming** a type — referencing `types.VIN` from some other document — needs no attribute of
its own. Access to the `types.` namespace comes entirely from what a document `include`s: if a
document includes a file whose header carries `@[types]`, every type that file defines becomes
available as `types.<Name>` inside the including document.

*Runtime support: implemented — `@[types]` detection and the every-statement-is-a-definition
rule (`anvil_type_registry_load` rejects any document without the attribute), and cross-file
`types.X` resolution via `include` (`anvil_type_registry_load_from_includes`, see "Using a type
from another document" below).*

## Defining a type

A type definition is a flat, standalone object — a name, a `type :=` field naming what it's built
on, and whatever constraints it needs:

```anvl
@[types]

VIN := {
   type := String;
   size := 17;
};

AssetId := {
   type := Numeric;
};
```

Type definitions never extend or inherit from one another. This isn't a missing feature — it's a
deliberate design choice: a type is an **anonymous, immutable** definition (a frozen shape, not a
mutable class with subclassable state), so "extending" one is a category error, not something to
add later. Each type stands alone.

*Runtime support: implemented. `anvil_type_registry_load` walks every top-level statement and
reads its `type :=` field.*

## The native primitives

Every type definition's `type :=` field names one of six native primitives, or the special `enum`
kind:

| Primitive | Meaning |
|---|---|
| `Numeric` | Any ANVL numeric value |
| `String` | Any ANVL string value |
| `Bool` | A boolean value |
| `Object` | A nested object shape |
| `Tuple` | A fixed tuple shape |
| `Array` | A variable-length array |
| `enum` | An engine-understood enumeration — see below |

Whether `Blob` becomes its own distinct primitive, or stays folded into `String` (structurally,
today, a blob is already a string with a tag attribute), is still an open design question.

*Runtime support: implemented (`anvil_type_kind` / `anvil_type_def_get_kind`) for all seven kinds
listed above.*

## Constraints

Beyond `type :=`, a definition can declare additional constraints — `size`, `values`, `min`,
`max` — the same vocabulary FlyWire's own real, production `.meta.anvl` schema files already use
for individual fields, now available on a named, reusable type instead of repeated inline:

```anvl
VIN := {
   type := String;
   size := 17;
};
```

*Runtime support: implemented (`anvil_type_def_get_size`/`get_min`/`get_max`). Each returns
whether the constraint was actually declared, not just a default — a type with no `min` and one
whose `min` happens to be 0 are distinguishable.*

## Enums

An enum type declares its membership as a plain array of **bare, unquoted** identifiers — no
`label`/`value` pairs needed for the common case:

```anvl
AssetStatus := {
   type := enum;
   values := [ active, maintenance, out_of_service ];
};
```

Each entry gets both a string label (its own text) and an implicit numeric ordinal (its position
in the array, starting at 0) — `active` is ordinal 0, `maintenance` is 1, `out_of_service` is 2.
Non-sequential ordinals (matching a database enum with intentional gaps) and flag-style
(bitmask) enums are real, flagged future work, deliberately deferred rather than front-loaded.

*Runtime support: implemented (`anvil_type_def_get_value_count`/`get_value`) — each member's own
label text is readable by its ordinal (array index).*

## Object types with `fields`

An `Object` type may describe its own shape, reusable by every schema field that references it:

```anvl
@[types]

Person := {
   type := Object;
   fields := {
      id   := { type := Numeric; required := true; };
      name := { type := String; size := 40; };
   };
};
```

A schema field `owner := { type := types.Person; };` then validates the nested contents of `owner`
(`owner.id` is required, `owner.name` is size-checked, any other member is an unknown field).
The nested rules use the same vocabulary as top-level rules (and nest further, including
`type := Array` with `items := { ... }`, `min_items` and `max_items`). If the schema field declares
its own `fields`, they replace the type's; the two are never merged. A type whose `fields` refer back
to itself (directly or through another type) is reported by the schema as a `SELF_REFERENCE`
diagnostic and is not expanded. Violations name the full path, e.g. `links[2].target`.

## Nullable values

A field with a declared type rejects an explicit `null` (`note := null;`) as a type mismatch, the
same as any other wrong-kind value. `nullable := true` makes `null` a legal value for it, and a
`null` then skips every other constraint (`size`, `min`/`max`, `values`, nested `fields`). Absent and
null stay different things: an absent field is governed only by `required`, so
`required := true; nullable := true;` means "must be present, and may be null". `nullable` can be
declared on a type, is inherited by a field that references it (a field's own `nullable := false`
overrides it), and applies at every depth, array items included. A field with no declared type has
nothing for `null` to violate, so it accepts it either way.

## Using a type from another document

A schema, or any other document, brings a type into scope by including the file that defines it,
then referencing it by its `types.` — prefixed name:

```anvl
include "vehicle_types.anvl";

vin_serial := { type := types.VIN; required := true; };
```

`types.` is a namespace the engine exposes once a document has included a `@[types]` file — it
has nothing to do with that file's own name. Two different `.types.anvl` files could both define
types reachable as `types.X`, `types.Y` regardless of which physical file each include came from.

Includes are followed **transitively**. If `ticket.types.anvl` includes `common.types.anvl`, a
schema that includes only `ticket.types.anvl` still sees `types.Timestamp` from `common.types.anvl`;
it does not have to repeat the chain. A document without `@[types]` in the middle of a chain is
walked through (its own includes may be types files) but contributes no definitions.

*Runtime support: implemented (`anvil_type_registry_load_from_includes`) — walks `doc`'s include
graph depth-first in include order, visiting each document once, and merges every
`@[types]`-carrying document's definitions into a single combined registry, keyed by bare name.
Deeper includes merge first and the including document's own definitions after, so a definition
closer to the consumer overrides a deeper one. `types.` really is a flat namespace, regardless of
which file each name actually came from. Any name collision resolves last-write-wins, and is
reported (see Diagnostics below). An include cycle is a load error in the core
(`ANVL_ERR_INCLUDE_CYCLIC`) and its bodies are never parsed, so a cyclic graph yields no
definitions; the walk itself still terminates.*

## Diagnostics

Loading a registry never fails on a bad definition; it records what it noticed, and the consumer
decides whether that is fatal. `anvil_type_registry_get_diagnostic_count` /
`anvil_type_registry_get_diagnostic` list them in the order found, and each diagnostic has a
category, the type's name, the path of the document it came from, and a message
(`anvil_type_diagnostic_get_*`, snprintf-style buffers). Diagnostics are copied out at load time,
so they stay readable after the source documents are disposed.

| Category | Meaning | Effect on the registry |
|---|---|---|
| `ANVIL_TYPE_DIAG_NAME_COLLISION` | a name was defined more than once; the message names both documents | the later definition wins |
| `ANVIL_TYPE_DIAG_UNKNOWN_BASE_TYPE` | `type :=` is missing or names no recognized kind | not registered |
| `ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT` | `size`/`min`/`max` isn't a valid number (`size` must be a non-negative integer) | that constraint is treated as not declared, never as `0` |
| `ANVIL_TYPE_DIAG_MALFORMED_ENUM_VALUES` | `values` is not an array | no labels are declared |

A schema that includes types files reports these too: `anvil_schema_load` copies the registry's
diagnostics into the schema's own list (`anvil_schema_get_diagnostic`, category
`ANVIL_SCHEMA_DIAG_TYPE_REGISTRY`, with the registry's category under
`anvil_schema_diagnostic_get_type_category`), followed by the schema's own: `UNKNOWN_TYPE` (a typo
such as `types.Prority`), `MALFORMED_CONSTRAINT`, `RULE_NOT_OBJECT`, and `CONFLICTING_CONSTRAINTS`
(`min` greater than `max`). Unknown rule keys such as FlyWire's `pooled` or `mask` are never
diagnostics. A schema with diagnostics still loads and validates; a CI gate is simply
"`anvil_schema_get_diagnostic_count` is 0".

## What's not built yet

For a complete picture, the pieces of this design that are settled but not yet implemented:

- Inline strong-typing on an arbitrary value via `@[type=types.VIN]` (already legal grammar —
  attributes are valid on any AML statement, at any nesting depth — but nothing interprets it
  yet).
- Type inference through inheritance (`base`) — architecturally free once built, since inherited
  fields already alias their ancestor's real statement object, attributes and all.
- Everything about `@[schema]` and schema validation — a separate, not-yet-started module
  (`schema.c`) that will depend on `types.c`, never the reverse.

See `notes/native-schema.md` for the full design record, including the reasoning behind each
decision above.
