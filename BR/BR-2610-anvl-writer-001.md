# BR-2610-anvl-writer-001: the writer's name-set hash truncates its FNV-1a constants on 32-bit `size_t`

**ID:** BR-2610-anvl-writer-001
**Type:** Bug Report (portability)
**Owner:** anvl, to be worked by the ANVL coder agent
**Filed:** 2026-10-25
**Filed by:** ANVL Bindings agent, found building `anvil.wasm` against `e6a86b0` (SR-2610-anvl-vendor-bump-001)
**Status:** resolved
**Blocks:** nothing today; should land before FR-2610-anvl-bindings-002 exposes the writer to JS and before SR-2610-anvl-distribution-001 publishes `anvil-wasm`
**Tags:** writer, portability, wasm, hash

---

## Summary

`hash_name` in `src/writer/grammar.c:233` is FNV-1a over a `size_t` accumulator with 64-bit
constants:

```c
static size_t hash_name(const char *s) {
   size_t h = 14695981039346656037ull;
   ...
      h *= 1099511628211ull;
```

On any target where `size_t` is 32 bits (wasm32 under Emscripten, 32-bit ARM, `-m32`), both
constants are silently truncated. Emscripten's clang reports it:

```
vendor/anvil/src/writer/grammar.c:235:15: warning: implicit conversion from 'unsigned long long'
to 'size_t' (aka 'unsigned long') changes value from 14695981039346656037 to 2216829733
[-Wconstant-conversion]
```

The result is a 32-bit hash with a non-standard offset basis and prime, not FNV-1a.

## Impact

Low, and that is why this is not urgent. `hash_name` feeds only the writer's duplicate-name set
(`hash & (capacity - 1)` at `grammar.c:268` and in `nameset_grow`), so the hash is deterministic
and lookups stay correct. What changes is bucket distribution on 32-bit targets, and the hash no
longer matches native. It also puts a compiler warning in every wasm build, and it is a trap for
anyone who later treats this hash as a stable value.

## Expected behavior

`hash_name` computes true 64-bit FNV-1a on every target, the same way `src/sigma/map.c:29-30`
already does:

- accumulate in `uint64_t` with `UINT64_C(14695981039346656037)` and `UINT64_C(1099511628211)`;
- fold to `size_t` only at the return (or at the point of use, if the caller prefers the full
  width). On a 64-bit target the result is bit-identical to today's.

## Test cases

1. **Known vector.** `hash_name` (exposed to the test through the existing grammar test seam, or an
   `#ifdef ANVL_TESTING` accessor if none exists) returns the published 64-bit FNV-1a values for
   `""` (`0xcbf29ce484222325`) and `"a"` (`0xaf63dc4c8601ec8c`), compared as `uint64_t`. On a 64-bit
   build this passes before the fix, so it pins behavior rather than demonstrating the bug.
2. **The bug, observable.** Compile `src/writer/grammar.c` with `-m32 -Wconstant-conversion -Werror`
   (or `-Wall -Wextra -Werror` under clang). It fails before the fix and is clean after. If `-m32`
   libraries aren't installed on a dev machine, a `static_assert(sizeof(uint64_t) == 8)` plus a
   compile-only `make` target `check-32bit` that skips with a message when `-m32` is unavailable is
   acceptable. The Linode box's `emcc` also reproduces it directly.
3. **Duplicate detection unchanged.** Existing writer tests for duplicate names, and a case that
   inserts enough names to force several `nameset_grow` passes, stay green.
4. **Regression.** Full unit suite, `test_release`, `check-exports`, `check-reader-only` green;
   Valgrind-clean on the writer suite.
5. Changelog entry under `## [Unreleased]` → `### Fixed`.

## Notes

- `src/core/source.c:27-30` and `src/sigma/map.c:29-30` define the same constants with
  `UINT64_C`. `map.c` is the model to copy. `source.c` uses a word-wise variant for content hashing
  (BR-2609-anvl-003), and its registry key is worth a look while here: confirm it doesn't assign a
  64-bit value to a `size_t` either. The Bindings agent's wasm build compiled `source.c` without
  that warning, so it is probably fine, but nobody has checked what the value does on 32-bit.
- Verification path for the Bindings agent afterwards: bump `anvil.wasm`'s pin, rebuild on the
  Linode box (`build.sh`), and confirm the warning is gone.

## Resolution

Branch `fix/BR-2610-anvl-writer-001`, commit `9635f58`, merged to `main`. Tests W18-W19 in `test_writer.c`. Full unit regression
green with unchanged counts elsewhere (writer 17 -> 19), `test_release`, `check-exports`,
`check-reader-only` green, Valgrind-clean on writer, writer_roundtrip and builder.

- `hash_name` is now `uint64_t wg_hash_name` (declared in `src/writer/grammar.h` as the test seam):
  `UINT64_C` constants, `uint64_t` accumulator, and `slot_for()` folds to `size_t` only to index the
  table, in both `wg_nameset_add` and `nameset_grow`. A `_Static_assert(sizeof(uint64_t) == 8)` guards it.
- **W18** pins the published vectors (`""` = `0xcbf29ce484222325`, `"a"` = `0xaf63dc4c8601ec8c`, plus
  `"foobar"` = `0x85944171f73967e8`); as the ticket says, it passes before and after on a 64-bit build.
  **W19** inserts 1500 names (several `nameset_grow` passes, capacity >= 4096), then re-inserts all of
  them: every repeat is a duplicate, the count is exact.
- **The bug is not reproducible on this machine.** There are no 32-bit libc headers (`-m32` fails at
  `stdint.h`), no `clang`, no `emcc`. What I did instead: compiled the *old* function body against a
  32-bit stand-in for `size_t` with `-Wall -Wextra -Werror -Woverflow`, which fails with "changes value
  from 14695981039346656037 to 2216829733"; the new shape compiles clean. `make check-32bit` runs the
  real file with `-m32 -Werror` wherever the headers exist and prints SKIPPED otherwise. **Final proof
  is the Bindings agent's rebuild of `anvil.wasm` on the Linode box: the warning should be gone.**
- **Notes item (source.c):** `Source.hash`, `anvl_source_t.hash` and the registry keys are `uint64_t`
  end to end (`source_registry.c` keys on `sizeof(uint64_t)` bytes), and `source_compute_hash` uses
  `UINT64_C` constants and `uint64_t`/`memcpy`. Nothing is narrowed to `size_t`, so it behaves the same
  on 32-bit. No change needed.
