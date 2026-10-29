# BR-2610-anvl-bindings-003: `anvil.node`/`anvil.wasm` collapse a repeated object field name to its last occurrence

**ID:** BR-2610-anvl-bindings-003
**Type:** Bug Report (binding, cross-binding divergence)
**Owner:** anvil-node, anvil-wasm (ANVL Bindings agent)
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Bindings agent's FR-2610-anvl-bindings-003 finding
**Status:** resolved
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

## FlyWire and Rivet call sites (checked read-only before the fix)

Nothing relies on last-wins, so the fix went ahead. Nothing in either checkout was changed.

- **FlyWire data:** every `.anvl` file in the checkout (26), loaded the way `schema-registry.loadFromText` loads them, has **no
  object with a repeated name at any depth**: `schemas/assets.meta.anvl` (10 fields), `schemas/maintenance_records.meta.anvl` (5),
  `tests/fixtures/pooled.meta.anvl` (4), and the rest. (`tests/fixtures/vehicles.meta.anvl` and `vehicles.bindings.anvl` use a legacy
  form with a missing `;` that native rejects at 8:38 and 10:37, so `anvil.node` cannot parse them and cannot be affected.)
- **FlyWire code:** `schema-registry.js:66` iterates `root.entries()` of the schema document, whose top level cannot repeat (the
  resolver rejects it), then `has`/`get` on fixed keys (`type`, `size`, `pooled`, ...). `harness/*.js` reads fixed top-level keys
  (`utid`, `set`, ...). No test sends a repeated name.
- **The one behavior difference, in a request body, not in code or data:** `resolveAssignment` builds its column list from
  `setNode.entries()`. A client that sent `set := { name := "a"; name := "b"; }` was accepted before (last value); now `entries()`
  yields `name` twice, `project()` returns two `name` fields, and `get('name')` is the first, so the statement names the column twice and
  the database rejects it. That is a malformed request failing loudly instead of silently taking the last value; FlyWire may prefer to
  reject a repeated column explicitly in `resolveAssignment`, which is its call.
- **Rivet:** the only traversal of object entries is `src/importer/nodes.js` `toPlain()`, which does `out[name] = toPlain(child)` in
  order, so a plain object built that way is still last-wins and its result is identical before and after (pinned by a test).
  `get`/`has` elsewhere read fixed keys (`config.js`, `migrate.js`, the q-or block ids), which differ only if a block repeats that exact key.

## Resolution

Bindings agent, 2026-10-26. `anvil.node`: branch `feat/BR-2610-anvl-bindings-003-node`, commit `c7b8fd5`, on
`feat/FR-2610-anvl-bindings-003-node`. `anvil.wasm`: branch `feat/BR-2610-anvl-bindings-003-wasm`, commit `85b563a`, on
`feat/FR-2610-anvl-bindings-003-wasm`. Not yet merged or pushed. 186 tests in `anvil.node` (185 pass, 1 `todo`) and 199 in `anvil.wasm`
(197 pass, 2 `todo`), 12 new in each. TDD: written first and RED (5 of 12 failing, the rest guards), then GREEN; in `anvil.wasm` the
RED was confirmed afterwards by stashing the change.

- **Change:** `count` is the native statement count; `get(name)` resolves to the **first** occurrence (`has(name)` is true if any
  occurrence has the name); `entries()` yields every statement in declaration order, repeats included. JS only: FR-2610-anvl-bindings-003
  already put the ordered statement list in the envelope whenever a name repeats, so there is no native change and no rebuild.
  A first-occurrence index is built once per object that has that list; an object without one answers from the name-keyed map as before.
- **No repeats, no change:** results are identical and the attribute-free envelope is byte-identical (the exact-string baselines still pass).
- **Test cases:** the BR's table (1), no repeats unchanged (2), nested objects, objects inside arrays and tuples, interleaved names,
  and repeats combined with attributes (3), existing suites green (4). Also pinned: the top level still rejects a repeat, a plain object
  built from `entries()` is still last-wins (Rivet's `toPlain`), and `parseRawValue()` returns a plain JS object, so a repeat there is
  last-wins by construction.
- **One existing assertion changed:** FR-2610-anvl-bindings-003's test asserted `get('a')` is the last occurrence ("the established
  last-wins meaning"). That is the behavior this BR replaces, so it now asserts the first.

