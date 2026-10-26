# FR-2610-anvl-bindings-002: `AnvilWriter` / `AnvilBuilder` in `anvil.node` (parity with anvil.py and anvil.net)

**ID:** FR-2610-anvl-bindings-002
**Type:** Feature Request (bindings)
**Owner:** anvil-node, then anvil-wasm (second slice, added 2026-10-25)
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** flywire, rivet (downstream consumers)
**Status:** open — not started
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
