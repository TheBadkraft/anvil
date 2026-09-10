# ANVL

**ANVL** — *Attributed · Node · Variadic · Language*

A text format with two dialects — **AML** for declarative modelling and configuration, and
**AMP** for restricted, scalar-only messaging payloads. Anvil Native, the reference
implementation, is one C parser with no runtime dependencies; Node.js and WebAssembly bindings
wrap it directly, so a document parses identically no matter which one reads it.

> **Status:** release-candidate software. The grammar and public API are stable enough to build
> against, but the surface may still move before a 1.0 release.

## Quick start

```anvl
#!aml
server := {
    host := "localhost";
    port := 8080;
};
```

```c
#include "anvil_flat.h"

anvil_document doc = anvil_load_buffer(source, source_len);
anvil_statement stmt = anvil_statement_get(doc, "server");
```

The same document parses the same way through every binding — see [Bindings](Bindings-Guide.md)
for the Node.js/WebAssembly/native-C equivalents of the snippet above.

## Documentation

- **[Getting Started](getting-started.md)** — task-oriented C API walkthrough: load a document,
  read what's in it, clean up. Every example compiled and run against the real parser.
- **[Language Reference](language-reference.md)** — the full grammar: dialects, statement/value
  forms, attributes, inheritance, imports, weak typing, errors, and the public C API/bindings
  surface.
- **[Types](types-reference.md)** — the opt-in type system: native primitives, `enum`,
  constraints, cross-file `types.X` resolution.
- **[Schema](schema/README.md)** — AnvilSchema: why it isn't a bolt-on the way XSD/XSLT or JSON
  Schema are, and what's actually implemented.
- **[AML Guide](AML-Guide.md)** — objects, arrays, tuples, attributes, inheritance, `$` VarRefs, blobs
- **[AMP Guide](AMP-Guide.md)** — the restricted messaging dialect and what it forbids
- **[Bindings](Bindings-Guide.md)** — native C library, Node.js, and WebAssembly: usage guides and downloads

## Why not just JSON?

JSON is a fine wire format. It's a rough config format — no comments, no way to reference a
value you already defined, no restricted subset for contexts where a payload shouldn't be able
to grow arbitrary structure. See the [home page](/) for the full comparison.

## License

Proprietary — see the [anvil repo](https://github.com/TheBadkraft/anvil) for licensing detail.
