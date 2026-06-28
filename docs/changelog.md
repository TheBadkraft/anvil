# Changelog

All notable changes to the Anvil project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [v0.5.3-alpha] — pre-release (2026-06-28)

### Added

- **Value-level collection traversal API** (`include/context.h`, `src/core/context.c`) — five new functions enabling traversal of nested collections inside field values (FR-2604-anvl-001):
  - `Context.value_element_count(ctx, val)` — count elements in array/tuple values
  - `Context.get_value_element(ctx, val, index)` — access array/tuple elements by index
  - `Context.value_field_count(ctx, val)` — count fields in object values
  - `Context.get_value_field(ctx, val, index)` — access object fields by index
  - `Context.get_value_field_by_name(ctx, val, name, len)` — access object fields by name (linear scan)
- **Tests: VT01–VT15** (`test/unit/test_context.c`) — 15 new test cases covering value-level element/field access, NULL guards, out-of-range handling, and nested object traversal
- **Custom merge policy API** (`include/resolver.h`, `src/resolver/resolver.c`) — four new functions enabling consumer-controlled inheritance merge strategies (FR-2604-anvl-002):
  - `anvl_node_state_get_base_index(state, stmt_idx)` — resolve base statement index
  - `anvl_node_state_get_own_fields(state, stmt_idx)` — access unmerged fields from derived statement
  - `anvl_merge_policy_fn` — callback type for custom field merge logic
  - `anvl_node_state_get_merged_fields_custom(state, stmt_idx, policy, userdata)` — merge with policy callback
- **Tests: CM01–CM10** (`test/unit/test_resolver.c`) — 10 new test cases covering base index lookup, own fields access, array concatenation, object deep merge, field exclusion, NULL policy preservation, nested inheritance, error handling, and caching
- **`Anvil.get_version()`** (`include/anvil.h`, `src/core/anvil.c`) — runtime version query on the `Anvil` vtable; returns `major.minor.patch+build-tag` string with build number injected at compile time via `ANVL_BUILD`
- **Version macros** (`include/constants.h`) — consolidated to `ANVL_VERSION_MAJOR`, `ANVL_VERSION_MINOR`, `ANVL_VERSION_PATCH`, `ANVL_VERSION_TAG`, `ANVL_VERSION_STR`; removed duplicate definitions from `include/anvil.h` and `include/types.h`
- **`lib/Makefile`** — new; builds `lib/debug/` and `lib/release/` static and shared libraries; `ANVL_BUILD` injected via `git rev-list --count HEAD`
- **Tests: AM08–AM12** (`test/unit/test_amp_collections.c`) — empty array, empty tuple, empty object, single-element tuple rejection; two-element tuple acceptance
- **`test_version`** (`test/unit/test_version.c`) — standalone version output binary; appended to each test target run

### Fixed

- **Allocator API** (`src/resolver/resolver.c`) — updated all `Allocator.free()` calls to `Allocator.dispose()` to match current sigma.memory API (resolver module hadn't been updated since allocator API migration)
- **BR-2604-anvil-001:** Parser now rejects inheritance from anonymous objects. Anonymous object syntax (`Base { x := 10 }`) is incompatible with inheritance — statements that use inheritance (`:Base`) must use explicit assignment syntax (`Base := { x := 10 }` or `Derived:Base := { y := 20 }`). Added `ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS` error code (4052) and validation in `anvl_node_state_get_base_index()`. This enforces semantic consistency: anonymous objects are truly immutable and cannot be inherited from.
- **Parser: bare `#` rejection** — `parse_scalar_value` now rejects `#` not followed by an alphanumeric character with `ANVL_ERR_PARSER_UNEXPECTED_TOKEN`; valid forms `#RRGGBB` and `#identifier` unaffected
- **Parser: empty tuple `()` rejected** — `parse_tuple` now emits `ANVL_ERR_PARSER_EMPTY_TUPLE_NOT_ALLOWED` on empty tuple, consistent with existing empty array and empty object behavior
- **Parser: single-element tuple rejected** — `parse_tuple` now emits `ANVL_ERR_PARSER_TUPLE_TOO_FEW_ELEMENTS` when `element_count < 2`

### Changed

- **`src/core/anvil_impl.c` renamed to `src/core/anvil.c`**
- **`lib/` build scope** — `asl` and `serializer` removed from library build sources
- **`test/unit/Makefile`** — `ANVL_BUILD` injected; `test_release` target added; `test_version` wired as prerequisite and appended to all test targets

### Added (docs)

- **reference.md § 9.5** — Value-Level Collection Traversal chapter with API documentation and usage examples for traversing nested arrays/objects inside field values (FR-2604-anvl-001)
- **reference.md § 10** — Custom Merge Policies chapter with API documentation, rationale, usage examples (array concatenation, field exclusion, sigma.collections integration), and migration path (FR-2604-anvl-002)

---

## [v0.5.2-alpha] — pre-release (2026-03-21)

**Status:** E3 query path primitives implemented; all unit tests passing (19/19 suites, 0 failures)
**Milestone:** v1.0.0-rel gate clearance — E3 field/element navigation API complete

### Added

- **E3 query path primitives** (`include/context.h`, `src/core/context.c`) — five new
  slots on `anvl_context_i` enabling field and element navigation without raw struct access:
  - `Context.field_count(ctx, stmt)` — count of fields in an object-valued statement
  - `Context.get_field(ctx, stmt, i)` — i-th field by index
  - `Context.get_field_by_name(ctx, stmt, name, len)` — field by key name (NULL if absent);
    O(n) linear scan now, O(1) after `Map` backing lands (FR-2603-sigma-collections-002)
  - `Context.element_count(ctx, stmt)` — count of elements in an array or tuple statement
  - `Context.get_element(ctx, stmt, i)` — i-th element span (index-only — no names)
- **Tests: QP01–QP15** (`test/unit/test_interrogators.c`) — 15 new test cases covering all
  five primitives including NULL guards, out-of-range returns, and nested object traversal

### Fixed

- **Parser: `parse_object` nested field pool ordering** — outer object fields were added to
  `ctx->field_list` before inner (nested) object fields had been added, causing the outer
  object's `field_start` to index the wrong entries.  Fields are now collected in a
  temporary arena-allocated linked list during the parse loop and bulk-flushed to the pool
  after all descendant fields are committed, so `field_start` always points past all
  descendant entries.  This was a latent bug — not observable in practice until the E3
  `get_field_by_name` traversal exposed it.

### Added (docs)

- **Port-checklist E3 gate** — four of seven items now ✅:
  `field_count`, `get_field`, `get_field_by_name`, `element_count`, `get_element`, tests
  (remaining: `docs/reference.md` traversal docs, `anvil.api` package)

---

## [v0.5.1-alpha] — pre-release (2026-03-21)

**Status:** Phase 4 gates complete; all unit tests passing (19/19 suites, 73/73 parser tests, 0 failures)
**Milestone:** v1.0.0-rel gate clearance — Sigma.Collections audit, test samples updated

### Added

- **Test sample: `test/samples/vars.anvl`** — reference sample demonstrating the `vars {}` block,
  `$varref` fields in objects, and `$"interpolated {string}"` top-level statements
- **Test sample: `test/samples/schema.asch`** — reference sample demonstrating `@[schema]` enum,
  flags, and object type definitions (EntitySide, FilePerms, BlockConfig, ItemConfig)
- **Parser tests** (`test/unit/test_parser.c`) — two new sample-parse tests registered:
  `test_parse_vars_sample` and `test_parse_schema_asch`; parser test count raised to 73

### Added (docs)

- **Port-checklist Phase 4** — all gates now ✅:
  - Sigma.Collections audit complete: no allocator hook exists in any collection type;
    bump-arena lifecycle is incompatible; manual grow-arrays retained by design
  - Test samples: `vars.anvl` and `schema.asch` added; tests verified green

---

## [v0.5.0-alpha] — pre-release (2026-03-21)

**Status:** Sigma.Core migration complete; all unit tests passing (19/19 suites, 0 failures)
**Milestone:** Infrastructure upgrade — module system, new allocator API, sigma.core.text, test framework

### Added

- **Module system** (`src/core/module.c`)
  - Anvil registers as `SIGMA_ROLE_USER` via `sigma_module_init_all()` / `sigma_module_shutdown_all()` called from `src/cli/main.c`
  - Module bootstrap creates a 1 MiB bump slab for `StringBuilder` allocations; wires `StringBuilder.alloc_use()` on init and restores NULL on shutdown
  - Anvil dependency chain: `sigma.memory` → `sigma.core.module` → `sigma.core.text`

- **sigma.core.text** (`<sigma.core/strings.h>`)
  - All `<sigma.text/strings.h>` includes updated to `<sigma.core/strings.h>` — header is now the canonical location for `String`, `StringBuilder`, and helper functions

### Changed

- **Allocator API migration** (from sigma.memory 0.2.x R7 API to sigma.core/sigma.memory current)
  - `Allocator.dispose(x)` → `Allocator.free(x)` — ~60 call sites across `src/` and `test/`
  - `rscope` type → `bump_allocator` (`include/context.h`)
  - `Allocator.Resource.acquire(size)` → `Allocator.create_bump(size)`
  - `Allocator.Resource.release(arena)` → `Allocator.release((sc_ctrl_base_s *)arena)`
  - `Allocator.Resource.alloc(arena, size)` → `arena->alloc(arena, size)` (all call sites in `context_internal.h` and `parser.c`)
  - Removed `--wrap=malloc/free` linker flags from `TST_LDFLAGS`; new sigma.memory uses mmap/slab allocators and no longer exports `__wrap_*` symbols
  - `REQUIRES` / `TST_REQUIRES` in `config.sh` updated to reflect new package names

- **Test framework** (`test/unit/*.c`, 19 files)
  - Migration from direct `testset()` constructor registration to `Tests.enqueue(_register)` pattern
  - `_run_pending()` fires after `sigma_module_init_all()` in `main()`
  - `test/validation/test_prototype.c` moved from `test/unit/` to `test/validation/`

- **Context arena sizing** (`src/core/context.c`)
  - Slab multiplier increased from 4× to 16×, floor raised from 4 KiB to 64 KiB
  - Fixes OOM failures on larger sample files (modpack.anvl, arrays.anvl, tuples.anvl) caused by the doubling-without-free strategy in `ci_ensure_*_capacity` consuming arena space

### Fixed

- `writelnf` implicit declaration: removed all calls to the now-absent `writelnf()` helper across
  `test/unit/test_parser.c`, `test/unit/test_statements.c`, and `test/utilities/diagnostic.c`;
  replaced with `fprintf(stderr, "...\n", ...)` — `writelnf` was removed in the sigma.core rewrite
- Shutdown segfault: `_anvil_shutdown` no longer calls `Allocator.release(bump)` — sigma.memory drains its own controllers on module shutdown; releasing before sigma.test's cleanup caused a use-after-free in `stringbuilder_dispose`
- `Time` symbol missing in bench packages: added `sigma.core.utils.o` to `TST_REQUIRES` and bench package lists in `config.sh`

---

## [v0.4.5-alpha] — pre-release (2026-03-12)

**Status:** Schema resolver + validator complete; v0.4.5-alpha gate satisfied  
**Milestone:** Schema definition resolution and data validation against typed field rules

### Added

- **Schema** (`include/schema.h`, `src/schema/schema.c`)
  - `Schema.resolve(ctx)` — walks a `@[schema]`-attributed document; classifies each statement as `ANVL_SCHEMA_OBJECT`, `ANVL_SCHEMA_ENUM`, or `ANVL_SCHEMA_FLAGS`; returns `anvl_schema_ruleset_t *` or NULL with error
  - `Schema.validate(rules, data_ctx, file_path)` — walks a data document; for each typed statement whose base resolves to a known schema type, checks all declared fields are present with the correct `anvl_value_type`; collects all violations before returning (no fail-fast)
  - `Schema.get_type(rules, name)` — O(n) lookup by name in a ruleset
  - `Schema.ruleset_free(rules)` / `Schema.result_free(result)` — full deep-free helpers
  - `anvl_schema_type_t` — name, kind, fields array (Object) or values array (Enum/Flags)
  - `anvl_field_rule_t` — field name + `expected_type` (`anvl_value_type`)
  - `anvl_schema_ruleset_t` — dynamic array of `anvl_schema_type_t *` with capacity doubling
  - `anvl_schema_result_t` — `is_valid` flag + dynamic `anvl_schema_error_t` array
  - `anvl_schema_error_t` — validation error with code, message (owned copy), and optional file path
  - Error codes: `ANVL_ERR_SCHEMA_ATTR_MISSING` (4601), `ANVL_ERR_SCHEMA_TYPE_UNRESOLVED` (4602), `ANVL_ERR_SCHEMA_BASE_UNKNOWN` (4603), `ANVL_ERR_SCHEMA_VALIDATION_REQUIRED` (4604), `ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH` (4605), `ANVL_ERR_SCHEMA_VALIDATION_UNKNOWN_FIELD` (4606)
  - `anvil.schema` package in `config.sh`; `with_schema_objects` wired in `rtest`

- **20 Schema unit tests** (`test/unit/test_schema.c`)
  - **SC01–SC05** Resolver — enum/flags: non-null ruleset, no-attr error, enum kind, flags kind, enum values
  - **SC06–SC08** Resolver — object types: kind is OBJECT, field names correct, field expected types correct
  - **SC09–SC15** Validator: valid data passes, missing required field error, type mismatch error, extra fields permitted, untyped statements pass through, unknown base returns null, multiple violations all collected
  - **SC16–SC20** File I/O and multi-type: load .asch file, nonexistent file handled, file-based validation, two typed stmts both missing fields, two schema types both validated

### Fixed

- `find_own_field` in `schema.c`: parser allocates nested object fields depth-first (inner fields precede their parent in the flat `field_list`); fixed scan to cover the full statement field range rather than relying on `field_start + field_count` which excluded sibling fields after a nested object
- AML C parser requires commas between object fields (unlike the C# reference); all schema test source strings updated accordingly

---

## [v0.4.3-alpha] — retroactive gate (2026-03-11)

**Status:** Released (retroactive tag — work landed in v0.1.1-alpha)  
**Milestone:** AMP scalar arrays/tuples gate satisfied

This gate was satisfied early. The AMP scalar enforcement work was implemented as low-hanging fruit during the initial port and shipped in v0.1.1-alpha, predating the formal versioning scheme. No new code; this entry closes the gate retroactively.

### Gate Condition Satisfied

- AMP scalar array `[v, …]` parsing ✅ — `parse_array()` rejects non-scalar elements in AMP dialect
- AMP scalar tuple `(v, …)` parsing ✅ — `parse_tuple()` rejects non-scalar elements in AMP dialect
- `ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR = 4401` ✅ — defined in `include/errors.h`
- 5 parser tests ✅ — 2 happy-path (valid scalar array/tuple), 3 rejection (nested object/array/tuple in AMP)

*See v0.1.1-alpha entry for full implementation details.*

---

## [v0.4.0-alpha] — pre-release (2026-03-12)

**Status:** ASL parser + evaluator complete; v0.4.0-alpha gate satisfied  
**Milestone:** Full ASL (Anvil Script Language) parse + tree-walk evaluation with flat scope, control flow, and external dispatch

### Added

- **ASL** (`include/asl.h`, `src/asl/asl.c`)
  - `Asl.parse(meta, src)` — Pratt-style recursive-descent parser; returns heap-allocated AST (`asl_node_t *`)
  - `Asl.exec(meta, src, args, argc, out, modules, ...)` — parse → bind parameters → tree-walk eval → return value
  - `Asl.node_free(node)` / `Asl.value_free(val)` — recursive free helpers
  - `asl_value_t` — tagged union: `ASL_NULL`, `ASL_INT`, `ASL_FLOAT`, `ASL_STRING`, `ASL_BOOL`, `ASL_TUPLE`, `ASL_LIST`, `ASL_MAP`
  - `asl_node_t` — AST node with child array; node kinds cover literals, binary/unary ops, identifiers, assign, block, if, for, break, continue, call, return
  - `asl_function_meta_t` — function name/body/parameter spans (source-level offsets)
  - `asl_module_t` — external module dispatch callback (`name` + `call` function pointer + `userdata`)
  - `asl_ext_lookup_cb` / `asl_fn_dispatch_cb` — callbacks for `$varref` resolution and function dispatch
  - Flat scope (`asl_scope_impl_t`): max 256 vars, deep-copy on store/return
  - Truthiness: `null`→false, zero/`""`/empty containers→false, else→true
  - `i++` / `i--` desugared to `assign(i, binary(i, 1))`
  - `for` loop children: `[init, cond, step, body]`; `break` / `continue` as scope-exit flags
  - Error codes: `ANVL_ERR_ASL_PARSE_ERROR` (5101), `ANVL_ERR_ASL_RUNTIME_ERROR` (5102), `ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED` (5103), `ANVL_ERR_ASL_BREAK_OUTSIDE_LOOP` (5104), `ANVL_ERR_ASL_CONTINUE_OUTSIDE_LOOP` (5105)
  - `anvil.asl` package in `config.sh`; `with_asl_objects` wired in `rtest`

- **25 ASL unit tests** (`test/unit/test_asl.c`)
  - **A01–A05** Parser structural: non-null node, root is BLOCK, literal child, float child, string child
  - **B01–B05** Literals: exec int 42, float 3.14, string `"hello"`, bool `true`, `null`
  - **C01–C05** Arithmetic: add, multiply, subtract, modulo, string concatenation
  - **D01–D05** Control flow: if-taken, else-taken, for-loop accumulator, break, continue
  - **E01–E05** Scope / dispatch: local assignment, `$varref` external lookup, comparison, logical `&&`, parameter binding

---

## [v0.3.0-alpha] — pre-release (2026-03-12)

**Status:** Import Graph complete; v0.3.0-alpha gate satisfied  
**Milestone:** Full import DAG resolution with DFS cycle detection, diamond dedup, and loader callback interface

### Added

- **Import Graph** (`include/import.h`, `src/import/import.c`)
  - `import "path" [as alias]` declaration parsing in `parser.c`; stored as source spans in `ctx->import_list`
  - `struct anvl_import_decl` — `{path_pos, path_len, alias_pos, alias_len}` (types.h)
  - `import_list` field in `anvl_context_t` — flat array of declarations (context.h)
  - `ImportGraph.build(ctx, owner_path, loader, userdata)` — DFS graph walk
  - `ImportGraph.dispose(graph)` — frees all owned memory
  - `anvl_import_loader_cb` — binding-provided callback; core never opens file descriptors
  - Canonical-first dedup (diamond dedup): same path loaded twice → single entry, extra alias registered
  - DFS load-stack cycle detection → `ANVL_ERR_IMPORT_CYCLIC`
  - Duplicate alias detection → `ANVL_ERR_IMPORT_DUPLICATE_ALIAS`
  - File-not-found propagation → `ANVL_ERR_IMPORT_FILE_NOT_FOUND`
  - Placement guards: import after statements → `ANVL_ERR_IMPORT_NOT_FIRST`; import in AMP dialect → `ANVL_ERR_IMPORT_AMP_FORBIDDEN`
  - Stem alias derivation: `my-blocks.aml` → `my_blocks` (StringBuilder-based, Allocator-backed)
  - `anvil.import` package in `config.sh`; `with_import_objects` wired in `rtest`

- **10 import unit tests** (`test/unit/test_import.c`)
  - Parser: no imports, single import, explicit alias, import-after-statement error
  - Graph builder: empty graph, stem alias, explicit alias, duplicate alias error, cyclic error, diamond dedup

---

## [v0.2.2-alpha] — pre-release (2026-03-12)

**Status:** Vars / VarRef complete; Import Graph remaining for v0.3.0-alpha gate  
**Milestone:** Full VarRef + interpolated string resolution; build system modernized

### Added

- **Vars / VarRef** (`include/vars.h`, `src/vars/vars.c`)
  - `Vars.build(ctx, src)` — resolves VarRef chains, detects circular references at build time
  - `Vars.resolve(state, key, key_len, ...)` — look up a key in the resolved vars state
  - `Vars.materialise_interp(state, ctx, vm)` — expands `$"…{ref}…"` to a heap-allocated string
  - `Vars.dispose(state)` — frees all vars-state owned memory
  - Vtable interface pattern consistent with `Anvil` / `Context` / `Source` / `Serializer`
  - `ANVL_VALUE_VAR_REF` and `ANVL_VALUE_INTERP_STRING` value types parsed in `parser.c`
  - Vars block (`vars { }`) fully parsed; key/value entries stored as source spans
  - Interpolated string segment list (literal + ref spans); resolved at materialise time
  - Circular ref detection eager at `Vars.build()` time
  - Missing key returns `ANVL_ERR_VARS_KEY_NOT_FOUND` on `Vars.materialise_interp()`
  - String building in `materialise_interp` uses **sigma.text** `StringBuilder` (Allocator-backed)

- **20 vars unit tests** (`test/unit/test_vars.c`)
  - Vars build: no vars block, empty block, single/multi key
  - VarRef: direct ref, ref-to-ref chain, unknown key
  - Circular ref detection: direct cycle (A→A), indirect cycle (A→B→A), non-cycle chain
  - InterpolatedString: literal-only, single ref, multi-ref, prefix/suffix, multi-segment
  - Error paths: `materialise_interp` on non-interp-string value, null state

### Changed

- **Build system**: Makefile removed; project now uses `cbuild` / `rtest` exclusively
- **Include path**: all test files migrated from `<sigtest/sigtest.h>` → `<sigma.test/sigtest.h>`
  (`/usr/local/include/sigma.test/` is the canonical location)
- **`.vscode/c_cpp_properties.json`**: updated to sigma.* include paths; removed stale makefile-tools
  provider; `cStandard` updated to `c23`
- **`vars_materialise_interp`**: replaced hand-rolled `sb_t` buffer with `StringBuilder` from
  **sigma.text** — all string-building heap allocations now flow through `Allocator`

### Test Results

- 192 tests across 13 test files; 192/192 passing
- Zero memory leaks

---

## [v0.2.1-alpha] — pre-release (2026-03-11)

**Status:** Serializer complete; v0.2.0-alpha gate fully satisfied  
**Milestone:** AML/AMP/ASL serializer (writer) with vtable interface

### Added

- **Serializer / Writer** (`include/serializer.h`, `src/serializer/serializer.c`)
  - `Serializer.serialize(ctx, src, opts)` — emits AML/AMP/ASL text to a heap string
  - `Serializer.to_stream(ctx, src, opts, stream)` — writes directly to a `FILE *`
  - `opts.minify` — single-line, comma-separated, no padding
  - AML dialect: full support for scalar, object, array, tuple, blob value types
  - AMP dialect: scalar-only guard enforced at write time
  - Blob verbatim fidelity: raw source span emitted; round-trip idempotency verified
  - Vtable interface (`Serializer` global const) matching `Anvil` / `Context` / `Source` pattern
  - Uses **sigma.text** `StringBuilder` for all output buffering

- **Serializer tests** (`test/unit/test_serializer.c`)
  - Round-trip tests for AML and AMP dialects
  - Blob fidelity tests (ST06c–ST06f)
  - Minify mode output tests

### Test Results

- 172 tests across 12 test files; 172/172 passing
- Zero memory leaks

---

## [v0.2.0-alpha] — pre-release (2026-03-11)

**Status:** Complete — Resolver + Writer (serializer) gate satisfied; see v0.2.1-alpha  
**Milestone:** Single-inheritance resolution, cycle detection, lazy merge cache

### Added

- **Inheritance Resolver** (`include/resolver.h`, `src/core/resolver.c`)
  - `anvl_resolver_build_state(ctx, src)` — validates the inheritance DAG using Kahn's
    topological sort; returns NULL (no error) when no statements have a base (fast-path)
  - `anvl_node_state_get_merged_fields(state, idx)` — lazily computes merged field list;
    derived fields override base fields (last-write-wins); result cached per slot
  - `anvl_node_state_warm_all(state)` — pre-warms all merge slots eagerly
  - `anvl_node_state_dispose(state)` — frees all resolver-owned heap
  - FNV-1a open-addressing hash (`anvl_id_map_t`) for identifier→index mapping
  - Forward-reference support (derived declared before base — Kahn handles ordering)
  - `ANVL_ERR_RESOLVER_CYCLE_DETECTED` (4050) — set on cycle in build_state
  - `ANVL_ERR_RESOLVER_MISSING_BASE` (4051) — deferred; set on first access in get_merged_fields

- **Type name utilities** (`src/core/types.c`)
  - `anvl_value_type_name(type)` — returns display string for `anvl_value_type` enum values
  - `anvl_stmt_type_name(type)` — returns display string for `anvl_stmt_type` enum values

- **10 resolver unit tests** (`test/unit/test_resolver.c`)
  - No-inheritance fast-path
  - Single-level: inherits unoverridden fields; derived override; base unchanged
  - Three-level chain: full resolution and midpoint correctness
  - Forward reference (child declared before parent)
  - Cycle detection (A:B + B:A)
  - `warm_all` idempotency
  - Missing base deferred error

### Fixed

- **`parser.c`**: `value_meta->data.object.field_start` was never set — only `field_count`
  was transferred from the temporary `value` object to `value_meta` during statement
  finalization. This meant all object statements appeared to own fields starting at index 0,
  breaking resolver merge logic, field interrogation, and any future writer output.

### Architecture

- Resolver is a pure post-parse pass: takes a fully-built `context` + `source`,
  returns a disposable `anvl_node_state_t`; never modifies the context
- Merge cache holds `field*` pointer arrays pointing back into `ctx->field_list`; no deep copies
- Topological sort validates the DAG at build-state time; missing bases are deferred until
  first field access (consistent with Anvil.Net reference behavior)

### Test Results

- 153 tests across 11 test files; 153/153 passing
- Zero memory leaks

---

## [v0.1.1-alpha] — 2026-03-11

**Status:** Released  
**Milestone:** AMP scalar arrays/tuples enforcement + all Phase 1–3 error codes

### Added

- **AMP scalar enforcement in arrays and tuples** (`src/core/parser.c`)
  - `parse_array()` and `parse_tuple()` now reject non-scalar elements in AMP dialect
  - `ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR = 4401` added to `include/errors.h`
  - AMP arrays/tuples were previously blocked entirely; now allowed with scalar-only enforcement

- **All remaining Phase 1–3 error codes** (`include/errors.h`, `src/core/errors.c`)
  - Resolver: `ANVL_ERR_RESOLVER_CYCLE_DETECTED = 4050`, `ANVL_ERR_RESOLVER_MISSING_BASE = 4051`
  - Vars: `ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED = 4101` … `ANVL_ERR_VARS_KEY_NOT_FOUND = 4105`
  - Import: `ANVL_ERR_IMPORT_NOT_FIRST = 4201` … `ANVL_ERR_IMPORT_CYCLIC = 4206`
  - Using: `ANVL_ERR_USING_MODULE_NOT_FOUND = 4305` … `ANVL_ERR_USING_AFTER_STATEMENTS = 4307`
  - Full message and name table entries added for all new codes

- **5 new AMP parser tests** (`test/unit/test_parser.c`)
  - Valid AMP scalar array accepted
  - Valid AMP scalar tuple accepted
  - AMP array with nested object rejected (4401)
  - AMP array with nested array rejected (4401)
  - AMP tuple with nested object rejected (4401)

### Test Results

- 143 tests across 10 test files; 143/143 passing (up from 138 in v0.1.0-alpha)

---

## [v0.1.0-alpha] - 2025-12-20

**Status:** Production-Ready Parser Release  
**Milestone:** First public release with zero memory leaks and comprehensive AMP support

### Added

- Zero-copy, direct-construction parser for AML/ASL source code
- AMP (Anvil Messaging Protocol) dialect support with full validation
- Comprehensive memory management with 18 tracked allocations
- 17 AMP messaging protocol tests with 100% passing rate
- Parser documentation suite with 4 core documents:
  - PARSER_ARCHITECTURE.md: Complete architecture and design patterns
  - PARSER_QUICK_REF.md: Developer quick reference guide
  - TEST_COVERAGE_MAP.md: Test-to-code-path mapping
  - PARSER_DOCUMENTATION_INDEX.md: Navigation and index
- Version identifier: ANVIL_VERSION "v0.1.0-alpha" in include/anvil.h
- Memory leak fixes with comprehensive error path cleanup
- Error handling system with 24+ error codes
- Source interrogation interface for position tracking and metadata
- Context-owned statement and value metadata system

### Fixed

- **Leak #1:** Missing disposal at assignment operator check (parse_statement line 178-180)
  - Fixed base_meta and attr_meta not being disposed when := operator not found
  
- **Leak #2:** Missing disposal in attribute validation (parse_statement line 196)
  - Fixed value object not being disposed when attributes applied to scalar type
  
- **Leak #3:** Missing disposal in value_meta allocation (parse_statement line 211)
  - Fixed allocated value not being disposed on value_meta OOM condition
  
- **Leak #4:** Missing disposal in parse_source statement allocation (parse_source line 88-95)
  - Fixed dispose_statement() being a no-op stub
  - Now properly frees statements that fail to parse before being added to context
  - Resolved 6 orphaned statement allocations (1 from successful parses + 5 from AMP rejections)

### Verified

- All 17 AMP test cases passing
- Zero memory leaks: 138/138 allocations freed in full test suite
- Memory tracking via sigtest fat.o integration
- AMP dialect validation for rejection of:
  - Complex types (objects, arrays, tuples) in scalar-only context
  - Inheritance syntax (: operator) in AMP mode
  - Attributes (@[...] syntax) in AMP mode
- Full AML support with objects, inheritance, and attributes
- AML/AMP dialect auto-detection
- Envelope file parsing (response and event formats)
- Meta-buffer VALUE span tracking

### Architecture

- Parser is stateless after anvl_parse() returns
- All allocations owned by context until Context.dispose()
- Temporary allocations in error paths properly cleaned up
- Direct value construction without builder pattern overhead
- Metadata-first approach with self-contained buffers

### Documentation

- 40+ code examples across documentation
- 12 functions fully documented with signatures and examples
- 10 design patterns explained with implementation details
- Complete memory allocation lifecycle tracking
- Test coverage mapping for 51+ test scenarios
- Future improvements roadmap

### Known Limitations

- No semantic validation (type checking, inheritance resolution)
- No error recovery (stops on first error)
- No string materialization (values are position/length spans)
- No streaming parser (full source must be buffered)
- No bytecode compilation or execution

### Test Results

- test_messaging.c: 17/17 passing
  - 5 valid AMP scalar tests passing
  - 5 AMP rejection tests passing (object, array, tuple, attribute, inheritance)
  - 2 AML dialect tests passing (allows objects and inheritance)
  - 5 envelope and metadata tests passing
- Memory: 138 mallocs, 138 frees (zero leaks)
- Build: All warnings treated as errors (strict compilation)

### Breaking Changes

None - this is the initial release.

### Dependencies

- C23 (ISO/IEC 9899:2023)
- **SigmaTest** framework 
- **SigmaCore** library

### Files Changed

Core Implementation:
- src/core/parser.c: Fixed dispose_statement() to actually free memory
- include/anvil.h: Added version identifier

Documentation:
- docs/PARSER_ARCHITECTURE.md: Updated with 4 leak fixes and production status
- docs/PARSER_QUICK_REF.md: Updated version and test counts
- docs/TEST_COVERAGE_MAP.md: Updated version identifier
- docs/PARSER_DOCUMENTATION_INDEX.md: Updated production-ready status

### Contributors

- BadKraft (Author, Parser Implementation, Memory Audit, Documentation)

### Acknowledgments

- sigtest framework for memory tracking and testing infrastructure
- sigcore library for system utilities

---

## Future Releases

### Planned for v0.2.0

- Streaming parser support for large files
- Error recovery with checkpoint system
- String materialization cache
- LLVM AST materialization option
- Dialect-specific validators
- Performance profiling and optimization

### Future Considerations

- Incremental parsing support
- IDE integration features
- Advanced error messages with suggestions
- Custom dialect support
- Bytecode compilation target

---

**For detailed technical information, see:**
- Architecture: docs/PARSER_ARCHITECTURE.md
- Quick reference: docs/PARSER_QUICK_REF.md
- Test mapping: docs/TEST_COVERAGE_MAP.md
- Language spec: docs/ANVIL_SPEC_DRAFT.md
