# BR-2610-anvl-writer-001: the writer's name-set hash truncates its FNV-1a constants on 32-bit `size_t`

**ID:** BR-2610-anvl-writer-001
**Type:** Bug Report (portability)
**Owner:** anvl, to be worked by the ANVL coder agent
**Filed:** 2026-10-25
**Filed by:** ANVL Bindings agent, found building `anvil.wasm` against `e6a86b0` (SR-2610-anvl-vendor-bump-001)
**Status:** open
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
