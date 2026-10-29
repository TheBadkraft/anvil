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

---

## Release audit (2026-10-26, ANVL Release agent)

All five published `0.8.0-rc` tarballs were downloaded from anvldata.com, confirmed byte-identical to
`site/assets/downloads/`, extracted, and probed with `scripts/release/audit-published.sh 0.8.0-rc`
(`check-grammar.sh` per artifact: reported version, `include "missing.anvl";`, `name := "David";`).

| Artifact | SHA256 (first 12) | Reported version | `include` | `name := "David"` |
|---|---|---|---|---|
| core `anvil-v0.8.0-rc-linux-x86_64` | `cc81cd9c533f` | `0.8.0+89-rc` | **FAIL** `Expected ':=' after identifier` | ok |
| node `anvil-node-v0.8.0-rc-linux-x86_64` | `087bd0ee3860` | `0.8.0+0-rc` | **FAIL** `Expected ':=' after identifier` | ok |
| wasm `anvil-wasm-v0.8.0-rc` | `2f0ea3b17313` | `0.8.0+0-rc` | **FAIL** `Expected ':=' after identifier` | ok |
| net `anvil-net-v0.8.0-rc-linux-x64` | `47be922099f1` | `0.8.0+127-rc` | ok (`Included file not found`) | ok |
| py `anvil-py-v0.8.0-rc-linux-x64` | `1f5f829d810` | `0.8.0+135-rc` | ok (`Included file not found`) | ok |

Findings:

1. **Three of five artifacts are stale: core, node and wasm.** They predate the `import` → `include`
   rename. The core tarball (Sep 9) was suspected stale and is. `anvil-net` and `anvil-py` are good.
2. **The reported version cannot tell the builds apart.** The native library reports
   `MAJOR.MINOR.PATCH+BUILD-TAG`, where `BUILD` is `git rev-list --count HEAD` injected at compile
   time (`Makefile`, `BUILD_NUM`). node and wasm report `+0`: they were built where no git history
   was visible (the vendored submodule), so their string identifies nothing. All five carry the same
   `0.8.0-rc` in their filename and `package.json`.
3. `ANVL_VERSION_STR` in `include/constants.h` is not what the library reports; the reported string
   is assembled from `ANVL_VERSION_MAJOR/MINOR/PATCH/TAG` and `ANVL_BUILD`. The release script keeps
   them in step.

Release-check consequences (implemented in `scripts/release/check-grammar.sh`): the version check
requires `X.Y.Z+BUILD-tag` with `BUILD > 0` and, for a release, `BUILD` equal to the commit count of
the release tag. A build with `+0` fails the release.

RED/GREEN evidence: against the five published files the check fails on core, node and wasm (include
grammar) and on node and wasm (version `+0`). Positive control: the same check passes against a core
package assembled from current `main`.
