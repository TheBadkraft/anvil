# Anvil.O Compilation (`.anvlo`)

## Status

Concept / design in progress. No implementation yet.

## Idea

Compile `.anvl` source files into internal `.anvlo` object files, analogous to
`.c` → `.o` compilation. The goal is to produce a binary artifact that can be
loaded directly without re-running the Anvil parser.

## Purpose

- **Faster loading**: modules can be loaded from pre-parsed `.anvlo` files
  instead of re-scanning and re-parsing source text.
- **Distribution**: compiled artifacts can be shipped without source.
- **Build integration**: `anvlo` files participate in a build graph where
  dependents only recompile when their upstream objects change.
- **Tooling**: linters, optimizers, and linkers can operate on a stable
  intermediate representation.

## Core principle

An `.anvlo` file is not a flat byte stream; it is a structured container of
sections. The source-hash identity model gives each document a stable key, so
compiled objects can be looked up and merged by content hash.

## Proposed sections

| Section | Purpose |
|---------|---------|
| `header` | Magic, version, target dialect, build number. |
| `source` | Original source buffer (or a hash-verified copy) for slice metadata. |
| `docmeta` | Document identity: source hash, filepath offset, header metadata. |
| `imports` | Import graph edges: source hashes of imported documents. |
| `attrs` | Module attribute slice metadata. |
| `nodes` | Parsed AST nodes with offsets into `source` or `strings`. |
| `strings` | Deduplicated string data referenced by nodes and metadata. |
| `errors` | Pre-recorded parser errors, if any. |
| `symbols` | Exported identifiers and their resolution targets. |

## Relationship to the source-hash registry

- The process-wide source hash registry already maps `Source.hash(src)` to
  `module_document`.
- An `.anvlo` file serializes that same identity so a loader can reconstruct
  the registry entry without parsing.
- Loading an `.anvlo` file becomes: map sections → allocate document → register
  in `Registry` by source hash → done.

## What we resolve now

- `.anvlo` is the intermediate object format name and file extension.
- It is section-based, similar to ELF or wasm.
- It preserves the no-copy slice metadata model: nodes and metadata reference
  offsets into a source section rather than duplicating text.
- Source hash is the canonical identity key for objects and imports.

## Open questions

1. **Format**: custom binary format, ELF-like, or something like wasm/custom?
2. **Versioning**: how do we handle format evolution and backward compatibility?
3. **Source retention**: does `.anvlo` keep the full source, only a hash, or
   both? Full source is easiest for slice metadata; hash-only requires
   reconstituting slices as offsets into a separate source file.
4. **Import resolution**: are imports stored as raw paths, resolved hashes, or
   both?
   - In my estimate, it seems that compiling an anvl source differs slightly
   from C. in C where each source become an `.o` file, we might bring all 
   imports into the root, like a fat `.o` file might be built from several 
   C sources.
5. **Body representation**: do we store a full AST, a flat node list, or a
   higher-level IR?
   - TBD
6. **Error handling**: should parse errors be compiled into `.anvlo` so a
   downstream loader knows the object is invalid?
   - No ... if errors exist, we don't write an `.anvlo`.
7. **Build tooling**: what is the CLI for `anvilc` and how does it integrate
   with make/cmake?
   - all TBD
8. **Linking**: how are multiple `.anvlo` files combined into a module or
   package?
9. **Cross-platform**: endianness, pointer width, alignment requirements.

## Concerns

- **Over-engineering**: avoid designing a full linker before the parser is
  stable. Start with a minimal format that can round-trip a parsed document.
- **Source vs. IR**: keeping full source in `.anvlo` defeats some size goals.
  Reconstituting slices from offsets into an external source file complicates
  loading.
  - at the very minimum, original full source is minimized (and maybe binarized ???)
- **Incremental builds**: source hashes give us cheap change detection, but we
  still need a way to invalidate dependents when an interface changes.
- **Debugging**: compiled objects should still support error messages with line
  and column information.

## Next steps

1. Stabilize header scanning and document loading.
2. Define the minimal `.anvlo` record for a single parsed document.
3. Implement a writer that serializes a `module_document` after header scan.
4. Implement a reader that reconstructs the document and registers it in the
   source hash registry.
5. Only then expand to multi-document linking and build-tool integration.
