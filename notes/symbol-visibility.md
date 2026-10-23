# Symbol visibility

## Status: done

## Problem

`libanvil.so` (and anything that statically links anvil's object files directly, like
anvil-node's `.node` addon) exported every global symbol by default — including the internal
`src/sigma/*` implementation (the `Math`/`List`/`Map`/`Time`/`String`/`Query`/`Source`/
`Registry`/`Collections`/`Iterator`/`Files`/`FArray`/`SparseIterator` vtable globals, and
helper functions like `collection_add`, `iter_next`, `mod_initialize`, `doc_scan_header`,
`array_base_get_element`, ...). None of that is part of Anvil's public ABI — it's internal to
how anvil's own `.c` files talk to each other.

On Linux, a process that loads two native shared objects (e.g. two different Node native
addons in the same `node`/Electron process) resolves global symbols across *all* of them by
default. Two unrelated addons defining a same-named global symbol — trivial with names this
generic — means the dynamic linker can silently bind a reference in one module to the *other*
module's definition. Found this exact way: `anvil-node`, loaded inside a VS Code extension
host alongside other extensions' native/managed components (observed specifically with C#
Dev Kit's Roslyn/ServiceHub processes active in the same process), segfaulted deterministically
at a fixed address — the bit pattern for the float `1.0` — every single time, only in that
process, never in isolation. Classic cross-addon symbol collision signature: identical fault
address regardless of ASLR, present only when specific other native code shares the process.

## Fix

- `-fvisibility=hidden` on every compiled object (Makefile's `DEBUG_CFLAGS`/`RELEASE_CFLAGS`).
- Every public header (`anvil_flat.h`, `anvil_vtable.h`, `anvil_writer.h`,
  `anvil_writer_vtable.h`, `anvil_builder.h`, `anvil_builder_vtable.h`) wraps its own
  declarations in `#pragma GCC visibility push(default)` / `pop`, so the intended public ABI
  stays exported from the final shared object exactly as before — nothing downstream (anvil.net's
  P/Invoke, anvil-node, any future binding) changes.

Verified: `nm -D lib/release/libanvil.so` now lists only the intentional `anvil_*` flat API
plus the vtable globals (`Anvil`, `Statement`, `Document`, `Error`, `Value`, `Attribute`,
`Writer`, `Builder`, ...) — 101 symbols, down from the same set *plus* ~40 internal sigma
symbols. Full `test/unit` suite (18 binaries, tens of thousands of assertions) and
`make check-reader-only` both pass unchanged — hidden visibility only affects what's visible
*outside* the final linked binary, never internal linkage within it.

## Known residual, out of scope here

`sigma.system.alloc.o` (`/usr/local/packages/sigma.system.alloc.o`) is a separately-built,
pre-compiled system package linked in as-is — not recompiled by this repo's build — and still
exports one generic global (`Allocator`). Fixing that belongs to that package's own build, not
anvil's.

## Downstream

Any binding that statically links anvil's sources directly (anvil-node's `binding.gyp`, not
just this repo's own `make so-release`) needs the matching `-fvisibility=hidden` added to
*its own* compiler flags for the fix to take effect in that binary too — the header pragma
alone only preserves the public ABI's visibility; the hiding comes from the compile flag.
