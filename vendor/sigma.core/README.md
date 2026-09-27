# vendor/sigma.core — pinned snapshot, not a submodule (yet)

This directory carries exactly two files pulled verbatim from the real `sigma.core` repo:

- `src/system_alloc.c` / `include/system_alloc.h` — the `sigma.system.alloc` provider
  (`FR-2609-sigmem-001`): a zero-setup, zero-Module-bootstrap `sc_allocator_i` implementation,
  ported from this repo's own `src/sigma/memory.c` and brought into the sigma.* ecosystem proper.

**Source**: `sigma.core` repo, branch `feature/FR-2609-sigmem-001-system-alloc`, commit `db65b98`.
That branch is not yet merged to `sigma.core`'s own `main` and has not been pushed to its GitHub
remote — a real git submodule isn't possible yet (there's nothing public to pin to). This is a
deliberate, documented interim state, not the vendored-and-drifting-copy problem this whole
migration exists to get away from: these two files are an exact, unmodified copy of a specific
named commit's `src/system_alloc.c`, with exactly one line changed in `include/system_alloc.h`
(see below) — not an independently-maintained fork.

**One deliberate edit**: upstream's `system_alloc.h` includes `"sigma.core/allocator.h"` and
`"sigma.core/types.h"`. Anvil already has its own `include/sigma/allocator.h` (which itself
already includes `<sigma/types.h>`) — verified byte-for-byte identical content to sigma.core's
real headers, only the include-path convention differs. Vendoring a *second* physical copy of
`sc_alloc_policy` et al. under `sigma.core/` collides at compile time: a full `enum {...}` body,
unlike a repeated identical `typedef`, cannot be redeclared in C even when the two definitions are
byte-identical — confirmed directly by the real `error: redeclaration of enumerator 'POLICY_BUMP'`
this caused before the fix. The fix: `system_alloc.h`'s own includes here are repointed to
`#include <sigma/allocator.h>` (Anvil's existing header) instead of vendoring a colliding second
copy. No logic changed, only which physical header supplies the same interface.

**Revisit this whole directory once `sigma.core` merges `FR-2609-sigmem-001` to `main` and pushes
it** — at that point this should become a real git submodule (matching every other vendored
dependency's convention in this ecosystem), and `system_alloc.h`'s include-repoint noted above can
either stay (if `sigma.core` itself ever wants to depend on Anvil's headers, unlikely) or be
reverted to the upstream form once Anvil's own `<sigma/allocator.h>` is retired in favor of a real
shared dependency on `sigma.core`'s package.
