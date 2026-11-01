# Release Engineer: v0.13.0-rc

The Release Engineer's assignment for `v0.13.0-rc` (2026-10-31). The role and its rules are in
`.claude/agents/anvl-release-engineer.md`. The tooling and every check carry over from `release-v0.12.1-rc.md` and are
on main. It is a minor bump because behaviour changes: failed loads report specific IO codes, and include cycles and
the integer type follow the corrected rules. It completes anvil.js feature parity (reader, writer, builder, types,
schema), which FlyWire, orb.ux and King & Crucible are waiting on.

## What it contains

- **Native** (since v0.12.1-rc).
  - BR-2610-anvl-io-001: a root file that can't be read reports its specific IO code (1001, 1002, 1003). An `include`
    that names a directory is the include error 4204.
  - BR-2610-anvl-004: include cycles are found by normalised path (`a.anvl`, `./a.anvl` and `d/../a.anvl` are one
    document).
  - BR-2610-anvl-schema-004: `integer` rejects a decimal or exponent literal beyond double range (`1e400`).
  - BR-2610-anvl-schema-005: `TypeRegistry` `fromIncludes` merges the root's own `@[types]` last.
  - Builder: codes 19 and 20 have messages, and a missing base is refused (BR-004/005).
  - Conformance corpora: builder (915) and schema (559). No ABI change.
- **anvil.js.**
  - The builder: `AnvilBuilder`, `AnvilNode`, `AnvilMember` with anvil-node's API and native's bytes.
  - Types and schema: `TypeRegistry` and `Schema` at native parity.
  - `setIo({read(path)})` for includes in browsers.
  - The fast paths for `parseRawValue` and `validateValue`, and a cold-start eager compile.
- **py and net:** pins move to the new tag.

## Sources (merged and pushed)

| Repo | Branch | Commit |
|---|---|---|
| anvil (native) | `main` | `64a1bdd` for the library; `agents/` changes after it carry no code |
| anvil.js | `main` | `022aa4c` (pins native `64a1bdd`; move the pin to the tag) |
| anvil.py | `main` | `6d1edad` |
| anvil.net | `main` | `56f2066` |

## Scope

1. **Native release branch.** Create `release/v0.13.0-rc` from main.
   - Set the version to `0.13.0-rc` (minor 13, patch 0).
   - Cut `[Unreleased]` into `[v0.13.0-rc]`. Correct entries that are now stale (for example, the site entry that says
     "Not deployed yet" when it was deployed).
   - Update this file.
   - Stop, and report the commit. The Principal Engineer merges and tags.
2. **Binding pins.** py, net and anvil.js each get a `release/v0.13.0-rc` branch.
   - Move `vendor/anvil` to the tag.
   - Set the version to `0.13.0-rc`.
   - Cut the changelog.
   - For anvil.js:
     - regenerate BUILD_INFO, `src/errors.js` and `dist/`;
     - at the tag, all of these must pass: the reader corpus (1884), writer (764), builder (915), schema (559) and
       `npm test`;
     - initialize `vendor/anvil` in any checkout you test in (`git submodule update --init`).
3. **Build from the tag, twice.** The two builds must be byte-identical.
4. **Checks on the packaged files.** Run all of the v0.12.1-rc checks, plus these against the **installed** anvil.js
   package:
   - The named ESM import of `AnvilBuilder`, `TypeRegistry`, `Schema` and `setIo`.
   - The builder and schema corpora through the installed copy.
   - `lastError().code` of 1002 for a missing file.
   - The builder and the schema on a FlyWire-shaped document in the contained browser harness: chromium and firefox,
     esm and global, one run at a time.
5. **Benchmarks.** Label every figure: subject vs baseline (version), time ratio, lower is faster. Measure cold (fresh
   process, first call) and warm.
   - **Native vs native v0.12.1-rc:** more than 1.02x blocks. Two native results are already owner-accepted and don't
     block; report them:
     - `validate_floats` about 3% (a layout effect);
     - the `libanvil.so` small-document cost (BR-2610-anvl-parser-008).
   - **py and net vs v0.12.1-rc:** more than 1.02x blocks.
   - **anvil.js vs anvil-node v0.10.0-rc and v0.11.0-rc:** more than 1.00x blocks. The exceptions are the known gaps
     listed in the anvil.js README, "Performance notes": report those against the README figures. A gap that's worse
     than the README figure blocks.
   - **anvil.js vs anvil.js v0.12.1-rc:** more than 1.02x blocks. The exception is the warm small-document rows of
     BR-2610-anvl-js-perf-001: `amp-bin-read-5`, `amp-bin-poll-5` and `errdoc`, at about 1.33x to 1.5x. The owner
     accepted them on 2026-10-31 and backlogged them. Report those rows; any other row above 1.02x blocks.
   - **anvil.js vs `JSON.parse` / `JSON.stringify`:** report only.
6. **Stage** in `~/release-staging/v0.13.0-rc/`.
   - The tarballs.
   - `SHA256SUMS-v0.13.0-rc`.
   - A cumulative `SHA256SUMS` that keeps every earlier line.
   - Scrubbed release notes:
     - the changes above;
     - the IO code change, as a behaviour change;
     - anvil.js now complete: reader, writer, builder, types and schema;
     - its documented differences (writer: 15 C-API-only cases and safe integers; builder: 49 C-API-only cases);
     - its known performance gaps, labelled;
     - the licence terms are in progress.
   - The Technical Writer handoff.
   - anvil-node and anvil-wasm are not released, and their v0.11.0-rc files stay published.

**Not authorized in this task:** pushing tags, merging, deploying. The deploy needs the owner's explicit go, which the
Principal Engineer asks for once the release is staged.

**Resources:**
- CPUs 2-5.
- Foreground only, every heavy command under the systemd memory cap, no background jobs, no shell left waiting on
  stdin.
- Browsers only through the contained harness, one run at a time.
- Nothing in `$HOME` except `~/release-staging/v0.13.0-rc/`.
- Rivet credentials go inline on every command.

## Status (2026-10-31)

| Step | State |
|---|---|
| 1. Cut core `0.13.0-rc` | branch `release/v0.13.0-rc` cut; waiting for the merge and the tag |
| 2. Pins on `release/v0.13.0-rc` | done and merged: py `570a70d`, net `fb33a72`, anvil.js `92c159f` |
| 3. Build twice | byte-identical (4 tarballs) |
| 4. Checks on the packaged files | pass, incl. new installed-copy checks and FlyWire-shaped browser check (14 checks x chromium, firefox x esm, global) |
| 5. Benchmarks | done: native passes; anvil.js rows beyond gates accepted by the owner 2026-10-31 and backlogged (BR-2610-anvl-js-004, BR-2610-anvl-js-perf-001) |
| 6. Stage | done in `~/release-staging/v0.13.0-rc/` (notes, handoff, SHA256SUMS); nothing deployed |
| 7. Deploy and live check | after the owner's go and the Technical Writer's deploy |
