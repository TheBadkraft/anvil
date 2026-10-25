# BR-2610-anvl-visibility-001: `libanvil.so` exports no schema or type-registry functions

**ID:** BR-2610-anvl-visibility-001
**Type:** Bug Report (public ABI / packaging)
**Owner:** anvl, worked by the ANVL coder agent (roadmap addendum A1)
**Filed:** 2026-10-25
**Filed by:** anvil (planning), from the coder agent's own finding in FR-2610-anvl-types-001
**Status:** resolved
**Blocks:** FR-2610-anvl-public-api-003, the `anvil.py`/`anvil.net`/`anvil.java` slices of
FR-2610-anvl-bindings-001, and any release that ships `libanvil.so`
**Tags:** abi, visibility, shared-library, schema, types, rivet-readiness

---

## Summary

The library compiles with `-fvisibility=hidden` (`d27bdaf`). Every public header pins its
declarations back to default visibility with `#pragma GCC visibility push(default)` / `pop`:
`anvil_flat.h`, `anvil_vtable.h`, `anvil_writer*.h`, `anvil_builder*.h`. `include/anvil_schema.h`
and `include/anvil_type_registry.h` don't. Their functions are therefore hidden in the shared
object.

## Reproduction

Checked against `main` at `1ed7e1c`. `make so-release` into a scratch directory, then
`nm -D --defined-only libanvil.so`:

| Symbol | Result |
|---|---|
| `anvil_load_buffer`, `anvil_value_get_numeric`, `anvil_writer_new`, `anvil_document_get_path` | exported |
| `anvil_schema_load`, `anvil_schema_validate_value`, `anvil_type_registry_load_from_includes` | **missing** |

## Impact

- Every consumer that loads `libanvil.so` can parse but can't use AnvilTypes or AnvilSchema at all.
  That covers `anvil.py` (ctypes), `anvil.net` (P/Invoke) and `anvil.java` (FFM), and also C
  programs linking the shared object.
- `anvil.node` and `anvil.wasm` compile the sources into their own artifact, so they aren't affected.
- Nothing caught it: `test_release` links the static archive.

## Expected behavior

- Both headers carry the same `push(default)` / `pop` wrapper and comment as `anvil_flat.h`.
- A check fails the build when any function declared in a public header is missing from
  `nm -D` of the release shared object. Add it as a `make check-exports` target alongside
  `check-reader-only`, so the next new header can't repeat this.

## Test cases

1. `nm -D --defined-only lib/release/libanvil.so` lists every `anvil_schema_*` and
   `anvil_type_registry_*` / `anvil_type_*` function declared in the two headers.
2. `make check-exports` passes. It fails if the wrapper is removed from either header (verify once
   by hand).
3. Internal symbols stay hidden: no `anvl_*`, `sigma`, `List`/`Map`/... symbols in `nm -D`
   (the guarantee from `d27bdaf`).
4. Full unit suite green; `test_release` green; Valgrind-clean on schema and types.
5. Changelog entry under `## [Unreleased]` → `### Fixed`.

## Resolution

Branch `fix/BR-2610-anvl-visibility-001`, commit `003e843`, merged to `main`.

- Both headers carry the `push(default)` / `pop` wrapper and comment.
- `make check-exports` (builds `so-release`, then `scripts/check-exports.sh`): every function declared
  in `include/anvil_*.h` (declarations starting in column 0, plus the `extern const` vtable objects) must
  appear in `nm -D --defined-only`, and no internal symbol may. Test case 2 verified by hand: with the
  schema header's pragma changed to `hidden` the gate fails listing every `anvil_schema_*` symbol;
  restored, it passes (161 public symbols).
- **Test case 3 needed more than the wrapper.** `Allocator` (a sigma global from the prebuilt
  `/usr/local/packages/sigma.system.alloc.o`, outside `-fvisibility=hidden`'s reach) was exported from
  `libanvil.so`: the cross-addon collision risk `d27bdaf` fixed, still open for that one symbol. A
  version script `libanvil.exports` (global: `anvil_*` and the ten vtable objects; local: everything else)
  now defines the exported set. A new public vtable object (e.g. FR-2610-anvl-public-api-003's
  `Schema`/`TypeRegistry`) must be added to the map, and `check-exports` fails until it is.
- `include/anvil.h` is not scanned: it declares only the legacy internal `Anvl` vtable, which must stay hidden.
- Real-consumer check: a C program linked against `libanvil.so` loads a schema, validates, and reads a
  violation.
- Unit suites green (counts unchanged), `test_release`, `check-reader-only`, Valgrind-clean on
  schema and types. Debug and release `.so` both link with the map.
