# AML Import & Namespace Rules

Implementation reference for the Anvil parser. Edited as the implementation evolves. Derived from design sessions in August 2026.

**Partially superseded** — see `notes/deferred-work.md` § *Superseded notes*: § *Import graph order* below is wrong under diamond imports, and its motivating premise (that document-processing order matters) turned out to be false anyway — the resolver design is map-based, not order-dependent, which also supersedes § 7's bottom-up-parse-order requirement (corrected in `document-header-scan.md` and `document-body-parse.md`). § 6 *Inheritance Syntax* was briefly flagged as stale too but turned out to be correct — `document-body-parse.md` now confirms `Ident : Base { ... }` (no `:=`) as the real `OBJECT_BLOCK` grammar. The rest of this document still stands.

---

## 1. No Namespaces, No Import Aliases

AML has no namespace system and no import aliases. Imports merge flat into a single document namespace.

Relative import paths are resolved from the importing file's own directory. The root document's directory is the starting point for the first import; each subsequently imported file resolves its own imports relative to its own directory.

```anvl
// In root.anvl, located in /project/
import "./common/types";     // resolves to /project/common/types
import "./business/models";  // resolves to /project/business/models

// In /project/common/types.anvl:
import "../shared/base";     // resolves to /project/shared/base
```

## 2. Name Collisions Are a Hard Error

Name collisions across imported documents fail fast. The author's escape hatch is wrapping statements in an anonymous object to scope them locally. If two separately imported documents collide with each other, the importer resolves it on their side — by definition they're working with copies, not shared originals.

## 3. Import Takes a Quoted Path

One form, no bare-literal alternative. The quoted string is resolved relative to the importing file's own directory, as described in §1.

Extensions are optional. By convention AML imports are written without an extension:

```anvl
import "../shared/config";
import "./common/types";
```

When no extension is given, the loader searches the directory for any file whose stem matches the path and uses the first match returned by the filesystem. The discovered file's actual extension provides the dialect hint. Explicit extensions are also accepted if the author prefers:

```anvl
import "./common/types.aml";
```

The optional nature of extensions does not need to be prominently documented; it simply works either way. The loader does not impose deterministic ordering on directory listings.

## 4. Imported Statements Become Part of One Flat Merged Document

There is no derived reference identifier. If an imported file has a statement called `foo`, you reference it as `foo`, exactly as if it were typed locally. The filename plays no role in referencing.

## 5. Filename Matters for All Error Reporting

Not just collisions — any parse or resolution error should identify which source file it came from.

## 6. Inheritance Syntax

Inheritance is not assignment. It uses a single colon:

```anvl
user : entity {
   name := string
}
```

The parser must distinguish `derived : base` from `name := value`.

## 7. Two-Phase Parse: Full Import Graph Before Resolution

1. **Phase 1 — Header scan.** Scan headers across the whole import graph to discover every document and build the complete ordered document list.
2. **Phase 2 — Body parse.** Parse all document bodies in reverse dependency order so base/leaf documents are parsed before the documents that import them.
3. **Phase 3 — Resolution.** Only after every document in the graph is fully parsed does inheritance and statement resolution run.

This guarantees that by the time any resolver touches an inheritance link, the base document it points to is already fully parsed and available.

## 8. Diamond Imports and Cycles Resolve via Canonical-Path Tracking

As import headers are scanned, each file is tracked by canonical path. If a file has already been imported (or is already queued to be imported), it is not re-imported. This single mechanism handles both diamond imports and circular imports without special-casing either.

---

## Implementation Notes

### Header scanning boundary

Header scanning can be exposed as `doc_scan_header` for independent testing, and called from `doc_load_source` during normal loading. A dedicated test set (`test_document_scan`) can validate header extraction without requiring a full parse.

### Import graph order

The ordered `ctx->docs` list is populated during header scanning as imports are discovered. For example:

- `docA` imports `docB` and `docC`
- `docB` imports `docBA`
- `docC` imports `docCA` and `docCB`

Discovery order (depth-first, following import declaration order):  
`docA, docB, docBA, docC, docCA, docCB`

Parsing in reverse gives the correct bottom-up order:  
`docCB, docCA, docC, docBA, docB, docA`

Yes — the reverse of the discovery list is a valid bottom-up parse order, provided every dependency is appended before its importer's body is parsed. Header scanning must complete across the entire graph before any body parsing begins.

### Source-file attribution

Every parsed statement/identifier must carry a reference to its originating source file for error reporting. The exact mechanism is still to be determined; options include:

- A `source_file` pointer or index on each statement node.
- A source-file index stored alongside the statement in the context's docs list.

### Canonical-path function

Canonical path resolution normalizes a path so equivalent paths compare equal. It should:

- Resolve `.` and `..` segments.
- Resolve symbolic links (optional but recommended).
- Produce an absolute path.

Implementation options:

- Use POSIX `realpath()` for full canonicalization.
- Implement a custom resolver if sandboxing or symlink handling needs to be restricted.

This function lives in the Files layer.

### Loading trigger

`Anvil.load(filepath)` should trigger the full two-phase parse immediately. Immediate parsing gives predictable, eager error reporting and keeps tests deterministic.

---

**Key design insight:** an anonymous object already functions as a namespace-like scoping container. This is what dissolves the perceived need for a real namespace system — scoping is available when needed, without import-time namespace machinery when it isn't.
