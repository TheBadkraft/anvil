# `sigma.system.alloc` dependency — a real sibling-repo link, not a vendored copy

Design/rationale record for `FR-2609-sigmem-001`'s resolution: Anvil's `Allocator`
(`sc_allocator_i`) implementation is no longer Anvil's own vendored-and-modified
`src/sigma/memory.c` — it's `sigma.core`'s real `src/system_alloc.c`, compiled directly from that
repo's own checkout.

## What this actually depends on

`anvil` and `sigma.core` must be checked out as **literal siblings**, under the same parent
directory (`.../repos/anvil`, `.../repos/sigma.core`) — the root `Makefile`'s `SIGMA_CORE`
variable (`../sigma.core`) and every test-tier Makefile's own copy of that same relative path
assume this directly. This matches the sibling-relative-path convention this whole sigma.*
ecosystem's own build scripts already use elsewhere (e.g. `sigma.memory`'s own `config.sh`:
`-I../sigma.core/include`) — not a convention invented here.

`sigma.core` must have `feature/FR-2609-sigmem-001-system-alloc` checked out, at or after commit
`db65b98` (`feat: Add sigma.system.alloc — zero-setup sc_allocator_i provider`). That branch is
**not merged to `sigma.core`'s own `main`, and not pushed to its GitHub remote** — there is
nothing public to pin a git submodule to yet. This is a real, load-bearing, currently-undeclared
dependency on another repo's local, uncommitted-upstream state. Revisit this whole arrangement
(ideally converting to a real git submodule, or a real installed-package reference once this
ecosystem's own `cpkg`/`/usr/local/packages/` machinery is actually set up on the build host)
once that branch merges and is pushed.

## Why not a vendored copy

The first pass at this landed `src/system_alloc.c`/`include/system_alloc.h` as a pinned snapshot
*copied* into `anvil/vendor/sigma.core/` — corrected directly: Anvil should not carry copies of
another project's source at all (beyond the still-open `sigma.collections` migration, deliberately
scoped out of this FR — see below). The fix compiles `../sigma.core/src/system_alloc.c` straight
from its real location; nothing from `sigma.core` is copied into this repo.

## The one real header-collision this surfaced

`sigma.core`'s own `include/system_alloc.h` includes `"sigma.core/allocator.h"` /
`"sigma.core/types.h"`. Anvil used to carry its own `include/sigma/allocator.h` — verified
byte-for-byte identical content to `sigma.core`'s real header, only the include-path convention
differed. Having *both* physical copies reachable from the same translation unit is a real
compile error in C (`redeclaration of enumerator 'POLICY_BUMP'`) even though the content is
identical — a full `enum {...}` body, unlike a repeated identical `typedef`, cannot be redeclared.

Resolved by retiring Anvil's own copy entirely: `include/sigma/allocator.h` is deleted, and every
one of its ~25 consumers (`src/core/*.c`, every `src/sigma/*.c` file, all the collections headers
that need the opaque `bump_allocator`/`Allocator` types) now includes `<sigma.core/allocator.h>`
directly — the real header, via the real sibling-repo `-I` path, not a second copy.

## `include/sigma.core/types.h` — a one-line redirect, not a vendored copy

`sigma.core/allocator.h` itself needs `<sigma.core/types.h>` to resolve — and confirmed directly,
that nested path doesn't exist even inside `sigma.core`'s **own** repo on this branch (neither its
raw `include/` nor its packaged `package/include/` has `types.h` at the nested path, only flat at
`include/types.h`). This is a real, pre-existing gap in `sigma.core`'s own packaging, not
something to fix by editing another team's repo from here.

`include/sigma.core/types.h` in this repo is a one-line redirect (`#include <sigma/types.h>`) to
Anvil's own pre-existing, verified-identical types header — it defines nothing itself. Report the
underlying gap back to `sigma.core` rather than treat this shim as a permanent fixture; remove it
once `sigma.core` fixes its own packaging (or once Anvil's own `<sigma/types.h>` is retired in
favor of depending on `sigma.core`'s package directly, a bigger, separate, not-yet-scoped change).

## What's still vendored, and why

`src/sigma/{list,map,arrays,array_base,farray,collections,query}.c` remain Anvil's own vendored
copies — deliberately, not an oversight. Checked directly against `sigma.collections`' real
`list.h`/`map.h`: `sigma.collections` has grown a newer `alloc_use`-aware interface
(`FR-2603-sigma-collections-007`) that Anvil's vendored copies don't have yet. Swapping these to
`sigma.collections.o` now, before that interface parity lands, risks a real ABI mismatch, not just
a rebuild. Per direct instruction: once both (a) all memory-related vendored copies are gone
(done, this note) and (b) `sigma.collections`' interfaces are actually updated/synced, all of
these get removed too, in favor of linking `sigma.collections.o` the same way this note's own
`system_alloc.c` link works — a real sibling-repo reference, not vendored source.

`src/sigma/math.c`/`time.c`/`strings.c` also remain vendored for now — `time.c`/`strings.c` do
have a real home in `sigma.core`'s own `src/`, but swapping those wasn't part of this FR's scope
and hasn't been separately verified for interface parity; `math.c` has no confirmed upstream home
in the sigma.* ecosystem at all as of this writing. Not addressed here — flagged for a future,
separately-scoped pass.

## Related

- `FR-2609-sigmem-001.anvl` (`q-or/feature-reqs/`) — the cross-project design record this
  integration was built against.
- `notes/anvil-sigma-subset-is-rd-sandbox` (session memory, not a file in this repo) — the
  broader framing this migration is one step of: `src/sigma/` was always meant to be a
  research-and-development sandbox for the real, separate Sigma repos, not a permanent fork.
