# Plan 2611: portable ANVL implementations (Python, C#, Java)

**Status:** proposed, 2026-11-04. Nothing in the port phase is approved. The owner approves each language after
its harness is proven.
**Umbrella ticket:** see the Rivet FR named in "Tickets" below.

## Owner direction (2026-11-04)

- "in order to have a truly portable `anvil.py` it needs to be written in Python; `anvil.net` in C#; `anvil.java` in
  Java. and before I agree to that, I need the same kind of checks in place for each platform as we put in place for
  `anvil.js`."
- "write a plan for each language ... under one SR or FR ticket in Rivet ... then each task in each plan lane can be
  ticketed."
- "we should definitely work out a build on demand for performance users ... through the `anvldata` site."
- Naming: the ports take the names `anvil.py`, `anvil.net` and `anvil.java`. The current native-backed bindings are
  renamed or archived, for example `anvl-java-binding`. The Linode remotes are renamed and kept, and the local repos
  are repurposed for the ports.

## Why

- **The native-backed bindings aren't portable today.** Every native and binding download is Linux x86-64 only.
  - Native links a prebuilt, machine-local `/usr/local/packages/sigma.system.alloc.o` (an x86-64 ELF object).
  - Its build assumes gcc with C23, a GNU linker version script for exports, and POSIX file calls in
    `src/core/files.c`.
- **NuGet, Python wheels and jar resources only solve packaging.** Each still needs a native build per platform.
- **A pure implementation in the host language runs wherever the language runs.** anvil.js proved this can be done
  without losing parity.

## The two products after this plan

| | Pure port (portable) | Native-backed binding (performance) |
|---|---|---|
| Names | `anvil.py`, `anvil.net`, `anvil.java` | `anvl-py-binding`, `anvl-net-binding`, `anvl-java-binding` (proposed) |
| Runs on | Anywhere the language runs | Platforms native is built for |
| Distributed | Published packages and downloads | Built on demand through anvldata.com (`plan-2611-build-on-demand.md`) |
| Parity | Held to native by the harness below | It *is* native; held to its own binding suite |

## The rule: harness first, proven against the native-backed binding

The checks that made anvil.js trustworthy are built for each language **before** any port code. Each harness is then
proven against that language's current native-backed binding. That binding is native underneath, so it must pass
100% of every corpus with zero fuzz differences.
- A harness that passes there is known to be correct.
- A failure there is a binding bug, and it gets filed.

Only then does the owner decide whether to port. The pure port must pass the same harness. The native-backed binding
remains the reference until the port is complete.

**The checks** (the anvil.js set, per language):

| # | Check | Shared source | Per language |
|---|---|---|---|
| H1 | Conformance, byte-exact: reader (`anvil-dump 2`, 1902 at native 2288cd8), writer (`anvil-writer-script 1`, 858), builder (`anvil-builder-script 1`, 978), schema (`anvil-schema-script 1`, 561) | native `conformance/` and `FORMAT.md` | four runners that print the dump formats, plus a C-API-only list each |
| H2 | Differential fuzz against `anvil-dump` (reader, writer, builder, schema), recorded seeds, minimised repros | native `anvil-dump`, `conformance/seed/fuzz.py` | a fuzz driver |
| H3 | Drift tables: every error code, name, message and enum against native's headers | `conformance/drift-tables.txt` | a drift test |
| H4 | check-tdd adapter: per-test outcomes, assertion vs error | `scripts/tdd/check-tdd.py` | python package adapter, dotnet adapter, gradle adapter |
| H5 | Speed gates, labelled per STANDARDS §8, cold and warm | `bench/` workloads | a bench harness; baseline = the native-backed binding |
| H6 | Docs samples executed against the port | `scripts/docs/check-examples.py` | python and C# exist; Java to add |

**Harness gate (per language):** H1-H5 green against the native-backed binding, reviewed by the Verification
Engineer. Then the owner decides on the port.

## Port phases (per language, the anvil.js order)

| Phase | Scope | Gate |
|---|---|---|
| P1 | Reader and the public read API at the binding's API | reader corpus 1902/1902, reader fuzz 0 differences |
| P2 | Writer | writer corpus byte-exact |
| P3 | Builder | builder corpus byte-exact |
| P4 | Types and schema | schema corpus 561/561 |
| P5 | Packaging and release under the port's name | every release check, including the full suite |

Each phase is strict TDD under the mandatory checker, with an independent verification before merge.

## Naming and repository moves

These happen **when a language's port phase starts**, never before, so no consumer loses a working binding.
1. On Linode, rename the bare repo (for example `repos/anvil.java.git` to `repos/anvl-java-binding.git`) and keep it.
   Create an empty `repos/anvil.java.git` for the port.
2. Locally, rename the checkout (`~/Projects/repos/anvil.java` to `~/Projects/repos/anvl-java-binding`) and point its
   `origin` at the renamed remote. A new `~/Projects/repos/anvil.java` is cloned from the new empty remote.
3. Rename the binding's package identity so the two products can't collide in one environment:
   - Python import name and dist name;
   - NuGet `PackageId`;
   - Java group and artifact.

   The names are an owner decision; proposed defaults are in each lane.
4. Published downloads are never removed. New native-backed builds get new file names.
5. **Downstream moves first.** Before the rename, each consumer is told and given the new path:
   - mcdeuce: `includeBuild("../repos/anvil.java")` becomes the binding's new path, or the port once P5 passes;
   - K&C: anvil-py by URL.

## Order

1. v0.14.0-rc ships first. This plan doesn't touch it.
2. **Python harness (H1-H5),** proven against anvil.py. It's the quickest runner to write, and the Python checker
   adapter exists for tooling.
3. **C# harness, then Java harness.** These can run in parallel with the Python port if the owner approves it.
4. Ports, one language at a time, in the order the owner chooses.
5. Build on demand runs as its own lane (`plan-2611-build-on-demand.md`).

## Decisions for the owner

- D1. Approve harness-first, and the proof against the native-backed binding.
- D2. The names for the renamed native-backed bindings: repos, packages, imports and downloads.
- D3. The speed-gate threshold for each port. A pure C# or Java port may be slower than today's native-backed binding,
  because calling native from those languages is cheap. Propose: report-only for the first port release, then set a
  gate from the measurements.
- D4. The order of the ports.

## Lanes

- `notes/plan-2611-port-python.md`
- `notes/plan-2611-port-csharp.md`
- `notes/plan-2611-port-java.md`
- `notes/plan-2611-build-on-demand.md`

## Tickets

The umbrella FR is in project `anvil`. Each lane task is filed under it as its lane's harness phase begins. Port-phase
tickets are filed only after the owner approves that language.
