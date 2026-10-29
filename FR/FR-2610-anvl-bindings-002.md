# FR-2610-anvl-bindings-002: `AnvilWriter` / `AnvilBuilder` in `anvil.node` (parity with anvil.py and anvil.net)

**ID:** FR-2610-anvl-bindings-002
**Type:** Feature Request (bindings)
**Owner:** anvil-node, then anvil-wasm (second slice, added 2026-10-25)
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** flywire, rivet (downstream consumers)
**Status:** resolved — `anvil.node` and `anvil.wasm` slices done (see Resolution)
**Tags:** bindings, anvil-node, writer, builder, amp, rivet-readiness

---

## Summary

`anvil.py` and `anvil.net` both ship the streaming writer and document builder (Bindings-Guide
".NET / Python — Writing"). `anvil.node` doesn't, so Node consumers emit AML/AMP by string
concatenation. FlyWire's `src/am-packet.js` and `harness/server.js` do this, joining
`'#!amp'` and `name := value;` lines by hand.

## Motivation

Hand-built ANVL text is where quoting and escaping bugs live: a title containing `"`, a body
containing a backtick inside a blob. Rivet will emit AMP responses carrying user-entered text.
The native writer already produces canonical, dialect-checked output (AMP's forbidden constructs
are rejected at write time), and Node is the one binding without it.

## Design

Mirror the Python/.NET surface in JavaScript idiom:

- `AnvilWriter.create(AnvilDialect.AMP | AnvilDialect.AML)`, chainable `.statement(name)`,
  `.string()/.number()/.bool()/.null()/.blob(bytes, tag?)`, `.beginArray()/.endArray()`,
  `.beginTuple()/.endTuple()`, `.beginObject()/.endObject()`, `.finish()` → string and
  `.finishBytes()` → Buffer.
- `AnvilBuilder.create(dialect)` with detached-node construction and `.emit()/.emitBytes()`.
- Errors surface as `AnvilWriterError` with `.code` (sticky-first-error semantics for the writer,
  non-sticky for the builder, exactly as in the native layer).

## Test cases

1. AMP writer: `status := ok; code := 200; tags := [a, b];` emitted, then reparsed by `anvil.node`,
   yields equal values.
2. AMP writer: `beginObject()` raises `AnvilWriterError` with the AMP-object-forbidden code.
3. A string containing `"` and `\` round-trips exactly.
4. A blob containing arbitrary bytes, including a backtick, round-trips through `finishBytes()` and
   back through the loader. If the native writer can't represent this, the test documents the
   failure and a native BR is filed.
5. Builder: build, emit, reparse, compare.
6. Existing anvil.node suite green.

## Dependencies

None. The native writer (`anvil_writer.h`, `anvil_builder.h`) is already implemented.

## Decisions (planning, 2026-10-25, owner-approved)

- **Scope extends to `anvil.wasm`** as a second slice, after the `anvil.node` slice: the same
  `AnvilWriter`/`AnvilBuilder` surface and the same names, so a JS caller gets one API on server
  and browser. The wasm build already compiles the writer in (SR-2610-anvl-vendor-bump-001); the
  slice exports its entry points and wraps them.
- **The release waits for both slices** (SR-2610-anvl-distribution-001). `anvil.node` without the
  writer would lag `anvil.py`/`anvil.net`, and the writer is how a JS caller produces the ANVL
  text that `Schema.validateValue` takes.

## Resolution (`anvil.node` slice)

Bindings agent, 2026-10-25. Branch `feat/FR-2610-anvl-bindings-002` in `anvil.node`, commit
`fef915b`, stacked on `feat/FR-2610-anvl-bindings-001-node` and the vendor-bump branch. Not yet
merged or pushed. 107/107 tests (36 new in `test/writer.test.js`), exports still only the two
N-API entry points.

- **Surface as built:** `AnvilWriter` and `AnvilBuilder` (with `AnvilNode` and `AnvilMember`),
  `AnvilWriterError` with `.code`, and frozen `AnvilDialect` and `WriterErrorCode`, the latter two
  pinned against `anvil_writer_types.h`. Same members as `anvil.py`/`anvil.net`, in JS idiom
  (`beginArray()`, camelCase, `value()` taking `null`/boolean/number/`bigint`).
- **Test cases:** 1 (AMP message, canonical text, reparsed), 2 (AMP `beginObject()` raises `DIALECT`,
  and is sticky), 3 (a string with `"`, `\`, newline, NUL and non-ASCII round-trips exactly), 5
  (builder build/emit/reparse/re-emit) and 6 (suite green) are covered as written.
- **Test case 4, partly.** The writer emits blob bytes **exactly** (asserted byte-for-byte against
  `finishBytes()`), and UTF-8 blob content round-trips through the loader. Two things the FR
  anticipated do not hold, and neither is a native-writer defect, so no native BR:
  - A backtick in blob content is rejected with `INVALID_BLOB`. This is documented native behavior
    (`anvil_writer_types.h`): a blob has no escape for its closing delimiter. The test pins it.
  - Non-UTF-8 bytes cannot be read back by `anvil.node`. The loader takes a JS string, which cannot
    carry them, so a binary blob comes back lossy. It needs a bytes-capable loader (new surface).
    Checking this surfaced a silent-failure defect in `parse()`: **BR-2610-anvl-bindings-001**.
- **Native writer facts worth knowing**, found by testing against the canonical output (and
  cross-checked against `anvil.py`): a tagged blob is written `@tag` + backticked content; the
  numeric grammar requires the exponent's sign (`1.5e+3` is valid, `1.5e3` is `INVALID_NUMERIC`),
  although the reader accepts `1e3`; attributes on one statement are grouped `@[a, b=c]`.
- **No `FinalizationRegistry`** (same as FR-2610-anvl-bindings-001): handles are N-API externals with
  a finalizer. Valgrind shows 0 definite leaks once finalizers have run; the residue before that is
  Node's own bookkeeping for handles not yet collected. RSS is flat against the `parse()` control.
- **Deferred:** the `Bindings-Guide.md` Node section (same reason as FR-2610-anvl-bindings-001:
  it documents the published `0.8.0-rc` download); a bytes-capable loader and `asBytes()`; the
  FlyWire call sites (`src/am-packet.js`, `harness/server.js`) still concatenate by hand, and
  switching them over is FlyWire's change to make.

## Resolution (`anvil.wasm` slice)

Bindings agent, 2026-10-26. Branch `feat/FR-2610-anvl-bindings-002-wasm` in `anvil.wasm`, commit `8e72de9`, on the
slice-4 branch above it. Not yet merged or pushed. 160/160 tests (37 new in `test/writer.test.js`, derived from
`anvil.node`'s by script). TDD: stubs and the full test set first (36 failing), then the C, the `emcc` build, and the JS.

- **Same surface as `anvil.node`:** `AnvilWriter`, `AnvilBuilder`, `AnvilNode`, `AnvilMember`, `AnvilWriterError`, and the
  frozen `AnvilDialect` and `WriterErrorCode`, the latter two pinned against `anvil_writer_types.h`.
- **Twenty-five native entry points** in the new `src/writer_binding.c`, so the writer is no longer dropped by the
  linker (`.wasm` ~150 KB). Node and member pointers cross as plain numbers.
- **Differences forced by the runtime** (README): output bytes are a `Uint8Array`, not a `Buffer`; integers are written
  as exact decimal text through the numeric entry point, so no 64-bit value crosses the JS/WASM boundary and a `bigint`
  of any size is exact; `build.sh` also exports `HEAPU8`.
- **A finding, filed as BR-2610-anvl-bindings-002:** `anvil.wasm`'s `parse()` hands text to the module NUL-terminated,
  so a document containing a NUL is cut at it, silently. The writer emits a NUL exactly (it is length-delimited), so the
  FR's string round-trip test could not include a NUL in wasm; the test asserts the writer's bytes instead and cites the BR.
- **Deferred:** the `Bindings-Guide.md` `anvil.wasm` section, to the release (Release agent, then Documenter).

