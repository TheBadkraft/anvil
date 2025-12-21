# Anvil — The Final Data Language  
**ANVL · Version 0.1.0-alpha · December 20, 2025**  

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
Anvil is not just another languge.  
Anvil is a paradigm shift in data structure, object modelling, and messaging.  

 · Zero-copy · Typeless · Human-first · Fast is an understatment · 
 · Multi-dialect (AML, AMP, ASL-**TBD**) · Immutable AST-**TBD** · 
 · Perfect round-tripping · Resolver-**TBD** · 
  
### Built to replace every legacy data and configuration language in existence.

This repository contains the **reference implementation of Anvil in pure C**. We will be planning and executing bindings/wrappers in at least 6 other languages or platforms:
- C++
- C#/.Net
- Java
- Python
- Rust
- Node

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
| Immutable AST, round-trip identical writer | **TBD** (v0.2.0) |
| Resolver 100 % pluggable (`$var`, `$func()`, inheritance, interpolation) | **TBD** (v0.2.0) |
| No macros except literal constants | Locked |
| No CMake. GNU Makefile until AnvilBuild exists | Locked |
| C23 default, C11 fallback only when forced | Locked |

## Dialects

| Dialect | Purpose                          | Status     |
|--------|----------------------------------|------------|
| AML    | Declarative data modeling        | Complete   |
| AMP    | Anvil Messaging Protocol         | Complete   |
| ASL    | Lightweight imperative scripting | **TBD** (v0.2.0) |

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

| Metric                              | Previous (Java POC) | Current C (v0.1.0-alpha) | Target (v1.0) |
|-------------------------------------|---------------------|--------------------------|---------------|
| 8 KB real-world config parse        | ~25 µs              | Benchmarking in progress | < 10 µs       |
| 64 KB enterprise config parse       | ~120 µs             | **TBD** (pending perf run) | < 50 µs       |
| 1 MB data file parse                | ~2000 µs            | **TBD** (pending perf run) | < 800 µs      |
| Heap allocations (hot path)         | 0                   | 0                        | 0             |
| Memory peak during parse            | < 64 KB             | **TBD** (pending measurement) | < 32 KB       |

Note: First three entries are from Java proof-of-concept. C implementation performance benchmarking in progress. Zero-allocation guarantee maintained through meta-buffer and direct construction approach.

C version will be faster, smaller, and run on bare metal without a JVM.

## Roadmap

| Milestone         | Target   | Content                                      |
|-------------------|----------|----------------------------------------------|
| Anvil 0.1.0-alpha | Dec 2025 | Pure C parser (AML, AMP), zero memory leaks  |
| Anvil 0.2.0       | Q1 2026  | Immutable AST, Resolver, streaming parser    |
| Anvil 1.0         | Q2 2026  | Full AML/AMP support, ASL functions, bindings |
| AnvilBuild 1.0    | Q3 2026  | Build system written in ASL using `.anvil`   |
| Anvil 2.0         | 2027     | Multi-file modules, package system, VM       |
| Anvil Everywhere  | 2027+    | Official bindings for major languages        |

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

This software is proprietary and confidential.  
Unauthorized copying, distribution, modification, or use of this software, via any medium, is strictly prohibited without express written permission from the copyright holder.

The core C implementation and specification are released under a source-available license for internal and partnered use only.  
External distribution requires explicit licensing.

[`anvil-engine`]: https://github.com/TheBadkraft/anvil-engine
