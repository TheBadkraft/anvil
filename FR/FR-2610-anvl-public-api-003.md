# FR-2610-anvl-public-api-003: `Schema` and `TypeRegistry` vtables

**ID:** FR-2610-anvl-public-api-003
**Type:** Feature Request (native library / public API)
**Owner:** ANVL Bindings agent. Not part of `notes/roadmap-2610-rivet-readiness.md`; the ANVL
coder agent does not work this ticket.
**Filed:** 2026-10-25
**Filed by:** anvil (planning)
**Requested by:** rivet, flywire (downstream consumers, through the `anvil.py`/`anvil.net` bindings)
**Status:** open — not started. Phase 3 is merged; **blocked on BR-2610-anvl-visibility-001**
(schema/type-registry functions aren't exported from `libanvil.so`, so vtable instances over them
would have nothing to bind through)
**Decision:** roadmap D4, answered 2026-10-25: add the vtables after Phase 3
**Tags:** public-api, vtable, schema, types, bindings, rivet-readiness

---

## Summary

`include/anvil_schema.h` and `include/anvil_type_registry.h` are flat-only. Every other public
surface also ships a vtable that is pointer-identical to its flat functions: the reader
(`anvil_vtable.h`), the writer (`anvil_writer_vtable.h`, `extern const anvil_writer_i Writer`) and
the builder (`anvil_builder_vtable.h`). `anvil.py` (ctypes `Structure` + `in_dll`) and `anvil.net`
(delegates over the struct) bind through those vtables. So their slices of FR-2610-anvl-bindings-001
can't start until schema and the type registry have one too. `anvil.node` calls C directly and
isn't blocked.

## Why after Phase 3

Phase 3 adds most of this surface: `validate_value` + options, nested rules, diagnostics,
numeric-bound getters, `format`/`tag`, and introspection. Adding the vtables earlier would mean
re-ordering or appending struct members after a binding has already mirrored the layout. Doing it
once over the settled surface gives each binding one layout to mirror.

## Design

Follow the `Writer`/`Builder` precedent exactly:

- **New headers:** `include/anvil_schema_vtable.h` (`typedef struct anvil_schema_i {...} anvil_schema_i;`,
  `extern const anvil_schema_i Schema;`) and `include/anvil_type_registry_vtable.h`
  (`anvil_type_registry_i`, `extern const anvil_type_registry_i TypeRegistry;`). Each includes only
  its own flat header.
- **Members:** one per public function in the matching flat header *as it stands after Phase 3*,
  in header declaration order, named without the module prefix (`load`, `validate`,
  `validate_value`, `get_violation_count`, ...), the way `anvil_writer_i` does it.
- **Definitions:** the const instances live next to the flat implementations, as
  `src/writer/writer_vtable.c` does for the writer, and are wired into the Makefile.
- **Export:** both instances must stay in the dynamic symbol table now that internal symbols
  are hidden (`d27bdaf`). Check with `nm -D lib/release/libanvil.so`.
- **Name check:** confirm `Schema` and `TypeRegistry` collide with no existing exported symbol
  before committing to them.

## Test cases

1. `Schema`: every member is pointer-identical to its flat function (same shape as `test_writer.c`'s
   `W16_vtable_matches_flat`).
2. `TypeRegistry`: the same.
3. Completeness: every public function declared in `anvil_schema.h` and `anvil_type_registry.h`
   has a vtable member. The test enumerates them, so a function added later without a member
   fails the suite.
4. `nm -D lib/release/libanvil.so` lists `Schema` and `TypeRegistry`.
5. A load → validate → read-violations round trip through `Schema` only (no flat calls) gives
   the same results as the flat path.
6. Full unit suite green; Valgrind-clean.

## After this lands

- `anvil.py` and `anvil.net` slices of FR-2610-anvl-bindings-001 bind these structs.
- `site/assets/docs/Bindings-Guide.md` lists the two new vtables alongside `Writer`/`Builder`.
- `docs/changelog.md` gets an `## [Unreleased]` → `### Added` entry naming this FR. This is part
  of "done", not a follow-up.
