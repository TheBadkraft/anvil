# Anvil — The Final Data Language  
**ANVL · Version 0.4.5-alpha · March 13, 2026**  

**Private Repository · Internal Reference Implementation**

```text
          █████╗ ███╗   ██╗██╗   ██╗██╗██╗     
         ██╔══██╗████╗  ██║██║   ██║██║██║     
         ███████║██╔██╗ ██║██║   ██║██║██║     
         ██╔══██║██║╚██╗██║╚██╗ ██╔╝██║██║     
         ██║  ██║██║ ╚████║ ╚████╔╝ ██║███████╗
         ╚═╝  ╚═╝╚═╝  ╚═══╝  ╚═══╝  ╚═╝╚══════╝
```

**A**ttributed **N**ode **V**ariadic **L**anguage

> “JSON died so Anvil could live.”  
> ~ Badkraft, 2025

Anvil is not another config format.  
Anvil is the **end** of config formats.  
Anvil is not just another language.  
Anvil is a paradigm shift in data structure, object modelling, and messaging.  

 · Zero-copy · Typeless · Human-first · Fast is an understatement ·  
 · Multi-dialect (AML, AMP, ASL-**TBD**) · Immutable AST-**TBD** ·  
 · Perfect round-tripping · Resolver-**complete** · AMP arrays/tuples-**complete** ·  
 · UDP-friendly · **Parser holds zero payload data — zero attack surface** ·  
  
### Built to replace every legacy data and configuration language in existence.

This repository contains the **reference implementation of Anvil in pure C**. We will be planning and executing bindings/wrappers in at least 6 other languages or platforms:
- C++
- C#/.Net
- Java
- Python
- Rust
- Node

---

## The Case for Anvil

### One Parser. Three Complete Layers.

Every distributed system has three data problems: *modeling*, *transport*, and *behavior*. Most stacks answer each with a different language, a different parser, a different security model, and a different failure mode.

Anvil collapses all three into one language, one grammar, one parser:

| Dialect | Shebang | Problem It Solves | Replaces |
|---------|---------|-------------------|---------|
| **AML** | `#!aml` | Declarative data modeling and configuration | JSON, YAML, TOML, XML |
| **AMP** | `#!amp` | Structured messaging and transport | MQTT, AMQP, protobuf wire formats |
| **ASL** | `#!asl` | Embedded scripting and behavior automation *(v0.4.0)* | Lua, inline eval loops |

The dialect is declared on the first line. The parser enforces it immediately. No runtime modes, no flags, no separate parsers.

### The Parser Has No Attack Surface.

The parser's entire internal state is integers — byte offsets and lengths into the buffer you provide. No string is ever copied. No content is buffered inside the parser at any point.

Blob payloads are skipped entirely: the parser records where a blob starts and how long it is, then moves on **without reading a single byte of content**. Under adversarial conditions — heap dump, memory probe, fuzzer, timing side-channel — the parser cannot expose payload data because it holds none.

Forbidden constructs (in AMP: objects, attributes, inheritance, imports) are rejected at parse time with no additional runtime guard layer. The parser is not a liability in the trust chain. This is not a performance claim — it is a security property.

### Runs on Any Transport. Including UDP.

AMP messages carry their own fragmentation metadata at the application layer — no transport header changes required:

```
#!amp
parts    := (1, 3)              // fragment 1 of 3
msg_id   := "a3f9"              // correlates fragments at reassembly
payload  := @aes256-gcm`...`   // this fragment's ciphertext
```

AMP is equally at home on TCP, UDP, WebSocket, serial, or a message queue. The protocol does not know or care about transport. Delivery is the consumer's problem by design — and that constraint is what makes it deployable everywhere.

---

## Repository Structure

```
anvil/                     ← this repo (private)
├── src/                  ← pure C reference implementation (Anvil)
│   ├── include/anvil.h
│   ├── core/
├── bindings/
│   ├── Anvil.J/           ← Java 21+ binding (current production)
│   ├── Anvil.CS/          ← C# / .NET 8 binding
│   ├── Anvil.CPP/         ← Modern C++20/23 binding
│   ├── Anvil.PY/          ← Python 3.12+ binding
│   └── Anvil.R/           ← Rust binding
├── spec/                  ← formal grammar, examples, golden files
├── tools/                 ← AnvilBuild (future), minifier, validator
├── Makefile
└── README.md              ← you are here
```

## Core Principles (Non-Negotiable)

| Principle                         | Status |
|-----------------------------------|---------|
| Attitude ... because it's earned | Locked |
| Zero-copy parsing, spans into original buffer | Locked |
| Immutable AST, round-trip identical writer | **TBD** (v0.2.0 — Writer pending) |
| Resolver 100 % pluggable (`$var`, `$func()`, inheritance, interpolation) | **Partial** (inheritance ✅; vars + imports TBD v0.3.0) |
| No macros except literal constants | Locked |
| No CMake. GNU Makefile until AnvilBuild exists | Locked |
| C23 default, C11 fallback only when forced | Locked |

## Dialects

One language. One parser. Three dialects — activated by shebang on the first line.

| Dialect | Shebang | Domain | Status |
|---------|---------|--------|--------|
| **AML** | `#!aml` | Declarative data modeling — replaces JSON, YAML, TOML | ✅ Complete |
| **AMP** | `#!amp` | Cipher-agnostic structured messaging — UDP-friendly, zero parser attack surface | ✅ Complete |
| **ASL** | `#!asl` | Lightweight imperative scripting — embedded behavior automation | ⚠️ Planned (v0.4.0) |

AMP is the only messaging protocol whose parser holds no payload data at any point during parsing — not a performance optimization, a security property. See [docs/amp.md](docs/amp.md) for the full specification, security customization guide, and UDP fragmentation pattern.

## Bindings Status (Official Only)

| Binding   | Language     | Status            | Repository       |
|-----------|--------------|-------------------|------------------|
| Anvil.J   | Java 21+     | **TBD** (Planned Q1 2026) | —          |
| Anvil.CS  | C# / .NET 8  | **TBD** (Planned Q1 2026) | —          |
| Anvil.CPP | C++20/23     | **TBD** (Planned Q2 2026) | —          |
| Anvil.PY  | Python 3.12+ | **TBD** (Planned Q2 2026) | —          |
| Anvil.R   | Rust 1.75+   | **TBD** (Planned Q3 2026) | —          |

No official bindings exist yet. The Java implementation referenced in prior work was a proof of concept.

## Performance Metrics

Measured on Linux x86-64, `-O0 -g` (debug build), sigma.memory 0.2.5 resource-scope bump allocator.
All parse times are single cold-parse wall-clock. Throughput is file size / parse time.

### Parse throughput (single parse, cold)

| File                     | Size   | Parse time | Statements | Throughput    |
|--------------------------|--------|------------|------------|---------------|
| `assignments.anvl`       | 461 B  | ~11 µs     | 15         | ~40 MB/s      |
| `objects.anvl`           | 391 B  | ~11 µs     | 3          | ~34 MB/s      |
| `inherits.anvl`          | 448 B  | ~10 µs     | 2          | ~43 MB/s      |
| `attributes.anvl`        | 660 B  | ~13 µs     | 3          | ~47 MB/s      |
| `arrays.anvl`            | 1.2 KB | ~12 µs     | 5          | ~97 MB/s      |
| `tuples.anvl`            | 344 B  | ~7 µs      | 3          | ~45 MB/s      |
| `modpack.anvl`           | 7.0 KB | ~128 µs    | 32         | ~54 MB/s      |
| `deep_nesting.anvl`      | 1.8 KB | ~2.4 µs    | 0          | ~726 MB/s     |
| `complex_nested.anvl`    | 3.1 KB | ~2.2 µs    | 0          | ~1,350 MB/s   |
| `repetitive_data.anvl`   | 3.7 KB | ~2.8 µs    | 0          | ~1,265 MB/s   |
| `blob_heavy_mixed.anvl`  | 1.5 KB | ~2.0 µs    | 0          | ~700 MB/s     |
| `blob_heavy_json.anvl`   | 1.2 KB | ~2.3 µs    | 0          | ~513 MB/s     |

### Summary metrics

| Metric                          | Java POC  | C v0.1.x (pre-FT-12) | C v0.2.x (FT-12, Resource scope) | Target v1.0 |
|---------------------------------|-----------|----------------------|----------------------------------|-------------|
| 8 KB config parse               | ~25 µs    | ~159 µs              | **~128 µs** (-19%, debug build)  | < 10 µs     |
| Simple array parse (1.2 KB)     | —         | ~159 µs              | **~12 µs** (13× faster)          | —           |
| Simple tuple parse (344 B)      | —         | ~78 µs               | **~7 µs** (11× faster)           | —           |
| Blob / pass-through files       | —         | —                    | **~2 µs** / 500–1,350 MB/s       | —           |
| Heap allocations (hot path)     | 0         | 0                    | **0**                            | 0           |
| Allocator                       | JVM GC    | sigma.memory Arena   | **sigma.memory Resource (mmap bump)** | —      |
| Alloc cost per node             | GC amort. | O(1) R7 bump         | **O(1) bump, zero R7 coupling**  | O(1)        |
| Memory peak (7 KB parse)        | < 64 KB   | < 64 KB              | **≤ 28 KB** (4× src_len slab)    | < 32 KB     |

> **sigma.memory team note:** FT-12 (`Allocator.Resource` scope) eliminated all BTree/skiplist overhead from
> parse-time allocation. Prior to FT-12, `SCOPE_POLICY_FIXED` incorrectly routed through MTIS; with Resource
> scopes every allocation is a pure pointer-bump into an mmap slab. Array/tuple parse is 11–13× faster as a
> direct result. Blob and pass-through files already bypassed allocation entirely and are unchanged.
> Measured at `-O0`; release builds (`-O2`) expected to improve further.

## Roadmap

| Milestone              | Target   | Content                                      | Status |
|------------------------|----------|----------------------------------------------|--------|
| Anvil 0.1.0-alpha      | Dec 2025 | Pure C parser (AML, AMP), zero memory leaks  | ✅ Released |
| Anvil 0.1.1-alpha      | Mar 2026 | AMP scalar arrays/tuples, all error codes    | ✅ Released |
| Anvil 0.2.0-alpha      | Mar 2026 | Inheritance Resolver ✅, Writer (pending)     | ⚠️ In progress |
| Anvil 0.3.0-alpha      | Q2 2026  | VarRef resolution, Import graph              | ❌ Planned |
| Anvil 0.4.0-alpha      | Q2 2026  | ASL parser + evaluator core                  | ❌ Planned |
| Anvil 1.0              | Q3 2026  | Full AML/AMP/ASL, schema validation, bindings | ❌ Planned |
| AnvilBuild 1.0         | Q4 2026  | Build system written in ASL using `.anvil`   | ❌ Planned |

---

> “We didn’t open-source Anvil. For a good reason ...
> - You want a data structure? ANVL
> - You want a message packet? ANVL
> - You want a minifiable configuration? ANVL
> - You want a secure, encrypted envelope? ANVL
> If Anvil cannot give you the data structure you want, you don't need the structure.
>
> We didn't just create a variadic structure for you data ... we weaponized your data.”
>
> ~ Badkraft · December 2025

Welcome to the future.  
Your data will never be the same.

---

## License

**Copyright © 2025 Quantum Override. All rights reserved.**

This software is proprietary and confidential. Unauthorized copying, distribution, modification, or use of this software, via any medium, is strictly prohibited without express written permission from the copyright holder.

Anvil is available under a tiered commercial license:

| Tier | Includes |
|------|----------|
| **Tier 1 — Core** | Anvil parser (`anvil.o`), all dialect support (AML, AMP, ASL), public API headers |
| **Tier 2 — Security** *(add-on)* | `anvil.security.o`, AMP Secure Packet specification, cipher registry API, `@amp.*` tag namespace |

See [LICENSE.txt](LICENSE.txt) for terms. `[TODO]` Full commercial license terms to be published.

[`anvil-engine`]: https://github.com/TheBadkraft/anvil-engine
