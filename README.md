# Anvil — The Final Data Language
**ANVL · Version 0.8.0-rc**

**Private Repository · Reference Implementation in Pure C**

![ANVL Wordmark](site/images/ANVL_wordmark_light.png)

**A**ttributed **N**ode **V**ariadic **L**anguage

> "JSON died so ANVL could live."
> ~ Badkraft, 2025

Anvil is not another config format. Anvil is a single language, one grammar, one parser, that
answers three problems most stacks solve with three different tools: declarative data modeling,
structured messaging, and (eventually) embedded scripting.

---

## Status, honestly

This README used to claim more than the codebase actually did. Fixed now — everything below
reflects what's real and verified today, not what's planned.

| Piece | Status |
|---|---|
| **AML** (`#!aml`) — declarative data modeling | ✅ Complete |
| **AMP** (`#!amp`) — structured messaging, zero parser attack surface | ✅ Complete |
| **Resolution** — `$identifier` VarRef, `base`/inheritance, field-merging | ✅ Complete |
| **Public C API** — `anvil_flat.h` (flat) + `anvil_vtable.h` (vtable) | ✅ Complete |
| **Opt-in type system** (`@[types]`, `anvil_types.c`) | ✅ Implemented, first phase |
| **AnvilSchema** (`@[schema]`, validating a document's shape, full constraint checking) | ✅ Implemented |
| **Distributable library** (static `.a` + shared `.so`, debug/release, functional/e2e tests against all four) | ✅ Implemented |
| **ASL** (`#!asl`) — embedded scripting | 📝 Design only — nothing implemented, not required for "ANVL proper" |

## The Case for Anvil

### One Parser. Multiple Problems.

| Dialect | Shebang | Problem It Solves | Replaces |
|---------|---------|--------------------|----------|
| **AML** | `#!aml` | Declarative data modeling and configuration | JSON, YAML, TOML, XML |
| **AMP** | `#!amp` | Structured messaging and transport | MQTT, AMQP, protobuf wire formats |
| **ASL** | `#!asl` | Embedded scripting and behavior automation *(design stage)* | Lua, inline eval loops |

The dialect is declared on the first line and enforced immediately — no runtime modes, no flags,
no separate parsers.

### The Parser Has No Attack Surface.

The parser's internal state is integers — byte offsets and lengths into the buffer you provide.
No string is ever copied during parsing. Blob payloads are skipped entirely: the parser records
where a blob starts and how long it is, then moves on without reading a single byte of content.
Under adversarial conditions — heap dump, memory probe, fuzzer, timing side-channel — the parser
cannot expose payload data because it holds none.

AMP forbids objects, attributes, inheritance, and imports outright, rejected at parse time with
no separate runtime guard layer.

### Weakly Typed by Default. Strongly Typed When You Opt In.

AML/AMP's default is a weak type system — `foo := 42;` is just a numeric value with no declared
shape. On top of that, Anvil now has a native, opt-in type system: define a reusable, named,
constrained type once (`VIN := { type := String; size := 17; };`), reference it from anywhere
that imports the file that defines it. Nothing about opting in changes the default for anyone who
doesn't. See [`docs/types-reference.md`](docs/types-reference.md) for the full reference.

Schema (validating a *data* document's shape against declared field types) builds on the same
type system and is fully implemented, including constraint checking (`size`/`min`/`max`/`values`)
and type inheritance — see [`notes/native-schema.md`](notes/native-schema.md) for the design
record. "ANVL proper" (core + types + schema) now builds as a real static library and is the
project's current milestone before AnvilScript starts — see
[`notes/distributable-library.md`](notes/distributable-library.md).

---

## Repository Structure

```
anvil/
├── include/
│   ├── anvil.h                  ← internal umbrella header
│   ├── anvil_flat.h             ← public API — flat function calls
│   ├── anvil_vtable.h           ← public API — vtable convenience layer
│   ├── anvil_types.h            ← public ABI types (opaque handles, stable enums)
│   ├── anvil_type_registry.h    ← public API — opt-in type registry
│   ├── anvil_schema.h           ← public API — AnvilSchema
│   ├── errors.h                 ← internal only, despite living here — ~90 granular parser/
│   │                               resolver codes, deliberately not exposed (the small,
│   │                               stable public category lives in anvil_types.h instead)
│   ├── internal/                ← internal-only headers (module, parser, source, ...)
│   └── sigma/                   ← vendored Sigma collections subset (types, allocator, list, map, ...)
├── src/
│   ├── core/                    ← parser, resolver, document, module, source, errors
│   ├── sigma/                   ← vendored Sigma collections implementation
│   ├── anvil_types.c            ← opt-in type registry — part of ANVL proper, not an add-on
│   └── schema/schema.c          ← AnvilSchema — the real add-on, layered on top of types
├── lib/                         ← built by the root Makefile, not checked in — debug/release
│   ├── debug/                   ← libanvil.a + libanvil.so, full DWARF info
│   └── release/                 ← libanvil.a + libanvil.so, no debug symbols (stripped)
├── test/
│   ├── unit/                    ← TestBit-based unit suites (the supported quality gate)
│   ├── functional/              ← links only against the built lib/, proves the artifact itself
│   ├── sigma/                   ← Sigma-subset unit suites
│   └── fixtures/                ← .anvl test fixtures
├── docs/                        ← language/API reference, user-facing docs
├── notes/                       ← design-thread records (internal, not user-facing)
├── Makefile                     ← builds the real distributable library
└── README.md
```

Official language bindings (Node.js via N-API, browser via WebAssembly) live in their own
repositories (`anvil.node`, `anvil.wasm`), each vendoring this repo as a pinned git submodule
rather than living inside it.

## Core Principles

| Principle | Status |
|---|---|
| Zero-copy parsing — spans into the original buffer | ✅ Locked |
| Lexerless, forward-only parsing | ✅ Locked |
| Parser holds zero payload/blob data — zero attack surface | ✅ Locked |
| No external dependencies | ✅ Locked |
| C23 (`-std=c2x`) | ✅ Locked |
| Strict TDD — RED before GREEN, Valgrind-clean | ✅ Practiced throughout |

## Building and Testing

The root `Makefile` builds the real distributable library — core + types + schema bundled into
one static archive for now (see [`notes/distributable-library.md`](notes/distributable-library.md)
for why, and what's still open about that choice):

```sh
make lib            # both lib/debug/libanvil.a and lib/release/libanvil.a
make so             # both lib/debug/libanvil.so and lib/release/libanvil.so
make lib-release     # release only — no debug symbols, verified via readelf/objdump
```

Each unit-test suite is still its own Makefile target, compiling `src/*.c` directly rather than
linking the library (faster iteration during TDD):

```sh
cd test/unit
make all      # build every suite
make test_anvil_native val   # build, run under Valgrind, one suite
```

`test/unit/Makefile` lists the mandatory core sources (`ANVIL_SRCS`) plus opt-in module sources
(e.g. `TYPES_SRCS` for `src/anvil_types.c`) per binary that needs them.

`test/functional/` is different on purpose: it links only against the *built* library
(`lib/debug/libanvil.a` or `lib/release/libanvil.a`), never a `src/*.c` file directly, to prove
the shipped artifact itself works end to end for a real consumer:

```sh
cd test/functional
make debug     # build the lib (if needed) and run against lib/debug/libanvil.a
make release   # same, against the stripped lib/release/libanvil.a
```

## What is Sigma?

`src/sigma/` is a vendored subset of **Sigma** — a separate collections/allocator library
(`List`, `Map`, `farray`/`parray`/`slotarray`, a `Stack`, an arena/bump allocator, a
generic `Query` iterator, ...) that Anvil's core is built directly on top of: every growable
list, every identifier map, every arena allocation in `src/core/` goes through Sigma, not a
hand-rolled equivalent. It's vendored in-repo (not a separate dependency) as an R&D sandbox —
new capability gets tried here first, against Anvil's own real usage, before it lands in the
real, separate Sigma repos. `test/sigma/` is Sigma's own dedicated test suite, independent of
whatever subset Anvil itself happens to exercise.

## Test Coverage

A manually-updated snapshot, not a CI-generated badge — this repo has no CI pipeline yet, so a
live badge would just be one more thing to trust without a way to verify it. Regenerate anytime
with `make coverage` (needs `gcov`, plus `lcov`/`genhtml` for the HTML report;
`test/unit/Makefile` and `test/sigma/Makefile` also have their own `coverage` targets for a
single directory's suites). Combined `test/unit` + `test/sigma` run, most recent regen:

| | Line | Function |
|---|---|---|
| **Overall** (26 source files: core + types + schema + sigma) | **81.2%** | **90.9%** |
| Core (`src/core/`) | 60–100%, mostly 80–92% | mostly 100% |
| `anvil_types.c` / `schema/schema.c` | 92.3% / 89.0% | 100% / 100% |
| Sigma (`src/sigma/`) | 51–100%, wide spread | 58–100%, wide spread |

Sigma's spread is real and expected, not a gap to close on a schedule: `test/unit` only
exercises the slice Anvil's core actually calls, and `test/sigma` covers real functions Anvil
never touches at all (`parray`/`slotarray`, most of `strings.c`) — the combined number already
reflects both suites together, which is why it's higher than either alone. Nothing here is
enforced by CI; it'll drift out of date as new code lands until someone reruns `make coverage`
and updates this table by hand. That's a deliberate, accepted tradeoff for now, not an oversight.

## Documentation

- [`docs/getting-started.md`](docs/getting-started.md) — practical, task-oriented walkthrough of
  the C API: load a document, read what's in it, clean up.
- [`docs/language-reference.md`](docs/language-reference.md) — the full language reference:
  dialects, document/value grammar, attributes, inheritance, imports, weak typing, errors, and
  the public C API/bindings surface.
- [`docs/types-reference.md`](docs/types-reference.md) — the opt-in type system: how to define
  types, the native primitive vocabulary, enums, cross-file resolution.
- [`docs/schema/README.md`](docs/schema/README.md) — why AnvilSchema exists, and why it isn't a
  bolt-on the way XSD/XSLT or JSON Schema are. `schema.c`'s first phase is now implemented — the
  *why* was written before the code because it doesn't change once the code lands, and it hasn't.
- [`docs/dialect-ownership-matrix.md`](docs/dialect-ownership-matrix.md) — which dialect owns
  which language feature.
- [`notes/`](notes/) — design-thread records for anyone picking up active work; not user-facing,
  but the most current, accurate source of "why" for anything still in progress.

---

## License

**Copyright © 2026 Quantum Override. All rights reserved.**

This software is proprietary and confidential. Unauthorized copying, distribution, modification,
or use of this software, via any medium, is strictly prohibited without express written
permission from the copyright holder.

See [`LICENSE.txt`](LICENSE.txt) for terms.
