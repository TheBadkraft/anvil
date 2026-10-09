# FR-2609-anvl-writer-001: a streaming writer and document builder in Anvil Native

**ID:** FR-2609-anvl-writer-001
**Type:** Feature Request (native library / public API)
**Owner:** anvl
**Filed:** 2026-10-09
**Filed by:** anvil
**Status:** in progress — the streaming writer and the document builder (each flat + vtable) are
implemented and tested; binding wrappers: `anvil.java` done, the rest not started
**Continues:** [`FR-2609-anvl-public-api-001`](FR-2609-anvl-public-api-001.md) — the public API there is
read-only; this adds the other direction.
**Tags:** writer, serialization, public-api, bindings

---

## Summary

Give Anvil Native a way to *produce* ANVL, not just read it. The first slice is a forward-only,
event-style **streaming writer** that emits canonical AML/AMP text, validated against the real
grammar so that whatever it accepts, the reader parses back identically. The second is a **document
builder** on top of it: assemble a tree in any order, then emit it.

## Where it lives, and why

Scoped as three layers (decision recorded 2026-10-09):

| Layer | What | Lives in |
|---|---|---|
| 1. Emission | value tree → ANVL text (quoting, escaping, blob/attribute syntax, dialect rules) | native: `include/anvil_writer*.h`, `src/writer/` |
| 2. Builder | construct a document programmatically | native, same add-on: `include/anvil_builder*.h` |
| 3. Object mapping / codecs | host object ↔ ANVL; later binary serialization | bindings, built only on layers 1–2 |

Layers 1–2 are native because the grammar knowledge (reserved words, bare-literal rules, numeric
grammar, AMP restrictions) must exist once; a per-binding writer is where bindings start to drift.
The writer is an add-on, not part of the reader: **separate headers** (`anvil_writer_types.h`,
`anvil_writer.h`, `anvil_writer_vtable.h`, and the builder's `anvil_builder_types.h`, `anvil_builder.h`,
`anvil_builder_vtable.h`) that include no reader header, and **separate sources**
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

## What shipped: the document builder

- Handles: `anvil_builder` (the document), `anvil_node` (a value), `anvil_member` (a statement). The
  builder owns everything made through it.
- Values are created **detached** (`null`/`bool`/`numeric`/`int`/`double`/`string`/`bare`/`blob`/
  `varref`/`array`/`tuple`/`object`), then attached exactly once: `append` into an array/tuple, or
  `add(object-or-NULL, name, value)` as a statement (NULL = the document). `set_base`,
  `member_attribute[_string]`, `find`, and module `attribute`/`include` complete the surface.
- **Emission goes through the streaming writer** (`anvil_builder_write(b, w)` into a caller's writer,
  `anvil_builder_emit` for text), so output and grammar rules are exactly the writer's; both share
  `src/writer/grammar.c`.
- Eager vs emit-time errors: anything decidable from the call alone fails at that call (bad name/
  numeric/bare/blob/attribute, AMP-forbidden constructs, a node attached twice, a cycle, a node from
  another builder, a repeated top-level name, a base on a non-object). What needs the finished tree
  (empty array/object, 1-tuple, nesting past `ANVIL_BUILDER_MAX_DEPTH` = 256) fails at emit.
- **Errors are not sticky** (unlike the writer): a failed call changes nothing and
  `anvil_builder_get_error` reports the most recent call. Error codes are the writer's.
- Safe on adversarial depth: disposal and the depth check are iterative, and a 200000-deep tree
  disposes cleanly.
- Both layers expose a flat API and a vtable (`Writer`, `Builder`) - the vtable is the one struct a
  binding binds, pointer-identical to the flat functions.

A gap this exposed in the writer, fixed here: the reader's resolver rejects a repeated top-level
name, so the writer (and builder) now reject it too (`ANVIL_WRITER_ERR_DUPLICATE_NAME`); names inside
objects may repeat. Names pulled in by an `include` are invisible to the writer.

## Tests

- `test/unit/test_writer.c` — exact-output and error assertions. Links **only** `src/writer/` and
  testbit (no reader code), which is itself the standalone proof. Includes vtable pointer identity.
- `test/unit/test_writer_roundtrip.c` — the drift gate against the real parser: every scalar kind,
  escapes, edge tokens, collections, attributes/base/varref/AMP, a real `include` through the include
  loader, a **fixed-point copy of every loadable `test/fixtures/*.anvl`**, and a seeded pseudo-random
  pass asserting that anything the writer accepts reads back as the same kind and text.
- `test/unit/test_builder.c` (links the add-on only; exact-output tests reuse the writer's expected
  text) and `test/unit/test_builder_roundtrip.c`: a hand-built document through the parser; a
  fixed-point copy of every loadable fixture through the builder; and 500 seeded random trees whose
  builder output must equal the streaming writer's byte for byte and parse.
- All four suites are clean under valgrind.

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

- **Builder follow-ups**: removing/replacing a statement or element, and the deferred "attach a
  dynamically-built document as an include" idea (`notes/deferred-work.md`), which needs the include
  loader to accept an in-memory source.
- **Binding wrappers**: every binding wraps the `Writer`/`Builder` vtables one-to-one.
  - `anvil.java` — **done** (`AnvilWriter`, `AnvilBuilder`; 24 marshaling tests).
  - `anvil.net`, `anvil.py` — not started (bump `vendor/anvil` first).
  - `anvil.node`, `anvil.wasm` — not started; first need the deferred `sigma.system.alloc` vendor bump.
- **Conformance corpus** shared by all bindings (golden outputs).
- **Binary serialization / codecs** (layer 3).
