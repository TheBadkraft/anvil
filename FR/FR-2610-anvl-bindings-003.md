# FR-2610-anvl-bindings-003: statement-level attributes (and attribute values) in `anvil.node` and `anvil.wasm`

**ID:** FR-2610-anvl-bindings-003
**Type:** Feature Request (bindings, surface parity)
**Owner:** anvil-node, then anvil-wasm (ANVL Bindings agent)
**Filed:** 2026-10-26
**Filed by:** anvil (planning)
**Requested by:** rivet (found by Rivet Coder building the q-or importer; verified by the Rivet Project Engineer)
**Status:** resolved — `anvil.node` and `anvil.wasm` slices done; see Resolution
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

1. `x @[status="done", flag] := 1;`: the statement's attributes are `[{key:'status', value:'"done"'},
   {key:'flag', value:null}]`; `findAttribute('status').value === '"done"'`; `findAttribute('nope')`
   is absent. *(Corrected 2026-10-26: `value` is native's raw text, so a quoted string keeps its quotes,
   as in `anvil.py` and `anvil.net`. The decoded `done` is the native read of
   FR-2610-anvl-public-api-004, added to all four bindings together when it lands; see Resolution.)*
2. Object-block statements (`ID @[type=bug] { … };`) and nested statements inside objects carry
   their own attributes.
3. Module attributes: `#!aml` with `@[version="1.2"]`. The root `findAttribute('version').value ===
   '"1.2"'` (raw, as in case 1; decoded `1.2` is FR-2610-anvl-public-api-004), and
   `hasAttribute('version')` is still true (regression).
4. A document with no attributes produces an envelope no larger than today (the hot path is unchanged).
5. Repeated statement names inside an object both appear in `statements()`, in order.
6. Existing suites green; the anvil.node addon still exports only its N-API entry points.
7. anvil.wasm: tests 1–6 ported.
8. A changelog entry in each binding and in anvil's `docs/changelog.md`.

## Dependencies

None. Native already exposes statement and document attributes.

## Resolution

Bindings agent, 2026-10-26. `anvil.node`: branch `feat/FR-2610-anvl-bindings-003-node`, commit `2c28fa4`, on
`fix/BR-2610-anvl-bindings-002`. `anvil.wasm`: branch `feat/FR-2610-anvl-bindings-003-wasm`, commit `4506503`, on
`fix/BR-2610-anvl-bindings-002`. Not yet merged or pushed. 174 tests in `anvil.node` (173 pass, 1 `todo`) and 187
in `anvil.wasm` (185 pass, 2 `todo`), 14 new in each. TDD: the test set first (11 of 14 failing against inert stubs in
each repo), then the C, the `emcc` build for wasm, and the JS. The addon still exports only its two N-API entry points,
and Valgrind shows 0 errors and 0 bytes lost over attributed, repeated-name, nested and NUL-valued documents.

### The shape (decided here, as the FR asked)

- **Document level** (on the root node `parse()` returns): `attributes` → frozen `[{ key, value }]` and
  `findAttribute(key)` → `{ key, value }` or `null`. `hasAttribute(key)` is unchanged. A non-root node has no module
  attributes: `attributes` is `[]`, `findAttribute` is `null`.
- **Statement level:** `node.statements()` → `AnvlStatement[]` in declaration order, `node.statement(name)` → the first
  with that name or `null`. `AnvlStatement` is `{ name, value: AnvlNode, attributes, findAttribute(key) }`, exported.
  `statements()` is `[]` for a value that is not an object. The root's own statements work the same way.
- **An attribute** is an immutable `{ key, value }`, `value` `null` for a flag (`@[active]`), in declaration order.
  A missing key is `null`.
- **Repeated names:** `statements()` has one entry per occurrence; `get(name)` keeps its established last-wins meaning and
  `statement(name)` is the first, which is what `find_statement` returns in py/net.

### The envelope (internal, still JSON, as the FR accepted)

An object's envelope is `{"type":"object","value":{name:...}}` as before. When, and only when, a statement in it has
attributes or a name repeats, it also carries an ordered `"stmts":[{"n":name,"a":[[key,value|null],...],"v":<tagged>}]`:
`a` only for an attributed statement, and `v` only for a repeated name's earlier occurrences (every other statement's value
is the name-keyed map's, so nothing is duplicated except those). The root envelope also carries `"moduleAttrs"` when the
document has module attributes; `"attributes"` (the keys) is unchanged. Detection of repeats is a sort of the statement
names, O(n log n) per object. **An attribute-free document's envelope is byte-identical to before** (pinned in each repo by
`test/attribute-free-envelopes.json`).

### Measurements (BR-2609-anvl-004's methodology: median of 15 runs, old addon against new on the same documents)

| | `anvil.node` old → new | `anvil.wasm` old → new |
|---|---:|---:|
| `parseRawValue()`, 10,000 rows × 10 fields (path not touched) | 18.56 → 18.57 ms (1.00x) | 27.98 → 28.44 ms (1.02x) |
| `parse()`, FlyWire-style 3-statement message, 2,000 parses | 4.23 → 4.41 ms (1.04x) | 8.30 → 8.74 ms (1.05x) |
| `parse()`, 5,000 object statements, no attributes | 10.60 → 11.06 ms (1.04x) | 15.28 → 16.91 ms (1.11x) |
| `parse()`, 5,000 object statements, an attribute on each | 12.85 → 19.80 ms (1.54x) | 16.92 → 25.55 ms (1.51x) |

Envelope bytes for the attribute-free 5,000-statement document and the message are identical (957,832 and 201). So the hot
path does not pay *nothing*: attribute-free `parse()` is about 4% slower in Node and 5% to 11% in WASM, from the extra pass
over each object's statements. For FlyWire's real message that is roughly 0.09 µs per parse in Node. The 1.5x for attributed
documents is the cost of carrying the data. Accepted by planning (about 0.09 µs on a FlyWire-sized message); the pass is not made conditional, and this table is the record.

### Decisions confirmed by planning (2026-10-26)

1. **Attribute values stay raw: a quoted string keeps its quotes.** `status="done"` is `'"done"'` in all four bindings, which is
   what the native getter (`anvil_attribute_get_value`) copies and what `anvil.py` and `anvil.net` return. **The FR's test case 1,
   which reads `'done'`, was wrong**, and the tests in `anvil.node` and `anvil.wasm` assert the raw form. A decoded read is a native
   capability, FR-2610-anvl-public-api-004; when it lands, a decoded accessor is added to **all four** bindings together, with
   `value` staying raw everywhere. Bare values (`type=bug`) and numbers (`n=5`) are as written either way.
2. **The repeated-name collapse** in `get(name)`, `entries()` and `count` (native keeps both occurrences: `count` 3 for
   `{ a := 1; a := 2; b := 3; }`, where JS said 2) is now **BR-2610-anvl-bindings-003**, resolved.

### Tests

The FR's cases 1–8 are covered: attributes with a flag and absent key (1), object-block and nested statements, including
inside arrays (2), module attributes with `hasAttribute` unchanged (3), the exact attribute-free envelope (4), repeated names
in order (5), the existing suites and the export list (6), the same tests in `anvil.wasm` (7), changelogs (8). Also pinned:
attribute values and module attribute values with a NUL (nothing after it lost), quotes, backslashes and non-ASCII.

