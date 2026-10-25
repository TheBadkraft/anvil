# FR-2610-anvl-types-001: type-registry diagnostics — name collisions and malformed definitions are reported, not silently resolved

**ID:** FR-2610-anvl-types-001
**Type:** Feature Request (native library)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** open — not started
**Tags:** types, registry, diagnostics, includes, rivet-readiness

---

## Summary

`anvil_type_registry_load_from_includes` merges every `@[types]` include into one flat `types.`
namespace keyed by bare name. When two includes define the same name, the later one wins
silently (`Map.set` at `src/anvil_types.c` ≈ line 191; also listed under "What's not built yet" in
`docs/types-reference.md`). Malformed definitions are also accepted silently. Examples: an
unrecognized `type :=` value, a non-numeric `size`, or an enum `values` that isn't an array. Each
just yields a definition with missing data.

A consumer can't tell "this type is what I declared" from "this type was replaced, or partly read."

## Motivation

Rivet will split its types across files (shared `common.types.anvl`, domain `ticket.types.anvl`).
A collision there silently changes validation behavior, and the only symptom is a wrong accept or
reject somewhere downstream.

## Design

Primitives, not policy: the registry *reports*. Whether a collision is fatal stays the
consumer's call.

- Add a diagnostics list to the registry handle:
  `size_t anvil_type_registry_get_diagnostic_count(anvil_type_registry)`,
  `anvil_type_diagnostic anvil_type_registry_get_diagnostic(anvil_type_registry, size_t)`,
  and accessors for category, type name, source document path, and message, using the same
  snprintf-style buffer convention as the rest of the public API.
- Categories (a stable, public enum, independent of `errors.h`):
  `NAME_COLLISION`, `UNKNOWN_BASE_TYPE`, `MALFORMED_CONSTRAINT`, `MALFORMED_ENUM_VALUES`.
- Collision resolution stays deterministic and documented: last-write-wins stays the behavior,
  and is now reported. The diagnostic names both source documents.
- No change to `anvil_type_registry_find`/`anvil_type_resolve` signatures.

## Test cases

1. Two included `@[types]` documents both defining `VIN`: diagnostic count is 1, and the category
   is `NAME_COLLISION`. Name is `VIN`. The message names both include paths. `anvil_type_resolve(reg, "VIN")`
   returns the definition from the later include.
2. `Foo := { type := Strng; }` yields one `UNKNOWN_BASE_TYPE` diagnostic naming `Foo`.
3. `Foo := { type := String; size := abc; }` yields one `MALFORMED_CONSTRAINT` diagnostic, and
   `anvil_type_def_get_size` returns false (not a declared size of 0).
4. `E := { type := enum; values := red; }` yields one `MALFORMED_ENUM_VALUES` diagnostic.
5. A registry built from clean includes reports 0 diagnostics.
6. Diagnostics survive disposal of the source documents (same independence rule as schema handles).
7. Valgrind-clean; full suite green.

## Dependencies

None. BR-2610-anvl-types-001 is related: a non-numeric constraint becomes a
`MALFORMED_CONSTRAINT` diagnostic here.
