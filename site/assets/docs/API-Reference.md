# API Reference

All examples assume:

```js
import { parse, load, write, lastError, AnvilNode, AnvilError, ErrorCode } from './src/index.js';
```

`NodeType` and `ScalarKind` are **not** re-exported from `src/index.js` —
import them directly from `./src/node.js` if you need the enum constants
rather than comparing against string literals.

This library is ESM-only. `package.json` declares a `require` export
pointing at `src/index.cjs`, but that file does not currently exist —
`require('anvl-js')` will fail until a CJS build is added.

---

## Module exports (`src/index.js`)

### `parse(sourceText, file = null)`

Parses ANVL source text.

- **Returns:** `AnvilNode | null`. Never throws on a syntax/structural
  error — returns `null` and records the failure for `lastError()`.
- `file` is an optional path/name attached to any resulting `AnvilError`,
  for error messages that need to identify their source document.

### `load(path)`

Reads a file from disk and parses it. **Async** — Node.js only (dynamically
imports `node:fs/promises`; fails gracefully with `INVALID_PATH` outside
Node).

- **Returns:** `Promise<AnvilNode | null>`.

```js
const node = await load('./config.anvl');
```

### `lastError()`

- **Returns:** the `AnvilError` from the most recent `parse()`/`load()`
  call, or `null` if that call succeeded.
- Only reflects the **most recent** call — call it immediately after
  `parse()`/`load()`, before any other parse.

### `write(node, options = {})`

Serializes a parsed document back to ANVL source text.

- `options.minify` (`boolean`, default `false`) — when `true`, collapses to
  a single line with no unnecessary whitespace, and *always* inserts a
  comma between top-level statements (even though the parser treats commas
  there as optional) so the output remains unambiguous to re-parse.
- **Returns:** a `string`. Returns `""` if `node` doesn't carry a reference
  to its parsed document — see the caveat in
  [How-To: Round-tripping](How-To.md#round-tripping).

### `parseRawValue(sourceText, options = {})`

A schema-directed decode entry point for when the caller already knows the
shape of a value and just wants it fast — no `AnvilNode` tree gets built at
all. Motivating case: pulling a known-shape value (e.g. an array of tuples)
out of a blob's raw content, where building and immediately discarding a
full generic tree per value is pure overhead.

Parses a **fragment** rather than a document — no shebang, and no
requirement to be a complete statement. A fragment is either of two shapes,
distinguished automatically by what's actually there:

```anvl
[(1,2,3), (4,5,6)]        // a bare value
payload := [(1,2,3), (4,5,6)]   // a name := value fragment
```

- Both shapes return **just the value** — the name in the second shape is
  discarded, so a caller never has to branch on which shape it got.
- A fragment is never broken by punctuation it doesn't require: a trailing
  `;` is tolerated in either shape but never mandatory.
- Any value kind is supported — scalars, arrays, tuples, objects, blobs —
  **except `$` VarRef**, which fails everywhere it appears, including
  nested several levels deep inside an otherwise-valid array/object under
  AML where the general grammar would normally allow it. This is an
  architectural exclusion, not a scope choice: there's no document-level
  registry here for a VarRef to resolve against, so letting one through
  unresolved would be silently wrong rather than merely unsupported.
- `options.dialect` (`'aml' | 'amp'`, default `'aml'`) — matches every other
  dialect-detection point in the library defaulting to AML; pass `'amp'` to
  additionally enforce AMP's rules (no objects, scalar-only collection
  elements) on either shape.
- Objects become plain JS objects (field order preserved); tuples and
  arrays both become plain JS arrays, since JS has no separate fixed-arity
  tuple type; blobs become their content string, tag dropped (matching
  `AnvilNode.asString()`'s convention for blobs).
- **Returns:** a plain JS value — never an `AnvilNode`. **Caveat:** unlike
  every other entry point in this library, a `null` return does not by
  itself mean failure — an ANVL `null` scalar legitimately decodes to JS
  `null` on success. Always check `lastError()`, not just the return
  value's truthiness, to tell success from failure.
- Never throws. Reuses the same internal grammar/scanning functions the
  main parser uses, unchanged, so every existing grammar-level failure
  (empty collections, tuple arity, unterminated strings, AMP's rules) is
  inherited automatically rather than re-validated.

```js
parseRawValue('[(1,2,3), (4,5,6)]');
// -> [[1, 2, 3], [4, 5, 6]]

parseRawValue('payload := [(1,2,3), (4,5,6)];');
// -> same result -- the "payload" name is discarded
```

---

## `AnvilNode`

A single, read-only facade type for every ANVL construct (scalar, object,
array, tuple, blob) — there is no separate node class per construct.

### Type inspection

| Member | Returns | Notes |
|---|---|---|
| `.type` | `"scalar" \| "object" \| "array" \| "tuple" \| "blob" \| null` | |
| `.kind` | `"string" \| "int" \| "float" \| "bool" \| "null" \| "hex" \| "bare" \| null` | Only meaningful when `.type === "scalar"`; `null` otherwise |

### Scalar access

| Member | Returns | Notes |
|---|---|---|
| `.asString()` | `string \| null` | For `hex`, returns the canonical uppercase 6-digit form (no `#`). For `bool`, returns `"true"`/`"false"`. For `null`-kind, returns `null`. For blobs, returns the raw content. |
| `.asInt()` | `number \| null` | Works for `int` and `hex` kinds; truncates `float`. `null` otherwise. |
| `.asFloat()` | `number \| null` | Works for `float` and `int` kinds. `null` otherwise. |
| `.asBool()` | `boolean \| null` | Only for `bool` kind. |
| `.isNull()` | `boolean` | `true` only for `null`-kind scalars (including unresolved/circular VarRefs). |
| `.asBuffer()` | slice object or `null` | Zero-copy `{ pos, len, toString() }` view into the original source, when available. |

### Object navigation

| Member | Returns |
|---|---|
| `.get(key)` / `.field(key)` | `AnvilNode \| null` |
| `.has(key)` | `boolean` |
| `.keys()` | `string[]`, in declaration order |
| `.entries()` | `[string, AnvilNode][]`, in declaration order |
| `.count` | number of fields |

### Array / Tuple

| Member | Returns |
|---|---|
| `.at(index)` | `AnvilNode \| null` |
| `.count` | number of elements |
| `for (const el of node)` | iterates elements (arrays/tuples) or field values (objects) |

### Inheritance

| Member | Returns | Notes |
|---|---|---|
| `.hasBase()` | `boolean` | |
| `.baseIdentifier()` | `string \| null` | Exposes the relationship only — field merging through inheritance is not implemented; see [AML Guide: Inheritance](AML-Guide.md#inheritance). |

### Attributes

| Member | Returns | Notes |
|---|---|---|
| `.hasAttribute(key)` | `boolean` | |
| `.attribute(key)` | `string \| null` | Flag attributes (no `=value`) return `""`. |
| `.attributes` | `{ key, value }[]` | For a top-level statement's value, this includes both the statement's own attributes and the document's module-level attributes. |

### Identity

| Member | Returns | Notes |
|---|---|---|
| `.is(name)` | `boolean` | `true` if this node is the value of the statement/field named `name`. |

---

## `AnvilError`

Extends `Error`. Shape:

```ts
{
  message: string,
  code: number,      // see Error Codes
  line: number,
  column: number,
  file: string | null,
  name: "AnvilError"
}
```

## `ErrorCode`

See [Error Codes](Error-Codes.md) for the full table.
