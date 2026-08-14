# Document Header Scan

Implementation reference for the header-scan step that precedes full body parsing in AML. Edited as the implementation evolves.

## Context

AML uses a two-phase parse:

1. **Header scan** — discover imports, dialect, and module attributes without parsing the body.
2. **Body parse** — parse statements and expressions after the full import graph is known.
3. **Resolution** — resolve inheritance, references, and imports after all bodies are parsed.

This document focuses on phase 1.

## Responsibilities of the header scan

The header scan runs over the source buffer and extracts only the leading document-level constructs:

- Optional shebang (`#!aml`, `#!amp`, `#!asl`) for dialect.
- Optional module attributes (`@[...]`).
- Import declarations (`import "path";`).
- Stop at the first body statement (e.g., `name := value`, `derived : base`, or anonymous block).

The scan does **not**:

- Parse statement bodies or values.
- Resolve inheritance targets.
- Validate that imported files exist (that happens when they are loaded).

## Public function boundary

```c
anvl_result doc_scan_header(module_document doc, anvl_err_code *out_err_code);
```

- Input: a document whose source buffer has already been loaded.
- Output: dialect resolved, module attributes recorded (if any), imports collected in the document.
- Returns `ANVL_RES_OK` on success, `ANVL_RES_ERR` on failure.

This function is exposed for independent testing and is called internally by `doc_load_source` after loading the source.

## Data to collect per document

The header scan populates fields on `module_document` (or a per-document header struct):

| Field | Purpose |
|-------|---------|
| `dialect` | Resolved dialect from shebang or file hint. |
| `attributes` | Module attributes parsed from `@[...]`. |
| `imports` | Ordered list of import paths declared in the header. |

Exact struct layout is still to be determined.

## Scanning rules

1. Start at the beginning of the source buffer.
2. Skip whitespace and comments (`//`, `/* */`).
3. If the next token is `#!`, parse the dialect token.
4. Skip whitespace and comments again.
5. If the next token is `@[`, parse module attributes.
6. Skip whitespace and comments again.
7. While the next token is `import`, parse the quoted path and consume the terminating `;`.
8. Stop when a non-header construct is encountered.

## Shebang parsing

The shebang line has the form `#!dialect` where `dialect` is one of:

- `aml`
- `amp`
- `asl`

It may be preceded by whitespace and comments. It overrides any file-extension dialect hint.

## Import path parsing

Each import declaration has the form:

```anvl
import "path";
```

- `import` keyword.
- Flexible whitespace before the quoted string.
- A double-quoted string literal representing the path.
- A terminating semicolon.

Extensions are optional. By convention AML imports are written without one; the loader searches the directory and uses the first file whose stem matches the path. The discovered file's actual extension provides the dialect hint. The loader does not impose deterministic ordering on directory listings. Explicit extensions are also accepted.

Paths are stored exactly as written in the source. Canonicalization and resolution happen when the import graph is expanded.

## Module attributes

Module attributes are declared with `@[...]` syntax. For now, the header scan should at minimum recognize and skip the block to avoid misinterpreting its contents as imports or statements.

Example:

```anvl
@[schema("v1")]
```

Full attribute parsing can be deferred, but the scan must know where the attribute block ends.

## Error handling

The scan reports errors such as:

- Unterminated comment.
- Invalid shebang dialect.
- Malformed import declaration (missing quotes, missing semicolon).
- Unexpected token in header (e.g., a bare identifier before imports are complete).

## Relationship to `doc_load_source`

Normal loading flow:

```
doc_load_source(doc, origin, source, len, err)
  |- source loaded into doc->source
  |- doc_scan_header(doc, err)
  |- (later) import graph expansion, body parse, resolution
```

For tests that exercise only header extraction, `doc_scan_header` can be called directly after creating and loading a source.

## Testing strategy

A dedicated test set should cover:

- Empty header (no shebang, no imports).
- Shebang detection with leading whitespace/comments.
- Multiple imports in order.
- Imports interleaved with comments.
- Missing semicolon after import.
- Missing quotes around import path.
- Invalid shebang.
- Unterminated comment before shebang.
- Module attributes before imports.
- First body statement terminates the header scan.

## Open questions

1. Should the header scan also detect the `namespace` keyword if AML ever adds it, or is that out of scope?
2. Should module attributes be fully parsed during header scan, or just located and deferred?
3. What is the exact struct for storing per-document header data?
4. Should imports be stored as raw string slices into the source buffer, or copied into owned memory?
