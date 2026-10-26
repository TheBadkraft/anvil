# BR-2610-anvl-bindings-003: `anvil.node`/`anvil.wasm` collapse a repeated object field name to its last occurrence

**ID:** BR-2610-anvl-bindings-003
**Type:** Bug Report (binding, cross-binding divergence)
**Owner:** anvil-node, anvil-wasm (ANVL Bindings agent)
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Bindings agent's FR-2610-anvl-bindings-003 finding
**Status:** open
**Tags:** bindings, anvil-node, anvil-wasm, parity, objects

---

## Summary

ANVL allows a name to repeat inside an object (the reader's resolver rejects repeats only at the
top level). For `o := { a := 1; a := 2; b := 3; };`:

| | `count` | lookup of `a` | entries |
|---|---|---|---|
| Native, `anvil.py`, `anvil.net` | 3 | **first** (`1`, via `anvil_value_find_statement`) | all three, in order |
| `anvil.node`, `anvil.wasm` | **2** | **last** (`2`) | `a` once, `b` |

The JS envelope is a JSON object, so later keys overwrite earlier ones. The same document gives
different answers in different official bindings, which breaks
`anvil-bindings-official-surface-stays-consistent`. FR-2610-anvl-bindings-003 added
`statements()`, which is already complete and correct. This BR makes the older accessors agree too.

## Expected behavior

- `count` equals the native statement count.
- `get(name)` / `has(name)` resolve to the **first** occurrence, as native and py/net do.
- `entries()` yields every statement in declaration order, repeats included.
- A document with no repeated names behaves exactly as today (envelope and results byte-identical).

## Before fixing

FlyWire is the one consumer that could depend on last-wins. Check its call sites read-only
(`get`/`entries`/`count` on objects whose names might repeat) and record the finding here. If
anything relies on last-wins, stop and report; don't break it silently.

## Test cases

1. The table above: node and wasm give 3 / `1` / all three, in order.
2. No repeated names: results and envelope unchanged (regression, exact-string).
3. Nested objects with repeats behave the same.
4. The existing suites stay green; changelog entries in both bindings and in anvil.
