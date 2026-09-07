# AnvilScript — ScriptEngine Design

This note is the concrete mechanical design for the AnvilScript runtime subsystem. It owns the structures and behavior referenced at a higher level in `notes/anvilscript-design.md`.

## Terminology

- **ANVL parser** — the existing Anvil Native parser that handles top-level ANVL/AML/AMP/ASL documents, including the function declaration *signature*.
- **AnvilScript parser** — the component that takes a function body slice and produces an `asl_ast_node` tree.
- **ScriptEngine** — the umbrella subsystem that owns parsing, compilation to bytecode, and execution. It is the runtime surface exposed to the rest of Anvil.
- **ASL runtime / VM** — the bytecode compiler, interpreter, call stack, and function registry.

## Subsystem boundaries

```
+---------------------------------------------------+
|                   ScriptEngine                    |
|  +-------------+  +------------+  +-------------+ |
|  |   Parser    |->|  Compiler  |->|  ASL VM     | |
|  |             |  |            |  |             | |
|  | body slice  |  | AST ->     |  | bytecode    | |
|  | + params    |  | bytecode   |  | + stack     | |
|  +-------------+  +------------+  | + registry  | |
|                                   +-------------+ |
+---------------------------------------------------+
          ^
          |
+---------+-----------------------------------------+
|                  ANVL Native parser               |
|  parses signature; hands ScriptEngine body slice  |
+---------------------------------------------------+
```

The ANVL parser never enters function bodies. When it encounters a function declaration, it validates the signature and parameter list, then calls the ScriptEngine to parse the body lazily (the AST is built on first evaluation, or eagerly if the caller requests it).

## Shared structures

The following structures are defined in `notes/anvilscript-design.md` and are used directly by the ScriptEngine:

- `asl_ast_kind`
- `asl_ast_node`
- `asl_interpolated_segment`
- `anvl_asl_func_t`
- `asl_func_desc`
- `asl_registry`

They are repeated here by reference, not copied, to keep the design record single-source-of-truth.

## Parser

### Why a hand-written parser

The grammar is constrained to function bodies, the team already owns hand-written parsers for AML/AMP, and the ANVL boundary is custom. A Pratt-style recursive-descent parser is the natural fit.

### Parser qualities

1. **Single-pass scan to AST** — tokens consumed as the parser descends; no separate token tree.
2. **Pratt/top-down operator precedence** for expressions.
3. **Source span on every node** — start/end offsets relative to the body slice; combined with document base offset for absolute error reporting.
4. **Panic-mode error recovery** — on syntax error, skip to the next statement boundary and continue.
5. **Arena allocation** — all nodes live in the module arena.
6. **No backtracking** — LL-friendly with 1–2 token lookahead.
7. **Minimal tokenizer** — keywords, identifiers, literals, operators, punctuation; reuses ANVL scalar-literal parsing.
8. **Clear `{`/`}` boundary** — parser begins after `{` and stops at the matching `}`.
9. **Validation separate from parsing** — semantic checks happen later.
10. **Testable by fragments** — can parse a single expression or statement in isolation.

### Token kinds

```c
typedef enum {
    ASL_TOK_EOF,
    ASL_TOK_IDENTIFIER,
    ASL_TOK_NUMBER,
    ASL_TOK_STRING,
    ASL_TOK_INTERPOLATED,   // prefix: "$" string
    ASL_TOK_TRUE,
    ASL_TOK_FALSE,
    ASL_TOK_NULL,

    // Keywords
    ASL_TOK_VAR,
    ASL_TOK_IF,
    ASL_TOK_ELSE,
    ASL_TOK_FOR,
    ASL_TOK_WHILE,
    ASL_TOK_BREAK,
    ASL_TOK_CONTINUE,
    ASL_TOK_RETURN,
    ASL_TOK_FUNCTION,       // "=>" or contextual? kept for future lambdas

    // Operators
    ASL_TOK_PLUS, ASL_TOK_MINUS, ASL_TOK_STAR, ASL_TOK_SLASH, ASL_TOK_PERCENT,
    ASL_TOK_EQ, ASL_TOK_NE, ASL_TOK_LT, ASL_TOK_GT, ASL_TOK_LE, ASL_TOK_GE,
    ASL_TOK_ASSIGN,         // "="
    ASL_TOK_AND, ASL_TOK_OR, ASL_TOK_NOT,

    // Punctuation
    ASL_TOK_LPAREN, ASL_TOK_RPAREN,
    ASL_TOK_LBRACE, ASL_TOK_RBRACE,
    ASL_TOK_LBRACKET, ASL_TOK_RBRACKET,
    ASL_TOK_COMMA, ASL_TOK_SEMICOLON, ASL_TOK_COLON, ASL_TOK_DOT,
} asl_token_kind;
```

### Operator precedence (Pratt)

Higher binding power = tighter binding. `prefix` is the power applied when the operator is seen as a prefix; `infix` is the left-binding power required to consume it as an infix operator.

| Operator | Prefix BP | Infix BP |
|---|---|---|
| `()` call, `.` member | — | 100 |
| unary `+`, `-`, `!` | 90 | — |
| `*`, `/`, `%` | — | 80 |
| `+`, `-` | — | 70 |
| `<`, `>`, `<=`, `>=` | — | 60 |
| `==`, `!=` | — | 50 |
| `&&` | — | 40 |
| `\|\|` | — | 30 |
| `=` assignment | — | 10 |

These numbers are illustrative; the actual values can be adjusted. The parser's `expr(min_bp)` loop advances while the next token's left-binding power is >= `min_bp`.

### Entry points

```c
// Parse a complete function body into a block node.
// 'source' is the original document; 'body' is the span inside { ... };
// 'base_offset' is the absolute document offset of body.start.
asl_ast_node *asl_parse_body(anvil_document doc, anvl_slice body, usize base_offset);

// Parse a single expression in isolation (useful for tests/interpolation).
asl_ast_node *asl_parse_expression(anvil_document doc, anvl_slice expr, usize base_offset);

// Parse a single statement in isolation (useful for tests).
asl_ast_node *asl_parse_statement(anvil_document doc, anvl_slice stmt, usize base_offset);
```

## Tokenizer details

The tokenizer operates directly on the body slice. It is intentionally small because the grammar is small.

### Scalar literal reuse

Numeric, string, `true`, `false`, and `null` literals are recognized by the tokenizer but their value construction is delegated to the existing ANVL value-fragment parser. The tokenizer captures the source span, and the parser later asks the ANVL fragment parser to build an `anvl_value_t*` from that span.

### Interpolated strings

`$"hello {name} world {1+2}"` is tokenized as a single `ASL_TOK_INTERPOLATED` token carrying the full span. The parser later expands it into `ASL_EXPR_INTERPOLATED` by scanning the span for `{`/`}` pairs and recursively parsing each embedded expression.

### Comments and whitespace

- `//` line comments are skipped.
- `/* ... */` block comments are skipped.
- Whitespace separates tokens but is otherwise ignored.
- Newlines are whitespace; ASL is semicolon-terminated, not newline-sensitive.

### Keyword recognition

Keywords are recognized after an identifier is lexed, by table lookup. This avoids a large upfront keyword scan and keeps the tokenizer simple.

## Compiler (AST -> bytecode)

The compiler walks the AST and emits a flat instruction stream. It is a single pass over each function body. The output is an `asl_bytecode` object owned by the function descriptor.

### Output structure

```c
typedef struct asl_bytecode {
    byte *code;             // instruction stream
    usize count;            // number of bytes emitted
    usize capacity;

    anvil_value *constants; // constant pool (literals)
    usize constant_count;

    anvl_slice *spans;      // per-instruction source span, for error reporting
    usize *line_starts;     // optional line table for richer diagnostics
} asl_bytecode;
```

### Compilation strategy

- **Statements**: emit in order; declarations reserve a local slot.
- **Expressions**: emit in postfix/operand-stack order so the VM pushes values and operators consume them.
- **Control flow**: `if`, `while`, `for` emit jump instructions with placeholder offsets that are patched after the branch target is known.
- **Function calls**: emit arguments, then `OP_CALL` with arity; the VM resolves the callee at runtime.
- **Variables**: local variables are referenced by slot index; globals (module-level) are referenced by name and resolved through the registry.

## ASL VM

The VM executes a bytecode instruction stream using the arena-backed `Stack` from FR-011 as its operand stack, plus a linked frame chain.

### Frame layout

Each active function call has a frame:

```c
typedef struct asl_frame {
    struct asl_frame *parent;   // caller's frame
    asl_func_desc *func;        // function being executed
    usize return_ip;            // instruction pointer to resume in caller
    usize slot_offset;          // base offset in the operand stack for this frame's locals
    usize local_count;          // number of local slots
} asl_frame;
```

Locals live in the operand stack at `slot_offset .. slot_offset + local_count`. Arguments are the first N slots. New locals are appended above the arguments. Because the operand stack is a `Stack`, `slot_offset` is simply the stack depth at the moment the frame is entered.

### Operand stack discipline

- `OP_LOAD_LOCAL n` pushes the value at depth `slot_offset + n`.
- `OP_STORE_LOCAL n` pops a value into the slot at depth `slot_offset + n`.
- Binary operators pop two values and push the result.
- `OP_CALL` pops the callee and arguments, pushes a new frame, binds arguments to parameter slots, and begins executing the callee's bytecode.
- `OP_RETURN` pops the top value (or pushes `null` if none), tears down the frame, truncates the stack back to `slot_offset`, and pushes the return value.

### Control flow

- `OP_JUMP offset` — unconditional relative jump.
- `OP_JUMP_IF_FALSE offset` — pop value, jump if falsy.
- `OP_JUMP_IF_TRUE offset` — pop value, jump if truthy.
- `OP_BREAK` and `OP_CONTINUE` are placeholders patched to the appropriate loop exit/head during compilation.

### Exception channel

When the VM encounters an exception (undefined variable, arity mismatch, type error), it sets the context-local error state, records the current instruction's source span, and halts. It does not unwind the stack by default; the host can inspect the optional debug frame stack if enabled. The arena-bound operand stack is discarded along with the evaluation context.

## Error reporting contract

When the ANVL parser hands a function body to the ScriptEngine, it provides:

- `body` — the source span inside `{ ... }`;
- `params` — array of validated parameter-name slices;
- `doc` — the source document;
- `base_offset` — absolute document offset of `body.start`.

The ScriptEngine reports errors using spans relative to the original document by adding `base_offset` to node-relative spans.

## Dependencies on Sigma FRs

Both prerequisites are implemented as of 2026-09-07:

- **FR-2603-sigma-collections-007** — per-instance allocator override for `List`/`Collection` so variable stores, AST child arrays, and parser scratch space can be arena-bound.
- **FR-2603-sigma-collections-011** — arena-backed `Stack` primitive (built on `collection`) for the ASL VM operand stack, with `push`/`pop`/`peek`, `mark`/`restore`, and allocator override.

The ScriptEngine can now rely on arena-bound collections for its transient state.

## Related notes

- `notes/anvilscript-design.md` — high-level ASL design and shared structures.
- `notes/anvilscript-theoretical-sketches.md` — concrete examples of built-ins and host bindings.
- `FR/FR-2603-sigma-collections-007.md`
- `FR/FR-2603-sigma-collections-011.md`
