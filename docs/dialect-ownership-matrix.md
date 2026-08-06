# Dialect Ownership Matrix and Parsing Policy

Status: active policy reference for parser and language-surface refactors.

## Core Policy

- Explicit dialect capability checks are required for dialect-owned features.
- If no shebang or other dialect clue is present, dialect defaults to the least restrictive mode: ASL.
- AML keeps module-definition composition features: inheritance and anonymous blocks.
- Mentions of AMP+ should be minimal in core docs and code comments.
- AMP+ is treated as a constrained extension profile of AMP for government-use contexts.

## Ownership Matrix

| Feature | AML | ASL | AMP | Notes |
|---|---|---|---|---|
| import declarations | Yes (primary owner) | Optional/No (decide explicitly per parser gate) | No | Keep deterministic ordering and cycle checks. |
| using declarations | No (migrating out) | Yes (owner) | No | Should be rejected outside ASL once gates are enforced. |
| vars block | No (migrating out) | Yes (owner) | No | Treat as ASL capability with explicit errors when unavailable. |
| var-ref (`$name`, `${name}`) | No (migrating out) | Yes (owner) | No | Gate scanner/parser branches by dialect. |
| interpolation (`$"...{...}"`) | No (migrating out) | Yes (owner) | No | Gate in value parser and scanner diagnostics. |
| inheritance/base clause | Yes (owner) | No (unless explicitly expanded later) | No | Remains integral to AML module composition. |
| anonymous blocks | Yes (owner) | No (unless explicitly expanded later) | No | Remains integral to AML module composition. |
| module attributes (`@[...]`) | Yes | Optional (policy decision) | No | If shared with ASL, keep explicit check and error text per dialect. |

## Implementation Guardrails

- Add capability predicates that parser stages can call consistently.
- Keep gate checks close to parse entry points for each feature.
- Emit precise dialect errors instead of allowing silent fallback behavior.
- Prefer one normalization path for dialect inference before parsing starts.

## Dialect Resolution Order

1. Shebang if present.
2. Other explicit source-level clue(s), if policy defines them.
3. Default to ASL when unresolved.

## Notes For Current Refactor

- This policy is independent of module lifecycle unit tests and should not require modifying the current module test set.
- Apply these rules as document/source/parser work proceeds.
