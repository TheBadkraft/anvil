# BR-2610-anvl-distribution-001: the published `0.8.0-rc` anvil-node and anvil-wasm tarballs predate the `import` → `include` rename

**ID:** BR-2610-anvl-distribution-001
**Type:** Bug Report (distribution / release integrity)
**Owner:** anvil-node / anvil-wasm
**Filed:** 2026-10-25
**Filed by:** flywire
**Requested by:** flywire (downstream consumer)
**Status:** open
**Related:** SR-2609-anvl-distribution-001, SR-2610-anvl-vendor-bump-001, SR-2610-anvl-distribution-001, FR-2609-anvl-public-api-002
**Tags:** distribution, release, anvil-node, anvil-wasm, versioning, include

---

## Summary

The tarballs served at `https://anvldata.com/assets/downloads/anvil-node-v0.8.0-rc-linux-x86_64.tar.gz`
and `.../anvil-wasm-v0.8.0-rc.tar.gz` contain binaries built from Anvil core *before*
FR-2609-anvl-public-api-002 renamed AML's `import` to `include`. Both still accept `import` and
treat `include` as a plain identifier. The source they're labeled with (`vendor/anvil` pinned at
`52f29bf`, after the rename) does not match what's inside them.

## Reproduction

With the published tarballs installed (`npm install` of the URLs above):

```js
anvil.parse('#!aml\ninclude "x.anvl";\nx := 1;\n')  // null, lastError: "Expected ':=' after identifier"
anvil.parse('#!aml\nimport "x.anvl";\nx := 1;\n')   // parses (anvil-node); "Imported file not found" (anvil-wasm)
```

The same `include` document against a local `anvil.node` build at `009fb4b` fails with "Included
file not found", which is the correct behavior: `include` is recognized as a keyword. Both builds
report `0.8.0+0-rc`.

## Impact

- FlyWire's schema files all start with `include "schema-gen/postgres.types.anvl";`. With the
  published tarballs, none of them parse, and most of FlyWire's test suite fails
  (flywire BR-2610-flywire-005).
- One version string covers two different grammars, so a consumer can't tell from the version
  which one it has.

## Why the SR-2609 verification missed it

SR-2609-anvl-distribution-001's post-fix verification (by both sides) parsed
`#!aml\nname := "David";\n`. That document has no `include`, so it can't tell the two grammars apart.

## Requested

1. Republish both tarballs from the current source, under a version string that differs from the
   stale build's (a release-candidate suffix bump, or the 0.9.x release SR-2610-anvl-distribution-001
   already plans), and withdraw or relabel the stale `0.8.0-rc` files.
2. Add a release check that runs before each tarball upload: parse a document containing an `include`
   directive against the *packaged* binary (not the build tree), and fail the release if the parser
   reports "Expected ':='".

## FlyWire's interim handling

FlyWire reverts to its sibling-checkout dependencies and adds a guard test, so this can't recur
silently on FlyWire's side. It re-adopts the published tarballs at the SR-2610-flywire-001 bump,
once the guard passes against them.
