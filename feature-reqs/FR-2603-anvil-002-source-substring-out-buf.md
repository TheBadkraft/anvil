# FR-2603-anvil-002 — `Source.substring` caller-supplied output buffer

| Field      | Value                                               |
|------------|-----------------------------------------------------|
| ID         | FR-2603-anvil-002                                   |
| Status     | **resolved**                                        |
| Priority   | high                                                |
| Tags       | source, api, ownership, allocation, breaking-change |
| Filed      | 2026-03-23                                          |
| Resolved   | 2026-03-29                                          |
| Owner      | anvil                                               |

---

## Summary

Change `Source.substring` (and by consequence `Statement.identifier`) to write into a
caller-supplied buffer rather than allocating internally via `Allocator.alloc()`.

---

## Problem

The current signature:

```c
char *(*substring)(source, usize start, usize len);
```

allocates via `Allocator.alloc()` — the SLB0 reclaim controller — and returns the pointer
to the caller. The caller has no safe, portable way to free it:

- `free()` corrupts the heap (libc free on a bump/reclaim arena pointer)
- `Allocator.free()` works only if the caller knows which controller owns the memory
- The header documents no ownership contract

`Statement.identifier()` is the sole internal caller, so every consumer of that public API
has the same problem. The bug was discovered in `sigma.test/src/config_reader.c` when
`free(ident)` caused a segfault at runtime.

---

## Proposed Change

### `anvl_source_i.substring`

```c
// before
char *(*substring)(source, usize start, usize len);

// after
void (*substring)(source, usize start, usize len, char *out_buf);
```

`len` already constrains the write — the caller must supply `out_buf` of at least `len + 1`
bytes. No return value is needed; the callee writes `len` bytes and null-terminates.

Implementation of `source_substring` becomes:

```c
static void source_substring(source self, usize start, usize len, char *out_buf)
{
    usize available = (usize)(self->buffer.end - self->buffer.bucket) - start;
    usize actual    = len < available ? len : available;
    memcpy(out_buf, (char *)self->buffer.bucket + start, actual);
    out_buf[actual] = '\0';
}
```

### `Statement.identifier`

The `anvl_statement_i.identifier` signature follows the same pattern:

```c
void (*identifier)(statement, source, char *out_identifier);
// caller: char name[64]; Statement.identifier(stmt, src, name);
```

**Rationale:** This matches the Q-OR Memory Ownership convention (see `q-or/DOC_STANDARDS.md`):
- **Caller-supplied buffers** for bounded results (identifiers, substrings)
- **No allocator handoff** — the callee never allocates memory that the caller must free
- Trust boundary preserved — callee writes to caller's buffer without crossing allocation domains

Stack buffers are sufficient for all identifier comparison use cases. The `out_*` prefix signals
caller ownership clearly in the API signature.

---

## Impact

- **Breaking change** to `anvl_source_i` and `anvl_statement_i` vtable layouts
- Single internal call site: `statement_identifier()` in `src/core/context.c`
- All known external callers: `sigma.test/src/config_reader.c` (one call site, already
  working around the bug via raw `meta[]` access — will be cleaned up once this lands)
- No other files in the anvil tree call `Source.substring` directly

---

## Test Cases

| # | Input | Expected |
|---|-------|----------|
| 1 | `Source.substring(src, 0, 5, buf)` where source contains `"hello world"` | `buf == "hello"`, NUL-terminated |
| 2 | `len` exceeds available bytes | writes available bytes, NUL-terminates, no overflow |
| 3 | `start` at last byte, `len == 1` | writes that byte + NUL |
| 4 | `Statement.identifier(stmt, src, buf)` for stmt with ident `"suite"` | `buf == "suite"` |
| 5 | Repeated calls with same buf | idempotent, correct content each time |

---

## Migration Notes

Callers updating from the old signature:

```c
// old — leaks / segfaults on free
const char *id = Statement.identifier(stmt, src);
strcmp(id, "suite");
free(id);  // BUG

// new — stack buffer, no allocation
char id[stmt_ident_len + 1];
Statement.identifier(stmt, src, id);
strcmp(id, "suite");
```

---

## Resolution

**Implemented:** 2026-03-29  
**Branch:** `feat/fr-002-output-buffers`

### Changes Made

1. **API Signatures Updated** (`include/context.h`, `package/include/context.h`):
   - `char *Source.substring(src, start, len)` → `void Source.substring(src, start, len, char *out_buf)`
   - `const char *Statement.identifier(stmt, src)` → `void Statement.identifier(stmt, src, char *out_identifier)`

2. **Implementation** (`src/core/context.c`):
   - `source_substring()` — writes to caller buffer, no allocation
   - `statement_identifier()` — writes to caller buffer, no allocation

3. **Test Migration** (11 files, 40+ call sites):
   - Caller-supplied buffers: stack for known-small, heap for variable-length
   - Pattern: `char *buf = malloc(len+1); API(..., buf); free(buf);`
   - Files: test_resolver, test_parser, test_meta_buffers, test_import, test_anon_block, test_source, test_vars, test_serializer

4. **Allocator Cleanup**:
   - Removed `Allocator.free()` calls from `src/asl/asl.c` (8 sites) — bump allocator pattern
   - Removed `Allocator.free()` calls from `src/vars/vars.c` (5 sites) — bump allocator pattern
   - Updated test helpers to use `free()` for malloc'd buffers, `Allocator.dispose()` for API strings

### Test Results

All 20 unit tests passing:
```
Test Suite Summary (unit/)
========================================
  Total:  20
  Passed: 20
  Failed: 0
========================================
```

### Breaking Changes

This is a **breaking API change**. All external callers must:
1. Allocate buffer before calling API
2. Remove old free/dispose logic on returned pointer
3. Use new output-parameter pattern

No ABI compatibility layer provided — clean break for v0.1.0-alpha.
