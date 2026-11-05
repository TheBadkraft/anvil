# Release Engineer: v0.14.1-rc

The Release Engineer's assignment for `v0.14.1-rc` (2026-11-05). The role and its rules are in
`.claude/agents/anvl-release-engineer.md`. The tooling and every check carry over from `release-v0.14.0-rc.md`; the
v0.14-specific checks (`scripts/release/check-v014.sh`) still apply.

Owner, 2026-11-05: "ship as is ... and backlog". The anvil.js speed cost on documents with references ships backlogged.
The older crash and stall bugs (deep nesting, long inheritance chains, quadratic chain resolution) are not in this
release.

## What it contains

- **Native** (behaviour change):
  - BR-2611-anvl-resolver-002: a `$` reference that leads back to a value that contains it is resolver error 4054
    `ANVL_ERR_RESOLVER_REFERENCE_CYCLE`. The builder refuses the same documents with writer code 24
    `ANVIL_WRITER_ERR_REFERENCE_CYCLE`. A plain chain cycle `a := $b; b := $a;` still resolves to null.
  - 174 exports, unchanged.
- **anvil.js:** the same rule and codes (BR-2611-anvl-js-v014-002).
- **anvil.py** (BR-2611-anvl-py-001) and **anvil.net** (BR-2611-anvl-net-001): first 0.14 releases of both.
  - Their 4 failing tests are fixed.
  - They mirror v0.14: error source, block form, writer codes 17 to 23, and 4022.
  - Their pins move from v0.14.0-rc to this tag. Their mirrors of 4054 and code 24 must be added or confirmed at the new
    pin, and their full suites must pass there. If a mirror is missing, stop and report it to me; do not add it yourself.
- **Not in this release:**
  - anvil.java (never published).
  - anvil-node and anvil-wasm (frozen).

## Sources (merged and pushed)

| Repo | Branch | Commit |
|---|---|---|
| anvil (native) | `main` | `d98fca5` or later (code at `cb79b8e`) |
| anvil.js | `main` | `1498320` (pins native `cb79b8e`; move the pin to the tag) |
| anvil.py | `main` | `21b4abb` (pins v0.14.0-rc) |
| anvil.net | `main` | `47841e9` (pins v0.14.0-rc) |

## Scope

Steps 1 to 7 are as in `release-v0.14.0-rc.md`, with these changes:

1. **Native release branch** `release/v0.14.1-rc` from `main`.
   - Set the version to `0.14.1-rc` (patch 1).
   - Cut `[Unreleased]` into `[v0.14.1-rc]`, with the 4054 behaviour change leading.
   - Then stop and report the commit. I merge it and push the tag.
2. **Binding pins** on `release/v0.14.1-rc` in anvil.js, anvil.py and anvil.net, moving each to the tag.
   - At the pin, these must pass:
     - anvil.js: every corpus at native's counts, the full suite and the compat run.
     - anvil.py and anvil.net: their **full** suites, including drift.
   - Report each commit; I merge fast-forward.
3. **Build twice:** byte-identical. Four tarballs: core, anvil.js, py, net.
4. **Checks on the packaged files:** every v0.14.0-rc check, plus:
   - 4054 at the documented position in core and anvil.js;
   - builder code 24;
   - `s1 := { a := $s1; };` refused at once;
   - the corpus walker (`check-walk-corpus`) against the packaged core.

   The py and net probes run against their new packages. Docs samples (`check-examples --strict`) run against all four
   packages, and any failing sample blocks.
5. **Benchmarks** (labelled; subject vs baseline v0.14.0-rc):
   - Native: more than 1.02x on a no-reference workload blocks. Reference-dense rows (1.07x to 1.17x) are backlogged
     (FR-2611-anvl-resolver-001); report them.
   - anvil.js: report every row. Reference-bearing rows (about 1.5x to 2x) are backlogged (BR-2611-anvl-js-perf-003).
6. **Stage** in `~/release-staging/v0.14.1-rc/`.
   - The notes lead with the 4054 behaviour change.
   - Python and .NET move to 0.14.1-rc, so they now include the v0.14 changes, the nested-base rule among them.
   - Known speed gaps are stated plainly: reference-dense documents are slower, about 1.1x in native and up to 2x in
     anvil.js.
   - Known issues, in plain words: very deep nesting can crash the parser; long inheritance chains load slowly.
7. **Deploy:** the owner's go is "ship as is" (2026-11-05). The Technical Writer deploys after step 6 passes.

**Not authorized in this task:** pushing tags, merging, deploying.

**Resources:**
- CPUs 2-5.
- Foreground only, every heavy command under the systemd memory cap.
- Browsers only through the contained harness, one run at a time.
- Nothing in `$HOME` except `~/release-staging/v0.14.1-rc/`.
- No Rivet writes.

## Status

| Step | State |
|---|---|
| 1. Cut core `0.14.1-rc` | |
| 2. Binding pins | |
| 3. Build twice | |
| 4. Checks on the packaged files | |
| 5. Benchmarks | |
| 6. Stage | |
| 7. Deploy and live check | done 2026-11-05 on the owner's explicit go: site 0c7b7ee, Worker version 01d8fa6d; live sha256 x4 and SHA256SUMS identical to staging, 0.14.1-rc+585, docs samples 127/0 against the live files, all 41 earlier downloads 200, verify-staged 69 PASS; live browser rerun pending (harness install: playwright-core 1.65.0 not on npm; staged identical bytes passed both browsers) |
