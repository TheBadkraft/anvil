# Bindings

**Anvil Native** — a reference C implementation of the AML/AMP parser — is the one parser behind
everything below. Nothing here is a separate reimplementation: the Node, WebAssembly, .NET, and
Python bindings are thin wrappers over the exact same C source, and anvil.js is a pure-JavaScript
reader held to the same results by a shared conformance corpus, so a document parses identically
no matter which one reads it. Pick the one that matches where your code runs.

| | Runtime | Status |
|---|---|---|
| [Native library](Bindings-Guide.md#native-library) | Any C/C++ program (static or shared link) | Downloadable today |
| [anvil.js](Bindings-Guide.md#javascript-anvil-js) | Node.js 18+ and any browser (plain JavaScript) | Downloadable today (reader only) |
| [WebAssembly](Bindings-Guide.md#webassembly) | Browser, or any JS host | v0.11.0-rc, no further updates; replaced by anvil.js |
| [Node.js](Bindings-Guide.md#nodejs) | Node.js (N-API) | v0.11.0-rc, no further updates; replaced by anvil.js |
| [.NET](Bindings-Guide.md#net) | .NET 9+ (C#, or any CLR language) | Downloadable today |
| [Python](Bindings-Guide.md#python) | CPython 3.11+ (ctypes) | Downloadable today |

The current release is `v0.12.0-rc`, built from one source tag: the native library, anvil.js,
.NET and Python. The Node.js and WebAssembly downloads are from `v0.11.0-rc`: they are frozen,
receive no further updates and are replaced by anvil.js, but they stay downloadable at their
existing addresses. Checksums for every download are in
[`SHA256SUMS`](/assets/downloads/SHA256SUMS); after downloading, check them with
`sha256sum -c SHA256SUMS`.

The native downloads (library, Node.js, Python and .NET) are **Linux x86_64 (glibc) only**; on any
other platform, build from source. The WebAssembly build is not platform-specific. Java has a
binding too, but it is not distributed.

### The version string

A library reports its version as the release plus a build number after a `+`, valid semver:
a prebuilt `v0.12.0-rc` download reports `0.12.0-rc+389` (anvil.js reports it from `getVersion()`),
and the build number is the same in every download of the release. A binding **built from source**
has no build number of its own, so it reports plain `0.12.0-rc` (no `+` part). That is expected. Set `ANVL_BUILD=<n>` when you build
or install from source if you want a build number in the string. Treat the string as a
sanity check, not a compatibility contract to branch logic on.

### What each binding offers

| | Node.js | WebAssembly | .NET | Python |
|---|---|---|---|---|
| Parse, statements, values | yes | yes | yes | yes |
| Attributes, raw and decoded | yes | yes | yes | yes |
| Type registry and schema | yes | yes | yes | yes |
| Writer and builder | yes | yes | yes | yes |

Node.js and WebAssembly share one API. .NET and Python share another, with lazy navigation
instead of a one-call conversion, and each follows its own language's idiom.

## JavaScript (anvil.js)

**[Download anvil.js-v0.12.0-rc.tar.gz](/assets/downloads/anvil.js-v0.12.0-rc.tar.gz)** — a
pure-JavaScript ANVL reader: no native code, no WebAssembly. It replaces the Node.js and WebAssembly
downloads going forward (both stay available at v0.11.0-rc). It has the same reader API as the Node.js
binding below (`parse`, `parseRawValue`, `lastError`, `getVersion`, nodes and statements, `asInt`,
`asBigInt`, `numericKind` and the rest), plus `isNull()`, the blob tag, an `implementation` property
(`'anvil.js'`) and a `BUILD_INFO.json`. It reads every document the same way Anvil Native does: it
passes all 1,867 cases of the shared conformance corpus.

**This release has the reader only.** Types, schema validation and the writer are not in anvil.js
yet; use the Python or .NET packages, or the C library, where you need those.

```sh
npm install ./anvil.js-v0.12.0-rc.tar.gz
```

```js
const anvl = require('anvil.js');           // Node 18 or later
const doc = anvl.parse('#!aml\nfoo := 1;');
doc.get('foo').asInt(); // 1
anvl.getVersion();      // "0.12.0-rc+389"
```

In an ES module use the default import: `import anvl from 'anvil.js'` (named imports from the package
entry are not offered). An `include` resolves against the working directory.

In a browser, with no bundler, the tarball holds `dist/anvil.mjs` (an ES module) or
`dist/anvil.global.js` (a script tag that defines `window.anvl`). The browser builds have no `include`.
Both were run in Chromium and Firefox before release.

The package licence is `SEE LICENSE IN LICENSE.txt`; the terms are in progress (see the [download page](/download)).

## Native library

The C library itself — link it directly if you're not going through a JS binding at all.

**[Download anvil-v0.12.0-rc-linux-x86_64.tar.gz](/assets/downloads/anvil-v0.12.0-rc-linux-x86_64.tar.gz)**
— static (`libanvil.a`) and shared (`libanvil.so`) builds, release (stripped, `-O2`), plus every
public header (`include/anvil_*.h`). Linux x86_64 only today; other platforms build from source
(`make lib` in the [anvil repo](https://github.com/TheBadkraft/anvil)) until real cross-platform
CI exists.

```sh
tar -xzf anvil-v0.12.0-rc-linux-x86_64.tar.gz
gcc your_program.c -I anvil-v0.12.0-rc-linux-x86_64/include \
    -L anvil-v0.12.0-rc-linux-x86_64/lib -lanvil -o your_program
```

The library reports `0.12.0-rc+389` from `anvil_get_version()`. `libanvil.so` exports exactly
the public API and no internal symbols, so it cannot collide with other libraries loaded in the
same process.

What the headers cover:

| Header | What it is |
|---|---|
| `anvil_flat.h` | The primary API: load, statements, values, attributes, errors |
| `anvil_vtable.h` | The same API as a struct of function pointers |
| `anvil_types.h` | Opaque handles and stable error categories |
| `anvil_type_registry.h` | The opt-in type registry (`@[types]`) |
| `anvil_schema.h` | AnvilSchema (`@[schema]`) |
| `anvil_writer.h`, `anvil_builder.h` | The streaming writer and the document builder, which emit canonical AML and AMP |
| `anvil_schema_vtable.h`, `anvil_type_registry_vtable.h` (and the writer and builder vtables) | Vtable forms of the schema, registry, writer and builder |

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

An attribute's value can be read raw (`anvil_attribute_get_value`, the source text) or decoded
(`anvil_attribute_get_value_handle`, a value: `@[status="done"]` reads as the string `done`).
Values also have exact readers: `anvil_value_get_numeric`, `anvil_value_get_bytes` and
`anvil_value_get_blob_tag`.

**Thread safety.** Independent loads are safe: any number of threads may call `anvil_load`,
`anvil_load_buffer` and `anvil_parse_value_fragment` at once, even on identical text, because each
load owns its state. Sharing **one** handle (a document, a schema, a registry) across threads is
the caller's job: serialize it, and never use it during or after its disposal. The writer and
builder are single-thread.

Full C API reference: see the anvil repo's own `docs/language-reference.md` and
`docs/getting-started.md`.

## WebAssembly

**[Download anvil-wasm-v0.11.0-rc.tar.gz](/assets/downloads/anvil-wasm-v0.11.0-rc.tar.gz)** — the
compiled module (`dist/anvil.wasm`, about 160 KB), its Emscripten glue (`dist/anvil.js`), a
JS-facing wrapper (`lib/index.js`) with the same API as the Node binding below, and a prebuilt
script-tag bundle for browsers (`dist/anvil.browser.js`). The module carries
the core reader, the type registry, AnvilSchema and the writer and builder.

### Requirements

- Any JS host that can load a WASM module (browser or Node); not tied to Linux or x64
- No other runtime dependencies

### Installing and layout

```sh
npm install ./anvil-wasm-v0.11.0-rc.tar.gz
```

```js
const anvil = require('anvil-wasm');
```

The package keeps its files in two directories: the entry point (`main`) is `lib/index.js`, and
the compiled module is `dist/anvil.js` plus `dist/anvil.wasm`, and the script-tag bundle is `dist/anvil.browser.js`. Earlier downloads were flat (`index.js`, `anvil.js` and `anvil.wasm` at the top), so `require('anvil-wasm')`
is unaffected, but anything that referred to those files by path must now use `lib/` and `dist/`. In a browser
with a bundler, import `anvil-wasm` as above and keep `dist/anvil.wasm` beside `dist/anvil.js`: the
module fetches it at startup.

### In a browser without a bundler: `anvil.browser.js`

Load the prebuilt bundle with a `<script>` tag. It needs no bundler and no `require`, and it defines
`window.anvl`, which is exactly the `require('anvil-wasm')` API (`ready`, `parse`, `parseRawValue`,
`lastError`, `getVersion`, `AnvlNode`, `AnvilWriter`, `AnvilBuilder`, `AnvilDialect`, `Schema`,
`TypeRegistry` and the error classes):

```html
<script src="/vendor/anvil/anvil.browser.js"></script>
<script>
  anvl.ready().then(() => {
    const doc = anvl.parse('#!aml\nanswer := 42;\n');
    console.log(doc.get('answer').asInt());   // 42
  });
</script>
```

`anvil.wasm` stays a separate file, so your server can send it as `application/wasm` and a service
worker can cache it. By default `ready()` fetches it from **next to the script**: serve both files
from one directory, or say where the `.wasm` is:

```js
await anvl.ready({ wasmUrl: '/assets/anvil.wasm' });
// or Emscripten's own hook, called with 'anvil.wasm' and the script's directory:
await anvl.ready({ locateFile: (file, scriptDirectory) => `/assets/${file}` });
```

`ready(options)` takes `{ wasmUrl }` or `{ locateFile }`, not both (a `TypeError`). The options apply
to the first call; later calls return the same load, and a load that fails (offline, a 404) can be
retried by calling `ready()` again. The CommonJS entry accepts the same options (`wasmUrl` is then a
file path or URL).

**Serving.** Send the `.wasm` as `Content-Type: application/wasm`; the browser's streaming compile
needs it, and without it Emscripten falls back to a slower path.

**Service worker.** To start offline, precache both files from the same URLs the page uses:

```js
// sw.js
const ASSETS = ['/vendor/anvil/anvil.browser.js', '/vendor/anvil/anvil.wasm'];
self.addEventListener('install', (e) => e.waitUntil(caches.open('anvil-v1').then((c) => c.addAll(ASSETS))));
self.addEventListener('fetch', (e) => e.respondWith(caches.match(e.request).then((r) => r || fetch(e.request))));
```

Bump the cache name when you update the package, so the script and the `.wasm` always come from the
same build.

npm does not refresh a `file:` or tarball dependency whose version has not changed. When you
reinstall a new build of the same version, delete `node_modules/anvil-wasm` first, then install.

### One real difference from Node: loading is async

Instantiating a WASM module takes a moment, so you pay that cost once, explicitly, before doing
anything else:

```js
const anvil = require('anvil-wasm');

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
the error object, attributes, the type registry and schema, the writer and builder) is identical
to the Node binding's own API — see [Node.js](Bindings-Guide.md#nodejs) below for the full
reference. The differences are these:

- **`await anvil.ready()` first**, once. Everything is exported from the same package:
  `anvil.Schema`, `anvil.TypeRegistry`, `anvil.AnvilWriter`, `anvil.AnvilBuilder` and the enums.
- **An in-memory filesystem for includes.** A schema or type file that `include`s another file
  reads the module's own virtual filesystem, which you fill: `anvil.writeFile(path, textOrBytes)`
  (parent directories are created) and `anvil.removeFile(path)` (a missing file is a no-op).
  `loadFile` and `fromIncludesFile` resolve includes beside the file; the text loaders (`load`,
  `fromIncludes`) resolve them against the filesystem root, `/`.

  ```js
  anvil.writeFile('/schemas/types.anvl', typesText);
  const schema = anvil.Schema.loadFile('/schemas/asset.anvl');
  ```
- **Bytes are a `Uint8Array`**, not a `Buffer`: `finishBytes()`, `emitBytes()` and `writer.data`
  return one, and `blob()` accepts any typed array or a string.
- **Handles are WebAssembly heap memory.** `dispose()` frees one. A `FinalizationRegistry` frees an
  abandoned one when the garbage collector runs, but nothing else reclaims it, so call `dispose()`
  in a loop that creates many.
- **Text is passed with its byte length**, so a NUL character inside a string or blob is data and
  nothing after it is lost. Names, paths and identifiers reject an embedded NUL with a `RangeError`.

## Node.js

**[Download anvil-node-v0.11.0-rc-linux-x86_64.tar.gz](/assets/downloads/anvil-node-v0.11.0-rc-linux-x86_64.tar.gz)**
— a prebuilt N-API addon (native addon, not a from-scratch JS parser) plus its JS-facing
wrapper. Installing it needs no compiler and no build step.

### Requirements

- Node.js with N-API support (any current LTS). Built and tested against Node 22.x — N-API's
  own ABI-stability guarantee means other recent majors should work too, but that hasn't been
  verified against older ones.
- Linux x86_64 (glibc) only today, same as the [native library](Bindings-Guide.md#native-library). Other
  platforms build from source (`npm install && npm run build`, needs a C2x compiler and Python 3
  for `node-gyp`) — not yet published to a package registry, so building means pointing at a
  real checkout, not `npm install anvil-node`.
- No runtime dependencies once built.

```sh
npm install ./anvil-node-v0.11.0-rc-linux-x86_64.tar.gz
```

npm does not refresh a `file:` or tarball dependency whose version has not changed. When you
reinstall a new build of the same version, delete `node_modules/anvil-node` first, then install;
otherwise npm keeps the old copy.

```js
const anvil = require('anvil-node');

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
| `.asInt()` | `number \| bigint \| null` | `null` unless the value is a number. A safe integer is a `number`; an integer past 2^53 is the exact `bigint`; a float literal is the truncated value. |
| `.numericKind` | `'integer' \| 'float' \| null` | The literal's spelling: no `.` and no exponent is `'integer'`, anything else is `'float'`, even when its value is integral. `null` for a non-number. |
| `.asNumber()` | `number \| null` | The double a float literal denotes (`Infinity` if it overflows); the value of a safe integer; `null` for an integer past 2^53 (it has no exact `number`) and for a non-number. |
| `.asBigInt()` | `bigint \| null` | The exact value of an integer literal of any size; `null` for a float literal and for a non-number. |

`2020` and `2020.0` are the same number but different literals, and `numericKind` tells them apart:
`2020` is `'integer'` (`asNumber()` `2020`, `asBigInt()` `2020n`), `2020.0` is `'float'`
(`asNumber()` `2020`, `asBigInt()` `null`), and `9007199254740993` is `'integer'` with `asNumber()`
`null` and `asBigInt()` `9007199254740993n`. An integer field can reject `2020.7` with
`Number.isInteger(node.asNumber())` and `2020.0` with `node.numericKind === 'float'`.

**Integers beyond 2^53.** An integer literal inside the safe range is a `number`. One outside it is
an exact `BigInt`, never a rounded `number`: `parseRawValue('9223372036854775807')` is
`9223372036854775807n`, and `AnvilWriter.value()` writes any `bigint` as exact text, so what you write
reads back. Mixing a `BigInt` with a `number` in arithmetic throws a `TypeError`, so a value that used
to be silently rounded now fails loudly. An attribute's `decodedValue` follows the same rule.
`parseRawValue()` returns plain values, which carry no kind: `2020.0` is the number `2020` there.

**Number errors.** The exponent sign is optional (`1e3` is the float `1000`). A malformed number is an
error at every depth, fragments included: for `5.`, `-5.`, `5.e3`, `1e`, `1e+` and `1e-`, `parse()` and
`parseRawValue()` return `null` and `lastError()` says why. Neither throws.

**Attributes and statements**

| Member | Returns | Notes |
|---|---|---|
| `.hasAttribute(key)` | `boolean` | Root only — checks the document's own module-level `@[...]` attributes. |
| `.attributes` / `.findAttribute(key)` | `{ key, value, decodedValue }[]` / the matching one or `null` | Module attributes, on the root only; any other node's `attributes` is `[]`. |
| `.statement(name)` | `AnvlStatement \| null` | The first top-level statement with that name. |
| `.statements()` | `AnvlStatement[]` | An object value's statements in order, repeats included; `[]` for anything else. |

### Attributes: raw `value` and `decodedValue`

An `AnvlStatement` has `name`, `value` (an `AnvlNode`), `attributes` and `findAttribute(key)`. Each
attribute has two readings of the same text:

```js
const doc = anvil.parse('#!aml\n@[version="1.2"]\nticket @[type=bug, urgent] := { id := 7; };\n');

doc.findAttribute('version').value;          // '"1.2"'  raw source text, quotes kept
doc.findAttribute('version').decodedValue;   // '1.2'    escapes resolved, delimiters stripped, typed
const ticket = doc.statement('ticket');
ticket.findAttribute('type').value;          // 'bug'
ticket.findAttribute('urgent').value;        // null: a flag has no value
ticket.value.statements();                   // the object's statements, in order
```

`decodedValue` types the value: `n=42` is the number `42`, `b=true` is `true`, and a value the
scalar grammar does not accept whole (`range=1..5`) is the string `'1..5'`. It is `null` for a flag
and for an explicit `@[x=null]`; the raw `value` tells them apart (`null` for the flag, `'null'`
for the explicit one).

Whitespace inside an attribute list is free, newlines included, and a value never includes the
whitespace around it, so a multi-line `@[ ... ]` list is valid.

A repeated name inside an object is kept: for `{ a := 1; a := 2; b := 3; }`, `count` is 3,
`get('a')` returns the first, and `entries()` and `statements()` yield all three in order.

`parse()` and `parseRawValue()` throw a `TypeError` for a non-string argument.

### Type registry and schema

Both work over ANVL **text**, not JS objects (ANVL is not JSON; objects use `:=`):

```js
const { Schema, TypeRegistry, SchemaViolation } = require('anvil-node');

const schema = Schema.load(schemaText);         // text of a document carrying @[schema]
const result = schema.validate(dataText);       // { valid, violations: [{ category, field, message }] }
const body   = schema.validateValue('{ asset_id := 7; name := "widget"; }',
                                    { skipRequired: false, allowUnknown: false });
result.violations.some((v) => v.category === SchemaViolation.REQUIRED);
schema.diagnostics;                             // what loading reported about the schema itself
schema.dispose();

const reg = TypeRegistry.load(typesText);       // a document carrying @[types]
reg.find('VIN');                                // TypeDef | null
reg.resolve('String');                          // also answers for the built-in types
reg.diagnostics; reg.loadError; reg.dispose();
```

- **`validateValue(text, opts)`** checks an object value (a request body such as `{ ... }`) as if
  it were a document's top level. `skipRequired` skips required-presence (patch semantics) and
  `allowUnknown` ignores undeclared fields.
- **`validateField(docText, name, opts)`** parses the text, finds the top-level statement `name` and
  validates its object value in one call. Same result shape and options. An absent statement is
  one `REQUIRED` violation with an empty `field`. **`{ each: true }`** treats an array or tuple
  value as a batch (`set := [ {...}, ... ]`): every element is validated with the same options and
  its violations are reported at `[i]` and `[i].field`.
- **Introspection.** `schema.fields` and `schema.findField(name)` return read-only `SchemaField`
  snapshots with `name`, `kind`, `typeName` (as written, such as `'types.Id32'`), `required`,
  `nullable`, `integer` and, when declared, `size`, `min`, `max`, `minItems`, `maxItems`, `values`,
  `format` and `tag`. An undeclared constraint is `undefined`, never a default. `nested`,
  `findNested(name)`, `items` and `extras` (extension keys, each `{ name, value }`) cover the rest.
- **Fails closed on an unresolved type.** A field whose `type` cannot be resolved (a misspelled
  `types.Name`) is reported in `schema.diagnostics`, and every *present* value of it, at any depth, is a
  violation with category `SchemaViolation.UNRESOLVED_TYPE` (`10`). An explicit `null` fails too, and
  `skipRequired` and `allowUnknown` do not excuse it. An absent field is judged only by `required`.
  The same code is `UnresolvedType` in .NET and `UNRESOLVED_TYPE` in Python.
- **Failures return `null`** and set `lastError()`: not a `@[schema]` or `@[types]` document, or
  text that does not parse.
- **Includes.** `Schema.loadFile(path)` resolves a document's `include`s relative to that file; the
  text forms resolve them against the process's working directory.
- **Lifetime.** A schema or registry is a self-contained copy, living until `dispose()` (or garbage
  collection). Using a disposed handle throws, and a second `dispose()` is a no-op. Violations,
  diagnostics and fields are plain snapshots. Call `dispose()` in a tight synchronous loop.
- **Limits.** `min` and `max` are JS numbers, so a bound past 2^53 loses precision, and one that
  overflows a 64-bit integer or a double is `undefined`.

The `iso8601_datetime` format is strict RFC 3339 (a `T` separator, and `Z` or a full `+hh:mm`
offset); see the [types reference](types-reference.md#formats).

### Writing: `AnvilWriter` and `AnvilBuilder`

```js
const { AnvilWriter, AnvilBuilder, AnvilDialect } = require('anvil-node');

const w = AnvilWriter.create(AnvilDialect.AMP);   // streaming: forward-only, chainable
const text = w.statement('status').bare('ok')
              .statement('code').value(200)
              .statement('tags').beginArray().bare('a').bare('b').endArray()
              .finish();
w.dispose();

const b = AnvilBuilder.create(AnvilDialect.AML);  // builder: attach in any order, re-emit at will
const server = b.object();
server.add('host', b.bare('localhost'));
b.add('server', server).attribute('env', 'production');
const text2 = b.emit();
b.dispose();
```

- `value(x)` takes `null`, a boolean, a number or a `bigint` (a safe integer or a `bigint` is
  written exactly). A string is ambiguous, so `value('x')` throws a `TypeError`: use `string()`
  (quoted) or `bare()` (unquoted). `numeric(text)` takes ANVL numeric text, and `blob(tag, data)`
  takes bytes or a string.
- A rejected call throws `AnvilWriterError` with a `.code`. The writer's first error is sticky; the
  builder's are not (a failed call changes nothing).
- `finish()` / `emit()` return a string, and `finishBytes()` / `emitBytes()` a `Buffer`.
- The writer rejects a repeated top-level name, as the reader does.

**Blobs.** The writer is byte-exact, but a backtick inside blob content is rejected, because a
blob has no escape for its delimiter. Node cannot yet read non-UTF-8 blob bytes back: the loader
takes a JavaScript string, so such a blob reads back lossy. Also, a parsed blob's tag is not
exposed yet in Node.js or WebAssembly (the Python and .NET bindings have `blob_tag()` and
`BlobTag()`).

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

Returns the underlying Anvil Native version string — `"0.11.0-rc+316"` for the prebuilt download,
plain `"0.11.0-rc"` for a source install with no build number (see
[the version string](Bindings-Guide.md#the-version-string)). A toolchain sanity check, not a
compatibility contract to branch logic on.

## .NET

**[Download anvil-net-v0.12.0-rc-linux-x64.tar.gz](/assets/downloads/anvil-net-v0.12.0-rc-linux-x64.tar.gz)**
— a prebuilt managed assembly plus the native library it links against. The tarball holds
`lib/Anvil.Net.dll`, `lib/libanvil.so`, a README and the licence. **There is no NuGet package**:
nothing is published to nuget.org, and the tarball contains no `.nupkg`. Reference the assembly
directly and copy `libanvil.so` beside your own output (the native library is loaded from the
assembly's directory):

```xml
<ItemGroup>
  <Reference Include="Anvil.Net"><HintPath>path/to/lib/Anvil.Net.dll</HintPath></Reference>
  <None Include="path/to/lib/libanvil.so" Link="libanvil.so" CopyToOutputDirectory="PreserveNewest" />
</ItemGroup>
```

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
  `vendor/anvil`, then `dotnet build`) — not yet published to NuGet, so building means pointing at
  a real checkout, not `dotnet add package`. A source build reports its version without a build
  number unless you pass one (see [the version string](Bindings-Guide.md#the-version-string)).
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
| `.Type` | `AnvilValueType` | `Null \| Bool \| Numeric \| String \| Blob \| Bare \| Array \| Tuple \| Object` — finer-grained than the Node/WASM bindings' `.type`, which collapses several of these into one `'scalar'` kind. |
| `.Count` | `int` | Field count for an object, element count for an array/tuple, `0` otherwise. |
| `this[int index]` | `AnvlValue?` | Array/tuple element by position; `null` if out of bounds or not array/tuple-shaped. |
| `this[string key]` | `AnvlValue?` | Object field's value directly, not the statement. `null` if `key` isn't present or this isn't object-shaped. |
| `.Has(key)` | `bool` | Shares the same lazy per-key cache as the indexer above — a repeated lookup of the same key on the same instance costs one native call total, not one per access. |
| `.Entries()` | `IEnumerable<KeyValuePair<string, AnvlValue>>` | Declaration order. Empty for a non-object value. |
| `.AsString()` / `.AsBool()` | `string?` / `bool?` | Each `null` unless `.Type` matches (`String` or `Bare` / `Bool`) — a bare, unquoted literal like `host := localhost;` reads as a string. |
| `.AsInt()` / `.AsLong()` | `int?` / `long?` | `Numeric` only. **Exact or `null` — never a wrapped or saturated number** (`9007199254740993` is exact through `AsLong()`; `AsInt()` is `null` because it doesn't fit). A decimal/exponent literal truncates toward zero. |
| `.AsBigInteger()` | `BigInteger?` | `Numeric` only. Exact for an integral literal of *any* size; `null` only when the value overflows a double (`1e+999`). |
| `.AsDouble()` / `.AsNumericText()` / `.IsIntegral()` | `double?` / `string?` / `bool?` | `Numeric` only: the nearest double, the exact source text (`"007"`, `"1e+3"`, a bignum), and whether it was authored as an integer (`5` vs `5.0`). |
| `.AsBytes()` / `.BlobTag()` | `byte[]?` / `string?` | `Blob` only: its exact content, and its `@tag` (`null` when untagged). Blob content is arbitrary bytes, so it is not `.AsString()`. |
| `.RawBytes()` | `byte[]?` | The exact, uninterpreted source span of **any scalar** — a string's escapes stay as written. `null` for an array, tuple or object. |

### `AnvlStatement` and `AnvilAttribute`

| Member | Returns | Notes |
|---|---|---|
| `AnvlStatement.Name` / `.Value` | `string` / `AnvlValue?` | The declaration's name and the value it's assigned. |
| `AnvlStatement.Attributes` / `.FindAttribute(key)` | `IReadOnlyList<AnvilAttribute>` / `AnvilAttribute?` | This statement's own `@[...]` attributes. |
| `AnvilAttribute.Key` / `.Value` / `.DecodedValue` | `string` / `string?` / `AnvlValue?` | `.Value` is the raw source text (`status="done"` is `"done"` with its quotes) and is `null` for a flag attribute (`@[active]`). `.DecodedValue` reads the same attribute as a value: escapes resolved, delimiters stripped, typed (`status="done"` is a `String` `done`, `n=42` a `Numeric`, `type=bug` a `Bare`). It is `null` for a flag; an explicit `@[x=null]` is a `Null` value. |

Whitespace inside an attribute list is free, newlines included, and a value never includes the
whitespace around it.

### Type registry and schema

```csharp
using var schemaDoc = AnvilDocument.Load("asset.schema.anvl");   // carries @[schema]; may include @[types]
using var schema = AnvilSchema.Load(schemaDoc)!;                  // null if it has no @[schema]

using var data = AnvilDocument.LoadBuffer("#!aml\nname := \"widget\";\nstatus := scrapped;\n")!;
AnvilSchemaValidation result = schema.Validate(data);             // IsValid + Violations
foreach (AnvilSchemaViolation v in result.Violations)             // Code, Field, Message
    Console.WriteLine($"{v.Code} at {v.Field}: {v.Message}");

// A request body's `set` object, without loading a second document:
using var fragment = AnvilDocument.ParseValueFragment("{ name := \"widget\"; }")!;
schema.ValidateValue(fragment.FragmentValue!, new AnvilValidationOptions(SkipRequired: true));

// ...or straight from document text:
schema.ValidateField("#!aml\nset := { name := \"widget\"; };\n", "set");

using var typesDoc = AnvilDocument.Load("asset.types.anvl")!;
using var registry = AnvilTypeRegistry.Load(typesDoc)!;           // or FromIncludes(doc)
AnvilTypeDef year = registry.Find("AssetYear")!;
year.TryGetMin(out long min);                                     // false when not declared, never a default
AnvilNumeric? bound = year.Max;                                   // exact, including fractional or overflowing bounds
```

- A schema and a registry are self-contained copies: they outlive the document they were loaded
  from, and are `IDisposable`. Their views (`AnvilTypeDef`, `AnvilSchemaField`) throw
  `ObjectDisposedException` once the owner is disposed.
- `ValidateField(text, name, options)` parses the text, finds the top-level statement and
  validates its object value. An absent statement is one `Required` violation with an empty
  `Field`; text that does not parse is a `FormatException`. To validate a batch
  (`set := [ {...}, ... ]`), iterate the array and call `ValidateValue` on each element.
- Introspection: `schema.Fields` / `FindField` give each rule's `Kind`, `TypeName`, `IsRequired`,
  `IsNullable`, `IsInteger`, `Min` / `Max`, `TryGetMinItems` / `TryGetMaxItems`, `Values`, `Format`,
  `Tag`, `Nested`, `Items` and `Extras`.
- `registry.Diagnostics` and `schema.Diagnostics` are what loading reported; whether any is fatal
  is your call.

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

### Writing: `AnvilWriter` and `AnvilBuilder`

```csharp
using var w = AnvilWriter.Create(AnvilDialect.Aml);        // streaming: forward-only, chainable
string text = w.Statement("server").BeginObject()
    .Statement("host").Bare("localhost")
    .Statement("port").Value(8080L)
    .EndObject().Finish();

using var b = AnvilBuilder.Create(AnvilDialect.Aml);       // builder: attach in any order, re-emit at will
AnvilNode server = b.Object();
server.Add("host", b.Bare("localhost"));
b.Add("server", server).Attribute("env", "production");
string text2 = b.Emit();
```

| Member | Notes |
|---|---|
| `AnvilWriter.Create(dialect)` / `AnvilBuilder.Create(dialect)` | `AnvilDialect.Aml` or `.Amp`. `IDisposable`; `Dispose()` is idempotent and use afterwards throws `ObjectDisposedException`. |
| `.Statement(name, base?)`, `.Attribute(key, value?)`, `.Include(path)` | Writer header/statement calls (`AttributeString` for a quoted value). |
| `.Value(x)` | `bool`, `long`, `ulong`, `BigInteger` or `double` — an integer past `long` is written exactly. Text is `.String()` (quoted) or `.Bare()`; bytes are `.Blob(tag, data)`; a reference is `.VarRef(name)`. |
| `.BeginArray()/.EndArray()`, `.BeginTuple()/.EndTuple()`, `.BeginObject()/.EndObject()` | Writer collections. |
| `.Finish()` / `.FinishBytes()` | Writer output as `string` / exact `byte[]` (use bytes for non-UTF-8 blob content, and read it back with `AnvilDocument.LoadBuffer(byte[])`). |
| `AnvilBuilder.Add(name, node)` / `.Find(name)`, `AnvilNode.Append(el)` / `.Add(name, node)`, `AnvilMember.SetBase(...)` / `.Attribute(...)` | Builder structure. Values are created detached (`.Object()`, `.Array()`, `.Tuple()`, `.String()`, …) and attached exactly once. `.Emit()` / `.EmitBytes()` / `.Write(writer)` produce output. |
| `AnvilWriterException.Code` | An `AnvilWriterErrorCode`. The writer's first error is sticky; the builder's are not (a failed call changes nothing). A null argument throws `ArgumentNullException`, an embedded NUL or a node from another builder `ArgumentException`, before reaching native code. |

### `AnvilDocument.GetVersion()`

Returns the underlying Anvil Native version string (`"0.12.0-rc+389"` for the prebuilt download) —
same toolchain sanity check as the Node/WASM bindings' `getVersion()`, not a compatibility
contract.

## Python

**[Download anvil-py-v0.12.0-rc-linux-x64.tar.gz](/assets/downloads/anvil-py-v0.12.0-rc-linux-x64.tar.gz)**
— a pure-`ctypes` binding, no compiled extension module, plus the native library it links
against (`libanvil.so`, inside the package at `src/anvil/runtimes/linux-x64/native/`).

Extract it and put the `src` directory on `PYTHONPATH`:

```sh
tar xzf anvil-py-v0.12.0-rc-linux-x64.tar.gz
export PYTHONPATH="$PWD/anvil-py-v0.12.0-rc-linux-x64/src:$PYTHONPATH"
python3 -c "from anvil import AnvilDocument; print('ok')"
```

The package lives under `src/` (earlier downloads had `anvil/` at the top), so point
`PYTHONPATH` at `src`, not at the extracted directory itself.

Same design as the .NET binding above, translated to Python's own idiom rather than copied
verbatim: real, lazy, on-demand navigation through Anvil Native's vtable ABI (`anvil_vtable.h`),
resolved once via `ctypes`' `in_dll` (the same mechanism P/Invoke uses on the .NET side), not a
JSON-blob port. Where C# organizes the vtable groups as nested static classes, this binding uses
one real Python submodule per group (`_native/document.py`, `_native/value.py`, ...) — the
idiomatic Python equivalent (matching how `os.path`/`urllib.parse` are themselves organized),
not a synthetic namespace object or a class used purely for grouping.

### Requirements

- CPython 3.11 or later. No dependency beyond the standard library — `ctypes` ships with every
  install.
- Linux x64 (glibc) only today. Other platforms build from source (`make so-release` in
  `vendor/anvil`) — not yet published to PyPI, so building means pointing at a real checkout, not
  `pip install anvil-py`. The package has no packaging metadata, so its version is only what the
  native library reports.
- No other runtime dependencies once built.

```python
from anvil import AnvilDocument, ValueType

with AnvilDocument.load("config.anvl") as doc:
    if doc is None or doc.has_errors:
        err = doc.error if doc else None
        print(f"Failed: {err.message if err else 'allocation failed'}")

    server = doc.find_value("server")
    if server is not None and server.type == ValueType.OBJECT:
        host = server["host"].as_string() if server["host"] else None
        port = server["port"].as_int() if server["port"] else None

    for stmt in doc.statements:
        print(f"{stmt.name}: {stmt.value.type}")
```

Everything that needs `doc` stays inside the `with` block — Python's `with` disposes exactly at
block-exit (unlike C#'s `using var` declaration, which lives until the end of the enclosing
scope), so `doc.statements` (or anything else touching `doc`) called after the block has already
been disposed and raises `RuntimeError`.

`load`/`load_buffer`/`parse_value_fragment` never throw on a syntax error — they return a
document whose `.has_errors` is `True`. Python's `with` statement is this binding's translation of
the .NET binding's `using`/`IDisposable` — both are deterministic (cleanup runs exactly at
block-exit), unlike `__del__`'s own GC-finalizer timing, which has the identical non-determinism
problem that ruled out a live-handle design for the Node/WASM bindings.

### `AnvilDocument` — the one disposable root

| Member | Returns | Notes |
|---|---|---|
| `AnvilDocument.load(path)` | `AnvilDocument \| None` | `None` only on a hard I/O failure (missing file); a syntax error still returns a document with `.has_errors == True`. |
| `AnvilDocument.load_buffer(source)` | `AnvilDocument \| None` | Same contract, from an in-memory string. |
| `AnvilDocument.parse_value_fragment(text)` | `AnvilDocument \| None` | Parses a single, standalone value expression — see `.fragment_value` below. |
| `.has_errors` / `.error_category` / `.error` | `bool` / `ErrorCode` / `AnvilError \| None` | `.error` gives message/line/column; `.error_category` alone is cheaper when you only need to branch on the failure kind. |
| `.find_statement(name)` | `AnvlStatement \| None` | Root-level lookup by name — the whole declaration (name, value, its own `@[...]` attributes). |
| `.find_value(name)` | `AnvlValue \| None` | Convenience one-hop equivalent of `find_statement(name).value`. |
| `.statements` / `.includes` | generator of `AnvlStatement` / `AnvilDocument` | Declaration order. Each yielded `.includes` document is independently usable as its own context manager. |
| `.attributes` / `.find_attribute(key)` | document-level `@[...]` attributes, same shape as a statement's own (below). |

### `AnvlValue` — type inspection and navigation

| Member | Returns | Notes |
|---|---|---|
| `.type` | `ValueType` | `NULL \| BOOL \| NUMERIC \| STRING \| BLOB \| BARE \| ARRAY \| TUPLE \| OBJECT` — same finer-grained kinds as the .NET binding, more granular than the Node/WASM bindings' `.type`. |
| `.count` | `int` | Field count for an object, element count for an array/tuple, `0` otherwise. |
| `value[i]` (int) | `AnvlValue \| None` | Array/tuple element by position; `None` if out of bounds or not array/tuple-shaped. |
| `value[key]` (str) | `AnvlValue \| None` | Object field's value directly, not the statement. `None` if `key` isn't present or this isn't object-shaped. |
| `.has(key)` | `bool` | Shares the same lazy per-key cache as the indexer above — a repeated lookup of the same key on the same instance costs one native call total, not one per access. |
| `.entries()` | generator of `(str, AnvlValue)` | Declaration order. Empty for a non-object value. |
| `.as_string()` / `.as_bool()` | `str \| None` / `bool \| None` | Each `None` unless `.type` matches (`STRING` or `BARE` / `BOOL`) — a bare, unquoted literal like `host := localhost;` reads as a string. |
| `.as_int()` | `int \| None` | `NUMERIC` only. **Exact for every integral literal** (past int64 too — Python ints are unbounded); a decimal/exponent literal truncates toward zero; `None` when it overflows a double (`1e+999`). |
| `.as_float()` / `.numeric_text()` / `.is_integral()` | `float \| None` / `str \| None` / `bool \| None` | `NUMERIC` only: the nearest float, the exact source text (`"007"`, `"1e+3"`, a bignum), and whether it was authored as an integer (`5` vs `5.0`). |
| `.as_bytes()` / `.blob_tag()` | `bytes \| None` / `str \| None` | `BLOB` only: its exact content, and its `@tag` (`None` when untagged). Blob content is arbitrary bytes, so it is not `.as_string()`. |
| `.raw_bytes()` | `bytes \| None` | The exact, uninterpreted source span of **any scalar** — a string's escapes stay as written. `None` for an array, tuple or object. |

### `AnvlStatement` and `AnvilAttribute`

| Member | Returns | Notes |
|---|---|---|
| `AnvlStatement.name` / `.value` | `str` / `AnvlValue \| None` | The declaration's name and the value it's assigned. |
| `AnvlStatement.attributes` / `.find_attribute(key)` | `list[AnvilAttribute]` / `AnvilAttribute \| None` | This statement's own `@[...]` attributes. |
| `AnvilAttribute.key` / `.value` / `.decoded_value` | `str` / `str \| None` / `AnvlValue \| None` | `.value` is the raw source text (`status="done"` is `'"done"'`, quotes kept) and is `None` for a flag attribute (`@[active]`). `.decoded_value` reads the same attribute as a value: escapes resolved, delimiters stripped, typed (`status="done"` is a `STRING` `done`, `n=42` a `NUMERIC`, `type=bug` a `BARE`). It is `None` for a flag; an explicit `@[x=null]` is a `NULL` value. |

Whitespace inside an attribute list is free, newlines included, and a value never includes the
whitespace around it.

### Type registry and schema

```python
from anvil import AnvilDocument, AnvilSchema, AnvilTypeRegistry

with AnvilDocument.load("asset.schema.anvl") as schema_doc:     # carries @[schema]; may include @[types]
    schema = AnvilSchema.load(schema_doc)                         # None if it has no @[schema]

with schema, AnvilDocument.load_buffer('#!aml\nname := "widget";\nstatus := scrapped;\n') as data:
    result = schema.validate(data)                                # is_valid + violations
    for v in result.violations:                                   # code, field, message
        print(v.code.name, v.field, v.message)

    # A request body's `set` object, without loading a second document:
    with AnvilDocument.parse_value_fragment('{ name := "widget"; }') as fragment:
        schema.validate_value(fragment.fragment_value, skip_required=True)

    # ...or straight from document text:
    schema.validate_field('#!aml\nset := { name := "widget"; };\n', "set")

with AnvilDocument.load("asset.types.anvl") as types_doc, AnvilTypeRegistry.load(types_doc) as registry:
    year = registry.find("AssetYear")
    year.min, year.max                                            # int, float or None (None when not declared)
```

- Options are keyword-only (`skip_required=`, `allow_unknown=`).
- `validate_field(document, name)` takes ANVL text, bytes or an `AnvilDocument`, and validates the
  named top-level statement's object value. An absent statement is one `REQUIRED` violation with an
  empty `field`; text that does not parse is a `ValueError`. Validate a batch by iterating the array
  and calling `validate_value` on each element.
- Absent constraints are `None`, never a default. `min_numeric` / `max_numeric` give the exact
  `AnvilNumeric`, including an `overflowed` flag.
- Introspection: `schema.fields` / `find_field(name)` give each rule's `kind`, `type_name`,
  `is_required`, `is_nullable`, `is_integer`, the bounds, `values`, `nested`, `items` and `extras`.
- `registry.diagnostics` and `schema.diagnostics` are what loading reported; whether any is fatal is
  your call. Both registry and schema are context managers and outlive the document they came from.

### `parse_value_fragment` / `.fragment_value` — for when you already know the shape

```python
with AnvilDocument.parse_value_fragment("[(1,2,3),(4,5,6)]") as frag:
    rows = frag.fragment_value
    if rows is not None and rows.type == ValueType.ARRAY:
        first = rows[0][0].as_int()  # 1 -- rows[0] is the Tuple, [0] its first element
```

### Writing: `AnvilWriter` and `AnvilBuilder`

```python
from anvil import AnvilBuilder, AnvilDialect, AnvilWriter

with AnvilWriter.create(AnvilDialect.AML) as w:          # streaming: forward-only, chainable
    text = (w.statement("server").begin_object()
             .statement("host").bare("localhost")
             .statement("port").value(8080)
             .end_object().finish())

with AnvilBuilder.create(AnvilDialect.AML) as b:         # builder: attach in any order, re-emit at will
    server = b.object()
    server.add("host", b.bare("localhost"))
    b.add("server", server).attribute("env", "production")
    text = b.emit()
```

| Member | Notes |
|---|---|
| `AnvilWriter.create(dialect)` / `AnvilBuilder.create(dialect)` | `AnvilDialect.AML` or `.AMP`. Context managers; `.dispose()` is idempotent and use afterwards raises `RuntimeError`. |
| `.statement(name, base=None)`, `.attribute(key, value=None)`, `.include(path)` | Writer header/statement calls (`attribute_string` for a quoted value). |
| `.value(x)` | `None`, `bool`, `int` or `float` — an int past int64 is written exactly. Text is `.string()` (quoted) or `.bare()`; bytes are `.blob(tag, data)`; a reference is `.varref(name)`. |
| `.begin_array()/.end_array()`, `.begin_tuple()/.end_tuple()`, `.begin_object()/.end_object()` | Writer collections. |
| `.finish()` / `.finish_bytes()` | Writer output as `str` / exact `bytes` (use bytes for non-UTF-8 blob content, and read it back with `AnvilDocument.load_buffer(bytes)`). |
| `AnvilBuilder.add(name, node)` / `.find(name)`, `AnvilNode.append(el)` / `.add(name, node)`, `AnvilMember.set_base(...)` / `.attribute(...)` | Builder structure. Values are created detached and attached exactly once. `.emit()` / `.emit_bytes()` / `.write(writer)` produce output. |
| `AnvilWriterError.code` | A `WriterErrorCode`. The writer's first error is sticky; the builder's are not (a failed call changes nothing). Wrong argument types raise `TypeError`, an embedded NUL `ValueError`, before reaching native code. |

### `AnvilDocument.get_version()`

Returns the underlying Anvil Native version string (`"0.12.0-rc+389"` for the prebuilt download) —
same toolchain sanity check as every other binding's `getVersion()`/`GetVersion()`, not a compatibility contract.
