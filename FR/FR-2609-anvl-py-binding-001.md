# FR-2609-anvl-py-binding-001: `anvil.py` — the next binding, built to the `anvil.net` standard

**ID:** FR-2609-anvl-py-binding-001
**Type:** Feature Request (binding)
**Owner:** anvl
**Filed:** 2026-09-21
**Filed by:** anvil
**Status:** resolved — implemented, tested, documented, and distributed
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

## Open questions — resolved

1. **Exact surface shape — decided: plain module-level functions, one submodule per vtable
   group** (`src/anvil/_native/document.py`, `_native/value.py`, ...). Shown side by side against
   a namespace object and per-group classmethod classes before deciding; the deciding factor was
   that this is the one option with no live Python-style debate attached — real Python modules
   as namespaces is how the standard library itself organizes related functions (`os.path`,
   `urllib.parse`), where the other two candidates each carry a real, ongoing style disagreement
   (attaching bound callables onto a plain object; classes used purely as namespaces, which
   PEP 8 itself leans away from).
2. **Minimum supported CPython version — 3.11+.** No feature of this binding needs anything
   newer; picked as a floor that's comfortably current without excluding recently-shipped
   versions.
3. **Naming — resolved for the repo, deferred for the package.** Repo is `anvil.py`, confirmed.
   PyPI package name is still open — no PyPI publish has happened yet (matching every other
   binding's own "tarball first, registry later" sequencing), so no name has been claimed or
   checked for collisions.
4. **Whether Python's own real usage surfaces a core-API gap — no.** Every primitive
   `anvil.net`'s own design work already surfaced (`anvil_document_find_statement`,
   `anvil_value_find_statement`) covered this binding's needs directly; no new core (`anvil`
   repo) change was required to build `anvil.py`.

## Sequencing

Followed exactly as planned: design (a quick plan-mode-style comparison of the three surface-shape
candidates, resolved directly with the user rather than a full plan-mode session) → scaffold
(repo, submodule, build step, verified standalone) → binding layer (vtable marshal, strict TDD,
one RED/GREEN cycle per vtable member) → wrapper types (same TDD discipline, bottom-up:
`AnvilError` → `AnvilAttribute` → `AnvlStatement` → `AnvlValue` → `AnvilDocument`) → docs →
distributable tarball + `anvldata.com` knowledge-base section.

## Resolution

All acceptance criteria met:

- `anvil.py` repo live on Linode (`linode:repos/anvil.py.git`), `vendor/anvil` submodule pinned,
  `make so-release` verified standalone before any code was written.
- Vtable-based binding, not flat-export-only: each of the 8 vtable groups (`Anvil`, `Document`,
  `Statement`, `Value`, `Attribute`, `Error`, `StatementIterator`, `DocumentIterator`) is a
  `ctypes.Structure` read once via `in_dll`, its function-pointer fields resolved into cached
  callables at import time.
- `AnvilDocument` (a real context manager, `with`/`.dispose()`, idempotent, raises on
  use-after-dispose) plus `AnvlValue`/`AnvlStatement`/`AnvilAttribute`/`AnvilError`, all lazy. The
  per-key memoization cache carried forward from `anvil.net` — proven directly via a
  call-count assertion (`monkeypatch`), not just value consistency, that a repeated lookup of the
  same key costs exactly one native call.
- `pytest` suite, marshaling-layer scope only: 45/45 green, built one strict RED→GREEN cycle at a
  time, no implementation code written ahead of a failing test that demanded it.
- `docs/getting-started.md` written, every example actually run against the real parser first
  (via a throwaway script, not assumed) — the same walkthrough surfaced a real, worth-documenting
  gap (no public accessor for an identifier's/blob's raw text, matching a gap `anvil.net` already
  has) rather than being silently worked around.
- A real packaging gap found and fixed before distribution: the library-path resolution
  (`_lib.py`) was hardcoded to this dev checkout's `vendor/anvil` layout, which doesn't exist in a
  standalone tarball. Fixed to check a packaged `runtimes/linux-x64/native/` layout first,
  falling back to the dev-checkout path — unit-tested directly (`tmp_path`, no real filesystem
  dependency) rather than only verified by building a tarball.
- `anvil-py-v0.8.0-rc-linux-x64.tar.gz` published on `anvldata.com`, verified standalone
  (extracted to a clean directory with nothing else present, imported via `PYTHONPATH`, real
  parse). `Bindings-Guide.md` gained a Python section in parity with the existing four — its own
  first code sample was caught with a real bug before publishing (a `with`-block scoping mistake:
  Python disposes exactly at block-exit, unlike C#'s `using var` declaration, which lives until
  the end of the enclosing scope) and fixed before going live.

**Not done, by design, matching the FR's own explicit scope**: storage/migration tooling (Q-Or's
territory), Windows/macOS support (no build hardware yet), and a real PyPI publish (tarball-first
sequencing, same as every other binding).
