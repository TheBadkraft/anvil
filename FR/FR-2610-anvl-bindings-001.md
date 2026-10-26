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

## Decisions (planning, 2026-10-25, owner-approved)

Taken on the Bindings agent's slice-1 findings.

1. **New slice 1b (`anvil.node`, next): `schema.validateField(docText, name, opts)`.** In one native
   call, it parses the document text, finds the top-level statement `name`, and runs
   `anvil_schema_validate_value` on its value. It returns the same `{ valid, violations }` shape as
   `validate`. If the statement is missing, that's one violation with an empty path. If it isn't an
   object, the native TYPE_MISMATCH. It needs no native change. Why: `anvil.node`'s `parse()`
   converts eagerly and frees the native document, so `validateValue` can only take text. FlyWire
   validates the `set` object of a request body it already holds as ANVL text, and without this
   it would have to serialize that object back first. `validateValue` stays as-is. Batch create
   (`set := [ {...}, ... ]`) validates each element. If that's awkward from JS, add
   `validateField(docText, name, { each: true })` with paths `[i].field`, and record the choice.
   Slice 1b also picks up the already-deferred `schema.fields` introspection.
2. **New slice 4 (`anvil.wasm`): the same `TypeRegistry`/`Schema` surface as `anvil.node`.** Same
   names and result shapes, including `validateField`, so FlyWire's server (Node) and browser
   (wasm) sides share one calling convention. Export the native entry points the wrapper needs
   (`EXPORTED_FUNCTIONS`), so the linker stops dropping types and schema. Update the README's
   "Native modules carried". Tests run in Node against the built module, and one smoke test covers
   the `ENVIRONMENT=web` path.
3. **The release waits for all of Phase 4** (this FR's slices 1b, 2, 3 and 4, plus
   FR-2610-anvl-bindings-002). See SR-2610-anvl-distribution-001.
