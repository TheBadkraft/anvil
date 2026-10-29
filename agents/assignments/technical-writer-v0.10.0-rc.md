# Technical Writer: docs refresh and v0.10.0-rc publication

The Technical Writer's standing assignment. Role and rules: `.claude/agents/anvl-technical-writer.md`.
This file was `agents/ANVL-DOCUMENTOR.agent.md` until the move to sub-agents on 2026-10-29.

**Status (2026-10-29):** not started. Nothing below has been done yet. Every deploy needs
`AUTHORIZED: deploy anvldata.com`.

## Assignment 1: now (not gated on the release)

1. **`docs/schema/README.md` is stale** (last touched 2026-09-19). Bring it up to what AnvilSchema
   does today, working from the resolved tickets. Those are FR-2610-anvl-schema-001..005,
   FR-2610-anvl-types-001..005, BR-2610-anvl-nul-001 and BR-2610-anvl-registry-001. Cover:
   - nested `fields`/`items`, `min_items`/`max_items` and violation paths;
   - `nullable`, `integer`, `format`, and the Blob primitive with `tag`;
   - `anvil_schema_validate_value` with `skip_required`/`allow_unknown`;
   - load diagnostics, introspection and extension keys;
   - exact numeric bounds;
   - the NUL and thread-safety guarantees.

   `docs/types-reference.md` was updated alongside those tickets. Keep the two consistent and
   cross-linked. Then update the site copy (`site/assets/docs/schema/README.md`), scrubbed.
2. **Scrub the site docs.** Leave FlyWire's mentions (deliberate marketing). Remove ticket IDs,
   `notes/` paths, agent or role names, private ops, and any mention of Rivet, Sigma, CabNet or
   q-or from `site/`.
3. **Sync check.** `getting-started.md`, `language-reference.md` and `types-reference.md` changed
   in both `docs/` and `site/assets/docs/`. Diff each pair and fix any drift, keeping the
   intentional site adaptations.
   - **3b. State the attribute line-break rule** in `docs/language-reference.md` §5 and its site
     copy. Owner decision, 2026-10-26: whitespace, including newlines, is allowed anywhere between
     the tokens of an attribute list. So a multi-line list is valid; it's purely a style choice.
     An attribute's value never includes the whitespace around it (BR-2610-anvl-attr-001).
     Contrast that, in one sentence, with the one place a newline *is* significant: a statement's
     `;` must stay on its value's line (§3). Example:
     ```anvl
     ticket @[
        type=bug,
        status="open"
     ] {
        title := "Login fails";
     };
     ```
   - **3c. State the `iso8601_datetime` profile** (added 2026-10-29) in the schema and types
     docs. It is the strict RFC 3339 form: a `T` separator, and `Z` or a `±hh:mm` offset, as
     `fmt_datetime` in `src/anvil_types.c` checks. A space separator or a short `±hh` offset (what
     Postgres emits) is rejected. Normalize before validating.
4. **The live site is stale.** Every page checked on 2026-10-26 differed from the repo. Prepare a
   deploy of the above. It can go out before the release, since the docs describe native
   behaviour that's already merged. Say in your report whether anything in it describes a download
   that doesn't exist yet.

## Assignment 2: prepare now, publish with `v0.10.0-rc`

The Release Engineer has staged the artifacts. The handoff is
`~/release-staging/v0.10.0-rc/handoff/HANDOFF-documentor.md`, which is internal and never goes in
`site/`. Prepare everything on a branch:

5. **Downloads** (owner decision R2). Replace every `0.8.0-rc` file in `site/assets/downloads/`
   with the `v0.10.0-rc` artifacts byte for byte, plus `SHA256SUMS`, and run `sha256sum -c`.
   Remove the old files; there's no archive. Then:
   - update `site/download.html`: names, sizes, install snippets, the Linux-x64-only note;
   - set `site/assets/version.js` to `0.10.0-rc`;
   - re-grep `site/` for `0.8.0-rc` and `0.9.0-rc`.
6. **Bindings-Guide** (`site/assets/docs/Bindings-Guide.md`) still describes `0.8.0-rc`. Write:
   - types and schema per binding (Node, WASM, .NET, Python): `TypeRegistry`, `Schema`,
     `validate`, `validateValue`, `validateField` (including Node/WASM `each`), and `fields`
     introspection;
   - attributes: `attributes`/`findAttribute`/`statements()`, raw `value` vs `decodedValue`;
   - the writer and builder for Node/WASM;
   - WASM's in-memory FS (`writeFile`/`removeFile`);
   - thread safety: independent loads are safe;
   - the npm note: npm doesn't refresh a `file:` dependency, so remove `node_modules/<pkg>`
     before reinstalling;
   - the version string. A prebuilt tarball reports `0.10.0-rc+284`. A source install reports
     plain `0.10.0-rc`, because it has no build number. That's expected; set `ANVL_BUILD` at
     install time to supply one;
   - the WebAssembly (`lib/`, `dist/`) and Python (`src/`) layout changes, from the handoff.

   Sources: the handoff, the resolutions of FR-2610-anvl-bindings-001/-002/-003 and
   SR-2610-anvl-vendor-bump-002, and `notes/deferred-work.md` ("Deferred — bindings: types and
   schema"). Each binding's own README is accurate for its surface.
7. **Public changelog.** Add a "Changelog" page (`site/assets/docs/changelog.md`, plus a nav link
   in `docs.html`) for `v0.10.0-rc`. Curate it from the staged `release-notes-v0.10.0-rc.md`:
   user-facing changes only, scrubbed.

After an authorized deploy, report the live URLs you checked. The Principal Engineer then starts
the Release Engineer for its live check.
