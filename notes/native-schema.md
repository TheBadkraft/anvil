# Native Schema & Opt-In Typing

## Status

**Design thread captured, not yet scheduled.** Raised while starting the Node-binding-prep gaps (`notes/public-api.md`) — the repo owner flagged that Anvil's own native schema/opt-in-typing system had been set aside and needs picking back up, and that it's meant to eventually *replace* `../flywire/`'s own ad-hoc schema handling (`@[schema]`-attributed `.meta.anvl` files, `schema-gen/postgres.js`, drift-checking) rather than sit alongside it. Full grammar/implementation deliberately deferred — see "Sequencing" below — but recorded now so nothing is lost.

## The idea, as described

Two related but distinct pieces:

1. **Native schema** — a document (itself ordinary AML, `@[schema]`-attributed) that declares the *shape* another document's top-level statements must have — field presence, field types, and so on. This already has reserved error codes sitting unused in `include/errors.h`'s "Schema Errors (46xx)" block (`ANVL_ERR_SCHEMA_ATTR_MISSING`, `ANVL_ERR_SCHEMA_TYPE_UNRESOLVED`, `ANVL_ERR_SCHEMA_BASE_UNKNOWN`, `ANVL_ERR_SCHEMA_VALIDATION_REQUIRED`, `ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH`, `ANVL_ERR_SCHEMA_VALIDATION_UNKNOWN_FIELD`) — clearly planned for once, never wired up in this rebuild.

2. **Opt-in strong typing** — a schema can define what a type name like `Int32`, `Bool`, or `Float` actually means, or declare an entirely custom type (the repo owner's own example: a `VIN` type for FlyWire's vehicle `assets` table). A schema could `import "types.anvl";` to pull in a shared type-definitions file and use those types when declaring field shapes. "Opt-in" — a document/schema that doesn't care about strict typing never has to touch this; it's an additional layer on top of AML's existing weak-typing default (`document-body-parse.md`'s "Anvil's default is a weak type system"), not a replacement for it.

## Prior art — `anvil.bak/src/schema/schema.c` (671 lines, pre-rebuild)

A real, substantial prior design and implementation existed before this rebuild started, worth reviewing for ideas when this work actually picks up (not copying wholesale — the rebuild's grammar/resolver architecture has moved on since):

- `Schema.resolve` walks a schema document carrying the `@[schema]` module attribute, classifies top-level declarations via phase-2 attributes (`@[schema.enum]`, `@[schema.flags]`) or plain object declarations, and produces an `anvl_schema_ruleset_t`.
- `Schema.validate` walks a *data* document; for every top-level statement whose `base` resolves to a known schema type name, checks field presence and type against the ruleset. Collects every validation error before returning (not fail-fast) — a deliberately different error policy from the parser/resolver's own fail-fast-per-phase approach.
- Notably: this design already used a statement's `base` (inheritance target) as the mechanism for saying "this statement conforms to that schema type" — worth revisiting given `resolution-phase.md`'s own `base`/inheritance work is now complete and could plausibly be reused rather than needing a parallel mechanism.

## Sequencing — recommended: after the Node/JS binding work, not before, not indefinitely deferred

Two things point the same direction:

1. **FlyWire is actively generating real schema patterns right now** — Postgres-table-to-`.meta.anvl` generation, drift-checking, `@[schema]`-attributed files already in real use (`schema-gen/postgres.js`, `schema-registry.js`). The same lesson the buffer-load/attributes/raw-value gap analysis just taught (build against a real consumer's actual usage, not assumptions) applies here even more directly — every additional real pattern FlyWire produces before the native schema grammar is locked in is free design validation.
2. **The current task (closing the 3 Node-binding gaps, then the actual Node binding repo) is close to done and already scoped.** Interrupting it to design a whole new grammar layer fragments focus on both.

**One concrete link acted on now, not deferred**: gap #2 of the Node-binding-prep work (a public accessor for module/document-level `@[...]` attributes) is being built generically — `anvil_document_get_attribute(doc, key) -> value`-shaped, not narrowly scoped to just what FlyWire's `@[schema]` check needs today — specifically so the eventual schema resolver can reuse the exact same accessor for its own `@[schema]` detection, rather than needing a second, redundant mechanism later.

## Open questions (not yet worked through)

- Grammar for type declarations themselves — is a type just another AML object/statement shape (matching how `anvil.bak` classified schema types via ordinary object declarations + phase-2 attributes), or does it need dedicated syntax?
- Whether `base`-as-conformance-marker (the `anvil.bak` approach) is still the right mechanism now that `base`/inheritance has its own, different, already-settled semantics in this rebuild (`resolution-phase.md`) — reusing it for schema conformance *and* field-merging inheritance simultaneously could be a semantic collision, or could be exactly the right unification. Not analyzed yet.
- Fail-fast (matching every other phase in this rebuild) vs. collect-all-errors (matching `anvil.bak`'s validator) for schema validation specifically.
- Where schema validation sits relative to the existing 4-phase pipeline — a phase 5, or an entirely separate, opt-in pass a caller invokes deliberately (matching "opt-in" typing's own framing)?

## Related notes

- `notes/public-api.md` — the Node-binding-prep gap #2 (module attributes) this note connects to directly.
- `notes/resolution-phase.md` — `base`/inheritance semantics that may or may not interact with schema conformance.
- `include/errors.h` — the already-reserved, unused Schema Errors (46xx) block.
- `deferred-work.md` — index entry pointing here.
