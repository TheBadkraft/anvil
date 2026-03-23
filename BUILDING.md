# Bash Build System (BBS) v0.2.0 — Anvil

A Makefile-free build system using `cbuild` (in PATH) and a local `config.sh`. Sources and test structure mirror the Sigma ecosystem conventions.

## Overview

| Script | Source | Purpose |
|--------|--------|---------|
| `cbuild` | `/usr/local/scripts/cbuild` (in PATH) | Build library, clean, install |
| `ctest` | `/usr/local/scripts/ctest` (in PATH) | Run a single flat test (legacy) |
| `rtest` | `./rtest` (local) | Run nested tests by subdirectory |
| `config.sh` | `./config.sh` (local) | All build variables and custom functions |

---

## Quick Start

```bash
# Build shared library → bin/lib/libanvil.so
cbuild lib

# Compile only (no link)
cbuild compile

# Clean object files
cbuild clean

# Run all unit tests
./rtest unit

# Run a single test
./rtest unit/parser

# Run a test with valgrind
./rtest unit/parser --valgrind
```

---

## Configuration (`config.sh`)

All build variables are in `config.sh`. Key settings:

| Variable | Default | Description |
|----------|---------|-------------|
| `CC` | `gcc` | Compiler |
| `STD` | `c2x` | C standard |
| `CFLAGS` | `-Wall -Wextra -g -fPIC ...` | Library compile flags |
| `TST_CFLAGS` | `$CFLAGS -DTSTDBG -I./test` | Test compile flags |
| `LDFLAGS` | `-shared` | Library link flags |
| `REQUIRES` | `sigma.memory sigma.text` | Library packages (resolved from `/usr/local/packages/`) |
| `TST_REQUIRES` | `sigma.memory sigma.text sigma.test` | Test packages |
| `LIB_NAME` | `anvil` | Output: `bin/lib/libanvil.so` |

### Source Layout Note

Anvil sources span two subdirectories (`src/core/*.c`, `src/utilities/*.c`). `config.sh` defines a custom `anvil_build_lib()` function that handles this — `cbuild`'s default single-directory glob is bypassed via `BUILD_TARGETS`.

---

## Test Structure

Tests follow sigma.memory's nested layout:

```
test/
  unit/          ← Core functionality tests (fast, isolated)
  integration/   ← Cross-component tests
  performance/   ← Benchmarks
  stress/        ← Long-running / high-load tests
  validation/    ← Correctness / spec-compliance tests
  utilities/     ← Shared helpers (helpers.c, diagnostic.c, prototype.c)
  samples/       ← .anvl / .aml sample files
```

Test files follow the `test_<name>.c` convention in their subdirectory.

---

## Running Tests (`rtest`)

```bash
# Run one test (subdir/name — omit "test_" prefix)
./rtest unit/parser

# Run all tests in a subdirectory
./rtest unit

# Run with valgrind
./rtest unit/parser --valgrind
./rtest unit --valgrind

# Show help
./rtest --help
```

`rtest` compiles library objects, test helper objects, and the test binary on demand (incremental — only recompiles stale files). All linking is done against `/usr/local/packages/*.o` entries in `TST_REQUIRES`.

---

## Memory Checking

### Valgrind

```bash
# Enable in config.sh (already true by default)
VALGRIND_ENABLED=true

./rtest unit/parser --valgrind
./rtest unit --valgrind
```

### AddressSanitizer (ASAN)

```bash
# Enable in config.sh
ASAN_ENABLED=true

cbuild clean_all
cbuild lib
./rtest unit/parser
```

ASAN requires recompilation; valgrind works on existing binaries.

---

## Build Targets (`cbuild <target>`)

| Target | Action |
|--------|--------|
| `all` / `lib` | Build `bin/lib/libanvil.so` |
| `compile` | Compile object files only (no link) |
| `clean` | Remove `.o` files from `build/` |
| `clean_all` | Remove all build artifacts |
| `install` | Build and install to system (`/usr/lib/`, `/usr/include/`) |
| `root` | Show project info (verifies `config.sh` in use) |

---

## Package Dependencies

Packages are pre-built fat object files resolved from `/usr/local/packages/<name>.o`.

| Package | File | Used by |
|---------|------|---------|
| `sigma.memory` | `/usr/local/packages/sigma.memory.o` | lib + tests |
| `sigma.text` | `/usr/local/packages/sigma.text.o` | lib + tests |
| `sigma.test` | `/usr/local/packages/sigma.test.o` | tests only |

---

## VS Code Tasks

Tasks are defined in `.vscode/tasks.json` and map to `cbuild` / `rtest` commands. Use **Terminal → Run Task** or the keyboard shortcut.
