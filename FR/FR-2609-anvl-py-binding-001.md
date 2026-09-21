# FR-2609-anvl-py-binding-001: `anvil.py` — the next binding, built to the `anvil.net` standard

**ID:** FR-2609-anvl-py-binding-001
**Type:** Feature Request (binding)
**Owner:** anvl
**Filed:** 2026-09-21
**Filed by:** anvil
**Status:** open — not started
**Continues:** [`FR-2609-anvl-public-api-001`](FR-2609-anvl-public-api-001.md)'s "Bindings — organization
and sequencing" section, which already names Python (`Anvil.Py`) as one of the planned bindings and
`ctypes`' `in_dll` as its documented path to the vtable's `extern const` data symbols.
**Tags:** bindings, python, ctypes, anvil.py

---

## Summary

Build `anvil.py`, a real Python binding over Anvil Native, as the next binding in sequence after
`anvil.net` — same repo/test/distribution methodology, same vtable-first binding mechanism, same
lazy/OOP-native design, adapted to Python's own idiom where the languages genuinely differ (context
managers instead of `IDisposable`, `ctypes` instead of `Marshal`/`DllImport`). This FR scopes and
sequences the work; it does not implement it.

## Motivation

Python is already named in the bindings roadmap (`C#, C++, Rust, Python, Java`) and in
`FR-2609-anvl-public-api-001`'s own audience list. `anvil.net` just proved the whole methodology
end to end — vendored submodule, vtable-based marshaling (not flat-export-only P/Invoke), lazy
per-key-cached navigation, a native-idiom test suite, a prebuilt tarball on `anvldata.com` with a
matching knowledge-base section — and gives this binding a concrete, working standard to build to
instead of re-deriving the same design questions from scratch. `ctypes` is explicitly anticipated
for this role already: `FR-2609-anvl-public-api-001`'s vtable-design rationale lists `ctypes`'
`in_dll` alongside P/Invoke and Rust's `extern "C" { static ... }"` as one of the FFI mechanisms the
vtable's plain exported-data-symbol shape was chosen to support directly, with nothing extra to add.

## Standard: `anvil.net` is the template, translated where Python genuinely differs

Each `anvil.net` decision, carried forward or translated:

| `anvil.net` | `anvil.py` | Why |
|---|---|---|
| Private repo, `linode:repos/anvil.net.git`, `vendor/anvil` submodule pinned to a real commit | Private repo, `linode:repos/anvil.py.git`, same submodule pattern | No released/versioned `libanvil` to depend on yet — same reasoning as every prior binding. |
| Vtable-based: one `Marshal.PtrToStructure` per group, `Marshal.GetDelegateForFunctionPointer`, cached once at static-constructor time | Vtable-based: one `ctypes.Structure` per group read via `AnvilLib.in_dll(lib, "Document")` etc., each field wrapped as a `ctypes.CFUNCTYPE`-typed callable, cached once at import/module-init time | Same mechanism, same "one marshal, not one for every call" discipline — `ctypes` is Python's equivalent of `Marshal`, and `in_dll` reads an `extern const` data symbol exactly the way `NativeLibrary.GetExport` + `PtrToStructure` did. Not flat-export-only `ctypes.CDLL` calls — that would be the Python equivalent of the flat-P/Invoke mistake corrected mid-`anvil.net`. |
| `AnvilDocument`/`AnvlValue`/`AnvlStatement`/`AnvilAttribute`/`AnvilError`, lazy, no JSON blob | Same five types (Pythonic naming TBD — see Open questions), same lazy, direct-call navigation | P/Invoke-class FFI calls are cheap (tens of ns); `ctypes` calls are the same order of magnitude — no boundary-crossing pressure toward an eager JSON blob, same as the `anvil.net` finding. |
| `AnvlValue`'s lazy per-key memoization cache (`Dictionary<string, AnvlStatement?>`), built one entry at a time | Same cache, `dict`-backed, same one-entry-at-a-time population | This was a deliberate, hard-won design decision (not an incidental detail) — repeated lookups of the same key must stay O(1) after the first call, not re-scan or re-call the native side. Carried forward as-is, not re-litigated. |
| `IDisposable`/`using`, deterministic | Python context manager (`with AnvilDocument.load(...) as doc:`), `__enter__`/`__exit__`, plus an explicit `.close()`/`.dispose()` for non-`with` use | Python's `__del__`/GC-finalizer timing has the same non-determinism problem `anvil.net`'s own README documents for JS — `with` is Python's real deterministic-disposal idiom, the direct translation of `using`, not a new decision. |
| `Anvil.Net.dll` + `libanvil.so` tarball on `anvldata.com`, verified standalone (extract, reference, real parse) | Equivalent tarball (the Python package plus `libanvil.so`) on `anvldata.com`, same standalone verification bar | Matches the parity already established across `anvil-node`/`anvil-wasm`/`anvil.net`'s download cards and `Bindings-Guide.md` sections. |
| NuGet named as a real, not-yet-done distribution goal (not skipped) | PyPI named as the equivalent real, not-yet-done goal | Direct precedent from the correction already on record for `anvil.net` ("we should have a NuGet option... don't want to avoid that as a viable distro") — PyPI is Python's NuGet-equivalent and gets the same treatment: real goal, sequenced after the tarball, not decided against. |
| xUnit, marshaling-layer only, never re-tests ANVL grammar | `pytest`, same scope | Matches every prior binding's own testing-scope convention. |
| `README.md` (Status/Design/Why-a-submodule/Build/Surface/Testing-scope/Distribution/Repo-layout) + `docs/getting-started.md`, every snippet actually run before publishing | Same README shape, same `docs/getting-started.md` standard, same "run it before you publish it" bar | Direct carry-forward — this bar already caught real defects in `anvil.net`'s own docs during this work; no reason to lower it for the next binding. |

## Out of scope

- **Storage/migration tooling** (Postgres, FR/BR/SR record-keeping) — explicitly not this FR's
  concern. That's Q-Or's territory now that a private Linode instance is available for it; `anvil.py`
  is a general-purpose binding, not built around that one downstream use.
- **`vars{}`/inheritance/ASL/schema surface** — no binding has this; `anvil.py` doesn't add it either.
- **Windows/macOS** — Linux x64 only, same as every other binding today, for the same reason
  (`SR-2609-anvl-distribution-001`'s recorded plan: no build hardware for those platforms yet,
  added once a contributor with real access joins, not attempted blind).
- **Locking the exact Pythonic API surface** (module layout, naming) — see Open questions below;
  that's a plan-mode design pass before implementation, the same step `anvil.net` went through
  before any code was written.

## Open questions — resolve in a plan-mode pass before implementation starts

1. **Exact surface shape.** C# has `Anvil.Document.FindStatement(...)`-style nested static classes;
   Python has no direct equivalent idiom. Candidates: a thin namespace object (`anvil.document.find_statement(...)`),
   plain module-level functions grouped by submodule, or classmethods on lightweight per-group
   classes. Whichever is chosen, it should still cleanly mirror the vtable's own grouping (the same
   principle `anvil.net`'s own nested-class structure was built on), not flatten it back into one
   giant module.
2. **Minimum supported CPython version.** Not yet decided — needs only enough to support `ctypes.CFUNCTYPE`
   and dataclasses/typing cleanly; almost any current-and-supported 3.x line qualifies, exact floor
   TBD.
3. **Naming**: repo `anvil.py` (matches `anvil.net`/`anvil.node`/`anvil.wasm`'s convention) is assumed,
   not yet confirmed; the PyPI package name (`anvil-py`? `anvilpy`? `anvil`, if unclaimed?) is a
   separate, real decision — PyPI name squatting/collisions aren't checked yet.
4. **Whether Python's own real usage surfaces a core-API gap**, the way `anvil.net`'s design work
   surfaced `anvil_document_find_statement`/`anvil_value_find_statement`. Not assumed either way —
   to be discovered by actually sketching the binding against a real document, not guessed at here.

## Sequencing

Design (plan mode, resolving the Open questions above) → prerequisite core fixes, if any surface →
scaffold (repo, submodule, build step) → binding layer (vtable marshal) → wrapper types → tests →
docs → distributable tarball + `anvldata.com` knowledge-base section — the same order `anvil.net`
itself followed.

## Acceptance criteria

- `anvil.py` repo exists on Linode, `vendor/anvil` submodule vendored, builds `libanvil.so` from
  source via the same `make so-release` step every other binding already uses.
- Vtable-based binding (`ctypes.Structure` + `in_dll`, cached callables) — not flat-export-only.
- `AnvilDocument`-equivalent context manager + the four supporting types, lazy navigation, the
  per-key memoization cache carried forward from `anvil.net`.
- `pytest` suite, marshaling-layer scope only, green.
- `docs/getting-started.md`, every example actually run first.
- Tarball published on `anvldata.com`, `Bindings-Guide.md` gets a Python section in parity with the
  existing four, verified standalone the same way every prior tarball was.
