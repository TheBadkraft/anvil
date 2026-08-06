# Sigma Collections Roadmap (Set + Map Composition)

Status: planned, intentionally out of current delivery scope.

## Immediate Position (Current Scope)

- Keep Sigma Map as a sparse, open-addressed hash map.
- Keep API thread-compatible, not thread-safe.
- Do not add internal locking in this phase.
- Keep iterator usage explicit: recreate iterators after map mutation.

## Near-Term Hardening Goals

- Preserve minimal map API surface while reducing shared mutable state.
- Keep mutation/iteration invalidation behavior explicit and documented.
- Move toward safer defaults without taking responsibility for concurrent access.

## Set + Map Composition Direction

Goal: identify stand-alone structures, then compose richer containers from those primitives.

### 1) Define internal hash-core interface and slot contract

- Create an internal hash core responsible for:
  - hash/probe/slot-state semantics
  - resize/rehash behavior
  - sparse iteration support
- Keep ownership and lifetime rules centralized in this core.

### 2) Move current Map probing/resize logic into hash core with no behavior changes

- Refactor only internal wiring first.
- Preserve existing public map behavior and signatures.
- Keep all existing map tests green during the extraction.

### 3) Add Set wrapper first

- Build Set as a thin wrapper over hash core.
- Focus on membership-only operations:
  - add
  - has
  - remove
  - count
  - iterator

### 4) Refactor Map wrapper to use the same hash core

- Keep map public API stable while delegating to shared core.
- Preserve sparse iterator behavior for map entry traversal.
- Add tests that validate parity before/after refactor.

## Non-Goals For This Roadmap Phase

- No internal synchronization primitives in the core containers.
- No guarantees for safe concurrent mutation without external synchronization.
- No large API expansion until internal composition is stabilized.

## Exit Criteria

- Shared hash core is adopted by both Set and Map wrappers.
- Existing map test suite remains passing.
- New set test suite validates membership semantics and iteration behavior.
- Thread model documentation remains explicit: thread-compatible, caller-synchronized.
