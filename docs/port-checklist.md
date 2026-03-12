# Anvil C — Feature Port Checklist
**Target: Anvil v1.0.0-rel**  
**Reference: `~/repos/anvil.net/docs/CoreBoundary.md` (frozen spec authority)**  
**Target: Anvil.Net `~/repos/anvil.net/Anvil.Net/Anvil.Net.csproj` (frozen backend project)**
**Last updated: 2026-03-11**

---

## How to Use This Document

This is the guiding document for porting all remaining Anvil features from the frozen Anvil.Net reference into the Anvil C implementation.

- **Spec authority**: `anvil.net/docs/CoreBoundary.md` — every core-boundary decision is already made there; do not re-decide in C
- **Binding-specific items**: listed for awareness only — they are out of scope for `anvil/`
- **Status legend**: ✅ Complete · ⚠️ Partial · ❌ Not started · 🚫 Out of scope (binding-specific)

---

## Repo Strategy

### Branch model
- `main` — stable, tagged releases only
- `port/<feature>` — one branch per checklist item; merge to `main` when tests pass and memory is clean
- Never merge a port branch until: all related tests pass, Valgrind reports zero leaks, CHANGELOG entry is written

### For each port item
1. Open `port/<feature>` branch
2. Consult `anvil.net/docs/CoreBoundary.md` for the exact C equivalent of each .Net type
3. Consult `anvil.net/` source (frozen) for reference behavior — do not copy code, use it to understand contracts
4. Write tests first (existing test harness in `test/unit/`)
5. Implement until tests pass
6. Valgrind check
7. Update `CHANGELOG.md` and bump version accordingly
8. Update this checklist status
9. Merge to `main`, tag if milestone complete

### Version gates
| Gate | Condition |
|------|-----------|
| `v0.2.0-alpha` | Writer + Resolver (inheritance) complete |
| `v0.3.0-alpha` | VarRef + Import Graph complete |
| `v0.4.0-alpha` | ASL parser + evaluator core complete |
| `v0.4.3-alpha` | AMP scalar arrays/tuples complete |
| `v0.4.5-alpha` | Schema validation core complete |
| `v1.0.0-rel` | All Phase 1–3 items complete, Sigma.Memory integrated, public API frozen, benchmarks documented |

---

## Phase 1 — Close v0.1.x–v0.3.x Parity

**Reference**: CoreBoundary.md §1–§7

### 1.1 · Types implementation
| Item | Status | Notes |
|------|--------|-------|
| `types.c` implementation | ✅ | `anvl_value_type_name()` + `anvl_stmt_type_name()` implemented (v0.1.1-alpha) |

### 1.2 · Inheritance Resolver
**Spec ref**: CoreBoundary.md §5 — `AnvilResolver` → `anvl_resolver_t` + `anvil_resolve()`

| Item | Status | Notes |
|------|--------|-------|
| `resolver.c` implementation | ✅ | Full implementation (v0.2.0-alpha, pending writer for tag) |
| Kahn topological sort for inheritance | ✅ | Cycle detection eager at build-state time; missing base deferred |
| Per-node merge cache (`anvl_node_state_t.merge_cache`) | ✅ | Write-once per slot; lazy on first access; warm_all available |
| `id_to_idx` hash (identifier → stmt index) | ✅ | FNV-1a open-addressing hash; pre-sized to keep load < 0.5 |
| `computed` bitmap | ✅ | uint8_t array; one byte per statement slot |

### 1.3 · Vars / VarRef Resolution
**Spec ref**: CoreBoundary.md §6 — `AnvilVarsState` → `anvil_vars_state_t`

**Design divergences from .Net reference (intentional):**
- Interpolation hole syntax: `.Net` uses `{.ref}` (dot-prefix); C port uses `{ref}` (cleaner, dot was a .Net-ism)
- Blob interpolation: not implemented; blobs remain 100% verbatim (`$` sigil on blobs is a parse error)

| Item | Status | Notes |
|------|--------|-------|
| Vars block parsing (`vars { }`) | ❌ | |
| `$identifier` VarRef value type | ❌ | Parsed as new value type; resolved at materialise time against vars state |
| `$"…{ref}…"` InterpolatedString value type | ❌ | Segment list (literal + ref spans); resolved at materialise time |
| `anvl_vars_state_t` | ❌ | Holds vars block fields + resolved value table |
| Circular ref detection (eager, at vars-state construction) | ❌ | |
| Missing key deferred to first dereference | ❌ | |
| `ANVL_ERR_VARS_*` error codes (4101–4105) | ⚠️ | Codes defined in `errors.h`; not yet wired |

### 1.4 · Writer
**Spec ref**: CoreBoundary.md — writer emits AML/AMP/ASL text from a parsed context

| Item | Status | Notes |
|------|--------|-------|
| `serializer.c` implementation | ✅ | Ported as `anvil.serializer` package (`port/serializer`, merged `8499c1c`) |
| AML dialect write support | ✅ | All value types: scalar, object, array, tuple, blob |
| AMP dialect write support | ✅ | Scalar-only guard enforced at write time |
| Blob verbatim fidelity | ✅ | Raw source span emitted; idempotency verified (ST06c–ST06f) |
| Minify mode | ✅ | Single-line, comma-separated, no padding |
| Vtable interface (`Serializer.serialize` / `Serializer.to_stream`) | ✅ | Matches `Anvil`/`Context`/`Source` pattern |

### 1.5 · Import Graph Resolution
**Spec ref**: CoreBoundary.md §7 — graph walk in core; file I/O in binding

| Item | Status | Notes |
|------|--------|-------|
| `anvil_import_graph_t` DAG structure | ❌ | |
| `anvil_import_check_cycle()` — pure graph algorithm | ❌ | Belongs in core |
| File I/O callback interface (binding feeds bytes) | ❌ | Core receives buffer/callback; never opens a file descriptor |
| Import error codes (`ImportFileNotFound`, `ImportCyclicDependency`) | ❌ | |

---

## Phase 2 — Close v0.4.x Core Parity

### 2.1 · AMP Scalar Arrays and Tuples (v0.4.3 delta)
**Spec ref**: CoreBoundary.md §5 (Phase table) — `AmpArrayElementNotScalar = 4401`

| Item | Status | Notes |
|------|--------|-------|
| Parse scalar array `[v, …]` in AMP dialect | ✅ | Landed in v0.1.1-alpha |
| Parse scalar tuple `(v, …)` in AMP dialect | ✅ | Landed in v0.1.1-alpha |
| `AmpArrayElementNotScalar = 4401` error code | ✅ | Landed in v0.1.1-alpha |
| Tests for valid scalar arrays/tuples | ✅ | 2 happy-path tests in test_parser.c |
| Tests for invalid nested elements (rejection) | ✅ | 3 rejection tests in test_parser.c |

### 2.2 · ASL Parser and Evaluator Core (v0.4.0–v0.4.1)
**Spec ref**: CoreBoundary.md §8 — `AslParser` → `anvil_asl_parse()`, `AslEvaluator` → `anvil_asl_eval()`

| Item | Status | Notes |
|------|--------|-------|
| `anvil_asl_parse()` — parse function bodies into `asl_node_t` tree | ❌ | |
| `asl_node_t` / `asl_op_t` / `asl_value_t` | ❌ | Tagged union: int/float/bool/string/null/list/map |
| `asl_scope_t` — linked list of local variable frames | ❌ | |
| `asl_function_t` — body node + parameter list | ❌ | |
| `asl_exec_context_t` — depth counter + dispatch table pointer | ❌ | |
| `anvil_asl_eval()` — recursive-descent evaluator | ❌ | |
| `asl_module_t` function pointer table | ❌ | C equivalent of `IAnvilCallableModule` trampoline |
| `@[attr]` on ASL functions (v0.4.2) | 🚫 | Binding-specific (`AnvilScriptEngine` API) |

### 2.3 · Schema Validation Core (v0.4.5)
**Spec ref**: CoreBoundary.md §9 — stateless tree walk belongs in core

| Item | Status | Notes |
|------|--------|-------|
| `anvil_schema_resolve()` | ❌ | Resolves enum/flags virtual bases; detects unknown types |
| `anvil_schema_ruleset_t` | ❌ | Immutable after load; shared across validation calls |
| `anvil_type_rule_t` / `anvil_field_rule_t` | ❌ | Pure predicates; no heap allocation during eval |
| Schema error codes `460x` | ❌ | `SchemaAttrMissing`, `SchemaTypeUnresolved`, etc. |
| `.asch` file extension recognition | ❌ | |

---

## Phase 3 — Binding-Specific (Out of Scope for `anvil/`)

These items live in language bindings, not the C core. Listed for completeness so they are not accidentally re-implemented here.

| Item | Binding | Notes |
|------|---------|-------|
| `AnvilNode` managed tree view | Anvil.Net / future bindings | GC root, `IEnumerable<T>`, indexers |
| Multi-error validation (`Validate*()`, `AnvilValidationResult`) | Binding | Loop is binding-layer concern; parser stays fail-fast |
| POCO serialization / deserialization | Anvil.Net | Reflection-based; permanently binding-specific |
| POCO code-gen (`AnvilCodeGen`, `CSharpEmitter`) | Anvil.Net | v0.4.6; permanently binding-specific |
| `Anvil` static class (`Load()`, `Parse()`, `LoadDirectory()`) | Binding | File I/O, managed return types |
| Script Engine API (`AnvilScriptEngine`, `IAnvilModule`) | Binding | C uses function pointer table (`asl_module_t`) |

---

## Phase 4 — v1.0.0-rel Gate

All items below must be true before tagging `v1.0.0-rel`:

| Gate Condition | Status |
|----------------|--------|
| All Phase 1 items complete | ❌ |
| All Phase 2 core items complete | ❌ |
| Sigma.Memory fully integrated (replaces all internal alloc) | ❌ |
| Sigma.Collections used for AST node/token storage | ❌ |
| Sigma.Text used for writer output layer | ❌ |
| Public API frozen — `include/anvil.h` matches C ABI sketch in CoreBoundary.md §6 | ❌ |
| Zero memory leaks (Valgrind) across all test suites | ✅ (current scope) |
| Performance benchmarks documented in `docs/benchmarks.md` | ❌ |
| CHANGELOG complete and up to date | ⚠️ (partial — v0.1.0-alpha through v0.2.0-alpha pre-release) |
| All test samples updated for v1.0 feature set | ❌ |

---

## Current State Summary

| Module | File | Lines | Status |
|--------|------|-------|--------|
| Core entry point | `src/core/anvil.c` | 79 | ✅ |
| Parser (AML + AMP) | `src/core/parser.c` | 875 | ✅ |
| Context | `src/core/context.c` | 1058 | ✅ |
| Errors | `src/core/errors.c` | 233 | ✅ |
| Operators | `src/core/operators.c` | 59 | ✅ |
| Symbols | `src/core/symbols.c` | 70 | ✅ |
| Utilities | `src/utilities/utils.c` | 56 | ✅ |
| Types | `src/core/types.c` | 40 | ✅ type name functions |
| Resolver | `src/core/resolver.c` | 490 | ✅ Full implementation |
| Serializer | `src/serializer/serializer.c` | ~550 | ✅ Full implementation |
| VarRef | *(no file)* | — | ❌ Not started |
| Import graph | *(no file)* | — | ❌ Not started |
| ASL | *(no file)* | — | ❌ Not started |
| Schema | *(no file)* | — | ❌ Not started |
