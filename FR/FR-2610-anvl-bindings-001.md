# FR-2610-anvl-bindings-001: expose the type registry and AnvilSchema through the bindings — `anvil.node` first

**ID:** FR-2610-anvl-bindings-001
**Type:** Feature Request (bindings)
**Owner:** anvil-node (first), then anvil-py and anvil-net (same surface, follow-on slices)
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet, flywire (downstream consumers)
**Status:** resolved — every slice done (1, 1b `anvil.node`; 2 `anvil.py`; 3 `anvil.net`; 4 `anvil.wasm`); see Progress
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

## Progress

### Slice 1: `anvil.node` — done (Bindings agent, 2026-10-25)

Branch `feat/FR-2610-anvl-bindings-001-node` in `anvil.node`, commit `45c2b8e`, on top of the vendor
bump branch (`chore/SR-2610-anvl-vendor-bump-001`, pin `f9ee783`). Not yet merged or pushed. 70/70
tests (30 new in `test/types-schema.test.js`), Valgrind-clean on the new code (0 errors, 0 bytes
definitely or indirectly lost), exports still only the two N-API entry points.

Test cases 1–5 are covered. Case 7 is deferred, see below.

- **Surface as built:** `TypeRegistry` (`load`, `loadFile`, `fromIncludes`, `fromIncludesFile`,
  `find`, `resolve`, `diagnostics`, `loadError`, `dispose`), `TypeDef`, `Schema` (`load`, `loadFile`,
  `validate`, `validateValue`, `diagnostics`, `dispose`), frozen enums `TypeKind`, `TypeDiagnostic`,
  `SchemaViolation`, `SchemaDiagnostic`, each pinned against the vendored headers by a test.
- **Input is ANVL text.** ANVL is not JSON, and a wire body is already ANVL, so `validateValue`
  takes the object value as text (`'{ a := 1; }'`). Producing ANVL text from a JS value is
  FR-2610-anvl-bindings-002.
- **Deviation: no `FinalizationRegistry`.** Handles are N-API externals with a finalizer, which is
  the same safety net without a JS-side registry. Node runs finalizers on a later event-loop
  turn, so a synchronous loop that abandons handles should call `dispose()`.
- **Addition: `registry.loadError`.** `fromIncludes` must not reject a consumer document that
  loaded with an error, because a name defined by two included `@[types]` documents collides in
  the consumer's namespace and `NAME_COLLISION` is how Anvil Native reports it (native test TYP13).
  The document's error is kept on the registry rather than discarded.
- **Includes:** the text loaders resolve `include`s against the process working directory, the file
  loaders against the file. Schemas that include `@[types]` documents should use `loadFile`.
- **Deferred to slice 1b (see Decisions):** `schema.fields` introspection (FR-2610-anvl-schema-005
  has landed natively); a `min`/`max` that overflows `int64`/`double` is reported as `undefined`
  because native has no text accessor for it (candidate native FR if Rivet needs it).
- **Deferred to the release (SR-2610-anvl-distribution-001, Release agent):** the `Bindings-Guide.md` Node
  section, because it documents the published `0.8.0-rc` download as "Downloadable today" and
  that artifact does not contain this surface. The README in `anvil.node` documents it meanwhile.
- **Not started:** slice 2 (`anvil.py`) and slice 4 (`anvil.wasm`). Slice 3 (`anvil.net`) is done, below.

### Slice 3: `anvil.net` — done (Bindings agent, 2026-10-26)

Done ahead of 1b, 2 and 4 on the planning priority change. Branch `feat/FR-2610-anvl-bindings-001-net`
in `anvil.net`, commit `ae66547`, off `chore/SR-2610-anvl-vendor-bump-001` (pin `f9ee783`). Not yet
merged or pushed. 137/137 tests (51 new). Strict TDD: the suite was built and RED first (48 of the
new tests failing on assertions), then taken to GREEN.

- **Surface**, all `IDisposable` where it owns a handle, `Anvil`-prefixed like the rest of the binding:
  `AnvilTypeRegistry` (`Load`, `FromIncludes`, `Find`, `Resolve`, `Diagnostics`), `AnvilTypeDef` (`Kind`,
  `TryGetSize`/`TryGetMin`/`TryGetMax(out long)`, `Min`/`Max` as `AnvilNumeric?`, `Format`, `Values`),
  `AnvilSchema` (`Load`, `Validate`, `ValidateValue`, `ValidateField` over text or a document,
  `Diagnostics`, `Fields`, `FindField`), `AnvilSchemaField` (`Name`, `Kind`, `TypeName`, `IsRequired`,
  `IsNullable`, `IsInteger`, `TryGetSize`, `Min`/`Max`, `TryGetMinItems`/`TryGetMaxItems`, `Values`,
  `Format`, `Tag`, `Nested`/`FindNested`, `Items`, `Extras`), `AnvilNumeric`, and the snapshots
  `AnvilSchemaValidation`, `AnvilSchemaViolation`, `AnvilSchemaDiagnostic`, `AnvilTypeDiagnostic`.
- **Native layer:** `TypeRegistryNative` and `SchemaNative`, 1:1 mirrors of the 20- and 40-member
  vtables, generated from the headers. `VtableDriftTests` pins them (`TypeRegistryRaw`, `SchemaRaw`),
  and the new `EnumDriftTests` pins `AnvilTypeKind` (incl. `Blob`), `AnvilTypeDiagnosticCode`,
  `AnvilSchemaViolationCode` (incl. `Tag`, `Format`) and `AnvilSchemaDiagnosticCode` value for value.
- **Choices to confirm** (each was a judgment call, recorded so planning can overrule):
  1. `ValidateField` for an **absent statement** returns one `Required` violation with an empty
     `Field` and a message naming the statement. The decision said "one violation with an empty path"
     without a code; `Required` is the closest native category. `anvil.node`'s slice 1b should use the
     same so the two bindings agree.
  2. `ValidateField` on **text that doesn't parse** throws `FormatException` (carrying the native
     message, line and column). `anvil.node` returns `null` and sets `lastError()`, which is that
     binding's convention; this is .NET's.
  3. `ValidateValue` takes a live `AnvlValue`, so a body already loaded needs no text round trip. The
     text form of `ValidateField` exists for the shared convenience.
  4. A batch (`set := [ {...}, ... ]`) is validated by the caller iterating the array and calling
     `ValidateValue` per element, which the .NET lazy model makes cheap. No `{ each: true }` form.
  5. Views (`AnvilTypeDef`, `AnvilSchemaField`) check their owner on every access and throw
     `ObjectDisposedException` after it is disposed, which is stricter than `AnvlValue`'s documented
     "invalid after dispose". No finalizer, matching `AnvilDocument`.
- **Hazard found, worth knowing in every binding:** Anvil Native rejects loading content identical to a
  document that is **still loaded**, reporting a bare `Io` error with an empty message. It surfaces in
  `ValidateField(text, ...)` when the caller holds a document with the same text. The exception now
  says so and points at the `ValidateField(AnvilDocument, ...)` overload.
- **Verification:** 30,000 load/validate/introspect/dispose rounds grow no faster than a document-only
  control (14 MB vs 20 MB working set), so no leak. The package was also consumed from the local feed
  as a downstream project would, and worked end to end.
- **Local package:** `dotnet pack -c Release` produces `src/Anvil.Net/bin/Release/Anvil.Net.0.10.0-dev.1.nupkg`
  (`libanvil.so` inside is byte-identical to the `f9ee783` build and exports the `Schema` and
  `TypeRegistry` vtables). **Consumers must reference `0.10.0-dev.1`**, not `0.8.0-rc`, whose copy in
  `~/.nuget/packages` is cached. The version is provisional and local-only, below the eventual
  `0.10.0-rc` (release decision R1).
- **Deferred:** the `Bindings-Guide.md` .NET section, to the release (Release agent hands it to the
  Documenter), since it documents the published `0.8.0-rc` download.

### Slice 1b: `anvil.node` — done (Bindings agent, 2026-10-26)

Branch `feat/FR-2610-anvl-bindings-001-node-1b` in `anvil.node`, commit `702d26a`, stacked on the
writer branch. Not yet merged or pushed. 130/130 tests (23 new in `test/schema-field.test.js`). TDD:
RED first (21 of 23 failing on assertions against inert stubs), then GREEN. Exports are still only
the two N-API entry points; Valgrind shows 0 definite or indirect leaks, and the only errors are
V8's own GC stack scanner (none has an `anvil_node` frame).

- **`schema.validateField(docText, name, { skipRequired, allowUnknown, each })`:** parses the text,
  finds the top-level statement, runs `anvil_schema_validate_value` on its value, in one native call.
  Result `{ valid, violations }`, or `null` plus `lastError()` if the text does not parse. An absent
  statement is one `REQUIRED` violation with an empty `field` and a message naming the statement,
  the same as `anvil.net` (owner-confirmed). A statement that is not an object is the native
  `TYPE_MISMATCH`.
- **Decision recorded: the batch form is needed in Node.** Applying the planning test (can the caller
  get a per-element handle?): no, because `parse()` converts eagerly and frees the native document,
  so a Node caller cannot iterate a batch itself. `each: true` therefore validates every element of
  an array or tuple with the same options and reports violations at `[i]` and `[i].field`. A single
  object still validates under `each`, and a non-object element is `TYPE_MISMATCH` at `[i]`. An
  empty batch is valid. `anvil.net` needs no such form (its caller iterates).
- **`schema.fields` / `schema.findField(name)`:** `SchemaField` snapshots (readable after
  `dispose()`) with `name`, `kind`, `typeName`, `required`, `nullable`, `integer`, and when declared
  `size`, `min`, `max`, `minItems`, `maxItems`, `values`, `format`, `tag`; plus `nested`/`findNested`,
  `items`, and `extras` (each `{ name, value }`, `value` an `AnvlNode` so array/tuple and blob types
  survive). Undeclared constraints are `undefined`. Tests use native's own fixtures, tracking SCH76-SCH79.
- **Known limits:** `min`/`max` are JS numbers, so a bound past 2^53 loses precision; one that
  overflows `int64`/`double` is `undefined`. Both were already true of `TypeDef` in slice 1.
- **No workaround for BR-2610-anvl-registry-001**, per planning. Node disposes every document inside
  the call that loaded it, so it does not hit the duplicate-content rejection in normal use.
- **Next:** slice 2 (`anvil.py`) and slice 4 (`anvil.wasm`), and FR-2610-anvl-bindings-002's
  `anvil.wasm` slice, which should reuse this exact surface and result shapes.

### Slice 4: `anvil.wasm` — done (Bindings agent, 2026-10-26)

Branch `feat/FR-2610-anvl-bindings-001-wasm` in `anvil.wasm`, commit `b8829ba`, on `fix/BR-2610-anvl-bindings-001`
(which is on the vendor-bump branch, pin `f9ee783`). Not yet merged or pushed. 123/123 tests (60 new). TDD: stubs
and the full test set first (50 failing), then the C, the `emcc` build on the Linode box, and the JS.

- **Surface and result shapes are `anvil.node`'s** (the tests are derived from `anvil.node`'s by script, so they
  assert the same thing): `TypeRegistry`, `Schema` with `validate`, `validateValue`, `validateField` and its `each`
  form, `diagnostics`, `fields`, `findField`, `SchemaField`, and the four frozen enums.
- **Twelve native entry points** are exported from `src/binding.c` (`anvil_wasm_registry_*`, `anvil_wasm_schema_*`),
  so the linker no longer drops the types and schema code: the `.wasm` grows from 87 KB to 131 KB.
- **Differences forced by the runtime**, all documented in the `anvil.wasm` README:
  - Includes and `loadFile` need a filesystem, and the module has none. The module's in-memory filesystem is
    exported (`FS`, `FORCE_FILESYSTEM`) and `writeFile(path, textOrBytes)`/`removeFile(path)` fill it. File loaders
    resolve includes beside the file; text loaders against `/`.
  - Handles are WASM heap memory, which nothing reclaims, so `dispose()` frees one and a `FinalizationRegistry`
    frees an abandoned one (this is the mechanism the FR originally named; `anvil.node` uses an N-API finalizer).
    Measured: abandoned handles with GC plateau at the same ~2 MB per 30,000 rounds as explicit disposal; without
    GC they grow (38 MB), which is why the docs say to call `dispose()` in loops.
- **`writeFile(path, textOrBytes)` and `removeFile(path)` are a deliberate wasm-only addition** (planning-accepted, 2026-10-26),
  beyond "same surface as `anvil.node`". The module has no host filesystem, so a schema that includes `@[types]`
  documents, and `loadFile`/`fromIncludesFile`, have nothing to read unless the caller can put files into the module's
  in-memory one. `anvil.node` and `anvil.net` do not need them and do not have them.
- **The decision's web-path smoke test is `test/web-path.test.js`.** It evaluates `dist/anvil.js` in a sandbox with no
  `process`, so Emscripten takes its `ENVIRONMENT=web` branch and fetches the `.wasm` over `fetch`, then validates a
  batch and loads a schema from the in-memory filesystem. It is not a real browser (no DOM, no real network), but it
  is the same code path, and it is the only test that runs that branch.
- **Known limits** shared with `anvil.node`: `min`/`max` are JS numbers. A limit specific to `anvil.wasm`: text is passed
  NUL-terminated, see **BR-2610-anvl-bindings-002**.
- **`dist/` is not committed.** Anyone checking out the branch must rebuild it (`build.sh` on the Linode box, with
  `build-sigma-system-alloc.sh` run once) before the tests can pass; `test/vendor-pin.test.js` fails a stale `dist/`.

### Slice 2: `anvil.py` — done (Bindings agent, 2026-10-26)

Branch `feat/FR-2610-anvl-bindings-001-py` in `anvil.py`, commit `0e9a93c`, on `fix/BR-2610-anvl-bindings-002` (which is on
the vendor-bump branch, pin `f9ee783`). Not yet merged or pushed. 164 tests pass and 1 is an expected failure (57 new).
TDD: stubs and the full test set first (55 failing), then the ctypes layer, then the public classes. Built after, and
paused for, BR-2610-anvl-bindings-002 as planning directed, so the NUL tests for the registry and schema were in the
test set from the start.

- **Surface, in Python idiom** (same members as `anvil.net`'s slice, with Python's conventions): `AnvilTypeRegistry`
  (`load`, `from_includes`, `find`, `resolve`, `diagnostics`), `TypeDef`, `AnvilSchema` (`load`, `validate`,
  `validate_value`, `validate_field`, `diagnostics`, `fields`, `find_field`), `SchemaField`, `AnvilNumeric`, the snapshot
  dataclasses `SchemaValidation`, `SchemaViolation`, `SchemaDiagnostic`, `TypeDiagnostic`, `SchemaExtra`, and the four
  enums. Everything that owns a handle is a context manager; views raise `RuntimeError` after their owner is disposed,
  matching `AnvilDocument`'s existing convention. No finalizer, matching `AnvilDocument`.
- **Python-idiom choices** (confirm or overrule): options are **keyword-only** (`skip_required=`, `allow_unknown=`);
  `validate_field` is **one method** taking text, bytes or an `AnvilDocument` (where `anvil.net` has two overloads);
  absent constraints are **`None`** (as the FR specified); `min`/`max` are an `int` when integral and a `float` when
  fractional, with `min_numeric`/`max_numeric` giving the exact `AnvilNumeric` (so the `TryGet*(out long)` forms of
  `anvil.net` have no Python counterpart: an `int` is unbounded). A text-parse failure is a `ValueError`, the analogue
  of `anvil.net`'s `FormatException`. An absent statement is one `REQUIRED` violation with an empty field, as in the
  other bindings.
- **Native layer:** `_native/type_registry.py` and `_native/schema.py`, 1:1 mirrors of the 20- and 40-member vtables,
  generated from the headers and reviewed; `tests/test_vtable_drift.py` pins them, `tests/test_enum_drift.py` pins the enums.
- **The identical-content hazard** (BR-2610-anvl-registry-001) is handled as in `anvil.net`: `validate_field(text)` raises a
  `ValueError` that names the cause and points at the document form. Drop it when that BR lands.
- **Verification:** 30,000 full rounds (load, introspect, validate, `validate_field`, registry) grow RSS by 0.1 MB, the same
  as a document-only control.
- **Deferred:** the `Bindings-Guide.md` Python section, to the release (Release agent, then Documenter).

