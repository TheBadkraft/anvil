# SR-2610-anvl-vendor-bump-002: re-bump every binding to today's native `main`, drop the workarounds, and add the decoded attribute accessor

**ID:** SR-2610-anvl-vendor-bump-002
**Type:** Service Request (dependency maintenance + small binding surface)
**Owner:** ANVL Bindings agent (anvil.node, anvil.wasm, anvil.net, anvil.py, anvil.java)
**Filed:** 2026-10-26
**Filed by:** anvil (planning)
**Requested by:** rivet, flywire (downstream consumers)
**Status:** resolved — implemented in all five bindings; see Resolution
**Blocks:** SR-2610-anvl-distribution-001 (the v0.10.0-rc release ships this pin), and Rivet's importer-workaround retirement
**Continues:** SR-2610-anvl-vendor-bump-001
**Tags:** bindings, vendor, rivet-readiness

---

## Summary

The bindings pin `vendor/anvil` at `f9ee783`. Native `main` (`8059560`, 2026-10-26) now carries
work none of them has:

- thread-safe, independent loads (BR-2610-anvl-registry-001)
- NUL-safe registry and schema strings (BR-2610-anvl-nul-001)
- decoded attribute values, `anvil_attribute_get_value_handle` (FR-2610-anvl-public-api-004)
- the `nullable`/`truely` bare-word fix (BR-2610-anvl-parser-001)
- real error messages and positions (BR-2610-anvl-errors-001)
- unterminated-comment errors (BR-2610-anvl-parser-002)

## Scope, per binding

1. **Bump `vendor/anvil` to native `main`.** anvil.java has no pin; it rebuilds against the new
   `libanvil.so`. Rebuild and run the full suite. anvil.wasm rebuilds on the Linode box; check that
   it still builds under emcc with `_Thread_local`, which the coder already verified once.
2. **Vtable and enum drift.** Add the appended `Attribute.get_value_handle` member to every binding
   that mirrors the `Attribute` vtable. The drift tests must catch it first (RED).
3. **Drop the identical-content workarounds:** anvil.net's xUnit `DisableTestParallelization`, the
   special exception text in `AnvilSchema.ValidateField` (.NET) and `validate_field` (Python), and
   anything else that existed only because of the global registry. Keep a test proving that two live
   documents with identical text both load.
4. **Flip the NUL todo/skipped tests** for registry and schema labels and extras to real tests.
5. **One decoded attribute accessor in all four bindings that expose attributes** (node, wasm, py,
   net; java too if it exposes attributes). It reads through `get_value_handle`, gives the decoded
   text (`status="done"` → `done`) and a typed form where the binding has one. `value` stays raw
   everywhere. Use the same name in each binding's idiom, and record the names here.
6. **Error-message regression:** a missing `;` now reads "Expected ';' to end the statement" through
   each binding's error surface (it used to read "Unknown error").

## Rules

- Do binding work in the worktrees. Park the shared checkouts on the new green `main` tips only
  when done, and report the commits.
- TDD, changelogs per binding and in anvil, and a Resolution section here.
- No push. Planning merges on the owner's go, then notifies FlyWire and Rivet with the anvil.node
  commit.

## Test cases

1. Each binding's suite is green on the new pin; vtable/enum drift tests pass with the new member.
2. Two live identical-text documents both load (every binding with a document type).
3. The decoded accessor gives `done` for `@[status="done"]`, `a"b` for `@[note="a\"b"]`, a number
   for `@[n=42]`, and null/None for a flag; `value` is unchanged (raw).
4. The NUL label and extra tests pass.
5. `x := nullable;` loads as a bare string through every binding.

## Resolution

Bindings agent, 2026-10-26. Every binding was bumped to native `e089837` (`v0.8.0-rc-176`; `anvil.java` has no pin and was
built and tested against it), worked in git worktrees under `anvil.worktrees/`, TDD, no push. Each binding's branch is
`chore/SR-2610-anvl-vendor-bump-002`.

| Binding | Commit | Tests | Notes |
|---|---|---|---|
| `anvil.node` | `fd7ee74` (merged) | 200, none `todo` | Valgrind: 0 errors, 0 bytes lost. Still exports only its two N-API entry points. |
| `anvil.wasm` | `8ee0623` | 213, none `todo` | Rebuilt on the Linode box under `emcc`. |
| `anvil.py` | `41834d1` | 178 | `Attribute` struct gained `get_value_handle`. |
| `anvil.net` | `c3d0dca` | 157 | `DisableTestParallelization` dropped; 10 runs in a row, all green. |
| `anvil.java` | `5d778a4` | 104 (was 84) | New `VtableDriftTests`; it found `Document` one member short. |

### The decoded accessor (the names, as the scope asked)

`value` stays the raw source text everywhere (a quoted string keeps its quotes). The decoded read, in each binding's idiom:

| Binding | Accessor | Type | Flag | Explicit `@[x=null]` |
|---|---|---|---|---|
| `anvil.node`, `anvil.wasm` | `attribute.decodedValue` (a property of the frozen `{ key, value, decodedValue }`, statement and module level) | a JS string, number, boolean or `null` | `null` (and `value` is `null`) | `null`, so **`value` tells them apart**: `null` for the flag, `'null'` for the explicit one |
| `anvil.py` | `AnvilAttribute.decoded_value` | an `AnvlValue`, so `.type`, `.as_string()`, `.as_int()`, `.as_bool()`, `.as_float()` | `None` | a `NULL`-typed `AnvlValue` |
| `anvil.net` | `AnvilAttribute.DecodedValue` | an `AnvlValue` (`Type`, `AsString()`, `AsInt()`, ...) | `null` | a `Null`-typed `AnvlValue` |
| `anvil.java` | `AnvilAttribute.getDecodedValue()` | an `AnvlValue` (`getType()`, `asString()`, `asInt()`, ...) | `null` | a `NULL`-typed `AnvlValue` |

The three lazy bindings can return a real value view, so an explicit null is distinguishable from a flag by the result itself. The
two JSON-envelope bindings cannot (a JS `null` is both), and use `value` for that. In `anvil.node`/`anvil.wasm` the decoded value is
carried as a third element of each attribute pair in the existing envelope, only for an attribute that has a value, so an
attribute-free document is still byte-identical. A decoded number past 2^53 loses precision there, as in `parseRawValue()`.

### Test cases

1. Every suite is green on the new pin, and the drift tests pass with the new member: `test_vtable_drift.py` (py), `VtableDriftTests`
   (net, which already existed, and java, which is new). They failed first, on `Attribute`, as required. `anvil.node` and
   `anvil.wasm` bind attributes through the flat API and have no vtable mirror, so they have no drift test for it.
2. Two live identical-text documents both load, in every binding with a document type (py, net, java), plus 64 concurrent identical
   loads in `anvil.net`. `anvil.node` and `anvil.wasm` have no live document type; there it is two live `Schema.load` handles on
   identical text and back-to-back identical `parse()` calls.
3. The decoded accessor: `status="done"` is `done`, `note="a\"b"` is `a"b`, `n=42` is a number, a flag is null/None, and `value` is
   unchanged, in all five. Also tested: bare words, booleans, explicit null, `range=1..5` (a non-scalar, which native exposes as a bare
   value of its raw text), module attributes, NUL bytes and non-ASCII.
4. The NUL tests are real tests now (the `todo`/`xfail`/skip is gone in node, wasm, py and net), and each also covers the schema's
   own enum labels.
5. `x := nullable;` loads as a bare string in all five, along with `truely` and `nullify`.
6. The missing `;` reads "Expected ';' to end the statement" at 2:7 through every binding's error surface (`lastError()`,
   `doc.error`, `doc.getError()`), and through `validate_field`/`ValidateField`'s parse error. Also pinned: an unterminated block comment
   is an error.

### Workarounds dropped

`anvil.net`: the xUnit `DisableTestParallelization` switch (`AssemblyInfo.cs` deleted) and the special `FormatException` text in
`ValidateField`. `anvil.py`: the special `ValueError` text in `validate_field` and its `ErrorCode` import. `anvil.java` never had one.
The binding documentation that explained the old registry rule was corrected. In `anvil.node`/`anvil.wasm`, `parse()` still converts
eagerly and never holds a native document; that design was first motivated by the global registry but is kept for the single-crossing
JSON performance reason (BR-2609-anvl-004), so I did not change it.

### Findings

- **`anvil.java` had a drift it could not see.** It reads each vtable group with a hard-coded member count and had no drift gate, so
  `Document` was declared with 8 members while native has 9 (`get_path`, BR-2610-anvl-vtable-001). The new `VtableDriftTests` compares
  every group's declared count with the real header and found it. `Document` is now read with 9 (the member is still not exposed).
- **`anvil.wasm` builds under `emcc` with native's `_Thread_local` parser state and no warnings.** The module is single-threaded, so it
  is a plain static.
- **`e089837` is not on GitHub**, so `git submodule update` fails against the URL. The pin was fetched into each submodule from the local
  `~/Projects/repos/anvil`, which also means the recorded submodule commit exists only locally until anvil is pushed.
- **The local `.nupkg` is stale.** `Anvil.Net.0.10.0-dev.1` in `src/Anvil.Net/bin/Release` still contains the `f9ee783` library. A repack
  needs a new version (for example `0.10.0-dev.2`), because `~/.nuget/packages` caches by version. Not done; the Release agent builds from
  the tag.
- **BR-2610-anvl-attr-001** (planning's note): attribute values including trailing whitespace, and `decodedValue` keeping the quotes when a
  newline follows, is a native fix. No binding change is needed; the bindings only read what native returns.

