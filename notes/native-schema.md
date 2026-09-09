# Native Schema & Opt-In Typing

## Status

**Active design thread.** Picked back up once both `anvil.node` and `anvil.wasm` reached their
planned surface (the sequencing this note originally called for). Grounded this round against
two real sources before designing anything: the actual current parser grammar (`src/core/
parser.c`) and FlyWire's real, currently-generated `.meta.anvl` files + `schema-registry.js`'s
real field-reading logic — not assumptions about either. Repo owner's framing: "we're going to
use the work FlyWire did for us (for free) to see what a schema looks like in a practical sense
... I said, 'create a schema format that makes sense for FlyWire's needs,' and what you see is
it." Agreed plan (see "Plan" below): spec both schema and types, adapt a schema-engine concept
from `schema-registry.js`, and build it as `schema.c`/`types.c` — optional add-on modules over
the core parser, the same base-library/add-on shape intended for AnvilScript later. Still being
discussed before implementation starts.

## Confirmed today — grammar and API already support this, no core parser changes needed

Two things assumed to need new grammar work turn out to already exist, fully implemented and
public:

1. **`@[...]` attributes are already valid on any AML statement, at any nesting depth, scalar or
   object-valued** — `parse_statement` (`src/core/parser.c:552`) parses an optional `@[...]` block
   *before* dispatching on `:=` vs `{`, and `parse_statement_list` (used for both the document
   body and every nested object body, `OBJECT_BLOCK` and `:= { ... }` alike) calls `parse_statement`
   recursively. So `@[type=Int32]` on a plain scalar field, at any depth, is legal today — the
   repo owner's own stated concern ("this means attributes would have to be valid on any AML
   identifier, scalar or otherwise") is already satisfied, not open grammar work.
2. **AMP already forbids attributes entirely** (AMP15, `Source.dialect(src) == ANVL_DIALECT_AMP`
   gate in `parse_statement`, tested) — confirming the "leaning toward AMP not having attribution"
   is already shipped, existing behavior, not a new decision.
3. **Full public accessor API already exists** for both levels: `anvil_document_get_attribute_count
   /get_attribute/find_attribute` (module-level) and `anvil_statement_get_attribute_count/
   get_attribute/find_attribute` (per-statement), both backed by `anvil_attribute_get_key/
   get_value`. A schema/type engine needs zero new primitives to read `@[schema]`, `@[types]`, or
   `@[type=Int32]` off any node — it's all there.

## FlyWire's real vocabulary (`flywire/schemas/assets.meta.anvl`, `flywire/src/schema-registry.js`)

A live, generated file:

```
#!aml
@[schema, table=assets]

asset_id      { type := int32; };
asset_type    { type := int32; required := true; };
vin_serial    { type := str; size := 17; required := true; };
make          { pooled := true; required := true; };
status        { pooled := true; required := true; values := ["active", "maintenance", "out_of_service"]; };
```

Key findings from reading `schema-registry.js`'s actual `loadFromText()`:

- The module-level opt-in is exactly `@[schema, table=assets]` — a flag (`schema`) plus a
  key=value (`table=assets`) in one comma-separated attribute block. Already-existing grammar,
  confirmed working, zero changes needed.
- **Field metadata is expressed as ordinary object fields, not attributes**: `type`, `size`,
  `required`, `pooled`, `values`, `min`, `max`, `mask`. `pooled` fields omit `type` entirely
  (pooled implies a fixed 4-byte int32 offset into a string pool, not a schema-declared type).
  `int32`/`date` are always 4 bytes (never a declared `size`); `str` declares `size` explicitly.
  `values`/`min`/`max`/`mask` are optional, generated from real Postgres CHECK constraints.
- This is **entirely ordinary AML data** — `schema-registry.js` reads it with nothing but
  `.has()`/`.get()`/`.asString()`/`.asInt()`/`.count`/`.at()`, the exact same generic surface
  `anvil.node`/`anvil.wasm`'s `AnvlNode` already exposes. No parser-level schema awareness was
  ever required to make this work in production.

## Two distinct mechanisms — previously conflated, now separated

1. **Schema files** (`.schema.anvl`, `@[schema]`) describing another document's field shape —
   use ordinary object-value fields, matching FlyWire's proven convention verbatim. This needs a
   ratified key vocabulary (see "Plan"), not new grammar.
2. **Inline strong-typing** on a value *within* any document (schema file or not) — the repo
   owner's `@[type=Int32]` idea, as an alternative to using `base`/inheritance as the
   type-conformance marker. Already legal grammar (see "Confirmed today" above); this is the
   opt-in-typing enrichment layer sitting on top of #1, not a replacement for it.

This also resolves one of the prior open questions below: `base`-as-conformance-marker
(`anvil.bak`'s approach) and `@[type=...]`-as-conformance-marker are no longer in tension by
default, since they're being used for two different things — `base` keeps its already-settled
inheritance semantics (`resolution-phase.md`), and `@[type=...]` is the new, separate mechanism
for direct type annotation. Whether schema *validation* (mechanism #1, matching a data document
against a schema file) should also use `base` as its "which schema type does this statement
conform to" marker is still open.

## Build-structure finding — no existing core/optional module precedent

Checked: there is no top-level build system in this repo. Every consumer (`test/unit/Makefile`,
`anvil.node`'s `binding.gyp`, `anvil.wasm`'s `build.sh`) hand-lists its own `ANVIL_SRCS`/source
array, currently always `src/core/*.c` + `src/sigma/*.c` in full — there's no existing "core vs.
optional" split to extend. Opt-in modules are therefore not retrofitting an existing seam; they
establish the pattern — `src/anvil_types.c` (opt-in, but part of ANVL proper, a sibling of
`core/`/`sigma/` directly under `src/`) and, later, `src/schema/schema.c` (the actual add-on,
layered on top of types, reserving `src/schema/` for itself), each with its own public header
that `anvil_flat.h`/`anvil_vtable.h` know nothing about — which is exactly why the repo owner
flagged this as also setting precedent for how AnvilScript gets added later (itself a direct
consumer of `anvil_types.c`, independent of `schema.c`) — worth designing deliberately, not as
an afterthought.

## Prior art — `anvil.bak/src/schema/schema.c` (671 lines, pre-rebuild)

- `Schema.resolve` walks a schema document carrying the `@[schema]` module attribute, classifies
  top-level declarations via phase-2 attributes (`@[schema.enum]`, `@[schema.flags]`) or plain
  object declarations, and produces an `anvl_schema_ruleset_t`.
- `Schema.validate` walks a *data* document; for every top-level statement whose `base` resolves
  to a known schema type name, checks field presence and type against the ruleset. Collects every
  validation error before returning (not fail-fast) — a deliberately different error policy from
  the parser/resolver's own fail-fast-per-phase approach.
- Used `base` as the sole conformance marker — see "Two distinct mechanisms" above for how this
  now sits alongside, not necessarily replaced by, `@[type=...]`.

## Plan (agreed direction, still being discussed before implementation)

1. **Spec both schema and types** — ratify the key vocabulary (`type`, `size`, `required`,
   `pooled`, `values`, `min`, `max`, `mask`, extended with whatever `@[types]`-defined custom
   types need) and the exact opt-in semantics of `@[schema]` (module header: this document
   declares field shape for other documents) vs. `@[types]` (module header: this document
   defines/extends type names available to `@[type=...]` annotations elsewhere). Extension:
   `.schema.anvl` (distinct from AnvilScript's already-noted `.anvs`).
2. **Adapt a schema-engine concept from `schema-registry.js`** — its `load`/`project`/
   `resolveAssignment`/`resolveCondition` shape (parse once, validate/read against a cached
   ruleset, reject unknown fields early) is real, working, production-proven design to translate
   into C, not reinvent.
3. **Ship as `schema.c` + `types.c`**, optional add-on modules built entirely on the existing
   public flat API (`anvil_statement_get_attribute*`, `anvil_document_get_attribute*` — see
   "Confirmed today") — no core parser/resolver changes required. This is also the first real
   instance of the base-library/optional-add-on structure the repo owner wants to reuse for
   AnvilScript later, so the module boundary (own header, own error namespace reusing the
   reserved 46xx block, own source directory, opt-in per consumer's build) is being designed
   deliberately as a reusable pattern, not a one-off.

AMP: still zero attribute support by existing, already-shipped, tested design (AMP15) — schema/
types remain unavailable on AMP documents directly. The workaround path already sketched:
embed ANVL Value Fragment text in a blob value (decodable via the already-built
`anvil_parse_value_fragment`), mirroring FlyWire's own `@table` blob-tag technique, to carry
type/schema-adjacent information into an AMP payload without changing AMP's grammar.

## Confirmed today (round 2) — derivation and attribute-key grammar

- **Inheritance-derived fields already carry attribute provenance for free.** `merge_inherited_
  fields` (`src/core/resolver.c:174`) merges an ancestor's fields into a derived statement's own
  field list via `List.append(fields, field)` (line 198) — an *aliased pointer to the ancestor's
  actual `anvl_statement`*, never a copy. So a field reached only through `base`/inheritance is,
  after resolution, the literal same object as the ancestor's own field — its `@[type=...]`
  (or any other) attribute is intact and reachable through the *existing* public
  `anvil_statement_get_attribute*` API with zero new work. Type inference through inheritance is
  already possible today, purely as a thin add-on over the public flat API.
- **VarRef (`$ref`)-derived values do not get the same treatment, and can't through the public
  API as it stands** — see the VARREF-transparency finding above (`anvil_types.h:91`,
  `resolve_varref_chain` resolving to a value pointer, not a statement). This is the one
  asymmetric piece: extending inference through `$ref` chains means `schema.c` reaching past the
  public flat API into resolver-internal structures (`context.h`, `varref.target`) — a real,
  deliberate scope increase, not a small addition. Recommend treating inheritance-derived
  inference as in-scope by default (free) and `$ref`-derived inference as a separate, explicit
  decision (still open).
- **Attribute *keys* don't accept the bare-literal character set `types.Int32` would need.**
  `parse_attribute_list`'s key scan (`parser.c:444`) uses `Source.is_identifier_part` — alpha/
  digit/underscore only, no `.` — unlike attribute *values*, which accept anything up to the
  next `,`/`]` (including dots) already. So `@[type=types.Int32]` (dotted reference in the
  *value* position) parses today with zero grammar changes; a bare-flag form like
  `@[types.Int32]` (dotted reference as the *key* itself, no `type=`) does not — it would need
  attribute-key scanning to accept `.`, a small but real, deliberate parser change. Current
  recommendation: keep the fixed `type=` key (free) rather than add the bare dotted-key form,
  unless the terser syntax is wanted enough to justify the change.

## Decided — `@[schema]` opt-in semantics

`@[schema]` marks a document as a schema *definition* — declaring shape, never filling one. A
data document being validated against a schema never carries `@[schema]` or any other
schema-referencing attribute; validation is the producer's or consumer's own responsibility
(load the schema, then check a data document against it as an explicit separate step — the
schema and the data it describes never need to reference each other from inside either
document).

**Superseded below**: this originally also said `@[schema]` implies `@[types]`'s
syntax-recognition, so a schema file wouldn't need to separately write `@[types]`. That framing
assumed a consuming document might need to self-attribute `@[types]` to unlock `types.X` — since
resolved, that assumption doesn't hold, so there's nothing left for `@[schema]` to imply. See
"`@[types]` has exactly one meaning" below.

## Decided — `@[types]` has exactly one meaning; consumption needs no attribute at all

Two rounds of ambiguity, both resolved the same way. First: `VIN { type := str; size := 17; };`
is syntactically identical to an ordinary object statement — nothing about the statement itself
says "type definition" rather than "anonymous object with those fields." Second, sharper: if
`@[types]` were also how a *consuming* document (not itself a type-definitions file) unlocks
`types.X` access, attributing a plain data document that way would force every one of its own
ordinary statements to be misread as type definitions too, per the first meaning — the two roles
can't share one attribute without colliding.

Resolved by giving `@[types]` exactly one meaning, full stop: **every top-level statement in a
`@[types]`-attributed document is a type definition.** It only ever appears on a defining file
(a `.types.anvl`) — there is no "just an object" at the top level of one; a document that wants
ordinary data alongside custom types simply doesn't carry `@[types]`, full stop. A *consuming*
document — a schema file, a plain data document, anything — never attributes itself at all to
gain `types.X` access. That access is granted purely by the import graph: if a document imports a
file whose own header carries `@[types]`, `types.X` references inside the importer resolve
against whatever that imported file registered. No self-declaration on the consumer's side, so no
way for its own statements to be swept up as type definitions by accident. Mirrors how `@[schema]`
files are already 100% schema fields in FlyWire's real usage, nothing else mixed in. Consequence
for `types.c`'s eventual registration pass: walk every root statement of a `@[types]` document
unconditionally as a type definition; one that doesn't have the right shape (no `type :=`) is a
validation error, not something silently treated as plain data.

## Decided — enum: simple sequential case confirmed; gaps/flags explicitly deferred

The proposed shape (`type := enum;` + bare-identifier `values` array, ordinal-by-position) is
confirmed for the common case. Non-sequential ordinals (matching a DB enum with intentional gaps)
and flag-style enums (bitmask semantics, not a single membership pick) are real, flagged
work — "sooner than later," not blocking, not designed yet. Repo owner's own framing: "principled
and disciplined approach, minimalist is likely best" — the escape hatch for the harder cases
should stay proportional, not front-loaded speculation.

## Decided — no type-to-type extension via `base`, on a principled basis

The repo owner's own resolution: types are **anonymous, therefore immutable, therefore by
definition not inheritable**. This supersedes the earlier, weaker pragmatic reasoning (native
primitives not being registered identifiers, composition not obviously earning its complexity)
with an actual principle — a type is a frozen, self-contained definition, not a mutable class
with subclassable state, so "extending" one is a category error, not just an unimplemented
feature. Type definitions in a `.types.anvl` file are standalone/flat — each declares
`type := <native-primitive>;` plus whatever constraint fields it needs (`size`/`values`/`min`/
`max`/etc.), no inheritance between types. This also retires the earlier "native vs. custom base
target" asymmetry (see round-2 findings above) by removing type-level `base` entirely rather than
reconciling two mechanisms.

## Decided — native primitive set

`Numeric`, `String`, `Bool`, `Object`, `Tuple`, `Array` — confirmed. Still open: whether `Blob`
gets its own distinct type identity or folds into `String` (structurally it's already a string
with a tag attribute).

## Decided — `types.c` is independent of `schema.c`; AnvilScript depends on `types.c` directly

Schema is data-centric — describing another document's shape — and stays its own concern.
AnvilScript will use the types registry/engine directly, without needing `schema.c` at all,
confirming `types.c` as the lower, more fundamental module (`schema.c` depends on it, never the
reverse). Separately flagged, explicitly not to be conflated with schema validation right now:
AnvilScript may eventually make validation *scripts* (executable logic, not declarative field
checks) accessible/runnable — "that's another thing entirely," a distinct future idea, not part
of this design thread.

## Confirmed — enum as an understood type kind, not a verbose `values` convention

The repo owner wants enum support that doesn't force every definition to spell out label/value
pairs. Proposal: `type := enum;` is a recognized type kind with *engine-understood* validation
behavior (membership check), and `values` stays exactly as terse as FlyWire's real usage already
is — a plain array — but of **bare, unquoted identifiers** rather than quoted strings:

```
AssetStatus {
   type := enum;
   values := [ active, maintenance, out_of_service ];
};
```

This needs zero new grammar — bare-literal scalars and arrays are both already fully implemented
(`parse_bare_literal`, `source_is_identifier_start`/`is_bare_literal_part`) — and delivers the
dual representation asked for two rounds ago (string label + numeric ordinal) *implicitly*: the
engine assigns each entry's ordinal as its array index (0-based), so both representations exist
without writing either `{ label := ...; value := ...; }` objects or `(label, value)` tuples for
the common, sequential case. Not yet designed: an escape hatch for non-sequential ordinals (e.g.
matching a Postgres smallint enum with intentional gaps) — likely an optional secondary
`ordinals := [...]` array, positionally aligned with `values`, paid for only when the default
(sequential-by-position) doesn't fit. Awaiting reaction before treating any of this as decided.

## Decided — schema validation collects all errors, deliberately unlike the parser

Unlike the core parser's own fail-fast-per-phase behavior, schema validation should report every
validation error it finds in one pass, not stop at the first. This matches `anvil.bak`'s original
`Schema.validate` design intent. Correction to an earlier citation in this note: `schema-registry.
js`'s `_validateValue` actually **throws immediately** on the first bad field despite being called
inside a per-field loop (`resolveAssignment`'s `.map()`) — so FlyWire's real, current JS validator
is fail-fast in practice today, not collect-all, even though it iterates every field. Schema.c's
collect-all behavior is a deliberate improvement over what FlyWire has now, not a continuation of
it.

## Decided — scope: ANVL's responsibility to the community, not to FlyWire specifically

The types registry/engine can be designed with full latitude — it's new, no outside consumer
depends on its shape yet. The schema registry/engine (validation, reporting) is different: it's
been grounded throughout in FlyWire's real `.meta.anvl`/`schema-registry.js` practice, and that
grounding must stay grounding, not become the spec. Repo owner's own framing: "we need to
bifurcate what is truly ANVL's responsibility to the community (all paradigms, not what works for
FlyWire today). FlyWire will adapt to what we implement and write their own adaptation for their
respective paradigm... we just give them the primitives (tools), and they figure out how to build
the house." Concretely: if FlyWire wants a composite schema+types bundle loaded together in its
client, that composition choice belongs in FlyWire's own adaptation layer, not in `schema.c`
itself — same "primitives, not policy" reasoning already applied to bindings, now applied to this
engine's own design scope. See the `anvil-design-for-community-not-single-consumer` memory.

## Decided — `schema.c`/`types.c` are real, separate C modules, "bolted on" only in the build sense

Answering the repo owner's own question directly: not a loose, ad hoc glue layer — genuine
compiled modules (`src/anvil_types.c`, and later `src/schema/schema.c`), each its own header, its
own error namespace reusing the reserved 46xx block, with real logic, `types.c` adapted from
`schema-registry.js`'s proven shape (load once, build a registry, look up against it) and
`schema.c` eventually doing the same for validation (load once, build a ruleset, validate/read
against it). "Bolted on" only describes how each is *built*: optional per consumer, no core
parser/resolver changes, same precedent this note already set for keeping them off the public
flat API's back — not a description of how seriously either module is engineered. Corrected
below: `types.c` isn't actually "bolted on" the same way `schema.c` is — it's opt-in but part of
ANVL proper; `schema.c` is the real add-on, built on top of it.

## Decided — `anvil_types.c` lives directly under `src/`, not under `src/schema/`

Repo owner's correction: types is opt-in, but it's part of ANVL proper — you can opt into types
without ever touching schema. Schema is the actual add-on layered on top of ANVL proper (types
included), not the other way around. So `anvil_types.c` (renamed from the original `types.c`, to
avoid confusion with `include/types.h`/`include/sigma/types.h` — internal type declarations used
throughout Anvil's own source, unrelated to this module) sits as a direct sibling of `core/` and
`sigma/` under `src/` — not nested inside `src/schema/`. `src/schema/` is reserved for `schema.c`
itself once that work starts, since schema (not types) is the genuine add-on.

## Found — loading types via `import` mostly already works, but one real public-API gap exists

Checked whether "the schema engine gets types loaded via `import "file.types.anvl";`" already
holds today. Good news: Anvil's existing import pipeline (`mod_load_imports`, `src/core/
module.c`) already fully parses, resolves, and registers every imported document — a
`.types.anvl` file pulled in via `import` is completely loaded internally with zero new work.
The gap: the **public flat API only exposes the root document.** `anvil_document_get_statements`
(`anvil_flat.c:287`) walks `doc->root->body` specifically; there is currently no public accessor
that reaches into `module_context.ctx->docs` (the full import graph, confirmed to hold every
imported `module_document` with its own body/header/attributes — `include/internal/module.h`) to
read an *imported* document's own top-level statements or its own `@[types]`/`@[schema]` module
attribute. So a schema engine built purely on the public flat API can't yet see what got
imported — the loading already happens, but there's no window onto it from outside core.

This is a real, small, natural Anvil Native addition. First draft of this note proposed a
count+index pair (`anvil_document_get_import_count`/`get_import(doc, index)`) — wrong, the same
anti-pattern already corrected once this session (see the `anvil-prefer-sigma-hps-iterator`
memory). Corrected shape: `ctx->docs` is a Sigma `list`, and `list` already has `as_queryable`
(`include/sigma/list.h:131`) — the exact mechanism `anvil_document_get_statements` already uses
via `FArray.as_queryable`. So this should be an iterator, mirroring `anvil_statement_iterator`
exactly: `anvil_document_get_imports(doc) -> anvil_document_iterator`,
`anvil_document_iterator_next(it, anvil_document *out)`, `anvil_document_iterator_dispose(it)`.
Not a design problem or a core-parser change either way. Needed before `schema.c` can do
anything with `import "file.types.anvl";` at all.

**Implemented.** `anvil_document_get_imports`/`anvil_document_iterator_next`/`_dispose` landed
(flat + vtable, `ANV32`/`ANV33`/`VT09`, Valgrind-clean, full regression green) — see
`notes/public-api.md`'s own entry for the full writeup, including a real double-free found and
fixed along the way (a shared-context handle's ownership needed one consistent model, not two).
This was a genuine prerequisite tackled before the rest of `types.c`, not `types.c` itself.

## Implemented — `types.c` is feature-complete for this phase: GREEN

`anvil_type_registry_load(doc)` / `anvil_type_registry_load_from_imports(doc)` /
`anvil_type_registry_dispose` / `anvil_type_registry_find(reg, name)` /
`anvil_type_def_get_kind(def)` / `get_size`/`get_min`/`get_max` / `get_value_count`/`get_value` /
`anvil_type_resolve` — `src/anvil_types.c`, `include/anvil_type_registry.h`, built entirely on
the public flat API as designed, zero core changes beyond the OBJECT_BLOCK fix below (which was a
real, independently justified gap, not scope creep).

- **Loading**: `anvil_type_registry_load` rejects any document lacking `@[types]` (returns NULL,
  not an empty registry) and walks every top-level statement via `anvil_document_get_statements`,
  reading each one's nested `type :=` field (no name-based nested-field lookup exists publicly,
  so this is an index+name-compare walk over `anvil_value_get_statement`) — mapped to the six
  capitalized native primitives or `enum`. Registered by name in a Sigma `Map` for O(1) repeated
  lookup (the "objectively optimal tool for Anvil's own internal code" — a by-name lookup, not a
  scan, so `Map`, not `Query`).
- **Constraints**: `size`/`min`/`max` are read in the same single pass as `type :=` (one walk
  over a definition's nested fields, not a second one) via a shared `strtoll`-based numeric
  reader — each accessor returns whether the field was actually declared, not just a value, so
  "no `min`" and "`min` is 0" stay distinguishable. `values` (enum only) reads FlyWire's own
  bare-identifier array convention into a list of individually-allocated label copies, freed
  correctly on registry disposal (Valgrind-verified — this was the one place genuinely new
  per-registry heap ownership got introduced).
- **Cross-file resolution**: `anvil_type_registry_load_from_imports(doc)` walks `doc`'s own
  *direct* imports (`anvil_document_get_imports`) and merges every `@[types]`-carrying one's
  definitions into a single combined registry, keyed by bare name — realizing "`types.` is a flat
  namespace, independent of which file a name came from" as actual working code. A name collision
  between two imports currently resolves last-write-wins (`Map.set`'s own default) — no collision
  reporting designed yet. `doc` itself never needs `@[types]` — matches the decided semantics
  exactly. The loading/collecting logic is shared between both entry points (`collect_type_defs`,
  called once per source document, whether that's `doc` itself or one of its imports) rather than
  duplicated.
- **Unified resolution**: `anvil_type_resolve(reg, name)` — one function resolving *any* type
  reference (a bare native primitive, bare `enum`, or a registered `types.X` custom type) to the
  same `anvil_type_def` handle, queryable through the same accessors regardless of source. `reg`
  may be `NULL` (native/enum resolution needs no registry at all). Natives are checked first
  always, even with a registry present, so a custom type can never accidentally shadow a reserved
  name. Backed by seven static, always-available `anvil_type_def` instances (one per native
  primitive + `enum`, never allocated/disposed) that `anvil_type_registry_find` itself was
  deliberately left untouched by — same contract, same tests, this sits additively on top. This
  is what `schema.c` will always resolve a field's `type :=` through. `TYP08`–`TYP10`.

`TYP01`–`TYP10` (`test/unit/test_types.c`), Valgrind-clean, full 14-suite regression green.

**Deliberately still out of scope**: a malformed type-definition statement (no `type :=`, or one
naming nothing recognized) is silently skipped, not reported as a validation error — no
error-reporting design exists yet for `types.c` specifically (schema.c's own "collect all errors"
decision was about *schema* validation, not type *registration*). Inline `@[type=types.VIN]`
annotation on an arbitrary value, and type inference through `base` inheritance, are both still
unimplemented — those are schema/consumer-side concerns, not things `types.c`'s own registry
needs to do.

**Resolved.** `anvil_statement_get_value` originally returned NULL for an anonymous `OBJECT_BLOCK`
statement (`name { ... };`, no `:=`) — its own doc comment said so ("not yet a supported
traversal"). The first `types.c` fixture draft used exactly that form (matching FlyWire's real
`.meta.anvl` shape) and silently registered nothing as a result — caught immediately by TYP01
failing, not a silent wrong answer. Rather than work around it, the public API gap itself got
closed: `parse_statement` (`src/core/parser.c`) now synthesizes a real `ANVL_VALUE_OBJECT` value
for an OBJECT_BLOCK statement too, aliasing the same `.body` list — both statement forms are now
indistinguishable through `anvil_statement_get_value`. A genuine double-free surfaced and got
fixed along the way (two independent disposal passes both owned the same aliased list) — full
writeup in `notes/public-api.md`. This means schema.c will *not* hit this gap when it starts:
FlyWire's real `.meta.anvl` files use the bare `OBJECT_BLOCK` form throughout
(`asset_id { type := int32; };`), and that form now works exactly like the `:=`-object form.
`types_basic.anvl`'s fixture could be switched back to the bare form at any point — kept as
`:=`-object for now simply because nothing requires touching it again yet.

**Build-structure correction**: `test/unit/Makefile` already had `SCHEMA_SRCS`/`SCHEMA_LIB`/
`RESOLVER_SRCS`/`VARS_SRCS`/`DEFAULT_LIB` variables — but dead ones, dating to a pre-rebuild
commit (`e86a002`, v0.5.0-alpha) referencing `src/resolver/`/`src/vars/`/`src/schema/schema.c`,
none of which exist in the current tree, and never wired into any actual build target. This
contradicts the earlier "Build-structure finding" above, which said there was no core/optional
precedent at all — there was a *vestige* of one, just stale and unusable as written. Replaced
with fresh `TYPES_SRCS`/`TYPES_LIB` pointing at the real `src/anvil_types.c` (renamed from
`types.c`, moved out from under `src/schema/` — see "Decided — `anvil_types.c` lives directly
under `src/`" below), and a real `$(BIN)/test_types` target — the first actual instance of an
opt-in-module build, serving as the concrete precedent for `schema.c` and eventually AnvilScript.

## Decided — schema's `int32`/`str`/`date`/`bool` vocabulary lives in adapter-specific `.types.anvl` files, not in `types.c`

Real compatibility gap found while starting schema.c's sketch: FlyWire's actual, currently-
generated `assets.meta.anvl` uses lowercase `int32`/`str`/`date` — none of which exist in
`types.c`'s own six-primitive vocabulary (`Numeric`/`String`/`Bool`/`Object`/`Tuple`/`Array`) or
its `enum` kind. First instinct was to hardcode these four as hidden, zero-import aliases inside
`types.c` itself, purely for FlyWire compatibility — rejected. The repo owner's better call: each
SQL adapter (Postgres, eventually MySQL/SQLite/etc.) gets its *own* real `.types.anvl` file
(`postgres.types.anvl`, ...) defining `Int32`/`Str`/`Date`/`Bool` as ordinary custom types over
the native primitives (`Int32 := { type := Numeric; };`, etc.) — needing **zero new code**, since
`anvil_type_registry_load_from_imports` already resolves exactly this. Generalizes properly
(MySQL's own integer-width quirks, SQLite's dynamic typing, Mongo's non-relational shape each get
their own file instead of ANVIL guessing one lowest-common-denominator vocabulary) and keeps
vendor-specific vocabulary out of core entirely.

Consequence, faced directly rather than avoided: this means FlyWire's existing checked-in
`.meta.anvl` files need regenerating (`type := int32;` → `import "postgres.types.anvl";` +
`type := types.Int32;`) to work against the new `schema.c` — "zero regeneration" (the deciding
reason for rejecting the hardcoded-alias option one round earlier) doesn't actually hold.
Resolved as an acceptable, deliberate tradeoff: regeneration was never fully avoidable anyway — a
new column or type in the live DB already forces a schema-gen re-run today, which is the entire
reason `compareSchema`/drift-checking exists. `postgres.types.anvl` becomes something FlyWireSDK
itself (repo owner's own framing — FlyWire is being considered as a real SDK product, "not just a
protocol") would own and ship as part of its own distribution, not something Anvil Native builds
or bundles — same "primitives, not policy" reasoning already applied everywhere else in this
design.

## Decided — inline field constraints stay a strict superset; naming is authoring guidance, not an enforced rule

Raised: a schema field's inline `values := [...]` (FlyWire's real, working style — quoted
strings, no backing type) sets a bad precedent if left unexamined — it's an undocumented,
unnamed enum, and a future maintainer has no way to know why. Resolved: `schema.c` keeps
accepting every inline constraint (`size`/`min`/`max`/`values`) exactly as FlyWire's real files
already use them — nothing mechanically changes, still a strict superset, no validation-time
rejection. The actual test isn't "enum vs. scalar" — it's whether naming the thing serves real
reuse/abstraction, not merely "the library could represent this as a type": a one-off `min`/`max`
used by exactly one field doesn't obviously earn a name, but an enum with real semantic members
almost always does (an undocumented value list is a worse failure mode than an undocumented
number range). That's a judgment call at authoring time schema.c has no way to detect
mechanically, so it belongs in the eventual schema reference doc as guidance, not as a load/
validate-time error.

**Where a named type belongs, once you do define one**: an adapter-level `.types.anvl`
(`postgres.types.anvl`) should stay scoped to genuine DBMS-bridging concerns — `Int32`/`Str`/
`Date`/`Bool`, the types that answer "how do I represent this column as an Anvil native type at
all." A business-domain enum like `AssetStatus` isn't that kind of bridge (it's specific to one
table's own semantics) and doesn't belong crammed into the adapter file just for convenience —
it gets its own properly-scoped definition instead.

## Open — AnvilSchema's own specialized types

Repo owner's own framing: "if AnvilSchema wants to introduce some specialized types, that's a
different story altogether" — explicitly flagged as separate from the DBMS-bridging-type
question above, not yet defined. Not guessed at here; revisit when there's a concrete need in
front of it rather than speculating now.

## Implemented — `schema.c` first slice: GREEN

`anvil_schema_load(doc)` / `anvil_schema_dispose` / `anvil_schema_validate(schema, data_doc)` /
`anvil_schema_get_violation_count`/`get_violation` / `anvil_schema_violation_get_category`/
`get_field`/`get_message` — `src/schema/schema.c`, `include/anvil_schema.h`, built entirely on
the public flat API and `anvil_type_registry.h` — no core parser/resolver changes. `SCH01`–`SCH07`
(`test/unit/test_schema.c`), Valgrind-clean, full 15-suite regression green.

- **Loading**: `anvil_schema_load` rejects any document lacking `@[schema]` (returns NULL, not
  an empty schema) and walks every top-level statement via `anvil_document_get_statements`,
  treating each as a field rule — same "no exceptions" pattern as `anvil_types.c`'s own
  `@[types]` handling. A field's `type :=` (if present) is resolved via `anvil_type_resolve`
  against a type registry built from the schema document's own imports
  (`anvil_type_registry_load_from_imports`) — `@[schema]` never implies `types.` access on its
  own, matching the decided semantics exactly; only the resolved *kind* is copied out, so the
  registry itself doesn't need to outlive the load call. A field with no recognized `type :=`
  (including none at all — a FlyWire-style `pooled` field) is still registered, just with
  nothing to type-check; its `required :=` is still read and enforced either way.
- **Validation collects everything, never fail-fast** (the decided policy): one pass checks
  every schema field's presence (`required`) and, where a value exists, its kind against the
  field's resolved type; a second pass over the data document's own statements catches anything
  not declared in the schema at all. All three violation categories reuse the reserved 46xx
  block's *numeric values* as a namespace nod (`ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED` = 4604,
  `_TYPE_MISMATCH` = 4605, `_UNKNOWN_FIELD` = 4606) via schema's own, independent public enum —
  not the internal `errors.h` symbols directly, since schema.c never includes internal headers.
- **Kind matching is quoting-agnostic**: a resolved `String` or `enum` kind accepts either a
  quoted STRING value or a bare IDENTIFIER value — FlyWire's own inline `values` convention uses
  quoted strings, `anvil_types.c`'s own enum convention uses bare identifiers, and neither should
  read as a mismatch just because of spelling.
- **Violations belong to the schema, not a separate result object**: each `anvil_schema_validate`
  call replaces whatever the previous call collected (matching the sketch from several rounds of
  design conversation earlier) — disposed and rebuilt fresh each time, not accumulated across
  calls.

**Deliberately still out of scope, matching `anvil_types.c`'s own precedent**: a malformed field
rule (a `type :=` naming nothing recognized) is silently left with no kind to check against,
not reported as an error of the schema itself — no error-reporting design exists yet for a
malformed *schema* (as opposed to a data document failing to satisfy a well-formed one).

## Implemented — `schema.c` constraint checking (`size`/`min`/`max`/`values`), with type inheritance

Extends the first slice: `SCH08`–`SCH12` (`test/unit/test_schema.c`), Valgrind-clean, full
15-suite regression green. A field's `size` (String — a maximum-length bound, not exact-width;
FlyWire's own `varchar(n)` mapping is the dominant real case), `min`/`max` (Numeric — inclusive
range), and `values` (membership, quoting-agnostic like kind-matching itself) are read in the
same single pass over a field's nested statements as `type`/`required`, mirroring
`anvil_types.c`'s own constraint-reading shape closely (a local `strtoll`-based numeric reader
and a values-array reader, deliberately duplicated rather than shared, since schema.c and
types.c are independent modules by design).

**Constraint inheritance, implemented as designed**: if a field's `type :=` resolves to a
custom `types.X` type that has its own `size`/`min`/`max`/`values`, those apply automatically
when the field doesn't declare its own — and the field's own inline declaration always wins,
never merged, exactly per the earlier "inline field constraints" decision. Verified directly:
a field referencing `types.VIN` (which declares `size := 17;`) inherits that bound with no
inline `size` of its own; a sibling field referencing the same type but *also* declaring its
own `size := 25;` uses 25, not 17.

**Two real bugs found and fixed while building the inheritance test, neither in the constraint
logic itself**:

1. The test fixture (`schema_with_custom_type.anvl`) originally wrote `@[schema, ...]` *before*
   `import "...";` — a genuine header-ordering violation (`shebang → imports → attributes →
   body`, per `document-header-scan.md`'s own scanning rules), not a schema.c bug at all. Caught
   immediately as a real parse error (`ANVIL_ERR_HEADER`, "Unexpected token"), not a silent
   misparse.
2. **A real, load-bearing gap in `schema.c` itself**: `read_field_rule` was passing a `type :=`
   field's raw text (e.g. the literal string `"types.VIN"`) straight into `anvil_type_resolve`
   without ever stripping the `types.` prefix first. `anvil_type_resolve`/`anvil_type_registry_find`
   look up entries by their *bare* declared name (`"VIN"`, not `"types.VIN"`) — the `types.`
   spelling is purely a source-level convention for how a person *writes* a reference, never
   something the registry's own lookup understood. This was silently broken from the first
   slice onward: `schema_basic.anvl` never used a `types.X` reference (only bare native names),
   so nothing exercised this path until the inheritance tests did. Fixed with a small
   `strip_types_prefix` helper in `schema.c`, applied before every `anvil_type_resolve` call.

## Open questions (not yet worked through)

- The exact `schema.c`/`types.c` module boundary as reusable precedent for AnvilScript: one
  add-on header or two, shared build-system convention across `test/unit/Makefile`/`binding.gyp`/
  `build.sh` for "core-only" vs. "core+schema" builds, and whether `types.c` can be genuinely
  independent of `schema.c` (types usable without the schema engine) or is always a dependency
  of it.

## Related notes

- `notes/public-api.md` — the module/statement attribute accessors this design reuses directly.
- `notes/resolution-phase.md` — `base`/inheritance semantics, relevant to the still-open
  conformance-marker question above.
- `include/errors.h` — the already-reserved, unused Schema Errors (46xx) block.
- `deferred-work.md` — index entry pointing here.
