# Anvil C — Feature Port Checklist
**Target: Anvil v1.0.0-rel**  
**Reference: `~/repos/anvil.net/docs/CoreBoundary.md` (frozen spec authority)**  
**Target: Anvil.Net `~/repos/anvil.net/Anvil.Net/Anvil.Net.csproj` (frozen backend project)**
**Last updated: 2026-03-21 (Phase 4 complete)**

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
| `v0.2.0-alpha` | Writer + Resolver (inheritance) complete | ✅ |
| `v0.3.0-alpha` | VarRef + Import Graph complete | VarRef ✅ — Import Graph ✅ |
| `v0.4.0-alpha` | ASL parser + evaluator core complete | ✅ |
| `v0.4.3-alpha` | AMP scalar arrays/tuples complete | ✅ (retroactive — landed in v0.1.1-alpha) |
| `v0.4.5-alpha` | Schema validation core complete | ✅ |
| `v0.5.0-alpha` | Sigma.Core migration complete; all 19 unit suites pass | ✅ |
| `v1.0.0-rel` | All Phase 1–3 items complete, Sigma.Memory integrated, public API soft-frozen, benchmarks documented |

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
| Vars block parsing (`vars { }`) | ✅ | Parsed in `parser.c`; stored as key/value entries in source |
| `$identifier` VarRef value type | ✅ | `ANVL_VAL_VAR_REF`; resolved at materialise time against vars state |
| `$"…{ref}…"` InterpolatedString value type | ✅ | `ANVL_VAL_INTERP_STR`; segment list (literal + ref spans) |
| `anvl_vars_state_t` | ✅ | Implemented in `src/vars/vars.c` |
| Circular ref detection (eager, at vars-state construction) | ✅ | Detected at `anvl_vars_state_build()` time |
| Missing key deferred to first dereference | ✅ | `ANVL_ERR_VARS_KEY_NOT_FOUND` on `anvl_vars_materialise()` |
| `ANVL_ERR_VARS_*` error codes (4101–4105) | ✅ | Defined and wired in `errors.h` / `vars.c` |

### 1.4 · Writer (Serializer)
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
| `anvil_import_graph_t` DAG structure | ✅ | `include/import.h`, `src/import/import.c` |
| `anvil_import_check_cycle()` — pure graph algorithm | ✅ | DFS load-stack in `import.c` |
| File I/O callback interface (binding feeds bytes) | ✅ | `anvl_import_loader_cb` — core never opens file descriptors |
| Import error codes (`ImportFileNotFound`, `ImportCyclicDependency`) | ✅ | `ANVL_ERR_IMPORT_*` 4201–4206 |

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

### 2.2 · ASL Parser and Evaluator Core (v0.4.0–v0.4.1) ✅
**Spec ref**: CoreBoundary.md §8 — `AslParser` → `Asl.parse()`, `AslEvaluator` → `Asl.exec()`

| Item | Status | Notes |
|------|--------|-------|
| `Asl.parse()` — parse function bodies into `asl_node_t` tree | ✅ | Pratt-style recursive-descent; returns heap-allocated AST |
| `asl_node_t` / `asl_op_t` / `asl_value_t` | ✅ | Tagged union: NULL/INT/FLOAT/STRING/BOOL/TUPLE/LIST/MAP |
| `asl_scope_t` — flat array (max 256 vars), deep-copy on store | ✅ | Flat preferred over linked list; same semantics |
| `asl_function_meta_t` — name/body/parameter source spans | ✅ | |
| `asl_exec_context_t` equivalent (eval ctx + depth guard) | ✅ | `asl_eval_ctx_t` internal; max depth 32 |
| `Asl.exec()` — tree-walk evaluator | ✅ | bind params → eval_block → return value |
| `asl_module_t` function pointer table | ✅ | `name` + `call` fn pointer + `userdata`; C equiv of `IAnvilCallableModule` |
| `asl_ext_lookup_cb` / `asl_fn_dispatch_cb` | ✅ | `$varref` resolution and function dispatch callbacks |
| `Asl.node_free()` / `Asl.value_free()` | ✅ | Recursive free; zero Valgrind leaks |
| 25 unit tests (A01–E05) | ✅ | Parser structural, literals, arithmetic, control flow, scope/dispatch |
| `@[attr]` on ASL functions (v0.4.2) | 🚫 | Binding-specific (`AnvilScriptEngine` API) |

### 2.3 · Schema Validation Core (v0.4.5) ✅
**Spec ref**: CoreBoundary.md §9 — stateless tree walk belongs in core

| Item | Status | Notes |
|------|--------|-------|
| `Schema.resolve()` | ✅ | Resolves enum/flags virtual bases; detects unknown types; error 4601–4603 |
| `anvl_schema_ruleset_t` | ✅ | Dynamic array with capacity doubling; shared across validation calls |
| `anvl_schema_type_t` / `anvl_field_rule_t` | ✅ | Object/Enum/Flags kind; field rules with expected `anvl_value_type` |
| Schema error codes `460x` | ✅ | 4601 `SchemaAttrMissing`, 4602 `SchemaTypeUnresolved`, 4603 `SchemaBaseUnknown`, 4604 `SchemaValidationRequired`, 4605 `SchemaValidationTypeMismatch`, 4606 `SchemaValidationUnknownField` |
| `.asch` file extension recognition | ✅ | Loaded via `Context.get_builder()->load_file()`; no special handling needed |
| 20 unit tests (SC01–SC20) | ✅ | Resolver enum/flags, resolver object, validator valid/invalid/extra/untyped, file I/O, multi-type |

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
| All Phase 1 items complete | ✅ |
| All Phase 2 core items complete | ✅ |
| Sigma.Memory fully integrated (replaces all internal alloc) | ✅ | sigma.core migration — `Allocator.create_bump`, `Allocator.free`, `arena->alloc`; module system in `src/core/module.c` |
| Sigma.Collections: judicious integration where it replaces manual grow-arrays | ✅ | Audit complete (2026-03-21): none of the sigma.collections types (farray, parray, list) expose a pluggable allocator hook. All `ci_ensure_*_capacity` arrays in `context_internal.h` allocate from `ctx->arena` (bump); abandoned grow-buffers are reclaimed together at `Context.dispose`. The bump-arena lifecycle is fundamentally incompatible with external-heap collections. Manual grow-arrays retained by design. Sigma.Collections remains applicable for future heap-lifetime data structures (e.g., module registry, symbol resolver) where arena coupling is not required. |
| Sigma.Text used for string building (vars, serializer) | ✅ | `StringBuilder` in `vars.c` + `serializer.c`; `String` in `context.c` |
| Public API soft-frozen — no new breaking changes; `include/anvil.h` ≈ stable | ✅ | API freeze as defined by CoreBoundary.md §6 applies to Anvil.Net, not this C port; soft freeze in effect here |
| Zero memory leaks (Valgrind) across all test suites | ✅ (current scope) |
| Performance benchmarks documented in `docs/benchmarks.md` | ✅ | Numbers in `README.md §Performance Metrics`; full suite via `./rtest benchmarks` |
| CHANGELOG complete and up to date | ✅ | v0.1.0-alpha through v0.4.5-alpha |
| All test samples updated for v1.0 feature set | ✅ | Added `vars.anvl` (vars block, VarRef, InterpolatedString) and `schema.asch` (enum/flags/object schema); tests registered in `test_parser.c` (73/73 pass) |

---

## Current State Summary

| Module | File | Lines | Status |
|--------|------|-------|--------|
| Core entry point | `src/core/anvil.c` | 79 | ✅ |
| Parser (AML + AMP) | `src/core/parser.c` | 1231 | ✅ |
| Context | `src/core/context.c` | 1058 | ✅ |
| Errors | `src/core/errors.c` | 233 | ✅ |
| Operators | `src/core/operators.c` | 59 | ✅ |
| Symbols | `src/core/symbols.c` | 70 | ✅ |
| Utilities | `src/utilities/utils.c` | 56 | ✅ |
| Types | `src/core/types.c` | 40 | ✅ type name functions |
| Resolver | `src/core/resolver.c` | 490 | ✅ Full implementation |
| Serializer | `src/serializer/serializer.c` | ~550 | ✅ Full implementation |
| Vars / VarRef | `src/vars/vars.c` | 287 | ✅ Full implementation (v0.2.2-alpha) |
| Import graph | `src/import/import.c` | 568 | ✅ Full implementation (v0.3.0-alpha) |
| ASL | `src/asl/asl.c` | 1560 | ✅ Full implementation (v0.4.0-alpha) |
| Schema | `src/schema/schema.c` | ~440 | ✅ Full implementation (v0.4.5-alpha) |

---

## Post-v1.0 Enhancements (Not in CoreBoundary.md)

Items below are new/additive features — not backports from Anvil.Net. They do not block v1.0.0-rel.

### E1 · Anonymous Top-Level Object Definitions

**FR**: `docs/feature-reqs/FR-2603-anvil-001-anonymous-block.md`

**Observed need**: `../q-or/registry.anvl` (may still be `.aml`) uses a bare block form:
```anvl
registry {
    ...
}
```
This is currently invalid — the parser requires `registry := { ... }`. However, the pattern
is natural and worth supporting as a first-class syntax for top-level named blocks.

**Design decisions** (2026-03-21, updated 2026-03-23):
- `key { ... }` at top level is **read-only** — the absence of `:=` signals a declaration, not
  an assignment. The block cannot be reassigned after parse; enforcement at parse time.
- Must **not** desugar to `key := { ... }` internally. Requires a distinct stmt type
  `ANVL_ANON_OBJECT` so `Statement.type()` is unambiguous and serialization
  preserves the `ident { }` form (round-trip correctness).
- `key` becomes a **module-global name**; duplicate declarations → `ANVL_ERR_ANON_BLOCK_REDECLARATION`.
- `${key.field}` remains a valid VarRef into an anonymous block.
- `vars { ... }` already works (`vars` is a reserved keyword); anonymous blocks extend that
  pattern to arbitrary identifiers.
- Parser lookahead is one token: `IDENT LBRACE` → anonymous block; `IDENT ASSIGN` → assignment.
- `Context.get_statement_by_name` and field traversal already work — no API changes needed.
- Import collision semantics TBD in the import subsystem.

| Item | Status |
|------|--------|
| Audit `../q-or/registry.anvl` usage patterns | ❌ |
| Add `ANVL_ANON_OBJECT` to `anvl_stmt_type` enum | ✅ |
| New error code `ANVL_ERR_ANON_BLOCK_REDECLARATION` | ✅ |
| Parser: recognise `IDENT LBRACE` at top level → `ANVL_ANON_OBJECT` | ✅ |
| Parser: reject `:=` assignment to a previously declared anonymous block name | ✅ |
| Serializer: emit `ident { }` form for `ANVL_ANON_OBJECT` | ✅ |
| Import collision semantics for anonymous global names | ❌ |
| Tests (AB01–AB08 in FR) | ✅ |

### E2 · Bitwise OR Flag Composition in Values

**Observed need**: flag field assignments of the form:
```anvl
flag := TOP | LEFT | RIGHT | BOTTOM | FLOAT
```
where the values are Enum/Flags schema members being OR-combined.

**Design decision** (2026-03-21): `|` is a scripting operator — **not allowed in strict AML**.
- `#!aml` shebang present: `|` in value position → `ANVL_ERR_PARSER_UNEXPECTED_TOKEN`. No exceptions.
- `#!asl` or **no shebang**: valid; parser routes `IDENT (PIPE IDENT)+` through the ASL
  evaluator. No new value type needed — ASL already owns binary expressions including `|`.
- "No shebang = permissive mode" is an established Anvil rule; expression values fit naturally.
- Schema interaction: a `ANVL_SCHEMA_FLAGS` field accepts an ASL-evaluated OR result.

| Item | Status |
|------|--------|
| Parser: `|` error in strict AML (shebang present + dialect == AML) | ❌ |
| Parser: route `IDENT (PIPE IDENT)+` through ASL evaluator in non-strict/no-shebang mode | ❌ |
| Schema validation: accept ASL OR-expression result against Flags type | ❌ |
| Serializer: emit `TOP \| LEFT \| RIGHT` form faithfully (round-trip) | ❌ |
| Tests | ❌ |

---

### E3 · Query Path Primitives (Node Navigation)

**Design decision** (2026-03-21): Node traversal stays **out of Anvil core**, mirroring how
`Anvil.Net.Api` is separate from `Anvil.Net`. Core = parse/serial/resolve. Navigation policy
lives in a consumer library (`anvil.api` C package or future binding).

**Philosophy**: primitives, not policy. No single pathing algorithm fits every application.
Expose raw iteration primitives; let consumers build their own traversal strategies without
being locked into one approach.

**Access model**: both **index** and **name** access where semantically applicable:
- Arrays and tuples: index only (`[0]`, `[1]`, …) — elements have no names
- Objects: index (`[0]`, `[1]`, …) for ordered walk, **or named** (`["field"]`) for direct
  lookup — object fields have both a position and a key
- Statements: index via `Context.get_statement(ctx, i)` — already exists

**Gap in current public API**: field and element iteration require raw struct access.
Proposed additions to `include/context.h` / `include/anvil.h`:
```c
// Object field iteration
usize  Context.field_count(ctx, stmt);              // count of fields in an object-valued stmt
field  Context.get_field(ctx, stmt, usize i);       // i-th field by index
field  Context.get_field_by_name(ctx, stmt, name);  // field by key name (NULL if absent)

// Array / tuple element iteration
usize      Context.element_count(ctx, stmt);        // count of elements
value_meta Context.get_element(ctx, stmt, usize i); // i-th element span (index only — no names)
```
With these six functions a consumer can implement any traversal: depth-first, jq-style path
expressions, jsonpath-style selectors, or a simple `node["field"]["subfield"]` accessor —
without Anvil having a policy opinion. An `anvil.api` package (post-v1.0) would be the
opinionated layer on top.

| Item | Status |
|------|--------|
| Audit current API: what field/element access already exists in `context.h` | ✅ |
| Add `Context.field_count` / `Context.get_field` (by index) | ✅ |
| Add `Context.get_field_by_name` (by key string) | ✅ |
| Add `Context.element_count` / `Context.get_element` (index-only) | ✅ |
| Document traversal primitives in `docs/reference.md` | ✅ |
| Tests (QP01–QP15 in `test_interrogators.c`) | ✅ |
| `anvil.api` package for higher-level path helpers (separate from core) | ❌ |

**Extension — Value-Level Collection Traversal** (v0.6.0-alpha, 2026-04-03):

E3 API above operates on **statement-level** collections only. Gap: cannot traverse nested
collections in field values (e.g., `sigma := { plugin_path := ["a", "b", "c"] }` — the array
is a field value, not a statement).

**Solution**: Five value-level primitives that mirror statement-level API but operate on `value`
structures directly:

```c
// Value-level collection element iteration
usize      Context.value_element_count(ctx, val);        // count of elements in field-value array/tuple
value_meta Context.get_value_element(ctx, val, usize i); // i-th element metadata

// Value-level object field iteration
usize Context.value_field_count(ctx, val);              // count of fields in field-value object
field Context.get_value_field(ctx, val, usize i);       // i-th field by index
field Context.get_value_field_by_name(ctx, val, name);  // field by key name (NULL if absent)
```

| Item | Status |
|------|--------|
| Add `Context.value_element_count` / `Context.get_value_element` | ✅ |
| Add `Context.value_field_count` / `Context.get_value_field` | ✅ |
| Add `Context.get_value_field_by_name` (linear scan, no lazy map) | ✅ |
| Document value-level API in `docs/reference.md` § 9.5 | ✅ |
| Tests (VT01–VT15 in `test_interrogators.c`) | ✅ |
| Validate use case for IN-2604-sigcli-anvl-001 (sigma.cli config traversal) | ⏳ |
