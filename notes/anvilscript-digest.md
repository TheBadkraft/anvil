# AnvilScript (ASL) — Digest of the Design Conversation

A single-entry-point synthesis of `notes/anvilscript-design.md` (895 lines),
`notes/anvilscript-scriptengine-design.md` (271 lines), and
`notes/anvilscript-theoretical-sketches.md` (201 lines) — written so a reader (including a
future session) can get the whole shape of the design in one pass instead of three. This digest
summarizes; it does not supersede. Where a fact matters (an exact grammar production, a struct
layout, a code sketch), the three source docs remain the source of truth — this file points to
them rather than re-deriving them.

**Status, unchanged from the source docs: design only, nothing implemented, no source code
written against any of this.** The three docs together represent "initial design forks settled;
remaining work is the detailed MVP specification" — i.e., the *shape* of the language and runtime
is decided in broad strokes; a full, buildable specification still needs writing, and several
named threads are explicitly open (see below).

## Why this exists, in one paragraph

A prior ASL existed once (v0.4.0-alpha — `include/asl.h`/`src/asl/asl.c`, a Pratt parser plus a
tree-walk evaluator, 25 tests) but was deleted along with the pre-rewrite architecture and shares
no code with anything below; this is a fresh design, informed by that history but not bound by
its choices (`docs/changelog.md`'s `[v0.4.0-alpha]` entry is the only surviving record of it).
Three design threads exist because they answer three different questions: **what ASL is and why**
(`-design.md`), **how the runtime is mechanically built** (`-scriptengine-design.md`), and **what
it looks like to actually use** (`-theoretical-sketches.md`, concrete `.anvs` source, explicitly
speculative/non-committal).

## What AnvilScript is, in one paragraph

ASL is an **in-process scripting DSL** that treats already-first-class AML values (scalars,
arrays, tuples, objects, attributes) as its native value model and adds only what AML
deliberately excludes: variables, expressions, control flow, functions, and dynamic
(re-evaluated) references. It is explicitly framed as the inverse of the JSON/JavaScript
relationship — **AML came first**, ASL is the scripting layer bolted on top, not the other way
around. It is not primarily an AML generator (though that's one valid use), and it's meant to be
callable both ways: host languages (C#, C++, Python, ...) can call ASL functions in-process, and
ASL can call back into host-registered functions.

## Settled decisions

Pulled from `-design.md`'s numbered "Decisions" list (15 items), grouped by topic rather than
left as a flat list:

**Scope & relationship to AML**
- ASL lives in *this* native repo as a peer of the schema/serializer core libraries, not as a
  per-binding feature — one-truth-implementation framing, same as the rest of Anvil Native.
- The AST is **partially shared with AML**: top-level declarations reuse `anvl_value_t`/
  `anvl_statement_t` where possible; function bodies get a dedicated runtime AST, built lazily.
- The **boundary is the function declaration signature**: ANVL's own parser owns
  `foo (a, b, c) => { ... };` — it validates the signature and parameter list, then hands the
  body span to the ScriptEngine. The ANVL parser never enters `{ ... }`. If a `.anvs`/`#!anvs`
  document declares no functions at all, the ScriptEngine is never activated — the source is just
  ANVL with ASL's extra dialect features turned on (dynamic `$` refs, interpolation, `vars`,
  `import`).
- **Default dialect changed from ASL to AML** — this was a real pivot from an earlier stance; ASL
  must now be declared explicitly (`#!asl` shebang or `.anvs` extension). Confirmed independent
  of the rest of the design settling; already live in `docs/dialect-ownership-matrix.md` (see
  "Already reconciled" below).
- Name: **AnvilScript**, abbreviated **ASL**, file extension **`.anvs`**.

**Language semantics**
- `vars { ... }` blocks are a header construct holding immutable module-global constants.
- `import "..."` is ASL's counterpart to `include`: `include` loads ANVL documents, `import` brings
  *foreign* (non-ANVL) code into scope — a prefixed URI selects a host resolver (`c:`, `csharp:`,
  `python:`, ...), defaulting to `c:`. The two are deliberately kept semantically separate so
  include-graph behavior (cycles, dedup, file paths) never mixes with host-callback registration.
  (Renamed from AML's `import`/ASL's `using` — `import` reads as the cross-industry word for
  pulling in any module, foreign ones included, while `using` read too much like C#'s own
  same-language namespace directive; see `FR-2609-anvl-public-api-002`.)
- Module attributes (`@[...]`) are supported in ASL, not just AML.
- Evaluation is a **distinct runtime phase after parsing** — parse first, evaluate later
  (matches the lazy-compile-on-first-call model described under Runtime below).
- AML values are first-class in ASL; ASL functions can return them directly.
- Cross-language calling works **both directions** — host calls ASL, ASL calls host.
- **Namespaces are file-name-based** for MVP (an included/imported module's file name *is* its namespace);
  a C#-style `namespace` keyword is a possible, explicitly-not-MVP future addition.
- **Error handling is context-local**: each evaluation context carries a last-error field; ASL
  functions may also return error values for in-language handling (see "Errors vs. exceptions"
  under Runtime).
- Parser technology: **hand-written recursive-descent/Pratt**, matching the rest of Anvil Native
  — no external parser generator, same team-owned style as AML/AMP.

**Distribution**
- Built-in function libraries (`math`, `time`, `io`, ...) ship as **downloadable source files**
  (`math.anvs`), not baked into the core library — so object-model-only consumers never pay for
  scripting features they don't use. Precompiled `.anvlo` is a later option, not required for
  MVP.

## Language shape

### Variable scope & mutability

| Construct | Scope | Mutability | Notes |
|---|---|---|---|
| `vars { x := 1; }` | Module global | Immutable | Header construct |
| `x := 1;` at top level | Module global | Mutable | ANVL-style; parser fails fast on duplicates |
| `var x = 1;` in a function | Block-scoped | Mutable | C/C#/Java-like |
| Function parameter | Function-local | Mutable copy | Pass-by-value of handle/reference; copy-on-write for containers |
| Function name binding | Module global | Immutable | Functions are first-class values; the *name binding* is not a variable |
| Delegate variable | Per declaration | Mutable | `var fn = foo; fn = bar;` |

Two assignment operators coexist by design, never mixed: `:=` for ANVL-style top-level
declarations and object-literal entries (constructing AML values), `=` for imperative assignment
inside function bodies (`var x = 1; x = 2;`).

### Functions

- Declaration: `foo (a, b, c) => { ... };` — binds `foo` as an immutable module-global function.
- First-class: assignable to a `var` as a delegate.
- Dynamic dispatch via `$`: `$foo` retrieves the function *value*; `$foo(1,2,3)` calls it through
  a dynamic ref. Bare `foo` (no `$`) inside a function body is a plain identifier reference — the
  `$` sigil is reserved for string interpolation *inside* function bodies (see the grammar note
  below), a real, deliberate asymmetry between the ANVL top-level's dynamic-ref syntax and ASL's
  own expression grammar.
- Qualified calls: `$math.abs(-5)` (top-level) / `math.abs(-5)` (inside a body) resolve a
  namespace + function name; an unqualified call is valid whenever it's unambiguous.
- Two real parameter-modifier forms, both real MVP features, not speculative:
  - **Varargs**: a trailing `name []` parameter packs all remaining call arguments into an AML
    array (`sum(args [])` — `sum(1,2,3)`, `sum(1)`, `sum()` all valid).
  - **Optional parameters with literal defaults**: `greet(name, greeting = "Hello")` — defaults
    must be literals (compiled into the constant table).
- Extension methods (`extend array { first_where(pred) => {...}; }`, invoked as
  `xs.first_where(...)`) are **part of MVP**, not a stretch goal — the registry needs
  receiver-type dispatch specifically to support them.

### Grammar shape (see `-design.md`'s full EBNF for the exact productions)

Function *bodies* get a small, C-family-shaped imperative grammar: `var` declarations, `=`
assignment, `if`/`else`, `for`, `while`, `break`/`continue`/`return`, expression statements,
standard precedence-climbing binary/unary expressions (`||`, `&&`, equality, comparison,
additive, multiplicative, unary), calls (bare or qualified), and AML-compatible literal
values/collections as first-class expressions. Tuples require ≥2 elements (`(x)` is grouping,
not a tuple); arrays/tuples have both bracket and function-call spellings (`[x,y]` /
`array(x,y)`) as pure syntactic sugar for the same value. String interpolation is
`$"...{expr}..."`, and — the one place `$` appears inside a function body at all — is
lexically distinct from the top-level dynamic-ref use of `$`.

### Built-ins (theoretical sketch — see `-theoretical-sketches.md` for full `.anvs` source)

Philosophy is explicitly **AML-first**: built-ins don't invent a parallel object system, they
script the existing AML value model and add only what it deliberately lacks (mutable
dynamic types, host bridges). Three categories:

1. **Constructors for types AML doesn't have** — `list` (mutable dynamic sequence), `dict`
   (mutable string-keyed map). Neither is an AML type; both convert to/from AML arrays/objects.
2. **Non-mutating operations on AML-native values** — `array.*`, `tuple.*`, `string.*`,
   `value.*` (type inspection/conversion). These return new values, never mutate the input.
3. **Host bridges** — `math`, `time`, `io`. Real functions wrap a thin host callback
   (`math._abs` → C's `fabs`); the public `.anvs`-declared wrapper is where arity checking and
   error messages live, in source, not hard-coded into the runtime.

A separate `string`/`buffer` split is explicit and deliberate: `string` stays immutable, pinned,
hashable, usable as a `dict` key with no copying; a distinct mutable `buffer` type exists for
building text incrementally (`buffer.append`, `buffer.to_string`) — no implicit string mutation
anywhere.

**Collections stay heterogeneous by default** (a `list` can mix a number, a string, and an
object), matching AML's own arrays/tuples. Opt-in type-locking (`list<numeric>.new()`) is
explicitly sketched as a **post-MVP** direction — but the foundation is already in place for it
without further runtime change: every `anvil_value` already carries a type tag, so a future
constraint-checking layer only needs to add a descriptor and a check at `append`/`set`, not
change storage or the instruction set.

## Runtime architecture (the ScriptEngine)

Three-stage pipeline, each stage a distinct subsystem:

```
ANVL parser --(body span + params)--> Parser --(AST)--> Compiler --(bytecode)--> VM
```

- **ANVL parser**: owns the function *signature* only, never enters `{ ... }`.
- **AnvilScript Parser**: hand-written Pratt/recursive-descent (same style as AML/AMP), single
  pass, arena-allocated nodes, source span on every node, panic-mode recovery (skip to next
  statement boundary on syntax error), reuses the *existing* ANVL scalar-literal parser for
  literal value construction rather than re-implementing it. Entry points exist to parse a full
  body, a lone expression, or a lone statement in isolation — deliberately fragment-testable.
- **Compiler**: single pass over the AST, AST → flat bytecode instruction stream + constant pool
  + per-instruction source spans (for error reporting without keeping the AST alive at runtime).
- **VM**: executes the bytecode against an arena-backed operand stack (the `Stack` collection
  from FR-011) plus a linked call-frame chain.

### The tree-walk-vs-bytecode decision

Explicitly weighed with a real tradeoff table (implementation cost, execution speed, memory,
debuggability, `.anvlo` compilation, host interop) — **tentative MVP direction: bytecode VM**,
accepting higher up-front implementation cost for faster execution, a natural path to compiled
`.anvlo` objects, and a clean parser/execution split. The AST remains the canonical IR (produced
once, compiled once, never walked at runtime) — this also means no tree-walk evaluator needs to
be written or maintained at all, and the same AST can later feed tooling (a formatter, static
analysis) independent of execution. A speculative, explicitly non-MVP "mixed model" (compile by
default, interpret specific nodes during a debug break) is noted but not designed.

### Frame & stack model

Two related structures, kept conceptually separate:
- **Operand stack** (the arena-backed `Stack`) — pure data flow: push args, pop into params, push
  return value, pop at the call site.
- **Frame chain** — name resolution: one `asl_frame` per active call (parent pointer, function
  descriptor, return IP, stack slot offset/count). Locals live directly in the operand stack at
  `slot_offset..slot_offset+local_count`; a frame's `slot_offset` is just "the stack depth when
  the frame was entered," which only works because the operand stack is a real stack. Without
  closures, frames are pure LIFO and can be arena-bound outright.

### Bytecode instruction set (already sketched concretely)

Value/local ops (`OP_CONST`, `OP_LOAD_LOCAL`/`STORE_LOCAL`, `OP_POP`/`DUP`), arithmetic/
comparison, control flow (`OP_JUMP`/`JUMP_IF_FALSE`/`JUMP_IF_TRUE`), calls (`OP_CALL` for
runtime-resolved callees, `OP_HOST_CALL` as a compile-time-known-callback optimization,
`OP_RETURN`), and collection-building ops (`OP_ARRAY_INIT`/`TUPLE_INIT`/`OBJECT_INIT`/
`OP_INTERPOLATED`). `.anvlo`/bytecode blobs carry a versioned header (`magic`/`major`/`minor`/
`flags`); the VM rejects a major-version mismatch, accepts minor ≤ its own.

### Errors vs. exceptions — a deliberate, named split

- **Errors**: expected, guardable fault paths (anything you could write an `if` around) —
  handled with ordinary in-language control flow, via returned error values.
- **Exceptions**: genuinely exceptional (type/arity mismatch, undeclared identifier, contract
  violation, resource exhaustion) — abort the current evaluation, report to the host via
  context-local error state. No stack unwinding through host runtimes by default.
- `break`/`continue`/`return` use a **separate internal control-flow sentinel**, never the error
  channel.
- A future `try-catch` is explicitly scoped as a tool for *capturing a richer report* at a
  meaningful boundary, not an everyday error-handling idiom — deliberately not the primary error
  story.
- **Three debug/trace levels**, selectable per-document (option or in-source `#debug` pragma):
  `off` (code+message only), `errors` (+ source span), `trace` (+ full call-frame stack — the
  mode a future `try-catch` would consume). Off by default; each level strictly adds cost, so the
  common case pays nothing extra.

### Compilation timing

Lazy by default — a function body compiles to bytecode on its *first call*, not at parse time,
so declared-but-never-invoked functions cost nothing extra. An `asl_compile_eager` document
option exists for callers who want it all compiled up front. Compiled bytecode caches on the
function descriptor; recompilation isn't a concern for MVP since function mutation isn't
supported.

### Function registry

One per loaded module, mapping both qualified (`math.abs`) and unqualified (`abs`, when
unambiguous) names to callable descriptors — backed by a Sigma `map`, with a second structure
for unqualified-name resolution and a third (`extensions_by_type`) specifically for extension-
method dispatch. Built-ins are **not hard-coded** — they populate the *same* registry via
`import`, indistinguishable at the registry level from user-defined functions loaded via `include`;
only their origin (host-backed vs. ASL-defined) differs, tracked by one `is_host` flag on the
descriptor. An ambiguous unqualified call (more than one candidate) is a hard exception, not a
silent pick.

## Dependencies already satisfied

Two Sigma collection features this whole runtime design leans on are **already implemented**
(2026-09-07), confirmed merged into `main` during the recent wrap-up pass:

- **`FR-2603-sigma-collections-007`** — per-instance allocator override for `List`/`Collection`
  (`create_with_allocator`/`new_with_allocator`). Lets the ScriptEngine bind variable stores, AST
  child arrays, parser scratch space, and the call stack to the module/context arena and abandon
  them in bulk on teardown — no manual disposal passes.
- **`FR-2603-sigma-collections-011`** — the arena-backed `Stack` collection (built on
  `collection`, inherits FR-007's allocator override) with `push`/`pop`/`peek` plus a
  `mark`/`restore` cursor explicitly earmarked for a possible future `yield` construct. This is
  the VM's operand stack.

Neither is theoretical or pending — both are real, tested, and sitting on `main` right now.
`FR-2603-sigma-collections-009` (speculative LINQ-style query chains) and `-010` (mutation-
detection version counters) remain genuinely open Sigma R&D items, unrelated to what ASL actually
needs to start.

## Already reconciled (worth knowing, since the design docs flag it as an open question)

`-design.md`'s own "Context" section, written early in the design conversation, flagged
`docs/dialect-ownership-matrix.md` as a *pre-this-design placeholder* that might not match the
settled decisions and would need deliberate reconciling. Checked directly: **it's already been
updated** — the matrix's own header now says the relevant rows (`vars`, `import`, var-ref,
interpolation, function declarations) are "under active reconsideration as part of AnvilScript
design," the default-dialect-is-AML pivot is recorded as "confirmed current," and the full
ownership table matches the Decisions list above exactly (ASL owns `import`/`vars`/var-ref/
interpolation/function bodies; AML keeps inheritance and `include`; anonymous blocks and module
attributes are now marked shared). So this particular open thread is closed — no reconciliation
work remains there. (The matrix's unrelated "AMP+ / government-use contexts" mention is a
separate, already independently-flagged item with no bearing on ASL — see
`notes/deferred-work.md`.)

## Open questions — consolidated from all three docs

Explicitly framed in the source docs as *inputs to the detailed spec, not blockers to starting
it*:

1. **Function-declaration C representation** — a concrete struct is proposed
   (`anvl_asl_func_t`: param list, body slice, lazy AST pointer) but marked "pending final
   review," including whether the runtime AST lives in the module's bump arena.
2. **ASL type system depth** — `var` as dynamically-typed is settled for MVP; **closures and
   in-language error-value types are explicitly TBD**, not just unscheduled.
3. **Exception pipeline** — called out as *higher priority* than its neighbors on the open-items
   list; must be lightweight and deterministic, no default stack-trace bookkeeping (see the
   three-level debug/trace design above, which is the current best answer but still open at the
   spec-detail level).
4. **Built-in function registry internals** — deliberately left loose pending real built-in
   modules and namespace-pathing decisions; "likely a Sigma `map`" is a direction, not a
   commitment.
5. **Specification format itself** — expected to stay fluid; the plan is grammar + semantics
   document first, expect structure to evolve, start from the ANVL/ASL boundary.
6. **`import` resolver protocol details** — the URI-prefix scheme (`c:`/`csharp:`/`python:`) and
   lazy-binding model are sketched, not finalized; only a `c:` resolver is described concretely.
7. **Extension method syntax** — the `extend type { ... }` shape is a sketch ("one direction that
   fits"), explicitly not a decided syntax, even though extension methods themselves are
   committed as an MVP feature.

## Explicitly deferred / post-MVP (not just "unscheduled" — named and set aside on purpose)

- **Closures** — nested functions can be created and returned, but cannot capture outer locals in
  MVP. The design notes closures don't require re-architecting anything already decided (function
  values would just need to carry a captured-environment reference) — deferred by choice, not by
  difficulty.
- **Opt-in collection type-locking** (`list<numeric>`) — foundation already present (every value
  carries a type tag), syntax and constraint-checker are the only missing pieces.
- **`try-catch`** — depends on the exception pipeline/trace-mode work landing first.
- **A `namespace` keyword** — file-name-based namespacing covers MVP.
- **Mixed tree-walk/bytecode execution** (for debugger single-stepping) — speculative, not
  designed.
- **`.anvlo` precompilation** — the bytecode format is designed *with* this as a target (the
  versioned header, the compiler's bytecode-first output shape) but actually producing/loading
  `.anvlo` files is not MVP work itself.

## Where to go next

- Full grammar/struct/instruction-set detail: `notes/anvilscript-design.md`.
- Parser/compiler/VM mechanics, frame layout, error-reporting contract: `notes/anvilscript-scriptengine-design.md`.
- Concrete (non-committal) `.anvs` examples for every built-in module sketched above, plus a host
  callback registration example and an `.anvlo` sketch: `notes/anvilscript-theoretical-sketches.md`.
- Current, already-reconciled dialect policy: `docs/dialect-ownership-matrix.md`.
- Historical prior art (removed, shares no code): `docs/changelog.md`'s `[v0.4.0-alpha]` entry.
