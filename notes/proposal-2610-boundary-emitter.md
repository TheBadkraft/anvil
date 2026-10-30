# Proposal 2610: Boundary-Aware Emitter (with review)

**Status:** proposed. Not scheduled. The owner asked on 2026-10-30 for it to be kept with its context.
**Origin:** the owner's proposal, received 2026-10-30, reproduced verbatim in part A.
**Review:** the ANVL Principal Engineer, the same day, in part B.
**Decision state:** part C. Nothing here is approved work until the owner says so.

## Why this note exists

The context that led to the proposal is easy to lose:

- **The native add-on was slower than pure JS.** Through 2026-10, anvil-node (the native add-on) and anvil-wasm
  wrapped the C parser and carried every parse result across the boundary as a JSON string, which JS then rebuilt.
  The binding speed work (6ad7616) made that JSON transfer the fastest way through the add-on. But the copy and
  rebuild on every value still cost more than parsing it in JS. The owner's verdict: "it's just not worth it for a
  user to deploy ANVL on the web if JSON is natively faster on every metric."
- **anvil.js replaced both bindings.** It was built in its place (roadmap-2610-anvil-js-parity.md): a pure-JS
  implementation held to native by the native-owned conformance corpus (1,867 cases at native acb4602), with
  differential fuzzing and drift tables. Against anvil-node v0.10.0-rc it takes 0.09–0.20x the time to parse and
  0.12–0.45x to parse and read (lower is faster). anvil-node and anvil-wasm are frozen at v0.11.0-rc.
- **The proposal is about keeping one parser.** It asks whether the C parser could serve JS after all, without the
  boundary cost, by emitting spans and never strings. That would mean JS no longer needs a second parser.
- **It shapes the VM.** It also sets the AnvilScript VM's boundary design before any VM code exists.

---

## A. The proposal (verbatim)

### Proposal: Boundary-Aware Emitter for the ANVL Native Parser

**Status:** exploratory proposal, for review with the engineer
**Date:** 2026-10-30
**Scope:** the WASM boundary for the native parser, and (briefly) the future AnvilScript VM

---

#### 1. Background

The Node and WASM bindings wrapped the native C parser, which meant every parse result had to be converted to JSON to cross the boundary. That conversion was the painful part: for each value the binding read into the source, copied the string out, and serialized the copy, only for the JS side to rebuild the structure.

Those bindings are now frozen. ANVL.JS, a pure JavaScript implementation, is complete and held in parity with native by a shared fixture suite of roughly 1,800 tests. Native drives; ANVL.JS follows downstream. That arrangement stays as is.

#### 2. The idea

Keep one parser. Change only the **output stage** based on who is consuming.

- **Native clients (default):** the normal node tree, as today.
- **WASM boundary build:** an emitter that writes the node schema directly from the parse, in the format the JS side can consume with no further conversion.

The parser does not change. It still builds its arena structure of spans into the source. The emitter is a serializer that sits beside the writer: it walks the tree and emits it, the same way the writer emits AML.

Because JSON is not otherwise used in this ecosystem, this would be a **primitive-layer exception**, justified only because ANVL.JS cannot exist on the far side of a WASM boundary.

#### 3. Spans only

The emitted output carries **spans only**, never materialized strings.

- Each value is a type tag plus a start offset and length into the source.
- Structure (identifier, base, attributes, nested values) is carried as it is in the node tree.
- Errors carry code, line, column, and file, at the same fidelity as successful results.
- The JS side holds the source and slices on demand.

This removes the expensive step from the old approach: no string reads, allocations, or copies on the native side, and the output stays small. It preserves the zero-copy design across the boundary.

#### 4. What we expect to see

- A single parser codebase, with no parallel binding logic to keep in sync.
- No per-value copying across the boundary.
- A node schema that mirrors the ANVL.JS node, so the existing parity tests can compare native boundary output to ANVL.JS output directly.
- The same schema usable in reverse: the JS side hands the writer a node description and the native side reads it. Define it once, serve both directions.

#### 5. Open questions

1. **Offset units.** WASM sees UTF-8 bytes; JS strings index in UTF-16 code units. For ASCII they are identical, so this only matters for non-ASCII content. Leading option: the JS side keeps the source as UTF-8 bytes and decodes spans on demand, so offsets mean the same thing on both sides. Needs to be settled up front.
2. **Node schema.** The exact fields per node type, to be defined with the engineer. Goal: match what the ANVL.JS node exposes.
3. **Input direction.** The reader that accepts the schema for writing back out, and how much of the writer it needs to cover.
4. **Source buffer ownership.** Who holds the source, and for how long, in linear memory.

#### 6. The VM

The AnvilScript VM (C, compiled to bytecode, with WASM bindings) is a different problem, and the lesson is to avoid a text format there entirely.

- Load compiled bytecode into WASM linear memory once and run it there.
- Host calls pass integers and offsets through a small set of imported functions, not structured data.
- Runtime values that cross the boundary (strings, objects) use handles and offsets into shared memory rather than copies.
- The `.anvlo` compiled format is already self-contained and relocatable, so it fits this model.

This shapes the VM's host interface from the start, so it is worth raising early.

#### 7. Risk and fallback

This is a scoping exercise. ANVL.JS is finished and in parity, so if the emitter approach proves worse in practice, nothing is lost. We are not going backwards.

#### 8. Proposed next steps

1. Review this with the engineer and settle the offset-unit question.
2. Draft the node schema against the ANVL.JS node.
3. Prototype the emitter on a small subset (scalars, objects, errors) and compare against ANVL.JS output using existing fixtures.
4. Decide go or no-go from the prototype.

---

## B. Review (Principal Engineer, 2026-10-30)

**Verdict: worth a prototype, but the prize is one parser instead of two, not speed.**

### anvil.js already uses this layout
Since R0, anvil.js keeps the source as a UTF-8 `Uint8Array` and parses into one `Int32Array` per parse, 8 ints per
node, with lazy facades that decode a span only when it's read. That is §3 (spans only), and it answers open
question 1 (offsets are UTF-8 bytes on both sides). The emitter would be C writing **anvil.js's existing tree
format** into WebAssembly memory, which anvil.js's facade layer would read unchanged. Parity testing would get
stronger too: trees could be compared int for int, not just as decoded values.

### Where the gain actually is
- **Speed (modest, unproven).** The emitter replaces only the parse step. Reading costs the same either way, and
  reads dominate real workloads. Small documents are where anvil.js trails `JSON.parse`, and a WebAssembly call
  plus copying the source in adds a fixed cost, so small documents could get slower. Large documents might win.
  Only a benchmark will tell.
- **Maintenance (real).** If the WebAssembly parser became the only JS parser, anvil.js's parser would be deleted.
  Most of the work of keeping two parsers matched for the reader (corpus re-pins, fuzzing, drift) would go away.

### Obstacles
1. **King & Crucible's MUST is no `.wasm` and no `ready()`** (roadmap §6d), and anvil.js meets it today. Browsers
   load WebAssembly asynchronously, which brings back an init step. So a WebAssembly reader could only sit
   alongside pure anvil.js as an option, which means the JS parser still has to be maintained. It could replace
   anvil.js only if K&C relaxes the requirement.
2. **§4, the writer in reverse: recommend dropping it.** Building output text in JS is cheap. Handing a node
   description to C means converting data at the boundary again, the cost this whole effort left behind. The
   anvil.js writer phase (started 2026-10-30) stays in JS, held byte for byte to native through the
   writer-script corpus (conformance/FORMAT.md, "anvil-writer-script 1").

### §6, the VM: agreed completely
The VM loads bytecode into WebAssembly memory once. Host calls pass only integers and offsets through a small set of
imported functions, and values cross as handles. This matches the Principal Engineer's earlier direction: a C VM
with a C ABI, a portable versioned bytecode, and a benchmark of a WebAssembly build against a small pure-JS
interpreter before committing either way. It belongs in the VM's host-interface design from day one
(FR-2610-anvl-asl-runtime-001). The owner also asked whether the VM should be C++; the answer was C, so the C ABI,
WebAssembly size and toolchain stay one.

### Two facts to correct
- `.anvlo` is not implemented. notes/anvlo-compilation.md says "Concept / design in progress. No implementation
  yet", so "already self-contained and relocatable" describes the design, not code.
- anvil.js is complete as a reader only. The writer phase started 2026-10-30; types, schema and builder follow.

### One format, three uses
The `.anvlo` design's `nodes` section ("parsed AST nodes with offsets into `source`") is the same span tree. One
canonical, versioned node-tree format, defined in native, could serve:
1. anvil.js's in-memory tree;
2. this boundary emitter's output;
3. the `.anvlo` nodes section.

That is worth doing whatever happens with the emitter.

---

## C. Proposed plan (awaiting owner approval)

1. **Now (documentation only).** Record anvil.js's tree layout as the candidate canonical node-tree format, versioned
   and owned by native, and link it from roadmap-2610-anvil-js-parity.md and anvlo-compilation.md.
2. **VM design.** Adopt §6 as a requirement of the AnvilScript VM host interface: bytecode resident in linear memory,
   integer and offset host calls, and handles for values.
3. **After AnvilScript's compiler and VM: a timeboxed spike.** Prototype the emitter (§8 step 3: scalars, objects,
   errors), with C emitting anvil.js's tree for the corpus. Measure, labelled per agents/STANDARDS.md §8:
   - parse + read vs pure anvil.js, on all workloads and on the 1–10,000-message ladder;
   - WebAssembly cold start and bundle size;
   - int-for-int tree equality with anvil.js across the corpus.
4. **Go/no-go.** Go only if parse + read is no slower than pure anvil.js on any workload, and King & Crucible
   accepts an init step, or it ships as an opt-in alongside pure anvil.js. Record the result in this note either
   way.
5. **Not before AnvilScript.** The owner put scripting next (parser, compiler, VM, DSL registry), and this blocks no
   one.
