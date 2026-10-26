# BR-2610-anvl-version-001: the reported version isn't semver, isn't the documented string, and is `+0` in Node/WASM builds

**ID:** BR-2610-anvl-version-001
**Type:** Bug Report (versioning, release-blocking)
**Owner:** anvl (native half, ANVL coder agent) and anvil-node/anvil-wasm (build half, ANVL Bindings agent)
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the ANVL Release agent's audit (BR-2610-anvl-distribution-001, "Release audit")
**Status:** native half resolved (branch `fix/BR-2610-anvl-version-001`, off main; not merged, not pushed); binding build halves still open
**Blocks:** SR-2610-anvl-distribution-001 (the release's version check)
**Tags:** version, build, release

---

## Summary

Three related defects:

1. **Format.** `src/core/anvil.c` builds the version as `"%d.%d.%d+%d-%s"`, i.e.
   `MAJOR.MINOR.PATCH+BUILD-TAG` (`0.8.0+0-rc`). In semver, the pre-release tag comes before build
   metadata: `0.10.0-rc+176`. As written, any semver parser reads the whole `+0-rc` as build
   metadata and sees a *release* `0.8.0`, not a release candidate.
2. **Two sources of truth.** `include/constants.h` has `ANVL_VERSION_STR "0.9.0-rc"` alongside
   `MAJOR/MINOR/PATCH/TAG`. The library reports a string assembled from the parts, never
   `ANVL_VERSION_STR`, so the two can drift (and have).
3. **`BUILD` is 0 outside the core Makefile.** The core `Makefile` computes
   `BUILD_NUM := $(shell git rev-list --count HEAD)`. But `anvil.node`'s `binding.gyp`
   (`"ANVL_BUILD=0"`) and `anvil.wasm`'s `build.sh` (`-DANVL_BUILD=0`) hard-code 0, so the
   published node and wasm artifacts report `+0`. A git count is also fragile in shallow clones or
   when there's no `.git` at all (an extracted tarball).

## Expected behavior

- **Native (coder):**
  - The reported string is `MAJOR.MINOR.PATCH[-TAG][+BUILD]`, e.g. `0.10.0-rc+176`. Omit
    `+BUILD` when BUILD is 0 or not provided; that's "unknown", not a fake 0.
  - `ANVL_VERSION_STR` is derived from the parts (a macro), or removed if nothing public uses it.
  - The Makefile takes an override (`BUILD_NUM ?=`, or `make ANVL_BUILD=…`), so a release build
    can pass the tag's number explicitly and doesn't depend on the checkout's git state.
  - Update `test_version` (and any test asserting the old shape).
- **Bindings:**
  - `binding.gyp` and `build.sh` stop hard-coding 0. They take `ANVL_BUILD` from the environment,
    defaulting to `git -C vendor/anvil rev-list --count HEAD` when that works and to "not
    provided" otherwise.
  - Check the net/py/java builds report the same string for the same pin.
- **Release (no code here, just the contract):** the release script computes BUILD once, from the
  `v0.10.0-rc` tag (`git rev-list --count v0.10.0-rc`), and passes it to every artifact's build. Its
  version check then requires `0.10.0-rc+<that number>`, identical across all artifacts.

## Test cases

1. A core build with `ANVL_BUILD=176` reports `x.y.z-rc+176`; one without reports `x.y.z-rc`.
2. `ANVL_VERSION_STR` (if kept) equals the reported string minus `+BUILD`.
3. anvil.node and anvil.wasm built with `ANVL_BUILD=176` report the same string as core; built in a
   checkout with no usable git, they report no `+BUILD` rather than `+0`.
4. Full native suite and `test_release`; each binding's suite; changelog entries.

## Native half: Resolution

Branch `fix/BR-2610-anvl-version-001`, off `main` (`7d6849a`). Tests VER01-VER02 (`test/unit/test_version.c`,
which used to only print the banner and now asserts), written first and RED (the old shape failed every
format case), plus `scripts/check-version.sh` (`make check-version`). Full unit regression green,
`test_release`, `check-exports`, `check-reader-only`, `check-test-release-fresh` green, Valgrind-clean.
The version numbers themselves are NOT bumped (the Release agent sets `0.10.0-rc` at the cut): this builds as
`0.9.0-rc[+N]`.

- **Format.** `anvl_format_version` (declared in `anvil.h`, so the shape is testable with any inputs) builds
  `MAJOR.MINOR.PATCH`, then `-TAG` if the tag is non-empty, then `+BUILD` if BUILD > 0. VER01 pins
  `0.10.0-rc+176`, build 0 and negative omitted, empty and NULL tags, multi-digit parts and a dotted tag, and
  truncation (always NUL-terminated).
- **One source of truth.** `ANVL_VERSION_STR` is kept (it is a public-looking constant) but derived from
  `ANVL_VERSION_MAJOR/MINOR/PATCH/TAG` by macro. VER02 asserts it equals the reported string minus `+BUILD`.
  It requires a non-empty tag (documented at the definition); the runtime formatter itself handles a tagless
  version.
- **`ANVL_BUILD` is optional.** `constants.h` defaults it to 0, so a build that doesn't pass it (previously a
  compile error) works and reports no `+BUILD`.
- **Overrides.** The root, `test/unit`, `test/sigma` and `test/infra` Makefiles all compute
  `BUILD_NUM := $(or $(ANVL_BUILD),$(BUILD_NUM),<git rev-list --count HEAD>,0)`: an explicit `ANVL_BUILD=` or
  `BUILD_NUM=` (command line or environment) wins, else the commit count, else 0. `git ... 2>/dev/null`, so a
  tree with no `.git` falls to 0 quietly (checked with `GIT_DIR=/nonexistent`: reports `0.9.0-rc`).
  Passing `ANVL_BUILD=0` explicitly forces "unknown" regardless of git.
- **A trap found and fixed:** `-D` flags are not tracked by the `-MMD` dependency files, so after the number
  changed (a new commit, or an explicit `ANVL_BUILD=`) `anvil.o`, the only object that reads it, kept the OLD
  `+BUILD`. A stamp file (`build/.anvl_build_number`, rewritten only when the value changes) is now a
  prerequisite of `anvil.o`. `check-version` proves it: it builds in a scratch directory with 176, then 0, then
  the default, then an environment override, then `BUILD_NUM=9`, asking the real library each time.
- **Not in scope here (the ticket's other halves):** `anvil.node`'s `binding.gyp` and `anvil.wasm`'s `build.sh`
  still pass `ANVL_BUILD=0`; with this change that already reports no `+BUILD` (not `+0`), and they can
  take `ANVL_BUILD` from the environment as the ticket describes. The release script's tag-count contract
  is the Release agent's.
