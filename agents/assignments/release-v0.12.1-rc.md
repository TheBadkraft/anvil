# Release Engineer: v0.12.1-rc

The Release Engineer's assignment for `v0.12.1-rc` (2026-10-30). The role and its rules are in
`.claude/agents/anvl-release-engineer.md`. The tooling and every check carry over from `release-v0.12.0-rc.md` and are
now on main. The owner set this patch release up for the C++ fixes. It also carries the anvil.js writer, which King &
Crucible is waiting on, and `lastError()` codes, which FlyWire is waiting on.

## What it contains

- **Native.**
  - `extern "C"` guards on every public header, plus the `check-cxx` gate (BR-2610-anvl-003).
  - The writer refuses an `include` after a module attribute with `ANVIL_WRITER_ERR_STATE`, and the builder emits
    includes first (BR-2610-anvl-writer-003). The grammar is unchanged; the writer now conforms to it.
  - The native writer corpus and `anvil-dump --writer-script`.
  - The AnvilScript body-parser depth limit, 5106 (BR-2610-anvl-asl-001). AnvilScript is off by default.
- **anvil.js.**
  - The writer: anvil-node's `AnvilWriter` API, byte-exact with native (749 cases match, plus 15 listed as C-API-only,
    for 764).
  - Node ESM named exports (`node/index.mjs`).
  - `lastError()` gains `code` and `name`.
  - The UTF-8 fix to the browser test harness.
- **py and net:** pins move to the new tag. They pick up the writer change through `libanvil.so`.

## Sources (merged and pushed)

| Repo | Branch | Commit |
|---|---|---|
| anvil (native) | `main` | `5b32b84` or later |
| anvil.js | `main` | `bb91bea` (pins native `c4575c4`; move the pin to the tag) |
| anvil.py | `main` | `9c89c37` |
| anvil.net | `main` | `73c721d` |

## Scope

1. **Native release branch.** Create `release/v0.12.1-rc` from main.
   - Set `ANVL_VERSION_PATCH` to 1, giving `0.12.1-rc`.
   - Cut `[Unreleased]` into `[v0.12.1-rc]`.
   - Update this file.
   - Stop, and report the commit. The Principal Engineer merges and tags.
2. **Release-tooling fix: the release blocker found in verification.**
   - `scripts/release/package-js.sh` generates `package.json`. Its `exports` must carry the `import` condition
     (`node/index.mjs`) next to `require`/`default`, and its `files` list must include `node/index.mjs`. Make the
     generated `exports` match the repo's `package.json` `exports`, or take it from there.
   - Add a check that runs `import { parse, AnvilWriter, AnvilDialect, WriterErrorCode, AnvilWriterError } from
     'anvil.js'` against the **installed** tarball. Prove it RED on the current script first.
3. **Binding pins.** py, net and anvil.js each get a `release/v0.12.1-rc` branch.
   - Move `vendor/anvil` to the tag.
   - Set the version to `0.12.1-rc`.
   - Cut the changelog.
   - For anvil.js, regenerate BUILD_INFO, `src/errors.js` and `dist/`. The reader corpus (1867), the writer corpus
     (764) and `npm run check` must all pass at the tag.
4. **Build from the tag, twice.** The two builds must be byte-identical.
5. **Checks on the packaged files.** Run all of the v0.12.0-rc checks, plus:
   - `check-cxx` against the extracted core package;
   - the anvil.js ESM named-import check;
   - the anvil.js writer, run on the K&C envelope from the installed package, in Node and through the contained
     browser harness (chromium and firefox, esm and global);
   - `lastError().code === 4001` from the installed package for a duplicate key;
   - LICENSE.txt identical in all four packages, taken from the template.
6. **Benchmarks.** Label every figure: subject vs baseline (version), time ratio, lower is faster.
   - Native vs native v0.12.0-rc: more than 1.02x blocks.
   - py and net vs v0.12.0-rc, if the tooling exists: more than 1.02x blocks.
   - anvil.js read and write vs anvil-node v0.10.0-rc: more than 1.00x blocks.
   - anvil.js vs `JSON.parse` / `JSON.stringify`: report only.
7. **Stage** in `~/release-staging/v0.12.1-rc/`.
   - The tarballs.
   - `SHA256SUMS-v0.12.1-rc`.
   - A cumulative `SHA256SUMS` that keeps every earlier line.
   - Scrubbed release notes. They list the changes above. They state the writer's documented API differences: the 15
     C-API-only cases, and that `value()` writes safe integers as integers. They note the licence terms are in
     progress.
   - The Technical Writer handoff.
   - anvil-node and anvil-wasm are not released, and their v0.11.0-rc files stay published.

**Not authorized in this task:** pushing tags, merging, deploying. The deploy needs the owner's explicit go, which the
Principal Engineer asks for once the release is staged.

**Resources:**
- CPUs 2-5.
- Foreground only, every heavy command under the systemd memory cap, no background jobs, no shell left waiting on
  stdin.
- Browsers only through the contained harness, one run at a time.
- Nothing in `$HOME` except `~/release-staging/v0.12.1-rc/`.
- Rivet credentials go inline on every command.

## Status (2026-10-30)

| Step | State |
|---|---|
| 1. Cut core: `0.12.1-rc`, `[Unreleased]` -> `[v0.12.1-rc]` | done on `release/v0.12.1-rc`, waiting for the merge and tag |
| 2. Tooling fix (package-js.sh exports) | on `release/v0.12.1-rc-tooling` |
| 3. Pins, builds, checks, benches, staging | after the tag |
