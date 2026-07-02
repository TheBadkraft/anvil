# Building Anvil (Current Repo Workflow)

This document reflects the build flow that exists in this repository today.

## Build Surfaces

- Core libraries: `lib/Makefile`
- Unit tests: `test/unit/Makefile`
- Official bindings orchestration: `bindings/Makefile`

## Prerequisites

- `gcc`
- `make`
- `python3` (for bindings handoff manifest generation)
- `valgrind` (optional, for memory checks)

## Quick Start

```bash
# 1) Build core debug libraries (.a and .so)
make -C lib

# 2) Build core release libraries (.a and .so)
make -C lib release

# 3) Run selected active unit suites
make -C test/unit test_resolver test_vars test_interp_string test_using test_schema test_anon_block_attrs

# 4) Run unit valgrind gate
make -C test/unit valgrind

# 5) Generate official bindings + contract manifest
make -C bindings generate
```

## Core Library Build

`lib/Makefile` outputs:

- `lib/debug/libanvil.a`
- `lib/debug/libanvil.so`
- `lib/release/libanvil.a`
- `lib/release/libanvil.so`

Commands:

```bash
make -C lib           # debug + bindings generation
make -C lib release   # release libs
make -C lib clean
```

Note: `make -C lib` triggers `make -C bindings generate` so official bindings
and handoff metadata stay in sync with core changes.

## Unit Test Build and Run

`test/unit/Makefile` is the active local quality gate.

Common commands:

```bash
make -C test/unit test_resolver
make -C test/unit test_vars
make -C test/unit test_schema
make -C test/unit valgrind
make -C test/unit clean
```

## Official Bindings

Bindings are maintained under `bindings/*/`:

- `bindings/node/`
- `bindings/python/`
- `bindings/dotnet/`

Orchestration:

```bash
make -C bindings generate
make -C bindings contract
make -C bindings signoff
```

Contract output:

- `bindings/.handoff/binding-handoff.json`

Related docs:

- `docs/maintainers/bindings-maintenance.md`
- `docs/maintainers/bindings-signoff-checklist.md`
- `docs/bindings-api-reference.md`

## Legacy/External Tooling Note

Some historical docs mention `cbuild` / `ctest` scripts from external locations.
Those are not required for the repository-local flow documented here.
If your environment still uses those wrappers, treat them as optional overlays
on top of the Makefile-based workflow above.
