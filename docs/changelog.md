# Changelog

All notable changes to the Anvil project are documented in this file.

**Milestone note:** ANVL proper (core + types + schema) is feature-complete and ships as a real
distributable library (static and shared, debug and release — see `notes/distributable-library.md`)
as of `v0.8.0-rc` below. AnvilScript is next — mostly bridging work between the ANVL parser and a
separate script-engine AST parser, so AnvilScript's own grammar can evolve independently of
ANVL's.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- **All five bindings re-pinned to native `main` (`e089837`) and given a decoded attribute accessor**
  (`SR-2610-anvl-vendor-bump-002`): `decodedValue` in `anvil.node`/`anvil.wasm`, `decoded_value` in `anvil.py`,
  `DecodedValue` in `anvil.net`, `getDecodedValue()` in `anvil.java`, next to the unchanged raw `value`
  (`status="done"` is `done`; a flag is null/None). The `Attribute` vtable's appended member is mirrored where it is read.
  The identical-content workarounds are gone (the `anvil.net` xUnit switch and the special exception text in `anvil.net` and
  `anvil.py`), the NUL tests are real tests, and the newer native's real `;` message, unterminated-comment error and
  `nullable`/`truely` bare words are pinned in every binding. `anvil.java` gained a vtable drift test, which found its
  `Document` group one member short.
- **`anvil_attribute_get_value_handle`** (`FR-2610-anvl-public-api-004`; flat API and `Attribute.get_value_handle`,
  both appended last): an attribute's value read AS A VALUE, so `anvil_value_get_type` (STRING / BARE /
  NUMERIC / BOOL / NULL), `get_text` (escapes resolved, delimiters stripped), `get_numeric` and `get_bytes`
  apply unchanged. `@[status="done"]` reads as the string `done`, `@[note="a\"b"]` as `a"b`, `@[n=42]` as the
  number 42; a flag attribute (`@[active]`) returns NULL. Works for module and statement attributes, the
  handle lives as long as its document, and a NUL inside a quoted value survives
  (`BR-2610-anvl-nul-001`). `anvil_attribute_get_value` is unchanged: it is still the raw-source-text
  reader and still returns `"done"` with its quotes. A value the scalar grammar doesn't accept as a whole
  (`@[range=1..5]`) is exposed as a BARE value of its raw text; reading never fails and no document that
  loaded before fails now. Bindings can drop any per-binding unquoting and add one decoded accessor.
- **`anvil.node` and `anvil.wasm` expose statement attributes and attribute values** (`FR-2610-anvl-bindings-003`):
  `node.attributes`/`findAttribute(key)` (module attributes with their values, on the root), `node.statements()`/`statement(name)`
  (an object's statements in order, repeated names included), and `AnvlStatement { name, value, attributes, findAttribute }`,
  with the same names and meaning as `anvil.py` and `anvil.net`. An attribute's value is native's text (a quoted string keeps
  its quotes) and `null` for a flag. `hasAttribute` is unchanged, and an attribute-free document's envelope is byte-identical;
  its `parse()` is about 4% slower in Node and 5% to 11% in WASM.
- **`anvil.py` gains the type registry and schema** (`FR-2610-anvl-bindings-001`, slice 2): `AnvilTypeRegistry`/`TypeDef` and
  `AnvilSchema` in Python idiom, over the `Schema` and `TypeRegistry` vtables. Validation with keyword-only
  `skip_required`/`allow_unknown`, `validate_field` over text, bytes or a loaded document, load diagnostics, and field
  introspection (nested rules, array items, blob tag, format, nullable/integer, extension keys), with exact numeric bounds.
  Everything that owns a handle is a context manager, and the enums are pinned against the C headers. With this, every slice
  of `FR-2610-anvl-bindings-001` is done: `anvil.node`, `anvil.net`, `anvil.wasm` and `anvil.py` share one surface.
- **`anvil.wasm` gains the type registry, schema, writer and builder** (`FR-2610-anvl-bindings-001` slice 4 and
  `FR-2610-anvl-bindings-002`'s wasm slice): the same surface and result shapes as `anvil.node`, including
  `validateField` with `each` and `schema.fields`, so a JS caller has one API on the server and in the browser. The
  module's in-memory filesystem is exposed (`writeFile`/`removeFile`) so schemas can include `@[types]` documents.
  The `.wasm` grows to about 150 KB because the types, schema and writer code is now reachable. Built on the pin
  `f9ee783`. Found while porting: `anvil.wasm`'s `parse()` cuts a document at an embedded NUL (`BR-2610-anvl-bindings-002`).
- **`anvil.node` gains `schema.validateField` and schema introspection** (`FR-2610-anvl-bindings-001`,
  slice 1b): `validateField(docText, name, opts)` validates a statement of a document you already
  hold as ANVL text in one call, with an `each` form for batch bodies (violations at `[i].field`),
  and `schema.fields`/`findField(name)` expose every rule's constraints, nested rules, array items,
  blob tag and extension keys. An absent statement is one `Required` violation with an empty field,
  the same as `anvil.net`.
- **`anvil.net` gains the type registry and schema** (`FR-2610-anvl-bindings-001`, slice 3):
  `AnvilTypeRegistry`/`AnvilTypeDef` and `AnvilSchema` in .NET idiom, over the `Schema` and
  `TypeRegistry` vtables. Validation with `SkipRequired`/`AllowUnknown`, `ValidateField(text, name)`
  for a request body held as ANVL text, load diagnostics, and field introspection (nested rules,
  array items, blob tag, format, nullable/integer, extension keys), with exact numeric bounds. Every
  handle is `IDisposable`, and the enums are pinned against the C headers. Built on the pin
  `f9ee783`. The local package is versioned `0.10.0-dev.1`, provisional until the release decision.
- **`Schema` and `TypeRegistry` vtables** (`FR-2610-anvl-public-api-003`, roadmap decision D4):
  `include/anvil_schema_vtable.h` (`anvil_schema_i`, `extern const anvil_schema_i Schema;`, 40
  members) and `include/anvil_type_registry_vtable.h` (`anvil_type_registry_i`,
  `extern const anvil_type_registry_i TypeRegistry;`, 20 members), implemented in
  `src/schema/schema_vtable.c` and `src/anvil_types_vtable.c`. Same convention as `Writer`/`Builder`:
  every member is pointer-identical to its flat function, in header declaration order, named without
  the module prefix; new flat functions are appended, never inserted. Exported from `libanvil.so`
  (`libanvil.exports`). Tests pin identity and completeness (a flat function without a member fails
  the suite) and run a load/validate/read-violations round trip through the vtable alone. This is
  what `anvil.py` and `anvil.net` bind.
- **Streaming writer** (`FR-2609-anvl-writer-001`): `include/anvil_writer.h` (flat),
  `anvil_writer_vtable.h` (`Writer`) and `anvil_writer_types.h`, implemented in `src/writer/`.
  Emits canonical AML/AMP, validated against the real grammar, and is a separate add-on from the
  reader (separate headers, no shared code). `make WITH_WRITER=0` builds a reader-only library and
  `make check-reader-only` proves it carries no writer symbols. Tests: `test_writer` (standalone,
  no reader linked) and `test_writer_roundtrip` (through the real parser, including a fixed-point
  copy of every loadable fixture).
- **Document builder** (same FR): `anvil_builder.h` (flat), `anvil_builder_vtable.h` (`Builder`),
  `anvil_builder_types.h`. Create values detached, attach them to arrays/objects/the document in any
  order, then `anvil_builder_emit` (or `anvil_builder_write` into a writer). Emits through the
  streaming writer; same grammar rules (`src/writer/grammar.c`); depth-limited and non-recursive on
  disposal. Tests: `test_builder`, `test_builder_roundtrip` (random trees must match the writer
  byte for byte).
- `anvil.java`, `anvil.py` and `anvil.net` wrap both (`AnvilWriter`, `AnvilBuilder`) and read numbers/blobs exactly; `anvil.node`/`anvil.wasm` to follow (behind the `sigma.system.alloc` vendor bump).
- **`anvil_value_get_numeric`** (flat + `Value.get_numeric`, appended last in the vtable): a NUMERIC value as an exact `int64_t` (integral text, including values past a double's 53-bit mantissa) or a `double`, with `overflowed` as the fall-back-to-`get_text` signal. Amends decision #6 of `FR-2609-anvl-public-api-001`; `anvil_numeric_t` is in `anvil_types.h`.
- **`anvil_value_get_bytes`** (flat + `Value.get_bytes`, appended last): the exact, uninterpreted source span of any scalar kind (a `STRING`'s escapes unresolved, a `BLOB` byte for byte, no NUL terminator); 0 for collections. The primitive that `get_text` is a view of, and the fallback for a number `get_numeric` can't hold. `get_text` is unchanged (still total) and re-documented as the string view. See decision #6 of `FR-2609-anvl-public-api-001`.
- **`anvil_value_get_blob_tag`** (flat + `Value.get_blob_tag`, the 9th and last `Value` field): a blob's `@tag`, which no accessor exposed before (the content readers return only what is between the backticks). Text buffer convention; 0 for an untagged blob or any other kind. The writer/builder round-trip suites now carry tags through a reader→writer copy.

- **Type-registry diagnostics** (`FR-2610-anvl-types-001`): `anvil_type_registry_get_diagnostic_count` /
  `_get_diagnostic` and `anvil_type_diagnostic_get_category/_name/_source/_message` (new header
  section in `anvil_type_registry.h`). The registry now reports what it used to resolve silently:
  `NAME_COLLISION` (last-write-wins is unchanged; the message names both documents),
  `UNKNOWN_BASE_TYPE`, `MALFORMED_CONSTRAINT` and `MALFORMED_ENUM_VALUES`. Diagnostics are copied at
  load time and outlive the source documents. Also adds `anvil_document_get_path` (flat API) so a
  diagnostic can name the document it came from. No existing signature changed.

- **Transitive `@[types]` resolution** (`FR-2610-anvl-types-002`): `anvil_type_registry_load_from_includes`
  now follows the whole include graph (depth-first, each document once) instead of only direct
  includes, so a schema that includes `ticket.types.anvl` sees the types `ticket.types.anvl` itself
  includes. A non-`@[types]` document in the chain is walked through. Deeper definitions merge
  first, so one closer to the consumer overrides it, reported as a `NAME_COLLISION`. The old
  direct-only behavior is not kept as a mode. `docs/types-reference.md` (and its site mirror,
  which also picks up the diagnostics section from `FR-2610-anvl-types-001`) describes it.

- **Schema load diagnostics** (`FR-2610-anvl-schema-004`): `anvil_schema_get_diagnostic_count` /
  `_get_diagnostic` and `anvil_schema_diagnostic_get_category/_type_category/_field/_message`.
  `anvil_schema_load` still returns a handle for a malformed schema, but now reports what it used to
  weaken silently: `UNKNOWN_TYPE` (a typo such as `types.Prority`), `MALFORMED_CONSTRAINT`
  (`size`/`min`/`max`/`values`/`required`), `RULE_NOT_OBJECT` and `CONFLICTING_CONSTRAINTS`
  (`min` > `max`), plus the type registry's own diagnostics passed through as `TYPE_REGISTRY`
  (collisions, bad definitions). `UNKNOWN_FORMAT` and `SELF_REFERENCE` are reserved for
  `FR-2610-anvl-types-004` and `-schema-001`. Unknown rule keys (FlyWire's `pooled`, `mask`) are
  deliberately not diagnostics. Gives CI a "schema is well-formed" gate: the count is 0.

- **`anvil_schema_validate_value` + `anvil_schema_options`** (`FR-2610-anvl-schema-002`): validates an
  OBJECT *value* (a request body such as `set := { ... };`, or one element of
  `set := [ {...}, {...} ]`) against a schema exactly as if it were a document's top level. Options:
  `skip_required` (patch semantics; every other check still runs on present fields) and
  `allow_unknown`. The struct carries its own `size`, so it can grow without breaking compiled
  callers. A non-object value yields one `TYPE_MISMATCH` with an empty field path. Each call replaces
  the previous call's violations. `anvil_schema_validate` keeps its signature and results: it now
  shares one validation pass with the value form. This is what lets FlyWire validate through
  AnvilSchema instead of its own JavaScript validator.

- **Nested schema validation** (`FR-2610-anvl-schema-001`): a schema rule is an ordinary nested object,
  so `type := Object` may carry `fields := { name := { <rule> }; ... }` (recursive, unknown-field
  checking included) and `type := Array` may carry `items := { <rule> }` for every element plus
  `min_items` / `max_items`. A `@[types]` Object type may carry `fields` and is reusable by every field
  referencing it (an inline `fields` replaces it, never merges); a self-referential type is reported
  as `SELF_REFERENCE` and not expanded. Violations now name the full path (`links[2].target`,
  `assignee.id`); top-level fields read exactly as before. New category
  `ANVIL_SCHEMA_ERR_VALIDATION_ITEM_COUNT` (appended). Violation order is schema order, depth first.
  Nested rules also produce load diagnostics with paths, and honour `anvil_schema_validate_value`'s
  `skip_required` / `allow_unknown` at every depth.

- **`nullable`** (`FR-2610-anvl-schema-003`): `nullable := true` makes an explicit `null` a legal value
  for a declared field, so a patch that clears `assignee_id` validates. Without it, `null` for a typed
  field is still `TYPE_MISMATCH`, now with the clearer message "field is not nullable". A nullable
  `null` skips every other constraint; `required := true; nullable := true;` means present-and-may-be-null.
  Inherited from a `@[types]` definition (a field's own `nullable := false` overrides it) and applied
  at every depth, array items included. A malformed `nullable` is a `MALFORMED_CONSTRAINT` diagnostic.

- **`integer` constraint and fractional bound getters** (`FR-2610-anvl-types-005`): `integer := true`
  requires a numeric value to be whole, judged by value (roadmap decision D2): `2`, `2.0` and `1e+3`
  pass, `2.5` fails with a `TYPE_MISMATCH` saying "value must be an integer" (no new category).
  Inherited from a type; a field's own `integer := false` overrides it; applied at every depth.
  `anvil_type_def_get_min_numeric` / `anvil_type_def_get_max_numeric` expose fractional and exponent
  bounds exactly; the `long long` getters keep their signatures and return false for such a bound
  instead of truncating. Appended API only.

- **`Blob` primitive** (`FR-2610-anvl-types-003`, roadmap decision D1: both): `type := Blob` is a new
  native primitive (`ANVIL_TYPE_BLOB`, appended to `anvil_type_kind`) that accepts only backtick
  blobs and may require a tag with `tag := png;` (new category
  `ANVIL_SCHEMA_ERR_VALIDATION_TAG`, appended). `type := String` now also accepts a blob, so long
  multi-line text validates as a string; an `enum` still does not. `size` on a blob counts payload
  bytes (the tag is not payload). `tag` is inherited from a type and applied at every depth. Quoted
  strings and bare words validate exactly as before. The open question in
  `docs/types-reference.md` is replaced by the decision.

- **`format` constraint for String** (`FR-2610-anvl-types-004`): `format := iso8601_date | iso8601_datetime |
  uuid`, checked by hand-written validators (no regex engine). `iso8601_date` has a real calendar check
  (month lengths, leap years); `iso8601_datetime` is the RFC 3339 profile with a mandatory offset;
  `uuid` is 8-4-4-4-12 hex, any case and version. A mismatch is the new
  `ANVIL_SCHEMA_ERR_VALIDATION_FORMAT` violation naming the field (appended). Inherited from a type,
  applied at every depth, checked on string, bare and blob values. An unknown format is a load
  diagnostic (`UNKNOWN_FORMAT` on a schema field, `MALFORMED_CONSTRAINT` on a type) and the
  constraint is treated as not declared. New `anvil_type_def_get_format`.

- **Schema introspection** (`FR-2610-anvl-schema-005`): a loaded schema can be read back, read-only.
  `anvil_schema_get_field_count` / `get_field` / `find_field` (declaration order) and
  `anvil_schema_field_get_*`: name, kind, the `type :=` as written (`types.Id32`), `required`,
  `nullable`, `integer`, and every constraint (`size`, `min`/`max` in numeric form, `min_items` /
  `max_items`, `values`, `format`, `tag`) with the "declared, not a default" convention. Settings are
  the effective ones - what validation enforces, inherited constraints included. Nested rules are
  reachable through `get_nested` / `find_nested` / `get_items`. Rule keys the schema doesn't consume
  (FlyWire's `pooled`, `mask`) are exposed as extras (`get_extra_count/_name/_value`) on any rule at
  any depth; each value is a schema-owned copy readable with the ordinary `anvil_value` accessors and
  valid after the source document is disposed. This lets FlyWire stop parsing the schema document
  in parallel with AnvilSchema.

### Changed
- The writer now rejects a repeated top-level name (`ANVIL_WRITER_ERR_DUPLICATE_NAME`), as the
  reader's resolver does. Added `ANVIL_WRITER_ERR_DEPTH_EXCEEDED` (builder).

### Added (planning)
- **Rivet-readiness ticket set.** These are the AnvilTypes/AnvilSchema/bindings gaps found while
  building Rivet, the in-house ticketing system: `BR-2610-anvl-types-001`,
  `BR-2610-anvl-schema-001`, `FR-2610-anvl-types-001`–`005`, `FR-2610-anvl-schema-001`–`005`,
  `FR-2610-anvl-bindings-001`/`002`, `SR-2610-anvl-docs-001`, `SR-2610-anvl-distribution-001`,
  and `SR-2610-anvl-vendor-bump-001`. That last one is new: `anvil.node`/`anvil.wasm` still pin
  `vendor/anvil` 45 commits behind `main`. A planning review corrected four tickets, including
  `is_integral`, which follows the number's spelling rather than its value, and three truncation
  sites the schema-buffer BR had missed. Sequenced in `notes/roadmap-2610-rivet-readiness.md`:
  Phases 1–3 are native work for the ANVL coder agent, and Phases 4–5 are bindings and release
  work for the ANVL Bindings agent. Tickets and plan only; nothing implemented yet.
- **`FR-2610-anvl-public-api-003`**: `Schema`/`TypeRegistry` vtables for the `anvil.py`/`anvil.net`
  bindings (roadmap decision D4). Owned by the Bindings agent, and starts after roadmap Phase 3.
- **`BR-2610-anvl-visibility-001`** (filed): `libanvil.so` exports none of the `anvil_schema_*` /
  `anvil_type_registry_*` functions, because their headers lack the `visibility push(default)`
  wrapper. Shared-library consumers (`anvil.py`, `anvil.net`, `anvil.java`) can't reach schema or
  types until it's fixed. Added as roadmap addendum A1 for the coder agent.
- **`BR-2610-anvl-vtable-001`** (filed): `anvil_document_get_path` exists only in the flat API, not
  the `Document` vtable, and no test checks vtable completeness. Roadmap addendum A2. Both A1 and A2
  must merge before the Bindings agent starts Phase 4.
- **Phase 4 scope decisions** (owner-approved, on the Bindings agent's slice-1 findings):
  `anvil.node` gains `schema.validateField(docText, name, opts)` (slice 1b), so a request body's
  `set` object validates without re-serializing it. `anvil.wasm` gets the same types/schema
  surface (FR-2610-anvl-bindings-001 slice 4) and the writer/builder (FR-2610-anvl-bindings-002
  second slice); today it compiles them in but exports nothing from them. The release
  (SR-2610-anvl-distribution-001, now owned by the Release agent) waits for all of Phase 4.
- **`BR-2610-anvl-registry-001`** (filed): loading text identical to a live document fails with an
  empty `Io` error. Underneath, the process-global source registry makes independent loads collide,
  which three bindings have now hit separately. Part 1 fixes the message; Part 2 proposes scoping
  dedup to one load's include graph.
- **`BR-2610-anvl-nul-001`** (filed): the type registry and schema store copied text as C strings,
  so an enum label containing a NUL is truncated and a string extension value containing one comes
  back NULL. Found by the Bindings agent's cross-binding NUL audit. The core reader itself is correct.
- **`FR-2610-anvl-bindings-003`** (filed): `anvil.node` and `anvil.wasm` expose only module
  attribute *keys*, root-only. Statement attributes, and all attribute values, are unreachable from
  JS, unlike `anvil.py`/`anvil.net`. Requested by Rivet for its q-or importer.
- **`FR-2610-anvl-public-api-004`** (filed): an attribute's value can only be read as raw source
  text (`status="done"` reads `"done"`, quotes and escapes included). Adds a decoded/typed read,
  preferably as an `anvil_value`, so no binding reimplements unquoting.
- **`BR-2610-anvl-bindings-003`** (filed): `anvil.node`/`anvil.wasm` collapse a repeated object
  field name to its last occurrence (`count`, `get`, `entries`), while native/py/net keep all of
  them and find the first.
- Release agent instructions: FlyWire's guard test (`tests/test-anvl-binding-grammar.js`) runs
  against the packaged node/wasm tarballs before upload, and FlyWire and Rivet are notified once
  the release is live.
- **`BR-2610-anvl-version-001`** (filed): the reported version is `MAJOR.MINOR.PATCH+BUILD-TAG`
  (not semver, so `0.8.0+0-rc` reads as a release), `ANVL_VERSION_STR` is a second, drifting
  source of truth, and Node/WASM builds hard-code `BUILD=0`. From the Release agent's audit.
- **`BR-2610-anvl-attr-001`** (filed): an attribute's raw value includes any whitespace before the
  next `,` or `]`, and a newline there makes the decoded value fall back to BARE with its quotes
  kept. From Rivet's q-or re-import.
- **Release decisions and agent instructions:** the release is `v0.10.0-rc` (R1). The website,
  including downloads, `version.js` and deploys, belongs to the ANVL Documentor, and the
  `0.8.0-rc` downloads are removed with no archive (R2). `agents/ANVL-RELEASE.agent.md` now builds
  and verifies artifacts and hands them to the Documentor. New `agents/ANVL-DOCUMENTOR.agent.md`
  covers the stale schema README, the site's internal-name scrub, the docs sync check, a deploy of
  already-merged docs, and preparing the downloads, Bindings-Guide and public changelog for the
  release.
- **`SR-2610-anvl-vendor-bump-002`** (filed): re-bump every binding to native `main` after the
  2026-10-26 merges, drop the identical-content workarounds, flip the NUL tests, and add one decoded
  attribute accessor across the bindings. It gates the v0.10.0-rc release.
- **`BR-2610-anvl-parser-001`** (filed): a bare value that begins with `null`/`true`/`false`
  (`nullable`, `truely`, `falsey`) fails to load. The literal is matched as a prefix. From Rivet's
  q-or importer.
- **`BR-2610-anvl-errors-001`** (filed): several parse errors report "Unknown error" (a missing
  `;`, for example), and a repeated top-level name reports no position.
- **`SR-2610-anvl-build-001`** (filed): `test/unit`'s `test_release` can link a stale
  `libanvil.a`, because its archive rule has no prerequisites.
- **`BR-2610-anvl-writer-001`** (filed): the writer's name-set hash (`hash_name`, `grammar.c`)
  truncates its 64-bit FNV-1a constants on 32-bit `size_t`, so wasm32 builds warn and hash
  differently from native. Lookups stay correct. Found building `anvil.wasm`; to land before the
  writer is exposed to JS.
- **`agents/ANVL-RELEASE.agent.md`**: instructions for a new ANVL Release agent. It builds from
  tags only, packages with scripts rather than by hand, and runs release checks against the packaged
  artifact: a grammar check for `include`, plus version, install, export, checksum and post-deploy
  checks. Deploys and tag pushes happen only on the owner's go. First assignment:
  SR-2610-anvl-distribution-001 and BR-2610-anvl-distribution-001, starting with an audit of all
  five published `0.8.0-rc` tarballs.

### Fixed
- **An attribute value's span now ends at the value, not at the next delimiter** (`BR-2610-anvl-attr-001`):
  the attribute scanners (statement and module attributes) ran each value on to the next `,` or `]`, so
  whitespace before the delimiter became part of the raw value: `@[a="1" , b="2"]` gave `a` the raw value
  `"1" `, and when the whitespace held a newline (`@[b="2"` newline `]`, the usual multi-line layout) the
  decoded value from `anvil_attribute_get_value_handle` fell back to BARE with its quotes kept. Spaces,
  tabs, CR and LF before the delimiter are no longer part of the value, so the raw value is exactly the
  token (`"2"`, `bug`, `1..5`) and the decoded value is STRING `2` / BARE `bug` whatever whitespace follows,
  including CRLF files. Whitespace after `=` was never affected. Every binding picks this up at its next
  pin.
- **An unterminated block comment is now an error wherever it opens** (`BR-2610-anvl-parser-002`): only the
  header scanner checked for it, so `x := 1; /* never closed` loaded with no error, a statement after it was
  silently lost, and inside an object, array, tuple or attribute list the error blamed the end of the file.
  It is now reported as "Unterminated block comment" at the line and column where the comment OPENS, in AML
  and AMP, from the body parser too. The header check also wrongly rejected a comment that closed as the very
  last thing in a file. A newline between a value and its `;` stays an error: that rule is deliberate
  (`notes/document-body-parse.md`) and is now stated in `docs/language-reference.md` and pinned for every
  value kind.
- **`make test_release` can no longer link a stale `libanvil.a`** (`SR-2610-anvl-build-001`):
  `test/unit/Makefile` only built the release archive when it was missing, so after a branch switch or
  rebase `test_release` linked an archive built from older sources and could fail (or pass) for no reason
  that had to do with the code. It now asks the top-level make every time, which tracks sources and
  headers and does nothing when the archive is current. New `make check-test-release-fresh` proves both
  halves (rebuilds after a core source changes, recompiles nothing when nothing changed).
- **Parse errors now say what is wrong, and a repeated top-level name has a position**
  (`BR-2610-anvl-errors-001`): eight error codes were declared in `errors.h` with no entry in the message
  table (and twelve with no name), so documented failures read "Unknown error". The worst offender was a
  missing `;`, which now reads "Expected ';' to end the statement" (position unchanged); the others are
  `ANVL_ERR_PARSER_EXPECTED_COLON`, the AMP object error, an invalid `$` reference, and the arena, source,
  context and not-implemented codes. A repeated top-level name was reported at line 0 / column 0; it now
  points at the SECOND occurrence and the message names the identifier and the first one's line
  (`Duplicate top-level identifier 'x' (first declared at line 2)`). A completeness test reads `errors.h`
  and fails when any code lacks a message or a name, so the tables can't fall behind again. "Unknown
  error" is now reachable only for a genuinely unknown code.
- **A bare value that merely begins with `null`, `true` or `false` now loads** (`BR-2610-anvl-parser-001`):
  the scalar parser matched the reserved literal as a PREFIX, so `x := nullable;`, `nulls`, `truely` and
  `falsey` were read as NULL/BOOL followed by stray characters and failed with an unhelpful error (Rivet's
  q-or importer had to quote such values). `null`, `true` and `false` are literals only as whole tokens
  now; everything else is an ordinary BARE word with its full text. Exact `null`/`true`/`false` are
  unchanged, in statements, arrays, tuples, objects and attribute values. The writer already reserved
  only the exact words, so `anvil_writer_bare("nullable")` was always accepted; it now reads back as BARE.
- **Loading documents is now thread-safe across independent documents, and identical text no longer
  collides** (`BR-2610-anvl-registry-001`, part 2). Any number of threads may call `anvil_load`,
  `anvil_load_buffer` and `anvil_parse_value_fragment` at the same time, on the same text or different
  text: each load owns its state and shares nothing with other loads. (Sharing ONE document handle
  across threads is still the caller's job; `anvl_get_error()` is per-thread.) Before this, source
  deduplication was a process-global, unsynchronized map, so: loading text identical to any live
  document anywhere in the process failed (the `anvil.node` `parse()` race, the `anvil.net` xUnit
  flake, `AnvilSchema.ValidateField(text)`); loading *different* text on several threads corrupted that
  map and failed loads too; and a second shared structure in the parser raced as well. Now
  deduplication is per module context (one load's include graph; a diamond include still loads each
  shared document once), each source points directly at its document, and the parser's last-error code
  is per-thread. Under ThreadSanitizer, 8 threads loading, reading and disposing now report 0 races and
  0 failed loads; before: 88-169 race reports and up to 1590 failed loads. Internal only, no public ABI
  change. New `make check-tsan`. Consumers that serialize loads to work around this can stop.
- **Schema extension keys with identical text are no longer lost** (`BR-2610-anvl-registry-001`, found
  while fixing the above; `FR-2610-anvl-schema-005`): extension-key values are copied into value
  fragments, which went through the same registry, so when two extras had identical text (for example
  `pooled := true` on two fields, or the same rule in two schemas) the second one silently read back as
  NULL from `anvil_schema_field_get_extra_value`. All of them are now readable (SCH85).
- **Loading text identical to a live document reports a real error** (`BR-2610-anvl-registry-001`,
  part 1): `ANVL_ERR_SOURCE_ALREADY_LOADED` ("A document with identical content is already loaded")
  replaced a reused parser code and is recorded on the load's error detail. Within one include graph it
  is what the diamond-include handling keys on.
- **Strings containing a NUL byte are no longer truncated or dropped by the type registry and schema**
  (`BR-2610-anvl-nul-001`): the core reader always loaded them intact, but the registry and schema
  copied text into C strings. An enum label `"a\0b"` read back as `"a"` from `anvil_type_def_get_value`
  and `anvil_schema_field_get_value`; a string extension key containing a NUL came back as a
  NULL-typed value from `anvil_schema_field_get_extra_value` (a NUL in an array/object/blob extra was lost
  the same way); a type's nested `fields` were cut off at a NUL; and `values` membership compared with
  `strcmp`, so `"a\0b"` matched a label `"a"`. Labels are now length-carried, the getters return the
  exact byte length and copy every byte (NULs included), membership is by length then bytes, and extras
  and a type's `fields` are re-parsed with their exact length. In the places that NAME something (a
  `type`, `format`, `tag`, or a `required`/`nullable`/`integer` flag) a quoted value containing a NUL is
  reported as malformed ("the value contains a NUL byte") instead of being truncated into a match
  (`"uuid\0x"` is not `uuid`). A NUL in a statement name, rule key or bare word is rejected by the core
  at load time, so those can't occur. No public signature changed.
- **A quoted attribute value containing an escaped quote no longer swallows the rest of the document**
  (`FR-2610-anvl-public-api-004`): the attribute scanner (statement and module attributes) toggled "inside
  a string" on every `"`, escaped or not, so `@[note="a\"b"]` left the string open, ate the closing `]`
  and failed the load (or mis-split later attributes). It is escape-aware now.
- **`anvil.node` and `anvil.wasm` no longer collapse a repeated name inside an object** (`BR-2610-anvl-bindings-003`). For
  `{ a := 1; a := 2; b := 3; }`, `count` is now 3, `get('a')` is the first (`1`) and `entries()` yields all three in order, as native,
  `anvil.py` and `anvil.net` do; they used to report 2, the last, and `a` once. Objects without repeated names are unchanged. A
  request body that repeats a column is no longer silently taken as its last value.
- **A NUL character inside a string value no longer cuts a document short in `anvil.wasm` and `anvil.java`**
  (`BR-2610-anvl-bindings-002`). In `anvil.wasm` every text entry point read its text with `strlen`, so
  `s := "x";\0t := 1;` parsed as `s` alone with no error; they now pass the byte length. In `anvil.java` every
  string value was decoded with a read that stops at the first NUL; it now decodes exactly the bytes native wrote.
  `anvil.node`, `anvil.net` and `anvil.py` were audited and were correct. Native itself is correct for documents.
  The type registry and schema's own loss (enum labels cut at a NUL, a string extension value lost) is fixed
  natively by `BR-2610-anvl-nul-001`.
- **`anvil.node` and `anvil.wasm`: `parse()` and `parseRawValue()` reject a non-string** (`BR-2610-anvl-bindings-001`).
  They used to parse an empty buffer for a number, object or array (and, in `anvil.wasm`, fail with an
  incidental internal error for a `Buffer`, `undefined` or `null`), returning a valid, empty document with no
  error. They now throw a `TypeError` naming the function and the type. An empty string is unchanged.
- **The writer's name-set hash is true 64-bit FNV-1a on every target** (`BR-2610-anvl-writer-001`):
  `hash_name` accumulated in a `size_t`, so on a 32-bit target (wasm32, 32-bit ARM, `-m32`) both FNV
  constants were silently truncated, giving a non-standard 32-bit hash and a `-Wconstant-conversion`
  warning in every `anvil.wasm` build. It now accumulates in `uint64_t` with `UINT64_C` constants (as
  `src/sigma/map.c` does) and folds to `size_t` only to pick a slot. Bit-identical on 64-bit targets;
  duplicate-name detection is unchanged. New `make check-32bit` compiles the writer grammar with `-m32
  -Werror` (skips with a message when the 32-bit headers aren't installed).
- **`anvil_document_get_path` is now in the `Document` vtable** (`BR-2610-anvl-vtable-001`): it was
  added to the flat API only, so bindings that bind the vtable struct (`anvil.py`, `anvil.net`,
  `anvil.java`) could not reach it. `get_path` is appended last in `anvil_document_i`, so existing
  member offsets don't move. A new completeness test (VT11) fails when any function declared in
  `anvil_flat.h` has no vtable member and isn't listed flat-only with a reason.
- **`libanvil.so` now exports the schema and type-registry API** (`BR-2610-anvl-visibility-001`):
  `anvil_schema.h` and `anvil_type_registry.h` lacked the `visibility push(default)` wrapper the other
  public headers carry, so with `-fvisibility=hidden` every `anvil_schema_*` / `anvil_type_*` function
  was missing from the shared object, and `anvil.py`/`anvil.net`/`anvil.java` could parse but not use
  types or schema. Both headers now carry it. New `make check-exports` (`scripts/check-exports.sh`)
  fails when any function or vtable object declared in a public header is not exported, or when an
  internal symbol is. A linker version script (`libanvil.exports`) now defines the exported set exactly,
  which also hides `Allocator`, a global from the precompiled `sigma.system.alloc.o` that
  `-fvisibility=hidden` cannot reach.
- **Numeric constraints and values are no longer truncated** (`BR-2610-anvl-types-001`): schema and
  type `min`/`max` were read with `strtoll`, so `min := 0.5` became `0`, data `3.7` passed
  `max := 3`, `5e+10` read as `5`, and text past 31 characters was cut. Both now read through
  `anvil_value_get_numeric` and compare exactly (no double rounding near 2^53/2^63; an overflowed
  data value compares by sign). A non-numeric, overflowing or fractional-`size` declaration is
  treated as not declared rather than as `0`. `anvil_type_def_get_min`/`_max` now return false for a
  fractional bound instead of a truncated integer.
- **No fixed-buffer truncation in types and schema** (`BR-2610-anvl-schema-001`): type names, field
  names, rule keys, `type :=` references, `required`, enum `values` comparisons and violation
  `field`/`message` text were read through fixed stack buffers (16/64/65/128 bytes) and compared by
  their clipped prefix, so two long type names sharing 63 characters collided, a 200-character enum
  label never matched, and an undeclared field could hide behind a declared one with the same
  64-character prefix. Everything is now read and stored at its full length. Also fixes a leak of an
  enum type's `values` when its `type :=` was unrecognised.
- Documentation drift fixed (`SR-2610-anvl-docs-001`): `docs/types-reference.md` no longer says schema
  is "design-only" (it describes the implemented type vocabulary and schema behavior, with a
  "Closed schemas" section and a byte-length `size` example), and the README status lines for types and
  schema agree with it. The stale "no numeric accessor" comments went away with
  `BR-2610-anvl-types-001`.

---

## [v0.9.0-rc] — (2026-10-09)

**Status:** minor bump — one public-API rename (`ANVIL_VALUE_IDENTIFIER` → `ANVIL_VALUE_BARE`, source-breaking for C callers, ABI-compatible).

### Changed
- **Public API rename: `ANVIL_VALUE_IDENTIFIER` → `ANVIL_VALUE_BARE`** (same numeric value, `5`),
  and the internal `ANVL_VALUE_IDENTIFIER` → `ANVL_VALUE_BARE`. Matches the `ANVIL_VALUE_<KIND>`
  family and the grammar's own term (`parse_bare_literal`, "Bare literal grammar"). A bare literal
  is an ordinary unquoted string, so `anvil.java`/`anvil.net`/`anvil.py`'s `asString()`/`AsString()`/
  `as_string()` now accept it alongside `STRING` (`anvil.node`/`anvil.wasm` already did, via their
  JSON conversion; their vendor bump is deferred — see `notes/deferred-work.md`).

---

## [v0.8.0-rc] — (2026-08-17 – 2026-09-19)

**Status:** ANVL proper (core parser + resolver, opt-in types, AnvilSchema) is feature-complete
and builds as a real distributable library. Minor bump, not a patch: opt-in types, AnvilSchema,
and the whole distribution/build story are new, additive surface area since `v0.7.0-alpha` — an
`-rc` tag rather than a plain minor release since AnvilScript bridging work is still to come
before a `v0.8.0` proper.

Three and a half weeks with no changelog entry at the start of this window, spanning a lot of
real, substantial work. The early part (body-parse through the first public-API pass) is
summarized at milestone grain rather than reconstructed commit-by-commit — treat it as
restructuring and refactoring on top of the v0.7.0-alpha foundation below, not an itemized
account. Everything from native schema/opt-in types onward is current work, documented in full.

### Added — Body parse, resolution, and the public API (mid-to-late August)

- **AML/AMP body grammar complete** — statements (assignment and bare-block form), all value
  kinds (scalars, blobs, arrays, tuples, objects, `$identifier` VarRef), inheritance
  (`base`/field-merging), and statement/module attributes (`@[...]`), with AMP's own restricted
  subset enforced throughout. Arena-backed node allocation; `doc->body` accumulate-then-freeze.
- **Resolution phase complete** (`src/core/resolver.c`) — global identifier map, `$identifier`
  VarRef resolution, `base`/anonymous-object legality checks, full field-merging inheritance
  (transitive chains, cycle detection).
- **Public API (Anvil Native ABI) — first full pass**: `anvil_load`/`anvil_load_buffer`/
  `anvil_dispose`/`anvil_has_errors`/`anvil_get_error`; statement/value accessors; the
  `anvil_vtable.h` convenience layer (pointer-identical to its flat counterparts); module- and
  statement-level attribute accessors; document-level statement enumeration; Value Fragment
  parsing (`anvil_parse_value_fragment`); detailed error diagnostics (message/line/column beyond
  the stable category alone).
- **AnvilScript (ASL) design work begun** — see `notes/anvilscript-design.md`,
  `notes/anvilscript-scriptengine-design.md`. Design only; nothing implemented.
- **Sigma FR-2603-sigma-collections-006** — heapless, predicated-scan `Query`/`sc_queryable`
  (HPS) iterator, replacing hand-rolled scans across the vendored Sigma subset.

### Fixed

- **`parser_set_error` silently clobbering a more specific error code** with a later, more
  generic one from an outer caller — `parser.err_code` had no first-error-wins protection of its
  own, unlike `ctx->errors`. Fixed to mirror `anvl_error_set`'s existing guarantee.

### Changed

- **anvldata.com's site** relocated from `anvil.js` to `anvil/site/` — Anvil.C is now the de
  facto reference implementation. File move only; hosting/deploy/content rewrite still open (see
  `notes/deferred-work.md`).

### Added — Native schema foundation & opt-in types (current)

- **Document include iterator** (`anvil_document_get_includes`/`anvil_document_iterator_*`) —
  closes the gap where the public API could load and resolve an include graph but never expose an
  included document's own statements/attributes from outside core.
- **`anvil_statement_get_value` now supports anonymous `OBJECT_BLOCK` statements** (`ident { ... };`,
  not just `ident := { ... };`) — both forms are now indistinguishable through the accessor,
  matching how they're already treated identically everywhere else.
- **Opt-in type registry — full first phase** (`src/anvil_types.c`, part of ANVL proper, not an
  add-on): `@[types]`-gated loading, the six native primitives + `enum`, constraint fields
  (`size`/`min`/`max`/`values`), cross-file `types.X` resolution via a document's own includes
  (`anvil_type_registry_load_from_includes`), and `anvil_type_resolve` — one function resolving
  any type reference (native, `enum`, or custom) to a uniform, queryable handle.
- **Sigma FR-2603-sigma-collections-007 + 011** — opt-in per-instance allocator override for
  `List`/`Collection`, and an arena-backed `Stack` built on top of it for the eventual AnvilScript
  runtime. Implemented by the Sigma team in the `sigma.anvil` worktree, merged in.
- **`docs/` knowledge base** — `getting-started.md`, `language-reference.md`, `types-reference.md`,
  and `schema/README.md` (AnvilSchema's introduction — why it isn't a bolt-on the way XSD/XSLT or
  JSON Schema are). `README.md` rewritten to match current reality; three stale
  `docs/maintainers/` planning documents removed (predated the rebuild, actively contradicted
  current design decisions).
- **AnvilSchema** (`src/schema/schema.c`, the actual add-on layered on top of `anvil_types.c`):
  `anvil_schema_load` (gated on `@[schema]`, every top-level statement a field rule),
  `anvil_schema_validate` checking required-field presence, type-kind mismatches, undeclared
  data fields, and full constraint checking (`size`/`min`/`max`/`values`) — including
  inheriting a constraint from a resolved `types.X` custom type when a field doesn't declare its
  own, field-level always overriding rather than merging. Collects every violation in one pass,
  never fail-fast.

### Fixed

- **`schema.c` never stripped the `types.` namespace prefix before resolving a field's declared
  type** — `type := types.VIN;`'s literal text was passed straight to `anvil_type_resolve`,
  which looks up entries by bare name (`"VIN"`), so any field referencing a custom type silently
  never resolved. Undetected until the constraint-inheritance tests exercised a `types.X`
  reference for the first time; the first slice's own tests only ever used bare native names.

### Changed

- **`anvil_schema_err_code` gets specific categories for each constraint kind** — `size`/`min`/
  `max`/`values` violations were being reported as `ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH`
  (the closest existing category at the time, not an accurate one — a value can be exactly the
  right *kind* and still violate a constraint). Added `ANVIL_SCHEMA_ERR_VALIDATION_SIZE`,
  `_RANGE`, and `_VALUES`; `TYPE_MISMATCH` now means only "wrong kind of value" again. Also drops
  the enum's original numbering, which had nodded at the internal `errors.h` 46xx block's
  reserved values — that block turned out to have no real call sites using it beyond its own
  message-string tables, so the "nod" was cosmetic only and is now gone in favor of schema's own
  independent, sequential numbering.

### Notes

- Schema's implementation covers loading, presence, type-kind, and constraint validation with
  inheritance. See `notes/native-schema.md` for the full record, including a second real fixture
  bug (header ordering) caught along the way.

### Added — Distributable library (current)

- **A real root-level `Makefile`** builds `lib/debug/libanvil.a` and `lib/release/libanvil.a` —
  ANVL proper (core + types + schema) bundled as one archive for this milestone (the bundle-vs-
  minimal-split packaging question stays deferred; see `notes/deferred-work.md`). Release
  objects compile with no `-g` at all, then get an explicit `strip --strip-debug
  --strip-unneeded` pass on the archive itself — verified directly via `readelf -S` to carry zero
  `.debug_*` sections, at roughly 40% the size of the debug archive.
- **`test/functional/`** — a new test tier, deliberately structured to compile only its own test
  file and link against the *built* library (never a `src/*.c` file, never an `internal/`
  header), proving the shipped artifact itself works for a real consumer. 6 tests (core parse,
  AMP's restrictions, opt-in type resolution, AnvilSchema validating both a clean and a violating
  document, and minified-shebang parsing), 26/26 assertions, run against all four combinations
  (static/shared × debug/release), Valgrind-clean on all four.
- **`test/unit`'s `test_release` target actually works now** — it referenced
  `$(LIB_DEBUG)`/`$(LIB_RELEASE)` from the start but nothing ever built them, so it had never
  once succeeded; also fixed a real missing link dependency (`test/utilities/debug.c`) surfaced
  by finally running it. 33/33 tests, 159/159 assertions, linked entirely against the real
  release archive.
- **Shared object (`.so`) build, evaluated and added** — `make so` builds
  `lib/{debug,release}/libanvil.so` from the same `-fPIC` object tree the static archives use.
  Verified as a genuine dynamic-link consumer (not just "it compiled"): a copy of the functional
  suite linked against `libanvil.so` via `-L/-lanvil` + `rpath`, confirmed via `ldd` that it
  actually loads the `.so` at runtime, GREEN, Valgrind-clean. `test/functional/Makefile` gained
  permanent `so-debug`/`so-release` targets. Full technical record:
  `notes/distributable-library.md`.

### Fixed — Shebang dialect resolution hardened

- **Fixed-length shebang matching, replacing scan-to-newline** — every legal shebang is exactly
  5 bytes (`"#!"` + a 3-letter dialect token), a permanent grammar invariant, so
  `source_parse_shebang` now compares those 5 bytes directly instead of scanning to the next
  `\n` and requiring an exact 3-byte match in between. This closes a real minification gap
  (`"#!aml name := 1;"` and fully minified `"#!amlname := 1;"` both failed dialect resolution
  before this fix) found while validating whether full minification was actually supported
  anywhere — it wasn't, and never had been tested.
- **A real, more serious bug found along the way**: an *unrecognized* shebang dialect (a typo,
  or outright garbage after `#!`) was never checked anywhere — `doc_scan_header` only
  special-cased `== ANVL_DIALECT_AMP`, so an invalid shebang silently fell through and was
  treated as permissive AML with zero errors reported. Verified directly before fixing:
  `#!xyz\nconfig := { a := 1; };` parsed clean despite naming no real dialect. Fixed with a new
  error code (`ANVL_ERR_PARSER_INVALID_SHEBANG_DIALECT`) and an explicit check at the top of
  `doc_scan_header`. `test_source.c` (SRC39–41), `test_header.c` (HDR21), and
  `test/functional/test_e2e.c` (FN06) all cover this; full record in
  `notes/document-header-scan.md`'s "Shebang parsing" section.

### Fixed — FlyWire performance investigation (BR-2609-anvl-001 through -004)

A real production consumer's benchmark (`notes/flywire-parse-scaling-benchmark.md`) found
`anvil-node` measuring ~1.8x *slower* per byte than the vendored, pure-JS `anvil.js` at
production scale on FlyWire's real wire shapes — the opposite of what small-payload testing had
shown. Chasing that down surfaced four real, independent inefficiencies, three in core Anvil
Native and one spanning both native bindings' own conversion layers:

- **BR-2609-anvl-001**: `anvil_value_get_text`/`resolve_string_escapes` did a redundant third
  scan per string conversion and had no fast path for escape-free strings (base64, UUIDs, most
  identifiers) — real and correct, but turned out not to be on FlyWire's actual hot path (see
  BR-002).
- **BR-2609-anvl-002**: the real cause — `parse_blob_literal` scanned blob content one byte at a
  time (~2.4 million vtable-indirect calls for an 810KB blob) instead of a bulk scan. Fixed with
  a new `Source.consume_until` primitive. Also fixed a real, unrelated Makefile bug this change's
  own header edit exposed: no header dependency tracking, so a header-only change could silently
  ship a corrupted library with a mismatched vtable layout.
- **BR-2609-anvl-003**: even after -002, a real-harness crossover remained at scale — traced to
  `source_from_buffer`'s FNV-1a content hash, computed unconditionally on every load (root
  documents and AMP documents included, neither of which can ever need it for include dedup).
  Made the hash itself ~8x faster (word-wise instead of byte-wise) rather than touching the
  registration logic that depends on it.
- **BR-2609-anvl-004**: a different FlyWire wire shape (`@table`, many small values rather than
  one large blob) surfaced a cost in each binding's *own* value-conversion layer, not core Anvil
  Native — `anvil-node` built one N-API call per leaf scalar value (measured ~7–10ms of pure
  call overhead alone on a 100,000-leaf document); `anvil-wasm` already used a smarter one-JSON-
  string-boundary-crossing strategy, but had two smaller bugs of its own (a per-character
  escaping loop, an unnecessary numeric string round-trip). `anvil-node` adopted `anvil-wasm`'s
  JSON-string strategy with both bugs fixed from the start; `anvil-wasm` got the same two fixes
  applied directly.

Combined real-world result, verified against FlyWire's own harness: `anvil-node` now beats
`anvil-js` at every tested scale (12.5x–19.3x faster at small payloads, still ~1.6–4.2x faster
at FlyWire's original 10,000-record scale where it had previously fallen behind), and the
`@table` shape improved a further ~18%. Full detail and real numbers in `BR/BR-2609-anvl-001.md`
through `-004.md` and `notes/flywire-parse-scaling-benchmark.md`.

### Added — Real parser throughput numbers

- **`bench/throughput.c`** — a permanent benchmark tool (not a one-off script), linked against
  the built release library artifact like `test/functional/` does, measuring `anvil_load_buffer`
  end to end on a realistic, hand-authored-style AML config (inheritance, attributes, nested
  objects, arrays, mixed scalar kinds) at three scales. Real numbers — ~103–125 MB/s, holding
  steady across two orders of magnitude of document size — published to `README.md`'s new
  "Performance" section and the site's new "Real throughput" section.

### Notes

- `FR-2603-sigma-collections-006` and `-007`'s status lines were still naming feature branches
  as "awaiting merge/review" — both were confirmed already merged into `main` (`8acacfb`,
  `2a4ad98`); corrected to plain `resolved`.

### Changed — `import` renamed to `include`; `import` reserved for AnvilScript (FR-2609-anvl-public-api-002)

- **AML's `import "path.anvl";` is now `include "path.anvl";`** — same mechanism (DAG resolution,
  cycle detection, diamond dedup, forbidden in AMP), only the keyword changed. `import` is freed
  for AnvilScript's own foreign-code-bridge construct (`import "c:anvil.std.math";` and similar),
  renamed from `using` for the same reason — the word `import` reads as the cross-industry
  standard for "pull in a module, foreign ones included," where `using` read too much like C#'s
  own same-language namespace directive.
- **Public API renamed to match**: `anvil_document_get_imports`→`anvil_document_get_includes`,
  `anvil_type_registry_load_from_imports`→`anvil_type_registry_load_from_includes`, vtable field
  and error codes (`ANVIL_ERR_IMPORT`→`ANVIL_ERR_INCLUDE`, `ANVL_ERR_IMPORT_*`→
  `ANVL_ERR_INCLUDE_*`, same numeric values) renamed alongside.
- **Keyword reservation swapped consistently**: `import` is now reserved ahead of AnvlScript's own
  grammar (not yet implemented), matching this codebase's existing "reserve ahead of the feature"
  policy that `using`/`vars` already used — `using` is now fully freed, no longer reserved for
  anything.
- Full test suite renamed and re-verified: 14/14 suites green, Valgrind-clean. A real,
  pre-existing latent buffer-overflow bug in `test_header.c` (a fixed-size buffer with no room for
  `Source.substring`'s null terminator, silently off-by-one with the old 6-character `import`)
  surfaced and was fixed once the equivalent content grew by one character to `include`.
- AnvilScript's design docs (`notes/anvilscript-design.md` and friends) reconciled to the new
  scheme — this also resolved a genuine pre-existing ambiguity where ASL's `import` was described
  as literally reusing AML's mechanism verbatim; the two are now unambiguously distinct.
- Full detail: `FR/FR-2609-anvl-public-api-002.md`.

---

## [v0.7.0-alpha] — pre-release (2026-08-15)

**Status:** Source-hash identity refactor complete; all active unit tests passing and Valgrind-clean
**Milestone:** Source identity, header scanning, and full Source interface coverage

### Added

- **Process-wide source-hash registry** (`include/internal/source_registry.h`, `src/core/source_registry.c`) — maps FNV-1a 64-bit source content hashes to `module_document` identities. Exposes `Registry` vtable: `init`, `add`, `remove`, `find`, `count`, `release`, `clear`.
- **Source interface expansion** (`src/core/source.c`) — full vtable implementation covering create/dispose, `from_buffer`, `from_file`, FNV-1a hash, position/line/column tracking, EOF/peek/match helpers, character classification, consume bounds, whitespace/comment skipping, shebang detection, and registry-backed `has_errors`/`set_error`.
- **Document header scanner** (`src/core/document.c`) — `doc_scan_header` extracts `import` declarations and module attributes (`@[...]`) before the first body statement. Stores imports/attributes as no-copy `anvl_slice` metadata into the source buffer.
- **Import loader** (`src/core/module.c`) — `mod_load_imports` expands the import graph recursively with path resolution relative to the parent document, cycle detection via a path stack, and diamond deduplication via the source-hash registry.
- **Header metadata types** (`include/internal/module.h`) — added `anvl_src_slice_t` (typedef `anvl_slice`, pointer alias `slice`), `anvl_doc_import_t` (with `resolved` back-pointer), `anvl_doc_attribute_t`, `struct anvl_doc_header_t`, and the `header` field on `module_document`.
- **Dedicated test suites**
  - `test/unit/test_header.c` — HDR00, HDR02–HDR05, HDR07–HDR10 covering empty headers, imports, comments, malformed imports, unterminated comments, attributes, ordering violations, and body termination; HDR11–HDR15 covering import-loader single import, nested import, diamond reuse, cycle rejection, and missing file.
  - `test/unit/test_source.c` — SRC00–SRC22 covering every public `Source` helper and registry error routing.
  - `test/unit/test_registry.c` — REG00–REG08 covering registry add/remove/find/count and reference-counted lifecycle.
- **Import-loader fixtures** (`test/fixtures/hdr_import_*.anvl`) — fixture graph for single, nested (`hdr_import_nested.anvl` → `hdr_import_types.anvl` → `hdr_import_base.anvl`), diamond, cyclic, and missing import tests.
- **Test utilities** (`test/utilities/helpers.c`, `test/utilities/helpers.h`) — added `slice_equals`, `slice_is_empty`, `setup_registered_doc`, and `setup_registered_file` shared helpers for source-slice assertions and registered-document setup and fixture-based document setup.
- **Test/infra Makefile update** (`test/infra/Makefile`) — added `errors.c` and `source_registry.c` to `ANVIL_SRCS` so infra suites link against the current core.

### Fixed

- **`Source.from_file` buffer leak** (`src/core/source.c`) — frees the intermediate buffer returned by `Files.load` after copying it into the source object.
- **`doc_load_source` source leak** (`src/core/document.c`) — no longer creates a new source object when `doc->source` already exists.
- **`anvl_error_set` NULL output guard** (`src/core/errors.c`) — accepts a NULL `out_err_code` so `Source.set_error(..., NULL)` still appends the error.
- **Document double-dispose in tests** (`test/unit/test_document.c`) — tests no longer call `doc_dispose` on documents already owned by a context after successful `mod_ctx_register_doc`; prevents use-after-free during `mod_ctx_dispose`.
- **Registry module reference lifecycle** (`src/core/module.c`) — `mod_dispose` calls `Registry.release()`, and `mod_initialize` error paths call `mod_dispose` to release the registry reference.
- **Registry cleanup in unit tests** (`test/unit/*.c`) — each suite teardown now calls `Registry.clear()` to obtain clean Valgrind reports when bypassing `mod_new`/`mod_dispose`.

### Changed

- **Slice representation switched to a self-referential pointer triple** (`include/internal/module.h`, `include/internal/source.h`, `src/core/source.c`, `src/core/document.c`, `src/core/module.c`) — `anvl_src_slice_t` (value typedef `anvl_slice`, pointer alias `slice`) now stores `{data, start, end}` char pointers into the owning source buffer instead of `{start, length}` byte offsets. Length, emptiness, and substring extraction are derived from the three pointers rather than cached: `Source.slice_length`/`Source.slice_is_empty` replace direct field access, and `Source.substring` is reworked to take a slice directly (`Source.substring(anvl_slice, char *out_buffer)`) instead of `Source.substring(source, start, length, buffer)`. All header-scan (`document.c`) and import-loader (`module.c`) call sites updated to build and read slices through this API.
- **Shebang parsing moved to source load** (`src/core/source.c`, `src/core/document.c`) — `Source.from_buffer`/`Source.from_file` now detect and consume `#!dialect` and advance the source position past the shebang line. `Source.is_shebang` reports the `has_shebang` flag, and `Source.dialect` reports `ANVL_DIALECT_ERROR` for invalid shebangs. `doc_scan_header` no longer validates the shebang; invalid shebangs are surfaced by the source loader instead. File-extension hints are applied only when no shebang is present, so a shebang overrides the extension.
- **Source struct** (`include/internal/source.h`) — added `bool has_shebang` field.
- **`test/unit/Makefile`** — added `test_source`, `test_registry`, and `test_header` build/run targets; disabled `test_fixtures` (depends on deprecated root/doc APIs).

### Test Results

- Unit suites: 28/28 module, 31/31 document, 9/9 registry, 15/15 header, 23/23 source — all passing
- Valgrind: 0 errors and 0 bytes in use at exit across all active unit suites
- Infra suites: `test_files`, `test_strings`, `test_version` passing

### Notes

- Prefixed legacy files (`_source.c`, `_parser.c`, etc.) remain in the tree as reference material; extract all relevant functionality before archiving them.
- Sigma library wishlist (`/memories/repo/sigma-wishlist.md`) captures candidates like `String.substring`, `starts_with`/`ends_with`, `trim`, and `index_of`/`contains` for a future Sigma refactoring iteration.

---

## [v0.5.6-alpha] — pre-release (2026-07-03)

### Fixed

- **Anonymous-block inheritance parse path** (`src/core/parser.c`) — parser now accepts anonymous object block inheritance syntax with optional attributes (`Ident : Base { ... }` and `Ident : Base @[...] { ... }`) and preserves base metadata on `ANVL_ANON_OBJECT` statements.
- **WASM32 shift-width warnings in blob length helpers** (`include/types.h`) — blob tag/content length packing now uses explicit `uint64_t` encode/decode so wasm32 targets avoid invalid `usize` 56-bit shifts.

### Changed

- **Parser structures coverage expanded** (`test/unit/test_parser_structures.c`) — added inheritance-focused anonymous-block structural tests (PS14, PS15) in the primary structures suite.
- **Test fixture layout cleanup** (`test/fixtures/`, `test/unit/*`, `test/benchmarks/*`, `test/utilities/helpers.*`) — migrated sample assets from `test/samples/` to `test/fixtures/` and updated test helpers/consumers to use the consolidated fixtures path.
- **Core build matrix expanded** (`lib/Makefile`, `BUILDING.md`) — added `wasm` output target (`lib/wasm/libanvil.a`), updated `clean` to remove wasm artifacts, and changed `make -C lib all` to build debug + release + wasm + bindings.
- **WebAssembly integration docs** (`docs/wasm.md`, `README.md`, `BUILDING.md`) — clarified that `lib/wasm/libanvil.a` is an Emscripten static archive and final browser package ownership lives in binding/app repos (for example `anvil.js`).
- **Users guide rename + schema examples refresh** (`docs/Anvil Users Guide.md`, `test/fixtures/schema.asch`) — renamed the guide from `Anvil C Users Guide` to `Anvil Users Guide` and pulled schema language examples directly from supported fixture-backed syntax.
- **No public C API surface changes** — this release is parser behavior, build orchestration, and documentation focused.

---

## [v0.5.5-alpha] — pre-release (2026-07-03)

### Added

- **Collection element child-value access API** (`include/context.h`, `src/core/context.c`) — added two context accessors so bindings can traverse non-scalar array/tuple elements without reparsing source spans:
  - `Context.get_element_value(ctx, stmt, index)`
  - `Context.get_value_element_value(ctx, val, index)`
- **Element metadata child handle** (`include/types.h`, `src/core/parser.c`) — `anvl_element_meta` now carries a `child` value handle for element-level traversal of nested object/array/tuple elements.
- **Tests: CTX13–CTX14** (`test/unit/test_context.c`) — coverage for object elements and nested collection elements retrieved via the new element child-value accessors.

### Fixed

- **Binding traversal gap for non-scalar collection elements** (`src/core/parser.c`, `src/core/context.c`) — bindings can now obtain nested element value handles directly from context APIs instead of being limited to type + source span metadata.
- **Blob value canonicalization across dialects** (`src/core/parser.c`) — blob literals now parse as `ANVL_VALUE_BLOB` consistently (including AML/ASL) instead of legacy array-based blob metadata. This keeps blob semantics stable for bindings and removes scalar-empty placeholder behavior when traversing nested values.

### Changed

- **Version bump** (`include/constants.h`) — updated to `0.5.5-alpha`.

---

## [v0.5.4-alpha] — pre-release (2026-07-01)

### Added

- **Test coverage: vars parser edge-cases** (`test/unit/test_vars.c`) — added V21–V23 TestBit checks for braced dotted VarRef parsing, unbraced dotted VarRef parsing, and dedicated unterminated braced VarRef parser error.
- **Test coverage: interpolation error-code assertions** (`test/unit/test_interp_string.c`) — IS07 and IS08 now assert exact parser error codes (`ANVL_ERR_VARS_UNTERMINATED_INTERP`, `ANVL_ERR_VARS_INVALID_VARREF`) instead of only checking generic error presence.
- **Make ergonomics (active suite targets)** (`test/unit/Makefile`) — added explicit convenience targets for `test_interp_string`, `test_using`, `test_anon_block_attrs`, and `test_schema` and wired `test_vars` into active aggregate/release/valgrind flows.
- **Official bindings workspace scaffold** (`bindings/`) — added `bindings/Makefile` orchestration, official workspace layout (`node`, `python`, `dotnet`), and shared handoff script (`bindings/scripts/handoff_manifest.py`) that emits `bindings/.handoff/binding-handoff.json`.
- **Pinned maintainership docs** (`docs/maintainers/`) — added `bindings-maintenance.md` and `bindings-signoff-checklist.md` for maintainers, contributors, and binding teams.

### Fixed

- **Valgrind blocker: `all` target recipe bug** (`test/unit/Makefile`) — `all` now correctly lists binaries as prerequisites instead of attempting to execute path strings as shell commands.
- **Vars state leak under valgrind** (`src/vars/vars.c`) — `Vars.build()` now disposes temporary allocations (`done`, `path`) and frees owned state on build failure; `Vars.dispose()` now releases `state->entries` and `state`. `test_vars` valgrind run is now clean.
- **Error table completeness** (`src/core/errors.c`) — wired `ANVL_ERR_VARS_UNTERMINATED_INTERP` into both error message and error name lookup tables.

### Changed

- **Docs consistency refresh** (`README.md`, `docs/parser-architecture.md`, `docs/reference.md`, `docs/test-gaps.md`, `docs/Anvil.Net Users Guide.md`) — aligned ASL status/roadmap wording with current implementation, replaced stale Sigma.Test references with TestBit framing for active suites, refreshed vars suite count in test-gaps, and clarified binding-specific var-ref syntax context.
- **Build orchestration split by concern** (`lib/Makefile`, `bindings/Makefile`) — core library build now triggers official bindings generation via a dedicated bindings Makefile instead of growing a monolithic root build script.

---

## [v0.5.3-alpha] — pre-release (2026-06-28)

### Added

- **Value-level collection traversal API** (`include/context.h`, `src/core/context.c`) — five new functions enabling traversal of nested collections inside field values (FR-2604-anvl-001):
  - `Context.value_element_count(ctx, val)` — count elements in array/tuple values
  - `Context.get_value_element(ctx, val, index)` — access array/tuple elements by index
  - `Context.value_field_count(ctx, val)` — count fields in object values
  - `Context.get_value_field(ctx, val, index)` — access object fields by index
  - `Context.get_value_field_by_name(ctx, val, name, len)` — access object fields by name (linear scan)
- **Tests: VT01–VT15** (`test/unit/test_context.c`) — 15 new test cases covering value-level element/field access, NULL guards, out-of-range handling, and nested object traversal
- **Custom merge policy API** (`include/resolver.h`, `src/resolver/resolver.c`) — four new functions enabling consumer-controlled inheritance merge strategies (FR-2604-anvl-002):
  - `anvl_node_state_get_base_index(state, stmt_idx)` — resolve base statement index
  - `anvl_node_state_get_own_fields(state, stmt_idx)` — access unmerged fields from derived statement
  - `anvl_merge_policy_fn` — callback type for custom field merge logic
  - `anvl_node_state_get_merged_fields_custom(state, stmt_idx, policy, userdata)` — merge with policy callback
- **Tests: CM01–CM10** (`test/unit/test_resolver.c`) — 10 new test cases covering base index lookup, own fields access, array concatenation, object deep merge, field exclusion, NULL policy preservation, nested inheritance, error handling, and caching
- **`Anvil.get_version()`** (`include/anvil.h`, `src/core/anvil.c`) — runtime version query on the `Anvil` vtable; returns `major.minor.patch+build-tag` string with build number injected at compile time via `ANVL_BUILD`
- **Version macros** (`include/constants.h`) — consolidated to `ANVL_VERSION_MAJOR`, `ANVL_VERSION_MINOR`, `ANVL_VERSION_PATCH`, `ANVL_VERSION_TAG`, `ANVL_VERSION_STR`; removed duplicate definitions from `include/anvil.h` and `include/types.h`
- **`lib/Makefile`** — new; builds `lib/debug/` and `lib/release/` static and shared libraries; `ANVL_BUILD` injected via `git rev-list --count HEAD`
- **Tests: AM08–AM12** (`test/unit/test_amp_collections.c`) — empty array, empty tuple, empty object, single-element tuple rejection; two-element tuple acceptance
- **`test_version`** (`test/unit/test_version.c`) — standalone version output binary; appended to each test target run

### Fixed

- **Allocator API** (`src/resolver/resolver.c`) — updated all `Allocator.free()` calls to `Allocator.dispose()` to match current sigma API (resolver module hadn't been updated since allocator API migration)
- **BR-2604-anvil-001:** Parser now rejects inheritance from anonymous objects. Anonymous object syntax (`Base { x := 10 }`) is incompatible with inheritance — statements that use inheritance (`:Base`) must use explicit assignment syntax (`Base := { x := 10 }` or `Derived:Base := { y := 20 }`). Added `ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS` error code (4052) and validation in `anvl_node_state_get_base_index()`. This enforces semantic consistency: anonymous objects are truly immutable and cannot be inherited from.
- **Parser: bare `#` rejection** — `parse_scalar_value` now rejects `#` not followed by an alphanumeric character with `ANVL_ERR_PARSER_UNEXPECTED_TOKEN`; valid forms `#RRGGBB` and `#identifier` unaffected
- **Parser: empty tuple `()` rejected** — `parse_tuple` now emits `ANVL_ERR_PARSER_EMPTY_TUPLE_NOT_ALLOWED` on empty tuple, consistent with existing empty array and empty object behavior
- **Parser: single-element tuple rejected** — `parse_tuple` now emits `ANVL_ERR_PARSER_TUPLE_TOO_FEW_ELEMENTS` when `element_count < 2`

### Changed

- **`src/core/anvil_impl.c` renamed to `src/core/anvil.c`**
- **`lib/` build scope** — `asl` and `serializer` removed from library build sources
- **`test/unit/Makefile`** — `ANVL_BUILD` injected; `test_release` target added; `test_version` wired as prerequisite and appended to all test targets

### Added (docs)

- **reference.md § 9.5** — Value-Level Collection Traversal chapter with API documentation and usage examples for traversing nested arrays/objects inside field values (FR-2604-anvl-001)
- **reference.md § 10** — Custom Merge Policies chapter with API documentation, rationale, usage examples (array concatenation, field exclusion, sigma.collections integration), and migration path (FR-2604-anvl-002)

---

## [v0.5.2-alpha] — pre-release (2026-03-21)

**Status:** E3 query path primitives implemented; all unit tests passing (19/19 suites, 0 failures)
**Milestone:** v1.0.0-rel gate clearance — E3 field/element navigation API complete

### Added

- **E3 query path primitives** (`include/context.h`, `src/core/context.c`) — five new
  slots on `anvl_context_i` enabling field and element navigation without raw struct access:
  - `Context.field_count(ctx, stmt)` — count of fields in an object-valued statement
  - `Context.get_field(ctx, stmt, i)` — i-th field by index
  - `Context.get_field_by_name(ctx, stmt, name, len)` — field by key name (NULL if absent);
    O(n) linear scan now, O(1) after `Map` backing lands (FR-2603-sigma-collections-002)
  - `Context.element_count(ctx, stmt)` — count of elements in an array or tuple statement
  - `Context.get_element(ctx, stmt, i)` — i-th element span (index-only — no names)
- **Tests: QP01–QP15** (`test/unit/test_interrogators.c`) — 15 new test cases covering all
  five primitives including NULL guards, out-of-range returns, and nested object traversal

### Fixed

- **Parser: `parse_object` nested field pool ordering** — outer object fields were added to
  `ctx->field_list` before inner (nested) object fields had been added, causing the outer
  object's `field_start` to index the wrong entries.  Fields are now collected in a
  temporary arena-allocated linked list during the parse loop and bulk-flushed to the pool
  after all descendant fields are committed, so `field_start` always points past all
  descendant entries.  This was a latent bug — not observable in practice until the E3
  `get_field_by_name` traversal exposed it.

### Added (docs)

- **Port-checklist E3 gate** — four of seven items now ✅:
  `field_count`, `get_field`, `get_field_by_name`, `element_count`, `get_element`, tests
  (remaining: `docs/reference.md` traversal docs, `anvil.api` package)

---

## [v0.5.1-alpha] — pre-release (2026-03-21)

**Status:** Phase 4 gates complete; all unit tests passing (19/19 suites, 73/73 parser tests, 0 failures)
**Milestone:** v1.0.0-rel gate clearance — Sigma.Collections audit, test samples updated

### Added

- **Test sample: `test/samples/vars.anvl`** — reference sample demonstrating the `vars {}` block,
  `$varref` fields in objects, and `$"interpolated {string}"` top-level statements
- **Test sample: `test/samples/schema.asch`** — reference sample demonstrating `@[schema]` enum,
  flags, and object type definitions (EntitySide, FilePerms, BlockConfig, ItemConfig)
- **Parser tests** (`test/unit/test_parser.c`) — two new sample-parse tests registered:
  `test_parse_vars_sample` and `test_parse_schema_asch`; parser test count raised to 73

### Added (docs)

- **Port-checklist Phase 4** — all gates now ✅:
  - Sigma.Collections audit complete: no allocator hook exists in any collection type;
    bump-arena lifecycle is incompatible; manual grow-arrays retained by design
  - Test samples: `vars.anvl` and `schema.asch` added; tests verified green

---

## [v0.5.0-alpha] — pre-release (2026-03-21)

**Status:** sigma migration complete; all unit tests passing (19/19 suites, 0 failures)
**Milestone:** Infrastructure upgrade — module system, new allocator API, sigma.text, test framework

### Added

- **Module system** (`src/core/module.c`)
  - Anvil registers as `SIGMA_ROLE_USER` via `sigma_module_init_all()` / `sigma_module_shutdown_all()` called from `src/cli/main.c`
  - Module bootstrap creates a 1 MiB bump slab for `StringBuilder` allocations; wires `StringBuilder.alloc_use()` on init and restores NULL on shutdown
  - Anvil dependency chain: `sigma` → `sigma.module` → `sigma.text`

- **sigma.text** (`<sigma/strings.h>`)
  - All `<sigma.text/strings.h>` includes updated to `<sigma/strings.h>` — header is now the canonical location for `String`, `StringBuilder`, and helper functions

### Changed

- **Allocator API migration** (from sigma 0.2.x R7 API to sigma/sigma current)
  - `Allocator.dispose(x)` → `Allocator.free(x)` — ~60 call sites across `src/` and `test/`
  - `rscope` type → `bump_allocator` (`include/context.h`)
  - `Allocator.Resource.acquire(size)` → `Allocator.create_bump(size)`
  - `Allocator.Resource.release(arena)` → `Allocator.release((sc_ctrl_base_s *)arena)`
  - `Allocator.Resource.alloc(arena, size)` → `arena->alloc(arena, size)` (all call sites in `context_internal.h` and `parser.c`)
  - Removed `--wrap=malloc/free` linker flags from `TST_LDFLAGS`; new sigma uses mmap/slab allocators and no longer exports `__wrap_*` symbols
  - `REQUIRES` / `TST_REQUIRES` in `config.sh` updated to reflect new package names

- **Test framework** (`test/unit/*.c`, 19 files)
  - Migration from direct `testset()` constructor registration to `Tests.enqueue(_register)` pattern
  - `_run_pending()` fires after `sigma_module_init_all()` in `main()`
  - `test/validation/test_prototype.c` moved from `test/unit/` to `test/validation/`

- **Context arena sizing** (`src/core/context.c`)
  - Slab multiplier increased from 4× to 16×, floor raised from 4 KiB to 64 KiB
  - Fixes OOM failures on larger sample files (modpack.anvl, arrays.anvl, tuples.anvl) caused by the doubling-without-free strategy in `ci_ensure_*_capacity` consuming arena space

### Fixed

- `writelnf` implicit declaration: removed all calls to the now-absent `writelnf()` helper across
  `test/unit/test_parser.c`, `test/unit/test_statements.c`, and `test/utilities/diagnostic.c`;
  replaced with `fprintf(stderr, "...\n", ...)` — `writelnf` was removed in the sigma rewrite
- Shutdown segfault: `_anvil_shutdown` no longer calls `Allocator.release(bump)` — sigma drains its own controllers on module shutdown; releasing before sigma.test's cleanup caused a use-after-free in `stringbuilder_dispose`
- `Time` symbol missing in bench packages: added `sigma.utils.o` to `TST_REQUIRES` and bench package lists in `config.sh`

---

## [v0.4.5-alpha] — pre-release (2026-03-12)

**Status:** Schema resolver + validator complete; v0.4.5-alpha gate satisfied  
**Milestone:** Schema definition resolution and data validation against typed field rules

### Added

- **Schema** (`include/schema.h`, `src/schema/schema.c`)
  - `Schema.resolve(ctx)` — walks a `@[schema]`-attributed document; classifies each statement as `ANVL_SCHEMA_OBJECT`, `ANVL_SCHEMA_ENUM`, or `ANVL_SCHEMA_FLAGS`; returns `anvl_schema_ruleset_t *` or NULL with error
  - `Schema.validate(rules, data_ctx, file_path)` — walks a data document; for each typed statement whose base resolves to a known schema type, checks all declared fields are present with the correct `anvl_value_type`; collects all violations before returning (no fail-fast)
  - `Schema.get_type(rules, name)` — O(n) lookup by name in a ruleset
  - `Schema.ruleset_free(rules)` / `Schema.result_free(result)` — full deep-free helpers
  - `anvl_schema_type_t` — name, kind, fields array (Object) or values array (Enum/Flags)
  - `anvl_field_rule_t` — field name + `expected_type` (`anvl_value_type`)
  - `anvl_schema_ruleset_t` — dynamic array of `anvl_schema_type_t *` with capacity doubling
  - `anvl_schema_result_t` — `is_valid` flag + dynamic `anvl_schema_error_t` array
  - `anvl_schema_error_t` — validation error with code, message (owned copy), and optional file path
  - Error codes: `ANVL_ERR_SCHEMA_ATTR_MISSING` (4601), `ANVL_ERR_SCHEMA_TYPE_UNRESOLVED` (4602), `ANVL_ERR_SCHEMA_BASE_UNKNOWN` (4603), `ANVL_ERR_SCHEMA_VALIDATION_REQUIRED` (4604), `ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH` (4605), `ANVL_ERR_SCHEMA_VALIDATION_UNKNOWN_FIELD` (4606)
  - `anvil.schema` package in `config.sh`; `with_schema_objects` wired in `rtest`

- **20 Schema unit tests** (`test/unit/test_schema.c`)
  - **SC01–SC05** Resolver — enum/flags: non-null ruleset, no-attr error, enum kind, flags kind, enum values
  - **SC06–SC08** Resolver — object types: kind is OBJECT, field names correct, field expected types correct
  - **SC09–SC15** Validator: valid data passes, missing required field error, type mismatch error, extra fields permitted, untyped statements pass through, unknown base returns null, multiple violations all collected
  - **SC16–SC20** File I/O and multi-type: load .asch file, nonexistent file handled, file-based validation, two typed stmts both missing fields, two schema types both validated

### Fixed

- `find_own_field` in `schema.c`: parser allocates nested object fields depth-first (inner fields precede their parent in the flat `field_list`); fixed scan to cover the full statement field range rather than relying on `field_start + field_count` which excluded sibling fields after a nested object
- AML C parser requires commas between object fields (unlike the C# reference); all schema test source strings updated accordingly

---

## [v0.4.3-alpha] — retroactive gate (2026-03-11)

**Status:** Released (retroactive tag — work landed in v0.1.1-alpha)  
**Milestone:** AMP scalar arrays/tuples gate satisfied

This gate was satisfied early. The AMP scalar enforcement work was implemented as low-hanging fruit during the initial port and shipped in v0.1.1-alpha, predating the formal versioning scheme. No new code; this entry closes the gate retroactively.

### Gate Condition Satisfied

- AMP scalar array `[v, …]` parsing ✅ — `parse_array()` rejects non-scalar elements in AMP dialect
- AMP scalar tuple `(v, …)` parsing ✅ — `parse_tuple()` rejects non-scalar elements in AMP dialect
- `ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR = 4401` ✅ — defined in `include/errors.h`
- 5 parser tests ✅ — 2 happy-path (valid scalar array/tuple), 3 rejection (nested object/array/tuple in AMP)

*See v0.1.1-alpha entry for full implementation details.*

---

## [v0.4.0-alpha] — pre-release (2026-03-12)

**Status:** ASL parser + evaluator complete; v0.4.0-alpha gate satisfied  
**Milestone:** Full ASL (Anvil Script Language) parse + tree-walk evaluation with flat scope, control flow, and external dispatch

### Added

- **ASL** (`include/asl.h`, `src/asl/asl.c`)
  - `Asl.parse(meta, src)` — Pratt-style recursive-descent parser; returns heap-allocated AST (`asl_node_t *`)
  - `Asl.exec(meta, src, args, argc, out, modules, ...)` — parse → bind parameters → tree-walk eval → return value
  - `Asl.node_free(node)` / `Asl.value_free(val)` — recursive free helpers
  - `asl_value_t` — tagged union: `ASL_NULL`, `ASL_INT`, `ASL_FLOAT`, `ASL_STRING`, `ASL_BOOL`, `ASL_TUPLE`, `ASL_LIST`, `ASL_MAP`
  - `asl_node_t` — AST node with child array; node kinds cover literals, binary/unary ops, identifiers, assign, block, if, for, break, continue, call, return
  - `asl_function_meta_t` — function name/body/parameter spans (source-level offsets)
  - `asl_module_t` — external module dispatch callback (`name` + `call` function pointer + `userdata`)
  - `asl_ext_lookup_cb` / `asl_fn_dispatch_cb` — callbacks for `$varref` resolution and function dispatch
  - Flat scope (`asl_scope_impl_t`): max 256 vars, deep-copy on store/return
  - Truthiness: `null`→false, zero/`""`/empty containers→false, else→true
  - `i++` / `i--` desugared to `assign(i, binary(i, 1))`
  - `for` loop children: `[init, cond, step, body]`; `break` / `continue` as scope-exit flags
  - Error codes: `ANVL_ERR_ASL_PARSE_ERROR` (5101), `ANVL_ERR_ASL_RUNTIME_ERROR` (5102), `ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED` (5103), `ANVL_ERR_ASL_BREAK_OUTSIDE_LOOP` (5104), `ANVL_ERR_ASL_CONTINUE_OUTSIDE_LOOP` (5105)
  - `anvil.asl` package in `config.sh`; `with_asl_objects` wired in `rtest`

- **25 ASL unit tests** (`test/unit/test_asl.c`)
  - **A01–A05** Parser structural: non-null node, root is BLOCK, literal child, float child, string child
  - **B01–B05** Literals: exec int 42, float 3.14, string `"hello"`, bool `true`, `null`
  - **C01–C05** Arithmetic: add, multiply, subtract, modulo, string concatenation
  - **D01–D05** Control flow: if-taken, else-taken, for-loop accumulator, break, continue
  - **E01–E05** Scope / dispatch: local assignment, `$varref` external lookup, comparison, logical `&&`, parameter binding

---

## [v0.3.0-alpha] — pre-release (2026-03-12)

**Status:** Import Graph complete; v0.3.0-alpha gate satisfied  
**Milestone:** Full import DAG resolution with DFS cycle detection, diamond dedup, and loader callback interface

### Added

- **Import Graph** (`include/import.h`, `src/import/import.c`)
  - `import "path" [as alias]` declaration parsing in `parser.c`; stored as source spans in `ctx->import_list`
  - `struct anvl_import_decl` — `{path_pos, path_len, alias_pos, alias_len}` (types.h)
  - `import_list` field in `anvl_context_t` — flat array of declarations (context.h)
  - `ImportGraph.build(ctx, owner_path, loader, userdata)` — DFS graph walk
  - `ImportGraph.dispose(graph)` — frees all owned memory
  - `anvl_import_loader_cb` — binding-provided callback; core never opens file descriptors
  - Canonical-first dedup (diamond dedup): same path loaded twice → single entry, extra alias registered
  - DFS load-stack cycle detection → `ANVL_ERR_IMPORT_CYCLIC`
  - Duplicate alias detection → `ANVL_ERR_IMPORT_DUPLICATE_ALIAS`
  - File-not-found propagation → `ANVL_ERR_IMPORT_FILE_NOT_FOUND`
  - Placement guards: import after statements → `ANVL_ERR_IMPORT_NOT_FIRST`; import in AMP dialect → `ANVL_ERR_IMPORT_AMP_FORBIDDEN`
  - Stem alias derivation: `my-blocks.aml` → `my_blocks` (StringBuilder-based, Allocator-backed)
  - `anvil.import` package in `config.sh`; `with_import_objects` wired in `rtest`

- **10 import unit tests** (`test/unit/test_import.c`)
  - Parser: no imports, single import, explicit alias, import-after-statement error
  - Graph builder: empty graph, stem alias, explicit alias, duplicate alias error, cyclic error, diamond dedup

---

## [v0.2.2-alpha] — pre-release (2026-03-12)

**Status:** Vars / VarRef complete; Import Graph remaining for v0.3.0-alpha gate  
**Milestone:** Full VarRef + interpolated string resolution; build system modernized

### Added

- **Vars / VarRef** (`include/vars.h`, `src/vars/vars.c`)
  - `Vars.build(ctx, src)` — resolves VarRef chains, detects circular references at build time
  - `Vars.resolve(state, key, key_len, ...)` — look up a key in the resolved vars state
  - `Vars.materialise_interp(state, ctx, vm)` — expands `$"…{ref}…"` to a heap-allocated string
  - `Vars.dispose(state)` — frees all vars-state owned memory
  - Vtable interface pattern consistent with `Anvil` / `Context` / `Source` / `Serializer`
  - `ANVL_VALUE_VAR_REF` and `ANVL_VALUE_INTERP_STRING` value types parsed in `parser.c`
  - Vars block (`vars { }`) fully parsed; key/value entries stored as source spans
  - Interpolated string segment list (literal + ref spans); resolved at materialise time
  - Circular ref detection eager at `Vars.build()` time
  - Missing key returns `ANVL_ERR_VARS_KEY_NOT_FOUND` on `Vars.materialise_interp()`
  - String building in `materialise_interp` uses **sigma.text** `StringBuilder` (Allocator-backed)

- **20 vars unit tests** (`test/unit/test_vars.c`)
  - Vars build: no vars block, empty block, single/multi key
  - VarRef: direct ref, ref-to-ref chain, unknown key
  - Circular ref detection: direct cycle (A→A), indirect cycle (A→B→A), non-cycle chain
  - InterpolatedString: literal-only, single ref, multi-ref, prefix/suffix, multi-segment
  - Error paths: `materialise_interp` on non-interp-string value, null state

### Changed

- **Build system**: Makefile removed; project now uses `cbuild` / `rtest` exclusively
- **Include path**: all test files migrated from `<sigtest/sigtest.h>` → `<sigma.test/sigtest.h>`
  (`/usr/local/include/sigma.test/` is the canonical location)
- **`.vscode/c_cpp_properties.json`**: updated to sigma.* include paths; removed stale makefile-tools
  provider; `cStandard` updated to `c23`
- **`vars_materialise_interp`**: replaced hand-rolled `sb_t` buffer with `StringBuilder` from
  **sigma.text** — all string-building heap allocations now flow through `Allocator`

### Test Results

- 192 tests across 13 test files; 192/192 passing
- Zero memory leaks

---

## [v0.2.1-alpha] — pre-release (2026-03-11)

**Status:** Serializer complete; v0.2.0-alpha gate fully satisfied  
**Milestone:** AML/AMP/ASL serializer (writer) with vtable interface

### Added

- **Serializer / Writer** (`include/serializer.h`, `src/serializer/serializer.c`)
  - `Serializer.serialize(ctx, src, opts)` — emits AML/AMP/ASL text to a heap string
  - `Serializer.to_stream(ctx, src, opts, stream)` — writes directly to a `FILE *`
  - `opts.minify` — single-line, comma-separated, no padding
  - AML dialect: full support for scalar, object, array, tuple, blob value types
  - AMP dialect: scalar-only guard enforced at write time
  - Blob verbatim fidelity: raw source span emitted; round-trip idempotency verified
  - Vtable interface (`Serializer` global const) matching `Anvil` / `Context` / `Source` pattern
  - Uses **sigma.text** `StringBuilder` for all output buffering

- **Serializer tests** (`test/unit/test_serializer.c`)
  - Round-trip tests for AML and AMP dialects
  - Blob fidelity tests (ST06c–ST06f)
  - Minify mode output tests

### Test Results

- 172 tests across 12 test files; 172/172 passing
- Zero memory leaks

---

## [v0.2.0-alpha] — pre-release (2026-03-11)

**Status:** Complete — Resolver + Writer (serializer) gate satisfied; see v0.2.1-alpha  
**Milestone:** Single-inheritance resolution, cycle detection, lazy merge cache

### Added

- **Inheritance Resolver** (`include/resolver.h`, `src/core/resolver.c`)
  - `anvl_resolver_build_state(ctx, src)` — validates the inheritance DAG using Kahn's
    topological sort; returns NULL (no error) when no statements have a base (fast-path)
  - `anvl_node_state_get_merged_fields(state, idx)` — lazily computes merged field list;
    derived fields override base fields (last-write-wins); result cached per slot
  - `anvl_node_state_warm_all(state)` — pre-warms all merge slots eagerly
  - `anvl_node_state_dispose(state)` — frees all resolver-owned heap
  - FNV-1a open-addressing hash (`anvl_id_map_t`) for identifier→index mapping
  - Forward-reference support (derived declared before base — Kahn handles ordering)
  - `ANVL_ERR_RESOLVER_CYCLE_DETECTED` (4050) — set on cycle in build_state
  - `ANVL_ERR_RESOLVER_MISSING_BASE` (4051) — deferred; set on first access in get_merged_fields

- **Type name utilities** (`src/core/types.c`)
  - `anvl_value_type_name(type)` — returns display string for `anvl_value_type` enum values
  - `anvl_stmt_type_name(type)` — returns display string for `anvl_stmt_type` enum values

- **10 resolver unit tests** (`test/unit/test_resolver.c`)
  - No-inheritance fast-path
  - Single-level: inherits unoverridden fields; derived override; base unchanged
  - Three-level chain: full resolution and midpoint correctness
  - Forward reference (child declared before parent)
  - Cycle detection (A:B + B:A)
  - `warm_all` idempotency
  - Missing base deferred error

### Fixed

- **`parser.c`**: `value_meta->data.object.field_start` was never set — only `field_count`
  was transferred from the temporary `value` object to `value_meta` during statement
  finalization. This meant all object statements appeared to own fields starting at index 0,
  breaking resolver merge logic, field interrogation, and any future writer output.

### Architecture

- Resolver is a pure post-parse pass: takes a fully-built `context` + `source`,
  returns a disposable `anvl_node_state_t`; never modifies the context
- Merge cache holds `field*` pointer arrays pointing back into `ctx->field_list`; no deep copies
- Topological sort validates the DAG at build-state time; missing bases are deferred until
  first field access (consistent with Anvil.Net reference behavior)

### Test Results

- 153 tests across 11 test files; 153/153 passing
- Zero memory leaks

---

## [v0.1.1-alpha] — 2026-03-11

**Status:** Released  
**Milestone:** AMP scalar arrays/tuples enforcement + all Phase 1–3 error codes

### Added

- **AMP scalar enforcement in arrays and tuples** (`src/core/parser.c`)
  - `parse_array()` and `parse_tuple()` now reject non-scalar elements in AMP dialect
  - `ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR = 4401` added to `include/errors.h`
  - AMP arrays/tuples were previously blocked entirely; now allowed with scalar-only enforcement

- **All remaining Phase 1–3 error codes** (`include/errors.h`, `src/core/errors.c`)
  - Resolver: `ANVL_ERR_RESOLVER_CYCLE_DETECTED = 4050`, `ANVL_ERR_RESOLVER_MISSING_BASE = 4051`
  - Vars: `ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED = 4101` … `ANVL_ERR_VARS_KEY_NOT_FOUND = 4105`
  - Import: `ANVL_ERR_IMPORT_NOT_FIRST = 4201` … `ANVL_ERR_IMPORT_CYCLIC = 4206`
  - Using: `ANVL_ERR_USING_MODULE_NOT_FOUND = 4305` … `ANVL_ERR_USING_AFTER_STATEMENTS = 4307`
  - Full message and name table entries added for all new codes

- **5 new AMP parser tests** (`test/unit/test_parser.c`)
  - Valid AMP scalar array accepted
  - Valid AMP scalar tuple accepted
  - AMP array with nested object rejected (4401)
  - AMP array with nested array rejected (4401)
  - AMP tuple with nested object rejected (4401)

### Test Results

- 143 tests across 10 test files; 143/143 passing (up from 138 in v0.1.0-alpha)

---

## [v0.1.0-alpha] - 2025-12-20

**Status:** Production-Ready Parser Release  
**Milestone:** First public release with zero memory leaks and comprehensive AMP support

### Added

- Zero-copy, direct-construction parser for AML/ASL source code
- AMP (Anvil Messaging Protocol) dialect support with full validation
- Comprehensive memory management with 18 tracked allocations
- 17 AMP messaging protocol tests with 100% passing rate
- Parser documentation suite with 4 core documents:
  - PARSER_ARCHITECTURE.md: Complete architecture and design patterns
  - PARSER_QUICK_REF.md: Developer quick reference guide
  - TEST_COVERAGE_MAP.md: Test-to-code-path mapping
  - PARSER_DOCUMENTATION_INDEX.md: Navigation and index
- Version identifier: ANVIL_VERSION "v0.1.0-alpha" in include/anvil.h
- Memory leak fixes with comprehensive error path cleanup
- Error handling system with 24+ error codes
- Source interrogation interface for position tracking and metadata
- Context-owned statement and value metadata system

### Fixed

- **Leak #1:** Missing disposal at assignment operator check (parse_statement line 178-180)
  - Fixed base_meta and attr_meta not being disposed when := operator not found
  
- **Leak #2:** Missing disposal in attribute validation (parse_statement line 196)
  - Fixed value object not being disposed when attributes applied to scalar type
  
- **Leak #3:** Missing disposal in value_meta allocation (parse_statement line 211)
  - Fixed allocated value not being disposed on value_meta OOM condition
  
- **Leak #4:** Missing disposal in parse_source statement allocation (parse_source line 88-95)
  - Fixed dispose_statement() being a no-op stub
  - Now properly frees statements that fail to parse before being added to context
  - Resolved 6 orphaned statement allocations (1 from successful parses + 5 from AMP rejections)

### Verified

- All 17 AMP test cases passing
- Zero memory leaks: 138/138 allocations freed in full test suite
- Memory tracking via sigtest fat.o integration
- AMP dialect validation for rejection of:
  - Complex types (objects, arrays, tuples) in scalar-only context
  - Inheritance syntax (: operator) in AMP mode
  - Attributes (@[...] syntax) in AMP mode
- Full AML support with objects, inheritance, and attributes
- AML/AMP dialect auto-detection
- Envelope file parsing (response and event formats)
- Meta-buffer VALUE span tracking

### Architecture

- Parser is stateless after anvl_parse() returns
- All allocations owned by context until Context.dispose()
- Temporary allocations in error paths properly cleaned up
- Direct value construction without builder pattern overhead
- Metadata-first approach with self-contained buffers

### Documentation

- 40+ code examples across documentation
- 12 functions fully documented with signatures and examples
- 10 design patterns explained with implementation details
- Complete memory allocation lifecycle tracking
- Test coverage mapping for 51+ test scenarios
- Future improvements roadmap

### Known Limitations

- No semantic validation (type checking, inheritance resolution)
- No error recovery (stops on first error)
- No string materialization (values are position/length spans)
- No streaming parser (full source must be buffered)
- No bytecode compilation or execution

### Test Results

- test_messaging.c: 17/17 passing
  - 5 valid AMP scalar tests passing
  - 5 AMP rejection tests passing (object, array, tuple, attribute, inheritance)
  - 2 AML dialect tests passing (allows objects and inheritance)
  - 5 envelope and metadata tests passing
- Memory: 138 mallocs, 138 frees (zero leaks)
- Build: All warnings treated as errors (strict compilation)

### Breaking Changes

None - this is the initial release.

### Dependencies

- C23 (ISO/IEC 9899:2023)
- **SigmaTest** framework 
- **SigmaCore** library

### Files Changed

Core Implementation:
- src/core/parser.c: Fixed dispose_statement() to actually free memory
- include/anvil.h: Added version identifier

Documentation:
- docs/PARSER_ARCHITECTURE.md: Updated with 4 leak fixes and production status
- docs/PARSER_QUICK_REF.md: Updated version and test counts
- docs/TEST_COVERAGE_MAP.md: Updated version identifier
- docs/PARSER_DOCUMENTATION_INDEX.md: Updated production-ready status

### Contributors

- BadKraft (Author, Parser Implementation, Memory Audit, Documentation)

### Acknowledgments

- sigtest framework for memory tracking and testing infrastructure
- sigcore library for system utilities

---

## Future Releases

*(The "Planned for v0.2.0" list that used to live here was from long before the current
module-context rebuild and no longer reflects reality — removed rather than left misleading.
Current direction:)*

- **Schema** (`@[schema]`) — active design/implementation effort; see `notes/native-schema.md`
  and `docs/schema/README.md`.
- **AnvilScript (ASL)** — design stage; see `notes/anvilscript-design.md`.
- A real, packaged library build (static and shared), enabling schema/AnvilScript as genuinely
  separate, linkable add-ons.

---

**For more, see:**
- [`README.md`](../README.md) — project overview and current status
- [`docs/language-reference.md`](language-reference.md) — the full language reference
- [`docs/getting-started.md`](getting-started.md) — practical C API walkthrough
- [`docs/types-reference.md`](types-reference.md) — the opt-in type system
- [`notes/`](../notes/) — design-thread records for active work
