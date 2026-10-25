# SR-2610-anvl-vendor-bump-001: bump `vendor/anvil` in `anvil.node` and `anvil.wasm` to current `main`

**ID:** SR-2610-anvl-vendor-bump-001
**Type:** Service Request (dependency maintenance)
**Owner:** anvil-node / anvil-wasm
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet, flywire (downstream consumers, indirectly)
**Status:** open
**Blocks:** FR-2610-anvl-bindings-001 (beyond slice 1's current surface), FR-2610-anvl-bindings-002,
SR-2610-anvl-distribution-001, and every native BR/FR in the rivet-readiness set reaching Node
**Tags:** bindings, anvil-node, anvil-wasm, vendor, sigma, rivet-readiness

---

## Summary

Both bindings pin `vendor/anvil` at `52f29bf` (`v0.8.0-rc-36`), 45 commits behind `main`. That pin
predates the streaming writer/builder, `anvil_value_get_numeric`, the blob-tag accessor,
internal-symbol hiding, and the `sigma.system.alloc` switch (`c807871`). No native fix in the
rivet-readiness set can reach a Node or browser consumer until this bump happens.
`FR-2609-anvl-writer-001` already records the writer wrappers for both bindings as blocked on
"the deferred `sigma.system.alloc` vendor bump". This SR is that bump.

## Two different problems

**`anvil.node`: mechanical.** Its build compiles the vendored sources natively, so it can
link the system-installed `/usr/local/packages/sigma.system.alloc.o` and include
`/usr/local/include` exactly as core's own `Makefile` does (`notes/sigma-system-alloc-dependency.md`).
Steps: bump the submodule, update `binding.gyp` sources (drop `src/sigma/memory.c`, add the
system include path and package object, add `src/writer/*` if the writer is wanted), rebuild,
and keep `-fvisibility=hidden` with no exported anvil/sigma symbols.

**`anvil.wasm`: needs a real decision.** `build.sh` compiles every vendored `.c` with `emcc`,
including `vendor/anvil/src/sigma/memory.c`, which `main` deleted. The replacement,
`sigma.system.alloc.o`, is a *native* x86-64 object, and Emscripten can't link it. Options:

1. Compile `sigma.system.alloc` from its source with `emcc` on the Linode build box (needs that
   source and the `sigma.core` headers there). This is the recommended option, because it
   matches the "not vendored" rule.
2. Add a wasm-only build of the allocator in `sigma.system.alloc`'s own packaging, producing a
   `.wasm`-target object under `/usr/local/packages/` on the build box.

Either way, also decide whether wasm now carries `src/anvil_types.c` and `src/schema/schema.c`.
Today it doesn't: `build.sh` lists core and sigma only. Record the decision, because
SR-2610-anvl-distribution-001 asks for it.

## Test cases

1. `anvil.node`: `git submodule status` shows `vendor/anvil` at a `main` commit at or after
   `c807871`; the existing test suite is green; `nm -D` on the addon shows no `anvl_*`/`sigma`
   symbols.
2. `anvil.node`: `anvil_value_get_numeric` and `anvil_writer_new` link (proven by a trivial test
   calling each through the binding's native layer).
3. `anvil.wasm`: builds on the Linode box against the bumped pin; the existing suite is green; the
   large-input regression (BR-2609-anvl-005) still passes through 5MB.
4. `anvil.wasm`: the README states which native modules the artifact carries (core / types /
   schema / writer).

## Notes

This is a pin bump, not new binding surface. New surface (types/schema, writer) is
FR-2610-anvl-bindings-001/002.
