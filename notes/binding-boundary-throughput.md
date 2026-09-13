# Binding-boundary throughput — how to not lose the parser's speed at the FFI wall

## Status

**Reference.** Not a design-in-progress — this is a synthesized guide, written after the fact
from a real, resolved investigation (`BR/BR-2609-anvl-004.md`), for whoever builds or touches the
*next* binding (or the next hot path in an existing one). None of the binding source lives in this
repo — `anvil-node` and `anvil.wasm` are sibling repos; this repo is Core Anvil Native (pure C).
This note exists here anyway because the lesson is about the boundary architecture, which is a
decision every binding author for this project has to make, and it belongs next to the rest of the
project's design record rather than being re-derived per binding.

## Context

`BR-2609-anvl-004` found that for FlyWire's `@table` wire shape (10,000 rows × 10 fields = 100,000
leaf scalars), **Core Anvil Native was never the bottleneck** — `anvil_parse_value_fragment` was
genuinely linear, ~9.86ms at n=10,000, verified directly via `anvl_parser_set_hook`
instrumentation. The cost was one level up, entirely inside each binding's own layer for
converting an already-parsed `anvil_value` tree into a host-language value. Two different
bindings, two different mistakes:

- **`anvil-node`** built one real `napi_value` per leaf — a `napi_create_string_utf8`/
  `napi_create_double`/`napi_set_element` N-API call per field, at volume. Isolated cost of just
  that pattern, no parsing or malloc involved: **~7–10ms** for this shape — comparable to or
  exceeding the actual parse cost.
- **`anvil.wasm`** already used the right *shape* of strategy (build one JSON string in C, cross
  the boundary once, let the host's native `JSON.parse()` materialize the tree) but its own
  `jsonbuf_append_json_string` and numeric-conversion code had two real inefficiencies that ate
  into the savings anyway (below).

The fix, and the full before/after numbers, are in `BR/BR-2609-anvl-004.md`; this note pulls out
the reusable principles so the next binding gets the architecture right the first time instead of
rediscovering this.

## Principle: crossing count dominates, not per-crossing payload size

The generalizable finding isn't "N-API is slow" — it's that **the number of boundary crossings**,
not the total bytes moved, is what the isolated benchmark actually showed costing 7–10ms with zero
parsing or allocation involved. One call moving a large buffer is cheap relative to many calls
moving small values, because each `napi_*` call carries fixed overhead (handle scope bookkeeping,
type checks, engine/GC interaction) independent of how small the value is.

**Consequence for architecture:** any binding converting a tree-shaped `anvil_value` result into a
host value should default to a **single-crossing strategy** — serialize the whole result on the
native side, cross once, deserialize with the host engine's own native (C++-implemented, not
script-level) fast path. `JSON.parse()` is the concrete instance for JS hosts; the equivalent for
another host language is whatever its runtime provides as a native bulk-deserializer for a compact
wire format.

Building the target tree directly (one native handle per node) should be treated as the
*exception*, justified only when the shape is small/bounded (a handful of top-level fields, not
scaling with document size) — not the default strategy for anything that scales with row/record
count.

## Principle: batching only pays off if the serializer itself is fast

`anvil.wasm` already had the right crossing-count architecture and still needed two independent
fixes, because a slow per-item inner loop can eat the exact savings the batching strategy bought:

1. **Don't call a checked/capacity-managed append function per character (or per element) when
   most spans need no transformation at all.** The escaping loop was doing exactly this — one
   function call with a capacity check per character, even when nothing needed escaping. Same
   class of bug as `BR-2609-anvl-002` (bulk-scan blob content instead of byte-by-byte), just
   relocated to the binding layer instead of core Anvil Native. Fix: bulk-scan a span to decide
   *whether* it needs escaping at all, and `memcpy` it through in one shot when it doesn't.
2. **Don't round-trip through a general-purpose numeric formatter when the source grammar is
   already a subset of the target grammar.** Numeric conversion was going through `strtod` +
   `snprintf("%.17g", ...)` (~106ns/call, measured) for every number. ANVL's numeric grammar is a
   strict subset of JSON's, with one exception (a leading zero) that needs a cheap check and a
   rare fallback — the common case can copy the raw source text straight through. This generalizes:
   whenever the wire/source text for a value is already valid in the target format, copy bytes,
   don't reparse-and-reformat.

Isolated numbers from the same investigation, same 813KB-scale shape, showing both fixes compound
(from `BR-2609-anvl-004`'s verification table):

| variant | time |
|---|---:|
| JSON-string build, naive port | 7.55ms |
| JSON-string build, escaping fast path only | 6.41ms |
| JSON-string build, both fast paths | 3.35ms |

## Principle: benchmark the real call site, not just the isolated mechanism

An earlier reported figure for the `anvil-node` fix (34.3%) was wrong — not fabricated, just an
apples-to-oranges comparison: an isolated prototype measuring "native conversion + `JSON.parse()`"
alone, set against a baseline that *also* included the consumer's own JS-side row-remapping loop
(`Table.decode()`'s own post-processing), which costs the same in both the old and new
implementation and so shouldn't be inside the delta being reported. It was caught and corrected
before being treated as final. The honest, real-harness number — built module, FlyWire's actual
`Table.decode()`, five consecutive runs — was **~18% faster** (27.057ms → ~22.2ms).

Two distinct kinds of benchmark serve different purposes and neither substitutes for the other:

- **Isolated microbenchmark** (e.g. "build this exact shape via raw N-API calls, no parsing, no
  malloc") is how you *prove which layer* the cost is in, the way this investigation proved core
  Anvil Native was innocent via `anvl_parser_set_hook` before ever looking at the binding code.
- **Real-harness end-to-end benchmark** (real built module, real consumer code path, several
  consecutive runs) is the only number that should ever be reported as "N% faster" — it's the only
  one that can't hide a wash like the row-remapping loop inside its baseline.

## Checklist for the next binding (or the next hot path in an existing one)

1. Default to **serialize-once / cross-once / deserialize-natively** for any tree-shaped result
   whose size scales with the document (records, rows, array length) — not per-leaf native-value
   construction.
2. If you must build host-native values directly (small, bounded shape only), batch construction —
   preallocate collections by known length, and don't re-check capacity/type per element.
3. In the serializer itself: bulk-scan for "does this span need transformation at all" before
   falling into a per-unit slow path; copy raw bytes through whenever the source format is already
   valid in the target format.
4. Prove which layer owns a suspected cost before changing it — instrument the core parser
   (`anvl_parser_set_hook` or equivalent) separately from the binding's own conversion code, the
   way this investigation ruled out core Anvil Native first.
5. Report performance deltas only from a real-harness, real-consumer, multiple-consecutive-run
   measurement. Treat an isolated-prototype number as a hypothesis to verify against the real path,
   not a result to publish — this investigation's own first pass (34.3%) got caught precisely
   because it wasn't re-checked against the real call site before being written down.
6. Confirm the fix is behavior-preserving with the existing test suite unmodified, then add new
   tests for the specific previously-unexercised paths the fix touches (this investigation added:
   leading-zero numeric fallback, escape correctness at buffer/run boundaries, buffer growth beyond
   initial capacity, the real reported wire shape end to end).

## Cross-references

- `BR/BR-2609-anvl-004.md` — the full investigation this note is drawn from: root cause, fix, and
  complete before/after numbers for both `anvil-node` and `anvil.wasm`.
- `BR/BR-2609-anvl-002.md` — the core-Anvil-Native precedent for the same class of bug (byte-by-byte
  scanning instead of bulk-scan), relocated to the binding layer in `-004`.
- `notes/flywire-parse-scaling-benchmark.md` — a different FlyWire-reported binding-boundary
  finding (large single-blob scaling, `anvil-node` vs. vendored `anvil.js`), same theme of "the
  boundary/binding layer, not core parsing, is where the real cost hides" but a different mechanism
  (allocation/copy scaling, not call volume) — worth reading alongside this note, not a duplicate
  of it.
- `anvil.net` is noted in `BR-2609-anvl-004` as currently unaffected by this whole class of cost —
  it doesn't wrap Anvil Native today (no `DllImport`/P-Invoke). Whoever builds that binding for real
  should read this note *before* choosing an architecture, not after discovering the same problem
  a fourth time.
