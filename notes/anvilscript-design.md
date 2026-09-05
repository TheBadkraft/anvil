# AnvilScript (ASL) — Design & Scope

## Status

**Design only — nothing implemented, no source being written against this note.** A prior ASL existed once (v0.4.0-alpha: `include/asl.h`/`src/asl/asl.c`, a Pratt-style parser plus a tree-walk evaluator over a tagged-union `asl_value_t`, flat scope, control flow, external module dispatch, 25 tests — see `docs/changelog.md`'s `[v0.4.0-alpha]` entry) but predates the module-context architecture rewrite and was removed from the tree under the "legacy `_*.c` reference sources" cleanup (`d8e5984`, repo owner's own deletion). Nothing of that implementation survives to build on directly. This note starts the design conversation fresh — what ASL *is*, what problem it solves that AML/AMP deliberately don't, and what extensibility means for it — informed by that history and by the fragments of policy already decided elsewhere (below), but not bound by the old implementation's choices.

**Initial design forks settled** (see [Decisions](#decisions) below). The remaining work is to produce the detailed MVP specification.

## Context — what's already decided or implied elsewhere

Nothing here is a finished ASL design; these are the load-bearing fragments already on the books that any real design has to be consistent with, gathered so the conversation starts from the current state of things instead of re-deriving it.

- **`docs/dialect-ownership-matrix.md`** — the current three-dialect policy (AML, ASL, AMP/AMP+), gated by explicit per-feature capability checks rather than implicit token support. As currently written: ASL is the default/least-restrictive dialect when no shebang or other clue is present; it owns `using` declarations, `vars` blocks, var-refs (`$name`, `${name}`), and interpolation (`$"...{...}"`); it deliberately does *not* own inheritance/`base` or anonymous blocks (those stay AML-only "module composition" features); module attributes (`@[...]`) are marked "optional/policy decision" for ASL. This matrix predates real ASL work in the current architecture and reads as a placeholder policy sketch, not a settled design — worth confirming or revising deliberately rather than assuming it's final.
- **`test/fixtures/body_varref.anvl`, `f11_static_ref.anvl`** (comments, verbatim): *"AML has no dynamic var-refs or interpolation (reserved for AnvilScript); cross-statement reference is always a static, resolve-once `$identifier`."* — i.e. AML's `$identifier` is a resolve-once alias; ASL is where dynamic (re-evaluated) var-refs and string interpolation are meant to live instead.
- **`.vscode/build.anvl`** (comment, verbatim, from a hypothetical `Sigma.Build` use case): *"AML ... there is no `vars` section, interpolation, or functions. It is verbose by design, so you can see what is this what the ASL (AnvilScript) will generate."* — one existing hint at a use case: ASL as an authoring convenience that *can generate* AML, with `Sigma.Build` accepting either as input. This is now treated as a capability, not the sole purpose.
- **`notes/deferred-work.md` § *Deferred to AnvilScript (ASL) design*** — five items explicitly parked pending this design, moved here now that this doc exists to own them (see "Open questions" below): `using` declarations, `vars` blocks' header-vs-body classification, a namespace keyword (if AML ever adds one), interpolation/dynamic var-refs, and whether `:=` is ever optional.
- **`README.md`** — architecture diagrams place `anvil.asl.o` ("AnvilScript parser + evaluator") as a peer of `anvil.schema.o`/`anvil.serializer.o` under the core library, above the language bindings layer — i.e. ASL is meant to live in this native repo, not as a per-binding feature, matching the "one-truth implementation" framing `public-api.md` already established for the rest of Anvil Native. Also: legacy `_*`-prefixed source files were retained for comparison during the refactor "AnvilScript (ASL) excepted" — since removed anyway (see Status above), so that carve-out no longer applies to anything in the tree.

## Founding framing

Three questions, deliberately mirroring how `public-api.md` opened its own design (same shape of question, different subject):

1. **What is ASL for, concretely?** What can a user do in ASL that they cannot do in AML/AMP, and why does that capability belong in a *separate dialect* rather than as an AML extension? (`build.anvl`'s comment above was the only concrete use case on record when design started; it is now treated as one capability among many.)
2. **What is ASL's relationship to AML?** A shared grammar core with extra productions layered on top (one scanner/parser, dialect-gated per the ownership-matrix model already sketched), a genuinely separate grammar that happens to interoperate, or something else? This determines a lot of downstream structure (shared `anvl_value`/`anvl_statement` representation vs. its own AST, as the old `asl_node_t`/`asl_value_t` had).
3. **What does extensibility mean for this language?** The old ASL had `asl_module_t` external-module dispatch (a callback-based FFI-into-the-script mechanism) and a `$varref` external-lookup callback — is "extensibility" about a host application injecting functions/values into a running script, embedding ASL as a scripting layer the way Lua is embedded, something narrower (e.g. just AML's own `import` mechanism reused), or something not yet imagined?

## Decisions

Forks resolved during initial design conversations.

1. **Purpose and scope** — ASL is an extensible, in-process scripting DSL that rides the coattails of the C-family (C, C++, C#, Java). It provides domain-specific built-in functions, operators, and user-defined functions that can be called from `#!asl` scripts and from host language runtimes (C#, C++, Python, etc.) in the same process. It is *not* primarily an AML generator, though generating AML is one valid use.
2. **AST representation** — Partially shared with AML. Top-level declarations parse into the existing `anvl_value_t` / `anvl_statement_t` structures where possible. Function declarations use a specialized representation: a new union member in `anvl_value_t` (or equivalent) holding parameter slices and the function body slice; the body is later parsed into a runtime-specific AST for evaluation. Zero-copy framing is not guaranteed once ASL is deployed in source.
3. **MVP feature set** — Built-in functions, expressions, variables, user-defined lambda functions, **and control flow**. Control flow is included because meaningful scripting is difficult without it. Syntax example:
   ```anvl
   foo (a, b, c) => {
      // body
   };
   ```
4. **`vars` blocks** — Header construct, placed at the end of the header. They contain immutable global constants.
5. **`using` declarations** — ASL-only header construct, analogous to `import` but for foreign code sources (C#, C++, etc.). `import` and `using` coexist in ASL: `import` loads Anvil modules; `using` brings foreign code into scope.
6. **Module attributes (`@[...]`)** — Supported in ASL.
7. **Evaluation model** — Distinct runtime evaluation phase after parsing.
8. **AML first-class citizenship** — AML scalars, objects, and collections are first-class types in ASL. ASL functions can return AML values.
9. **Cross-language callability** — Both directions: host bindings invoke ASL functions, and ASL can register/host callbacks.
10. **Default dialect** — Change from ASL to AML. ASL must be declared explicitly via shebang (`#!asl`) or file extension (`.asl`).
11. **Built-in distribution** — Built-in function libraries are distributed as source files (e.g., `math.asl`) so the core library does not carry scripting features that object-model-only consumers do not need. Pre-compiled `.anvlo` components may be supported later.
12. **Namespace model (initial)** — Namespaces are initially file-name-based (the imported module's file name is the namespace). A future `namespace` keyword (C#-style) is possible but not part of MVP.

## Variable scope and mutability

Proposed model, pending confirmation.

| Construct | Scope | Mutability | Notes |
|-----------|-------|------------|-------|
| `vars { x := 1; }` | Module global | Immutable | Header construct. |
| `var x = 1;` at top level | Module global | Mutable | Parser fails fast on duplicate top-level identifiers. |
| `var x = 1;` in function | Block-scoped | Mutable | C/C#/Java-like. |
| Function parameter | Function-local | Mutable copy | Pass-by-value of handle/reference; copy-on-write for containers. |
| Function name binding | Module global | Immutable | Functions are first-class values but function-name bindings are not variables. |
| Delegate variable | Per declaration | Mutable | `var fn = foo; fn = bar;` |

Inside function bodies, assignment uses `=` (e.g., `var x = 1;`). Top-level object declarations continue to use `:=` and inheritance clauses as in AML.

## Function semantics

- **Declaration**: `foo (a, b, c) => { ... };` binds `foo` as an immutable function in the module namespace.
- **First-class value**: Functions can be assigned to variables as delegates: `var fn = foo;`.
- **Dynamic resolution**: The `$` sigil resolves a name to its current value, just like dynamic var-refs. `$foo` retrieves the function value; `$foo(1, 2, 3)` calls it. `foo` alone is a literal identifier/string.
- **Qualified calls**: `$math.abs(-5)` resolves `math` as a namespace/module and `abs` as a function within it. If `math` is imported and `abs` is unambiguous in the current scope, `$abs(-5)` may also be valid.
- **Inheritance**: Follows AML semantics for object/block declarations. `bar : foo` is invalid when `foo` is a function because functions are immutable and not object bases.
- **Anonymous blocks**: Supported in ASL (this contradicts the current dialect ownership matrix, which marks them AML-only; the matrix must be updated).

## Calling convention

### ASL calls ASL

Function call is an expression. Arguments evaluate left-to-right, bind to parameters, body executes, and a value returns.

### Host calls ASL

Support both convenience and handle-based APIs:

- **By name**: `anvil_call(doc, "foo", args, arg_count, &result)`
- **By handle**: `anvil_function fn = anvil_function_find(doc, "foo"); anvil_function_invoke(fn, args, arg_count, &result);`

Arguments and returns use `anvil_value`. Exact arity matching in MVP; varargs and optional parameters deferred. Errors returned via out-parameter or distinct ASL error value type (TBD).

## Open questions

Unresolved or explicitly TBD. These are inputs to the detailed specification, not blockers to starting it.

1. **Function declaration representation** — Exact C structure for the parse-time value and the runtime AST pointer.
2. **Built-in function registry** — Sigma `map` of module name → (function name → descriptor). How are modules shipped/discovered?
3. **ASL type system** — Proposed: `var` as a weak-type keyword; otherwise TBD. How are ASL-specific types (closures, error values) represented?
4. **Error handling** — The existing 5101–5105 ASL error codes likely need a broader revamp/rework. Type errors, undefined functions, arity mismatches, etc., are not yet modeled.
5. **Dialect gating updates** — The ownership matrix and source default-dialect logic must be updated to reflect that ASL is no longer the default, to allow anonymous blocks in ASL, and to finalize module-attribute ownership.
6. **Specification format** — Produce a grammar + semantics document (likely in `notes/` or `docs/`) before implementation.
7. **Dialect name and file extension** — "ASL" / "AnvilScript" / `.asl` may change. Leading candidate: `.anvs` (avoids clash with `.meta.anvl` schema files). Decide before first public release.

## Related notes

- `docs/dialect-ownership-matrix.md` — current (pre-this-design) dialect policy.
- `notes/document-header-scan.md`, `notes/document-body-parse.md` — where several of the open questions above were first raised, from the AML side.
- `notes/deferred-work.md` — the previous home for this design's open questions; now points here.
- `docs/changelog.md` `[v0.4.0-alpha]` — historical record of the removed prior-art ASL implementation.
