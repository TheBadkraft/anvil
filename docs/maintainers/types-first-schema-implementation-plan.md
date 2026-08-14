# Types-First Then Schema Implementation Plan

Status: Draft for execution
Date: 2026-07-05
Scope: Replace legacy schema behavior with a split model where type modules are implemented and stabilized first, followed by schema modules.

## Goals

1. Enforce a hard conceptual split between type definitions and schema field definitions.
2. Keep schema authoring within existing value semantics (object blocks with keys such as type, size, min, max, default, optional).
3. Support two paths for FlyWire and other consumers:
   1. Authored `.asch` schemas.
   2. Derived schema artifacts from DBMS schema via registry tooling.

## Design Decisions (Frozen For This Plan)

1. Two explicit module opt-ins:
   1. `@[types]` for type modules (`.anvl`).
   2. `@[schema]` for schema modules (`.asch`).
2. Built-in weak type catalog is always available under `types.*`.
3. Strong types are opt-in and imported explicitly (for example `import "core.types.anvl" as T`).
4. Schema field definitions remain object-based and do not introduce new parser value forms.
5. Field constraints may tighten imported type constraints but cannot relax them.

## Target Language Surface (v1)

### Types Module (`@[types]`)

```anvl
#!aml
@[types]

VIN := {
    base := types.string
    length := 17
}

Int32 := {
    base := types.numeric
    bytes := 4
    signed := true
}
```

### Schema Module (`@[schema]`)

```anvl
#!aml
@[schema]
import "fleet.types.anvl" as T

TableSchema := {
    vehicle_id := { type := T.Int32, required := true }
    vin        := { type := T.VIN, required := true }
    status     := { type := types.string, length := 12, default := active }
}
```

### Weak-Type-Only Schema (No Imported Strong Types)

```anvl
#!aml
@[schema]

TableSchema := {
    vehicle_id := { type := types.numeric, bytes := 4 }
    vin        := { type := types.string, length := 17 }
}
```

## Phase Plan

## Phase 0 - Cleanup and Archive (Current)

1. Archive stale schema docs and keep a short pointer doc in place.
2. Mark legacy schema guide as non-normative.
3. Freeze this implementation plan as source of truth for execution.

Exit criteria:
1. Active schema docs do not claim deprecated syntax as normative.
2. Contributors can find the current plan from docs/maintainers.

## Phase 1 - Type System Foundation (`@[types]`) 

1. Define type module parser/resolver contracts using existing AST metadata only.
2. Implement a `types` resolver producing a type registry IR:
   1. Built-in weak types (`types.string`, `types.numeric`, `types.bool`, etc.).
   2. Imported strong types by namespace alias.
3. Implement type inheritance/derivation checks for `base` and structural constraint validation.
4. Add duplicate-name detection and deterministic error codes.
5. Add serializer for registry cache records (used by schema registry).

Exit criteria:
1. Full `types` unit suite green.
2. No dependency on schema validator for type resolution.
3. Valgrind clean on `types` tests.

## Phase 2 - Schema Core (`@[schema]`) 

1. Implement schema resolver that consumes:
   1. Built-in weak type registry.
   2. Imported strong type registries.
2. Compile field blocks into schema field IR using object-key constraints:
   1. `type`, `length` or `size` or `bytes`, `min`, `max`, `required`, `optional`, `default`, `nullable`.
3. Enforce constraint composition rules:
   1. Schema field can tighten type constraints.
   2. Relaxing base type constraints is compile-time schema error.
4. Validate data documents against compiled schema field IR.

Exit criteria:
1. Full schema unit suite green.
2. Legacy ambiguous declaration paths removed.
3. Valgrind clean on schema tests.

## Phase 3 - FlyWire and Registry Integration

1. Add DBMS-to-schema derivation pipeline:
   1. Source DBMS metadata.
   2. Produce normalized `.asch` and optional compatibility `.meta.anvl` artifact.
2. Cache compiled type/schema registries for fast validation.
3. Expose deterministic cache key strategy (schema hash + imports hash).

Exit criteria:
1. Same DBMS input produces deterministic schema output.
2. FlyWire can consume generated `.asch` without legacy `.meta.anvl` semantics.

## Test Strategy

1. Types parser/resolver tests:
   1. valid type definition
   2. duplicate type name
   3. unknown base type
   4. invalid constraint key for base kind
2. Schema resolver tests:
   1. weak-only schema
   2. imported strong types
   3. unknown alias reference
   4. type-relaxation rejection
3. Validator tests:
   1. required/default/optional interactions
   2. numeric bounds and width checks
   3. string length checks
   4. nullable behavior
4. Migration tests:
   1. legacy forms fail with explicit error codes
   2. error messages point to replacement patterns

## Suggested Implementation Order (Code)

1. Introduce new type registry interfaces and IR structs.
2. Add `types` resolver module and tests.
3. Refactor schema resolver to depend on type registry API.
4. Implement schema validator on compiled field IR.
5. Add import resolution and cache plumbing.
6. Remove legacy schema resolution branches and stale tests.

## Risks and Mitigations

1. Risk: weak and strong constraints collide unexpectedly.
   Mitigation: explicit tighten-only rule with compile-time conflict diagnostics.
2. Risk: import cycles across type modules.
   Mitigation: DAG check in importer and cycle error code.
3. Risk: FlyWire migration stalls due to artifact expectations.
   Mitigation: temporary dual-output mode (`.asch` primary, `.meta.anvl` compatibility).

## Deliverables

1. New type resolver implementation and unit suite.
2. New schema resolver/validator implementation and unit suite.
3. Registry cache format and integration hooks.
4. Updated user-facing docs with split semantics and migration examples.
