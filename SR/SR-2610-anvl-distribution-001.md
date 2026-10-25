# SR-2610-anvl-distribution-001: publish `anvil-node` / `anvil-wasm` 0.9.x artifacts carrying the types and schema surface

**ID:** SR-2610-anvl-distribution-001
**Type:** Service Request (packaging / distribution)
**Owner:** anvil-node / anvil-wasm
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** flywire, rivet (downstream consumers)
**Status:** open — blocked on FR-2610-anvl-bindings-001 slice 1
**Continues:** SR-2609-anvl-distribution-001 (established the tarball-on-anvldata.com path)
**Tags:** distribution, packaging, anvil-node, anvil-wasm, flywire, rivet-readiness

---

## Summary

FlyWire pins `anvil-node-v0.8.0-rc` and `anvil-wasm-v0.8.0-rc` tarballs from anvldata.com
(`flywire/package.json`). The core is at 0.9.0-rc. When FR-2610-anvl-bindings-001 slice 1 lands,
consumers can only use it once a new tarball is published that includes it. This SR covers the
release mechanics, not the binding code.

## Scope

1. Cut `anvil-node` 0.9.x and `anvil-wasm` 0.9.x with the native types and schema modules compiled
   in. Today the wasm build may be core-only; confirm and record which modules each artifact carries.
2. Publish to `site/assets/downloads/` on anvldata.com, using the same path and naming convention as 0.8.0-rc.
3. Record the native commit each artifact was built from, in the release notes and the binding's changelog.
4. Keep 0.8.0-rc downloadable, so FlyWire's current pin doesn't break before it bumps
   (SR-2610-flywire-001).

## Test cases

1. A clean directory with no sibling checkouts: `npm install <0.9.x anvil-node tarball URL>`
   succeeds, and `require('anvil-node').Schema` is a function.
2. The same for anvil-wasm: the module loads and exposes `Schema` (or a recorded statement that
   wasm doesn't carry schema, with the reason).
3. The 0.8.0-rc URLs still resolve.
4. The release notes name the native commit for each artifact.
