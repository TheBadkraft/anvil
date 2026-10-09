# FR-2609-anvl-writer-001: a streaming writer (and, next, a document builder) in Anvil Native

**ID:** FR-2609-anvl-writer-001
**Type:** Feature Request (native library / public API)
**Owner:** anvl
**Filed:** 2026-10-09
**Filed by:** anvil
**Status:** in progress — the streaming writer (flat + vtable) is implemented and tested; the document
builder and the binding wrappers are not started
**Continues:** [`FR-2609-anvl-public-api-001`](FR-2609-anvl-public-api-001.md) — the public API there is
read-only; this adds the other direction.
**Tags:** writer, serialization, public-api, bindings

---

## Summary

Give Anvil Native a way to *produce* ANVL, not just read it. The first slice is a forward-only,
event-style **streaming writer** that emits canonical AML/AMP text, validated against the real
grammar so that whatever it accepts, the reader parses back identically.

## Where it lives, and why

Scoped as three layers (decision recorded 2026-10-09):

| Layer | What | Lives in |
|---|---|---|
| 1. Emission | value tree → ANVL text (quoting, escaping, blob/attribute syntax, dialect rules) | native: `include/anvil_writer*.h`, `src/writer/` |
| 2. Builder | construct a document programmatically | native, same add-on (**not started**) |
| 3. Object mapping / codecs | host object ↔ ANVL; later binary serialization | bindings, built only on layers 1–2 |

Layers 1–2 are native because the grammar knowledge (reserved words, bare-literal rules, numeric
grammar, AMP restrictions) must exist once; a per-binding writer is where bindings start to drift.
The writer is an add-on, not part of the reader: **separate headers** (`anvil_writer_types.h`,
`anvil_writer.h`, `anvil_writer_vtable.h`) that include no reader header, and **separate sources**
(`src/writer/`) that link no reader code. `make WITH_WRITER=0` builds a reader-only library;
`make check-reader-only` proves it contains no writer symbols.

## What shipped

- Streaming API: header (`attribute`, `include`), `statement(name, base)`, every scalar
  (`null`/`bool`/`numeric`/`int`/`double`/`string`/`bare`/`blob`/`varref`), `begin_`/`end_` for
  `array`/`tuple`/`object`, `finish`, `data`. A statement is completed by exactly one value call.
- Canonical output: `#!aml`/`#!amp` shebang, 3-space indent, one `name := value;` per line, scalar
  collections inline, objects multi-line. No comments, no layout preservation.
- Validation (sticky first error, `anvil_writer_get_error`): identifiers and reserved words,
  numeric grammar, bare literals that wouldn't read back as BARE, blob tags/content, attribute
  values, include paths, empty arrays/objects and 1-tuples, base-requires-object, call-order
  errors, and the AMP restrictions (no attributes/include/base/object/varref, scalar-only
  collections).
- Vtable `Writer` (`anvil_writer_i`), pointer-identical to the flat functions — the one struct a
  binding binds, so its surface can't drift from the flat API.
- Memory: the writer owns a growable buffer; `anvil_writer_data` borrows it after `finish`.

## Tests

- `test/unit/test_writer.c` — exact-output and error assertions. Links **only** `src/writer/` and
  testbit (no reader code), which is itself the standalone proof. Includes vtable pointer identity.
- `test/unit/test_writer_roundtrip.c` — the drift gate against the real parser: every scalar kind,
  escapes, edge tokens, collections, attributes/base/varref/AMP, a real `include` through the include
  loader, a **fixed-point copy of every loadable `test/fixtures/*.anvl`**, and a seeded pseudo-random
  pass asserting that anything the writer accepts reads back as the same kind and text.
- Both suites are clean under valgrind.

## Decisions (resolved)

- **Canonical, not layout-preserving.** Preserving comments/layout would need a lossless tree the
  parser doesn't keep.
- **Unresolved structure.** The writer emits what the caller streams (varrefs, `base`, `include`);
  it resolves nothing.
- **Writer-owned buffer.** No sink callback yet.
- **Strictness over leniency.** `numeric` takes the documented `-?d+(.d+)?([eE][+-]d+)?` even though
  the reader also accepts `1.`; `bare` rejects anything the reader would classify otherwise,
  including `1.`.

## Findings along the way

- The flat reader API doesn't expose a blob's tag or a statement's inheritance base, so a
  reader→writer copy loses both (the tests copy blobs untagged and inherited objects as their merged
  fields). A lossless reader→writer round trip would need those accessors.
- The reader's numeric grammar is looser than the documented one (`1.` is NUMERIC).

## Not started

- **Document builder** (layer 2): an in-memory tree you assemble and emit — and the natural place for
  the deferred "attach a dynamically-built document as an include" idea (`notes/deferred-work.md`).
- **Binding wrappers**: every binding wraps the `Writer` vtable one-to-one. Node/WASM first need the
  deferred `sigma.system.alloc` vendor bump.
- **Conformance corpus** shared by all bindings (golden outputs).
- **Binary serialization / codecs** (layer 3).
