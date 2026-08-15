# Source Identity Registry

## Status

In progress. Transitioning from per-context namespace/doc_map identity to a
process-wide source-hash registry where `module_document` is the document
identity.

## Goal

- Use `Source.hash(src)` as the canonical document identifier.
- Maintain a process-wide registry mapping source hash → `module_document`.
- Pass `anvl_source` through the parser; let parser errors route through
  `Source.set_error(src, ...)` → registry lookup → context error list.
- Reuse one `anvl_parser` instance per context across all sources.

## Completed

- Added `uint64_t hash` to `anvl_source_t` (FNV-1a 64-bit content hash).
- Added `Source.hash(src)` accessor.
- Created `include/internal/source_registry.h` and `src/core/source_registry.c`.
- Defined `Registry` vtable: `init`, `add`, `remove`, `find`, `count`, `release`,
  `clear`.
- Added `test/unit/test_registry.c` with passing tests for basic registry ops
  and reference-counted lifecycle (REG07, REG08).

## Completed step 1

- Added `string filepath` to `anvil_mod_doc_t`.
- Removed `anvil_doc_identity_t` and `doc_identity` typedef from `types.h` and
  `module.h`.
- Updated `mod_ctx_register_doc` signature to
  `mod_ctx_register_doc(ctx, doc, filepath, err_code)`.
- Implemented `mod_ctx_register_doc` to set `doc->context`, copy `filepath`,
  register the document in the global `Registry`, and append it to `ctx->docs`.
- Updated `doc_dispose` and `Debug.dispose_doc` to remove the document from the
  registry and dispose `filepath`.
- Removed obsolete `make_stub_doc_identity` helper from `test/utilities/debug.c`.
- Updated `test/unit/test_document.c` DOC09/DOC10 to validate the new identity
  model.
- Updated `test/unit/test_module.c` calls to `Debug.stub_doc`.

## Completed step 2

- Removed `map doc_map` from `struct anvl_mod_ctx_t` in `include/internal/module.h`.
- Removed `bool strict_namespace` from `anvl_ctx_spec_t` in `include/types.h`.
- Removed `ANVL_CTX_DEFAULT_STRICT_NAMESPACE` from `include/constants.h`.
- Updated `mod_ctx_initialize` and `mod_ctx_dispose` to stop creating/destroying
  `doc_map`.
- Updated `src/core/module.c` context spec merge to stop touching
  `strict_namespace`.
- Updated `test/unit/test_module.c` to remove assertions checking `ctx->doc_map`
  and `strict_namespace`.

## Completed step 3

- Added module reference counting to the source registry
  (`module_ref_count` in `src/core/source_registry.c`).
- `Registry.init()` increments the module reference count and idempotently
  initializes the registry map.
- `Registry.release()` decrements the module reference count and clears the
  registry map when the count reaches zero.
- `Registry.clear()` remains an unconditional reset (primarily for tests).
- `mod_new()` calls `Registry.init()` after successful allocation.
- `mod_dispose()` calls `Registry.release()` instead of `Registry.clear()`.
- `mod_initialize()` error path now calls `mod_dispose(&mod)` to ensure the
  registry reference is released if module creation succeeds but context
  initialization/attach fails.
- Added `CR16_mod_dispose_releases_registry` to `test/unit/test_module.c`.

## Completed step 4

- Implemented `Source.has_errors(src)` in `src/core/source.c`.
  - Returns `false` for NULL source, zero hash, or unregistered source.
  - Uses `Registry.find(Source.hash(src))` to locate the owning document.
  - Returns `true` when the document's context error list is non-empty.
- Implemented `Source.set_error(src, code, line, column, file, out_err_code)` in
  `src/core/source.c`.
  - Returns `ANVL_RES_ERR` for NULL source, zero hash, or unregistered source.
  - Routes to `anvl_error_set(doc->context->errors, ...)` via registry lookup.
- Added `set_error` function pointer to `anvl_source_i` in
  `include/internal/source.h`.
- Added SRC08 tests in `test/unit/test_document.c`:
  - `SRC08a`: registered source reports no errors initially.
  - `SRC08b`: set_error records an error and has_errors reports true.
  - `SRC08c`: set_error fails for an unregistered source.
  - `SRC08d`: has_errors(false) for NULL source.
  - `SRC08e`: set_error fails for NULL source.

## Step 5 (deferred)

- Deferred: update parser to take `anvl_source` and emit errors via source
  interface. The parser will be revisited after document header/import loading
  is defined.

## Completed step 6 (cleanup)

- Disabled `test/unit/test_fixtures.c` in the Makefile. It depends on
  `internal/root.h` and deprecated `anvl_doc` / `Anvl.load` APIs that no longer
  exist after the source-hash identity refactor.
- Renamed `test_doc09_register_doc_identity` to `test_doc09_register_doc` in
  `test/unit/test_document.c` and updated the test runner label.

## Next iteration

Document header scanning and import resolution (see `document-header-scan.md`):

- Define the document header boundary: content from the optional shebang up to
  (but not including) the first valid ANVL statement.
- Resolve whether module-level attributes (`@[...]`) belong to the header or
  to the statement stream.
- Design header scanning to collect imports/usings so the loader can build the
  dependency graph before parsing the document body.

## Decisions

- Registry key is the source content hash (`source->hash`).
- Hash collisions accepted as acceptable risk for this stage (FNV-1a 64-bit).
- `module_document` retains a `context` back-pointer for convenience.
- `module_document` does **not** cache its own hash; `Source.hash(source)` is
  cheap and the source object owns the canonical value.
- Source reload is allowed: `doc_unload_source` must unregister the old hash;
  re-registration picks up the new hash.
- Registry is process-wide with module reference counting to support the
  intended one-module-per-process design while tolerating multiple modules in
  tests.

## Source interface test coverage

A dedicated Source test suite was added in `test/unit/test_source.c` (SRC00–SRC22) to exercise every public `Source` helper before the body parser consumes them. The suite covers create/dispose, buffer/file loading, FNV-1a hash stability, position/line/column tracking, EOF/peek/match helpers, character classification, consume bounds, data/length accessors, whitespace/comment skipping, shebang detection, and registry-backed `has_errors`/`set_error`. All 23 tests pass and are Valgrind-clean.

Shebang parsing was subsequently moved from the header scanner into `Source.from_buffer`/`Source.from_file`. The source object now carries a `has_shebang` flag and advances its position past the shebang line during load, so the header scanner starts on imports/attributes/body rather than re-reading the shebang. File-extension hints are applied only when no shebang is present.

## Valgrind follow-up

After the header scanner landed, `make test_document val` and `make test_header val` showed 2,072 bytes still reachable. The reachable blocks were the source-hash registry map allocated on the first `mod_ctx_register_doc` call inside a test. Tests that use `mod_ctx_initialize` directly (without `mod_new`) never called `Registry.release()`, so the map persisted until process exit.

To obtain clean Valgrind reports, each unit-test teardown function (`td`/`th`/`tm`/`ts`/`tr`) now calls `Registry.clear()` after resetting context spec defaults. `Registry.clear()` unconditionally disposes the registry map and resets the module reference count, which is safe in test teardown but must not be used in normal module lifecycle code.

All active unit suites (`test_module`, `test_document`, `test_registry`, `test_header`, `test_source`) now report 0 errors, 0 definitely lost bytes, and 0 bytes in use at exit.

To make unit-test runs report clean under Valgrind, test teardown can call `Registry.clear()` after the last test. `Registry.clear()` is already exposed in the public registry interface and unconditionally disposes the map and resets the refcount.

Two source-lifecycle bugs were also fixed during the Valgrind pass:

1. `Source.from_file` now frees the intermediate buffer returned by `Files.load` after copying it into the source object.
2. `doc_load_source` no longer calls `Source.create` when `doc->source` already exists; previously it created a new source object and leaked the old one.

## End of Line