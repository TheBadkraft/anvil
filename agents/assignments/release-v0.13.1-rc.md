# Release Engineer: v0.13.1-rc

The Release Engineer's assignment for `v0.13.1-rc` (2026-10-31). The role and its rules are in
`.claude/agents/anvl-release-engineer.md`. The tooling and every check carry over from `release-v0.13.0-rc.md`. The owner
approved this patch on 2026-10-31 ("apply the 3 fixes under a patch"). King & Crucible is waiting to move straight from
0.12.1-rc to this release.

## What it contains

- **Native.**
  - BR-2610-anvl-errors-002: error name strings equal the `errors.h` identifiers. 1001-1003 are now `ANVL_ERR_IO_FILE_READ`,
    `ANVL_ERR_IO_FILE_NOT_FOUND` and `ANVL_ERR_IO_INVALID_PATH`. **Behaviour change** for anything matching name strings.
  - BR-2610-anvl-schema-006: `integer := true` decides whole-ness from the literal's text. `1e-999` and
    `1.0000000000000001` now fail; `1e3` and `1.0` are unchanged. **Behaviour change.**
  - FR-2610-anvl-schema-009: `ANVIL_SCHEMA_ERR_ATTRIBUTE_MISSING` (11) and `ANVIL_TYPE_ERR_ATTRIBUTE_MISSING` (1), through the
    additive `anvil_schema_load_ex` and `anvil_type_registry_load_ex`, plus their name and message functions. 171 exports.
- **anvil.js.** The same three fixes. `SchemaErrorCode` and `TypeErrorCode` are exported. After a null `Schema.load*`,
  `lastError()` gives code 11, and after a null `TypeRegistry.load*` it gives code 1.
- **py and net:** pins move to the new tag.

## Sources (merged and pushed)

| Repo | Branch | Commit |
|---|---|---|
| anvil (native) | `main` | `c3fc1d6` or later; `agents/` changes after it carry no code |
| anvil.js | `main` | `e0bf95c` (pins native `bb509e3`; move the pin to the tag) |
| anvil.py | `main` | `570a70d` |
| anvil.net | `main` | `fb33a72` |

## Scope

Steps 1 to 7 are as in `release-v0.13.0-rc.md`, with these changes:

1. **Native release branch.** Set the version to `0.13.1-rc` (patch 1). Cut `[Unreleased]` into `[v0.13.1-rc]`. Then stop and
   report the commit; I merge fast-forward and tag.
2. **Binding pins.** At the tag, these must all pass: reader 1884, writer 764, builder 915, schema 561, `npm test`
   (234) and `npm run compat` (231, with the one documented exclusion).
3. **Build twice.** The two builds must be byte-identical.
4. **Checks on the packaged files.** Run all of the v0.13.0-rc checks, plus these against the installed packages:
   - `lastError()` from anvil.js after `Schema.load` of a document without `@[schema]`: code 11,
     `ANVIL_SCHEMA_ERR_ATTRIBUTE_MISSING`;
   - code 1 from `TypeRegistry.load` of a document without `@[types]`;
   - `lastError().name` is `ANVL_ERR_IO_FILE_NOT_FOUND` for a missing file;
   - `integer` rejects `1e-999`;
   - the 6 new native symbols are exported (171 in all);
   - the browser harness check includes the new exports.
5. **Benchmarks.** Label every figure: subject vs baseline (version), time ratio, lower is faster. Measure cold and warm.
   - **Native vs native v0.13.0-rc:** more than 1.02x blocks. The exceptions already accepted carry over.
   - **anvil.js vs anvil.js v0.13.0-rc:** report every row. The owner directed on 2026-10-31 that all performance issues are
     backlogged, and the verifier measured the patch's `validateValue` cost (cold call 1, 1.04x to 1.08x). A row above 1.15x
     that repeats on a rerun blocks; report it to me.
   - **anvil.js vs anvil-node:** report only, against the v0.13.0-rc release notes' figures.
6. **Stage** in `~/release-staging/v0.13.1-rc/`.
   - The tarballs, per-version and cumulative `SHA256SUMS`, scrubbed release notes and the Technical Writer handoff.
   - The notes lead with the two behaviour changes: the name strings, and `integer`.
   - The notes carry over the known speed gaps, and add this patch's labelled figures.

**Not authorized in this task:** pushing tags, merging, deploying.

**Resources:**
- CPUs 2-5.
- Foreground only, every heavy command under the systemd memory cap, no background jobs.
- Browsers only through the contained harness, one run at a time.
- Nothing in `$HOME` except `~/release-staging/v0.13.1-rc/`.
- Rivet credentials go inline on every command.

## Status (2026-10-31)

| Step | State |
|---|---|
| 1. Cut core `0.13.1-rc` | |
| 2. Pins on `release/v0.13.1-rc` | |
| 3. Build twice | |
| 4. Checks on the packaged files | |
| 5. Benchmarks | |
| 6. Stage | |
| 7. Deploy and live check | after the owner's go and the Technical Writer's deploy |
