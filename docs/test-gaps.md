# Anvil — Test Gap Analysis

**Audited:** 2026-03-12  
**Scope:** `test/unit/` — all 14 test files  
**Method:** Compare implemented functions (`static void test_*() {`) against registered testcases (`testcase(...)`) in each file

---

## Coverage Summary

| Test File | Implemented | Registered | Stubs (skip) | Real-but-unregistered | Status |
|-----------|:-----------:|:----------:|:------------:|:---------------------:|--------|
| `test_anvil.c` | 5 | 5 | 0 | 0 | ✅ Clean |
| `test_context.c` | 12 | 12 | 0 | 0 | ✅ Clean |
| `test_diagnostic.c` | 1 | 1 | 0 | 0 | ✅ Clean |
| `test_import.c` | 10 | 10 | 0 | 0 | ✅ Clean |
| `test_interrogators.c` | 11 | 11 | 0 | 0 | ✅ Clean |
| `test_messaging.c` | 17 | 17 | 0 | 0 | ✅ Clean |
| `test_meta_buffers.c` | 20 | 20 | 0 | 0 | ✅ Clean |
| `test_prototype.c` | 3 | 3 | 0 | 0 | ✅ Clean |
| `test_resolver.c` | 10 | 10 | 0 | 0 | ✅ Clean |
| `test_serializer.c` | 19 | 19 | 0 | 0 | ✅ Clean |
| `test_source.c` | 22 | 22 | 0 | 0 | ✅ Clean |
| `test_vars.c` | 20 | 20 | 0 | 0 | ✅ Clean |
| `test_parser.c` | 68 | 56 | 22 unregistered | 12 unregistered | ⚠️ Gaps |
| `test_statements.c` | 0 | 0 | — | — | ❌ Empty |

---

## Gap 1 — `test_statements.c` is empty ❌

**Priority: P0**

The entire statement-buffer accessor API has zero regression tests. The file contains only a comment explaining why:

> *"Tests were written for the old statement structure. The new architecture uses `meta[9]` array, `value_meta` pointer, `base_meta` pointer, `attr_meta` pointer. Tests should be rewritten to use the new meta-buffer accessors and assertions."*

This is the highest-priority gap. When Sigma.Build walks a `build.anvl` document, the statement traversal APIs — `Context.statement_count`, field/value accessor chain, type interrogators on statement nodes — are exactly what it calls on every parse. Without regression coverage, any regression in statement handling is invisible until Sigma.Build breaks at integration time.

**Minimum coverage needed for test_statements.c:**
- Statement count after parse
- Statement access by index (`Context.get_statement(ctx, i)`)
- Value type interrogation on scalar statements
- Field enumeration on object statements
- Value type interrogation on array/tuple statements
- Attribute presence and count on statements
- Inheritance (`base_meta`) presence on derived statements
- Boundary: index out of range behavior

---

## Gap 2 — `test_parser.c`: 12 real tests implemented but not registered ⚠️

**Priority: P1**

These functions exist, compile, and have meaningful bodies — but are never called by the test runner. Registering them is a one-`testcase()` fix each.

| Function | Nature | Sample file |
|----------|--------|-------------|
| `test_parse_arrays_sample` | Parses real content from `test/samples/` | `arrays.anvl` |
| `test_parse_assignments_sample` | Parses real content from `test/samples/` | `assignments.anvl` |
| `test_parse_attributes_sample` | Parses real content from `test/samples/` | `attributes.anvl` |
| `test_parse_objects_sample` | Parses real content from `test/samples/` | `objects.anvl` |
| `test_parse_tuples_sample` | Parses real content from `test/samples/` | `tuples.anvl` |
| `test_parse_inherits_sample` | Parses real content from `test/samples/` | `inherits.anvl` |
| `test_parse_modpack_sample` | Parses real content from `test/samples/` | `modpack.anvl` |
| `test_parse_deep_nesting` | Generates 10+ level deep nested structure | *(generated)* |
| `test_parse_inheritance_placeholder` | Inheritance syntax smoke test | *(inline source)* |
| `test_parse_generic_aurora` | Parses `generic.aurora` in ASL dialect | `generic.aurora` |
| `test_parse_unexpected_token` | Error path for unexpected token error | *(inline source)* |
| `test_parse_real_world_data` | Fetches and converts real external data | *(generated)* |

The 7 sample-file tests are particularly important: they validate the full range of `test/samples/*.anvl` document shapes — the same shapes that real `build.anvl` files will use. These exist as samples precisely because they represent realistic inputs, yet they're not being exercised during test runs.

### Sub-issue: vacuous assertion in `test_parse_generic_aurora`

The function's only assertion is:

```c
Assert.isTrue(true, "generic.aurora should parse without error");
```

This always passes regardless of what actually happens. It needs to be replaced with a structural check — at minimum verifying that `Context.parse(ctx)` returns true and `Context.statement_count(ctx) > 0`.

---

## Gap 3 — `test_parser.c`: 22 Assert.skip stubs not registered ⚠️

**Priority: P3**

These stubs are explicitly marked with `Assert.skip("Parser error check not implemented")`. They exist as placeholders for error-path coverage that was never completed. The error-test infrastructure is functional — the 14 currently-registered error tests all use `parse_source_with_err()` successfully; these stubs just need to be promoted and registered.

### Blocked on `value_meta`

| Function | Blocking condition |
|----------|--------------------|
| `test_parse_numbers_stub` | Number literal parsing validation deferred until `value_meta` is fully populated for numeric types |

### Error-path stubs (error check infrastructure works, tests just not written)

**Object error paths:**
- `test_parse_trailing_comma_in_object`
- `test_parse_duplicate_field_in_object`
- `test_parse_expected_object_close`
- `test_parse_expected_object_field`
- `test_parse_expected_object_value`
- `test_parse_invalid_key_in_object`

**Tuple error paths:**
- `test_parse_tuple_too_short`
- `test_parse_empty_tuple_element`
- `test_parse_expected_backtick`

**Attribute error paths:**
- `test_parse_duplicate_attribute_key`
- `test_parse_identifier_is_keyword`
- `test_parse_attribute_is_keyword`
- `test_parse_invalid_attribute`
- `test_parse_invalid_attribute_block`

**Statement-level error paths:**
- `test_parse_assignment_not_allowed_here`
- `test_parse_rocket_op_not_valid`
- `test_parse_unexpected_token`
- `test_parse_unexpected_char`

**Lexer error paths:**
- `test_parse_invalid_exponent`
- `test_parse_invalid_hex_literal`
- `test_parse_invalid_number`
- `test_parse_unterminated_freeform`

Currently, 14 of the ~35 parser error codes have test coverage. The 22 stubs above represent the remaining uncovered error codes.

---

## Gap 4 — No stress or integration tests ⚠️

**Priority: P2 (stress) / Deferred (integration)**

Both `test/stress/` and `test/integration/` are empty directories. `test/performance/test_performance.c` exists as a standalone binary but is not integrated into the test runner.

**Stress test candidates** (given Sigma.Build will produce large documents):
- Parse a 10,000-statement `.anvl` file
- Deeply nested object chains (100+ levels)
- Array with 10,000 elements
- Resolver with a 500-node inheritance graph
- Serializer round-trip on a large document

**Integration test candidates** (deferred until Sigma.Build is building):
- `sbuild` parsing a real `build.anvl` and resolving all `$pack()` references
- Full round-trip: parse → resolve → serialize → re-parse, check structural equivalence
- AMP dialect: parse a known AMP frame, serialize, compare to known-good output

---

## What Is Well-Covered

The following modules have systematic, numbered test sets with full registration and no gaps:

- **Resolver** — 10 cases covering the full inheritance resolution graph (no-inheritance fast path, single-level, three-level, forward references, cycle detection, `warm_all` idempotency)
- **Serializer** — 19 round-trip cases covering scalar, inline/block object, inline/block array, tuple, untagged/tagged/multiline/special-char blob, minify mode, full-document, AMP dialect, inheritance syntax
- **Vars** — 20 cases covering empty blocks, single/multiple entries, ordering errors, scalar/array/object varref resolution, chain resolution, interpolation, missing/circular ref errors
- **Import** — 10 cases covering no-imports, single, alias, after-statement error, graph build, diamond dedup, cycle detection
- **Meta-buffers** — 20 cases covering base meta allocation/retrieval, inheritance, all value types (scalar/array/object/nested/mixed), attribute metadata, blob tags
- **Source** — 22 cases
- **Context** — 12 cases
- **Interrogators** — 11 cases
- **Messaging** — 17 cases

The existing 56 registered parser tests cover the happy path thoroughly: assignments, objects, tuples, nested arrays, blobs, booleans, numbers (happy path), bare literals, module attributes, inheritance, AMP dialect, all registered error cases, and shebang handling.

---

## Action Items

| Priority | File | Action |
|----------|------|--------|
| P0 | `test_statements.c` | Rewrite from scratch for meta-buffer architecture |
| P1 | `test_parser.c` | Register 12 real-but-unregistered tests (11 valid + fix `generic_aurora` assertion first) |
| P2 | `test/stress/` | Create stress test suite — parse large/deep/wide documents |
| P3 | `test_parser.c` | Promote 21 error-path stubs from `Assert.skip` to active + register |
| P3 | `test_parser.c` | Promote `numbers_stub` once `value_meta` numeric population is complete |
