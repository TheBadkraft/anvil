# Deferred & Open Work

Single index of everything intentionally deferred to a later phase, or left as an open decision blocking current work, across all `notes/` documents. Check this file when starting a new iteration so nothing raised in a previous iteration's notes gets silently dropped. Entries are removed once resolved (the resolution moves into the owning doc's "Resolved questions" section) — this file should only ever list things still outstanding.

## Deferred to AnvilScript (ASL) design

- `using` declarations — `document-header-scan.md`.
- `vars` blocks — classify as header or body construct once ASL design begins — `document-header-scan.md`, `document-body-parse.md`.
- Namespace keyword, if AML ever adds one — `document-header-scan.md`.
- Interpolation and dynamic var-refs (`${...}`, `$"..."`) — AML/AMP have neither; entirely ASL's domain — `document-body-parse.md`.
- `:=`-optional exceptions, if AnvilScript ever needs one — `document-body-parse.md`.

## Deferred to Resolution phase (phase 4)

- **"Cannot be inherited but can be derived"**: whether something is a legal target to inherit/derive from is entirely the resolver's concern — `doc_parse_body` just captures `base` syntactically and never judges it. The distinction itself is still not understood (an earlier working theory for it turned out to depend on a since-corrected assumption about the grammar) — needs to be asked plainly when resolver design starts, not guessed at again. `document-body-parse.md`.
- Static value reference (`derived_var := base_var;`, `ANVL_VALUE_IDENTIFIER`) resolves once at Resolution time via a global identifier map built after all documents in the import graph are body-parsed — the resolver mechanism itself (presumably reusing the lazy-merge-cache pattern `anvl_resolver_build_state` already uses for inheritance) is not yet designed. `document-body-parse.md`.

Import-graph processing order (topological sort) is **not** on this list — it was considered and resolved as unnecessary; see `document-header-scan.md` § *Import-graph processing order — resolved as unnecessary* and `document-body-parse.md` § *Relationship to header scan and import loading*.

## Deferred to compilation (`anvilc` / `.anvlo`)

- The entire `.anvlo` object format — concept stage only, no implementation. `anvlo-compilation.md` lists its own open questions (format, versioning, source retention, import representation, body IR, error handling, build tooling, linking, cross-platform).
- Body compaction — a separate compile phase; the parse layer stays zero-copy. `document-header-scan.md`.

## Open — blocking the current body-parse iteration

- Is `tuple` AMP-legal? `document-body-parse.md`.
- Is a bare `#<hex-digits>` token (`hex := #12D4F;`) an `INTEGER`, or does it need its own value type? `document-body-parse.md`.
- Should `vars` blocks be a statement kind with a nested statement list, or a single value tree? `document-body-parse.md`.

## Testing gaps in the already-wrapped header-scan/import-loader phase — all closed

- ~~No test exercises a buffer-rooted document with an import~~ — **closed**: `HDR16_import_loader_buffer_root_cwd`, passed first run, no code changes, Valgrind-clean.
- ~~No fixture uses `../`-style imports~~ — **closed**: `HDR17_import_loader_dotdot` (new fixture `test/fixtures/hdr_import_sub/hdr_import_dotdot.anvl`), passed first run, no code changes, Valgrind-clean. Confirms a single-hop `../` resolve relative to the importing file's own directory works correctly.
- **Note, not a gap**: full path **canonicalization** (`realpath()` or equivalent — comparing two differently-written paths to the same file for dedup/cycle-detection purposes) is still unimplemented, per `aml-import-namespace-rules.md` § *Canonical-path function*. HDR17 doesn't exercise this — it's a single relative resolve, not a diamond/cycle scenario across differently-spelled paths. Left unimplemented deliberately; nothing in the body-parse plan needs it. If it's ever needed, see `document-header-scan.md` § *Path resolution: file-rooted vs. buffer-rooted documents*.

## Superseded notes

- `aml-import-namespace-rules.md` § *Import graph order* — its "reverse pre-order = valid bottom-up order" claim is wrong under diamond imports, though it turned out not to matter: no document-processing order is required at all (map-based resolver). See `document-header-scan.md` § *Import-graph processing order — resolved as unnecessary*. The rest of that document (import/namespace rules, two-phase parse framing) still stands as background context.
- `aml-import-namespace-rules.md` § 7 *Two-Phase Parse* — its claim that body-parse needs bottom-up document order is superseded for the same reason: the map-based resolver design makes document-processing order irrelevant. See `document-body-parse.md` § *Relationship to header scan and import loading*.
- ~~`aml-import-namespace-rules.md` § 6 *Inheritance Syntax*~~ — **retracted**: I initially flagged its example (`user : entity { name := string }`, no `:=` before the block) as stale, reasoning from an incorrectly-collapsed single-form grammar. It was actually right all along — confirmed as the `OBJECT_BLOCK` form (`ident [: base] [@[...]] { statements };`, never `:=`) once the real two-form grammar (`ASSIGN` vs `OBJECT_BLOCK`) was worked out. Not superseded; this entry is void.
