# Changelog

What changed in each release of Anvil, for people using it. Downloads are on the
[download page](/download).

## v0.14.0-rc

Released 2026-11-05. This is a release candidate.

v0.14.0-rc is a minor release with one breaking change. **Read it first if any of your documents put a base on a nested
statement.** It also adds the block-form writer and builder, names the document an error occurred in, and positions include
errors at the `include` keyword.

### Breaking change: only a top-level statement may have a base

A base (`a : b := { ... };` or `a : b { ... };`) is allowed on a **top-level statement only**. A base on a statement inside an
object, in either form and at any depth, is now an error found while parsing, reported at the base name:
`ANVL_ERR_PARSER_NESTED_BASE` (4022), "Only a top-level statement can have a base".

Why: the implementation had accepted a nested base by accident, and the language never meant it. A nested statement based on
its own enclosing top-level statement made the inheritance graph loop through containment. `s1 := { f3 : s1 { a := 1; }; };`
loaded without an error, and then any walk over the resolved tree (a dump, a serialiser, a binding) never ended. It now fails
at once with 4022.

- **Reader.** 4022 wins over every later error: a nested base that names nothing is 4022 (not 4051), and a nested base on a
  scalar is 4022 (not 4020). A `:` with no name after it (`{ a: 1; }`) is still "Expected identifier".
- **Writer and builder.** They refuse the same thing with the new `ANVIL_WRITER_ERR_NESTED_BASE` (23), "only a top-level
  statement can have a base". The writer gives it from `anvil_writer_statement` and `anvil_writer_statement_block` while an
  object is open. The builder gives it at once from `anvil_builder_set_base` for a member added to an object, at any depth,
  attached or not. In anvil.js: `WriterErrorCode.NESTED_BASE`, from `AnvilWriter#statement`, `AnvilWriter#statementBlock` and
  `AnvilMember#setBase`.
- **Documents that used a nested base no longer load.** Rewrite them with the base on a top-level statement. Nothing else
  changed: top-level bases, chains, cycles (4050), missing bases (4051) and anonymous bases (4052) behave as before.

### Also in this release

- **Block-form writer and builder.** The anonymous, read-only block form `name [: base] [@[attributes]] { ... };` can now be
  written. `anvil_writer_statement_block(writer, name, base)` starts such a statement; its value must be an object, so
  `anvil_writer_begin_object` follows, and any other value is the new `ANVIL_WRITER_ERR_BLOCK_REQUIRES_OBJECT` (21). It is AML
  only. `anvil_builder_set_block(builder, member, block)` marks a builder statement as block form (`false` restores
  `name := { ... };`). A block-form statement is read-only and cannot be a base (the reader gives 4052), so the builder refuses
  to emit or write a document whose base names a block-form member, and the streaming writer refuses a base that names a
  block-form statement it has already written: `ANVIL_WRITER_ERR_BASE_IS_BLOCK_FORM` (22). In the vtables the new entries are
  appended after the existing fields (`Writer.statement_block`, `Builder.set_block`), so the existing layout is unchanged.
- **An error says which document it is in.** `anvil_error_get_source(error, buffer, length)` (and `Error.get_source` in the
  vtable) gives the path the document was opened by: the including document's directory, a `/`, and the include string as written
  (not normalised). It is empty for an error in the root document. Line and column were already positions inside the included
  document; now the document is named. It covers syntax and header errors, include errors, an included document's own load
  error (an invalid shebang), and resolve errors that have a position. `anvil-dump` prints it, and the dump format is now
  version 2.
- **Include errors are at the `include` keyword.** A missing file (4204), a cycle (4206) and a path of 1024 bytes or more (9002)
  are reported at the line and column of the `include` keyword in the document that contains it, not at line 1, column 1 of the
  root. Columns count bytes.
- **The library now exports 174 functions** (171 before): `anvil_writer_statement_block`, `anvil_builder_set_block` and
  `anvil_error_get_source`. Nothing was removed or changed.
- **anvil.js has the same changes.** `lastError().source`; `AnvilWriter#statementBlock(name, base)` and
  `AnvilMember#setBlock(block)`, with codes 21 and 22; code 23 and error 4022; include errors at the `include` keyword. It
  matches the native conformance corpora on all of them: reader 1920, writer 851 (+ 16 C-API-only), builder 933 (+ 53 C-API-only)
  and schema 561.

### Known issue

A `$` reference to the object that contains it (`s1 := { a := $s1; };`), directly or through other references, still loads
without an error, and reading through such a value with an accessor that follows references never finishes. This was already the
case before this release, it is not fixed here, and it is separate from the nested-base loop above. Avoid a `$` reference that
points at one of its own enclosing objects.

### Downloads

The native download (core) is Linux x86_64 (glibc). anvil.js is plain JavaScript: it runs in Node 18 or later and in any
browser. Both are built from the same source tag; the library reports exactly `0.14.0-rc+554`, and anvil.js reports the same
string from `getVersion()`.

| Download | What it is |
|---|---|
| `anvil-v0.14.0-rc-linux-x86_64.tar.gz` | The C library: `libanvil.a`, `libanvil.so` and every public header |
| `anvil.js-v0.14.0-rc.tar.gz` | The pure-JavaScript package for Node and browsers: reader, writer, builder, types and schema |

**Python and .NET stay at v0.13.1-rc.** There is no v0.14.0-rc Python or .NET download; their v0.13.1-rc files stay current and
do not have the changes above (they read and write the same documents as v0.13.1-rc, including nested bases). The Node
(`anvil-node`) and WebAssembly (`anvil-wasm`) builds are not part of this release; their v0.11.0-rc downloads stay available at
their existing addresses. The Java binding is not distributed.

`SHA256SUMS` lists every published download, this release and the earlier ones (`SHA256SUMS-v0.14.0-rc` lists this release only):

<!-- example: fragment a release checksum list, not code -->
```
bfd7db7cc9cf04e32b865506354ef19c1d919a30e6c1ad392109716412e77070  anvil.js-v0.14.0-rc.tar.gz
1e98cbacf9f6dc04753f1ee7856fcdf52d19fbcec8e52eaa80c2e239d4a743e9  anvil-v0.14.0-rc-linux-x86_64.tar.gz
```

Earlier releases' lines follow unchanged in `SHA256SUMS`; every earlier file stays available at its address.

### Performance

Every figure is time taken, as subject divided by baseline, so lower is faster. "Cold" is the first call in a fresh
process; "warm" is the median of 101 calls after a warm-up. Measured on the packaged files, one machine, each side in its own
fresh process, median of 10 or more processes unless stated.

#### Native

native v0.14.0-rc vs native v0.13.1-rc, warm, packaged static library, 9 interleaved runs, one CPU: parse 0.989x to 0.995x,
validate 0.990x to 1.005x. Through `libanvil.so`: parse 1.001x to 1.004x, validate 0.998x to 1.000x. Loading 20,000 five-line
documents: 1.003x (static library) and 1.010x (`libanvil.so`). The first load in a fresh process, cold: 0.98x (static library,
14.7 against 15.0 microseconds) and 1.015x (`libanvil.so`, 18.2 against 17.9 microseconds).

#### anvil.js against anvil.js v0.13.1-rc

anvil.js v0.14.0-rc vs anvil.js v0.13.1-rc, reading FlyWire's per-request documents and the other reader workloads (22
documents of 17 to 10,000 rows), 10 processes: cold 0.95x to 1.04x, warm 0.97x to 1.02x. Unchanged.

`validateValue` on a 12-field record, same pair, microseconds per call: call 1 (cold) 0.95x to 1.00x, calls 20 to 120 0.99x to
1.03x, calls 200 to 400 0.95x to 1.01x, steady state 0.87x to 1.02x. Unchanged.

Schema load and validate, same pair: 0.97x to 1.00x. Parsing, and parsing plus reading every value (nine workloads): 0.90x to
1.12x, with no consistent direction between the two arm orders. Build and emit (thirteen workloads): 0.98x to 1.23x in the
arm order where the old version runs first and 0.68x to 1.13x in the other; the rows above 1.15x (two documents of one request or
response) were 1.11x and 0.96x on a rerun.

**Writing is slower for large documents.** anvil.js v0.14.0-rc vs anvil.js v0.13.1-rc, warm, writing 1,000 FlyWire-shaped
requests (`kc-request-x1000`, about 0.68 ms against about 0.59 ms): 1.19x, 1.21x, 1.22x and 1.20x in the order where the old
version runs first, 1.10x, 1.07x, 1.15x and 1.19x in the other order, on four runs; the same comparison of v0.13.1-rc against
itself gives 0.93x to 1.03x in that order. Writing 1,000 responses is 1.14x and 1.12x on two runs. The other seven writing
workloads, one to ten documents and the configuration and string workloads, are 0.76x to 1.25x with no consistent direction.
This is not fixed in this release.

#### Known performance gaps in anvil.js (not fixed; scheduled for a later release)

Unchanged from v0.13.1-rc, because anvil.js v0.14.0-rc reads within the range above of v0.13.1-rc: small documents are where
anvil.js is slower than anvil-node. These are not promises, and they are not fixed. They supersede the figures in the package's
README.

anvil.js v0.13.1-rc vs anvil-node v0.10.0-rc, FlyWire's per-request documents, time ratio, lower is faster:

| Document | Cold | Warm |
|---|---|---|
| small `@bin` read response, 1 / 5 / 17 rows | 1.38x / 1.35x / 1.59x | 2.09x / 2.64x / 1.42x |
| small `@bin` poll response, 1 / 5 / 17 rows | 1.36x / 1.36x / 1.58x | 2.02x / 2.59x / 1.42x |
| `@bin` envelope, 17 / 100 rows | 1.52x / 1.45x | 1.36x / 0.81x |
| `@table` envelope, 17 / 100 rows | 1.42x / 1.15x | 0.77x / 0.34x |
| error document | 1.65x | 1.91x |
| raw table text, 17 rows | 1.12x | 0.44x |

Every other document size measured, from 1,000 rows up, is at 0.19x to 0.93x.

- `validateValue` on a 12-field record, anvil.js vs anvil-node v0.10.0-rc, calls 200 to 400 of a process: 1.28x
  (create-valid), 1.18x (missing required field), 1.14x (type violation).
- `Schema.load`, anvil.js vs anvil-node v0.10.0-rc: 11.1x (0.374 ms against 0.034 ms for FlyWire's assets schema).
  Schema `validate` of one record: 1.06x to 1.08x; of 100 and 10,000 records: 0.40x to 0.42x.
- Against v0.12.1-rc, warm small documents are slower: `amp-bin-read-1` 1.24x, `amp-bin-poll-1` 1.18x, `amp-bin-read-5` 1.74x,
  `amp-bin-poll-5` 1.60x, `errdoc` 1.20x (anvil.js v0.13.0-rc vs anvil.js v0.12.1-rc). v0.14.0-rc and v0.13.1-rc are equal on
  these rows.

### Checks

Every check ran on the packaged files, not on a build tree.

- Two builds of each download from the tag are byte-identical.
- Core: the reported version is `0.14.0-rc+554`; an `include` of a missing file gives an include-not-found error; 174 exported
  functions and no internal symbol; every header compiles on its own and links from C++.
- anvil.js, installed with `npm install` into an empty directory: `require` and `import` work; named imports; the version
  matches; reader 1920, writer 851 + 16 C-API-only, builder 933 + 53 C-API-only and schema 561 conformance scripts match native
  byte for byte through the installed copy; Chromium 156 and Firefox 157, in the ESM and the global build, 16 checks each.
- For both: `s1 := { f3 : s1 { a := 1; }; };` fails with 4022 at once; the writer and the builder give 23 for a nested base; the
  block form is written as `name { ... };` and reloads, with 21 and 22 for the two refusals; an error in an included file names
  that file; an include error is at the `include` keyword.
- Every code sample in the documentation was run against these downloads: 117 passed, none failed.

### Licence

Every download includes a `LICENSE.txt` with the ANVL Software Licence terms, which are still in progress: free for
personal and educational use, free for commercial use by organizations with up to 3 developer seats, and no
redistribution.

### The version string

The library reports its version as `0.14.0-rc+554`: the release, then the build number after a `+`. This is valid
semver. The build number is the same in both downloads of this release.

## v0.13.1-rc

Released 2026-10-31. This is a release candidate.

v0.13.1-rc is a patch release with two behaviour changes. **Read these first if you match error names or use the `integer`
type.**

### Behaviour changes

- **Error name strings equal their identifiers.** The names of three IO errors were spelled differently from the
  identifiers in `errors.h`. They are now `ANVL_ERR_IO_FILE_READ` (1001), `ANVL_ERR_IO_FILE_NOT_FOUND` (1002) and
  `ANVL_ERR_IO_INVALID_PATH` (1003); before they were `ANVL_ERR_FILE_READ`, `ANVL_ERR_FILE_NOT_FOUND` and
  `ANVL_ERR_FILE_INVALID_PATH`. These were the only three mismatches among 107 codes, and a check now fails the build if any name
  differs from its identifier. The codes and the enum are unchanged. Anything that matches the old strings (a log filter, a test,
  `lastError().name` in anvil.js) must change.
- **`integer := true` decides whole-ness from the literal's text.** A decimal or exponent literal is whole only if its exact
  decimal value has no nonzero digit after the point. `1e-999`, `0.5e-400`, `1.000001` and `1.0000000000000001` now fail
  with "value must be an integer"; before, the last two and the underflowing literals passed because they rounded to a whole
  double. Zero in any spelling, `1e3`, `1.0`, a digits-only literal past 64 bits, and the rejection of `1e400` are unchanged.

### Downloads

The native downloads (core, Python and .NET) are Linux x86_64 (glibc). anvil.js is plain JavaScript: it runs in
Node 18 or later and in any browser. Each native file is built from the same source tag, and the library inside
reports exactly `0.13.1-rc+479`; anvil.js reports the same string from `getVersion()`.

| Download | What it is |
|---|---|
| `anvil-v0.13.1-rc-linux-x86_64.tar.gz` | The C library: `libanvil.a`, `libanvil.so` and every public header |
| `anvil.js-v0.13.1-rc.tar.gz` | The pure-JavaScript package for Node and browsers: reader, writer, builder, types and schema |
| `anvil-py-v0.13.1-rc-linux-x64.tar.gz` | Python package (CPython 3.11+, standard library only) |
| `anvil-net-v0.13.1-rc-linux-x64.tar.gz` | .NET 9 assembly (`Anvil.Net.dll`) with its native library |

`SHA256SUMS` lists every published download, this release and the earlier ones (`SHA256SUMS-v0.13.1-rc` lists this
release only):

<!-- example: fragment a release checksum list, not code -->
```
876fece1c4d4ee45bdf09b21b494e89ce0f1fd295b8e4eb649a089e89c61a114  anvil.js-v0.13.1-rc.tar.gz
eef31501ed61b0fa98002a370a67a08f0c047b0f9072b69cffd789e5bbf675d2  anvil-net-v0.13.1-rc-linux-x64.tar.gz
0237de90360a5780da2087c3ccbbafcca18df4787cf797c3c82ec64a8752a729  anvil-py-v0.13.1-rc-linux-x64.tar.gz
8579f079c1f49d9e2d6ace44b4584da3de5b995589b692bd75d19924fe945717  anvil-v0.13.1-rc-linux-x86_64.tar.gz
fa318c4e5fe3da7a3dbd626021747be06053bc6ec6891def314c1831e75aefd3  anvil.js-v0.13.0-rc.tar.gz
aaf9b3fcd9b0d85ec64d6088f4948819cf0dd65b54b29f2264201504c26c8740  anvil-net-v0.13.0-rc-linux-x64.tar.gz
bc8e79ccf291733a72a145451fb68abeb96b341dc9749b7ff5e8715f65245cd0  anvil-py-v0.13.0-rc-linux-x64.tar.gz
e366ebc87ba5716004bdfba6bbd60afa1226c1c3c1eabc5810bb2152835ac2ab  anvil-v0.13.0-rc-linux-x86_64.tar.gz
ee89439b3b1ce61134b7432dfb5fe7d9e3456bbd887b1a4f01824f48ce4ab6f1  anvil.js-v0.12.1-rc.tar.gz
4be20e69edc39228659589fb9ce5759b1324218f4178f4c1274ef95809be0ade  anvil-net-v0.12.1-rc-linux-x64.tar.gz
9dd6636973dfcb2bc84aa5753e871ba9638a5272445f4bb21eb5b1f375ff9748  anvil-py-v0.12.1-rc-linux-x64.tar.gz
7adc031aaadb5301fdfe06b798eaa366a63b7db7fd283fed0efebec05d8052b3  anvil-v0.12.1-rc-linux-x86_64.tar.gz
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

The Node (`anvil-node`) and WebAssembly (`anvil-wasm`) builds are not part of this release. Their v0.11.0-rc
downloads stay available at their existing addresses. The Java binding is not distributed.

### What else changed

- **A load can say why it failed (additive).** A document without `@[schema]` made `anvil_schema_load` return NULL, and one
  without `@[types]` made `anvil_type_registry_load` return NULL, with no reason. New functions `anvil_schema_load_ex` and
  `anvil_type_registry_load_ex` return the reason: `ANVIL_SCHEMA_ERR_ATTRIBUTE_MISSING` (11) and
  `ANVIL_TYPE_ERR_ATTRIBUTE_MISSING` (1), with name and message functions for each. The old functions behave as before.
  The library now exports 171 functions (165 before); nothing was removed or changed.
- **anvil.js has the same three changes.** After a null `Schema.load`, `lastError()` is code 11
  (`ANVIL_SCHEMA_ERR_ATTRIBUTE_MISSING`); after a null `TypeRegistry.load` it is code 1 (`ANVIL_TYPE_ERR_ATTRIBUTE_MISSING`).
  `SchemaErrorCode` and `TypeErrorCode` are exported, and the IO error names and `integer` follow the library. The conformance
  corpora grew to 561 schema scripts; anvil.js matches native on all of them, and on the reader (1884), writer (749 + 15
  C-API-only) and builder (866 + 49 C-API-only) corpora.
- **Python and .NET:** no API changes; they pick up the library changes.

### Performance

Every figure is time taken, as subject divided by baseline, so lower is faster. "Cold" is the first call in a fresh
process; "warm" is the median of 101 calls after a warm-up. Measured on the packaged files, one machine, each side in its own
fresh process, median of 10 or more processes.

#### Native

native v0.13.1-rc vs native v0.13.0-rc, warm, packaged static library, 9 interleaved runs, one CPU: parse 1.002x to
1.020x, validate 0.994x to 1.003x. Through `libanvil.so`: 0.989x to 1.007x. Loading 20,000 five-line documents: 0.987x
(static library) and 1.001x (`libanvil.so`). The first load in a fresh process, cold: 0.99x (static library, 14.5 against
14.7 microseconds) and 1.01x (`libanvil.so`, 17.2 against 17.1 microseconds).

#### anvil.js against anvil.js v0.13.0-rc

anvil.js v0.13.1-rc vs anvil.js v0.13.0-rc, reading FlyWire's per-request documents (parse, then `get('payload')`), 36
documents: cold 0.97x to 1.03x, warm 0.97x to 1.01x. Unchanged.

`validateValue` on a 12-field record, anvil.js v0.13.1-rc vs anvil.js v0.13.0-rc, microseconds per call, two runs (10 and 20
processes): call 1 (cold) 1.00x to 1.08x, calls 20 to 120 1.00x to 1.04x, calls 200 to 400 1.00x to 1.11x, steady state
0.98x to 1.19x (the two rows above 1.15x, `create-missing-required` and `update-partial`, were 1.03x and 1.12x on the rerun). This is the cost of the corrected `integer`
check on the fast path.

Writing, anvil.js v0.13.1-rc vs anvil.js v0.13.0-rc, warm, nine workloads, two runs: 0.94x to 1.21x with no consistent
direction (the one 1.21x, `kc-request-x10`, was 1.02x on the rerun).

#### Known performance gaps in anvil.js (not fixed; backlogged)

Unchanged from v0.13.0-rc: small documents are where anvil.js is slower than anvil-node. These are measured again for this
release, they are not promises, and they are not fixed. They supersede the figures in the package's README.

anvil.js v0.13.1-rc vs anvil-node v0.10.0-rc, FlyWire's per-request documents, time ratio, lower is faster:

| Document | Cold | Warm |
|---|---|---|
| small `@bin` read response, 1 / 5 / 17 rows | 1.38x / 1.35x / 1.59x | 2.09x / 2.64x / 1.42x |
| small `@bin` poll response, 1 / 5 / 17 rows | 1.36x / 1.36x / 1.58x | 2.02x / 2.59x / 1.42x |
| `@bin` envelope, 17 / 100 rows | 1.52x / 1.45x | 1.36x / 0.81x |
| `@table` envelope, 17 / 100 rows | 1.42x / 1.15x | 0.77x / 0.34x |
| error document | 1.65x | 1.91x |
| raw table text, 17 rows | 1.12x | 0.44x |

Every other document size measured, from 1,000 rows up, is at 0.19x to 0.93x.

- `validateValue` on a 12-field record, anvil.js vs anvil-node v0.10.0-rc, calls 200 to 400 of a process: 1.28x
  (create-valid), 1.18x (missing required field), 1.14x (type violation).
- `Schema.load`, anvil.js vs anvil-node v0.10.0-rc: 11.1x (0.374 ms against 0.034 ms for FlyWire's assets schema).
  Schema `validate` of one record: 1.06x to 1.08x; of 100 and 10,000 records: 0.40x to 0.42x.
- Against v0.12.1-rc, warm small documents are slower, as in v0.13.0-rc: `amp-bin-read-1` 1.24x, `amp-bin-poll-1`
  1.18x, `amp-bin-read-5` 1.74x, `amp-bin-poll-5` 1.60x, `errdoc` 1.20x (anvil.js v0.13.0-rc vs anvil.js v0.12.1-rc).

#### anvil.js against anvil-node, other workloads (information)

anvil.js v0.13.1-rc vs anvil-node v0.10.0-rc: parsing is 0.09x to 0.20x of the time, parsing plus reading every value 0.11x to
0.44x (nine workloads); writing 0.17x to 0.70x (nine workloads); builder and emit 0.18x to 0.95x (thirteen workloads). Against
`JSON.parse` (information only), parse 0.30x to 1.8x and parse plus read 0.7x to 4.3x.

### Licence

Every download includes a `LICENSE.txt` with the ANVL Software Licence terms, which are still in progress: free for
personal and educational use, free for commercial use by organizations with up to 3 developer seats, and no
redistribution.

### The version string

The library reports its version as `0.13.1-rc+479`: the release, then the build number after a `+`. This is valid
semver. The build number is the same in every download of this release.

## v0.13.0-rc

Released 2026-10-31. This is a release candidate.

v0.13.0-rc is a minor release because behaviour changes: a file that cannot be loaded now reports a specific error
code, include cycles are found by normalised path, and the `integer` type follows the corrected range rule. It also
completes anvil.js: reader, writer, builder, types and schema.

### Downloads

The native downloads (core, Python and .NET) are Linux x86_64 (glibc). anvil.js is plain JavaScript: it runs in
Node 18 or later and in any browser. Each native file is built from the same source tag, and the library inside
reports exactly `0.13.0-rc+463`; anvil.js reports the same string from `getVersion()`.

| Download | What it is |
|---|---|
| `anvil-v0.13.0-rc-linux-x86_64.tar.gz` | The C library: `libanvil.a`, `libanvil.so` and every public header |
| `anvil.js-v0.13.0-rc.tar.gz` | The pure-JavaScript package for Node and browsers: reader, writer, builder, types and schema |
| `anvil-py-v0.13.0-rc-linux-x64.tar.gz` | Python package (CPython 3.11+, standard library only) |
| `anvil-net-v0.13.0-rc-linux-x64.tar.gz` | .NET 9 assembly (`Anvil.Net.dll`) with its native library |

`SHA256SUMS` lists every published download, this release and the earlier ones (`SHA256SUMS-v0.13.0-rc` lists this
release only):

<!-- example: fragment a release checksum list, not code -->
```
fa318c4e5fe3da7a3dbd626021747be06053bc6ec6891def314c1831e75aefd3  anvil.js-v0.13.0-rc.tar.gz
aaf9b3fcd9b0d85ec64d6088f4948819cf0dd65b54b29f2264201504c26c8740  anvil-net-v0.13.0-rc-linux-x64.tar.gz
bc8e79ccf291733a72a145451fb68abeb96b341dc9749b7ff5e8715f65245cd0  anvil-py-v0.13.0-rc-linux-x64.tar.gz
e366ebc87ba5716004bdfba6bbd60afa1226c1c3c1eabc5810bb2152835ac2ab  anvil-v0.13.0-rc-linux-x86_64.tar.gz
ee89439b3b1ce61134b7432dfb5fe7d9e3456bbd887b1a4f01824f48ce4ab6f1  anvil.js-v0.12.1-rc.tar.gz
4be20e69edc39228659589fb9ce5759b1324218f4178f4c1274ef95809be0ade  anvil-net-v0.12.1-rc-linux-x64.tar.gz
9dd6636973dfcb2bc84aa5753e871ba9638a5272445f4bb21eb5b1f375ff9748  anvil-py-v0.12.1-rc-linux-x64.tar.gz
7adc031aaadb5301fdfe06b798eaa366a63b7db7fd283fed0efebec05d8052b3  anvil-v0.12.1-rc-linux-x86_64.tar.gz
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

The Node (`anvil-node`) and WebAssembly (`anvil-wasm`) builds are not part of this release. Their v0.11.0-rc
downloads stay available at their existing addresses. The Java binding is not distributed.

### What changed

#### Behaviour changes (native library, and so Python, .NET and anvil.js)

- **A file that cannot be loaded reports its specific error code.** Loading a missing file reports 1002 (file not
  found), a directory or a file that cannot be read reports 1001 (file read), and an empty path reports 1003 (invalid
  path), each with its message. Before, the error category was set but the code was 0 with an empty message. The category is
  unchanged. An `include` that names a directory is now the include error 4204 and no longer loads as an empty
  document.
- **Include cycles are found by normalised path.** `a.anvl`, `./a.anvl` and `d/../a.anvl` are one document. A cycle through a
  bare root file name, and an include spelled with `./` or `..` that loops back, are now the cycle error 4206. Before, they
  were accepted.
- **`integer` rejects a decimal or exponent literal beyond double range** (for example `1e400`) with a type mismatch. A
  digits-only literal past 64 bits is a whole number and still passes.
- **`TypeRegistry` `fromIncludes` merges the root document's own `@[types]`** last, so they override included ones.
- **The builder** has messages for its inheritance-cycle and missing-base errors (codes 19 and 20), and refuses a base
  that names no statement when it holds no `include`. Its checks are linear in the number of members.
- **Conformance corpora ship with the source** for second implementations: the builder (915 scripts) and the schema and
  type registry (559 scripts).

There is no change to the C API or ABI.

#### anvil.js is complete

anvil.js now has the reader, the writer, the builder, types and schema, with the same API as `anvil-node`.

- **The builder:** `AnvilBuilder`, `AnvilNode` and `AnvilMember`, with the same bytes as the C library on 866 of the 915
  native builder cases. The other 49 are C-API-only: they pass a null, a handle of another builder, or text that is not
  valid UTF-8, which JavaScript cannot express; they are listed in the package's tests.
- **The writer:** 749 of the 764 native writer cases match; the other 15 are C-API-only. `value()` writes a safe integer as
  an integer, as `anvil-node` does.
- **Types and schema:** `TypeRegistry` and `Schema` match the C library on all 559 native schema cases.
- **`setIo({ read(path) })`** lets a browser resolve `include` from files you supply.
- **Faster paths:** `parseRawValue` and `validateValue` have fast paths, and the module compiles eagerly at load, so a
  process's first `parse()` is much faster than in v0.12.1-rc.
- Named ES-module imports cover all of these: `import { AnvilBuilder, TypeRegistry, Schema, setIo } from 'anvil.js'`.
- Also verified: in Chromium and Firefox (ES module and script builds), a builder row validated against FlyWire's assets
  schema; and the reader (1884), writer, builder and schema corpora through the installed package.

#### Python and .NET

No API changes. Both pick up the native changes through the library.

### Performance

Every figure is time taken, as subject divided by baseline, so lower is faster. "Cold" is the first call in a fresh
process; "warm" is the median of 101 calls after a warm-up. Measured on the packaged files, one machine, each side in
its own fresh process, median of 10 to 20 processes.

#### Native

native v0.13.0-rc vs native v0.12.1-rc, warm, packaged static library, 9 interleaved runs, one CPU: parse 0.990x to
1.006x, validate 1.013x to 1.020x. Through `libanvil.so`: 0.994x to 1.006x. Loading 20,000 five-line documents: 1.015x
(static library) and 1.008x (`libanvil.so`). The first load in a fresh process, cold: 1.00x (static library) and 1.034x
(`libanvil.so`, 18.0 against 17.4 microseconds). The validate figure and the `libanvil.so` small-document cost are known and
accepted for this release.

#### anvil.js against anvil-node

anvil.js v0.13.0-rc vs anvil-node v0.10.0-rc: parsing is 0.09x to 0.21x of the
time and parsing plus reading every value 0.11x to 0.46x, across nine workloads (information only: against
`JSON.parse`, parse 0.30x to 1.79x and parse plus read 0.67x to 4.33x). Writing is 0.15x to 0.68x of the time across nine
workloads (information only: against `JSON.stringify`, 3.1x to 6.3x). The builder and emit is 0.23x to 0.96x across
thirteen workloads. Schema `validate` of 100 and 10,000 records is 0.39x to 0.42x.

#### Known performance gaps in anvil.js (not fixed; scheduled for a later release)

Small documents are where anvil.js is slower than anvil-node. These are measured, not promises, and they are not fixed
in this release. They supersede the figures in the package's README.

anvil.js v0.13.0-rc vs anvil-node v0.10.0-rc, reading FlyWire's per-request documents (parse, then `get('payload')`), time
ratio, lower is faster:

| Document | Cold | Warm |
|---|---|---|
| small `@bin` read response, 1 / 5 / 17 rows (`amp-bin-read-N`) | 1.39x / 1.40x / 1.60x | 2.09x / 2.67x / 1.38x |
| small `@bin` poll response, 1 / 5 / 17 rows (`amp-bin-poll-N`) | 1.41x / 1.42x / 1.50x | 2.07x / 2.59x / 1.42x |
| `@bin` envelope, 17 / 100 rows (`amp-bin-N`) | 1.56x / 1.48x | 1.31x / 0.84x |
| `@table` envelope, 17 / 100 rows | 1.44x / 1.19x | 0.77x / 0.35x |
| error document (`errdoc`) | 1.78x | 1.87x |
| raw table text, 17 rows | 1.08x | 0.43x |

(A second run on other CPUs, 20 processes, agrees within 0.05x. Against anvil-node v0.11.0-rc the ratios are 0.01x to 0.15x lower
in cold.) Every other document size measured, from 1,000 rows up, is at 0.19x to 0.93x.

- `validateValue` on a 12-field record, anvil.js vs anvil-node v0.10.0-rc, calls 200 to 400 of a process: 1.26x
  (create-valid), 1.13x (missing required field), 1.12x (type violation). Call 1, calls 20 to 120 and steady state are at 0.49x to 0.99x.
- `Schema.load`, anvil.js vs anvil-node v0.10.0-rc: 11.1x (0.374 ms against 0.034 ms for FlyWire's assets schema), partly the
  warm-up it performs so that later validation is fast.

anvil.js v0.13.0-rc vs anvil.js v0.12.1-rc:

- Cold, the same documents are faster in every row: 0.14x to 0.73x (the eager compile at load).
- Warm, small documents are slower: `amp-bin-read-1` 1.24x, `amp-bin-poll-1` 1.18x, `amp-bin-read-5` 1.74x,
  `amp-bin-poll-5` 1.60x, `errdoc` 1.20x. These are 2 to 5 microseconds per call. Every other warm row is within 1.02x.
- Writing: `kc-response-x10` 1.105x and 1.064x in two runs (0.0070 against 0.0064 ms per call). The other eight write
  workloads are between 0.89x and 1.08x run to run, with no consistent direction.

### Licence

Every download includes a `LICENSE.txt` with the ANVL Software Licence terms, which are still in progress: free for
personal and educational use, free for commercial use by organizations with up to 3 developer seats, and no
redistribution.

### The version string

The library reports its version as `0.13.0-rc+463`: the release, then the build number after a `+`. This is valid
semver. The build number is the same in every download of this release.

## v0.12.1-rc

Released 2026-10-30. This is a release candidate.

v0.12.1-rc is a patch release. The C headers can now be included from C++, the writer follows the grammar's
`include` rule, and anvil.js gains a writer, ES-module named imports and error codes from `lastError()`.

### Downloads

The native downloads (core, Python and .NET) are Linux x86_64 (glibc). anvil.js is plain JavaScript: it runs in
Node 18 or later and in any browser. Each native file is built from the same source tag, and the library inside
reports exactly `0.12.1-rc+420`; anvil.js reports the same string from `getVersion()`.

| Download | What it is |
|---|---|
| `anvil-v0.12.1-rc-linux-x86_64.tar.gz` | The C library: `libanvil.a`, `libanvil.so` and every public header |
| `anvil.js-v0.12.1-rc.tar.gz` | The pure-JavaScript package for Node and browsers: reader and writer |
| `anvil-py-v0.12.1-rc-linux-x64.tar.gz` | Python package (CPython 3.11+, standard library only) |
| `anvil-net-v0.12.1-rc-linux-x64.tar.gz` | .NET 9 assembly (`Anvil.Net.dll`) with its native library |

`SHA256SUMS` lists every published download, this release and the earlier ones (`SHA256SUMS-v0.12.1-rc` lists this
release only):

<!-- example: fragment a release checksum list, not code -->
```
ee89439b3b1ce61134b7432dfb5fe7d9e3456bbd887b1a4f01824f48ce4ab6f1  anvil.js-v0.12.1-rc.tar.gz
4be20e69edc39228659589fb9ce5759b1324218f4178f4c1274ef95809be0ade  anvil-net-v0.12.1-rc-linux-x64.tar.gz
9dd6636973dfcb2bc84aa5753e871ba9638a5272445f4bb21eb5b1f375ff9748  anvil-py-v0.12.1-rc-linux-x64.tar.gz
7adc031aaadb5301fdfe06b798eaa366a63b7db7fd283fed0efebec05d8052b3  anvil-v0.12.1-rc-linux-x86_64.tar.gz
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

The Node (`anvil-node`) and WebAssembly (`anvil-wasm`) builds are not part of this release. Their v0.11.0-rc
downloads stay available at their existing addresses. The Java binding is not distributed.

### What changed

#### Native library

- **C++ can include every public header.** Each header now has `extern "C"` guards, and the release build
  compiles and links a C++ program against every header.
- **The writer follows the grammar's `include` rule.** An `include` after a module attribute is refused with
  the writer's state error, and the builder writes its includes first. The grammar itself is unchanged: the writer
  now matches it. A program that wrote an include after attributes now gets an error and must write the include first.
- **AnvilScript parsing has a depth limit** (error 5106). AnvilScript is off by default.
- **A writer test corpus ships with the source** for people writing a second implementation: scripts of writer
  calls with the exact bytes the library produces.

#### anvil.js

- **The writer.** `AnvilWriter`, `AnvilDialect`, `WriterErrorCode` and `AnvilWriterError`, with the
  `AnvilWriter` API of `anvil-node` and the same bytes as the C library on all 749 cases of the native writer corpus.
  Where a JavaScript API cannot say what the C API can, 15 further cases are not run: 10 pass a null where a string is
  required and 5 write text that is not valid UTF-8. `value()` writes a safe integer as an integer, and a number
  with a fraction or beyond the safe range as a double, as `anvil-node` does. The builder is not in anvil.js yet.
- **Named imports in Node ES modules.** `import { parse, AnvilWriter, AnvilDialect } from 'anvil.js'` works, as does
  the default import. `require('anvil.js')` is unchanged.
- **`lastError()` carries the error code and its name** next to the message, line and column (a repeated key is
  `4001`).
- Types, schema validation and the builder are still not in anvil.js. Use the Python or .NET packages, or the C
  library, where you need those.

#### Python and .NET

No API changes. Both pick up the writer change through the native library.

### Performance

Measured on the packaged files, as time taken (lower is faster).

- **Native.** native v0.12.1-rc vs native v0.12.0-rc: parse 0.97x to 1.00x, validate 0.995x to 1.00x (packaged static
  library, 9 interleaved runs, one CPU); loading 20,000 small documents through `libanvil.so` 1.00x or better
  (0.998x). The .NET package, same assembly with only the library swapped: 0.995x to 1.000x on every workload.
- **anvil.js reading.** anvil.js vs anvil-node v0.10.0-rc takes 0.09x to 0.21x of the time to parse and 0.12x to 0.49x of
  the time to parse and read every value, across nine workloads. anvil.js vs `JSON.parse` (information only): 0.31x to
  1.79x to parse, 0.67x to 5.13x to parse and read through the public API.
- **anvil.js writing.** anvil.js write vs anvil-node v0.10.0-rc write takes 0.19x to 0.67x of the time across nine
  workloads (King and Crucible requests and responses, a 200-statement configuration, 2,000 numbers, 1,000 strings with
  escapes). anvil.js write vs `JSON.stringify` of the equivalent data (information only): 3.2x to 5.3x the time, so
  `JSON.stringify` is much faster.
- The known cost from v0.12.0-rc (tiny-document loads through `libanvil.so`, 1.027x to 1.039x vs v0.11.0-rc) is
  unchanged from v0.12.0-rc: native v0.12.1-rc vs native v0.12.0-rc is 1.00x.

### Licence

Every download includes a `LICENSE.txt` with the ANVL Software Licence terms, which are still in progress: free for
personal and educational use, free for commercial use by organizations with up to 3 developer seats, and no
redistribution.

### The version string

The library reports its version as `0.12.1-rc+420`: the release, then the build number after a `+`. This is valid
semver. The build number is the same in every download of this release.

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

<!-- example: fragment a release checksum list, not code -->
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
