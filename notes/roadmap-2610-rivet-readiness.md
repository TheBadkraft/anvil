# Roadmap: rivet-readiness (types, schema, bindings)

> **Status (2026-10-29):** Phases 1–4 are done. Phase 5 continues as the Release Engineer's
> assignment in `agents/assignments/release-v0.10.0-rc.md`. The agent names below predate the move
> to one session with sub-agents (`agents/README.md`): the coder agent is now the Core Engineer, the
> Bindings agent the Bindings Engineer, and the Release agent the Release Engineer.

> **NOTE to the ANVL coder agent: follow this roadmap strictly.**
> - Your scope is **Phases 1–3 plus Addendum A1–A2**: the native tickets numbered 1–12 below, in
>   that order, then A1 and A2,
>   plus the `SR-2610-anvl-docs-001` items mapped to them. Phases 4–5 are listed for sequencing
>   only. They belong to the ANVL Bindings agent, so don't start them.
> - Work **only** on tickets named in Phases 1–3. If a ticket in `BR/`, `FR/` or `SR/` isn't listed
>   there, it isn't yours, even if it looks related or is tagged `rivet-readiness`. Don't implement
>   it, edit it or change its status.
> - Don't reorder, merge or skip tickets. If one turns out to be blocked, mis-scoped or wrong,
>   stop, record the finding in that ticket, and report it to the repo owner rather than working
>   around it.

**Written:** 2026-10-25 (planning review)
**For:** the ANVL coder agent picking up this ticket set, and the repo owner
**Scope:** every open ticket tagged `rivet-readiness`, plus the two distribution SRs. It covers
the gaps found while building Rivet, the in-house ticketing system (Postgres-backed, with FlyWire
on the wire), against AnvilTypes and AnvilSchema.

Out of scope: `FR-2610-anvl-asl-runtime-001` (its own agent), `FR-2603-sigma-collections-009/010`
(Sigma backlog), and the `libanvlbind` versioning question (escalated to Q-OR).

---

## Working rules

- **Branch off `main`, not `anvilscript-design`.** That branch belongs to the AnvilScript work.
  Use one branch per ticket (`fix/BR-2610-anvl-types-001`, `feat/FR-2610-anvl-schema-002`, ...).
  The planning session merges each branch back when the repo owner says it's ready.
- **Strict TDD.** Write the full failing test set first. RED must be genuine: the suite builds and
  runs, and every new case fails on an assertion. Then go test by test to GREEN. Run
  `make <suite> <action>` (`make schema test`, `make schema val`), the full unit regression and
  Valgrind before calling a ticket done.
- **Close the loop in the ticket.** Set `Status:` and add a short Resolution section naming the
  branch and commit. Update `notes/deferred-work.md` if anything was deferred.
- **Every ticket finishes with a changelog entry.** Add it to `docs/changelog.md` under
  `## [Unreleased]` (`### Fixed` for a BR, `### Added`/`### Changed` for an FR), naming the ticket
  ID and the user-visible effect. Commit it on the ticket's branch. A ticket isn't done without it.
- **New public API is appended, never renumbered.** Enums only grow at the end. Existing
  signatures and behavior stay identical, and every ticket has a regression case for that.
- **Docs ride along.** Each `SR-2610-anvl-docs-001` item lands with the ticket that makes it
  true (mapping below), not as a separate pass at the end.

## Decisions needed before the phase that uses them

| # | Ticket | Question | Recommendation |
|---|---|---|---|
| D1 | FR-2610-anvl-types-003 | Blob: its own primitive, folded into `String`, or both? | **Both.** `String` accepts blobs, and a separate `Blob` primitive adds `tag :=`. |
| D2 | FR-2610-anvl-types-005 | Does `integer := true` accept `2.0`? (`is_integral` follows the *spelling*, so a spelling-based rule rejects it.) | **Value-based:** `2.0` and `1e3` pass. Pin it in a test either way. |
| D3 | SR-2610-anvl-vendor-bump-001 | How does `anvil.wasm` get `sigma.system.alloc` under Emscripten? Does wasm carry types/schema? | Build the allocator from source with `emcc` on the Linode box. Carry types+schema, since FlyWire's browser side validates too. |
| D4 | FR-2610-anvl-bindings-001 (py/net slices) | Add `Schema`/`TypeRegistry` vtables? Today both headers are flat-only, and py/net bind through vtables. | **Yes.** Add them once, after Phase 3 when the native surface has settled, the same way as `Writer`/`Builder`. |

Answers:
**D1**: Both
**D2**: Value-based
**D3**: Build the allocator on the Linode box (follow recommendation)
**D4**: Add vtables after Phase 3 (new FR???)

NOTE: I will set up an agent specifically for bindings
---

## Phase 1: native correctness (do first, sequentially)

Both bugs sit under every later feature, and both are in the same read paths of `schema.c` and
`anvil_types.c`. They make a good warm-up for the codebase.

1. **BR-2610-anvl-types-001**: numeric reads through `anvil_value_get_numeric`, exact
   comparisons. Blocks FR-2610-anvl-types-005. *Docs: SR-docs item 3.*
2. **BR-2610-anvl-schema-001**: no fixed-buffer truncation anywhere, including the field-name
   and violation-storage sites added in review. Violation `field`/`message` must be allocated to
   length *before* FR-2610-anvl-schema-001 introduces paths.

## Phase 2: load-time diagnostics and the registry

These give every later "this is malformed" case a real home. Doing them early means later FRs
never need their interim "treat as not declared until diagnostics land" behavior.

3. **FR-2610-anvl-types-001**: type-registry diagnostics.
4. **FR-2610-anvl-types-002**: transitive `@[types]` includes. It's in the same loader as #3, so
   collisions get reported as soon as they become possible. *Docs: SR-docs item 5.*
5. **FR-2610-anvl-schema-004**: schema load diagnostics, passing the registry's through.

## Phase 3: validation semantics (order matters)

Today `anvil_schema_validate` is one flat loop over a flat rule list. The first two tickets here
reshape it. Everything after them is a per-rule check slotted into the reshaped code once.

6. **FR-2610-anvl-schema-002**: `validate_value` plus options. Extract "validate a set of
   statements against a set of rules, with options" out of the document entry point. This
   unblocks FlyWire's switch to AnvilSchema, so it has the most leverage in the set.
7. **FR-2610-anvl-schema-001**: nested `fields`/`items`, paths, `min_items`/`max_items`. Rules
   become a tree, and the per-field check from #6 recurses.
8. **FR-2610-anvl-schema-003**: `nullable`.
9. **FR-2610-anvl-types-005**: `integer` plus numeric bound getters. *(needs D2; after #1)*
10. **FR-2610-anvl-types-003**: Blob. *(needs D1)*
11. **FR-2610-anvl-types-004**: `format` (date, date-time, uuid).
12. **FR-2610-anvl-schema-005**: introspection, including extension keys. It goes last because
    it exposes an accessor for every constraint above. Doing it earlier means re-opening it
    after each one.

Phase 3 ends at #12. D4 (vtables) isn't a coder-agent task.

*Docs: SR-docs items 1, 2, 4 can land any time in Phase 1–3, but before the first binding release.*

## Addendum (coder agent, after Phase 3)

A1. **BR-2610-anvl-visibility-001**: `anvil_schema.h`/`anvil_type_registry.h` get the
    `visibility push(default)` wrapper, plus a `make check-exports` gate. Found in the coder's own
    FR-2610-anvl-types-001 findings. It blocks `FR-2610-anvl-public-api-003` and the
    py/net/java binding slices. It doesn't block `anvil.node`/`anvil.wasm`.
A2. **BR-2610-anvl-vtable-001**: append `get_path` to the `Document` vtable, plus a vtable
    completeness test (every flat function has a member). Also from FR-2610-anvl-types-001's
    findings.

Both must be merged before the Bindings agent starts Phase 4.

## Phase 4: bindings (separate repos; can overlap Phase 3) [ANVL Bindings agent, not the coder agent]

13. **SR-2610-anvl-vendor-bump-001**: bump `anvil.node` and `anvil.wasm` off `52f29bf`. *(needs
    D3 for wasm)* Node can start as soon as Phase 1 is merged, so the fixes travel with it.
14. **FR-2610-anvl-bindings-001, slice 1 (`anvil.node`)**: `TypeRegistry`/`Schema` over what's
    landed. Re-bump and add accessors as Phase 3 tickets merge.
14b. **FR-2610-anvl-bindings-001, slice 1b (`anvil.node`)**: `schema.validateField(docText,
    name, opts)` plus `schema.fields` introspection. *(added 2026-10-25)*
15. **FR-2610-anvl-bindings-002**: writer/builder in `anvil.node`, then in `anvil.wasm` (second
    slice, added 2026-10-25). *(after #13)*
16. **FR-2610-anvl-bindings-001, slices 2–3 (`anvil.py`, `anvil.net`)**. *(after D4; unblocked)*
16b. **FR-2610-anvl-bindings-001, slice 4 (`anvil.wasm`)**: the same `TypeRegistry`/`Schema`
    surface as `anvil.node`. *(added 2026-10-25)*

## Phase 5: release [ANVL Release agent; now the Release Engineer, `agents/assignments/release-v0.10.0-rc.md`]

17. **SR-2610-anvl-distribution-001** and **BR-2610-anvl-distribution-001**: a tagged core release
    and binding tarballs on anvldata.com, built from that tag. *(after **all** of Phase 4: owner
    decision 2026-10-25. Version and the fate of the `0.8.0-rc` files are release decisions R1/R2.)*

---

## Critical path for Rivet/FlyWire

The shortest route to "FlyWire validates through AnvilSchema from Node" is:

**#1 → #2 → #6 → #13 → #14 → #17**, then #7, #8 and #12 as Rivet's nested and nullable fields
need them. Everything else improves correctness and coverage, but none of it blocks that first
end-to-end path.

## Housekeeping

- **SR-2609-anvl-distribution-001** is effectively done: hosting is live and both tarballs are
  npm-shaped and verified. All that's left is the deliberate Linux-x64-only scope for
  `anvil-node`. Recommend closing it, with that deferral recorded as its resolution.
- **FR-2609-anvl-writer-001** lists the `anvil.node`/`anvil.wasm` writer wrappers as not started.
  FR-2610-anvl-bindings-002 now owns the Node half. Cross-reference it when that FR lands.
