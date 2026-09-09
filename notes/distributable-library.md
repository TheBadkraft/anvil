# Distributable Library — wrapping up "ANVL proper"

Design/implementation record for turning "ANVL proper" (core parser + resolver, the opt-in type
registry, and AnvilSchema) into an actual shippable artifact, ahead of moving on to AnvilScript.
AnvilScript is explicitly not a required component for this — it stays design-only, sequenced
behind this milestone (`notes/deferred-work.md`).

## The gap this closes

A production-readiness audit this session found the repo had no real "build the library" story:
`test/unit/Makefile` only ever compiled `src/*.c` directly into each test binary; it already
referenced `$(LIB_DEBUG)`/`$(LIB_RELEASE)` (`lib/debug/libanvil.a`, `lib/release/libanvil.a`) and
had a `test_release` target meant to link against them, but no rule anywhere actually produced
those archives — the target had never once succeeded. A consumer could only vendor source files
in, never link against a real artifact.

## Decided

- **Bundle scope for this milestone: full ANVL (core + types + schema) in one archive**,
  `libanvil.a` — the repo owner's own framing ("let's assume full ANVL for now"). This is
  deliberately the *convenience* end of the still-open bundle-vs-minimal-split packaging
  question in `notes/deferred-work.md`; it doesn't resolve that question, it just picks an
  answer for now so "ANVL proper" can actually ship. The root Makefile keeps `CORE_SRCS`/
  `TYPES_SRCS`/`SCHEMA_SRCS` as separate variables internally, so a future minimal `libanvil`
  (no schema) or standalone `libanvil_schema.a` doesn't require restructuring, just a new target.
- **A real root-level `Makefile`** (repo root, new) is the canonical place the library gets
  built — `make lib` (or `lib-debug`/`lib-release` individually) produces
  `lib/debug/libanvil.a` and `lib/release/libanvil.a` via a straightforward compile-objects-then-
  `ar rcs` sequence. `test/unit/Makefile`'s pre-existing `$(LIB_DEBUG)`/`$(LIB_RELEASE)` targets
  now just recipe as `$(MAKE) -C $(ANVL) lib-debug`/`lib-release` — the root Makefile stays the
  single source of truth, nothing is duplicated.
- **Release is genuinely stripped of debug symbols**, not just "compiled without `-g`" — the
  repo owner asked explicitly for this to be verified, not assumed. Release objects compile with
  `-O2 -DNDEBUG`, no `-g` at all; after `ar rcs` builds the archive, an explicit
  `strip --strip-debug --strip-unneeded` pass runs on it as a second, independent guarantee.
  Verified directly (not just trusted): `readelf -S` on a release-archive member shows zero
  `.debug_*` sections, while the same member from the debug archive shows the full DWARF set
  (`.debug_info`, `.debug_line`, `.debug_str`, ...). Size difference confirms it too — debug
  archive ~761KB, release ~317KB for the same source set.
- **A genuinely new test tier: `test/functional/`**, deliberately structured differently from
  `test/unit/`: its Makefile compiles *only* `test_e2e.c` and testbit's own harness — never a
  single file from `src/`, and never an `internal/` header — then links against the already-
  built `lib/{debug,release}/libanvil.a`. This is the actual point of the exercise: proving the
  *shipped artifact* itself works end to end for a real consumer (public headers + a linked
  library, nothing else), not re-proving individual behaviors `test/unit/` already covers in
  depth. `test/functional/test_e2e.c` — `FN01` (core AML parse), `FN02` (AMP's real dialect
  restrictions, not stubbed), `FN03` (opt-in type registry resolves a custom type), `FN04`
  (AnvilSchema validates a clean document), `FN05` (AnvilSchema collects multiple real violations
  in one pass) — 5 tests, 21/21 assertions, run against *both* the debug and release archives
  (`make debug`/`make release` in `test/functional/`), Valgrind-clean on both.
- **A real, if minor, pre-existing bug surfaced by finally exercising `test_release`**: its
  recipe linked `test_module.c` against `$(TEST_HELPER)` but not `$(TEST_DEBUG)`
  (`test/utilities/debug.c`) — `test_module.c` actually needs `Debug`'s interface. Undetected
  until this session, because `test_release` had never successfully linked before (no library
  existed to link against). Fixed by adding `$(TEST_DEBUG)` to that one link line;
  `make test_release` now genuinely passes (33/33 tests, 159/159 assertions) linked entirely
  against the real release archive.

## Verified this session

- `make lib` (root) — builds both archives clean, no warnings.
- Full `test/unit` 15-suite regression — still green after the `Makefile` change (301 tests,
  1451 assertions, 0 failures) — this milestone touched build plumbing only, no `src/` logic.
- `test/unit`'s `test_release` — now actually succeeds (previously could never run at all).
- `test/functional` — 5/5 tests, 21/21 assertions, both debug and release, Valgrind-clean both.
- Confirmed by direct inspection (`readelf -S`, `nm`, `ls -la`) that the release archive carries
  no debug sections and is roughly 40% the size of the debug archive.

## Open questions (not yet worked through)

- The bundle-vs-minimal-split packaging/DX default itself — still deferred, see
  `notes/deferred-work.md`'s "Distribution philosophy" entry. Today's `libanvil.a` is the
  convenience answer; the minimalist split (bare `libanvil`, standalone `libanvil_schema.a`) has
  no build target yet.
- No shared-object (`.so`) target exists yet — only static archives. Not raised as a blocker;
  revisit if/when a consumer (e.g. a future non-Node/non-wasm binding) actually needs dynamic
  linking.
- No git tag/release cut yet — `libanvil.a` exists and works, but there's still no versioned,
  citable artifact for the `anvil.node`/`anvil.wasm` bindings to depend on other than a pinned
  commit.

## Related notes

- `notes/native-schema.md` — the schema/types design record this bundle actually ships.
- `notes/deferred-work.md` — the still-open packaging/DX default question this milestone doesn't
  resolve.
