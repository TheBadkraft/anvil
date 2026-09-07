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
11. **Built-in distribution** — Built-in function libraries are distributed as downloadable source files (e.g., `math.anvs`) so the core library does not carry scripting features that object-model-only consumers do not need. Pre-compiled `.anvlo` components may be supported later.
12. **Namespace model (initial)** — Namespaces are initially file-name-based (the imported module's file name is the namespace). A future `namespace` keyword (C#-style) is possible but not part of MVP.
13. **Error handling** — Context-local error state. Each `anvil_document` / evaluation context carries a last-error field; host code checks after a call. ASL functions may also return error values for in-language handling. The existing 5101–5105 codes are a starting point and will be expanded as the runtime grows.
14. **Parser technology** — AnvilScript uses the same hand-written parser approach as the rest of Anvil Native (recursive-descent / Pratt-style), producing a dedicated runtime AST for function bodies. No external parser generator.
15. **Dialect name and extension** — Name: **AnvilScript** (abbreviated **ASL**). File extension: `.anvs`.

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

1. **Function declaration representation** — Proposed C structure (pending final review):
   ```c
   // Forward declaration; defined in ASL runtime headers.
   struct asl_ast_node;

   typedef struct anvl_asl_func_t {
      list params;              // list of anvl_slice* parameter names
      anvl_slice body;          // source span inside { ... }
      struct asl_ast_node *ast; // NULL until lazily parsed for eval
   } anvl_asl_func_t;
   ```
   A new statement kind `ANVL_STMT_FUNCTION` and value type `ANVL_VALUE_FUNCTION` are added. The function name lives in `anvl_statement_t.name`; the value holds parameters, body slice, and a lazy runtime AST pointer. The runtime AST is likely allocated from the module's bump arena (TBD).
2. **ASL type system** — Proposed: `var` as a weak-type keyword. Variables are dynamically typed: a variable may hold a string in one statement and a number, array, or object in a later statement. Closures and error values are TBD.
3. **Exception pipeline** — Higher priority. Must be lightweight and deterministic. No heavy stack-trace bookkeeping by default; any trace/debugging aid should be optional and purposefully minimal.
4. **Built-in function registry** — Intentionally left loose until concrete built-in modules and namespace pathing are designed. Likely backed by a Sigma `map`.
5. **Specification format** — Fluid. Produce a grammar + semantics document before implementation, but expect structure to evolve as the language grows. Begin with the ANVL vs. AnvilScript boundary.

## What separates AnvilScript from ANVL

This is the intended opening framing for the full specification.

- **ANVL (top-level)** is a declarative document language. It models objects, arrays, scalars, attributes, imports, inheritance, and module structure. It is parsed by the existing ANVL parser into `anvl_value_t` / `anvl_statement_t` trees.
- **AnvilScript** is the imperative scripting layer that lives inside function bodies. It provides variables, expressions, control flow, function calls, and runtime evaluation. It is parsed by a separate ASL runtime parser into a dedicated AST.
- The boundary between the two is the function declaration signature: ANVL parses `foo (a, b, c) => { ... };`, but everything inside `{ ... }` is AnvilScript.
- If a `#!anvs` / `.anvs` document contains no function declarations, the AnvilScript Engine is never activated; the source is parsed as ANVL with ASL dialect features (`$` var-refs, interpolation, `vars`, `using`).

## MVP Grammar (EBNF)

This EBNF describes only the **AnvilScript function-body scripting language**. Top-level `#!asl` document structure (imports, `using`, `vars`, object blocks, attributes) is parsed by the existing ANVL parser and is not repeated here. The AnvilScript Engine activates only when a function declaration is encountered; if none are present, the source is effectively AML with ASL dialect capabilities (`$` var-refs, interpolation, `vars`, `using`) and no scripting runtime is needed.

The function declaration *signature* (`foo (a, b, c) => { ... }`) is owned by the ANVL parser; the ASL runtime receives the body as a source slice plus a validated parameter list. This grammar therefore begins at the first statement inside `{ ... }`.

```ebnf
(* === Function-body statements === *)
stmt_list         = { stmt } ;
stmt              = var_decl
                  | assignment_stmt
                  | if_stmt
                  | for_stmt
                  | while_stmt
                  | break_stmt
                  | continue_stmt
                  | return_stmt
                  | expr_stmt ;

var_decl          = "var" , identifier , "=" , expr , ";" ;
assignment_stmt   = identifier , "=" , expr , ";" ;
if_stmt           = "if" , "(" , expr , ")" , "{" , stmt_list , "}" , [ "else" , "{" , stmt_list , "}" ] ;
for_stmt          = "for" , "(" , [ var_decl | assignment_stmt | expr_stmt ] , ";" , [ expr ] , ";" , [ expr ] , ")" , "{" , stmt_list , "}" ;
while_stmt        = "while" , "(" , expr , ")" , "{" , stmt_list , "}" ;
break_stmt        = "break" , ";" ;
continue_stmt     = "continue" , ";" ;
return_stmt       = "return" , [ expr ] , ";" ;
expr_stmt         = expr , ";" ;

(* === Expressions === *)
expr              = logical_or ;
logical_or        = logical_and , { "||" , logical_and } ;
logical_and       = equality , { "&&" , equality } ;
equality          = comparison , { ( "==" | "!=" ) , comparison } ;
comparison        = additive , { ( "<" | ">" | "<=" | ">=" ) , additive } ;
additive          = multiplicative , { ( "+" | "-" ) , multiplicative } ;
multiplicative    = unary , { ( "*" | "/" | "%" ) , unary } ;
unary             = ( "-" | "+" | "!" ) , unary | postfix ;
postfix           = primary , [ "(" , [ arg_list ] , ")" ]
                  | qualified_call ;
qualified_call    = qualified_name , "(" , [ arg_list ] , ")" ;
qualified_name    = identifier , "." , identifier , { "." , identifier } ;
arg_list          = expr , { "," , expr } ;

primary           = value
                  | identifier
                  | "(" , expr , ")"
                  | interpolated_string ;

interpolated_string = '$' , '"' , { interpolation_text | "{" , expr , "}" } , '"' ;

(* === Values (AML-compatible literals and collections) === *)
value             = literal
                  | array
                  | tuple
                  | object_literal ;

array             = "[" , [ value , { "," , value } ] , "]"
                  | "array" , "(" , [ value , { "," , value } ] , ")" ;
tuple             = "(" , expr , "," , expr , { "," , expr } , ")"
                  | "tuple" , "(" , expr , "," , expr , { "," , expr } , ")" ;
object_literal    = "{" , { object_entry } , "}" ;
object_entry      = identifier , ":=" , value , ";" ;

(* === Literals === *)
literal           = numeric_literal
                  | string_literal
                  | bool_literal
                  | "null" ;

numeric_literal   = integer | decimal ;
integer           = digit , { digit } ;
decimal           = integer , "." , integer ;
string_literal    = '"' , { string_char } , '"' ;
bool_literal      = "true" | "false" ;

(* === Lexical === *)
identifier        = alpha , { alpha | digit | "_" } ;
alpha             = "a" ... "z" | "A" ... "Z" ;
digit             = "0" ... "9" ;
```

### Notes on the grammar

- This grammar describes only what appears inside the `{ ... }` function body. The surrounding ANVL document grammar handles `#!asl`, imports, `using`, `vars`, object blocks, attributes, and the function signature itself.
- Inside function bodies, bare identifiers are variable/function references. The `$` sigil appears **only** in string interpolation (`$"...{var}..."`); it is not used for var-refs or function calls inside function bodies.
- Assignment is `=` and local declaration is `var ... = ...`. Object literal entries retain ANVL's `:=` because they construct AML values, not execute assignments.
- Function calls may be bare (`foo(1, 2, 3)`) or qualified (`math.abs(-5)`). The grammar allows both forms. At runtime, an unqualified call is resolved to its fully qualified form whenever possible; the namespace prefix is required only when the name would otherwise be ambiguous. Member access without a call (e.g., `math.abs` as a value) is not in MVP.
- From the top-level ANVL layer, dynamic calls and var-refs use the `$` sigil. Unqualified dynamic refs are `$foo`; qualified ones are `$math.abs(-5)`.
- AML-compatible `value` literals (scalars, arrays, tuples, objects) are first-class expressions inside function bodies.
- Tuples require at least two elements. Use `(x, y)` or `tuple(x, y)`. Single-element tuples are not supported; `(x)` is grouping.
- Arrays may be written as `[x, y, z]` or `array(x, y, z)`. These forms are syntactic sugar for the same value.
- `string_char` and `interpolation_text` are left to the lexical specification.

## Runtime AST Nodes

The ASL runtime parser lazily converts each function body slice into a tree of `asl_ast_node` structures. These nodes are allocated from the module's bump arena (TBD), live as long as the document, and are consumed by a tree-walk evaluator.

### Node kinds

```c
typedef enum {
    // Statements
    ASL_STMT_BLOCK,
    ASL_STMT_VAR_DECL,
    ASL_STMT_ASSIGN,
    ASL_STMT_IF,
    ASL_STMT_FOR,
    ASL_STMT_WHILE,
    ASL_STMT_BREAK,
    ASL_STMT_CONTINUE,
    ASL_STMT_RETURN,
    ASL_STMT_EXPR,

    // Expressions
    ASL_EXPR_LITERAL,
    ASL_EXPR_IDENTIFIER,
    ASL_EXPR_QUALIFIED_NAME,
    ASL_EXPR_BINARY,
    ASL_EXPR_UNARY,
    ASL_EXPR_CALL,
    ASL_EXPR_ARRAY_INIT,
    ASL_EXPR_TUPLE_INIT,
    ASL_EXPR_OBJECT_INIT,
    ASL_EXPR_INTERPOLATED,
} asl_ast_kind;
```

### Node structure

```c
typedef struct asl_ast_node {
    asl_ast_kind kind;
    anvl_slice span;   // source span for error reporting

    union {
        struct { asl_ast_node **stmts; usize count; } block;
        struct { anvl_slice name; asl_ast_node *value; } assign;
        struct { asl_ast_node *cond, *then_stmt, *else_stmt; } if_stmt;
        struct { asl_ast_node *init, *cond, *step, *body; } for_stmt;
        struct { asl_ast_node *cond, *body; } while_stmt;
        struct { asl_ast_node *value; } return_stmt;

        struct { anvl_value_t *value; } literal;
        struct { anvl_slice name; } identifier;
        struct { anvl_slice namespace; anvl_slice name; } qualified_name;
        struct { anvl_slice op; asl_ast_node *left, *right; } binary;
        struct { anvl_slice op; asl_ast_node *operand; } unary;
        struct { asl_ast_node *callee; asl_ast_node **args; usize arg_count; } call;
        struct { asl_ast_node **elements; usize count; } collection;
        struct { anvl_slice *keys; asl_ast_node **values; usize count; } object;
        struct { asl_interpolated_segment *segments; usize count; } interpolated;
    };
} asl_ast_node;
```

### Design notes

- **Scalar literals** are represented as `anvl_value_t*`. They are constructed when the runtime AST is built by invoking the existing ANVL value-fragment parser on the source slice. The ANVL document parser itself never enters function bodies.
- **Qualified names**: `math.abs` is represented as `ASL_EXPR_QUALIFIED_NAME` with separate namespace (`math`) and name (`abs`) slices. This node can appear as a callee in `ASL_EXPR_CALL`, or anywhere a first-class qualified value is needed.
- **Interpolated strings**: `$"hello {name} world {1+2}"` is represented as a sequence of segments, each either a literal string fragment or an expression. Segments alternate: string, expr, string, expr, ... . A single segment is also valid for a plain interpolated string with no embedded expressions.
  ```c
  typedef struct asl_interpolated_segment {
      enum { ASL_SEG_STRING, ASL_SEG_EXPR } kind;
      union {
          anvl_slice text;       // for STRING: source slice of the text part
          asl_ast_node *expr;    // for EXPR: the embedded expression node
      };
  } asl_interpolated_segment;
  ```
- **Child arrays** (`block.stmts`, `call.args`, `collection.elements`, `object.values`) are contiguous arena-allocated arrays. Homogeneous traversals (e.g., find first `return` in a block) can use the Sigma HPS `Query` iterator over these arrays.
- **Heterogeneous semantic children** (`if_stmt.cond`, `if_stmt.then_stmt`, `if_stmt.else_stmt`) use named pointers.

## Exception pipeline

### Errors vs. exceptions

ASL separates *errors* from *exceptions* deliberately:

- **Errors** are expected, guardable fault paths. If you can write an `if` to check for the condition, validate input, or return a sentinel value, it is an error. Errors are handled in-language with ordinary control flow.
- **Exceptions** are truly exceptional conditions: programmer mistakes (type mismatch, arity mismatch, use of an undeclared identifier), contract violations inside a built-in, or resource exhaustion that the script cannot reasonably guard. Exceptions abort the current evaluation and report to the host.

A future `try-catch` mechanism is not for everyday error handling; it is a tool for purposefully capturing a more detailed exception report (source span, call path, optional trace) when an exceptional fault crosses a boundary where recovery or logging is useful.

### Lightweight design

- **Context-local error state** on the evaluation context:
  ```c
  typedef struct anvl_asl_error {
      anvl_err_code code;   // e.g., ANVL_ERR_ASL_TYPE_MISMATCH
      anvl_slice span;      // source location
      const char *message;  // borrowed or arena-allocated short message
  } anvl_asl_error;
  ```
- **Immediate propagation**: when any evaluator step encounters an exception, it sets the context error and returns a sentinel. Parent nodes check after each subexpression; on error they return immediately. No stack unwinding through host runtimes.
- **Control flow is not an exception**: `break`, `continue`, and `return` use a separate internal control-flow sentinel, not the error channel.
- **Host API**:
  ```c
  int anvil_call(anvil_document doc, const char *name,
                 anvil_value *args, size_t arg_count,
                 anvil_value *out_result);
  // Returns 0 on success, non-zero on exception.
  // Caller inspects anvil_get_last_error(doc) for details.
  ```
- **Optional trace**: a debug/verbose mode can push `(function_name, span)` frames into an arena-backed array. Off by default; enabled via a context flag.

## Sigma collections and the ASL runtime

The ASL runtime reuses Sigma collections where they fit naturally:

- **AST child arrays** — contiguous arrays of pointers/structs; homogeneous traversals use the HPS `Query` iterator.
- **Variable stores** — per-scope ordered collections of value handles. `list` is a natural fit, now arena-bindable via FR-007's per-instance allocator override.
- **Function registry** — `map` for module-level name → function descriptor lookups.
- **Call stack** — a dedicated **arena-backed stack** via the new `Stack` collection from FR-011.

### Arena-backed function stack

The ScriptEngine uses a single stack per evaluation context for the mechanics of function calls:

1. **Caller pushes arguments** onto the stack.
2. **Callee pops parameters** from the stack into its local frame.
3. **Callee pushes its return value** (if any) back onto the stack.
4. **Caller pops the return value** after the call completes.

This keeps argument/return lifetime trivial and avoids per-call heap allocations. Because the stack is backed by an arena allocator, the whole stack is reclaimed in bulk when the evaluation context is disposed.

FR-011 delivered a dedicated `Stack` collection backed by `collection` (which itself carries FR-007's allocator override). The ScriptEngine uses it for:

- `push` / `pop` / `peek` semantics.
- Arena-bound storage: `Stack.new_with_allocator` binds the stack to the module/context arena.
- A lightweight `mark` / `restore` cursor for possible future `yield`-style cooperative suspension.

**FR-007 impact on AnvilScript:** With per-instance allocator override now available, the runtime can allocate variable stores, AST child arrays, parser scratch space, and the call stack against the module/context arena and abandon them in bulk on teardown. This eliminates manual disposal passes for short-lived collections.

## Function registry

The ScriptEngine maintains a single registry per loaded module/document. At runtime the registry maps qualified names (`math.abs`) and unqualified names (`abs`, when unambiguous) to callable function descriptors. Built-in functions are not hard-coded; they are loaded into the registry from modules reached via `using` declarations, exactly like user-defined functions loaded via `import`. The only difference is origin: built-in modules ship as source files (e.g., `math.anvs`) and register themselves by declaring functions with the expected signatures.

### Registry shape (sketch)

```c
typedef struct asl_func_desc {
    anvl_slice qualified_name;      // "math.abs"
    anvl_slice short_name;          // "abs"
    anvl_slice namespace;           // "math"

    union {
        struct {
            struct asl_ast_node *ast;
            anvl_slice *params;
            usize param_count;
        } asl;
        struct {
            anvil_value (*invoke)(anvil_document doc,
                                  anvil_value *args, usize arg_count);
        } host;
    } impl;

    bool is_host;                   // true: native callback; false: ASL function
} asl_func_desc;

// Per-module registry backed by a Sigma map keyed by qualified name.
// A secondary structure resolves unqualified names against imported namespaces.
typedef struct asl_registry {
    map by_qualified_name;          // string -> asl_func_desc *
    map short_to_qualified;         // string -> list of asl_func_desc *
} asl_registry;
```

### Resolution rules

1. A qualified call (`math.abs`) looks up the exact qualified name.
2. An unqualified call (`abs`) first checks the module's own functions, then built-ins/`using` imports.
3. If more than one candidate exists for an unqualified name, the call is ambiguous and raises an exception.
4. Host callbacks and ASL functions share the same lookup path; the descriptor's `is_host` flag selects the invocation mechanism.

This is intentionally loose. The exact backing types (`map`, `list`, `parray`) and the `using` load protocol will be refined as the first built-in modules are designed.

## AnvilScript built-ins (theoretical sketch)

### Philosophy: AML-first

This is the inverse of the JSON/JavaScript story. JSON was discovered inside JavaScript as a native object literal syntax. Here, **AML came first**: scalars, arrays, tuples, objects, attributes, and inheritance are the native value model. AnvilScript is bolted on top as the scripting layer, and it treats those AML values as first-class citizens. Built-ins therefore do not invent a parallel object system; they script with the AML model and add only the dynamic, mutable, and host-side types that AML deliberately excludes.

Built-in functions favor lowercase, dotted namespaces, and short descriptive names. They follow ANVL's preference for clarity over terseness and avoid cryptic abbreviations.

### Categories

1. **Constructors for dynamic types AML does not have**: `list`, `dict`.
2. **Non-mutating operations on AML-native values**: `array.length`, `string.slice`, `object.keys`.
3. **Host bridges**: `math`, `time`, `io`.

Operations on AML-native values return new values; they do not mutate the original. Operations on `list` and `dict` mutate in place and return the collection to allow chaining.

### Proposed modules

#### `value` — type inspection and conversion

```anvs
value.type(x)        // -> "numeric" | "string" | "bool" | "null"
                     //    | "array" | "tuple" | "object"
                     //    | "list" | "dict" | "function"
value.is_null(x)     // -> bool
value.as_string(x)   // -> string: deterministic stringification
```

#### `array` — AML array operations (non-mutating)

```anvs
array.length(a)      // -> numeric
array.at(a, i)       // -> value; raises if out of bounds
array.slice(a, s, e) // -> array
array.find(a, v)     // -> numeric | null
array.contains(a, v) // -> bool
```

#### `tuple` — tuple operations

Tuples share several operations with arrays but are immutable and fixed-length. Where semantics differ, `tuple` has its own entry point:

```anvs
tuple.length(t)      // -> numeric
tuple.at(t, i)       // -> value
tuple.slice(t, s, e) // -> tuple
```

#### `list` — mutable dynamic sequence

`list` is not an AML type. It exists for scripting convenience and can be converted to/from an AML array.

```anvs
list.new()           // -> list
list.from_array(a)   // -> list
list.append(l, v)    // -> list
list.size(l)         // -> numeric
list.at(l, i)        // -> value
list.remove_at(l, i) // -> value
list.clear(l)        // -> list
list.to_array(l)     // -> array
```

#### `dict` — mutable key/value map

`dict` is also not an AML type. Keys are strings.

```anvs
dict.new()           // -> dict
dict.set(d, k, v)    // -> dict
dict.get(d, k)       // -> value | null
dict.has(d, k)       // -> bool
dict.remove(d, k)    // -> bool
dict.keys(d)         // -> array of strings
dict.values(d)       // -> array
dict.to_object(d)    // -> AML object (keys become identifiers)
```

#### `string`

```anvs
string.length(s)     // -> numeric
string.slice(s, s, e) // -> string
string.index_of(s, t) // -> numeric | null
string.split(s, sep) // -> array of strings
string.join(parts, sep) // -> string
string.trim(s)       // -> string
```

#### `math`, `time`, `io`

As sketched in the design threads. `math.abs`, `math.floor`, `time.now`, `time.elapsed`, `io.print`.

See `notes/anvilscript-theoretical-sketches.md` for concrete `.anvs` source examples of these modules.

### Implementation backing

Most `array`/`string` operations are natural host callbacks wrapping Sigma collection functions. `list` and `dict` can be host-backed using Sigma `list` and `map` (arena-bound via FR-007), or they can be pure AnvilScript structures. For MVP, host-backed is simpler and validates the registry model.

### Extension methods

AML-native types (`array`, `tuple`, `object`, `string`) are plain data structures with no attached methods by design. AnvilScript could introduce **extension methods**: functions declared as attached to a type and then invoked as if they were members of a value of that type.

Syntax is not decided. One direction that fits the lambda semantics:

```anvs
extend array {
    first_where(pred) => {
        // `this` refers to the array instance
        var i = 0;
        while (i < array.length(this)) {
            var v = array.at(this, i);
            if (pred(v)) { return v; }
            i = i + 1;
        }
        return null;
    };
}
```

Invocation would then look like:

```anvs
var xs = [1, 2, 3, 4];
var first_even = xs.first_where(x => x % 2 == 0);  // -> 2
```

Extension methods are purely a lookup convenience. They do not add fields to AML values, do not mutate the underlying value, and are resolved at call time by the registry. The receiver is passed as an implicit first argument, so `xs.first_where(pred)` is equivalent to `array.first_where(xs, pred)` (or whatever qualified name the extension is registered under).

This is theoretical and deferred past MVP, but the registry design should not prevent it.

### Collection type-safety

By default, AnvilScript collections are **heterogeneous**, just like AML arrays and tuples. A list can hold a number, a string, and an object in the same sequence:

```anvs
var xs = list.new();
list.append(xs, 1);
list.append(xs, "hello");
list.append(xs, { name := "x"; });
```

A future opt-in type-locking mechanism could restrict a collection to a single element type:

```anvs
var nums = list<numeric>.new();
```

This is not part of MVP. If added later, it should be a declarative constraint checked at runtime (or by an optional static pass), not a mandatory part of the type system.

### Built-in distribution formats

Built-in modules can ship in multiple forms:

- **Source `.anvs`** — human-readable, interpreted at load time.
- **Compiled `.anvlo`** — precompiled AnvilScript object file, analogous to compiled ANVL objects.
- **Host stub `.anvs` + registered callbacks** — declares the namespace and signatures in source; the runtime binds each function to a host callback.

The same registry populates from all three sources, so callers cannot distinguish them.

## Design threads

These topics are captured as active threads. They are not all next, but each must be resolved before the runtime is implemented.

### 1. `using` syntax for external sources

Two models on the table:

- **URI-style prefix**: `using "csharp:MyApp.Controllers";` or `using "cpp:render/backends/gl";`. The prefix names the foreign language/runtime, and the remainder is interpreted by that runtime's resolver.
- **Discoverable bare name**: `using "System.Core";`. The resolver searches configured repositories, paths, or registered packages and determines the source kind from metadata.

Both can coexist: the prefix is explicit; bare names rely on discovery. The open question is what the default resolver protocol looks like and where the discovery catalog lives.

### 2. Built-in module packaging

Sketching built-in modules is useful even as a theoretical exercise because it tests the namespace and registry designs.

Candidate modules:

- `math` — `abs`, `min`, `max`, `floor`, `ceil`, `sin`, `cos`, etc.
- `strings` — `length`, `substring`, `index_of`, `trim`, `split`, `join`.
- `arrays` — `length`, `append`, `remove_at`, `find`, `sort`.
- `time` — `now`, `elapsed`, `format_utc`.
- `io` (or `console`) — `print`, `read_line` (host-dependent).

Built-ins ship as source files (e.g., `math.anvs`) where possible, registering themselves through ordinary function declarations. Pure-host functions (e.g., `time.now`) are declared in an `.anvs` stub and bound to a host callback by the runtime loader.

### 3. Host callback binding

Host callbacks should be registered through a generic C API, not a language-specific binding layer. One path: expose a registry function that takes a qualified name and a C function pointer with a standard signature:

```c
anvil_value my_callback(anvil_document doc, anvil_value *args, usize arg_count);
anvil_register_function(doc, "math.abs", my_callback);
```

Existing functions from `std.h`/Sigma can be wrapped and registered the same way, which also shows how a larger API (e.g., OpenGL-style bindings) would be imported: a host module declares the namespace in `.anvs` and registers each function at load time. The binding layer must remain paradigm-agnostic — Anvil.C does not favor one host framework over another.

### 4. ASL type system

ASL starts by mirroring native ANVL types:

- `numeric` (integer and decimal)
- `string` (text; blob is a specialized string)
- `bool`
- `null`
- collections: `array`, `tuple`, `map`
- `object` (AML object/block values)

Variables are dynamically typed in MVP. A possible future direction is opt-in strong typing declared via `type` or schema attributes.

**Open string design question**: differentiate a fixed, immutable string from a mutable `char[]` buffer. One option is to keep `string` immutable and introduce a separate mutable buffer type (tentatively `text` or `buffer`) for string-building operations.

### 5. Evaluation model

This is one of the largest design necessities and must be fully developed before the runtime is implemented, even if some parts are marked tentative.

#### Tree-walk vs. bytecode

Two plausible execution strategies for the runtime AST:

| Aspect | Tree-walk | Bytecode VM |
|---|---|---|
| **Implementation cost** | Low. The AST already exists; evaluation is recursive dispatch on node kind. | Higher. Requires an instruction set, encoder, decoder, and execution loop. |
| **Execution speed** | Slow. Repeated pointer chasing, no instruction cache locality, deep call stacks for nested expressions. | Faster. Compact linear instructions, dispatch loop, easier profiling/optimization. |
| **Memory at runtime** | Minimal beyond the AST and operand stack. | Extra bytecode buffer plus constant table; roughly comparable if compact. |
| **Debugging/source maps** | Natural. Every node carries its source span; errors map directly. | Requires explicit line/debug metadata, but well-understood. |
| **Compilation to `.anvlo`** | Hard. Serializing a tree compactly is possible, but a loader must reconstruct and walk it. | Natural. Bytecode is the intended output format for `.anvlo`. |
| **Host interop** | Easy. Host callbacks are just another node-evaluation case. | Easy. Host calls become a `CALL_HOST` instruction. |

**Tentative direction for MVP**: bytecode VM. The up-front implementation cost is accepted in exchange for faster execution, simpler `.anvlo` compilation, and a cleaner separation between the parser and the execution engine. Error reporting is mitigated by keeping source-span metadata attached to each instruction (e.g., an optional debug map from instruction offset to source location).

The AST is still produced by the parser and is the canonical IR for the source representation. The compiler walks the AST once to emit bytecode; the VM executes the bytecode. This means:

- Tree-walk code does not need to be written or maintained.
- The same AST can feed both the bytecode compiler and future tooling (formatter, static analysis).
- `.anvlo` files can store bytecode directly, with source-span metadata embedded for diagnostics.

**Possible mixed model**: compile to bytecode by default; optionally keep the AST live and interpret specific nodes (e.g., during a breakpoint or single-step) only where the extra flexibility outweighs performance. This is speculative and not MVP.

#### Closures

A **closure** is a function value that captures the variables visible in the scope where it was created, so those variables remain accessible when the closure is invoked later, possibly outside that scope.

Example:

```anvs
make_counter () => {
    var count = 0;
    return () => {
        count = count + 1;
        return count;
    };
};

var counter = make_counter();
var a = counter();  // -> 1
var b = counter();  // -> 2
```

Closures require each function value to carry a reference to its captured environment. In an arena-based runtime, that environment can be a small object (or a linked frame pointer) allocated from the same arena as the function value. Closures are **tentative** for MVP because they complicate the frame model; if omitted, nested functions can still be created but cannot reference outer local variables.

#### Arena stack and variable scopes

The runtime uses at least two related structures during execution:

1. **Operand stack** — an arena-backed stack (`PArray`-like) holding arguments, return values, and temporary expression results. It is a pure value stack: push argument values before a call, pop them into parameters, push the return value, pop it at the call site.

2. **Variable frame chain** — a linked list or stack of frames, one per active function call, mapping identifiers to values. Each frame contains the function's parameters and local variables. A frame pointer links to the caller's frame for lexical scope resolution.

Relationship:

- The operand stack is for **data flow** (values moving between expressions and calls).
- The frame chain is for **name resolution** (finding `x` when the evaluator sees an identifier).
- When a function is called, a new frame is pushed onto the frame chain and the operand stack's top N values are bound to parameters.
- When the function returns, its frame is popped and a return value is pushed onto the operand stack.

If closures are supported, a frame may outlive its function call until no closures reference it. Without closures, frames can be stack-allocated and discarded in LIFO order.

This thread is high priority but does not have to be the very next topic.

## Bytecode instruction set

The instruction set is part of the language execution semantics. The VM and compiler implement it mechanically.

### Value and local operations

```c
OP_NULL          // push null
OP_TRUE          // push true
OP_FALSE         // push false
OP_CONST idx     // push constants[idx]
OP_LOAD_LOCAL n  // push local slot n
OP_STORE_LOCAL n // pop into local slot n
OP_POP           // discard top of stack
OP_DUP           // duplicate top of stack
```

### Arithmetic and comparison

```c
OP_NEGATE
OP_NOT
OP_ADD
OP_SUBTRACT
OP_MULTIPLY
OP_DIVIDE
OP_MODULO
OP_EQUAL
OP_NOT_EQUAL
OP_LESS
OP_GREATER
OP_LESS_EQUAL
OP_GREATER_EQUAL
```

### Control flow

```c
OP_JUMP offset              // relative jump
OP_JUMP_IF_FALSE offset     // pop; jump if falsy
OP_JUMP_IF_TRUE offset      // pop; jump if truthy
```

### Functions and calls

```c
OP_CALL arity               // resolve callee; invoke with arity arguments
OP_RETURN                   // return top of stack (or null) to caller
OP_HOST_CALL idx arity      // call host callback constants[idx] with arity arguments
```

`OP_CALL` is used for both ASL and registered functions when the callee is a runtime value. `OP_HOST_CALL` is an optimization for callees known at compile time to be host callbacks.

### Collections

```c
OP_ARRAY_INIT n             // build array from top n stack values
OP_TUPLE_INIT n             // build tuple from top n stack values
OP_OBJECT_INIT n            // build object from top n*2 key/value pairs
OP_INTERPOLATED n           // build string from n segments
```

### Conventions

- The instruction stream is a flat byte array.
- Multi-byte operands (`idx`, `offset`, `arity`, `n`) are stored little-endian in 32-bit slots unless compact encoding is needed.
- Source-span metadata is stored separately and indexed by instruction offset.

## Source position metadata

The source already knows its position. When the ANVL parser hands a function body to the ScriptEngine, it should pass:

- the body span (`{ ... }`);
- the validated parameter list;
- the source document / base offset, so source spans inside the body can be mapped back to absolute document positions for error reporting.

This is straightforward but must be part of the ScriptEngine invocation contract.

## Knowledge base

As the design hardens, build a separate or embedded knowledge base that tracks language principles, idioms, built-in library behavior, and migration notes. This will become the source material for public documentation once AnvilScript is published.

## Related notes

- `docs/dialect-ownership-matrix.md` — current (pre-this-design) dialect policy.
- `notes/document-header-scan.md`, `notes/document-body-parse.md` — where several of the open questions above were first raised, from the AML side.
- `notes/deferred-work.md` — the previous home for this design's open questions; now points here.
- `notes/anvilscript-theoretical-sketches.md` — concrete-but-speculative examples of built-in modules, extension methods, host bindings, and `.anvlo` compilation.
- `notes/anvilscript-scriptengine-design.md` — concrete mechanical design for the parser, compiler, VM, and ScriptEngine boundaries.
- `docs/changelog.md` `[v0.4.0-alpha]` — historical record of the removed prior-art ASL implementation.
