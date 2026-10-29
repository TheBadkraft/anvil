# BR-2610-anvl-bindings-002: a document containing a NUL character is silently cut at it (`anvil.wasm`, and the audit that followed)

**ID:** BR-2610-anvl-bindings-002
**Type:** Bug Report (binding, silent data loss)
**Owner:** anvil-wasm (ANVL Bindings agent)
**Filed:** 2026-10-26
**Filed by:** ANVL Bindings agent, found porting `anvil-node`'s writer tests to `anvil.wasm`
**Status:** resolved
**Blocks:** nothing today (resolved)
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

## Resolution

Bindings agent, 2026-10-26. Planning approved the fix and widened this BR into a cross-binding audit. Branch
`fix/BR-2610-anvl-bindings-002` in each of the five binding repos. Not yet merged or pushed. In every repo the
tests were written first, each text path was checked against a document with a NUL **inside a string value**,
**inside a blob**, and **outside any value**, and the writer and builder round trips were compared byte for byte.

**Native first.** A tiny C program calls `anvil_load_buffer` with an explicit length on a document containing a NUL
in a string and in a blob, with statements after each. Native keeps everything: the string reads back as `61 00 62`,
the blob as `78 00 79`, and the later statements are intact. The source library is correct; any loss was in a binding.

| Binding | Result | Change |
|---|---|---|
| `anvil.wasm` (`0586248`) | **Bug, fixed.** Every text entry point took NUL-terminated text and read it with `strlen`, so `s := "x";\0t := 1;` parsed as `s` alone, with no error. | Every `anvil_wasm_*` text entry point (parse, parseRawValue, the type registry and schema loaders and validators) takes the byte length and reads exactly that many bytes. Names and paths stay NUL-terminated and reject a NUL. Rebuilt on the Linode box. `test/nul.test.js`; 171 pass, 2 `todo`. |
| `anvil.java` (`f1656b0`) | **Bug, fixed.** `AnvilNative.readBufferedString` learned the exact length written, then decoded with `MemorySegment.getString(0)`, which stops at the first NUL. Every string value (and every other buffered text read) was silently cut. Blob bytes and writer output were already exact. | Decode exactly the bytes written. `NulTests`; 84/84. Six of nine audit tests failed first. |
| `anvil.node` (`4a5a636`) | No bug. N-API passes the length. | Tests only: `test/nul.test.js`; 159 pass, 1 `todo`. |
| `anvil.net` (`55f60f9`) | No bug. The UTF-8 string marshaler carries embedded NULs and the readers size by length. | Tests only: `NulTests`; 146 pass, 1 skipped. |
| `anvil.py` (`2b63a75`) | No bug. ctypes passes `bytes` with an explicit length and the readers size by length. | Tests only: `tests/test_nul.py`; 107 pass. |

**Native limits found, filed by planning as BR-2610-anvl-nul-001, and deliberately not worked around** (verified from C, so they are native
and not binding bugs). The document itself loads completely; the loss is in what the type registry and schema *copy*:
- The type registry truncates an enum label at a NUL (`anvil_type_def_get_value` returns 1 byte for `"a\0b"`).
- The schema does the same for its own enum labels (`anvil_schema_field_get_value`).
- The schema copies a string extension value containing a NUL as a **NULL-typed** value, so it is lost entirely
  (`anvil_schema_field_get_extra_value` returns type NULL, length 0).
Each binding records these as a `todo` or skipped test (`anvil.wasm`, `anvil.node`, `anvil.net`) so they flip to passing
when native is fixed. They are not in `anvil.py` or `anvil.java` yet, because those bindings have no type registry or
schema surface (`anvil.py` slice 2 will carry the test from day one).

**What this BR changed my mind about.** I had assumed the NUL-terminated copy in `anvil.wasm` was the only instance. It was
not: Java had the same silent loss through a different mechanism (a `getString` read, not a `strlen` write), found only
because the same tests were run against every binding before any code was read.

