# Plan 2611, lane: Python (`anvil.py`, pure Python)

Part of `notes/plan-2611-portable-ports.md`. Status: proposed (2026-11-04).

## Today

- `~/Projects/repos/anvil.py` (`linode:repos/anvil.py.git`): ctypes over `libanvil.so`, pinned at native v0.13.1-rc.
- The import name is `anvil`, and there's no package metadata yet (FR-2610-anvl-py-001).
- It loads only `runtimes/linux-x64/native/` or its own `vendor/anvil` build. There's no system fallback
  (validated 2026-11-03).
- Its suite is 182 pass and 4 fail at its own pin (BR-2611-anvl-py-001: an include-order writer test, and unmirrored
  FR-2610-anvl-schema-009 members).
- **Known consumer:** King & Crucible's server, pinned to anvil-py v0.11.0-rc by URL.

## Harness phase (proven against the current ctypes binding)

| Task | Work | Done when |
|---|---|---|
| PY-H0 | Fix BR-2611-anvl-py-001 first, so the reference passes its own suite | suite green at the pin |
| PY-H1a | `tests/conformance/reader_run.py`: prints `anvil-dump 2` for a corpus input, through the public API only | 1902/1902 byte-exact against native's dumps |
| PY-H1b | `writer_run.py` for `anvil-writer-script 1` | writer corpus byte-exact; C-API-only list exact and enforced |
| PY-H1c | `builder_run.py` for `anvil-builder-script 1` | builder corpus byte-exact; C-API-only list exact |
| PY-H1d | `schema_run.py` for `anvil-schema-script 1` | 561/561 |
| PY-H2 | `scripts/fuzz.py`: reader fragments, writer, builder and schema scripts, compared with `anvil-dump` | recorded seeds, 0 differences, minimised repros on any |
| PY-H3 | Drift test against `conformance/drift-tables.txt`: codes, names, messages, enums | green |
| PY-H4 | check-tdd **python package adapter**: product is `src/`, tests are `tests/`, pytest per-test outcomes, AssertionError vs any other error | the checker's own fixtures prove it; one real pytest range checked |
| PY-H5 | Bench harness on the shared workloads, cold and warm, labelled | baseline numbers recorded for the ctypes binding |

The runners use only the public API, so the same runners later drive the pure port unchanged.

**Harness gate:** PY-H1-H5 green against the ctypes binding, independently verified. Then the owner decides on the
port.

## Port phase (after owner approval)

| Task | Scope |
|---|---|
| PY-R0 | Rename the ctypes binding: repo `anvl-py-binding`, import `anvl_py_binding` (proposed), downloads renamed. Notify K&C. New empty `anvil.py` repo on Linode. |
| PY-P1 | Reader and read API (pure Python): reader corpus and fuzz |
| PY-P2 | Writer |
| PY-P3 | Builder |
| PY-P4 | Types and schema |
| PY-P5 | Packaging: `pyproject.toml` metadata, a pure-Python wheel (`py3-none-any`), and the release checks |

**Python-specific notes**
- Integers are arbitrary precision, so int64 bounds and BigInt cases are easy.
- Byte-exact output means working on `bytes`, not `str`, for the source and the writer.
- Error columns are UTF-8 byte columns, as in native.
- **Speed is the real unknown.** A pure-Python reader may be much slower than ctypes on large documents but faster on
  small ones, because ctypes pays per call. H5 measures both before any gate is set (D3).

## Not in scope

The ctypes binding's own development, beyond PY-H0 and its v0.14 bump.
