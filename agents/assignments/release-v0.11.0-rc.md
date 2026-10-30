# Release Engineer: v0.11.0-rc

The Release Engineer's assignment for `v0.11.0-rc` (2026-10-29). Role and rules:
`.claude/agents/anvl-release-engineer.md`. The tooling and every check carry over from
`release-v0.10.0-rc.md`.

A minor bump, because the release changes behaviour: the number grammar
(FR-2610-anvl-parser-002) and fail-closed schema validation (BR-2610-anvl-schema-002).

## Sources (merged and pushed)

| Repo | Branch | Commit |
|---|---|---|
| anvil (native) | `main` | `92fd772` (build 314 = `git rev-list --count 92fd772`) |
| anvil.node | `main` | `cd67224` |
| anvil.wasm | `main` | `cb77760` |
| anvil.py | `main` | `0224f69` |
| anvil.net | `main` | `b7d1760` |
| anvil.java | `master` | `598116e` (not distributed) |

The release commit adds two commits on top of `92fd772` (the release tooling, then the version and changelog), so with a fast-forward merge the tag
`v0.11.0-rc` sits on a commit with BUILD = 316 (`git rev-list --count v0.11.0-rc`). A merge commit
would make it 317; BUILD is always computed from the tag as pushed, and every artifact is
rebuilt if it changes.

## Scope

1. **Native.** Branch `release/v0.11.0-rc`: version string `0.11.0-rc`; `[Unreleased]` cut into
   `[v0.11.0-rc]`; this file. The Principal Engineer merges and tags on `main`.
2. **Bindings.** Each on `release/v0.11.0-rc`: `vendor/anvil` moved to the tag commit, version
   `0.11.0-rc`, changelog cut.
3. **The WASM package ships the browser bundle** (FR-2610-anvl-wasm-002, test case 5).
   `package-wasm.sh` copies `dist/anvil.browser.js` and lists it in `files`. The release checks
   (`check-install.sh wasm`, `check-grammar.sh`, `check-readme.sh`) assert the bundle is in the tarball
   and loads in a no-`require` sandbox with the same keys as `require('anvil-wasm')`, with `ready()`
   and `getVersion()` working. A new `check-browser.sh` runs the repo's `test-browser/run-browser.sh`
   against the *extracted* package in Firefox and Chromium, glue and bundle modes.
4. **Build from the tag, reproducibly**; wasm on the Linode box under `/tmp`. Every check on the
   packaged files, including FlyWire's guard, read-only. Expected FlyWire results: `test-int32-validation`
   fails on `1e3` (FR-2610-flywire-013) and its `5.` tests change: an owner-approved grammar
   change, not a blocker. Bench node and wasm against `v0.10.0-rc`; anything above 1.02x blocks.
5. **Stage** in `~/release-staging/v0.11.0-rc/`: tarballs, `SHA256SUMS`, scrubbed
   `release-notes-v0.11.0-rc.md`, `handoff/HANDOFF-technical-writer.md`.

Not authorized in this task: pushing tags, deploying, merging.

## Status (2026-10-29)

| Step | State |
|---|---|
| 1. Cut core: `0.11.0-rc`, `[Unreleased]` -> `[v0.11.0-rc]` | done, on `release/v0.11.0-rc` |
| 2. Merge and tag on `main` | waiting for the Principal Engineer |
| 3. Bindings on `release/v0.11.0-rc` | see the release report |
| 4. Packaging, checks, bench, staging | see the release report |
| 5. Deploy and live check | after the Technical Writer's deploy |
