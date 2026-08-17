# Document Body Parse

Implementation reference for the body-parse step that follows header scanning and import loading in AML.

## Context

AML parsing proceeds in three phases:

1. **Header scan** — discover shebang, imports, and module attributes.
2. **Import loading** — expand the import graph recursively.
3. **Body parse** — parse statements and build a document-level statement/value tree.
4. **Resolution** — resolve inheritance, references, and imports after all bodies are parsed.

This document focuses on phase 3.

## Responsibilities of the body parse

The body parser walks the source from where `doc_scan_header` left off (`doc->source->pos` at the first body character) and produces a list of statements for the document.

The parser does **not**:

- Resolve symbol references or inheritance targets.
- Evaluate expressions.
- Allocate output/canonical values.
- Validate that referenced identifiers exist.

The parser **does**:

- Recognize statement kinds: assignment, inheritance/anon-block, `vars`, `using`.
- Capture source spans for every identifier, base, value, and statement.
- Build a recursive value tree for the right-hand side of assignments.
- Attach module attributes that appear before a statement (statement-level attributes).
- Report malformed syntax through `Source.set_error`.

## Public function boundary

```c
anvl_result doc_parse_body(module_document doc, anvl_err_code *out_err_code);
```

- Input: a document whose header has been scanned and whose imports have been loaded.
- Output: `doc->body` populated with a list of `anvl_doc_statement_t` pointers.
- Returns `ANVL_RES_OK` on success, `ANVL_RES_ERR` on failure.

This function is exposed for independent testing and is called internally after `mod_load_imports` succeeds.

## Slice conventions

The body parser uses the same `anvl_slice` type as the header scanner (`include/internal/module.h`):

```c
typedef struct anvl_src_slice_t {
   char *data;   // base pointer to the source allocation
   char *start;  // first byte of the slice
   char *end;    // one-past-the-end byte of the slice
} anvl_slice;
typedef anvl_slice *slice;
```

A slice is self-referential and empty when `start == end`; text length is `end - start`. Both are derived on demand via `Source.slice_is_empty`/`Source.slice_length` rather than stored. Slices are no-copy references into `doc->source` and become invalid if the source is disposed.

## Statement structure

A statement is a flat declaration in the document body. It carries a span, a kind, optional name/base identifiers, an optional value tree, and optional statement attributes.

```c
typedef enum {
   ANVL_STMT_ASSIGN,      // name := value
   ANVL_STMT_INHERIT,     // name : base { body }
   ANVL_STMT_ANON_BLOCK,  // : base { body } or name : base { body }
   ANVL_STMT_VARS,        // vars { ... }
   ANVL_STMT_USING,       // using "path";
} anvl_stmt_kind;

typedef struct anvl_doc_statement_t {
   anvl_stmt_kind kind;
   anvl_slice span;           // full source span of the statement
   anvl_slice name;           // declared identifier; empty when absent
   anvl_slice base;           // inheritance base; empty when absent
   anvl_doc_value value;      // assigned value tree; NULL for pure inheritance/using
   list attributes;           // anvl_doc_attribute pointers, or NULL
} anvl_doc_statement_t;
typedef anvl_doc_statement_t *anvl_doc_statement;
```

Notes:

- `name` is empty for anonymous blocks, `vars`, and `using`.
- `base` is only used for inheritance and anonymous-block statements.
- `value` is `NULL` for `using` declarations and for inheritance statements that carry only a base with no inline value.
- `attributes` holds any `@[...]` blocks that appear immediately before the statement.

## Value structure

Values form a recursive tree. Every value node has a type, a source span, and type-specific children.

```c
typedef enum {
   ANVL_VALUE_NONE,
   ANVL_VALUE_NULL,
   ANVL_VALUE_BOOL,
   ANVL_VALUE_INTEGER,
   ANVL_VALUE_FLOAT,
   ANVL_VALUE_STRING,
   ANVL_VALUE_BLOB,        // tag stored separately; content in text span
   ANVL_VALUE_ARRAY,
   ANVL_VALUE_TUPLE,
   ANVL_VALUE_OBJECT,
   ANVL_VALUE_PAIR,        // object entry: key + value
   ANVL_VALUE_VARREF,      // ${...} or $ident
   ANVL_VALUE_IDENTIFIER,  // bare symbol reference
} anvl_value_type;

typedef struct anvl_doc_value_t {
   anvl_value_type type;
   anvl_slice text;         // full source span of the value
   anvl_slice tag;          // blob tag (e.g. @hex); empty for non-blobs
   union {
      struct {
         list items;        // array/tuple: list of anvl_doc_value_t *
      } collection;
      struct {
         list entries;      // object: list of anvl_doc_value_t * (each type PAIR)
      } object;
      struct {
         anvl_doc_value key;
         anvl_doc_value val;
      } pair;
   };
} anvl_doc_value_t;
typedef anvl_doc_value_t *anvl_doc_value;
```

Notes:

- Scalar values (null, bool, integer, float, string) are fully described by `type` and `text`.
- Blob values use `tag` for the `@tag` prefix and `text` for the raw content slice.
- Array and tuple children live in `collection.items`.
- Object children live in `object.entries`; each entry is a `PAIR` value.
- `VARREF` and `IDENTIFIER` values keep their raw text so resolution can interpret them later.

## Storage details

`statements` is a Sigma list with `sizeof(void *)` stride, storing pointers to heap-allocated `anvl_doc_statement_t` entries. Each statement owns:

- Its own scalar fields and slices.
- Its `value` tree (if any), which recursively owns child lists.
- Its `attributes` list (if any).

Statement disposal iterates the body list, frees each statement, recursively frees the attached value tree, and disposes the list.

## Document structure additions

`struct anvl_mod_doc_t` gains a `body` field:

```c
struct anvl_mod_doc_t {
   anvl_source source;               // source for the document
   module_context context;           // owning context, set on registration
   string filepath;                  // normalized path to the loaded module
   struct anvl_doc_header_t *header; // parsed header metadata
   list body;                        // list of anvl_doc_statement pointers
};
```

## Parsing rules

Body statements are parsed sequentially until EOF. The parser skips whitespace and comments between statements.

For each statement:

1. Collect any leading `@[...]` attribute blocks into `statement->attributes`.
2. Read the first identifier or keyword.
3. Dispatch on what follows:
   - `:=` → assignment; parse value into `statement->value`.
   - `:` → inheritance or anonymous block; parse optional name, optional base, and optional body block.
   - `vars` keyword → `vars` block; parse body as a value tree or sub-statement list (TBD with ASL design).
   - `using` keyword → `using` declaration; parse quoted path.
4. Record the full source span from first attribute (or statement start) to the terminating newline/semicolon/block.

Inheritance notes:

- ANVL discourages inheriting without adding or overriding. A plain `name : base;` may be accepted syntactically and treated as equivalent to `name := base;` during resolution, or it may be rejected as a parse error. Decide before implementation.
- Anonymous blocks (`Ident : Base { ... }` and `: Base { ... }`) are supported.
- Statement-level attributes may appear before inheritance blocks.

Value parsing:

- Parse literals (null, bool, integer, float, string, blob) as scalar value nodes.
- Parse arrays `[ ... ]`, tuples `( ... )`, and objects `{ ... }` as collection value nodes.
- Object entries are parsed as `PAIR` values.
- Bare identifiers and varrefs are parsed as `IDENTIFIER` / `VARREF` nodes for resolution.

## Error handling

Errors route through `Source.set_error(doc->source, ...)`. Common error cases:

- Unexpected token at statement start.
- Missing `:=` or `:` after identifier.
- Missing value after `:=`.
- Unterminated string, array, tuple, object, or attribute block.
- Malformed blob tag or content.
- Invalid identifier in varref.

## Testing strategy

A dedicated test suite in `test/unit/test_body.c` (new) exercises the body parser. Suggested initial cases (BODY00–BODY15):

- **BODY00** — empty body returns OK and an empty statement list.
- **BODY01** — simple assignment `name := 42;`.
- **BODY02** — string assignment `name := "hello";`.
- **BODY03** — array assignment `name := [1, 2, 3];`.
- **BODY04** — tuple assignment `name := (a, b, c);`.
- **BODY05** — object assignment `name := { a: 1, b: 2 };`.
- **BODY06** — blob assignment `name := @hex AB01;`.
- **BODY07** — varref assignment `name := ${other};`.
- **BODY08** — inheritance block `derived : base { override := 2; };`.
- **BODY09** — anonymous block `: base { field := 1; };`.
- **BODY10** — statement attributes `@[active] derived : base { ... };`.
- **BODY11** — missing value after `:=` reports error.
- **BODY12** — unterminated array reports error.
- **BODY13** — unterminated object reports error.
- **BODY14** — invalid blob tag reports error.
- **BODY15** — multiple statements captured in order.

## Relationship to header scan and import loading

The body parser runs after:

```
doc_load_source(doc, origin, source, len, err)
mod_ctx_register_doc(ctx, doc, filepath, err)
doc_scan_header(doc, err)
mod_load_imports(ctx, doc, err)
doc_parse_body(doc, err)
```

If `doc_scan_header` or `mod_load_imports` reports an error, body parsing is skipped for that document.

## Open questions

1. Should `vars` blocks be parsed as a statement kind with a nested statement list, or as a single value tree?
2. Should plain `name : base;` be accepted or rejected?
3. Should statement-level attributes be allowed on every statement kind, or restricted?
4. How are interpolated strings represented in the value tree? As a single `STRING` node with embedded varrefs, or as a list of fragment nodes?
5. Should object keys support arbitrary expressions, or only identifiers/string literals?
