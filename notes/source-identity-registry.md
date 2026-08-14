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

## Active step

4. Implement `Source.set_error` and `Source.has_errors` using registry lookup.

## Follow-up steps

5. Update parser to take `anvl_source` and emit errors via source interface.
6. Remove or disable intermediate tests that are no longer relevant.

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
