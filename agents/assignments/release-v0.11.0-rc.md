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
| 1. Cut core: `0.11.0-rc`, `[Unreleased]` -> `[v0.11.0-rc]` | done; tag `v0.11.0-rc` on `8025289`, BUILD 316, pushed |
| 2. Bindings on `release/v0.11.0-rc`, pinned to `8025289` | done, waiting for the merge: node `753d613`, wasm `ffc7ade` (both re-cut on the float-kind fixes), py `0e5acb3`, net `4566150` (java: no pin, not distributed) |
| 3. Build from the tag, reproducibly | done; core, py and net unchanged; node and wasm rebuilt twice, byte-identical |
| 4. Non-browser checks on the packaged files | pass: grammar and version `0.11.0-rc+316`, exports, README, install, surface, bundle sandbox, FlyWire guard 3/3 |
| 5. Real-browser check of the packaged wasm tarball (contained harness) | pass: chromium and firefox, glue and bundle, 8 checks each |
| 6. Bench node and wasm vs `v0.10.0-rc` | pass: worst workload node 1.013x, wasm 1.016x; validate faster |
| 7. Stage | done in `~/release-staging/v0.11.0-rc/`, no hold |
| 8. Deploy and live check | after the Technical Writer's deploy |

The release tooling fixes (`check-browser.sh` through the contained harness) are on `release/v0.11.0-rc-tooling`.

Staged SHA256s (also in `SHA256SUMS`):

```
fee2cc4abb5fb72e162735ba72edeef450610b650dec0b02c11d3548c27c07f6  anvil-net-v0.11.0-rc-linux-x64.tar.gz
2cf2f5120318d2c0307c378c4a951f89107358e5c932cdb3bfbf511fc45f24f5  anvil-node-v0.11.0-rc-linux-x86_64.tar.gz
0c5a14bcd117d15fef9bd0a5dd00467fac9786e69d7c0999edcf5d0eec89a952  anvil-py-v0.11.0-rc-linux-x64.tar.gz
1d8ae18e21f150b979e751a0d43cfe4d88931c5a5a79fe0c8e1de0ec35874794  anvil-v0.11.0-rc-linux-x86_64.tar.gz
17ca4e8c3433178b86e2d08bf215a914564f5c207691c59cbc4378e0c0498339  anvil-wasm-v0.11.0-rc.tar.gz
```
