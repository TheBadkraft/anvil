# Release Engineer: v0.14.0-rc

The Release Engineer's assignment for `v0.14.0-rc` (2026-11-05). The role and its rules are in
`.claude/agents/anvl-release-engineer.md`. The tooling and every check carry over from `release-v0.13.1-rc.md`. The owner
asked for the block-form writer and builder in 0.14.0 (2026-11-02). On 2026-11-05 the owner said "commit, merge, and tag
this rc". The rexx project (copilot-rexx, BR-2611-anvl-writer-001) is waiting on it.

## What it contains

- **Native.**
  - BR-2611-anvl-writer-001: block-form writer `anvil_writer_statement_block` and builder `anvil_builder_set_block`, with
    codes `ANVIL_WRITER_ERR_BLOCK_REQUIRES_OBJECT` (21) and `ANVIL_WRITER_ERR_BASE_IS_BLOCK_FORM` (22).
  - FR-2611-anvl-errors-001: `anvil_error_get_source`, and reader dump format 2 (`source=`).
  - BR-2611-anvl-errors-001: include errors are reported at the `include` keyword.
  - BR-2611-anvl-resolver-001: **breaking.** Only a top-level statement may have a base.
    - The reader gives 4022 `ANVL_ERR_PARSER_NESTED_BASE`.
    - The writer and builder give `ANVIL_WRITER_ERR_NESTED_BASE` (23).
    - This fixes the endless walk on `s1 := { f3 : s1 { a := 1; }; };`.
  - 174 exports.
- **anvil.js.** The same changes:
  - `lastError().source`;
  - `AnvilWriter#statementBlock` and `AnvilMember#setBlock`;
  - 4022 and 23.
- **Not in this release:**
  - BR-2611-anvl-resolver-002, the `$` reference to an enclosing object. It is pre-existing and awaits an owner decision; name
    it under known issues.
  - anvil.py, anvil.net and anvil.java. Their v0.14 bumps are open (BR-2611-anvl-py-001, BR-2611-anvl-net-001,
    FR-2611-anvl-java-002), and py and net fail the full-suite release gate at their pins. Their published 0.13.1-rc downloads
    stay current, and the notes say so.
  - anvil-node and anvil-wasm stay frozen.

## Sources (merged and pushed)

| Repo | Branch | Commit |
|---|---|---|
| anvil (native) | `main` | `f3328c3` (code is `c9b3228`; later commits are docs, notes and `agents/`) |
| anvil.js | `main` | `5525047` (pins native `c9b3228`; move the pin to the tag) |

## Scope

Steps 1 to 7 are as in `release-v0.13.0-rc.md`, with these changes:

1. **Native release branch** `release/v0.14.0-rc` from `main`.
   - Set the version to `0.14.0-rc` (minor 14, patch 0).
   - Cut `[Unreleased]` into `[v0.14.0-rc]`. The breaking change leads.
   - Then **stop and report the commit**. I fast-forward main and push the tag.
2. **anvil.js pin.** On `release/v0.14.0-rc` in anvil.js, move `vendor/anvil` to the tag and regenerate `dist` and
   `BUILD_INFO`. At the pin, these must all pass:
   - reader 1920;
   - writer 851 plus 16 C-API-only (867);
   - builder 933 plus 53 C-API-only (986);
   - schema 561;
   - the full anvil.js suite (275) and the compat run (231, with the documented exclusion).

   Report the commit; I merge.
3. **Build twice.** The two builds must be byte-identical. There are two tarballs: core and anvil.js.
4. **Checks on the packaged files.** Run all of the v0.13.1-rc checks that apply to core and anvil.js, plus these against the
   installed packages:
   - `s1 := { f3 : s1 { a := 1; }; };` fails with 4022 at once, in both core and anvil.js;
   - writer code 23, and `setBase` on a nested member gives 23;
   - `statementBlock`/`setBlock` emit `name { ... };`, with codes 21 and 22;
   - `lastError().source` names the included file;
   - an include error is at the `include` keyword;
   - 174 exports;
   - the browser harness check includes the new exports.

   Docs samples (`scripts/docs/check-examples.py --strict`) run against the packaged core and anvil.js, and any failing
   sample blocks the release. py and net samples run against their 0.13.1-rc packages and are reported, not blocking.
5. **Benchmarks.** Label every figure: subject vs baseline (version), time ratio, lower is faster. Measure cold and warm.
   - **Native vs native v0.13.1-rc:** more than 1.02x blocks. The exception already accepted carries over: validate_small
     1.020x, layout, BR-2611-anvl-parser-001, backlogged.
   - **anvil.js vs anvil.js v0.13.1-rc:** report every row. All performance issues are backlogged (owner direction). A row
     above 1.15x that repeats on a rerun gets reported to me. BR-2611-anvl-js-perf-001 is already backlogged.
6. **Stage** in `~/release-staging/v0.14.0-rc/`.
   - The tarballs, per-version and cumulative `SHA256SUMS`, scrubbed release notes and the Technical Writer handoff.
   - The notes lead with the breaking change (nested bases), then the block form, `source` and the include position.
   - The notes list the known issue BR-2611-anvl-resolver-002 in plain words, with no ticket IDs.
   - The notes say py and net stay at 0.13.1-rc.
   - The notes carry over the known speed gaps.

**Not authorized in this task:** pushing tags, merging, deploying.

**Resources:**
- CPUs 2-5.
- Foreground only, every heavy command under the systemd memory cap, no background jobs.
- Browsers only through the contained harness, one run at a time.
- Nothing in `$HOME` except `~/release-staging/v0.14.0-rc/`.
- Rivet credentials go inline on every command.

## Status

| Step | State |
|---|---|
| 1. Cut core `0.14.0-rc` | |
| 2. anvil.js pin | |
| 3. Build twice | |
| 4. Checks on the packaged files | |
| 5. Benchmarks | |
| 6. Stage | |
| 7. Deploy and live check | owner go 2026-11-05 ("release and publish"); after step 6 passes |
