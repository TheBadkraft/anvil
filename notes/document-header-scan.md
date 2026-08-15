# Document Header Scan

Implementation reference for the header-scan step that precedes full body parsing in AML. Edited as the implementation evolves.

## Context

AML uses a two-phase parse:

1. **Header scan** — discover dialect, imports, and module attributes without parsing the body.
2. **Body parse** — parse statements and expressions after the full import graph is known.
3. **Resolution** — resolve inheritance, references, and imports after all bodies are parsed.

This document focuses on phase 1.

## Responsibilities of the header scan

The header scan runs over the source buffer and extracts only the leading document-level constructs:

- Optional shebang (`#!aml`, `#!amp`, `#!asl`) for dialect.
- Import declarations (`import "path";`).
- Optional module attributes (`@[...]`).
- Stop at the first body statement (e.g., `name := value`, `derived : base`, or anonymous block).

The scan does **not**:

- Parse statement bodies or values.
- Resolve inheritance targets.
- Validate that imported files exist (that happens when they are loaded).

## Storage details

`imports` and `attributes` are Sigma lists configured with `sizeof(void *)` stride and therefore store pointers to heap-allocated `anvl_doc_import_t` and `anvl_doc_attribute_t` entries. The entries themselves own no additional allocations; their slice fields point into the source buffer, preserving the no-copy parser philosophy. Header disposal iterates each list, frees every entry, and then disposes the lists.

## Public function boundary

```c
anvl_result doc_scan_header(module_document doc, anvl_err_code *out_err_code);
```

- Input: a document whose source buffer has already been loaded.
- Output: dialect resolved, imports and module attributes recorded.
- Returns `ANVL_RES_OK` on success, `ANVL_RES_ERR` on failure.

This function is exposed for independent testing and is called internally by `doc_load_source` after loading the source.

## Header data structure

```c
typedef struct anvl_src_slice_t {
   usize start;
   usize length;
} anvl_src_slice;

typedef struct anvl_doc_import_t {
   anvl_src_slice decl;   // "import \"path\"" (no trailing ';')
   anvl_src_slice path;   // "\"path\"" (quotes included)
} anvl_doc_import_t;
typedef anvl_doc_import_t *anvl_doc_import;

typedef struct anvl_doc_attribute_t {
   anvl_src_slice key;    // identifier (e.g., "active", "is_active")
   anvl_src_slice value;  // value text; length 0 means flag attribute
} anvl_doc_attribute_t;
typedef anvl_doc_attribute_t *anvl_doc_attribute;

struct anvl_doc_header_t {
   list imports;     // anvl_doc_import pointers
   list attributes;  // anvl_doc_attribute pointers
};
```

The `module_document` struct gains a `header` field:

```c
struct anvl_mod_doc_t {
   anvl_source source;        // source for the document
   module_context context;    // owning context, set on registration
   string filepath;           // normalized path to the loaded module
   struct anvl_doc_header_t *header; // parsed header metadata
};
```

## No-copy principle

The parser has always been no-copy. Header metadata follows the same model:

- Imports are stored as declaration/path slice pairs into the source buffer.
- Attributes are stored as key/value slice pairs; `value.length == 0` denotes a flag attribute.
- No path or attribute text is duplicated; slices are stable as long as the source object lives.
- When an `.anvlo` object file is generated, the same slices can be serialized as offsets into a mapped source section.

## Scanning rules (ordered)

Header elements must appear in this order:

1. Skip whitespace and comments (`//`, `/* */`).
2. If the next token is `#!`, the source loader has already parsed the dialect token and advanced the source position past the shebang line. The scanner only checks that the dialect was valid.
3. Skip whitespace and comments.
4. While the next token is `import`, parse the quoted path and consume the terminating `;`.
5. Skip whitespace and comments.
6. If the next token is `@[`, parse module attributes.
7. Skip whitespace and comments.
8. If the next token is `import` after attributes have been seen, report an ordering error.
9. Stop when a non-header construct is encountered.

Import declarations following any attribute block are rejected because the header ordering is shebang → imports → attributes → body.

## Shebang parsing

The shebang line has the form `#!dialect` where `dialect` is one of:

- `aml`
- `amp`
- `asl`

It may be preceded by whitespace. It overrides any file-extension dialect hint. The shebang itself is **not** stored as header metadata; it only affects dialect resolution.

Shebang detection and consumption now happen at source load time (`Source.from_buffer` / `Source.from_file`). After loading, the source position is advanced past the shebang line, so the header scanner starts on the first import/attribute/body token rather than re-reading the shebang. The source object records whether a shebang was found in `src->has_shebang`; `Source.is_shebang` reports this flag. The header scanner only validates that an invalid shebang dialect token surfaces as an error.

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

Imports are stored as metadata slices pointing into the source buffer. Canonicalization and resolution happen when the import graph is expanded.

## Module attributes

Module attributes are declared with `@[...]` syntax and must appear after imports.

Example:

```anvl
@[schema, schema_version=1]
@[author="David Boarmahn"]
@[created=08-15-2026] // not sure if we're supporting `-` in bare literals; this value might be quoted
```

Each attribute entry inside a block (comma-separated) becomes one `anvl_doc_attribute_t`:

- `@[active]` → `key = "active"`, `value.length = 0` (flag).
- `@[is_active=true]` → `key = "is_active"`, `value = "true"`.
- `@[ident=null]` → `key = "ident"`, `value = "null"`.

The value slice is raw text after `=` and is trimmed of surrounding whitespace at the slice boundaries without copying. A zero-length value means the attribute is a flag. Full attribute value parsing is deferred until the body parser or resolver needs it.

## Error handling

The scan reports errors such as:

- Unterminated comment.
- Invalid shebang dialect.
- Malformed import declaration (missing quotes, missing semicolon).
- Import after an attribute (ordering violation).
- Body statement appearing before imports/attributes are complete.

Errors route through `Source.set_error(doc->source, ...)`.

## Relationship to loading and registration

`doc_scan_header` is invoked after the document source is loaded **and** the document is registered with a context. This ordering is required because scan errors route through `Source.set_error`, which looks up the owning document via the source hash registry.

Normal loading flow:

```
doc_load_source(doc, origin, source, len, err)
  |- source loaded into doc->source
mod_ctx_register_doc(ctx, doc, filepath, err)
  |- document registered in source hash registry
doc_scan_header(doc, err)
  |- header scanned, imports/attributes recorded
|- (later) import graph expansion, body parse, resolution
```

For tests that exercise only header extraction, create a context, load the source, register the document, then call `doc_scan_header` directly.

## Testing strategy

A dedicated test suite in `test/unit/test_header.c` covers the scanner with eleven cases (HDR00–HDR10):

- Empty header (no shebang, no imports).
- Shebang detection with leading whitespace/comments.
- Multiple imports in order.
- Imports interleaved with comments.
- Missing semicolon after import.
- Missing quotes around import path.
- Invalid shebang.
- Unterminated comment before shebang.
- Module attributes after imports.
- Import after attribute fails (ordering violation).
- First body statement terminates the header scan; source location is placed on the first body character.

`test/unit/test_document.c` includes overlapping header tests (HDR00–HDR10) that exercise `doc_scan_header` through the document API.

Current status (build 12):

- `test_header`: 11/11 passing; Valgrind-clean
- `test_document`: 31/31 passing; Valgrind-clean
- `test_module`: 28/28 passing; Valgrind-clean
- `test_registry`: 9/9 passing; Valgrind-clean
- `test_source`: 23/23 passing; Valgrind-clean

All active unit suites report 0 Valgrind errors and 0 lost bytes (only the expected still-reachable source-hash registry map blocks remain).

## Deferred to AnvilScript / later work

- `using` declarations (ASL).
- `vars` blocks — to be classified as header or body when ASL design begins.
- Namespace keyword, if AML ever adds it.

## Resolved questions

1. **Header ordering**: shebang → imports → attributes. Enforced by the scanner.
2. **Header storage**: `struct anvl_doc_header_t` with `imports` and `attributes` lists, owned by `module_document`.
3. **No-copy**: imports/attributes stored as slice metadata into the source buffer.
4. **Import slice**: declaration slice excludes `;`, path slice includes surrounding quotes.
5. **Attribute slice**: each attribute stored as key/value slice pair; zero-length value means flag.
6. **Scan timing**: header scan runs after source load and document registration so errors can route through the source registry.
7. **vars/usings**: deferred to AnvilScript design.

## Implementation inventory

The header-scan work required changes beyond the scanner itself. The following files were touched and why:

- `include/internal/module.h` — added `anvl_src_slice_t`, `anvl_doc_import_t`/`anvl_doc_import`, `anvl_doc_attribute_t`/`anvl_doc_attribute`, `struct anvl_doc_header_t`, and the `header` field on `module_document`.
- `src/core/source.c` — expanded the previously minimal `Source` interface with FNV-1a hashing, peek/consume/match helpers, whitespace/comment skipping, line/column tracking, and registry-backed `has_errors`/`set_error`. The code was newly written for the scanner rather than ported from `_source.c`. Shebang parsing was later moved here from `document.c` so that the source loader advances past the shebang line and resolves the dialect before the header scanner runs.
- `src/core/document.c` — implemented `doc_scan_header` and its helpers; created/disposed import/attribute lists; fixed `doc_load_source` so it no longer leaks the existing source object when one is already present. `header_scan_shebang` was simplified to a dialect-validity check now that the source loader consumes the shebang.
- `src/core/errors.c` — fixed `anvl_error_set` to accept a NULL `out_err_code` so `Source.set_error(..., NULL)` still appends the error.
- `src/core/module.c` — ensured `mod_dispose` releases the registry reference acquired in `mod_new`.
- `src/core/source_registry.c` — unchanged in this phase; `Registry.clear()` remains available for test teardown.
- `test/utilities/debug.c` — added `Registry.release()` to `dispose_mod_manual` so module-based tests keep the registry refcount correct.
- `test/unit/test_header.c` — new dedicated header suite (HDR00–HDR10).
- `test/unit/test_source.c` — new dedicated Source interface suite (SRC00–SRC22) covering all public helpers and registry-backed error routing.
- `test/unit/test_document.c` — added SRC08a–SRC08e for source error routing and HDR00–HDR10 for header scanning; fixed double-dispose of documents already owned by a context (SRC08a, SRC08b, DOC09, DOC10).
- `test/unit/Makefile` — added `test_source` build/run target.
- `test/infra/Makefile` — added `errors.c` and `source_registry.c` to `ANVIL_SRCS` so infra suites link against the current core.

### Legacy source archive note

Prefixed legacy files (`_source.c`, `_parser.c`, etc.) contain the pre-refactor implementations and remain in the tree as reference material. When replacing one, extract all relevant functionality first, then move the legacy file to an archive location (or mark it clearly as fully superseded) rather than leaving it to be rediscovered later.

### Source interface coverage

A dedicated Source test suite in `test/unit/test_source.c` now covers all public `Source` helpers (SRC00–SRC22):

- Create/dispose, null-safety, and lifecycle (SRC00–SRC02).
- `Source.from_buffer` and `Source.from_file` success/failure paths (SRC03–SRC09).
- `Source.hash` stability/distinctness (SRC10).
- Position, line, and column tracking (SRC11, SRC18).
- `Source.is_eof`, `Source.is_eof_offset` (SRC12).
- `Source.peek`, `Source.peek_offset` (SRC13).
- `Source.match_length` and `Source.match_operator` (SRC14).
- Character classification: `Source.is_alpha`, `Source.is_digit`, `Source.is_hex_digit` (SRC15).
- `Source.consume` bounds handling (SRC16).
- `Source.data` and `Source.length` (SRC17).
- Whitespace/comment skipping and shebang detection (SRC19–SRC20).
- `Source.has_errors` and `Source.set_error` registry routing (SRC21–SRC22).

All listed helpers now have direct unit tests and pass under Valgrind.

## Registry cleanup note

Tests that bypass `mod_new` and use `mod_ctx_initialize` directly allocate the source-hash registry on the first `mod_ctx_register_doc` call but never release it because they have no module to call `Registry.release()`. `Registry.clear()` can be invoked in a test-suite teardown to dispose the map and reset the refcount. This is safe for unit tests but should not be used in normal module lifecycle code.

## Document ownership note

Once a document is registered with a context via `mod_ctx_register_doc`, the context owns the document and will dispose it in `mod_ctx_dispose`. Callers must not call `doc_dispose` on a registered document unless the registration failed or the document was explicitly unregistered. Double-disposing a registered document causes use-after-free when the context later iterates its docs list.

## Open questions

1. Should imports carry a resolved dialect hint once the import graph is expanded?
2. How does the header struct serialize into an `.anvlo` file? (See `anvlo-compilation.md`.)
