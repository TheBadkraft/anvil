# Plan 2611, lane: C# (`anvil.net`, pure C#)

Part of `notes/plan-2611-portable-ports.md`. Status: proposed (2026-11-04).

## Today

- `~/Projects/repos/anvil.net` (`linode:repos/anvil.net.git`): P/Invoke and vtable over `libanvil.so`, pinned at
  native v0.13.1-rc.
- NuGet `PackageId` is `Anvil.Net`, with `IsPackable` true. It packs only `runtimes/linux-x64/native/`.
- It loads through `NativeLibrary.Load` from the assembly directory only. There's no `LD_LIBRARY_PATH` or system
  fallback (validated 2026-11-03).
- Its xunit suite is 166 pass and 4 fail at its own pin (BR-2611-anvl-net-001).

## Harness phase (proven against the current P/Invoke binding)

| Task | Work | Done when |
|---|---|---|
| NET-H0 | Fix BR-2611-anvl-net-001, so the reference passes its own suite | suite green at the pin |
| NET-H1a-d (FR-2611-anvl-net-001) | A small console tool `tools/Anvil.Conformance` with `reader`, `writer`, `builder` and `schema` modes. It prints the four dump formats through the public API. | reader 1902, writer, builder (C-API-only lists exact) and schema 561, all byte-exact |
| NET-H2 (FR-2611-anvl-net-002) | A fuzz driver against `anvil-dump` (reader, writer, builder, schema) | recorded seeds, 0 differences |
| NET-H3 (FR-2611-anvl-net-003) | Drift test against `conformance/drift-tables.txt` | green |
| NET-H4 (FR-2611-anvl-net-004) | check-tdd **dotnet adapter**: `dotnet test` with a TRX logger; per-test pass, fail or error; `Assert` failures vs other exceptions; product `src/`, tests `test/`; `--disable-build-servers` | the checker's fixtures prove it; one real range checked |
| NET-H5 (FR-2611-anvl-net-005) | Bench harness, BenchmarkDotNet or a plain Stopwatch harness matching the shared workloads, cold and warm, labelled | baseline numbers for the P/Invoke binding |

**Harness gate:** NET-H1-H5 green against the P/Invoke binding, independently verified.

## Port phase (after owner approval)

| Task | Scope |
|---|---|
| NET-R0 | Rename the binding: repo `anvl-net-binding`, `PackageId` `Anvil.Net.Native` (proposed), downloads renamed. New empty `anvil.net` repo. |
| NET-P1 | Reader and read API (pure C#, `ReadOnlySpan<byte>` over the source) |
| NET-P2 | Writer |
| NET-P3 | Builder |
| NET-P4 | Types and schema |
| NET-P5 | NuGet `Anvil.Net` as a pure `AnyCPU` package with no `runtimes/` folder, plus the release checks |

**C#-specific notes**
- Spans and `Utf8` APIs make byte-exact parsing natural.
- `long` and `ulong` cover int64; use `BigInteger` only where native allows it.
- **Speed:** P/Invoke into native is cheap, so the pure port may be slower than the binding on large documents. H5
  measures it first (D3).
- **Target framework:** keep the binding's current target framework unless the owner chooses otherwise.
