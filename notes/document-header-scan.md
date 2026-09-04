# Document Header Scan

**Status: implemented and GREEN.** Phase 1 of the document pipeline (`doc_scan_header`) is complete — shebang/import/module-attribute scanning, import-graph loading (`mod_load_imports`, including diamond/cycle handling), and the source-hash registry it all sits on. `test_header.c` (`HDR00`–`HDR20`), `test_source.c`, `test_registry.c`, `test_module.c`'s `CR17`–`CR22` all pass, Valgrind-clean. Kept as implementation reference below, not rewritten now that it's done — see per-section "Status" notes throughout for what shipped and why.

Implementation reference for the header-scan step that precedes full body parsing in AML. Edited as the implementation evolves.

See `notes/deferred-work.md` for anything raised here that's deferred to a later phase or still an open decision.

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
   char *data;  // pointer to the start of the source buffer (raw bytes)
   char *start; // pointer to the start of the slice within the source buffer
   char *end;   // pointer to the end of the slice within the source buffer
} anvl_slice;
typedef anvl_slice *slice;

typedef struct anvl_doc_import_t {
   anvl_slice decl;          // "import \"path\"" (no trailing ';')
   anvl_slice path;          // "\"path\"" (quotes included)
   module_document resolved; // child document after import graph expansion; NULL until loaded
} anvl_import;
typedef anvl_import *anvl_doc_import;

typedef struct anvl_doc_attribute_t {
   anvl_slice key;   // identifier (e.g., "active", "is_active")
   anvl_slice value; // value text; empty slice (start == end) means flag attribute
} anvl_doc_attribute_t;
typedef anvl_doc_attribute_t *anvl_doc_attribute;

struct anvl_doc_header_t {
   list imports;     // anvl_doc_import pointers
   list attributes;  // anvl_doc_attribute pointers
};
```

A slice is self-referential: `data` is the base of the owning source buffer, and `start`/`end` bound the slice within it. Nothing else is stored — length, emptiness, and the substring itself are all derived from these three pointers via the `Source` interface rather than cached:

```c
usize (*slice_length)(anvl_slice);    // end - start
bool  (*slice_is_empty)(anvl_slice);  // start == end
usize (*substring)(anvl_slice, char *out_buffer); // copies start..end into out_buffer, NUL-terminates
```

This replaced an earlier `usize start; usize length;` offset-pair representation. The pointer form avoids needing the owning source's base pointer as separate context when working with a slice in isolation, and keeps every slice validity check (`data`/`start`/`end` non-NULL, `start <= end`) local to the slice value itself.

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
2. The source loader has already consumed any leading shebang; the header scanner starts on the first import/attribute/body token.
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

Shebang detection and consumption now happen at source load time (`Source.from_buffer` / `Source.from_file`). After loading, the source position is advanced past the shebang line, so the header scanner starts on the first import/attribute/body token rather than re-reading the shebang. The source object records whether a shebang was found in `src->has_shebang`; `Source.is_shebang` reports this flag, and `Source.dialect` reports `ANVL_DIALECT_ERROR` if the shebang token was invalid. The header scanner itself does not re-validate the shebang.

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
@[author="David Boarman"]
@[created=08-15-2026] // not sure if we're supporting `-` in bare literals; this value might be quoted
```

Each attribute entry inside a block (comma-separated) becomes one `anvl_doc_attribute_t`:

- `@[active]` → `key = "active"`, `value` empty (`start == end`, flag).
- `@[is_active=true]` → `key = "is_active"`, `value = "true"`.
- `@[ident=null]` → `key = "ident"`, `value = "null"`.

The value slice is raw text after `=` and is trimmed of surrounding whitespace at the slice boundaries without copying. An empty value slice means the attribute is a flag. Full attribute value parsing is deferred until the body parser or resolver needs it.

## Error handling

The scan reports errors such as:

- Unterminated comment.
- Malformed import declaration (missing quotes, missing semicolon).
- Import after an attribute (ordering violation).
- Body statement appearing before imports/attributes are complete.

Invalid shebang dialects are reported at source-load time, not by the header scanner.

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

A dedicated test suite in `test/unit/test_header.c` covers the scanner with nine cases (HDR00, HDR02–HDR05, HDR07–HDR10):

- Empty header (no shebang, no imports).
- Multiple imports in order.
- Imports interleaved with comments.
- Missing semicolon after import.
- Missing quotes around import path.
- Unterminated comment before shebang.
- Module attributes after imports.
- Import after attribute fails (ordering violation).
- First body statement terminates the header scan; source location is placed on the first body character.

(The dedicated shebang-detection and invalid-shebang tests were removed because shebang handling lives entirely in the source loader; `test_source` and `test_document` still exercise valid shebangs.)

`test/unit/test_document.c` includes overlapping header tests that exercise `doc_scan_header` through the document API.

Current status (build 12+):

- `test_header`: 9/9 scanner tests passing; 7 import-loader tests (HDR11–HDR17) passing against the real loader. 17/17 total, Valgrind-clean.
- `test_document`: 31/31 passing; Valgrind-clean.
- `test_module`: 28/28 passing; Valgrind-clean.
- `test_registry`: 9/9 passing; Valgrind-clean.
- `test_source`: 23/23 passing; Valgrind-clean.

All active unit suites report 0 Valgrind errors and 0 bytes in use at exit.

The header-scan and import-loader work is wrapped.

## Import graph expansion

After the header scan records import slices, a separate loader phase expands the graph:

```c
anvl_result mod_load_imports(module_context ctx, module_document root, anvl_err_code *out_err_code);
```

Responsibilities:

- Walk `root->header->imports` in order.
- Strip quotes from each `path` slice.
- Resolve the path relative to `root->filepath`.
- Load the referenced file as a new document (`doc_load_source` + `mod_ctx_register_doc`).
- Scan the child header (`doc_scan_header`).
- Recursively expand the child’s imports.
- Set `anvl_doc_import_t.resolved` to the loaded child document.
- Detect cycles and missing files, reporting errors on the requesting document.

The loader does not parse bodies; it only ensures the full import graph is loaded and header-scanned before body parsing begins. This separation keeps path resolution, cycle detection, and duplicate-document deduplication in one place.

### Cycle detection

A transient stack of source hashes (or document identities) tracks documents currently being expanded. If an import resolves to a document already on the stack, the loader reports `ANVL_ERR_IMPORT_CYCLIC` on the requesting document and unwinds.

### Duplicate document deduplication

The source-hash registry naturally deduplicates diamond imports. When the loader attempts to register a file whose content hash is already present, it reuses the existing `module_document` and sets `resolved` to that document rather than loading a second copy. The freshly-loaded duplicate `module_document` that `mod_ctx_register_doc` rejected is then disposed via the ordinary `doc_dispose`.

**Resolved — bug found and fixed: disposing the discarded duplicate was silently un-registering the original.** `doc_dispose` used to call `Registry.remove(Source.hash(doc->source))` unconditionally for whatever source it was disposing. Since the discarded duplicate shares the *exact same content hash* as the original document it lost the dedup race to, disposing it removed the registry entry belonging to the still-alive original — even though that original was never disposed and remained in `ctx->docs`. From that point on, any `Source.*` registry-lookup call (`get_arena`, `new_node`, `set_error`, `has_errors`) would silently fail for that document, indistinguishable from it being genuinely unregistered.

This went undetected through `HDR13` (diamond reuse) because that test only checks `ctx->docs` size and `resolved` pointer equality — it never performs a registry lookup afterward. It surfaced only once `notes/document-body-parse.md`'s arena work needed a real post-diamond-load registry lookup (`Source.new_node` on the surviving document, from `test_module.c`'s `CR22`). Fixed in `doc_dispose` (`src/core/document.c`) by only removing the registry entry when this document is actually the one currently registered under that hash: `if (Registry.find(hash) == doc) { Registry.remove(hash); }`. Covered directly by `HDR20` (below).

### Path resolution: file-rooted vs. buffer-rooted documents

"Resolve the path relative to `root->filepath`" (above) needs to account for how `root` was loaded, since a root document doesn't always have a real file behind it:

- **File-rooted**: `Anvil.load(filepath)`-style loading gives `root->filepath` a real path. `import_resolve_dir` takes the directory component (everything before the last `/`) as the base for resolving that document's own imports, and this repeats recursively — each document resolves its imports relative to *its own* directory, not the original root's, per `aml-import-namespace-rules.md` §1.
- **Buffer-rooted**: a document loaded via `Source.from_buffer` (no real file) has no meaningful directory. `import_resolve_dir` falls back to `.` — imports in a buffer-loaded root resolve relative to the process's current working directory at the time the import is loaded, not to any notion of "where the buffer came from" (there isn't one). This falls out of the existing NULL/no-slash handling in `import_resolve_dir` already (an empty or bare-filename `filepath` both resolve to `.`) rather than being special-cased — but there is currently no dedicated test exercising a buffer-rooted document with an import, so this behavior isn't locked in by a test yet. Flagged in `notes/deferred-work.md`.

`./` and `../` are not special-cased differently from each other — both are ordinary relative-path text that gets string-joined onto the resolved parent directory (`import_resolve_dir` + `snprintf("%s/%s", parent_dir, import_name)`) and handed to the filesystem as-is; the OS resolves `.`/`..` segments when the file is actually opened. What's *not* yet implemented is path **canonicalization** before that string join/compare — `aml-import-namespace-rules.md` § *Canonical-path function* already flags this as TBD (`realpath()` or a custom resolver). Without it, two imports of the same file written differently (e.g. `"./a/../a/x"` vs `"./a/x"`) would be treated as distinct paths by the cycle-detection stack's `strcmp` and by dedup, even though they resolve to the same file. No current fixture exercises `../`-style imports, so this gap isn't test-covered either. Also tracked in `notes/deferred-work.md`.

### Import loader test contract (HDR11–HDR20)

A new batch of header-suite tests exercises `mod_load_imports` through fixture files:

- **HDR11** — single import resolves: root imports `hdr_import_base.anvl`; `resolved` is set and two documents are registered.
- **HDR12** — nested imports: root imports `hdr_import_nested.anvl`, which imports `hdr_import_types.anvl`, which imports `hdr_import_base.anvl`; all four documents are loaded and linked.
- **HDR13** — diamond reuse: root imports base and types, types imports base; both paths point `resolved` at the same base document and only three total documents are registered.
- **HDR14** — cyclic import rejected: a self-importing fixture reports `ANVL_ERR_IMPORT_CYCLIC`.
- **HDR15** — missing import file: an import referencing a non-existent file reports an error on the root document.
- **HDR16** — buffer-rooted document, no fixture file: a document registered with a bare (no-directory) `filepath` via `setup_registered_doc` imports `../fixtures/hdr_import_base.anvl` and resolves successfully, confirming imports on a buffer-loaded root resolve relative to the process's CWD. Passed on the first run with no code changes — the `import_resolve_dir` NULL/no-slash fallback already did the right thing; this test just locks it in.
- **HDR17** — `../` resolves relative to the importing file's own directory: `test/fixtures/hdr_import_sub/hdr_import_dotdot.anvl` (a new fixture in a new subdirectory) imports `"../hdr_import_base.anvl"`, which correctly navigates back up to `test/fixtures/hdr_import_base.anvl`. Also passed on the first run with no code changes — `import_resolve_dir` computes each document's own directory correctly on recursion, and the unquoted `../` segment is handled by the filesystem when the file is opened, with no canonicalization needed for a single-hop resolve like this one.

Both HDR16 and HDR17 are Valgrind-clean (0 errors, 0 leaks). Together they close the buffer-root and `../`-resolution testing gaps noted in `deferred-work.md`. What's still open — and distinct from what these tests cover — is path **canonicalization** (`realpath()` or equivalent): comparing two *differently-written* paths to the same file (e.g. `"./a/../a/x"` vs `"./a/x"`) for dedup/cycle-detection purposes. HDR17 only proves a single relative resolve works; it doesn't touch canonicalization at all.

- **HDR18** — repeated shebang rejected: a document with two `#!` lines reports `ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS`.
- **HDR19** — import loader body-size hint: `mod_load_imports`'s `out_body_size_hint` sums `Source.length()` across the whole graph, diamond counted once — verified against an independent walk of `ctx->docs` on the diamond fixture rather than hardcoded byte counts. See `notes/document-body-parse.md` "Arena-backed allocation".
- **HDR20** — registry survives disposal of a discarded diamond duplicate: the bug fix documented above, in "Duplicate document deduplication". After the diamond fixture's import graph fully resolves, the surviving `base` document must still be findable via `Registry.find` by its own content hash.

These tests exercise the loader API and the `resolved` back-pointer on `anvl_doc_import_t`.

## Deferred to AnvilScript / later work

- `using` declarations (ASL).
- `vars` blocks — to be classified as header or body when ASL design begins.
- Namespace keyword, if AML ever adds it.
- Body compaction / `.anvlo` generation — keep the parse layer zero-copy; compaction belongs to a separate compile phase.

## Import-graph processing order — resolved as unnecessary

`mod_load_imports` discovers the import graph via DFS, and `ctx->docs` ends up in DFS *pre-order* (a document is registered before its imports are recursively expanded) as a side effect of that traversal. **No document-processing order is actually required.** The resolver design is map-based: every document in the graph is body-parsed (in any order) before Resolution begins, so by the time anything resolves a `base`/`IDENTIFIER` reference, every statement in the whole graph is already registered in an identifier map. Resolution fails only on a missed lookup, never because of processing order — see `document-body-parse.md` § *Relationship to header scan and import loading* for the full reasoning. This also means `.anvlo` linking's eventual "how do imports fold into a root object" question (`anvlo-compilation.md` open question 4) doesn't need a topological order either, just the same completeness guarantee (whole graph loaded/parsed before linking).

That said, `notes/aml-import-namespace-rules.md` § *Import graph order* contains a genuine bug worth keeping on record independent of whether anything needs the fix: it concludes that **reversing** the pre-order discovery list gives a valid bottom-up (dependencies-first) order. That's only true for tree-shaped import graphs. It breaks under diamond imports, which this project explicitly supports and tests (HDR13): take `u` importing `v` and `w`, where both `v` and `w` import `x` (already registered/deduped by the time `w` is reached). Pre-order discovery is `[u, v, x, w]`; reversing gives `[w, x, v, u]` — but `w` depends on `x`, and `w` now comes *before* `x`. Reversed pre-order is wrong whenever a shared dependency is reachable through more than one path. If an ordering is ever wanted for some other reason (readability, deterministic output, etc.), the correct construction is DFS **post-order** (append each document to the order list only *after* fully recursing into its own imports, skipping documents already in the list for dedup) — no reversal required, and it directly guarantees dependencies precede dependents even through diamonds, unlike reversed pre-order.

## Resolved questions

1. **Header ordering**: shebang → imports → attributes. Enforced by the scanner.
2. **Header storage**: `struct anvl_doc_header_t` with `imports` and `attributes` lists, owned by `module_document`.
3. **No-copy**: imports/attributes stored as slice metadata into the source buffer.
4. **Import slice**: declaration slice excludes `;`, path slice includes surrounding quotes.
5. **Attribute slice**: each attribute stored as key/value slice pair; empty value slice means flag.
6. **Slice representation**: `anvl_slice` is a self-referential `{data, start, end}` pointer triple into the owning source buffer, not a `{start, length}` offset pair. Length, emptiness, and substring extraction are derived via `Source.slice_length`/`slice_is_empty`/`substring` rather than stored.
7. **Scan timing**: header scan runs after source load and document registration so errors can route through the source registry.
8. **vars/usings**: deferred to AnvilScript design.

## Implementation inventory

The header-scan work required changes beyond the scanner itself. The following files were touched and why:

- `include/internal/module.h` — added `anvl_src_slice_t`/`anvl_slice`/`slice`, `anvl_doc_import_t`(`anvl_import`)/`anvl_doc_import`, `anvl_doc_attribute_t`/`anvl_doc_attribute`, `struct anvl_doc_header_t`, and the `header` field on `module_document`.
- `src/core/source.c` — expanded the previously minimal `Source` interface with FNV-1a hashing, peek/consume/match helpers, whitespace/comment skipping, line/column tracking, and registry-backed `has_errors`/`set_error`. The code was newly written for the scanner rather than ported from `_source.c`. Shebang parsing was later moved here from `document.c` so that the source loader advances past the shebang line and resolves the dialect before the header scanner runs.
- `src/core/document.c` — implemented `doc_scan_header` and its helpers; created/disposed import/attribute lists; fixed `doc_load_source` so it no longer leaks the existing source object when one is already present. `header_scan_shebang` was removed from `doc_scan_header`; shebang handling now lives entirely in the source loader.
- `src/core/errors.c` — fixed `anvl_error_set` to accept a NULL `out_err_code` so `Source.set_error(..., NULL)` still appends the error.
- `src/core/module.c` — ensured `mod_dispose` releases the registry reference acquired in `mod_new`; implemented the recursive import loader (`mod_load_imports`, `import_load_child`, `import_load_child_recursive`, `import_path_on_stack`) with cycle detection and diamond deduplication.
- `src/core/source_registry.c` — unchanged in this phase; `Registry.clear()` remains available for test teardown.
- `test/utilities/debug.c` — added `Registry.release()` to `dispose_mod_manual` so module-based tests keep the registry refcount correct.
- `test/unit/test_header.c` — new dedicated header suite (HDR00, HDR02–HDR05, HDR07–HDR10); import-loader tests HDR11–HDR17 appended and passing, including HDR16 (buffer-rooted CWD-relative resolution) and HDR17 (`../` resolves relative to the importing file's own directory).
- `test/fixtures/hdr_import_sub/hdr_import_dotdot.anvl` — new fixture in a new subdirectory, imports `../hdr_import_base.anvl` to exercise HDR17.
- `test/utilities/helpers.c`/`helpers.h` — added `slice_equals`, `setup_registered_doc`, and `setup_registered_file` shared helpers for source-slice assertions, registered-document setup, and fixture-based document setup. `slice_equals` now takes only an `anvl_slice` (no separate `anvl_source`, since the slice is self-referential) and compares through `Source.slice_length`/`Source.substring`; the standalone `slice_is_empty` test helper was removed in favor of calling `Source.slice_is_empty` directly.
- `test/fixtures/hdr_import_*.anvl` — fixture graph for import-loader tests (single, nested, diamond, cyclic, missing); `hdr_import_nested.anvl` was added to create the 3-level import chain required by HDR12.
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
