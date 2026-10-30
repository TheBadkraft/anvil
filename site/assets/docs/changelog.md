# Changelog

What changed in each release of Anvil, for people using it. Downloads are on the
[download page](/download).

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
