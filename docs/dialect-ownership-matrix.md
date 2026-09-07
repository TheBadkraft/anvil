# Dialect Ownership Matrix and Parsing Policy

Status: active policy reference for parser and language-surface refactors. Predates the current
rebuilt architecture in places — the ownership rows below (`vars`, `using`, var-ref, interpolation)
are under active reconsideration as part of AnvilScript (ASL) design; see `notes/anvilscript-design.md`
for the current state of that work. The default-dialect policy below (AML, not ASL) is confirmed
current and does not depend on that design settling.

## Core Policy

- Explicit dialect capability checks are required for dialect-owned features.
- If no shebang or other dialect clue is present, dialect defaults to **AML** (pivoted from the pre-rebuild stance of defaulting to ASL — ASL must now be declared explicitly via shebang `#!asl` or a `.anvs` file extension). This matches the current implementation (`src/core/files.c`, `src/core/source.c`).
- AML keeps module-definition composition features: inheritance and anonymous blocks.
- **ANVL/AnvilScript boundary**: top-level ANVL owns the document structure and the function declaration *signature* (`foo (a, b, c) => { ... }`). AnvilScript owns only the imperative body inside `{ ... }`. The ASL runtime receives the body as a source slice plus a validated parameter list; it does not re-parse the signature. If a `#!anvs` / `.anvs` document contains no function declarations, the AnvilScript Engine is never activated and the source is parsed as ANVL with ASL dialect features (`$` var-refs, interpolation, `vars`, `using`).
- Mentions of AMP+ should be minimal in core docs and code comments.
- AMP+ is treated as a constrained extension profile of AMP for government-use contexts.

## Ownership Matrix

| Feature | AML | ASL | AMP | Notes |
|---|---|---|---|---|
| import declarations | Yes (primary owner) | Yes (reuses ANVL `import`) | No | Keep deterministic ordering and cycle checks. |
| using declarations | No | Yes (owner) | No | Brings foreign code into scope; rejected outside ASL. |
| vars block | No | Yes (owner) | No | Immutable module-global constants; header construct in ASL. |
| var-ref (`$name`, `${name}`) | Resolve-once static alias | Yes (dynamic owner) | No | AML `$x` resolves once during parsing; ASL `$x` re-evaluates at runtime. |
| interpolation (`$"...{...}"`) | No | Yes (owner) | No | Dynamic string interpolation; gate by dialect. |
| function declarations | No | Yes (owner) | No | ANVL parses the signature; ASL owns the body inside `{ ... }`. |
| function-body statements | No | Yes (owner) | No | `var`, assignment, `if`/`for`/`while`, `return`, expressions, etc. |
| inheritance/base clause | Yes (owner) | No | No | Remains integral to AML module composition. |
| anonymous blocks | Yes (owner) | Yes (shared) | No | Previously AML-only; now allowed in ASL as an expression/declaration form. |
| module attributes (`@[...]`) | Yes | Yes (shared) | No | Supported in ASL; keep explicit check and error text per dialect. |

## Implementation Guardrails

- Add capability predicates that parser stages can call consistently.
- Keep gate checks close to parse entry points for each feature.
- Emit precise dialect errors instead of allowing silent fallback behavior.
- Prefer one normalization path for dialect inference before parsing starts.
- **Boundary enforcement**: when the ANVL parser meets a function declaration, it parses the signature, validates parameter uniqueness, and hands the ScriptEngine the body slice plus parameter names. The ScriptEngine must not re-parse or reinterpret the signature. Errors inside the body are reported with source spans relative to the original document.

## Dialect Resolution Order

1. Shebang if present.
2. Other explicit source-level clue(s), if policy defines them (e.g. file extension — `.aml`/`.anvl`, `.amp`, `.asl`).
3. Default to AML when unresolved.

## Notes For Current Refactor

- This policy is independent of module lifecycle unit tests and should not require modifying the current module test set.
- Apply these rules as document/source/parser work proceeds.
