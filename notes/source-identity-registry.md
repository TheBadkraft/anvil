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
- Defined `Registry` vtable: `add`, `remove`, `find`, `count`, `clear`.
- Added `test/unit/test_registry.c` with passing tests for basic registry ops.

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

## Active step

2. Remove `doc_map` from `anvl_mod_ctx_t` and `strict_namespace` from
   `anvl_ctx_spec_t`.
   - Drop `map doc_map` from the context struct.
   - Drop `bool strict_namespace` from `anvl_ctx_spec_t`.
   - Remove `ANVL_CTX_DEFAULT_STRICT_NAMESPACE` from `include/constants.h`.
   - Update `mod_ctx_initialize` and `mod_ctx_dispose` to stop creating/destroying
     `doc_map`.
   - Update `src/core/module.c` context spec merge to stop touching
     `strict_namespace`.
   - Update `test_module.c` assertions that check `ctx->doc_map` and
     `strict_namespace`.

## Follow-up steps

3. Initialize registry when module initializes; clear registry in `mod_dispose`.
4. Implement `Source.set_error` and `Source.has_errors` using registry lookup.
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
