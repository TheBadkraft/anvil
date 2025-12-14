# Anvil — The Final Data Language  
**ANVL · Version 1.0 · December 2025**  
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

Zero-copy · Typeless · Human-first · Blazing fast · Dual-dialect · Immutable AST · Perfect round-tripping · Resolver-pluggable · Built to replace every legacy data language in existence.

This repository contains the **reference implementation of Anvil in pure C**, with official first-class bindings for the only languages that matter.

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
|-----------------------------------|--------|
| Single source of truth: `core/include/anvil.h` | Locked |
| Public API = one global `const struct anvil_interface Anvil` | Locked |
| Zero-copy parsing, spans into original buffer | Locked |
| Immutable AST, round-trip identical writer | Locked |
| Resolver 100 % pluggable (`$var`, `$func()`, inheritance, interpolation) | Locked |
| No macros except literal constants | Locked |
| No CMake. GNU Makefile until AnvilBuild exists | Locked |
| C23 default, C11 fallback only when forced | Locked |

## Dialects

| Dialect | Purpose                          | Status     |
|--------|----------------------------------|------------|
| AML    | Declarative data modeling        | Complete   |
| ASL    | Lightweight imperative scripting | In progress (functions, `$call()`, VM) |

## Bindings Status (Official Only)

| Binding   | Language     | Status            | Repository       |
|-----------|--------------|-------------------|------------------|
| Anvil.J   | Java 21+     | Production (v0.1.7) | [`anvil-engine`] |
| Anvil.CS  | C# / .NET 8  | Planned Q1 2026   | —                |
| Anvil.CPP  | C++20/23     | Planned           | —                |
| Anvil.PY  | Python 3.12+ | Planned           | —                |
| Anvil.R   | Rust 1.75+   | Planned           | —                |

The `anvil-engine` in Java is a _proof of concept_ library. It is free to be replicated, forked, clones, modified, and distributed.

All bindings consume the exact same C interface. No exceptions. No sugar in core.

## Performance (Current Java Baseline – C Will Crush It)

| Metric                       | Java 21 (warm) | C Target (2026) |
|------------------------------|----------------|-----------------|
| 8 KB real-world config parse | ~25 µs        | < 15 µs         |
| Heap allocations (hot path)   | 0              | 0               |
| Memory peak                  | < 64 KB        | < 32 KB         |

C version will be faster, smaller, and run on bare metal without a JVM.

## Roadmap

| Milestone         | Target   | Content                                      |
|-------------------|----------|----------------------------------------------|
| Anvil 1.0         | Q1 2026  | Pure C core, full AML complete, ASL functions |
| AnvilBuild 1.0    | Q2 2026  | Build system written in ASL using `.anvil`   |
| Anvil 2.0         | 2026    | Multi-file modules, package system, VM       |
| Anvil Everywhere  | 2027+   | Every major language has an official binding |

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
