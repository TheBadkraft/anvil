# FlyWire feedback — anvl.parse() scaling on a single large blob value

**Reported by:** FlyWire | **Date:** 2026-09-10 | **Status:** benchmark
data for the ANVL team to interpret and quantify — not a bug report, not
an FR. Written the way `notes/public-api.md`'s FR was: real numbers,
real reproduction steps, a stated hypothesis clearly marked as ours and
unverified, not asserted as diagnosis.

## Summary

Comparing `anvil-node` against the vendored, pure-JS `anvil.js` on
`anvl.parse()` of FlyWire's real AMP wire text (one large base64-encoded
blob value inside a small envelope), we found:

- `anvil-node` wins decisively on a process's first-ever parse call, at
  every size tested (8.5KB to 814KB) — no fixed startup cost we could
  detect.
- `anvil.js` pays a real, one-time fixed cost (~0.6–1.2ms) on its own
  first-ever parse call in a process, regardless of string size — after
  that, its cost scales linearly with size, with a *shallower* per-byte
  slope than `anvil-node`'s.
- Once both sides are past that one-time cost, `anvil-node`'s cost
  scales linearly too, but steeper — the two cross over somewhere
  around 1,000–2,000 records (~82–163KB), and `anvil-node` ends up
  **~1.8x slower** than warmed `anvil.js` at 10,000 records (814KB), on
  this specific "one big blob string" document shape.

This is the opposite of the small/medium-payload result FlyWire
expected from earlier testing (`notes/anvl-native-surface.md`,
`anvil-node`'s own `npm test` suite) — those never exercised a single
string value anywhere near this large. We think this is worth your
team's attention because FlyWire's real wire format (`AMPacket.pack()`)
always frames its binary payload as exactly this shape: one long
base64 string inside a small envelope, and that payload's length grows
directly with the number of records in a message.

## Methodology

**What's measured:** `anvl.parse()` only — nothing else in FlyWire's
pipeline (query, `Codec.encode()`, `AMPacket.pack()`, `AMPacket
.unpack()`, `Codec.decode()`) differs between backends at all; those
stages are identical code, identical cost, regardless of which parser
receives the AMP text. Isolated by running each backend in its own
process (`FLYWIRE_ANVL_BACKEND` pins the backend for a whole process —
`src/anvl/index.js` resolves it once, at first require) against the
exact same rows, real Postgres data (`flywire_dev.assets`, the
10,000-row `BULKGEN` bulk-seeded set, not synthetic).

**Document shape:** `AMPacket.pack()`'s real output — a short header
(`utid`, `payload_size`) plus one `blob := "<base64>";`-shaped value
holding the entire encoded record set. At `n=10,000` that's roughly a
610KB binary payload, ~814KB once base64-inflated and wrapped.

**Two conditions, both real, both worth reporting:**

- **Single-call** — one real `anvl.parse()` call per document size, in
  a process that has already parsed *other*, differently-sized strings
  before it (this is what a real FlyWire benchmark run looks like:
  `bench/native-backend-compare.js` runs scales 100 → 1,000 → 10,000
  sequentially in one process, per backend). Not a true "cold JVM"
  measurement — the engine/runtime is generally warm, but this
  *specific* string, at this *specific* size, has never been parsed
  before in this process.
- **Warmed** — immediately preceded by one throwaway `anvl.parse()`
  call on the *identical* string, same process, result discarded, then
  timed again. Isolates any one-time-per-distinct-input cost (e.g. a
  fresh buffer allocation sized to that string) from steady-state,
  repeated-parse cost.

**Reproduce:** in the FlyWire repo (sibling to this one), with
Postgres seeded (`npm run db:seed:bulk`):

```
node bench/native-backend-compare.js
```

That script (`bench/native-backend-compare.js` +
`bench/backend-worker.js`) reports the single-call condition, at
n=100/1,000/10,000, averaged informally over three separate full runs
(consistent every time — not run-to-run noise). The warmed condition
and the intermediate scales (2,000/3,000/5,000/7,000) below came from a
short ad hoc script built on the same primitives (`Codec`, `AMPacket`,
`anvl.parse`), not currently checked into the repo as a permanent
script — happy to add it if useful to you.

## Data

### Single-call (first touch of this string, in an already-running process)

| n | bytes | anvil-js | anvil-node | anvil-node vs anvil-js |
|---:|---:|---:|---:|---:|
| 100 | 8,564 | 1.221ms | 0.118ms | **10.3x faster** |
| 1,000 | 81,765 | 0.247ms | 0.264ms | ~tied |
| 2,000 | 163,102 | 0.439ms | 0.609ms | 1.4x slower |
| 3,000 | 244,434 | 0.489ms | 0.824ms | 1.7x slower |
| 5,000 | 407,102 | 0.869ms | 1.536ms | 1.8x slower |
| 7,000 | 569,766 | 1.124ms | 1.956ms | 1.7x slower |
| 10,000 | 813,766 | 1.699ms | 3.061ms | 1.8x slower |

(The n=100 anvil-js number is the outlier that gave away the fixed
cost — every later call, at every size, drops to sub-millisecond
territory immediately. This is consistent across three independent
full runs of `bench/native-backend-compare.js`, not a one-off.)

### Warmed (one throwaway parse of the identical string first)

| n | bytes | anvil-js | anvil-node | anvil-node vs anvil-js |
|---:|---:|---:|---:|---:|
| 100 | 8,564 | 0.625ms | 0.031ms | **20x faster** |
| 1,000 | 81,765 | 0.183ms | 0.236ms | 1.3x slower |
| 2,000 | 163,102 | 0.319ms | 0.477ms | 1.5x slower |
| 3,000 | 244,434 | 0.532ms | 0.708ms | 1.3x slower |
| 5,000 | 407,102 | 0.804ms | 1.417ms | 1.8x slower |
| 7,000 | 569,766 | 1.260ms | 2.059ms | 1.6x slower |
| 10,000 | 813,766 | 1.530ms | 2.723ms | 1.8x slower |

Even warmed, `anvil.js` still shows a smaller residual cost at n=100
than its own trend line from n=1,000 onward would predict (0.625ms vs.
an extrapolated ~0.02–0.03ms) — we suspect our "warmed" condition
doesn't fully eliminate whatever the one-time cost is, only reduces
it. We haven't chased that further.

## Analysis

Fitting the warmed numbers roughly linearly against byte count (crude,
endpoint-to-endpoint, not a real regression):

- `anvil.js`, n=1,000→10,000: (1.530 − 0.183)ms over (813,766 −
  81,765) bytes ≈ **1.84µs per KB**.
- `anvil-node`, same range: (2.723 − 0.236)ms over the same byte
  range ≈ **3.40µs per KB**.

So in steady state, on this document shape specifically, `anvil-node`
costs roughly **1.8x more per byte** than `anvil.js`. That factor
matches the "1.8x slower" ratio observed directly at n=10,000 in both
conditions above — the per-byte slope, not noise, is the actual
difference.

**A different document shape, for contrast (not apples-to-apples, a
separate one-off test):** parsing a ~1.68MB document shaped as 100,000
small, distinct top-level field assignments (`f0 := 12345; f1 := ...;`)
instead of one large blob took `anvil-node` ~95–125ms — wildly worse
per-byte than the blob case above. We didn't test `anvil.js` on the
same shape, so we can't say whether that's `anvil-node`-specific or a
"many small tree nodes is expensive on both sides" effect — flagging
it only because it shows the cost isn't simply proportional to total
byte count in general; document *shape* (one big string vs. many small
tokens) appears to matter a lot, at least on the `anvil-node` side.

## FlyWire's hypothesis (ours, unverified — not a diagnosis)

Our own working guess, not checked against `anvil-node`'s C source: a
buffer resize/reallocation cost that scales with the length of the one
large string/blob value being processed — plausible candidates being
the N-API boundary crossing itself (copying the incoming JS string into
a native buffer for the C parser to read) or an internal buffer that
grows incrementally rather than being sized once up front for a value
of known length. We don't have evidence for either specific mechanism
— just the shape of the data above, which is consistent with *some*
per-byte cost on `anvil-node`'s side that `anvil.js` doesn't pay in the
same proportion once its own one-time cost is out of the way.

## What we're asking

Not asking for a fix blind — asking whether your team can characterize
this precisely: is there a known cost in `anvil-node`'s (or `libanvil`
itself's) handling of one very large string/blob token that would
explain a ~1.8x-per-byte gap versus a JS engine's native string
scanning? Is it N-API marshalling specifically, or something in the
parser itself? And practically: at what payload size (if any) does
this stop mattering relative to ANVL's other advantages — FlyWire's
real production message sizes are the thing we'd want to know that
against, not an abstract threshold.

## Anvil team follow-up — mechanism found, both in core Anvil Native

**Status: characterized and verified, fix in progress.** Filed as
[`BR-2609-anvl-001`](../BR/BR-2609-anvl-001.md). Neither cause is N-API
marshalling — both live in `src/core/anvil_flat.c`, in the code path every
string-valued read goes through (`anvil_value_get_text`), so they affect
every binding equally, not just `anvil-node`. FlyWire's wire shape (one huge
string, and — because it's base64 — containing no backslash-escape bytes at
all) happens to land on exactly the pattern that triggers both.

**1. A redundant third full-string scan.** `anvil_value_get_text` always
calls `resolve_string_escapes()` once with a `NULL` buffer purely to compute
`needed` (the resolved length), then calls it a second time into the real
caller-supplied buffer — discarding that second call's own accurate return
value in favor of the first. `anvil-node`'s own `CopyAnvlText` helper
(`binding.c`) already calls `anvil_value_get_text` twice itself (once to
size, once to copy), so one conversion of one string currently costs three
full passes over the bytes where two are achievable.

Verified with a standalone microbenchmark (`resolve_string_escapes` copied
verbatim, 813,766-byte base64-alphabet buffer — FlyWire's own n=10,000 size —
2,000 iterations): eliminating the redundant third scan (by making
`anvil_value_get_text` trust the real call's own return value when a real
buffer is supplied) took the per-call cost from **1.0696ms to 0.7232ms —
32.4% saved**, independent of the fix below.

**2. No fast path for the zero-escape case.** `resolve_string_escapes` is a
per-byte branching loop (checking every byte for `\`) regardless of whether
the string contains any backslash at all. Base64 text — FlyWire's exact
payload shape — never does. A `memchr`-for-any-backslash check followed by a
single `memcpy` when none is found (falling back to the existing per-byte
loop, completely untouched, whenever a real backslash is present) is
dramatically faster for this common case.

Verified with the same microbenchmark/buffer: the per-byte loop cost
**0.3627ms per call**; the `memchr`+`memcpy` fast path cost **0.0060ms per
call — 98.3% saved, roughly 60x**.

Both fixes are behavior-preserving (identical resolved output for every
input, including strings that do contain real escapes) and land entirely in
core Anvil Native, so the benefit reaches `anvil-node`, `anvil-wasm`, and any
future binding without binding-specific work. We'll report real
before/after numbers against your own `bench/native-backend-compare.js`
shape (not just the isolated microbenchmark above) once the fix lands and
passes full regression.

## Real-world implication for FlyWire, stated plainly

FlyWire's server process (`harness/server.js`) is long-lived — it
parses many messages over its life, not one per process. That means
`anvil.js`'s one-time fixed cost is real but gets amortized away almost
immediately in production; what matters for FlyWire's actual workload
is the **warmed** numbers above, not the single-call ones. On that
basis, for FlyWire's specific wire shape (one large base64 blob per
message) at FlyWire's production-relevant scale (10,000 records,
~814KB), `anvil.js` currently comes out faster than `anvil-node`, not
slower — the opposite of what we expected and the opposite of what
small-payload testing had shown us. We're not drawing a migration
conclusion from this yet; we want your read on the mechanism first,
per the above.
