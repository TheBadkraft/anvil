# `sigma.system.alloc` dependency — a real system-installed package, not vendored or sibling-relative

Design/rationale record for `FR-2609-sigmem-001`'s resolution: Anvil's `Allocator`
(`sc_allocator_i`) implementation is no longer Anvil's own vendored-and-modified
`src/sigma/memory.c` — it's `sigma.core`'s real `src/system_alloc.c`, linked as a real installed
package, the same way any other system library is.

## What this actually depends on

A one-time, machine-level install, done once per dev machine (or CI image):

```sh
sudo mkdir -p /usr/local/packages
sudo chown "$(whoami)" /usr/local/packages /usr/local/include

mkdir -p /usr/local/include/sigma.core
cp <sigma.core>/include/sigma.core/allocator.h /usr/local/include/sigma.core/allocator.h
cp <sigma.core>/include/types.h                /usr/local/include/sigma.core/types.h
cp <sigma.core>/include/system_alloc.h          /usr/local/include/system_alloc.h

gcc -O2 -DNDEBUG -fPIC -std=c2x -I/usr/local/include \
    -c <sigma.core>/src/system_alloc.c -o /usr/local/packages/sigma.system.alloc.o
```

`<sigma.core>` is a checkout of the `sigma.core` repo, branch `feature/FR-2609-sigmem-001-system-alloc`,
at or after commit `db65b98` (`feat: Add sigma.system.alloc — zero-setup sc_allocator_i provider`).
That branch is **not merged to `sigma.core`'s own `main`, and not pushed to its GitHub remote** —
there is nothing public to pin a git submodule to yet. Revisit this whole install step (ideally
replaced by `sigma.core`'s own real `cpkg`/package-publish tooling, once that's actually set up on
build hosts) once that branch merges and is pushed — at that point `/usr/local/packages/sigma.system.alloc.o`
should come from a real `cpkg`/`cpub` run against `sigma.core`, not a hand-run `gcc -c`.

Anvil's own `Makefile` (and `test/unit`, `test/sigma`, `test/infra`'s Makefiles) just reference
the two fixed, absolute paths — `-I/usr/local/include` and `/usr/local/packages/sigma.system.alloc.o`
— directly. Nothing in this repo computes or assumes a checkout layout for `sigma.core` at all.

## Why this, not a vendored copy, and not a sibling-relative path either

Two earlier passes at this were each corrected in turn:

1. **First pass**: `src/system_alloc.c`/`include/system_alloc.h` landed as a pinned snapshot
   *copied* into `anvil/vendor/sigma.core/`. Corrected directly: Anvil should not carry copies of
   another project's source at all (beyond the still-open `sigma.collections` migration,
   deliberately scoped out of this FR — see below).
2. **Second pass**: fixed the vendoring by compiling `../sigma.core/src/system_alloc.c` directly
   from a real **sibling-repo checkout** (`anvil` and `sigma.core` as literal siblings under the
   same parent directory). This genuinely removed the vendored copy, but broke the moment `anvil`
   is checked out *nested* inside something else — which is exactly how every binding vendors it
   (`anvil.net/vendor/anvil`, `anvil.node/vendor/anvil`, etc.). Confirmed directly, not assumed: a
   real simulation (clone a binding's `vendor/anvil`, bump it to this commit, `make so-release`)
   failed with `sigma.core/allocator.h: No such file or directory`, because `../sigma.core`
   resolved to `anvil.net/vendor/sigma.core`, which doesn't exist.

The system-install approach fixes this completely: `/usr/local/...` is absolute and machine-wide,
so it resolves identically regardless of where `anvil` is checked out — standalone, or nested five
levels deep inside some binding's `vendor/` directory. Re-verified the same way the sibling-path
version was shown to break: cloned a binding's `vendor/anvil`, bumped it to this fix, ran
`make so-release` — succeeds.

This also matches a convention that already exists in this ecosystem but wasn't populated on this
dev machine until now: `sigma.memory`'s own `config.sh` already references
`/usr/local/packages/sigma.core.o`-style paths as its own expected dependency shape.

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
directly — the real header, now resolved via `/usr/local/include`, not a second copy.

## `/usr/local/include/sigma.core/types.h` — a real copy of a header with nowhere else to live, not a vendored-and-modified one

`sigma.core/allocator.h` itself needs `<sigma.core/types.h>` to resolve — and confirmed directly,
that nested path doesn't exist even inside `sigma.core`'s **own** repo on this branch (neither its
raw `include/` nor its packaged `package/include/` has `types.h` at the nested path, only flat at
`include/types.h`). This is a real, pre-existing gap in `sigma.core`'s own packaging, not
something to fix by editing another team's repo from here — the installed copy at
`/usr/local/include/sigma.core/types.h` is `types.h`'s real, completely unmodified content, just
placed at the path `sigma.core`'s own header expects it at. Report the underlying gap back to
`sigma.core` rather than treat this as a permanent fixture — once `sigma.core`'s own packaging
puts `types.h` at this nested path itself, this manual copy step goes away.

## What's still vendored, and why

`src/sigma/{list,map,arrays,array_base,farray,collections,query}.c` remain Anvil's own vendored
copies — deliberately, not an oversight. Checked directly against `sigma.collections`' real
`list.h`/`map.h`: `sigma.collections` has grown a newer `alloc_use`-aware interface
(`FR-2603-sigma-collections-007`) that Anvil's vendored copies don't have yet. Swapping these to
`sigma.collections.o` now, before that interface parity lands, risks a real ABI mismatch, not just
a rebuild. Per direct instruction: once both (a) all memory-related vendored copies are gone
(done, this note) and (b) `sigma.collections`' interfaces are actually updated/synced, all of
these get removed too, in favor of linking `/usr/local/packages/sigma.collections.o` the same way
this note's own `sigma.system.alloc.o` link works — a real installed package, not vendored source.

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
