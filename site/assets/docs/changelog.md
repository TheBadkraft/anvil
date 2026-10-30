# Changelog

What changed in each release of Anvil, for people using it. Downloads are on the
[download page](/download).

## v0.12.0-rc

Released 2026-10-30. This is a release candidate.

v0.12.0-rc makes the grammar consistent, so several inputs that used to load are now errors. It also
introduces **anvil.js**, a pure-JavaScript ANVL reader that replaces the Node and WebAssembly builds going
forward.

### Downloads

The native downloads (core, Python and .NET) are Linux x86_64 (glibc). anvil.js is plain JavaScript: it
runs in Node 18 or later and in any browser. Each native file is built from the same source tag, and the
library inside reports exactly `0.12.0-rc+389`; anvil.js reports the same string from `getVersion()`.

| Download | What it is |
|---|---|
| `anvil-v0.12.0-rc-linux-x86_64.tar.gz` | The C library: `libanvil.a`, `libanvil.so` and every public header |
| `anvil.js-v0.12.0-rc.tar.gz` | The pure-JavaScript reader for Node and browsers (`require('anvil.js')`, plus `dist/anvil.mjs` and `dist/anvil.global.js` for a browser) |
| `anvil-py-v0.12.0-rc-linux-x64.tar.gz` | Python package (CPython 3.11+, standard library only) |
| `anvil-net-v0.12.0-rc-linux-x64.tar.gz` | .NET 9 assembly (`Anvil.Net.dll`) with its native library |

Checksums are in [`SHA256SUMS`](/assets/downloads/SHA256SUMS), which lists every published download, this release and earlier ones
(`SHA256SUMS-v0.12.0-rc` lists this release only):

```
bbdfba42f6bb4b1cd2195eef4915713f007da5ce53d9bf096fdc4992edc93472  anvil.js-v0.12.0-rc.tar.gz
3f665f841a9c6c17e53fc4ff405f680d5757470cb3a94c37aaae353896191653  anvil-net-v0.12.0-rc-linux-x64.tar.gz
39bb99217acbb7262ebe88511b2f9766794782770c247ebc71882ed90f8a9b41  anvil-py-v0.12.0-rc-linux-x64.tar.gz
e695abf19d8a16dc71bcffb6346f8e28843eacf14cd3d6561cbd07f69dab2bcf  anvil-v0.12.0-rc-linux-x86_64.tar.gz
fee2cc4abb5fb72e162735ba72edeef450610b650dec0b02c11d3548c27c07f6  anvil-net-v0.11.0-rc-linux-x64.tar.gz
2cf2f5120318d2c0307c378c4a951f89107358e5c932cdb3bfbf511fc45f24f5  anvil-node-v0.11.0-rc-linux-x86_64.tar.gz
0c5a14bcd117d15fef9bd0a5dd00467fac9786e69d7c0999edcf5d0eec89a952  anvil-py-v0.11.0-rc-linux-x64.tar.gz
1d8ae18e21f150b979e751a0d43cfe4d88931c5a5a79fe0c8e1de0ec35874794  anvil-v0.11.0-rc-linux-x86_64.tar.gz
17ca4e8c3433178b86e2d08bf215a914564f5c207691c59cbc4378e0c0498339  anvil-wasm-v0.11.0-rc.tar.gz
ecaffbd1fc88b13b5a51164d320d1cbc8d38c6981f0890ab3c685d054f91c470  anvil-net-v0.10.0-rc-linux-x64.tar.gz
0f4990601ee4d92dc74284ab96c1fc802e213df336609b7c7bfebc0a8d716ccc  anvil-node-v0.10.0-rc-linux-x86_64.tar.gz
de6596406ad838a532bc65332b83d79efa3ff6d7145f0c951f5dd55b77dfab99  anvil-py-v0.10.0-rc-linux-x64.tar.gz
32e52f3979659ecc56f652e5cedc739856ea0a90a60bb66558c13380b4c45880  anvil-v0.10.0-rc-linux-x86_64.tar.gz
10eb6cc989d0a9615c951a941602d148a02caee76bb25c8168e3fe85021047d9  anvil-wasm-v0.10.0-rc.tar.gz
```

The Java binding (`anvil.java`) is not distributed.

### anvil.js replaces the Node and WebAssembly builds

`anvil.js` is a new, separate package, written in JavaScript with no native code and no WebAssembly. It has
the same reader API as `anvil-node` (`parse`, `parseRawValue`, `lastError`, `getVersion`, nodes and
statements, `asInt`, `asBigInt`, `numericKind` and the rest) and reads every document the same way ANVL
Native does: it passes all 1,867 cases of the shared conformance corpus. It also offers `isNull()`, the
blob tag, an `implementation` property (`'anvil.js'`) and a `BUILD_INFO.json`.

- In Node: `const anvl = require('anvil.js')`. An `include` resolves against the working directory. In an
  ES module, `import anvl from 'anvil.js'` gives the same object.
- In a browser, with no bundler: `dist/anvil.mjs` (an ES module) or `dist/anvil.global.js` (a script tag
  that defines `window.anvl`). Both were run in Chromium and Firefox before release.
- This release's anvil.js is a **reader**. Types, schema validation and the writer are not in it yet.
  Use the Python or .NET packages, or the C library, where you need those.

The Node (`anvil-node`) and WebAssembly (`anvil-wasm`) builds are not part of this release and receive no
further updates. Their v0.11.0-rc downloads stay available at their existing addresses, and nothing
about them changes.

### What changed (behaviour changes)

#### The grammar is consistent

- **Duplicate keys are errors everywhere.** A repeated object field is error 4001, and a repeated
  attribute key (on a statement, or across the module attributes) is error 4013. Both were silently
  kept before. A repeated top-level name is still 4053, and overriding an *inherited* field is still
  fine. The writer and builder refuse to produce a document with a repeated field or attribute key.
- **A base that is not an object is an error.** `derived : base` where `base` is a scalar, string, array,
  tuple, blob, bare word, reference or function used to load and merge nothing. It is now error 4020.
- **A keyword or reserved word is refused as an attribute key** (error 4004), as it already was for
  statement names and object fields. The writer and builder refuse the same names.
- **Some errors report a more specific code.** The same inputs are rejected at the same place, with a
  code that says why (for example a comma before `}` in an object is 3004, `@[]` is 2008, an
  unexpected control character is 5001). If you match on error codes, see the language reference.

#### Attribute values are scalars

- An attribute value must be a single scalar, bare or quoted: `@[status=done]`, `@[n=5]`,
  `@[on=true]` and quoted strings that contain brackets are fine. A value that starts with `[`, `(` or
  `{` is error 2006, wherever a comma or `]` falls (before, some of these loaded as bare text). A blob
  (a value starting with a backtick or `@`) is not an attribute value either. Put a blob in the
  statement's value.
- The writer and builder refuse the same attribute values.

#### Builder and references

- The builder refuses a base that names a non-object member of the same document when it writes or
  emits, with the same meaning as error 4020.
- A `$name` reference to a block-form statement (`a { x := 1; }; b := $a;`) now resolves.

### Performance

**anvil.js.** Measured on the packaged anvil.js against the v0.10.0-rc Node addon (median over six
alternating rounds, each reader in its own process, pinned to CPUs 4 and 5), as time taken (lower is faster):
anvil.js vs anvil-node v0.10.0-rc takes 0.09x to 0.17x of the time to parse, and 0.13x to 0.43x of the
time to parse and read every value, across nine workloads from 5,000 small statements to a 1 MB mixed
document. Against `JSON.parse` of the equivalent JSON (information only): anvil.js vs `JSON.parse` takes
0.31x to 1.53x of the time to parse, and 0.69x to 4.28x of the time to parse and read through the public API,
so JSON.parse is faster on large numeric tables.

**Native.** Measured on the packaged libraries (median of 9 interleaved runs, pinned to one CPU), native
v0.12.0-rc vs native v0.11.0-rc takes 1.005x to 1.012x of the time to parse (floats, integers and a mixed
document), and 0.98x of the time to validate (lower is faster). **Known cost:** native v0.12.0-rc vs native v0.11.0-rc: 1.027x–1.039x time on tiny-document loads through libanvil.so (lower is faster). It affects very many tiny documents loaded through the shared library (`libanvil.so`, and so the Python and .NET packages), measured on 20,000 loads of a five-line document, and comes from the new grammar checks. It is tracked for a later release.

### Licence

Every download now includes a `LICENSE.txt` with the new ANVL Software Licence terms, which are still in
progress: free for personal and educational use, free for commercial use by organizations with up to 3 developer
seats, and no redistribution. It replaces the earlier notice.

### The version string

The library reports its version as `0.12.0-rc+389`: the release, then the build number after a `+`.
This is valid semver. The build number is the same in every download of this release.

---

## v0.11.0-rc

Released 2026-10-30. This is a release candidate.

v0.11.0-rc changes behaviour in two places: the number grammar, and what a schema does with a field
whose type it cannot resolve. It also fixes `parse()` in browsers for the WebAssembly build and adds a
prebuilt browser bundle.

### Downloads

| Download | What it is |
|---|---|
| `anvil-v0.11.0-rc-linux-x86_64.tar.gz` | The C library: `libanvil.a`, `libanvil.so` and every public header |
| `anvil-node-v0.11.0-rc-linux-x86_64.tar.gz` | Node.js addon (N-API) |
| `anvil-wasm-v0.11.0-rc.tar.gz` | WebAssembly build for Node.js and the browser, with the browser bundle |
| `anvil-py-v0.11.0-rc-linux-x64.tar.gz` | Python package (CPython 3.11+, standard library only) |
| `anvil-net-v0.11.0-rc-linux-x64.tar.gz` | .NET 9 assembly (`Anvil.Net.dll`) with its native library |

The native downloads (C library, Node.js, Python and .NET) are Linux x86_64 (glibc). The
WebAssembly build runs on any WebAssembly host. Checksums are in
[`SHA256SUMS`](/assets/downloads/SHA256SUMS). The library reports `0.11.0-rc+316` in every download.
The Java binding is not distributed.

### Changed: number grammar

- `1e3` is now a number. The exponent sign is optional, so `1e3`, `1E3`, `1e+3`, `1e-3`, `-1e3` and
  `1.5e10` all read as numbers with their exact text. Before, `1e3` was a bare word. If you stored the
  bare word `1e5` as text, quote it (`"1e5"`) to keep it text.
- `5.`, `5.e3`, `1e`, `1e+` and `1e-` are errors, at every depth: top-level values, array, tuple and
  object members, value fragments and AnvilScript parameter defaults. A decimal point needs at least one
  digit after it, and an exponent needs digits. Words such as `007x`, `1ex` and `09-02-2026` are
  unaffected.
- The writer follows: it accepts an unsigned exponent, and refuses to write `1e5` as a bare word (it
  would read back as a number).

### Changed: schema validation fails closed

- A field whose `type` cannot be resolved no longer accepts anything. Loading such a schema still
  succeeds and still reports the unknown-type diagnostic, but validating a present value for that field,
  however deeply nested, now reports a violation with the new code `UNRESOLVED_TYPE` (10). An explicit
  `null` fails too, and `allow_unknown` and `skip_required` do not excuse it. An absent field is not a
  violation. A schema that used to validate a document through a misspelled type name can now reject it.
- The Node, WebAssembly, Python and .NET bindings expose the new code.

### Fixed and added: WebAssembly in browsers

- **Fixed:** in v0.10.0-rc, `parse()` and `parseRawValue()` failed in real Firefox and Chrome with a
  `TextDecoder` error, although `ready()` and `getVersion()` worked.
- **New:** a prebuilt browser bundle, `dist/anvil.browser.js`. A plain `<script>` tag loads it, with no
  bundler and no `require`, and it defines `window.anvl` with the same API as `require('anvil-wasm')`.
  `anvil.wasm` stays a separate file so it can be served as `application/wasm` and precached by a
  service worker. `ready({ wasmUrl })` or `ready({ locateFile })` says where the `.wasm` is; by default it
  is read from next to the script. The CommonJS entry takes the same options.

### Added: Node and WebAssembly numbers

- Integers beyond 2^53 are read exactly, as `BigInt`, instead of rounded.
- New `numericKind` (`'integer'`, `'float'` or `null`), `asNumber()` and `asBigInt()` on a parsed node.
  `2020` and `2020.0` can now be told apart.
- A trailing-dot number such as `5.` returns `null` with `lastError()` set. It never throws.

### Performance

Parsing and validation are at parity with v0.10.0-rc or better in every download. Measured on the
packaged Node and WebAssembly builds against v0.10.0-rc (median of 61 interleaved runs, pinned to one
CPU): parsing is within 1.02x on every workload, including documents of 3,000 and 10,000 floats
(Node 1.01x, WebAssembly 1.01x to 1.02x), and schema validation is faster (Node 0.90x, WebAssembly
0.72x of the time).

### Not in this release

- `isNull()` for Node and WebAssembly.
- The blob-tag accessor for Node and WebAssembly.

## v0.10.0-rc

Released 2026-10-26. This is a release candidate.

v0.10.0-rc is the first release to carry the whole of ANVL proper in every download: the core
parser, the opt-in type system, AnvilSchema, and the streaming writer and document builder.

### Replaces the earlier release candidate

The core, Node.js and WebAssembly downloads of the earlier release candidate were built before the AML keyword `import`
was renamed to `include`. They still read `import` and treat `include` as an ordinary identifier,
so a document that starts with `include "types.anvl";` fails with `Expected ':=' after
identifier`. Those downloads are withdrawn. If you installed one, replace it with the
`v0.10.0-rc` file.

### Downloads

| Download | What it is |
|---|---|
| `anvil-v0.10.0-rc-linux-x86_64.tar.gz` | The C library: `libanvil.a`, `libanvil.so` and every public header |
| `anvil-node-v0.10.0-rc-linux-x86_64.tar.gz` | Node.js addon (N-API) |
| `anvil-wasm-v0.10.0-rc.tar.gz` | WebAssembly build for Node.js and the browser |
| `anvil-py-v0.10.0-rc-linux-x64.tar.gz` | Python package (CPython 3.11+, standard library only) |
| `anvil-net-v0.10.0-rc-linux-x64.tar.gz` | .NET 9 assembly (`Anvil.Net.dll`) with its native library |

The native downloads (C library, Node.js, Python and .NET) are Linux x86_64 (glibc). The
WebAssembly build runs on any WebAssembly host. Checksums are in
[`SHA256SUMS`](/assets/downloads/SHA256SUMS). The Java binding is not distributed.

### The version string

The library reports `0.10.0-rc+284`: the release, then the build number after a `+`. This is valid
semver, and the build number is the same in every download of this release. A library built from
source without a build number reports just `0.10.0-rc`. The earlier downloads reported a build
number in a non-semver form, which a semver parser reads as a final release, not a pre-release.

### Added

Types and schema:

- **Nested validation.** An `Object` rule carries `fields`, and an `Array` rule carries `items`
  with `min_items` and `max_items`. Violations name the full path, for example `links[2].target`.
- **Validate a value, not just a document.** `anvil_schema_validate_value` checks an object such
  as a request body against a schema, with `skip_required` (patch semantics) and `allow_unknown`.
- **More constraints.** `nullable`, `integer` (judged by value, so `2.0` and `1e+3` pass), a
  `Blob` primitive with an optional `tag`, and `format` (`iso8601_date`, `iso8601_datetime`,
  `uuid`). `iso8601_datetime` is strict RFC 3339: a `T` separator, and `Z` or a full `+hh:mm`
  offset. `type := String` also accepts a blob.
- **Diagnostics.** A type registry and a schema now report what they used to resolve silently:
  name collisions, unknown types and formats, malformed constraints and conflicting constraints.
  A well-formed schema has zero diagnostics, which makes a good CI gate.
- **Transitive `@[types]` includes.** A schema sees the types its included type files include.
- **Introspection.** A loaded schema can be read back: every field, its type, its constraints,
  nested rules, array items and extension keys, with inherited constraints resolved.

Core:

- **Streaming writer and document builder** (`anvil_writer.h`, `anvil_builder.h`) emit canonical
  AML and AMP, validated against the real grammar. They are a separate add-on from the reader.
- **Decoded attribute values.** `anvil_attribute_get_value_handle` reads an attribute's value as
  a value: `@[status="done"]` reads as the string `done`. Every binding exposes it.
- **New value accessors:** exact numbers (`anvil_value_get_numeric`), raw bytes
  (`anvil_value_get_bytes`) and a blob's tag (`anvil_value_get_blob_tag`).
- **Schema and type-registry vtables** (`anvil_schema_vtable.h`, `anvil_type_registry_vtable.h`),
  alongside the existing flat API.

Bindings:

- **Node.js and WebAssembly** now have the same surface: parse, statement and attribute access,
  type registry, schema (including `validateField`, `fields` and `each` batches), writer and
  builder. WebAssembly's module includes types, schema and writer, so one API works on the server
  and in the browser.
- **Python and .NET** gain the type registry and schema, in each language's own idiom, plus
  decoded attribute values.

### Changed

- **`include`, not `import`**, in AML. This is the grammar change behind that problem.
- **Numeric constraints are exact.** `min`, `max` and values were truncated before, so
  `min := 0.5` behaved as `0`. A document that passed only because a bound was truncated can now
  fail. A non-numeric or fractional `size` is treated as not declared.
- **`type := String` accepts a blob.** Enums still do not.
- **The writer rejects a repeated top-level name**, as the reader already did.
- **A bare word that starts with `null`, `true` or `false`** (`nullable`, `truely`) now loads as an
  ordinary bare word. Exact `null`, `true` and `false` are unchanged.
- **An attribute's value no longer includes trailing whitespace** before the next `,` or `]`.
  A multi-line attribute list is valid.
- **Node.js and WebAssembly keep repeated names inside an object.** `{ a := 1; a := 2; }` has a
  `count` of 2, `get('a')` returns the first, and `entries()` yields both, as the other bindings
  do. They used to keep only the last.
- **Node.js and WebAssembly `parse()` and `parseRawValue()` throw a `TypeError`** for a non-string
  argument, where they used to return an empty document.
- **Package layouts changed for WebAssembly and Python.** The WebAssembly package keeps its files in `lib/` and
  `dist/` and its entry point is `lib/index.js`. The Python package keeps `anvil/` under `src/`,
  so put `<extracted directory>/src` on `PYTHONPATH`.

### Fixed

- **`libanvil.so` exports exactly the public API** (164 symbols) and no internal symbols, so it
  can't collide with other libraries loaded in the same process. The earlier library leaked
  internal symbols.
- **Thread-safe loading.** Any number of threads can load documents at once, including documents
  with identical text. Loading identical text used to fail.
- **Better errors.** A missing `;` reports "Expected ';' to end the statement", a repeated
  top-level name reports its position, and an unterminated block comment is an error wherever it
  opens.
- **No fixed-size truncation.** Long names, enum labels and messages are read in full.

### Known limitations

- The native downloads are Linux x86_64 (glibc) only; other platforms build from source. The
  WebAssembly build runs on any WebAssembly host.
- The Python package has no package metadata, so its version is only what the native library
  reports. It is not on PyPI, and the .NET package is not on NuGet.
- Node.js cannot yet read non-UTF-8 blob bytes back: the loader takes a JavaScript string, so such
  a blob is written exactly but reads back lossy.
- Node.js and WebAssembly do not yet expose a parsed blob's tag. Python and .NET do.
- A schema `min` or `max` that overflows a 64-bit integer or a double reads as `undefined` in the
  Node.js type registry, because the library has no text accessor for it.
- The `.wasm` file is larger than before (about 160 KB, up from 84 KB), because the types,
  schema and writer code is now reachable.
- AnvilScript (`#!asl`) isn't included in these builds; a `#!asl` document is rejected.
