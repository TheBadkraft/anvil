# FR-2609-anvl-public-api-002: rename AML's `import` to `include`; reserve `import` for AnvilScript

**ID:** FR-2609-anvl-public-api-002
**Type:** Feature Request (naming / public API)
**Owner:** anvl
**Filed:** 2026-09-19
**Filed by:** anvil
**Status:** resolved — renamed across core, tests, docs, notes, the AnvilScript design corpus, and
`anvil.node`'s two comment-only mentions
**Continues:** [`FR-2609-anvl-public-api-001`](FR-2609-anvl-public-api-001.md) — directly renames
the `anvil_document_get_imports`/`anvil_type_registry_load_from_imports` work introduced there.
**Tags:** naming, public-api, anvilscript, keyword-rename

---

## Summary

AML's `import "path.anvl";` (whole-document composition — DAG resolution, cycle detection,
diamond dedup, forbidden in AMP) is renamed to `include`, freeing the word `import` for
AnvilScript's (ASL, still design-only, not implemented) foreign-code-bridge construct, previously
named `using`.

## Motivation

Raised directly: AML's `import` was landing awkwardly once AnvilScript's design put a
general-purpose-language framing next to it. Cross-industry, `import` is the word people already
read as "pull in any module, possibly foreign" (Python importing C extensions, JS importing
WASM) — exactly ASL's own foreign-bridge job, previously spelled `using`, which reads more like
C#'s same-language namespace directive (plus an unrelated resource-disposal meaning). Checked
against AML's real sibling languages first: AML's own `import` mechanic — whole-file composition
into a schema/definition graph — is not itself unconventional; it's precisely what protobuf calls
`import` and Thrift calls `include` for the same job. Given `import` was unavailable to ASL (AML
already owned it), the fix was to give AML's mechanism the Thrift-precedented name (`include`) and
let ASL's foreign-bridge construct take the word everyone already expects there (`import`).

Worth doing now specifically: ANVL has effectively zero external footprint today (in-house
tooling only), so this was as cheap as this rename will ever be — and it resolves a genuine,
independently-discovered ambiguity in the AnvilScript design docs, which previously described
ASL's `import` as *literally reusing* AML's `import` mechanism verbatim. Under the new naming the
two are unambiguously distinct constructs by construction.

## Scope

**Core (`anvil` repo):** keyword constant (`ANVL_KEYWORD_IMPORT`→`ANVL_KEYWORD_INCLUDE`),
reserved-word wiring, the header-scan/include-loader implementation (`src/core/document.c`,
`src/core/module.c`), struct/type names (`anvl_import_t`→`anvl_include_t`), error codes
(`ANVL_ERR_IMPORT_*`→`ANVL_ERR_INCLUDE_*`, same numeric values), the public C API
(`anvil_document_get_imports`→`anvil_document_get_includes`,
`anvil_type_registry_load_from_imports`→`anvil_type_registry_load_from_includes`, vtable field
renamed to match), the full test suite (fixture files/directories renamed, test function names +
registered IDs renamed, two tests re-targeted at `include`/`import` respectively where their
intent was specifically about AML's real mechanism vs. ASL's reserved-ahead-of-feature one), and
every doc/notes file describing the mechanism (`docs/`, `notes/`, the `site/` manual doc mirror —
confirmed not auto-synced from `docs/`).

**Keyword reservation (the one place this deviates from a pure 1:1 swap):** this codebase already
has an established policy of reserving a keyword ahead of an unimplemented feature landing (see
`include/constants.h`'s own comment — `vars`/`using` were reserved ahead of AnvlScript before this
change). Applied consistently here: `import` is now reserved ahead of ASL's own foreign-bridge
grammar (not yet implemented — nothing consumes it beyond the reserved-word check), and `using` is
fully freed, matching the same "reserved ahead of the feature, so a name chosen today doesn't
silently break later" reasoning `using` itself used to embody. This is not a functional change
(neither word has any grammar production today) — it's the naming-consistency half of the same
decision.

**AnvilScript design docs (design-only, not implemented):** `notes/anvilscript-design.md`,
`notes/anvilscript-theoretical-sketches.md`, `notes/anvilscript-digest.md`,
`docs/dialect-ownership-matrix.md` — all reconciled to the new scheme (AML owns `include`; ASL
owns `import`, renamed from `using`), including the one genuinely delicate rewrite: the full
`using`-declarations design-thread section in `anvilscript-design.md`, which previously described
ASL's construct as reusing AML's mechanism verbatim and now describes two deliberately distinct
constructs.

**Explicitly out of scope:** `anvil.wasm` (zero mentions of `import` anywhere in that repo,
confirmed — no action needed) and `anvil.net` (a from-scratch C# reimplementation, not a wrapper
over this library — it has its own substantial, independently-built public surface around
`import`: `AnvilContext.ImportDecls`, `AnvilImportDecl`, 7 `AnvilErrorCode.Import*` values, and an
`as alias` feature this repo's `import`/`include` never had). A parallel rename there — if wanted,
for cross-ecosystem naming consistency — is a real, separate decision for a different codebase and
different consumers, not undertaken here. Flagged as a recommended future follow-up, not
committed.

`anvil.node`'s two comment-only mentions (`src/binding.c`, `README.md` — neither exposes an
import-graph concept to its own JS consumers) were reworded to match for accuracy; no functional
or public-surface change there.

## Verification

- Core library builds clean (`make lib-debug`), zero warnings.
- Full existing test suite (all 14 `test/unit/` binaries) rebuilt and re-run: 100% pass, full
  Valgrind clean-run (zero leaks/errors) across every suite.
- A genuine, pre-existing latent bug was found and fixed in the process: `test_header.c`'s HDR01
  used fixed-size buffers (`act_decl[13]`, `act_path[7]`) sized to their expected content length
  with no headroom for `Source.substring`'s own null terminator (`out_buffer[len] = '\0'`) — an
  always-present one-byte overflow that happened to stay silent with the shorter `import` keyword
  and became a real, detected stack-smashing crash once the equivalent content grew by one
  character to `include`. Fixed with correctly-sized buffers (`[16]`/`[8]`), unrelated to this
  rename's own scope but directly surfaced by it.
- Repo-wide case-insensitive grep swept for stray `import` mentions across `src/`, `include/`,
  `test/`, `docs/`, `notes/`, `BR/`, `FR/`, `site/` — remaining hits are all confirmed intentional:
  ASL's own reserved-ahead-of-feature `import` (design docs and the `ANVL_ERR_IMPORT_*`/
  `ANVL_STMT_IMPORT` scaffolding), real false positives (`src/sigma/farray.c`'s "important",
  `FR-2603-sigma-collections-006.md`'s "importantly", `BR-2609-anvl-004.md`'s C# `DllImport`,
  `site/scripts/build-site.js`'s genuine JS ES-module `import` statements), `test_source.c`'s
  generic string-matching-primitive tests (deliberately left alone — they exercise `match_length`/
  `slice_equals` generically, not AML's include semantics specifically, and `import` remains a
  real reserved word either way), `test/fixtures/f35_full.anvl` (already independently flagged
  stale/orphaned before this change, deliberately left untouched), and `anvil.net` (out of scope,
  see above).
- Historical `docs/changelog.md` entries (`[v0.7.0-alpha]`, `[v0.3.0-alpha]`, `[v0.1.1-alpha]`)
  deliberately left untouched — accurate point-in-time records of what those versions actually
  shipped, under their real names at the time. Only the current/unreleased section was updated to
  track today's real symbol names.

## Follow-up — `anvil.node`/`anvil.wasm` vendored-source bumps

Both bindings compile Anvil Native from source via a pinned `vendor/anvil` git submodule (no
released/versioned library to depend on yet), so this rename didn't reach either one automatically.
Scoped directly before touching anything (read-only pass per repo): neither repo's own test suite,
fixtures, or embedded ANVL string literals use `import`/`include` syntax at all — only the
vendored submodule pin itself was stale (both pinned to `06f2d0f8`, 9 commits behind this rename).

Fixed identically in both, matching each repo's own established "bump vendored anvil" commit
convention (five prior examples in `anvil-node` alone):
- **`anvil-node`**: submodule bumped to `52f29bf`, rebuilt via `node-gyp rebuild`, full suite
  re-run — 34/34 pass. Commit `97d1ad9`.
- **`anvil-wasm`**: submodule bumped to `52f29bf`, rebuilt via Emscripten on the Linode build host
  (the only place `emcc` is available), full suite re-run there directly — 37/37 pass. Commit
  `1422659`.

No test or fixture content needed rewriting in either repo — this was a pure vendored-source
refresh, not a consumer-code change.

## Resolution

Renamed across every scope listed above; full regression green and Valgrind-clean, in Anvil Native
itself and in both bindings that vendor it. Historical records preserved. A parallel `anvil.net`
rename is flagged as a recommended, separate follow-up decision — not committed to here.
