# SR-2610-anvl-build-001: `test_release` can link a stale `libanvil.a`

**ID:** SR-2610-anvl-build-001
**Type:** Service Request (build hygiene)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the coder agent's integrated-stack report
**Status:** resolved (branch `chore/SR-2610-anvl-build-001`, stacked on errors-001; not merged, not pushed)
**Tags:** build, makefile, tests, ci

---

## Summary

`test/unit/Makefile` declares `$(LIB_RELEASE): ; $(MAKE) -C $(ANVL) lib-release` with no
prerequisites (line ≈74). Once `lib/release/libanvil.a` exists, `test_release` never rebuilds it.
After a branch switch or rebase, it links an archive built from older sources. The coder agent hit
this: CR16/CR19/CR22 failed against an archive that predated a struct change, then passed after
`make lib-release lib-debug`. Nothing was wrong with the code. A green or red `test_release` can't
currently be trusted without a manual rebuild first.

## Fix

Always delegate to the top-level Makefile, which already tracks sources and headers (`-MMD -MP`):
make `$(LIB_RELEASE)` (and the debug archive, if used the same way) a `.PHONY`-style recipe that
runs `$(MAKE) -C $(ANVL) lib-release` every time. The top-level make is incremental, so this costs
nothing when the archive is current. Check `test/sigma` and `test/infra` Makefiles for the same
pattern.

## Test cases

1. Build, then touch a source file in `src/core`, then `make test_release` rebuilds the archive
   first (observable in the output).
2. With nothing changed, `make test_release` doesn't rebuild the archive's objects.
3. Changelog entry.

## Resolution

Branch `chore/SR-2610-anvl-build-001`, stacked on `fix/BR-2610-anvl-errors-001`.

- `test/unit/Makefile`: `lib-debug` / `lib-release` are now `.PHONY` recipes that run the top-level make,
  and `$(LIB_DEBUG)` / `$(LIB_RELEASE)` depend on them, so the archive is brought up to date on every
  `test_release`. The top-level make is incremental (`-MMD -MP`), so this is free when nothing changed.
  Same pattern `test/functional/Makefile` already used.
- `test/sigma` and `test/infra` were checked: both compile the sources they need directly into each test
  binary and never link an archive, so they cannot go stale this way. `test/functional` already delegates
  through phony `lib-debug`/`lib-release`/`build-so-*` prerequisites and is correct as it stands.
- **Test cases:** `scripts/check-test-release-fresh.sh` (`make check-test-release-fresh`) checks (1) after
  touching `src/core/errors.c`, `make test_release` rebuilds the archive's object first, and (2) with nothing
  changed it recompiles no library object. It was written first and failed on case 1 against the old
  Makefile ("it linked a stale one"), then passes. `test_release` (33/33), a normal suite and the functional
  suite still pass.
- CI note: a clean checkout was never affected (no archive to be stale). This fixes local and
  incremental runs, which is where it bit.
