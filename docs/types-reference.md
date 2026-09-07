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
its own. Access to the `types.` namespace comes entirely from what a document `import`s: if a
document imports a file whose header carries `@[types]`, every type that file defines becomes
available as `types.<Name>` inside the importer.

*Runtime support: implemented — `@[types]` detection and the every-statement-is-a-definition
rule (`anvil_type_registry_load` rejects any document without the attribute), and cross-file
`types.X` resolution via `import` (`anvil_type_registry_load_from_imports`, see "Using a type
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

## Using a type from another document

A schema, or any other document, brings a type into scope by importing the file that defines it,
then referencing it by its `types.` — prefixed name:

```anvl
import "vehicle_types.anvl";

vin_serial := { type := types.VIN; required := true; };
```

`types.` is a namespace the engine exposes once a document has imported a `@[types]` file — it
has nothing to do with that file's own name. Two different `.types.anvl` files could both define
types reachable as `types.X`, `types.Y` regardless of which physical file each import came from.

*Runtime support: implemented (`anvil_type_registry_load_from_imports`) — walks `doc`'s own
direct imports (not the whole transitive graph) and merges every `@[types]`-carrying one's
definitions into a single combined registry, keyed by bare name. `types.` really is a flat
namespace: two different imported files could each define types reachable this same way,
regardless of which file each name actually came from. A name collision between two imports
currently resolves last-write-wins — no error reporting for that yet.*

## What's not built yet

For a complete picture, the pieces of this design that are settled but not yet implemented:

- Inline strong-typing on an arbitrary value via `@[type=types.VIN]` (already legal grammar —
  attributes are valid on any AML statement, at any nesting depth — but nothing interprets it
  yet).
- Type inference through inheritance (`base`) — architecturally free once built, since inherited
  fields already alias their ancestor's real statement object, attributes and all.
- Reporting a name collision when two imports both define the same type name, instead of
  silently taking the later one.
- Everything about `@[schema]` and schema validation — a separate, not-yet-started module
  (`schema.c`) that will depend on `types.c`, never the reverse.

See `notes/native-schema.md` for the full design record, including the reasoning behind each
decision above.
