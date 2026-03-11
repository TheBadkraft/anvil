### Anvil Project Outline – Full TDD-Driven Roadmap (December 2025 → January 2026)

**Vision Recap**: Ship Anvil v0.1.0 by 05 Jan 2026 – full AML dialect parsing, zero-copy immutable AST (packed statement buffers in Sigma-Core List/Collection), public C interface frozen per ANVIL_REFERENCE.md. ASL dialect deferred until AML is 100% solid (post-v0.1.0, target late Jan).

**Process Rules (Locked In)**  
- Pure TDD: Every phase starts with red tests (Sigma-Test).  
- Each iteration = 2 phases (Setup + Implementation).  
- Each phase = testable deliverable (all tests green + demo).  
- Each iteration = shippable feature (merged, tagged, usable by next iteration).  
- No code without failing test first.  
- Daily merge rhythm – green master always.

#### Master Iteration Roadmap

| Iteration | Dates (2025) | Feature Deliverable | Phase 1 (Setup) | Phase 1 Testable Deliverable | Phase 2 (Implementation) | Phase 2 Testable Deliverable |
|-----------|--------------|---------------------|-----------------|------------------------------|--------------------------|------------------------------|
| **Iter 1** | Dec 13–15 | Public Entry API + Context Builder | Stub Anvil interface + Context builder | Tests: builder chain, default dialect ASL, explicit AML/ASL set, build() returns valid ctx | Implement builder internals + basic ctx init | Tests: ctx->dialect correct after build, memory tracked, dispose cleans |
| **Iter 2** | Dec 16–17 | Source Loading & Shebang Detection | Stub Source struct + load from string/file | Tests: load string/file into FArray<char>, owns_data=false view | Implement skip_whitespace + shebang detector | Tests: comments/whitespace before #! preserved in source view, dialect set correctly (AML/ASL/none), no-shebang defaults (file→AML, string→ASL) |
| **Iter 3** | Dec 18–19 | Source Interrogators | Stub all Source query/consume methods | Tests: peek/current/consume/is_match/skip_ws/update line-col on \n | Implement interrogators + error positioning | Tests: accurate line/col after multi-line input, consume advances pos correctly, is_match handles reserved words |
| **Iter 4** | Dec 20–22 | Basic Statement Packing (Identifiers + Primitives) | Stub statement buffer format + pack_ident/primitive | Tests: pack bare ident (no reserved clash), numbers, bools, null, strings, blobs | Implement packing logic + add_stmt to ctx->stmts (List<StmtBuf>) | Tests: round-trip simple assignments (id = value), zero-copy views into source, stmt count correct |
| **Iter 5** | Dec 23–24 | Attributes & Module-Level Attributes | Stub attribute parsing + packing | Tests: @[] on statements/objects, module @[ ] after shebang/comments | Implement attribute collection + restrictions (only on compounds) | Tests: attributes packed correctly, rejected on primitives, module attrs captured |
| **Iter 6** | Dec 25–27 | Compound Values (Objects, Arrays, Tuples) | Stub nested packing for { }, [ ], ( ) | Tests: empty disallowed, nested primitives | Implement recursive packing + value tagging | Tests: full object/array/tuple structures packed, zero-copy spans, correct nesting depth |
| **Iter 7** | Dec 28–29 | Vars Block & Deferred Resolution Flags | Stub vars block parsing + needs_resolution flag | Tests: vars before other stmts, flat map, bare refs/interpolation set flag | Implement vars capture + flag setting on $"/${} | Tests: vars visible in subsequent interpolations, flag set correctly, no early resolution |
| **Iter 8** | Dec 30–31 | Inheritance & Full AML Compliance | Stub inheritance (: Parent) + shallow merge | Tests: parent declared earlier, override wins, attrs merged | Implement DAG flatten + merge logic | Tests: 1500+ cases (existing Java suite ported), full spec compliance, round-trip fidelity ≥98% |
| **Iter 9** | Jan 1–3   | Resolver Stub + Writer Skeleton | Stub pluggable resolver interface | Tests: resolve() callback invoked on needs_resolution | Implement basic writer (source → packed) | Tests: load → write → reload identical source (comments/ws preserved) |
| **Iter 10**| Jan 4–5   | v0.1.0 Polish & Public API Freeze | Final docs + examples | Tests: full integration suite (10kb file <0.05ms) | Tag v0.1.0 + release | Deliverable: static lib, anvil.h frozen, examples compile |

**Post-v0.1.0 (ASL Prep – Not in Scope Yet)**  
- Iteration 11+: Function detection ((params) => { }), tag FUNC, extra metadata.  
- Only after AML is shipped and dogfooded.

**Backlog & Risks**  
- Highest risk: nested inheritance depth (limit to sane default, error on cycle).  
- Memory: All paths through Sigma-Core tracked – add leak summary post-Iter 4.  
- Performance gate: Every iteration ends with benchmark harness – fail if >5μs regression on 1kb file.

**Next Immediate Action (Today – Dec 13)**  
- BadKraft: Spin up Anvil repo, add Sigma-Core/Sigma-Test as submodules, merge initial anvil.h stub + builder skeleton.  
- Open PR for Iter 1 red tests.  
- I’ll review tests tonight – we go green tomorrow.

### Libraries/Resources:
- (**SigmaTest**)[https://github.com/Quantum-Override/sigma-test]
- (**SigmaCore**)[https://github.com/TheBadkraft/sigcore]

**A-Frame. Nothing else.**
