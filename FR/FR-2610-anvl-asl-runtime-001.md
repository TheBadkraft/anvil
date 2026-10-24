# FR-2610-anvl-asl-runtime-001: the AnvilScript compiler and bytecode VM

**ID:** FR-2610-anvl-asl-runtime-001
**Type:** Feature Request (native library / runtime)
**Owner:** anvl
**Filed:** 2026-10-24
**Filed by:** anvil
**Status:** open — not started
**Continues:** [`notes/anvilscript-design.md`](../notes/anvilscript-design.md) and
[`notes/anvilscript-scriptengine-design.md`](../notes/anvilscript-scriptengine-design.md) — converts
the "tentative" bytecode-VM direction recorded there into a committed decision, and scopes the
actual implementation (compiler, VM, host embedding) as a trackable FR.
**Tags:** anvilscript, asl, runtime, compiler, vm, host-embedding

---

## Summary

`src/asl/asl_body_parser.c` (the AST parser) is landed and tested (`ASLAST01`–`11`). Nothing past
it exists in code yet: no compiler, no VM, no host callback registration. This FR scopes that next
layer — the actual AnvilScript runtime — and commits the evaluation-model decision that
`anvilscript-design.md`'s "Tree-walk vs. bytecode" section left tentative.

## Decision: bytecode VM, not tree-walk (committed, was tentative)

`anvilscript-design.md`'s comparison table already made the case; this FR drops the hedge. The
tree-walk alternative's main advantage — low implementation cost because "the AST already exists"
— is weaker now than when that table was written, since the AST is landed and stable. The two
things bytecode buys (natural `.anvlo` compilation later; a clean `OP_CALL`/`OP_HOST_CALL` split for
the registry design) are both load-bearing for the host-embedding goal below, not speculative.

The AST remains the canonical IR and still feeds the compiler exactly as designed — no tree-walk
evaluator is written.

## Why Outpost changes the sequencing

`../outpost` already consumes `anvil.net` from a local build (`nuget.config` →
`../anvil.net/src/Anvil.Net/bin/Release`) for the object model. The next step there is running
AnvilScript inside Outpost and letting Outpost register its own game-specific built-ins (e.g.
`world.spawn_npc`) callable from ASL source — extending toward a custom DSL. The VM alone doesn't
deliver that: a host needs a generic registration path (`anvilscript-design.md`'s Design Thread #3,
Host Callback Binding) and a way for ASL source to reach it by qualified name (Design Thread #1,
`import`). Both are pulled into this FR's scope instead of waiting as open design threads, since
Outpost is a concrete near-term consumer, not a hypothetical one.

## Scope

**In scope:**

1. **Compiler** — AST → bytecode, per `anvilscript-scriptengine-design.md`'s "Compiler" section:
   single pass per function body, flat instruction stream, constant pool, per-instruction source
   spans for error reporting.
2. **VM** — frame chain, operand stack (the arena-backed `Stack` from
   `FR-2603-sigma-collections-011`), the `OP_*` instruction set as specced, the three-tier
   exception channel (`off`/`errors`/`trace`).
3. **Host callback registration** (Design Thread #3) — a generic
   `anvil_register_function(doc, "qualified.name", callback)` surface, paradigm-agnostic, plus
   `OP_HOST_CALL`.
4. **`import` declarations** (Design Thread #1) — at minimum the `c:` resolver, so a host's
   registered callbacks are reachable from ASL source by qualified name.

**Deliberately out of scope** (stay as open design threads or later FRs):

- Opt-in strong typing (Design Thread #4) — dynamic typing only for this FR.
- Built-in distribution formats beyond source `.anvs` (Design Thread #2) — no `.anvlo`
  object-file format yet.
- Closures — explicitly post-MVP per the design doc.
- Collection type-locking / generics.
- Bytecode version *migration* tooling (the header fields exist per the design doc; nothing reads
  a mismatched minor version differently yet).
- Binding wrappers (`anvil.net`/`.py`/`.java`/`.node`/`.wasm` exposing the ScriptEngine) — follow-on
  FRs once the native runtime lands, same sequencing `FR-2609-anvl-writer-001` used.

## Design references (not duplicated here)

- [`notes/anvilscript-design.md`](../notes/anvilscript-design.md) — Decisions 1–16, Design threads
  1–5, the bytecode instruction set sketch, bytecode versioning header.
- [`notes/anvilscript-scriptengine-design.md`](../notes/anvilscript-scriptengine-design.md) —
  subsystem boundaries, parser/compiler/VM structures, frame layout, operand stack discipline,
  error reporting contract.
- `FR-2603-sigma-collections-007` / `FR-2603-sigma-collections-011` — the per-instance allocator
  override and arena-backed `Stack` the VM's operand stack and frame chain depend on. Both already
  implemented (2026-09-07); no new Sigma work needed to start this FR.

## Sequencing (TDD slices)

Same cadence `asl_body_parser.c` itself used (`ASLAST01`–`11`): small, test-first, vertical slices,
each landing a working subset end-to-end before the next starts.

1. Compiler + VM, literals and arithmetic only — no locals, no control flow, no calls. Proves the
   instruction stream / constant pool / operand stack mechanics.
2. `var` declarations, assignment, local slots (`OP_LOAD_LOCAL`/`OP_STORE_LOCAL`).
3. Control flow (`if`/`else`, `while`, `for`) — jump emission and patching.
4. `return`, calls between ASL functions (`OP_CALL`, frame push/pop, vararg/default-param
   binding).
5. Host callback registration + `OP_HOST_CALL` (Design Thread #3) and `import` with the `c:`
   resolver only (Design Thread #1).
6. The exception channel's three debug modes.

Each slice gets its own flat + vtable surface where the design doc calls for one, and its own test
file — matching every other subsystem in this repo.

## Open questions carried in, not resolved by this FR

- Closures — explicitly post-MVP, not blocking.
- `.anvlo` compiled-object format — bytecode is "natural" for it per the design doc, but the file
  format itself isn't scoped here.
- The Pratt binding-power table in `anvilscript-scriptengine-design.md` is marked "illustrative" —
  fine to firm up during slice 1, not before.

## Status

Open. Not started. No external dependency blocks starting — `FR-2603-sigma-collections-007` and
`-011` are already satisfied.
