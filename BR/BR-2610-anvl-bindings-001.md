# BR-2610-anvl-bindings-001: `anvil.node` `parse()` returns an empty document, without an error, for any non-string argument

**ID:** BR-2610-anvl-bindings-001
**Type:** Bug Report (binding, silent failure)
**Owner:** anvil-node (ANVL Bindings agent)
**Filed:** 2026-10-25
**Filed by:** ANVL Bindings agent, found while checking FR-2610-anvl-bindings-002 test case 4
**Status:** open
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
  flywire should be checked (`src/am-packet.js`, `harness/server.js`) before it ships.
- Related, and not part of this fix: the loaders take a JS string, which cannot carry non-UTF-8
  bytes. A blob written with `AnvilWriter.blob()` from arbitrary bytes is emitted exactly, but reads
  back lossy. Closing that needs a bytes-capable loader (`parse(Buffer)` that means something, plus an
  `AnvlNode.asBytes()`), which is new surface and would be its own FR. It should not be built on
  top of today's silent behavior, which is why this BR comes first.
