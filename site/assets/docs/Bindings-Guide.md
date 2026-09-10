# Bindings

**Anvil Native** — a reference C implementation of the AML/AMP parser — is the one parser behind
everything below. Nothing here is a separate reimplementation: the Node and WebAssembly bindings
are thin wrappers over the exact same C source, so a document parses identically no matter which
one reads it. Pick the one that matches where your code runs.

| | Runtime | Status |
|---|---|---|
| [Native library](Bindings-Guide.md#native-library) | Any C/C++ program (static or shared link) | Downloadable today |
| [WebAssembly](Bindings-Guide.md#webassembly) | Browser, or any JS host | Downloadable today |
| [Node.js](Bindings-Guide.md#nodejs) | Node.js (N-API) | Downloadable today |

## Native library

The C library itself — link it directly if you're not going through a JS binding at all.

**[Download anvil-v0.8.0-rc-linux-x86_64.tar.gz](/assets/downloads/anvil-v0.8.0-rc-linux-x86_64.tar.gz)**
— static (`libanvil.a`) and shared (`libanvil.so`) builds, release (stripped, `-O2`), plus the
public headers needed to use them. Linux x86_64 only today; other platforms build from source
(`make lib` in the [anvil repo](https://github.com/TheBadkraft/anvil)) until real cross-platform
CI exists.

```sh
tar -xzf anvil-v0.8.0-rc-linux-x86_64.tar.gz
gcc your_program.c -I anvil-v0.8.0-rc-linux-x86_64/include \
    -L anvil-v0.8.0-rc-linux-x86_64/lib -lanvil -o your_program
```

```c
#include "anvil_flat.h"

anvil_document doc = anvil_load_buffer(source, source_len);
if (anvil_has_errors(doc)) {
    anvil_error err = anvil_document_get_error(doc);
    /* err's message/line/column */
} else {
    anvil_statement stmt = anvil_statement_get(doc, "host");
    /* anvil_statement_get_value(stmt), etc. */
}
anvil_dispose(doc);
```

Full C API reference: see the anvil repo's own `docs/language-reference.md` and
`docs/getting-started.md`.

## WebAssembly

**[Download anvil-wasm-v0.8.0-rc.tar.gz](/assets/downloads/anvil-wasm-v0.8.0-rc.tar.gz)** — the
compiled module (`anvil.wasm`), its Emscripten glue (`anvil.js`), and a JS-facing wrapper
(`index.js`) with the same shape as the Node binding below.

### Requirements

- Any JS host that can load a WASM module (browser or Node)
- No other runtime dependencies

### One real difference from Node: loading is async

Instantiating a WASM module takes a moment, so you pay that cost once, explicitly, before doing
anything else:

```js
const anvil = require('./index.js');

await anvil.ready(); // one-time async cost

const doc = anvil.parse('#!aml\nserver := { host := "localhost"; port := 8080; };');
if (!doc) {
  const err = anvil.lastError();
  console.error(`${err.message} at ${err.line}:${err.column}`);
} else {
  console.log(doc.get('server').get('host').asString()); // "localhost"
}
```

Every call after `ready()` resolves is a plain synchronous call — the async cost is paid once,
not per parse. Everything else below (`parse()`'s return shape, `parseRawValue()`, `AnvlNode`,
the error object) is identical to the Node binding's own API — see [Node.js](Bindings-Guide.md#nodejs) below for
the full reference; only the loading step differs.

## Node.js

**[Download anvil-node-v0.8.0-rc-linux-x86_64.tar.gz](/assets/downloads/anvil-node-v0.8.0-rc-linux-x86_64.tar.gz)**
— a prebuilt N-API addon (native addon, not a from-scratch JS parser) plus its JS-facing
wrapper. Verified standalone: extracted to a clean directory with nothing else present, parsed
real source, correctly reported a real syntax error via `lastError()`.

### Requirements

- Node.js with N-API support (any current LTS). Built and tested against Node 22.x — N-API's
  own ABI-stability guarantee means other recent majors should work too, but that hasn't been
  verified against older ones.
- Linux x86_64 (glibc) only today, same as the [native library](Bindings-Guide.md#native-library). Other
  platforms build from source (`npm install && npm run build`, needs a C2x compiler and Python 3
  for `node-gyp`) — not yet published to a package registry, so building means pointing at a
  real checkout, not `npm install anvil-node`.
- No runtime dependencies once built.

```js
const anvil = require('./lib/index.js'); // keep lib/index.js next to build/Release/anvil_node.node

const doc = anvil.parse('#!aml\nserver := { host := "localhost"; port := 8080; };');
```

`parse()` never throws on a syntax error — it returns `null` instead. Always check the result:

```js
if (!doc) {
  const err = anvil.lastError();
  console.error(`${err.message} at ${err.line}:${err.column}`);
} else {
  const config = doc.get('server');
  console.log(config.get('host').asString()); // "localhost"
  console.log(config.get('port').asInt());    // 8080
}
```

### `AnvlNode` — a single, read-only facade over a parsed value

**Type inspection**

| Member | Returns | Notes |
|---|---|---|
| `.type` | `'object' \| 'array' \| 'tuple' \| 'scalar' \| 'blob' \| null` | Tells `[1,2]` (an array) apart from `(1,2)` (a tuple), or a plain string apart from a blob — distinctions `.asString()`/`.at()`/`.count` alone can't make. Identical on both bindings. |

**Object navigation**

| Member | Returns | Notes |
|---|---|---|
| `.has(key)` | `boolean` | `false` for anything that isn't an object-shaped value. |
| `.get(key)` | `AnvlNode \| null` | `null` if `key` isn't present, or this node isn't object-shaped. |
| `.entries()` | `[string, AnvlNode][]` | Declaration order. `[]` for anything that isn't object-shaped. |
| `.count` | `number` | Field count for an object, element count for an array/tuple, `0` otherwise. |

**Array / Tuple**

| Member | Returns | Notes |
|---|---|---|
| `.at(index)` | `AnvlNode \| null` | `null` if out of bounds, or this node isn't array/tuple-shaped. |

**Scalar access**

| Member | Returns | Notes |
|---|---|---|
| `.asString()` | `string \| null` | `null` unless the underlying value is a JS string. |
| `.asBool()` | `boolean \| null` | `null` unless the underlying value is a JS boolean. |
| `.asInt()` | `number \| null` | `null` unless the underlying value is a JS number; truncates toward zero. |

**Attributes**

| Member | Returns | Notes |
|---|---|---|
| `.hasAttribute(key)` | `boolean` | Root only — checks the document's own module-level `@[...]` attributes. |

### `parseRawValue(sourceText)` — for when you already know the shape

No `AnvlNode` wrapper at all, just a plain JS value straight back. Parses a **Value Fragment** —
a single value expression, no shebang, no enclosing statement required:

```js
anvil.parseRawValue('[(1,2), (3,4)]');
// -> [[1, 2], [3, 4]]
```

**Caveat**: unlike every other entry point here, a `null` return does not by itself mean failure
— a genuine ANVL `null` value also decodes to JS `null` on success. Always check `lastError()`,
not just the return value's truthiness, to tell success from failure.

### `lastError()`

Returns `{ message, line, column }` for the most recent `parse()`/`parseRawValue()` failure, or
`null` if that call succeeded. Only reflects the *most recent* call — read it immediately after,
before any other call.

### `getVersion()`

Returns the underlying Anvil Native version string (e.g. `"0.8.0+89-rc"`) — a toolchain sanity
check, not a compatibility contract to branch logic on.
