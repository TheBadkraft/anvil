# Release Engineer: v0.12.0-rc

The Release Engineer's assignment for `v0.12.0-rc` (2026-10-30). Role and rules:
`.claude/agents/anvl-release-engineer.md`. The tooling and every check carry over from
`release-v0.11.0-rc.md`, except where this file says otherwise. The owner approved the release and its deploy on
2026-10-30.

It is a minor bump because behaviour changes. The grammar is now consistent (BR-2610-anvl-parser-005):
- duplicate keys are errors everywhere;
- a base that isn't an object is an error;
- keywords are refused as names;
- errors report specific codes.

Three further changes come from BR-2610-anvl-parser-007, BR-2610-anvl-writer-002 and BR-2610-anvl-resolver-001:
- attribute values are single scalars only;
- the builder refuses a non-object base;
- `$ident` resolves to a block-form statement.

This is also the **first release of anvil.js**.

## Sources (merged and pushed)

| Repo | Branch | Commit |
|---|---|---|
| anvil (native) | `main` | `c8d821a` (`agents/` only after `acb4602`) |
| anvil.js | `main` | `bafb3c9` (pins native `acb4602`) |
| anvil.py | `main` | `0e5acb3` |
| anvil.net | `main` | `4566150` |
| anvil.java | `master` | not distributed |
| anvil.node, anvil.wasm | frozen | **not released**. Their v0.11.0-rc tarballs stay published, and are never removed. |

## Scope

1. **Native.** On branch `release/v0.12.0-rc`:
   - set the version string to `0.12.0-rc`;
   - cut `[Unreleased]` into `[v0.12.0-rc]`;
   - update this file.

   The Principal Engineer merges and tags on `main`. Compute BUILD from the tag as pushed.
2. **Bindings: py and net.** Each on its own `release/v0.12.0-rc` branch:
   - move `vendor/anvil` to the tag commit;
   - set the version to `0.12.0-rc`;
   - cut the changelog.
3. **anvil.js.** On `release/v0.12.0-rc` in the private anvil.js repo:
   - Move `vendor/anvil` to the tag commit. Its conformance must stay at 1867/1867; any difference blocks the release.
   - Set the package version to `0.12.0-rc`. `getVersion()` must report `0.12.0-rc+<BUILD>`, the same BUILD as native.
   - Regenerate `BUILD_INFO.json`, keeping `implementation: 'anvil.js'`.
   - Cut its CHANGELOG.
   - Write new tooling, `scripts/release/package-js.sh`, which produces `anvil.js-v0.12.0-rc.tar.gz`. The tarball holds:
     - `package.json` with the name `anvil.js`;
     - `src/`, `node/` and `dist/anvil.mjs` + `dist/anvil.global.js`;
     - `BUILD_INFO.json`, the README and the licence.

     It must not contain tests, bench, vendor or scripts.
4. **Build from the tag, reproducibly.** Build twice; the results must be byte-identical.
5. **Run every check against the packaged files:**
   - grammar and version (`0.12.0-rc+<BUILD>` everywhere);
   - exports, README and install for core, py and net. For anvil.js, install from the extracted tarball into a scratch project. `require('anvil.js')` and `import` both work, `implementation === 'anvil.js'`, and the conformance corpus passes through the installed copy.
   - browser: test the anvil.js `dist/` builds from the **extracted** tarball, using only the contained harness (`anvil.wasm/test-browser/` via `contain.sh`). Run chromium and firefox, esm and global.
   - FlyWire's guard, read-only. Expected failures, which come from the owner-approved grammar change and do not block:
     - FlyWire's repeated-key tests (`test-validation-errors`, `test-repeated-keys`, `test-anvil-schema-crosscheck`, `test-validation-routing`) now get 4001;
     - `test-int32-validation` still fails on `1e3` (FR-2610-flywire-013), as it did at v0.11.0-rc.

     Report any other difference.
6. **Benchmarks.** Label every figure as subject vs baseline (version), time ratio, lower is faster (STANDARDS §8).
   - native vs native v0.11.0-rc: >1.02x blocks;
   - py and net vs their v0.11.0-rc, if the tooling exists: >1.02x blocks;
   - anvil.js (packaged) vs anvil-node v0.10.0-rc: >1.00x blocks;
   - anvil.js vs `JSON.parse`: report only.
7. **Stage** in `~/release-staging/v0.12.0-rc/`:
   - the tarballs;
   - a per-version `SHA256SUMS-v0.12.0-rc`;
   - the cumulative `SHA256SUMS`, which lists every published tarball, v0.10.0-rc and v0.11.0-rc included;
   - scrubbed `release-notes-v0.12.0-rc.md`. These state the behaviour changes, introduce anvil.js as the replacement for anvil-node and anvil-wasm, and say they stay downloadable at v0.11.0-rc;
   - `handoff/HANDOFF-technical-writer.md`.

**Not authorized in this task:** pushing tags, deploying, and merging. The Principal Engineer does those.

Resource rules: CPUs 2-5, foreground, every heavy command under the systemd memory cap. Browsers only through
the contained harness. Nothing in `$HOME` except `~/release-staging/v0.12.0-rc/`.

## Status (2026-10-30)

| Step | State |
|---|---|
| 1. Cut core: `0.12.0-rc`, `[Unreleased]` -> `[v0.12.0-rc]` | done on `release/v0.12.0-rc`, waiting for the merge and tag |
| 2. Bindings (py, net) and anvil.js on `release/v0.12.0-rc` | after the tag |
| 3. Build, checks, bench, stage | after the tag |
