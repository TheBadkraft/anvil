# FR-2610-anvl-bindings-001: expose the type registry and AnvilSchema through the bindings — `anvil.node` first

**ID:** FR-2610-anvl-bindings-001
**Type:** Feature Request (bindings)
**Owner:** anvil-node (first), then anvil-py and anvil-net (same surface, follow-on slices)
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet, flywire (downstream consumers)
**Status:** open — not started
**Tags:** bindings, anvil-node, anvil-py, anvil-net, schema, types, rivet-readiness

---

## Summary

`anvil_type_registry.h` and `anvil_schema.h` are C-only today. No binding (`anvil.node`,
`anvil.py`, `anvil.net`) calls any `anvil_type_registry_*` or `anvil_schema_*` function. Any
consumer outside C, which today means every real consumer, can parse ANVL but can't use the
type system or schema validation the README marks as implemented.

## Motivation

FlyWire (Node) re-implements validation in JavaScript because of this gap. Rivet's backend will
be Node, Python or .NET, and needs schema validation on every write.

## Design

Same sequencing as `FR-2609-anvl-writer-001` used: native surface first, then one binding at a
time, each its own slice.

1. **anvil.node** (unblocks FlyWire):
   - `TypeRegistry.fromIncludes(doc)`, `.find(name)`, `.resolve(name)`, `TypeDef` with
     `kind`, `size`, `min`, `max`, `values` (getters return `undefined` when not declared), and
     `diagnostics` once FR-2610-anvl-types-001 lands.
   - `Schema.load(doc)`, `.validate(doc)`, `.validateValue(value, { skipRequired, allowUnknown })`
     (FR-2610-anvl-schema-002), returning `{ valid, violations: [{ category, field, message }] }`.
     Categories are exported as a frozen enum object mirroring the C names.
   - `schema.fields` introspection (FR-2610-anvl-schema-005) when it lands.
   - Explicit `dispose()` plus a FinalizationRegistry safety net, matching how the binding
     manages document lifetimes today.
2. **anvil.py**: the same surface in Python idiom (context managers, `TypeDef.min is None` when undeclared).
3. **anvil.net**: the same surface in .NET idiom (`IDisposable`, `bool TryGetMin(out long)`).

Each binding exposes exactly what the native layer exposes. A native FR that adds surface
(types-003/004/005, schema-001/003/004/005) adds its binding accessors in that binding's next slice.

## Test cases

1. anvil.node: loading a `@[schema]` fixture plus a data fixture with one REQUIRED and one VALUES
   violation returns `valid === false` and exactly those two violations, with the native category,
   field and message.
2. anvil.node: `validateValue` on the `set` object of a FlyWire-style create body returns `valid === true`.
3. anvil.node: `TypeDef.min` is `undefined` for a type without `min`, and `0` for `min := 0;`.
4. anvil.node: using a `Schema` after `dispose()` throws, not crashes; double dispose is a no-op.
5. anvil.node: the existing binding test suite stays green; the addon still builds with
   `-fvisibility=hidden` and exports no anvil/sigma symbols.
6. anvil.py and anvil.net: tests 1–4, ported, each in its own slice.
7. `site/assets/docs/Bindings-Guide.md` documents the new surface per binding.

## Dependencies

None to start slice 1 (the current native surface is enough). Later accessors track the native FRs above.

**Correction (2026-10-25, planning review):** "the current native surface" means what `anvil.node`
actually vendors. Its pin is `52f29bf` (`v0.8.0-rc-36`), which does carry types and schema, so
slice 1 can start there. But no native fix or addition in this FR set (BR-2610-anvl-types-001
onward) reaches Node until `SR-2610-anvl-vendor-bump-001` bumps that pin. Treat the bump as a
prerequisite for every slice after the first. `anvil.wasm`'s build doesn't compile types or schema
at all today.
