# BR-2610-anvl-bindings-001: `anvil.node` `parse()` returns an empty document, without an error, for any non-string argument

**ID:** BR-2610-anvl-bindings-001
**Type:** Bug Report (binding, silent failure)
**Owner:** anvil-node and anvil-wasm (ANVL Bindings agent)
**Filed:** 2026-10-25
**Filed by:** ANVL Bindings agent, found while checking FR-2610-anvl-bindings-002 test case 4
**Status:** resolved
**Blocks:** nothing today; a prerequisite for a bytes-capable loader (see Notes)
**Tags:** bindings, anvil-node, parse, silent-failure, validation

---

## Summary

`parse(x)` in `anvil.node` returns an empty, valid-looking `AnvlNode` (`type === 'object'`, no
entries) when `x` is not a string, and records nothing in `lastError()`. Observed on `main`
(before FR-2610-anvl-bindings-002):

| argument | `parse(x)` | `parseRawValue(x)` |
|---|---|---|
| `Buffer.from('#!aml\na := 1;\n')` | empty object node | `null` |
| `42`, `undefined`, `null`, `{}` | empty object node | `null` |

The cause is `Parse` in `src/binding.c`: it calls `napi_get_value_string_utf8` without checking its
status, so a non-string leaves `text_len` at 0 and the native loader parses an empty buffer, which
is a valid empty document. `parseRawValue` fails the same way internally but its empty fragment is
an error, so it returns `null`, which is at least distinguishable.

## Why it matters

A caller who reads a file as a `Buffer` and passes it straight to `parse()` gets back a document
with no content and no error, and cannot tell it from a genuinely empty one. It is the easiest
mistake to make with this API, and it fails silently.

## Expected behavior

`parse()` and `parseRawValue()` reject a non-string argument loudly: a `TypeError` naming the
argument. (Returning `null` with `lastError()` set would also be consistent with how parse failures
are reported, but a wrong argument type is a programming error rather than a data error, and every
other entry point added since throws `TypeError` for it.)

## Test cases

1. `parse(Buffer)`, `parse(42)`, `parse(undefined)`, `parse(null)` and `parse({})` each throw a
   `TypeError`; the same for `parseRawValue`.
2. `parse('')` is unchanged: a genuinely empty document.
3. The existing suite stays green.

## Notes

- This is a behavior change for any caller relying on the empty result, which seems unlikely, but
  flywire should be checked (`src/am-packet.js`, `harness/server.js`) before it ships. Checked: see
  Resolution.
- Related, and not part of this fix: the loaders take a JS string, which cannot carry non-UTF-8
  bytes. A blob written with `AnvilWriter.blob()` from arbitrary bytes is emitted exactly, but reads
  back lossy. Closing that needs a bytes-capable loader (`parse(Buffer)` that means something, plus an
  `AnvlNode.asBytes()`), which is new surface and would be its own FR. It should not be built on
  top of today's silent behavior, which is why this BR comes first.

## Resolution

Bindings agent, 2026-10-26. Branch `fix/BR-2610-anvl-bindings-001` in both `anvil.node` (commit
`55e23c0`, 151/151 tests) and `anvil.wasm` (commit `47930cc`, 63/63 tests). Not yet merged or pushed.
`test/non-string.test.js` in each (21 tests): written first and RED (20 of 21 failing in `anvil.node`,
19 of 21 in `anvil.wasm`), then GREEN.

- **Fix:** `parse()` and `parseRawValue()` throw a `TypeError` for any argument that is not a string,
  before reaching native code. The message names the function and the type found
  (`parse() requires a string, not Buffer`); `Buffer`, typed arrays and arrays are named as such, and a
  `String` object is rejected rather than coerced. An empty string is unchanged (a genuinely empty
  document). `lastError()` is not touched by the throw.
- **`anvil.wasm` had the same hole**, in a messier form, so it is fixed in this ticket. A number, an
  object or an array parsed as an empty buffer, exactly as in Node. A `Buffer`, `undefined` or `null`
  already threw, but by accident, from deep in the marshaling layer
  (`str.charCodeAt is not a function`, `Cannot read properties of undefined (reading 'length')`).
- **FlyWire call sites, checked read-only** (nothing in the FlyWire checkout was changed): none passes
  a non-string, so this does not break FlyWire.
  - `harness/server.js:117, 220, 270` call `anvl.parse(text)` with `text` from `readBody(req)`, which
    ends in `Buffer.concat(chunks).toString('utf8')`: always a string.
  - `src/am-packet.js` makes no `parse()` call. `AMPacket.unpack(root)` reads an already-parsed node
    (`root.get('payload').asString()`) and returns a **`Buffer`** (`Buffer.from(payloadB64, 'base64')`).
    That Buffer goes to `Codec.decode(buf, schema)` in `src/codec.js`, FlyWire's own binary codec,
    which reads it with `readInt32LE` and never calls the ANVL binding. It does not reach
    `parseRawValue`, which is the one place a Buffer might have been mistaken for text.
  - `harness/client.js` passes `res.text()` results; `src/schema-registry.js` passes
    `fs.readFileSync(path, 'utf8')` or the caller's `loadFromText(source)` string; `src/table.js`
    `Table.decode(text)` takes `encode()`'s ANVL text (a different path from the binary `Codec`).
- **Left for planning (new surface, not built here):** the bytes-capable loader in the Notes above
  (`parse(Buffer)` that means something, plus `AnvlNode.asBytes()`). It is now unblocked: `parse(Buffer)`
  is a loud error rather than a silent empty document, so giving it a meaning later is not a
  behavior change for working code.

