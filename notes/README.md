# Notes Index

Reading order and status for everything in `notes/`, reconstructed from git history (`git log --diff-filter=A --follow`) plus this session's own record of what was split from what. Intended as the map for later synthesizing a comprehensive technical reference and user's guide from these notes — read this first, then follow into whichever file covers the phase you need.

**Always check `deferred-work.md` first** when starting new work — it's the single index of everything intentionally deferred or left open across every note below, so nothing raised in an earlier iteration gets silently dropped.

## Chronological order

### Iteration 1 — Source identity & header scan (2026-08-14)
- **`source-identity-registry.md`** — design record for the source-hash identity/registry mechanism (`Source.hash`, process-wide hash → `module_document` registry). Its own header still says "Status: in progress," which is stale — the registry is fully implemented and in heavy use as of this session (it's the exact mechanism `Source.get_arena`/`Source.new_node`/`Source.set_error` all look through, and this session found and fixed a real bug in how it interacts with `doc_dispose`). Worth a status-header pass later.
- **`document-header-scan.md`** — design record for `doc_scan_header`/`mod_load_imports`. Phase itself is functionally complete, but this file is still actively amended when related work surfaces (most recently: the diamond-dispose registry bug, `HDR18`–`HDR20`, this session).
- **`aml-import-namespace-rules.md`** — earlier, more informal design reference. Partially superseded (see its own header and `deferred-work.md` § *Superseded notes*); the rest still stands as background context.
- **`notes.md`** — miscellaneous, doesn't fit a specific doc. See *Miscellaneous* below — naming collision with this file worth resolving.

### Iteration 2 — `.anvlo` compilation exploration (2026-08-15)
- **`anvlo-compilation.md`** — concept-stage only, no implementation. Explicitly deferred (`deferred-work.md` § *Deferred to compilation*).

### Iteration 3 — `document-body-parse` (2026-08-17 – ongoing, current)
- **`document-body-parse.md`** — primary design record for this iteration: statement/value grammar, arena-backed allocation (now implemented and tested end-to-end), testing strategy.
- **`deferred-work.md`** — created partway through this iteration specifically because open questions and deferred items scattered across the notes above were getting lost between iterations ("we don't go back into old iteration notes to suggest future work — it will get lost"). Single always-checked-first index since.
- **`resolution-phase.md`** — split out of `document-body-parse.md` today (2026-08-22), once Resolution was recognized as its own future iteration rather than a sub-topic of body-parse. Not started yet.

### Iteration 4 — AnvilScript (ASL) design (2026-09-05)
- **`anvilscript-design.md`** — design-only, no implementation; a prior ASL implementation (v0.4.0-alpha) existed but predates the current architecture and has since been removed from the tree. Collects the ASL-relevant fragments already decided elsewhere (`docs/dialect-ownership-matrix.md`, fixture comments, `build.anvl`), the five items moved here from `deferred-work.md`'s former ASL section, and the founding framing questions the design conversation is starting from.

## Status key

- **Active** — the live design record for the current iteration; still being edited.
- **Reference** — phase is functionally complete, but the file is still amended when directly relevant work surfaces (not a closed/frozen document).
- **Historical / partially superseded** — kept for background context; specific corrected claims are tracked in `deferred-work.md` § *Superseded notes* rather than rewritten in place.
- **Index** — cross-cutting, not tied to one iteration (`deferred-work.md`, this file).
- **Not started** — design-only, no implementation (`anvlo-compilation.md`, `resolution-phase.md`, `anvilscript-design.md`).

## Miscellaneous

`notes.md` (deliberately not `README.md`'s neighbor in naming — it predates this index) holds a handful of small implementation notes from the source-identity iteration that never fit `source-identity-registry.md`'s format: type-usage conventions (`usize` over `size_t`), a couple of API-shape decisions, a caching note. Nothing else in `notes/` currently plays this "doesn't fit anywhere" role, but if that need comes up again, it probably belongs here or in a similarly-named catch-all rather than forcing it into an unrelated design doc.

Worth deciding: rename `notes.md` to something less confusable with this file (`scratch.md`, `misc.md`) now that a real index exists under a similar name — not renamed here since that's a naming call, not a structural one.
