## Plan: Consolidate Anvil Header Types

Standardize type ownership and naming across the include surface without implementation changes now: centralize public pointer aliases in one place, move concrete/private structs out of public headers, and preserve current symbols via compatibility aliases since API-breaking renames are deferred.

**Steps**
1. Build a type inventory matrix from the current public headers and classify each symbol as public interface, public handle, or internal-only.
2. Define the canonical ownership map for each type family.
   Depends on step 1.
3. Restructure public header layering to remove dependency inversion and cyclic coupling.
   Depends on step 2.
4. Move internal concrete struct layouts and internal helpers into internal headers.
   Depends on step 2.
5. Apply naming consolidation with compatibility aliases (document-first now, code changes later).
   Parallel with step 4 after step 2.
6. Remove deprecated or outgoing API surfaces (Context builder path) from public exposure in a staged way.
   Depends on step 4.
7. Verify headers with include-what-you-use checks and consumer compile tests.
   Depends on steps 3-6.

**Relevant files**
- /root/projects/anvil/include/types.h — should be the canonical public handle/type alias hub; currently also exposes concrete struct anvl_mod_t.
- /root/projects/anvil/include/anvil.h — keep only anvl_*_i interfaces and top-level API constants/includes; avoid owning data-struct layouts.
- /root/projects/anvil/include/source.h — currently contains concrete source storage layout; should expose interface and public handle only.
- /root/projects/anvil/include/context.h — currently mixes public query API with large concrete structs and builder internals that are being retired.
- /root/projects/anvil/include/internal/root.h — owns internal root/doc layouts; should not declare static helper functions for external linkage units.
- /root/projects/anvil/src/core/anvil.c — consumes internal root/doc layout and should be the home of root helper function prototypes.
- /root/projects/anvil/src/core/source.c — uses doc->src and source concrete storage; key consumer during source/doc handle split.
- /root/projects/anvil/src/core/context.c — currently depends on builder and concrete context layout; target for staged builder retirement.

**Verification**
1. Build a header self-include matrix: each public header compiles standalone in a tiny translation unit.
2. Compile core targets that include each public API header directly and through transitive paths.
3. Run unit and infra build-only targets to ensure no behavioral execution side effects during refactor.
4. Run symbol grep checks for forbidden patterns:
   - concrete struct definitions in public headers for internal-only objects
   - internal lower-case aliases used as public return types in public interfaces
   - stale builder symbols in public headers after retirement stage

**Decisions**
- API-break budget: no breaking renames now; produce consolidation map and documentation first, then stage refactors with aliases.
- Context builder direction: builder path is being removed; keep only durable public context query/parse APIs on the public surface.
- Naming convention to enforce:
  - lower-case anvl_* aliases are internal references
  - anvl_*_i interfaces remain public
  - public handle aliases follow struct anvl_*_t *TitleCase pattern

**Further Considerations**
1. Public source/context handles: decide final public alias names now (for example AnvlSource, AnvlContext) before staged migration to avoid repeated churn.
2. Compatibility window: define deprecation span for old aliases before removal (for example one minor release).
3. Internal header policy: enforce include/internal as non-public and block installation/export of those headers in packaging.