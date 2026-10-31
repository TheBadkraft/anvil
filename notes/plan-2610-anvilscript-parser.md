# Plan 2610: the AnvilScript parser, completed

**Status:** proposed, awaiting owner decisions (section 10). Design only; nothing implemented.
**Author:** ANVL AnvilScript Language Engineer (`anvl-scribe.badkraft`), 2026-10-30, on main d267faa.
**Branch:** `docs/anvilscript-parser-plan`.

The owner made AnvilScript the next hurdle: a fully functional AST parser, then the compiler, the VM and an
extensible DSL registry. This note is the plan for the first stage. It does not change `notes/anvilscript-design.md`;
section 2 lists what in the four design notes is outdated or contradictory and how each is resolved, and ticket
SR-2610-anvl-asl-003 marks them in place once the owner has answered.

Every claim about the design notes below was checked against the code on main
(`src/asl/asl_body_parser.c`, 650 lines; `src/core/asl_parser.c`, 348; `include/internal/asl_ast.h`;
`test/unit/test_asl_ast.c` ASLAST01 to 20; `test/unit/test_body_asl.c` ASL01 to 19).

## 0. Summary

- **Where we are.** The body parser handles `var`, plain and compound assignment, `++`/`--` as statements,
  `return expr;`, integer literals, identifiers, unary, the seven binary tiers and `( expr )`. Eight node kinds.
  Spans are set on three of them. Errors reuse the ANVL reader's codes. There is no dump, no corpus, no fuzzing.
- **Where we are going.** About 24 node kinds, every one with a span; a versioned, additive-only AST contract;
  a native-owned AST corpus with a canonical dump; fuzzing with invariant checks; specific 51xx error codes.
- **Size.** 26 tickets (6 existing, 20 new), roughly 34 engineer-days. Compiler work can start safely after
  **milestone M1** (about 19 days), which freezes every node the compiler needs for expressions, calls and control flow.
- **Owner decisions.** 13, in section 10. Each has a recommended default. Most affect one ticket; D1, D2 and D6
  shape the contract M1 freezes and should be answered before it starts.

## 1. The boundary, and what this plan owns

```
#!asl document --ANVL reader--> statements; function signature + body span --+
                                                                              v
                          asl_parse_body(body span, arena) --> asl_ast_node tree --> compiler --> bytecode --> VM
```

1. **ANVL reader (Core Engineer's).** Owns the document: shebang, header, includes, attributes, `name := value;`, and
   the *shape* of a function declaration `name @[attrs] (params) => { body };`. It hands the ScriptEngine the body span
   (between the braces) and the validated parameters. `src/core/asl_parser.c` implements the signature; it is
   ASL-owned code that sits in the reader's source list behind `WITH_ASL`.
2. **AnvilScript parser (mine).** `asl_parse_body` and its fragment siblings. A pure function: a byte span and an
   arena in, a tree or an error out. It never sees the document, the include graph or the type registry.
3. **Module-level ASL productions** (`import`, `vars`, `extend`). The design says the ANVL parser owns them; today it
   owns none of them (`import` and `vars` are only reserved words in the reader). They cannot be parsed from a function
   body, so they are neither the body parser's nor, today, the reader's. Owner decision **D8** says who adds the
   dispatch hook; the plan assumes a minimal `WITH_ASL` hook beside the existing function hook in `parse_statement`,
   reviewed by the Core Engineer.
4. **Not mine, and not in this plan:** name resolution (declare-before-assign, duplicates, arity), type checks, string
   escape decoding, numeric range checks, the registry, the compiler, the VM. The parser records enough for them
   (section 4) and guarantees nothing about them (section 4.6).

The boundary stays narrow in two directions: the parser takes `(span, arena)` and nothing else, and its tree
references the source buffer only through spans. Section 5 explains why this is also what makes a WebAssembly build
cheap.

## 2. The design notes: outdated and contradictory passages

"Owner" marks a row that needs an owner decision (section 10). Everything else is resolved by this plan, and
SR-2610-anvl-asl-003 marks the notes in place afterwards. "Design", "Digest", "Engine" and "Sketches" below are the
four notes in `notes/`.

| # | Passage | Problem | Resolution |
|---|---|---|---|
| 1 | Digest, intro and "Status": "design only, nothing implemented" | Outdated since the signature and body parser landed (Design, "Implementation"). | Mark superseded in place. |
| 2 | Design, Decision 15: file extension `.anvs`; Design, Decision 10 and the dialect matrix: `.asl` | Two extensions. The code knows only `.asl` (extension hint, `FORMAT.md`, "The dialect line"). `.anvs` is also used for built-in library files (`math.anvs`). | **Owner D13.** Recommend `.asl` for documents (implemented), `.anvs` stays the name for shipped library sources until the registry design says otherwise. |
| 3 | Design, EBNF: `identifier = alpha {alpha digit _}` | Omits a leading `_`. The code and the reader accept `_` as a start (`Source.is_identifier_start`). | Follow the code: `[A-Za-z_][A-Za-z0-9_]*`, ASCII only. Grammar change recorded (D6). |
| 4 | Design, EBNF: `decimal = integer "." integer`, no exponent | ANVL's numeric grammar is `digits(.digits)?([eE][+-]?digits)?` with errors for `5.`, `1e` (5008, 5009). A body that accepts less than the reader is a trap. | Body numeric literal = ANVL grammar without the leading `-` (unary minus is an operator). One literal kind `NUMERIC`: the reader has no int/float split either, only text. FR-2610-anvl-asl-001's "float" means the decimal and exponent spellings. |
| 5 | Design, EBNF: `array = "[" [value ...] "]"` allows empty; AML rejects `[]` | The same spelling means different things in the two dialects. | **Owner D4.** Recommend: `[]` and `{}` are legal in bodies (`var xs = [];` is the first thing a script wants); tuples keep two or more elements. |
| 6 | Design, EBNF: array elements are `value`, tuple elements are `expr`, object entries are `value` | `[x, y]` with variables would be illegal while `(x, y)` is legal. And a bare word is a string in AML but a variable in a body. | **Owner D5.** Recommend: elements and entry values are `expr`; a bare word in a body is always a variable reference. |
| 7 | Design, EBNF: `array(...)` and `tuple(...)` as alternative spellings, Sketches: `array.length(a)`, `tuple.length(t)` | If `array` and `tuple` were keywords, the built-in namespaces `array.*` and `tuple.*` could not be written. | **Owner D3.** Recommend: not keywords. `array(1, 2)` is an ordinary call; the registry lowers it. No sugar node. |
| 8 | Design, EBNF: `postfix = primary ["(" args ")"] \| qualified_call`; `xs.first_where(f)` in the extension-method text | `postfix` is not recursive, so `f(x)(y)` and `xs.first_where(f).second(g)` cannot parse. `math.abs(x)` and `xs.first_where(f)` are the same syntax; only scope tells namespace from receiver. | **Owner D2.** Recommend: `postfix = primary { "(" args ")" \| "." identifier }`, one `MEMBER` node, no `QUALIFIED_NAME` node. The compiler resolves a name chain. |
| 9 | Design, EBNF: `for` step is `expr`; `var_decl` includes its own `;` | `i++` is statement-only today (not an expression), and a `var` init would carry a double `;`. | Add `simple_stmt` (no terminator) for `for` init and step. Grammar addition, **D6**. |
| 10 | Design, EBNF: no `else if`; compound assignment and `++`/`--` absent | The code already has the compound family (ASLAST03 to 05). `else if` is universal in C-family code. | Add both to the grammar (**D6**). Compound forms stay desugared to `ASSIGN`. |
| 11 | Design, Decision 3 ("user-defined lambda functions" in the MVP) and the closure example; Sketches: `xs.first_where(x => x % 2 == 0)`, `pred(v)` | The EBNF has no lambda production. Engine lists `ASL_TOK_FUNCTION` "kept for future lambdas". | **Owner D7.** Recommend: parse lambdas, but late (FR-2610-anvl-asl-018), after M2. Closures remain post-MVP. |
| 12 | Design, EBNF: `extend_decl` listed with body grammar; kind `ASL_STMT_EXTEND`; "syntax is not decided" | It contains `function_decl`s, which are module-level, so it is unreachable from `stmt`. And the text calls the syntax undecided while the EBNF fixes it. | `extend` is a module-level declaration, not a body statement. `asl_parse_body` never returns `EXTEND`. **Owner D8, D9.** |
| 13 | FR-2610-anvl-asl-002 text lists `import` as a missing body production; Design: `import` is a header construct | Contradiction between my own ticket and the design. | The design is right. `import` and `vars` are module-level (**D8, D9**); inside a body they are an error. FR-2610-anvl-asl-002 is split (section 7). |
| 14 | Design, "Function semantics": `$foo(1, 2, 3)`, `$math.abs(-5)`; Design, grammar notes: `$` appears only in interpolation inside bodies | Two accounts of `$`. Digest follows the second. | In bodies, `$` introduces `$"` and nothing else (`$x` in a body is an error). `$name` at module level is the reader's and is not touched. Mark the first account outdated. |
| 15 | Design, "Implementation" #1: `ASL_STMT_EXPR` "stays unneeded"; EBNF `expr_stmt` | A call statement needs it. | Needed from FR-2610-anvl-asl-008. |
| 16 | Design, "Runtime AST Nodes": `literal { anvl_value_t *value }`, `binary.op` as a slice; code: embedded `anvl_value`, comment "revisit when the operator set grows" | The operator set has grown. The literal drags `anvl_value_t` (a reader type) into the contract. | AST contract v1 (section 4): `op_kind` beside the op span; literal is `{lit_kind, text span}`. **AST change.** |
| 17 | Design, `asl_ast.h` header comment ("5 kinds", "bare identifier or integer literal") | Stale. | Fixed by the AST contract ticket. |
| 18 | Design, "Parser qualities"; Engine, "Parser qualities" 3 and 4: "span on every node", "panic-mode error recovery"; Engine entry points `asl_parse_body(doc, body, base_offset)` | Spans are set on 3 of 8 kinds (BR-2610-anvl-asl-003). Recovery does not exist and the reader reports the first error only. Real signature is `(body, arena, &err)`. | Spans: BR-2610-anvl-asl-003. Recovery: **resolved: first error only**, like the reader; recovery is a tooling feature, post-compiler. Entry points: FR-2610-anvl-asl-003. |
| 19 | Engine, "Operator precedence (Pratt)" table, including `=` at 10 and `()`/`.` at 100 | The implementation is precedence climbing over seven stacked tiers; assignment is a statement, not an expression. | Table is illustrative and outdated. Keep the tiers; postfix sits above unary; assignment stays a statement (`a = b = c` and `y = x++` are not grammar). |
| 20 | Engine, "Token kinds" (`ASL_TOK_*`) and "Minimal tokenizer" | No token layer was built; the parser is scannerless over bytes. | Stay scannerless. Mark outdated. Lookahead is by byte peek; no backtracking is needed anywhere (section 3.4). |
| 21 | Engine, "Scalar literal reuse": literals built by the ANVL value-fragment parser | The body parser cannot call the reader (it is independent of `anvl_source` by design) and the AST should not depend on `anvl_value_t`. | Literal text is a span; the *grammar* is shared by being specified identically (section 3.2) and held by corpus cases, not by shared code. |
| 22 | `asl_ast.h`: "independent of the original document's `anvl_source`... parsed long after that source may be gone" | True of the `anvl_source` struct, false of the bytes: every name and literal is a span into the document buffer. | Contract (section 4.5): the buffer must outlive the tree. The document owns the buffer for as long as its functions exist, which is already the case. |
| 23 | Engine, "Source-position metadata": spans relative to the body slice plus `base_offset` | Spans are pointer slices into the document buffer; the absolute offset is `start - data`. | Contract: a node's offset in the document is `span.start - span.data`; relative to the body, `span.start - body.start`. No `base_offset` parameter. |
| 24 | Design, 5104 and 5105 `BREAK_OUTSIDE_LOOP`, `CONTINUE_OUTSIDE_LOOP`; "control flow is not an exception" | The two codes exist but nothing emits them; the text implies a runtime check. | The parser emits them (loop context is a parse-time fact). No tree contains `break`/`continue` outside a loop. |
| 25 | Design, "Variable scope": declare-before-assign enforcement "undecided" | A resolver or compiler pass. | Not the parser's. The parser accepts `x = 1;` whether or not `x` exists. Recorded in section 4.6. |
| 26 | Design, "Debug and trace modes": `#debug off;` pragma in source | Not in the grammar; `#` starts nothing in a body. | Out of parser scope: a document option (`anvil_document_set_debug`), not syntax. Mark deferred. |
| 27 | Design, "Typed declarations" (2026-10-24): `var x: T = ...`, `name: T` | Decided, unscheduled, position unresolved. | Not in this plan. Extension point: `VAR_DECL` gets an optional `type_name` span later, additive. |
| 28 | Design, "Implementation" #5: "more literal kinds in `asl_body_parse_expr`" | `true` currently parses as an identifier; `null` too. | FR-2610-anvl-asl-001. After it, `true`, `false`, `null` are keywords and literals. |
| 29 | Error codes: body parser returns 5001-range and 4xxx codes (`EXPECTED_VALUE`, `EXPECTED_ASSIGN`, `UNTERMINATED_STATEMENT`, `UNEXPECTED_TOKEN`, `EXPECTED_IDENTIFIER`); 5101 `ANVL_ERR_ASL_PARSE_ERROR` "ASL script parse failure" | Reused reader codes are aliases for a different module's reasons; 5101 is a catch-all. | Section 8. FR-2610-anvl-asl-003 replaces every reuse. 5101 stays defined and is never returned by the parser. Not an owner decision, but the Principal Engineer is told because existing test assertions change. |

## 3. The complete grammar

The grammar is the design EBNF, reconciled with the ANVL reader and extended only where section 2 says so. The
grammar is not bent to suit the implementation: wherever the implementation lacks a production, the production
stands and a ticket builds it. Additions beyond the design EBNF are tagged **[D6]** and need owner approval; the
recommended default for every one is "accept".

### 3.1 What is shared with the ANVL reader, and what is not

| Item | ANVL reader | Function body |
|---|---|---|
| Identifier | `[A-Za-z_][A-Za-z0-9_]*` | same |
| Keywords refused as names | `include import vars true false null` (lower case, whole word) | those, plus `var if else for while break continue return extend` (**D1**) |
| Numeric | `digits(.digits)?([eE][+-]?digits)?` with a leading `-`; `5.`, `1e` are errors | same, **without** the `-` (unary operator); a number followed by an identifier character is an error, because there are no bare words in a body |
| String | `"` ... `"`, backslash escapes, content is a span | same; the quotes are not part of the text span; escapes are not validated or decoded by the parser |
| Blob | `@tag`...`` ` ``, tag optional, max 31 identifier characters | same (**D12**) |
| Comments | `//` and `/* */`; unterminated `/*` is an error | same; unterminated is `ANVL_ERR_ASL_UNTERMINATED_BLOCK_COMMENT` |
| Bare word | a string value | **does not exist**; a bare word is a variable reference |
| `;` placement | must stay on the value's line | free whitespace and newlines (the Engine note says so; a body is not newline-sensitive) |
| `:=` | statement assignment | only inside object literals |
| Empty `[]` | invalid | valid (**D4**) |
| `$name` | resolve-once alias | not a body construct; `$` only before `"` |
| Attributes `@[...]` | on any statement | none inside a body; `@` starts a blob |

### 3.2 Lexical

```ebnf
identifier   = ident_start , { ident_start | digit } ;         (* ASCII; a keyword is not an identifier *)
ident_start  = "A".."Z" | "a".."z" | "_" ;
keyword      = "var" | "if" | "else" | "for" | "while" | "break" | "continue" | "return" | "extend"
             | "include" | "import" | "vars" | "true" | "false" | "null" ;
numeric      = digits , [ "." , digits ] , [ ( "e" | "E" ) , [ "+" | "-" ] , digits ] ;
string       = '"' , { any_byte_but_quote_and_backslash | "\" , any_byte } , '"' ;
blob         = "@" , [ tag ] , "`" , { any_byte_but_backtick } , "`" ;     (* D12 *)
comment      = "//" , { not_newline } | "/*" , { any } , "*/" ;
```

Offsets and columns are UTF-8 bytes. Non-ASCII bytes are legal only inside strings, blobs and comments; anywhere else
they are `ANVL_ERR_ASL_UNEXPECTED_CHARACTER`.

### 3.3 Productions and node kinds

| Production | AST node kind | Ticket |
|---|---|---|
| `body = { stmt }` | `BLOCK` (the root) | exists |
| `block = "{" , { stmt } , "}"` | `BLOCK` | FR-2610-anvl-asl-009 |
| `var_decl = "var" , identifier , "=" , expr , ";"` | `VAR_DECL` | exists |
| `assignment = identifier , ( "=" \| "+=" \| "-=" \| "*=" \| "/=" \| "%=" ) , expr` | `ASSIGN` (compound forms desugar to `ASSIGN` over a `BINARY`) | exists |
| `step = identifier , ( "++" \| "--" )` **[D6]** | `ASSIGN` over `BINARY` with a synthetic literal 1 | exists |
| `return_stmt = "return" , [ expr ] , ";"` | `RETURN` (value may be NULL) | BR-2610-anvl-asl-004 |
| `if_stmt = "if" , "(" , expr , ")" , block , [ "else" , ( block \| if_stmt ) ]` (the `if_stmt` alternative is **[D6]**) | `IF` | FR-2610-anvl-asl-009 |
| `while_stmt = "while" , "(" , expr , ")" , block` | `WHILE` | FR-2610-anvl-asl-010 |
| `for_stmt = "for" , "(" , [ for_init ] , ";" , [ expr ] , ";" , [ simple_stmt ] , ")" , block` | `FOR` | FR-2610-anvl-asl-011 |
| `for_init = "var" , identifier , "=" , expr \| simple_stmt` **[D6]** | `VAR_DECL`, `ASSIGN` or `EXPR` | FR-2610-anvl-asl-011 |
| `simple_stmt = assignment \| step \| expr` (no terminator) **[D6]** | `ASSIGN`, or `EXPR` | FR-2610-anvl-asl-011 |
| `break_stmt = "break" , ";"` | `BREAK` | FR-2610-anvl-asl-010 |
| `continue_stmt = "continue" , ";"` | `CONTINUE` | FR-2610-anvl-asl-010 |
| `expr_stmt = expr , ";"` | `EXPR` | FR-2610-anvl-asl-008 |
| `expr = logical_or` and the seven tiers below | `BINARY` | exists |
| `logical_or = logical_and , { "\|\|" , logical_and }` | `BINARY` | exists |
| `logical_and = equality , { "&&" , equality }` | `BINARY` | exists |
| `equality = comparison , { ( "==" \| "!=" ) , comparison }` | `BINARY` | exists |
| `comparison = additive , { ( "<=" \| ">=" \| "<" \| ">" ) , additive }` | `BINARY` | exists |
| `additive = multiplicative , { ( "+" \| "-" ) , multiplicative }` | `BINARY` | exists |
| `multiplicative = unary , { ( "*" \| "/" \| "%" ) , unary }` | `BINARY` | exists |
| `unary = ( "-" \| "+" \| "!" ) , unary \| postfix` | `UNARY` | exists |
| `postfix = primary , { "(" , [ arg_list ] , ")" \| "." , identifier }` (**D2**) | `CALL`, `MEMBER` | FR-2610-anvl-asl-008 |
| `arg_list = expr , { "," , expr }` | children of `CALL` | FR-2610-anvl-asl-008 |
| `primary = literal \| identifier \| "(" , expr , ")" \| tuple \| array \| object \| interpolated \| lambda` | see below | |
| `"(" , expr , ")"` | **no node**; the inner node is returned | exists |
| `literal = numeric \| string \| "true" \| "false" \| "null" \| blob` | `LITERAL` with `lit_kind` NUMERIC, STRING, BOOL, NULL, BLOB | FR-2610-anvl-asl-001, FR-2610-anvl-asl-007 |
| `identifier` | `IDENTIFIER` | exists |
| `array = "[" , [ expr , { "," , expr } ] , "]"` (empty: **D4**; elements as `expr`: **D5**) | `ARRAY` | FR-2610-anvl-asl-012 |
| `tuple = "(" , expr , "," , expr , { "," , expr } , ")"` | `TUPLE` | FR-2610-anvl-asl-012 |
| `object = "{" , { identifier , ":=" , expr , ";" } , "}"` (empty: **D4**) | `OBJECT` | FR-2610-anvl-asl-013 |
| `interpolated = "$" , '"' , { text \| "{" , expr , "}" } , '"'` | `INTERPOLATED` (segments) | FR-2610-anvl-asl-014 |
| `lambda = ( identifier \| "(" , [ identifier , { "," , identifier } ] , ")" ) , "=>" , ( expr \| block )` (**D7**) | `LAMBDA` | FR-2610-anvl-asl-018 |
| module level: `extend_decl = "extend" , identifier , "{" , { function_decl } , "}"` (**D8, D9**) | `ANVL_STMT_EXTEND` at the reader layer; never an `asl_ast_node` from `asl_parse_body` | FR-2610-anvl-asl-016 |
| module level: `import_decl = "import" , string , ";"` (**D8, D9**) | reader-layer statement | FR-2610-anvl-asl-017 |
| module level: `vars_block = "vars" , "{" , { identifier , ":=" , literal , ";" } , "}" , ";"` (**D8, D9**) | reader-layer statement | FR-2610-anvl-asl-017 |
| `function_decl` (signature) | `ANVL_STMT_FUNCTION`; body held as a span until `asl_parse_body` runs | exists |

Statement forms that look possible but are **not** grammar, each rejected with a specific code: the empty statement
`;`; a bare block `{ ... }` as a statement (a `{` at statement start is not an object literal either, see section 3.4);
assignment as an expression (`a = b = c`, `y = x++`); assignment to anything but a name (`xs[0] = 1`, `a.b = 1`);
indexing `xs[i]` (the design uses `array.at`); member access as a value is allowed syntactically (`a.b`) and left to the
compiler; ternary `?:`; bitwise operators; a trailing comma in an argument, array or tuple list.

### 3.4 Parsing without backtracking

All decisions use one token of lookahead plus the shape of what was just parsed:

- **Statement start.** A keyword selects its statement. Anything else parses an `expr`; if the result is a bare
  `IDENTIFIER` and the next bytes are `=`, a compound operator, `++` or `--`, it is an assignment. If the result is
  any other node followed by an assignment operator, `ANVL_ERR_ASL_INVALID_ASSIGN_TARGET`. Otherwise it is an `EXPR`
  statement and a `;` is required.
- **`(`.** Parse one `expr`. A `,` makes a tuple; a `)` is grouping. For lambdas (D7), if every element so far is a
  bare `IDENTIFIER` and `=>` follows the `)`, the group is rewritten as the parameter list; the elements were
  already parsed as identifiers, so nothing is re-read.
- **`x =>`** needs a lookahead of one token past an identifier primary.
- **`{`.** In expression position an object literal; after an `if`/`while`/`for` header or a lambda arrow a block;
  at statement start an error. The position decides; no guess is needed.
- **`$`** is valid only immediately before `"`.

### 3.5 Recursion

Every recursive production takes a nesting level (`asl_body_enter_nesting`, `ASL_MAX_NESTING_DEPTH` = 256): `( )`
grouping, prefix unary, call argument lists, array/tuple/object literals, nested blocks, interpolation expressions,
lambda bodies. Postfix chains, binary chains and statement lists are loops. Loops build deep *trees* without deep
*parse recursion*; section 4.4 bounds the tree.

## 4. The AST contract

This is what the compiler may rely on. It is versioned (`ASL_AST_VERSION`, 1) and **additive-only after M1**: a new
kind or a new optional field is a minor change; a change to the meaning, name or number of anything that exists bumps
`ASL_AST_VERSION`, the dump version and the corpus together, and is announced in the changelog. This is what makes
compiler work start-safe: the Runtime Engineer builds against a known-good commit, and a parser ticket can add
productions without moving what is already there.

### 4.1 Node header

Every node:

| Field | Meaning |
|---|---|
| `kind` | `asl_ast_kind`, **explicit numbers, never reused** (the current implicit 0 to 7 are renumbered once, before M1) |
| `flags` | bit 0 `SYNTHETIC`: the node was created by desugaring (`x++`, `x += e`); its span is the span of the token that caused it |
| `height` | 1 plus the largest child height; a leaf is 1 |
| `span` | `anvl_slice` (pointers into the source buffer) |

### 4.2 Kinds and fields

Numbers in brackets are the proposed fixed values (gaps leave room; parsers add, never renumber).

| Kind | Fields | Notes |
|---|---|---|
| `BLOCK` [1] | `stmts[]`, `count` | Root of every body; also every `{ }` body. Children never NULL. `count` may be 0. |
| `VAR_DECL` [2] | `name` span, `value` | `value` never NULL. |
| `ASSIGN` [3] | `name` span, `value` | Target is always a name. Compound forms and `++`/`--` are already desugared: `x += e` is `ASSIGN(x, BINARY(+, IDENTIFIER x, e))` with `SYNTHETIC` on the `BINARY` and the identifier copy. |
| `IF` [4] | `cond`, `then_block`, `else_stmt` | `else_stmt` is NULL, a `BLOCK`, or an `IF`. |
| `FOR` [5] | `init`, `cond`, `step`, `body` | `init` NULL or `VAR_DECL`/`ASSIGN`/`EXPR`; `cond` NULL or expression; `step` NULL or `ASSIGN`/`EXPR`; `body` a `BLOCK`. |
| `WHILE` [6] | `cond`, `body` | `body` a `BLOCK`. |
| `BREAK` [7], `CONTINUE` [8] | none | Appear only inside a loop body, never across a lambda body. |
| `RETURN` [9] | `value` | NULL for `return;`. |
| `EXPR` [10] | `expr` | |
| `LITERAL` [20] | `lit_kind`, `text` span | `lit_kind` is NUMERIC, STRING, BOOL, NULL or BLOB. `text` for a string excludes the quotes and keeps escapes raw; for a blob excludes tag and backticks; the tag is a second span; BOOL/NULL text is the keyword. A `SYNTHETIC` literal may have text in static storage; its `span` is still in range. |
| `IDENTIFIER` [21] | `name` span | |
| `BINARY` [22] | `op_kind`, `op` span, `left`, `right` | `op_kind` is `asl_op`: ADD SUB MUL DIV MOD EQ NE LT GT LE GE AND OR; explicit numbers. |
| `UNARY` [23] | `op_kind`, `op` span, `operand` | NEG, POS, NOT. |
| `CALL` [24] | `callee`, `args[]`, `arg_count` | `callee` is any expression. |
| `MEMBER` [25] | `object`, `name` span | `a.b.c` is `MEMBER(MEMBER(a, b), c)`. (D2) |
| `ARRAY` [26], `TUPLE` [27] | `elements[]`, `count` | `TUPLE` count is at least 2. |
| `OBJECT` [28] | `keys[]` spans, `values[]`, `count` | Keys unique. |
| `INTERPOLATED` [29] | `segments[]`, `count` | Each segment: STRING with a `text` span, or EXPR with a node. Never two adjacent STRING segments. |
| `LAMBDA` [30] | `params[]` spans, `count`, `body` | Reserved; produced only if D7 is accepted. `body` is a `BLOCK` or an expression. |
| `EXTEND` [11] | reserved | Not produced by `asl_parse_body` (D8). The number is held so the enum matches the design. |

`QUALIFIED_NAME` from the design sketch is retired (D2). The design's `ASL_EXPR_QUALIFIED_NAME` slot is not reused.

### 4.3 Spans

- A span is half-open, from the first byte of the first token the construct consumed to one past the last byte of
  the last token. Offsets are bytes.
- A statement's span includes its terminating `;`; a block's span includes both braces; a `for`/`if`/`while`
  span runs to the closing brace of its last block.
- Grouping parentheses are not a node. The inner node keeps its own span, which excludes the parentheses; the parent's
  span begins at the first token it consumed, so `(a + b) * c` has a `BINARY(*)` span starting at the `(`.
- Children lie within the parent's span and do not overlap; siblings are in source order.
- The root `BLOCK`'s span is the body slice itself.
- Every span, including a synthetic node's, lies within `[body.start, body.end]`. Offset of a node in the document is
  `span.start - span.data`; relative to the body, `span.start - body.start`. `span.data` is the document buffer start,
  as the current code already sets it.

### 4.4 Ownership, lifetime, limits

- All nodes and child arrays are allocated from the caller's `bump_allocator`. Nothing in the tree is individually
  freed. The parser's heap scratch is released on every return path, success or error (checked under valgrind in
  every ticket).
- Nodes are immutable after the parse returns. Concurrent reads are safe.
- Names, literals, operator tokens and segment texts are spans into the source buffer. **The buffer must outlive the
  tree**, and anything that outlives the buffer (a compiled function, `.anvlo`) must copy what it needs. The tree
  never copies text.
- On error the function returns NULL and an error (section 8); no partial tree is visible. Nodes already allocated
  stay in the arena until the arena is reclaimed.
- Two bounds hold for every tree returned: parse nesting at most `ASL_MAX_NESTING_DEPTH` (256), and tree height at
  most `ASL_MAX_TREE_HEIGHT` (proposed 1024, **D11**). The second bounds a compiler's recursion over a tree that a
  *loop* built (`a + a + a + ...`, `a.b.c.d...`).

### 4.5 What the compiler can assume (checked by an AST validator in every test and in fuzzing)

1. Every span is in range, children nest inside parents, siblings are ordered and disjoint.
2. Every non-optional child is non-NULL. Optional ones are listed in section 4.2.
3. `break`/`continue` occur only inside a loop body and not across a lambda.
4. No name (variable, parameter, member, object key) is a keyword.
5. A `LITERAL` NUMERIC text matches the numeric grammar; a STRING has balanced quotes; a BLOB has a closing backtick.
6. `TUPLE.count >= 2`; `OBJECT` keys are unique; `INTERPOLATED` segments alternate.
7. `height` is correct and at most the bound. Nesting is at most the bound.
8. Compound assignment and `++`/`--` never appear as such; only `ASSIGN`.

### 4.6 What the parser does not guarantee

Declared-before-use; duplicate `var` in a scope; that a callee exists or its arity; types; that `a.b` is a namespace
or a receiver; string escape validity or decoding (the reader states "standard backslash escapes" and does not decode
them; the compiler should reuse the reader's decoder when one exists, which is an open question for the Runtime
Engineer, not decided here); numeric range or precision (text is exact, `anvil_numeric_t` rules apply later);
unreachable code; whether `return` appears on every path.

### 4.7 Entry points

```c
asl_ast_node asl_parse_body(anvl_slice body, bump_allocator arena, asl_parse_error *err);
asl_ast_node asl_parse_expression(anvl_slice text, bump_allocator arena, asl_parse_error *err);
asl_ast_node asl_parse_statement(anvl_slice text, bump_allocator arena, asl_parse_error *err);
typedef struct { anvl_err_code code; usize offset; } asl_parse_error;   /* offset: bytes from text start */
```

Fragments refuse trailing input. This replaces the Engine note's `(doc, body, base_offset)` signatures. The current
`asl_parse_body(body, arena, &code)` changes in FR-2610-anvl-asl-003; it is internal, and the Runtime Engineer has
nothing built on it yet.

## 5. The VM boundary, from day one

`notes/proposal-2610-boundary-emitter.md` section 6 (agreed in part B of that note): bytecode is loaded into
WebAssembly linear memory once; host calls pass integers and offsets; values cross as handles; offsets are UTF-8
bytes. The parser serves it in six ways, all cheap now and expensive later:

1. **A pure function.** `(span, arena) -> tree`. No globals, no callbacks, no I/O, no registry. A WebAssembly build
   needs only the arena's allocator imported.
2. **Pointer-free in meaning.** Every cross-reference is a child pointer inside one arena or a span into the source.
   A tree can be written as a flat preorder array (kind, flags, span start, span end, up to six more words per node, in
   the style of anvil.js's 8-int node) by walking it, with no side tables. Explicit, stable kind numbers (4.1) make that
   encoding stable; this is the "one canonical node-tree format" the proposal asks for, applied to AnvilScript.
3. **No reader types in the tree.** `LITERAL` is `{lit_kind, span}`, not `anvl_value_t`; the parser stops calling
   `Source.*` classifiers (FR-2610-anvl-asl-004). The parser then links without the reader.
4. **Spans, never strings.** Same as the reader. Offsets are bytes, so native, WebAssembly and JS agree.
5. **Deterministic bytes.** The dump (section 6) is byte-identical on every run, so a JS or WebAssembly parser, if
   one is ever built, is held to it exactly as anvil.js is held to the reader corpus.
6. **Compiler input is the tree; VM input is not.** Nothing here makes the VM depend on the AST. The compiler
   (Runtime Engineer's) emits bytecode with per-instruction source offsets taken from node spans; that is why every
   node needs a span (BR-2610-anvl-asl-003) and why spans are plain offsets.

The `.anvlo` `nodes` section and the boundary emitter's output are the same tree in the same flat encoding; this plan
does not build either, but nothing in it prevents them.

## 6. Holding the parser to a corpus

Same method as the reader, in a separate corpus, because the reader corpus size is pinned downstream (anvil.js
checks 1,867 cases at native acb4602).

### 6.1 The dump

`anvil-dump --asl [--expr | --doc] <file | ->` prints `anvil-asl-dump 1`. Specified in `conformance/ASL-FORMAT.md`,
which follows `conformance/FORMAT.md`'s lexical rules (ASCII, `\n`, `key=value`, quoted strings, `start-end` spans,
two-space indent). Modes: `body` (input is a function body), `expr`, `doc` (a whole `#!asl` document; each function
record is followed by its parsed body, offsets relative to the document). Example for the input
`var x = 1 + 2;\nreturn x;`:

```
anvil-asl-dump 1
mode body
input-bytes 24
status ok
BLOCK span=0-24 height=4 count=2
  VAR_DECL span=0-14 height=3 name=4-5
    BINARY span=8-13 height=2 op=ADD optext=10-11
      LITERAL span=8-9 height=1 lit=NUMERIC text=8-9
      LITERAL span=12-13 height=1 lit=NUMERIC text=12-13
  RETURN span=15-24 height=2
    IDENTIFIER span=22-23 height=1 name=22-23
```

On error: `status error`, then one line
`error code=5111 name=ANVL_ERR_ASL_EXPECTED_SEMICOLON offset=9 line=1 column=10 message="..."`, and no tree (the same
rule as the reader: a failed parse defines nothing but the error). Kind and operator names are symbolic so a diff
is readable when the implementation under test is broken. The dump tool validates the invariants of 4.5 on every `ok`
parse and exits non-zero with an `invariant` line when one fails, so the fuzzer sees it.

### 6.2 The case set

`conformance/asl/cases/<group>/<name>.{body,expr,asl}` with a `.dump` beside each; native owns the dumps and
`make update-conformance-asl` regenerates them deliberately, never as a side effect. Groups: one per production,
`errors/` (one case per 51xx parse code at least), `limits/` (nesting 255/256/257, height, long chains, long
literals), `lexical/` (comments, numbers, strings, blobs, bytes over 0x7f), `desugar/` (compound, `++`), `spans/`.
Hand-written cases come with each ticket (a ticket is not done without its cases). A deterministic generator
(`conformance/seed/gen_asl.py`, FR-2610-anvl-asl-006) adds breadth, and a coverage script fails when a production
or a parse error code has no case. Target: 600 to 1,000 cases at M2.

### 6.3 Gates

- `make check-asl-conformance` (WITH_ASL=1): dump every case, compare with the checked-in dump.
- `check-asl` umbrella (SR-2610-anvl-asl-002): the ASL unit suites, the conformance check, a valgrind run over the
  corpus, a short fuzz run. Added to the Verification Engineer's list.
- A prove script, like `conformance/fuzz/prove.sh`: alter one parser rule in a scratch copy, require
  `check-asl-conformance` to go RED.
- `WITH_ASL=0` builds are untouched; `check-reader-only`, `check-exports`, `check-tsan`, `check-version` keep passing
  both ways.

### 6.4 Fuzzing (FR-2610-anvl-asl-020)

Reuses the batch protocol and `fuzz.py`'s minimiser. Inputs: a grammar-aware ASL generator with a hard cap on case
size (a case is at most 64 KB; a chain generator is capped by length, not by memory), a byte mutator over the ASL
corpus, **every prefix of every corpus case** (truncation is where unterminated-construct bugs live), and the nesting
and height stressors at 255/256/257 and at the height bound. Checks: no crash, no hang (timeout), no output on
stderr, two runs agree byte for byte, the 4.5 invariants hold, valgrind clean on a sample. The second side `--b` is
native itself until a JS or WebAssembly parser exists; the harness is built so that side is a command line.
Fuzz and corpus generation run under the memory cap (STANDARDS section 8):
`systemd-run --user --scope -p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=<s> -- <command>`, foreground, in
bounded chunks, streaming results.

## 7. Tickets, order and size

Sizes: S about half a day, M about a day and a half, L about three days. Every ticket is independently testable and
mergeable behind `WITH_ASL`, follows STANDARDS section 3 (full test set first, genuine RED, one requirement per test)
and ends with its corpus cases (after FR-2610-anvl-asl-005), its notes update and its changelog entry. New suites are
split out of `test_asl_ast.c` (1,024 lines) by topic: `test_asl_literals.c`, `test_asl_stmt.c`,
`test_asl_expr.c`, `test_asl_values.c`.

Existing tickets are marked **(existing)**. FR-2610-anvl-asl-002 is the umbrella and is split across the tickets
under "Phase 2" to "Phase 4"; it stays open as the tracking ticket and depends on its children.

### Phase 0: foundation (no new grammar)

| # | Ticket | Size | Depends on | Notes |
|---|---|---|---|---|
| 1 | FR-2610-anvl-asl-003 error position, fragment entry points, specific 51xx codes | M | | Migrates every reused 40xx/50xx code. Section 8. |
| 2 | FR-2610-anvl-asl-004 AST contract v1 | M | | Kind numbers, `op_kind`, literal `{lit_kind, span}`, `SYNTHETIC`, `height`, `ASL_AST_VERSION`, no `Source.*` calls, fixes the stale header comments. Records the AST change. |
| 3 | BR-2610-anvl-asl-005 unterminated `/*` **(existing)** | S | 1 | |
| 4 | BR-2610-anvl-asl-004 `return;` **(existing)** | S | 1 | `RETURN.value` may be NULL. |
| 5 | BR-2610-anvl-asl-003 `span` on every node **(existing)** | M | 2 | The span rule (4.3) and the range validator. Includes synthetic nodes. |
| 6 | BR-2610-anvl-asl-002 keywords refused as names **(existing)** | S | 1, D1 | `var`, assignment targets, identifiers in expressions. One keyword table (shared later with FR-2610-anvl-asl-015). |
| 7 | FR-2610-anvl-asl-005 AST dump mode and corpus harness | L | 5 | `anvil-dump --asl`, `ASL-FORMAT.md`, `check-asl-conformance`, prove script, backfill for the existing kinds. |

### Phase 1: literals

| # | Ticket | Size | Depends on | Notes |
|---|---|---|---|---|
| 8 | FR-2610-anvl-asl-001 numeric (decimal, exponent), string, bool, null literals **(existing)** | M | 2, 6 | `true`/`false`/`null` stop parsing as identifiers. Errors for `5.`, `1e`, `12ab`, unterminated string. |
| 9 | FR-2610-anvl-asl-007 blob literals | S | 8, D12 | |

### Phase 2: expressions and statements

| # | Ticket | Size | Depends on | Notes |
|---|---|---|---|---|
| 10 | FR-2610-anvl-asl-008 expression statements, calls, member access | L | 8 | `EXPR`, `CALL`, `MEMBER`, invalid assignment target. |
| 11 | FR-2610-anvl-asl-009 `if` / `else` / `else if` | M | 10 | `IF`; nested blocks take nesting levels. |
| 12 | FR-2610-anvl-asl-010 `while`, `break`, `continue` | M | 11 | Loop context; 5104 and 5105. |
| 13 | FR-2610-anvl-asl-011 `for` | M | 12 | `simple_stmt`. |
| 14 | FR-2610-anvl-asl-019 tree-height bound | S | 2, 10 | `ASL_MAX_TREE_HEIGHT`. |

**Milestone M1: the compiler can start.** After tickets 1 to 14 the parser produces every node a compiler needs for
scalar expressions, calls, member chains, assignment and all control flow, with spans, bounds and the dump. The
Principal Engineer picks a known-good commit; kinds up to `EXPR`/`MEMBER` are frozen at `ASL_AST_VERSION` 1. About
19 engineer-days.

### Phase 3: value constructors

| # | Ticket | Size | Depends on | Notes |
|---|---|---|---|---|
| 15 | FR-2610-anvl-asl-006 corpus generator and coverage rule | M | 7 | Can run beside phase 2. |
| 16 | FR-2610-anvl-asl-012 array and tuple literals | M | 10, D3, D4, D5 | |
| 17 | FR-2610-anvl-asl-013 object literals | M | 10, D4, D5 | |
| 18 | FR-2610-anvl-asl-014 interpolated strings | L | 10, D10 | |

**Milestone M2:** every expression form of the design grammar except lambdas. About 8 more days.

### Phase 4: module level and lambdas (need D7, D8, D9)

| # | Ticket | Size | Depends on | Notes |
|---|---|---|---|---|
| 19 | FR-2610-anvl-asl-015 body keywords refused in the signature | S | 6 | `src/core/asl_parser.c` has no keyword check today. |
| 20 | FR-2610-anvl-asl-016 `extend` declarations | M | 19, D8, D9 | Needs the dispatch hook. |
| 21 | FR-2610-anvl-asl-017 `import` and `vars` | M | 19, D8, D9 | Same hook. |
| 22 | FR-2610-anvl-asl-018 lambdas | M | 10, 12, D7 | `LAMBDA` kind; loop context resets. |

**Milestone M3:** the whole grammar. About 5 more days.

### Phase 5: hardening

| # | Ticket | Size | Depends on | Notes |
|---|---|---|---|---|
| 23 | FR-2610-anvl-asl-020 fuzz harness | M | 7, 15 | Can start after M1 and run through M2. |
| 24 | SR-2610-anvl-asl-002 `check-asl` gate, valgrind over the corpus | S | 7 | |
| 25 | SR-2610-anvl-asl-003 mark outdated notes, publish the contract note | S | owner answers | `notes/anvilscript-ast-contract.md`. |

Total: 6 existing and 20 new tickets (the 20 are FR-2610-anvl-asl-003 to 020 and SR-2610-anvl-asl-002 and 003), about
34 engineer-days: 8 S (4 days), 14 M (21 days), 3 L (9 days).

## 8. Error codes

Specific codes only; one per reason; no aliases of other modules' codes; no catch-all. Parse-time codes for the body
live in 51xx next to 5106; signature codes stay in 47xx. Names are the contract; the numbers below are provisional until
each ticket lands them in `include/errors.h` with a message in `src/core/errors.c` and a case in the corpus.

Existing, kept: 5103 (runtime), 5104 and 5105 (now emitted by the parser), 5106 `NESTING_TOO_DEEP`. 5101 and 5102 stay
defined and are **never returned by the parser**.

| Code | Name | Reason | Ticket |
|---|---|---|---|
| 5107 | `ANVL_ERR_ASL_NAME_IS_KEYWORD` | a keyword used as a name | BR-2610-anvl-asl-002 |
| 5108 | `ANVL_ERR_ASL_UNTERMINATED_BLOCK_COMMENT` | `/*` never closed | BR-2610-anvl-asl-005 |
| 5109 | `ANVL_ERR_ASL_EXPECTED_EXPRESSION` | replaces the reused `PARSER_EXPECTED_VALUE` | FR-2610-anvl-asl-003 |
| 5110 | `ANVL_ERR_ASL_EXPECTED_IDENTIFIER` | replaces the reused `PARSER_EXPECTED_IDENTIFIER` | FR-2610-anvl-asl-003 |
| 5111 | `ANVL_ERR_ASL_EXPECTED_SEMICOLON` | replaces the reused `PARSER_UNTERMINATED_STATEMENT` | FR-2610-anvl-asl-003 |
| 5112 | `ANVL_ERR_ASL_EXPECTED_INITIALIZER` | `var x;`; replaces the reused `PARSER_EXPECTED_ASSIGN` | FR-2610-anvl-asl-003 |
| 5113 | `ANVL_ERR_ASL_UNEXPECTED_CHARACTER` | a byte that begins no token | FR-2610-anvl-asl-003 |
| 5114 | `ANVL_ERR_ASL_EXPECTED_STATEMENT` | a token that cannot begin a statement; replaces the reused `PARSER_UNEXPECTED_TOKEN` | FR-2610-anvl-asl-003 |
| 5115 | `ANVL_ERR_ASL_UNCLOSED_PAREN` | grouping, call or header missing `)`; replaces the reused `PARSER_EXPECTED_VALUE` there | FR-2610-anvl-asl-003 |
| 5116 | `ANVL_ERR_ASL_INVALID_NUMBER` | `5.`, `12ab` | FR-2610-anvl-asl-001 |
| 5117 | `ANVL_ERR_ASL_INVALID_EXPONENT` | `1e`, `1e+`, `5.e3` | FR-2610-anvl-asl-001 |
| 5118 | `ANVL_ERR_ASL_UNTERMINATED_STRING` | | FR-2610-anvl-asl-001 |
| 5119 | `ANVL_ERR_ASL_UNTERMINATED_BLOB` | | FR-2610-anvl-asl-007 |
| 5120 | `ANVL_ERR_ASL_INVALID_BLOB_TAG` | tag over 31 characters or not an identifier | FR-2610-anvl-asl-007 |
| 5121 | `ANVL_ERR_ASL_INVALID_ASSIGN_TARGET` | `f() = 1`, `a.b = 1` | FR-2610-anvl-asl-008 |
| 5122 | `ANVL_ERR_ASL_EXPECTED_ARGUMENT` | `f(1,)`, `f(,)` | FR-2610-anvl-asl-008 |
| 5123 | `ANVL_ERR_ASL_EXPECTED_MEMBER_NAME` | `a.`, `a.1`, `a.if` | FR-2610-anvl-asl-008 |
| 5124 | `ANVL_ERR_ASL_EXPECTED_PAREN_OPEN` | `if x`, `while x` | FR-2610-anvl-asl-009 |
| 5125 | `ANVL_ERR_ASL_EXPECTED_BLOCK` | `{` missing after a header | FR-2610-anvl-asl-009 |
| 5126 | `ANVL_ERR_ASL_UNCLOSED_BLOCK` | body ended before `}` | FR-2610-anvl-asl-009 |
| 5127 | `ANVL_ERR_ASL_EXPECTED_FOR_SEPARATOR` | `for (a b)` | FR-2610-anvl-asl-011 |
| 5128 | `ANVL_ERR_ASL_UNCLOSED_BRACKET` | `[` without `]` | FR-2610-anvl-asl-012 |
| 5129 | `ANVL_ERR_ASL_EXPECTED_ELEMENT` | trailing comma, `()`, `(x,)` | FR-2610-anvl-asl-012 |
| 5130 | `ANVL_ERR_ASL_DUPLICATE_OBJECT_KEY` | | FR-2610-anvl-asl-013 |
| 5131 | `ANVL_ERR_ASL_EXPECTED_OBJECT_ASSIGN` | `:=` missing in an entry | FR-2610-anvl-asl-013 |
| 5132 | `ANVL_ERR_ASL_OBJECT_AT_STATEMENT_START` | `{` at statement start | FR-2610-anvl-asl-013 |
| 5133 | `ANVL_ERR_ASL_UNTERMINATED_INTERPOLATION` | `$"` never closed | FR-2610-anvl-asl-014 |
| 5134 | `ANVL_ERR_ASL_UNCLOSED_INTERPOLATION_BRACE` | `{` never closed | FR-2610-anvl-asl-014 |
| 5135 | `ANVL_ERR_ASL_EMPTY_INTERPOLATION` | `{}` (D10) | FR-2610-anvl-asl-014 |
| 5136 | `ANVL_ERR_ASL_STRAY_DOLLAR` | `$` not before `"` | FR-2610-anvl-asl-014 |
| 5137 | `ANVL_ERR_ASL_EXPECTED_LAMBDA_BODY` | `x =>` with nothing | FR-2610-anvl-asl-018 |
| 5138 | `ANVL_ERR_ASL_MODULE_DECL_IN_BODY` | `import`, `vars`, `include`, `extend` inside a body | FR-2610-anvl-asl-016, 017 |
| 5139 | `ANVL_ERR_ASL_TREE_TOO_TALL` | height bound (D11); not the nesting code, because the reason differs | FR-2610-anvl-asl-019 |
| 4704, 4705 | `ANVL_ERR_ASL_KEYWORD_PARAMETER`, `ANVL_ERR_ASL_KEYWORD_FUNCTION_NAME` | signature | FR-2610-anvl-asl-015 |
| 47xx | codes for `extend`/`import`/`vars` module-level reasons | allocated by those tickets | FR-2610-anvl-asl-016, 017 |

Each code also needs its row in the public-category mapping and the error-name table (`errors.c`), and appears in
`notes/conformance-error-coverage.md`'s style of table for the ASL corpus (6.2). Every reason in the table has at
least one corpus case; the coverage script (FR-2610-anvl-asl-006) enforces it. Position is always the start of the
token being read when the parse failed, as in the reader; for an unterminated construct, the end of input.

## 9. Risks

| # | Risk | Mitigation |
|---|---|---|
| 1 | **Contract churn after the compiler starts.** | Version + additive-only rule (4); M1 freezes a subset; the compiler builds only against a named commit. |
| 2 | **Tree depth from loops.** A 100,000-term `a + a + ...` is one parse loop and a 100,000-deep tree; the compiler, the dump tool and any tree walker overflow the stack. The existing nesting limit does not see it. | `height` + `ASL_MAX_TREE_HEIGHT` (FR-2610-anvl-asl-019), fuzz stressors, and the dump tool walks iteratively on a long left spine. D11. |
| 3 | **Test churn from the code migration.** Existing tests assert reused codes (`EXPECTED_VALUE` and others). | One ticket (FR-2610-anvl-asl-003) changes them and nothing else in those tests; reported to the Principal Engineer. |
| 4 | **Touching the Core Engineer's files.** The module-level hook sits in `parse_statement` and possibly the header scan. | D8; the hook is the same shape as the function hook; `check-reader-only` and the reader speed gate (native vs the previous native release, 1.02x limit, lower is faster) run on that ticket. |
| 5 | **Interpolation.** Nested strings, braces inside nested strings, an expression parsed from a sub-span that must still produce spans in the right buffer. | Parse the embedded expression with the same cursor, not a copied substring, so offsets stay absolute; takes a nesting level; corpus `limits/` cases; L-size. |
| 6 | **Statement start ambiguity** (`{` as object or block; `x => ...`; `(a, b) =>` vs tuple). | Section 3.4: position decides, post-hoc rewrite for lambdas, `{` at statement start is an error. |
| 7 | **Leaks on error paths.** The parser uses a heap `List` as scratch. | Every ticket runs valgrind on its suite and on the corpus (SR-2610-anvl-asl-002). |
| 8 | **A build that cannot link without the reader.** `asl_body_parser.c` calls `Source.*` and uses reader errors. | FR-2610-anvl-asl-004 removes both; WebAssembly feasibility is checked with a link test (no reader objects) in the dump-mode ticket. |
| 9 | **Owner decisions arrive late.** D1, D2, D5, D8 shape the contract. | Phase 0 and the tickets that do not need them go first; the order in section 7 is built so that M1 needs only D1, D2 and D6; D3 to D5, D10 and D12 are needed only from phase 3 on, and D7 to D9 only in phase 4. |
| 10 | **Escape decoding is undefined.** The reader does not decode; the compiler must. | Flagged to the Runtime Engineer (4.6); corpus cases keep escapes raw. A shared decoder would be a Core ticket. |
| 11 | **Performance.** Parsing is linear, one pass; no benchmark gate exists for the body parser. | None needed for the parser; the 1.02x reader gate applies to the ticket that touches `parse_statement`. |
| 12 | **UTF-8.** Identifiers are ASCII; a non-ASCII byte outside a string, blob or comment is an error. | Corpus `lexical/` cases for bytes 0x80 to 0xff at every position kind. |

## 10. Decisions needed from the owner

Each has a recommended default, which the plan assumes. Tickets that depend on an unanswered decision are not
started until it is answered.

| # | Decision | Recommended | Affects |
|---|---|---|---|
| **D1** | The keyword set refused as names in bodies. | The ANVL six plus `var if else for while break continue return extend`. `this`, `array`, `tuple`, `string`, `list`, `dict` are **not** keywords (`this` is an identifier the compiler binds in extension methods; the others are namespaces). | BR-2610-anvl-asl-002, FR-2610-anvl-asl-015, M1 |
| **D2** | Member access: one `MEMBER` node, or keep `QUALIFIED_NAME` for dotted names. | `MEMBER` only; the compiler resolves namespace versus receiver by scope. | FR-2610-anvl-asl-008, contract |
| **D3** | `array(...)` and `tuple(...)` spellings. | Ordinary calls; no keywords, no node. | FR-2610-anvl-asl-012 |
| **D4** | `[]` and `{}` valid in bodies though AML rejects `[]`. | Valid. Tuples still need two or more. | FR-2610-anvl-asl-012, 013 |
| **D5** | Array elements and object-entry values: `expr` (not `value`), and a bare word is a variable, never a string. | Yes. | FR-2610-anvl-asl-012, 013 |
| **D6** | Grammar additions beyond the design EBNF: `else if`; `simple_stmt` for `for`; the compound-assignment and `++`/`--` statements already shipped; a leading `_` in identifiers. | Accept all four. | FR-2610-anvl-asl-009, 011 |
| **D7** | Lambdas in the first grammar. | Yes, but built last (FR-2610-anvl-asl-018); closures stay post-MVP. | FR-2610-anvl-asl-018 |
| **D8** | Who adds the dispatch hook for the module-level `extend`, `import` and `vars` in `parse_statement` / header scan. | The Language Engineer adds a minimal `WITH_ASL` hook beside the function hook, reviewed by the Core Engineer. Alternative: file Core tickets. | FR-2610-anvl-asl-016, 017 |
| **D9** | Module-level syntax: `extend T { f(a) => { }; }` with no trailing `;` (as the EBNF), `import "prefix:name";`, `vars { x := 1; };`. The design calls the `extend` syntax undecided. | As written. | FR-2610-anvl-asl-016, 017 |
| **D10** | Interpolation: how to write a literal brace (recommend `\{` and `\}`, using the ordinary backslash), and whether `{}` is an error (recommend yes). | As recommended. | FR-2610-anvl-asl-014 |
| **D11** | Tree-height bound. | 1024. A body with more than 1,023 chained terms is refused. | FR-2610-anvl-asl-019 |
| **D12** | Blob literals as expressions. | Yes (design decision 8: AML values are first-class). | FR-2610-anvl-asl-007 |
| **D13** | File extension: `.asl` or `.anvs`. | `.asl` for documents (implemented); `.anvs` for shipped library sources. | notes, docs |

Resolved by this plan, confirm by silence: first error only, no recovery (row 18); `break`/`continue` outside a loop
are parse errors (row 24); compound forms stay desugared (row 10); no token layer (row 20); literal is kind plus span
(row 16); error codes migrate off the reader's (row 29).

## 11. What this plan changes

Nothing in the code and nothing in the four design notes. Rows 1 to 29 of section 2 are applied to the notes by
SR-2610-anvl-asl-003 after the owner answers section 10. Any AST change is recorded in `notes/anvilscript-design.md`
and the changelog by the ticket that makes it (FR-2610-anvl-asl-004 first), and named in that ticket's report.
