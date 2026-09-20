# Bindings

**Anvil Native** — a reference C implementation of the AML/AMP parser — is the one parser behind
everything below. Nothing here is a separate reimplementation: the Node, WebAssembly, and .NET
bindings are thin wrappers over the exact same C source, so a document parses identically no
matter which one reads it. Pick the one that matches where your code runs.

| | Runtime | Status |
|---|---|---|
| [Native library](Bindings-Guide.md#native-library) | Any C/C++ program (static or shared link) | Downloadable today |
| [WebAssembly](Bindings-Guide.md#webassembly) | Browser, or any JS host | Downloadable today |
| [Node.js](Bindings-Guide.md#nodejs) | Node.js (N-API) | Downloadable today |
| [.NET](Bindings-Guide.md#net) | .NET 9+ (C#, or any CLR language) | Downloadable today |

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
    anvil_statement stmt = anvil_document_find_statement(doc, "host");
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

## .NET

**[Download anvil-net-v0.8.0-rc-linux-x64.tar.gz](/assets/downloads/anvil-net-v0.8.0-rc-linux-x64.tar.gz)**
— a prebuilt managed assembly (`Anvil.Net.dll`) plus the native library it links against.
Verified standalone: extracted to a clean directory with nothing else present, referenced
directly by a fresh console project, parsed real source correctly.

Shaped differently from the two bindings above on purpose: Node/WASM boundary crossings are
genuinely expensive (V8's object model, `cwrap`/`ccall` overhead), so both convert a whole parsed
document to JSON and cross the boundary once. P/Invoke doesn't have that problem — a call with
blittable types costs tens of nanoseconds — so this binding does real, lazy, on-demand navigation
instead: every accessor below is a direct call into Anvil Native's vtable ABI
(`anvil_vtable.h`), resolved once at startup into cached delegates, nothing pre-materialized into
a tree.

### Requirements

- .NET 9 runtime or later (`net9.0` target; a newer major runtime works too via roll-forward, e.g.
  `<RollForward>LatestMajor</RollForward>`).
- Linux x64 (glibc) only today. Other platforms build from source (`make so-release` in
  `vendor/anvil`'s `v0.8.0-rc` tag or later, then `dotnet build`) — not yet published to NuGet, so
  building means pointing at a real checkout, not `dotnet add package`.
- No other runtime dependencies once built.

```csharp
using Anvil.Net;

using var doc = AnvilDocument.Load("config.anvl");
if (doc is null || doc.HasErrors)
{
    Console.WriteLine($"Failed: {doc?.Error?.Message} at {doc?.Error?.Line}:{doc?.Error?.Column}");
    return;
}

if (doc.FindValue("server") is { Type: AnvilValueType.Object } server)
{
    string? host = server["host"]?.AsString();
    int? port = server["port"]?.AsInt();
}

foreach (var stmt in doc.Statements)
    Console.WriteLine($"{stmt.Name}: {stmt.Value?.Type}");
```

`Load`/`LoadBuffer`/`ParseValueFragment` never throw on a syntax error — they return a document
whose `HasErrors` is `true`. `using`/`Dispose()` releases the native document deterministically;
the whole object graph (`AnvlValue`, `AnvlStatement`, `AnvilAttribute`) is a lightweight,
non-disposable view tied to that document's lifetime — never valid after it's disposed.

### `AnvilDocument` — the one disposable root

| Member | Returns | Notes |
|---|---|---|
| `AnvilDocument.Load(path)` | `AnvilDocument?` | `null` only on a hard I/O failure (missing file); a syntax error still returns a document with `HasErrors == true`. |
| `AnvilDocument.LoadBuffer(source)` | `AnvilDocument?` | Same contract, from an in-memory string. |
| `AnvilDocument.ParseValueFragment(text)` | `AnvilDocument?` | Parses a single, standalone value expression — see `FragmentValue` below. |
| `.HasErrors` / `.ErrorCategory` / `.Error` | `bool` / `AnvilErrorCode` / `AnvilError?` | `.Error` gives message/line/column; `.ErrorCategory` alone is cheaper when you only need to branch on the failure kind. |
| `.FindStatement(name)` | `AnvlStatement?` | Root-level lookup by name — the whole declaration (name, value, its own `@[...]` attributes). |
| `.FindValue(name)` | `AnvlValue?` | Convenience one-hop equivalent of `FindStatement(name)?.Value`. |
| `.Statements` / `.Includes` | `IEnumerable<AnvlStatement>` / `IEnumerable<AnvilDocument>` | Declaration order. Each yielded `Includes` document is independently `IDisposable`. |
| `.Attributes` / `.FindAttribute(key)` | document-level `@[...]` attributes, same shape as a statement's own (below). |

### `AnvlValue` — type inspection and navigation

| Member | Returns | Notes |
|---|---|---|
| `.Type` | `AnvilValueType` | `Null \| Bool \| Numeric \| String \| Blob \| Identifier \| Array \| Tuple \| Object` — finer-grained than the Node/WASM bindings' `.type`, which collapses several of these into one `'scalar'` kind. |
| `.Count` | `int` | Field count for an object, element count for an array/tuple, `0` otherwise. |
| `this[int index]` | `AnvlValue?` | Array/tuple element by position; `null` if out of bounds or not array/tuple-shaped. |
| `this[string key]` | `AnvlValue?` | Object field's value directly, not the statement. `null` if `key` isn't present or this isn't object-shaped. |
| `.Has(key)` | `bool` | Shares the same lazy per-key cache as the indexer above — a repeated lookup of the same key on the same instance costs one native call total, not one per access. |
| `.Entries()` | `IEnumerable<KeyValuePair<string, AnvlValue>>` | Declaration order. Empty for a non-object value. |
| `.AsString()` / `.AsBool()` / `.AsInt()` | `string?` / `bool?` / `int?` | Each `null` unless `.Type` matches exactly (`String` / `Bool` / `Numeric`); `AsInt()` truncates toward zero. |

### `AnvlStatement` and `AnvilAttribute`

| Member | Returns | Notes |
|---|---|---|
| `AnvlStatement.Name` / `.Value` | `string` / `AnvlValue?` | The declaration's name and the value it's assigned. |
| `AnvlStatement.Attributes` / `.FindAttribute(key)` | `IReadOnlyList<AnvilAttribute>` / `AnvilAttribute?` | This statement's own `@[...]` attributes. |
| `AnvilAttribute.Key` / `.Value` | `string` / `string?` | `.Value` is `null` specifically for a flag attribute (`@[active]`, no `=value`). |

### `ParseValueFragment` / `FragmentValue` — for when you already know the shape

Equivalent to `parseRawValue()` on the Node/WASM bindings, but returns a real `AnvlValue` rather
than a plain JS value — no enclosing document/statement context required:

```csharp
using var frag = AnvilDocument.ParseValueFragment("[(1,2,3),(4,5,6)]");
if (frag?.FragmentValue is { Type: AnvilValueType.Array } rows)
{
    int? first = rows[0]![0]!.AsInt(); // 1 -- rows[0] is the Tuple, [0] its first element
}
```

### `AnvilDocument.GetVersion()`

Returns the underlying Anvil Native version string (e.g. `"0.8.0+127-rc"`) — same toolchain
sanity check as the Node/WASM bindings' `getVersion()`, not a compatibility contract.
