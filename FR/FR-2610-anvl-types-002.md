# FR-2610-anvl-types-002: transitive `@[types]` resolution through nested includes

**ID:** FR-2610-anvl-types-002
**Type:** Feature Request (native library)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** resolved
**Tags:** types, registry, includes, rivet-readiness

---

## Summary

`anvil_type_registry_load_from_includes` walks only a document's *direct* includes
(`docs/types-reference.md`, "Using a type from another document"). If `ticket.types.anvl`
includes `common.types.anvl`, then a schema that includes only `ticket.types.anvl` can't see
`types.Timestamp`. The schema has to include every types file it transitively depends on,
by hand.

## Motivation

Layered type files are the normal shape for a real model. Shared primitives (IDs, timestamps,
emails) live in one file, and domain files build on them. Requiring every leaf schema to repeat
the whole include chain is fragile, and it gets worse each time a domain file adds an include.

## Design

- Walk the include graph depth-first in include order, visiting each document at most once
  (cycle-safe; the core already resolves include cycles at load time, so this only needs a
  visited set).
- Merge order: deeper includes first, then the including document's own definitions. A
  definition closer to the consumer overrides a deeper one, and FR-2610-anvl-types-001 reports
  that as a `NAME_COLLISION` diagnostic.
- Only documents carrying `@[types]` contribute. A non-types document in the chain is traversed
  (its includes may be types files) but contributes no definitions.
- Keep `anvil_type_registry_load_from_includes` as the entry point. Direct-only behavior is not
  kept as a mode, since no consumer depends on the narrower behavior.

## Test cases

1. Schema includes A.types; A.types includes B.types, which defines `Id`. `anvil_type_resolve(reg, "Id")` succeeds.
2. Include chain A → B → A (cycle) terminates, and each definition is registered once.
3. A non-`@[types]` document N sits between the schema and B.types (schema → N → B.types): `Id` from B resolves.
4. A and B both define `Id`, with A including B: the registry returns A's definition, and one
   collision diagnostic is reported (once FR-2610-anvl-types-001 lands; until then, A wins
   deterministically).
5. `docs/types-reference.md` "Using a type from another document" describes transitive behavior.
6. Valgrind-clean; full suite green.

## Dependencies

Soft: FR-2610-anvl-types-001 (diagnostics for collisions). Can land first without it.

## Resolution

Branch `feat/FR-2610-anvl-types-002`, commit `43ee36b`, merged to `main`. Tests TYP22–TYP26 (`test_types.c`) with fixtures
`trans_*`, `cyc_*`, `ovr_*`, `dia_*`. Full unit regression green with unchanged counts elsewhere,
`test_release` green, Valgrind-clean on types, schema, anvil_native, anvil_vtable, document, resolver,
module, header. Docs: `docs/types-reference.md` and `site/assets/docs/types-reference.md` (SR-2610-anvl-docs-001 item 5).

Finding - **test case 2's premise is wrong.** The ticket says "the core already resolves include
cycles at load time". It doesn't: an include cycle is a load *error* (`ANVL_ERR_INCLUDE_CYCLIC`,
"Cyclic include detected" on the requesting document), and the bodies of a cyclic graph are never
parsed, so `Alpha`/`Beta` in A -> B -> A cannot be registered by any registry code. TYP23 therefore
asserts what is achievable and true: the walk terminates, doesn't crash, invents no collision, and
registers nothing from the unparsed graph. The visited set (keyed by document path) is still
there, so the walk doesn't depend on the core's behavior; it is also what makes a diamond include
contribute once (TYP26). If A -> B -> A should yield both types, that is a core include-loader
change, not a registry one.

Also: the entry document's own definitions are still not merged (unchanged behavior; it is the
consumer, not an include). The mirror in `site/assets/docs/` was missing the FR-2610-anvl-types-001
diagnostics section; it is synced here.
