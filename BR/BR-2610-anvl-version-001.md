# BR-2610-anvl-version-001: the reported version isn't semver, isn't the documented string, and is `+0` in Node/WASM builds

**ID:** BR-2610-anvl-version-001
**Type:** Bug Report (versioning, release-blocking)
**Owner:** anvl (native half, ANVL coder agent) and anvil-node/anvil-wasm (build half, ANVL Bindings agent)
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the ANVL Release agent's audit (BR-2610-anvl-distribution-001, "Release audit")
**Status:** resolved: native half merged (`ab42e73`); binding build halves done on branches, awaiting merge
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

## Binding build half: Resolution

Done by the ANVL Bindings agent. Branches, not merged, not pushed.

| Binding | What changed | Branch / commits |
|---|---|---|
| anvil.node | `binding.gyp` takes `ANVL_BUILD` from `scripts/build-number.js` (`<!(node scripts/build-number.js)`), no longer `0` | `fix/BR-2610-anvl-version-001-build`: `245f125` (code + test), `a908fae` and `ffd05ff` (README) |
| anvil.wasm | `build.sh` takes `-DANVL_BUILD` from `scripts/build-number.sh`, no longer `0`; `dist/` rebuilt on the Linode box | `fix/BR-2610-anvl-version-001-build`: `70afa78` (code + test), `da57013` (README) |
| anvil.py, anvil.net, anvil.java | No build-side change: they build through native's own Makefile, which already takes `BUILD_NUM`/`ANVL_BUILD` (`ab42e73`) | READMEs only: py `79190fd`, net `add20db` + `2e364f6`, java `0695070` (on the re-pin branches) |

**The rule** (the same in `build-number.js` and `build-number.sh`; 9 tests each, `test/build-number.test.js`):

1. `ANVIL_BUILD`, when set and non-empty, wins. It must be a plain non-negative integer that fits a C `int`; anything
   else fails the build with a message, rather than reporting a made-up number. Leading zeros are dropped (a C literal
   `0176` is octal). `ANVIL_BUILD=0` forces "not provided".
2. Otherwise the commit count of `vendor/anvil` (`git rev-list --count HEAD`), but only when `vendor/anvil` is a full
   git checkout **of its own**: a `vendor/anvil` with no `.git` that sits inside some other repository would otherwise
   report that repository's commit count, and a shallow clone would report a truncated one. Both fall through to 3.
3. Otherwise 0, which native reports as no `+BUILD` (never a fake `+0`).

Tests cover each case, including the parent-repository and shallow-clone traps (RED first: the script did not exist), and
that the built addon / `dist/` carries the number the script resolves (RED against the previous build, then GREEN after a
rebuild). Node 212 tests, wasm 226.

**Test case 3, same string for the same pin.** At the `78bb318` pin node, wasm, py and net all report `0.9.0+272-rc`
(the pin's commit count). Getting there found a stale-library trap in the OLD native Makefile (pins before `ab42e73`): `-D`
flags are not tracked by `-MMD`, so after a submodule bump `anvil.o` kept the previous pin's number. The py and net
libraries had been built at `e089837` and reported `0.9.0+264-rc` for a pin whose count is 272, and the first
`Anvil.Net.0.10.0-dev.2` pack carried it. A `make -C vendor/anvil clean` rebuild fixed all of them; a pin at or after
`ab42e73` does this by itself (the stamp file), so the Release agent's re-pin to the tag needs no such step. Both READMEs
say so under "The version string". Java reports whatever `anvilNativeDir` points at.

**Other things done in this batch** (requested by the planning session, no separate ticket):

- **READMEs for the prebuilt packages.** The `anvil-node`, `anvil-wasm`, `anvil-py` and `anvil-net` READMEs now open with
  "Installing the prebuilt package" (requirements, platform, how to install from the tarball), then a Quick start (parse,
  schema `validateField`, writer; each snippet was run), then the API sections, with everything about building from source,
  the vendor pin, design history and the repo layout under one "Development" heading. No `](../` links remain
  (`scripts/release/check-readme.sh` passes on all four). The wasm README no longer defers to `anvil.node`'s: it is
  self-contained. The py and net `docs/getting-started.md` prerequisites no longer tell an installed user to init a
  submodule. `anvil.java` is not distributed, so its README only had stale facts corrected.
- **`Anvil.Net` is `0.10.0-dev.2`.** `dev.1` carries the `f9ee783` library. The package was verified by consuming it from
  a local feed with a `PackageReference` (a newline before `]` gives `b` the raw `"2"` and decoded `2`; version
  `0.9.0+272-rc`). The tarball route was verified too (a plain `Reference` + `libanvil.so` copied beside the output).

