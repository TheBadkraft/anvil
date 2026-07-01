# Anvil — Test Gap Analysis

**Audited:** 2026-03-12
**Updated:** 2026-07-01 (active-suite refresh)
**Scope:** Historical gap ledger for parser-era suites; active quality gate is TestBit targets in `test/unit/Makefile`
**Method:** Compare implemented functions against registered tests in each suite

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
| `test_vars.c` | 23 | 23 | 0 | 0 | ✅ Clean |
| `test_parser.c` | 80 | 68 | ~22 stubs (P3) | 0 | ⚠️ Stubs remain |
| `test_schema.c` | 20 | 20 | 0 | 0 | ✅ Clean |
| `test_statements.c` | 20 | 20 | 0 | 0 | ✅ Clean |

---

## Gap 1 — `test_statements.c` ✅ RESOLVED (P0)

**Completed:** 2026-03-12

Rewritten from scratch for the meta-buffer architecture. 20 tests (ST01–ST20) covering:
- Statement count (single, multi, empty document)
- `get_statement` by index + identifier span
- Scalar, object, array, tuple value types and spans
- Object field count and key spans
- Attribute presence (graceful degradation for scalars)
- `base_meta` absent/present/span
- Out-of-range boundary

All 20 tests pass.

---

## Gap 2 — `test_parser.c`: 12 real tests ✅ RESOLVED (P1)

**Completed:** 2026-03-12

All 12 real-but-unregistered tests registered. Also fixed:
- Stale `exp_pos` values in 6 sample-file tests (files changed since tests were written):
  - `attributes.anvl`: 56 → 83, line 5 → 7
  - `inherits.anvl`: 54 → 58 (line 5)
  - `modpack.anvl`: 164 → 6, line 5 → 2
  - `objects.anvl`: 6 → 7, line 5 → 3
  - `tuples.anvl`: 22 → 24, line 5 → 4
  - `generic.aurora`: 0 → 18, line 1 → 2
- `test_parse_inheritance_placeholder` used empty object `{ }` (not allowed) → changed to `{ x := 1 }`
- `test_parse_generic_aurora` had vacuous `Assert.isTrue(true, ...)` → replaced with `stmt_count == 0` (empty ASL file)

Parser test suite: **68/68 passing**.

---

## Gap 3 — `test_parser.c`: Assert.skip stubs ⚠️

**Priority: P3**

These stubs are marked with `Assert.skip("Parser error check not implemented")`. They exist as placeholders for error-path coverage that was never completed. The error-test infrastructure is functional; these stubs just need to be promoted and registered.

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
- `test_parse_unexpected_char`

**Lexer error paths:**
- `test_parse_invalid_exponent`
- `test_parse_invalid_hex_literal`
- `test_parse_invalid_number`
- `test_parse_unterminated_freeform`

(`test_parse_unexpected_token` was promoted and registered in P1.)

Currently, ~14 of the ~35 parser error codes have test coverage. The stubs above represent the remaining uncovered error codes.

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
- **Import** — 10 cases covering no-imports, single, alias, after-statement error, graph build, diamond dedup, cycle detection
- **Meta-buffers** — 20 cases covering base meta allocation/retrieval, inheritance, all value types (scalar/array/object/nested/mixed), attribute metadata, blob tags
- **Statements** — 20 cases covering count, access, all value types, attributes, inheritance, boundary (ST01–ST20)
- **Source** — 22 cases
- **Context** — 12 cases
- **Interrogators** — 11 cases
- **Messaging** — 17 cases

The 68 registered parser tests cover the happy path thoroughly: assignments, objects, tuples, nested arrays, blobs, booleans, numbers (happy path), bare literals, module attributes, inheritance, AMP dialect, all registered error cases, shebang handling, and all sample files.

---

## Action Items

| Priority | File | Action |
|----------|------|--------|
| ~~P0~~ | ~~`test_statements.c`~~ | ~~Rewrite from scratch~~ ✅ Done |
| ~~P1~~ | ~~`test_parser.c`~~ | ~~Register 12 real-but-unregistered tests~~ ✅ Done |
| P2 | `test/stress/` | Create stress test suite — parse large/deep/wide documents |
| P3 | `test_parser.c` | Promote ~22 error-path stubs from `Assert.skip` to active + register |
| P3 | `test_parser.c` | Promote `numbers_stub` once `value_meta` numeric population is complete |
