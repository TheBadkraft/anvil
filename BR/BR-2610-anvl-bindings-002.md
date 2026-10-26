# BR-2610-anvl-bindings-002: `anvil.wasm` hands text to the module NUL-terminated, so a document containing a NUL is silently cut at it

**ID:** BR-2610-anvl-bindings-002
**Type:** Bug Report (binding, silent data loss)
**Owner:** anvil-wasm (ANVL Bindings agent)
**Filed:** 2026-10-26
**Filed by:** ANVL Bindings agent, found porting `anvil-node`'s writer tests to `anvil.wasm`
**Status:** open
**Blocks:** nothing today
**Tags:** bindings, anvil-wasm, parse, silent-failure, nul

---

## Summary

Every text argument `anvil.wasm` passes to the module (`parse`, `parseRawValue`, and now
`Schema.validate`/`validateValue`/`validateField` and the loaders) is copied onto the WASM heap by
`withHeapString` as a NUL-terminated C string, and the native entry points read it with `strlen`.
A NUL character in the text therefore ends it early, with no error:

```
parse('#!aml\ns := "x";\u0000t := 1;\n')   // -> an AnvlNode with one statement, s; t is gone; lastError() is null
```

The document is accepted as a shorter, valid one. Whether the cut also leaves the document
syntactically valid depends on where the NUL falls: cut inside a quoted string it is an error
("unterminated"), cut between statements it is a silently truncated document.

`anvil.node` is not affected: its `Parse` passes the length N-API reports
(`napi_get_value_string_utf8`), and `anvil_load_buffer(text, length)` reads exactly that many bytes.

## Why it matters

A writer-produced document can legitimately contain a NUL (`AnvilWriter.string('a\0b')` writes it
exactly, because the writer is length-delimited), so a document that round-trips through
`anvil-node` loses data through `anvil-wasm`, and the loss is silent. The same hole is now in the
schema entry points (`validateField(text, ...)` would validate only the part before the NUL).

## Expected behavior

The module receives the text's length and reads exactly that many bytes, so a NUL is just data, as in
`anvil-node`. Concretely: the `anvil_wasm_*` text entry points take a length (`anvil_load_buffer`
and `anvil_parse_value_fragment` already do natively), and `lib/index.js` passes
`lengthBytesUTF8(text)`.

## Test cases

1. `parse('#!aml\ns := "x";\0t := 1;\n')` has both statements `s` and `t`.
2. A string value written with `AnvilWriter.string('a\0b')` reads back as `'a\0b'` through `parse`.
3. The same for `parseRawValue`, `Schema.validate`, `validateValue` and `validateField`.
4. Names and paths (NUL-terminated by design) still reject an embedded NUL with a `RangeError`.
5. The large-input regression (BR-2609-anvl-005) still passes through 5 MB; the existing suite is green.

## Notes

- This changes the C signature of the `anvil_wasm_*` text functions, which are internal to the
  binding (only `lib/index.js` calls them), so it is not a public-API change.
- Until this is fixed, `test/writer.test.js` in `anvil.wasm` asserts only that the writer emits the
  NUL byte exactly, and says so.
