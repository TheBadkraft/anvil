# SR-2610-anvl-build-001: `test_release` can link a stale `libanvil.a`

**ID:** SR-2610-anvl-build-001
**Type:** Service Request (build hygiene)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the coder agent's integrated-stack report
**Status:** open
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
