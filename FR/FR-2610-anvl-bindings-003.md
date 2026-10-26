# FR-2610-anvl-bindings-003: statement-level attributes (and attribute values) in `anvil.node` and `anvil.wasm`

**ID:** FR-2610-anvl-bindings-003
**Type:** Feature Request (bindings, surface parity)
**Owner:** anvil-node, then anvil-wasm (ANVL Bindings agent)
**Filed:** 2026-10-26
**Filed by:** anvil (planning)
**Requested by:** rivet (found by Rivet Coder building the q-or importer; verified by the Rivet Project Engineer)
**Status:** open — not started
**Tags:** bindings, anvil-node, anvil-wasm, attributes, parity, rivet-readiness

---

## Summary

`parse()` in `anvil.node` and `anvil.wasm` exposes attributes only as a list of **module-level
keys** on the root node (`lib/index.js`: "`moduleAttributeKeys` is only ever set on the root";
`AnvlNode.hasAttribute(key)` checks that list). The C side (`src/binding.c` ≈ line 555) marshals
keys only, root only. So from JS:

- Statement attributes (`x @[status="done"] := …`, `ID @[type=…, status=…] { … }`) aren't reachable at all.
- Even module attribute **values** are lost: `@[version="1.2"]` is visible as present, and its value isn't.

`anvil.py` and `anvil.net` expose both (`AnvlStatement.attributes` / `find_attribute` →
`AnvilAttribute` with `key` and `value`), and the native API has them. This breaks the
consistent-official-surface rule (`anvil-bindings-official-surface-stays-consistent`).

## Motivation

Rivet's q-or importer reads legacy q-or files that use per-statement attributes heavily
(`wip/*.anvl` step status, task priority/created, ticket headers). Until this lands, the importer
lifts attributes into fields textually, in a legacy normalizer that lives only in the importer.
That workaround carries this FR's ID as its TODO. Not blocking.

## Design

- **Marshaling:** extend the existing C→JS envelope (the accepted internal JSON mechanism; see
  `anvil-internal-json-marshaling-accepted`) to carry each statement's attributes as
  `[{ key, value }]`, with `value` `null` for a bare key, and to carry module attribute values too.
  Only emit when a statement has attributes, so attribute-free documents (FlyWire's hot path) pay
  nothing. Re-measure against `BR-2609-anvl-004`'s numbers and record the result.
- **Surface (JS idiom, same names and meaning as py/net):**
  - The root/document level: `attributes` → `[{ key, value }]` and `findAttribute(key)`, alongside the
    existing `hasAttribute(key)`, which keeps working unchanged.
  - The statement level: today `AnvlNode` has no statement object (`entries()` yields
    `[name, node]`). Add one, e.g. `node.statements()` → `AnvlStatement { name, value, attributes,
    findAttribute(key) }` in declaration order, plus `node.statement(name)` for the first one with
    that name. Names can repeat inside an object, so the ordered list is the complete form. The
    Bindings agent picks the final shape and records it here; whatever it is, py/net semantics
    hold (attribute order, a missing key → `null`/`undefined`, bare keys).
- **Values** are text, as `AnvilAttribute.value` is in py/net. The values must survive NUL-safe,
  per BR-2610-anvl-bindings-002's rules.
- `anvil.wasm` gets the identical surface (second slice), as with every other FR in this set.

## Test cases

1. `x @[status="done", flag] := 1;`: the statement's attributes are `[{key:'status', value:'done'},
   {key:'flag', value:null}]`; `findAttribute('status').value === 'done'`; `findAttribute('nope')`
   is absent.
2. Object-block statements (`ID @[type=bug] { … };`) and nested statements inside objects carry
   their own attributes.
3. Module attributes: `#!aml` with `@[version="1.2"]`. The root `findAttribute('version').value ===
   '1.2'`, and `hasAttribute('version')` is still true (regression).
4. A document with no attributes produces an envelope no larger than today (the hot path is unchanged).
5. Repeated statement names inside an object both appear in `statements()`, in order.
6. Existing suites green; the anvil.node addon still exports only its N-API entry points.
7. anvil.wasm: tests 1–6 ported.
8. A changelog entry in each binding and in anvil's `docs/changelog.md`.

## Dependencies

None. Native already exposes statement and document attributes.
