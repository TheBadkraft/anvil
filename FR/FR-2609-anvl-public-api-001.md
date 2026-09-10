# Public API — Anvil Native

## Status

**All three slices implemented and GREEN.** `Anvil` (lifecycle), `Statement`/`Value` (read/query accessors), and now the vtable convenience layer over all three groups — real, tested (`test/unit/test_anvil_native.c` 17/17 + `test_anvil_vtable.c` 4/4, 92 assertions total, Valgrind-clean). A caller can load a document and walk its entire resolved value tree via either calling style. See "Implementation" below for what shipped, including a real core-parser bug this work found. Still ahead: anonymous (`OBJECT_BLOCK`) field traversal and everything else in "Open questions" below.

## Founding framing (repo owner, verbatim/near-verbatim)

This is **Anvil Native** — the one-truth implementation. Once in production, no other one-off language-specific implementation will be supported; all other languages come to this native library. The project builds primitives here, not policy — the job is to give users the tools to use this library, not to hand-hold them through using it well. Callers are free to write APIs native to their own paradigms on top of this, and they're free to get that wrong; Anvil Native doesn't prevent that, it isn't its job to.

Three questions frame the sketch:

1. **What does the API expose, and why?**
2. **What does the API deliberately *not* expose, and why?**
3. **Who is the audience?** — .NET, Java, Node, Python, Rust, C++, or some subset/none of these, and what does that imply about the shape of the surface (a stable C ABI other languages bind to, presumably, given "primitives not policy" — but that's to be confirmed, not assumed).

## Audience & positioning

ANVL is meant to be a genuine contender against JSON/YAML/TOML/etc. as *the* format of choice — not a niche format with one companion library. The audience is universal: .NET, Java, Node, Python, Rust, C++, and whatever else shows up later. The repo already contains one-off `anvil.net`/`anvil.js` implementations; the vision is to retire those in favor of thin bindings over this one native library, so no single language ever "owns" ANVL or gets a richer/more-capable implementation than any other.

## Decisions

1. **Scope: read + query only for this pass.** Parse an existing document, walk/query the resolved result. Programmatic construction of a document in memory and serializing it back out to `.anvl` text is real, wanted, but deliberately a separate, later phase — sketched once the read side is solid, not designed simultaneously.

2. **Data never crosses the ABI as a raw struct.** `anvl_slice`/`anvl_value_t`/`anvl_statement_t` and friends are C-internal representations tied to the arena's bulk-free lifetime — never exposed directly. Every language gets opaque handles (`anvil_document*`, `anvil_value*`, ...) and reads data only through accessor functions. This is deliberately uniform across every language, C/C++/Rust included, even though they *could* safely read a raw pointer — no language gets privileged unsafe access, matching "no language takes ownership of ANVL."

3. **Single-threaded per context/document, documented as a hard constraint, not internally synchronized.** Matches the current internal implementation exactly (arena, `Map`, `List` are all unsynchronized). A binding's own runtime is responsible for any cross-thread marshaling. Revisit only if a concrete use case actually needs concurrent access to one context — not a default engineering cost to take on speculatively.

4. **ABI shape: a vtable-first convenience layer, backed by an equally first-class flat export baseline.** Two views of one naming law, not two separate designs:
   - **Vtables** — a handful of named, *single-level* (not nested) `extern const` structs of function pointers, one per logical group (`Anvil`, `Statement`, `Value`), mirroring the pattern the codebase's own internal code already uses (`Anvl`). Deliberately not deep struct-of-structs nesting — that's fine for C/C++/Rust/.NET but genuinely painful to marshal from Java/Python, and one level keeps the convenience-layer languages happy without making anything worse for anyone else.
   - **No getter function — plain exported data symbols instead.** Originally sketched as "the vtable getter is itself a flat export" (e.g. `anvil_get_vtable()`), but once actually implemented, that machinery turned out to be unnecessary: `extern const anvil_i Anvil;` (and the other two) are directly linkable as exported data symbols from every target FFI (P/Invoke, Rust `extern "C" { static ... }`, ctypes' `in_dll`, `dlsym`/`GetProcAddress` for the rest) — exactly how the internal codebase's own `Anvl` already works from C, with nothing extra to add. Simpler than a getter, and consistent with existing precedent rather than inventing new machinery.
   - **Flat exports** — every vtable entry has a mechanically-named, independently-documented, individually-callable `extern "C"` mirror: `Group.method` → `group_method`, e.g. `Anvil.load` → `anvil_load`. This is the guaranteed-universal baseline every language can always fall back to via ordinary, mature FFI (JNI, ctypes, N-API, `extern "C"` linking, P/Invoke) — not "vtable plumbing" demoted to an implementation detail. A Java binding, for instance, never needs to touch the vtable at all (JNI has no clean story for holding/invoking a raw C function pointer pulled out of a struct without extra machinery like JNA or the Java 21+ Foreign Function & Memory API) — it just binds the flat exports directly, at zero extra cost versus a world where the vtable didn't exist. That's what makes leading with the vtable in documentation/examples safe: it's pure upside for the languages that like it, and a genuine zero-cost opt-out for the ones that don't.
   - **Naming**: public ABI uses the full `anvil_` prefix, deliberately distinct from the internal codebase's existing abbreviated `anvl_` convention — `anvil_*` is the external, marketed surface; `anvl_*` stays internal-only. Never mix the two in a public header. This also means the new public vtable instance `Anvil` (full word) and the legacy `Anvl` (missing the second `i`, `include/anvil.h`) are two *different*, deliberately near-colliding symbols — an unfortunate but accurate consequence of that legacy file's own retirement (see its own header comment); `anvil_vtable.h` calls this out explicitly.
   - **Header split (implemented)**: three headers. `include/anvil_types.h` (opaque handles + the public `anvil_err_code` enum, includes nothing internal) → `include/anvil_flat.h` (the flat exports, includes only `anvil_types.h`) → `include/anvil_vtable.h` (the vtable struct types + `extern const` instances, includes only `anvil_types.h`, deliberately *not* `anvil_flat.h` — including one header alone never exposes the other calling style's names, so a file is forced to pick one, not drift into mixing them).
   - **Verification (implemented)**: mechanical, not manual — `test_anvil_vtable.c`'s `VT01`–`VT03` assert *pointer identity* between every vtable field and its flat counterpart (`Anvil.load == anvil_load`, etc.), not just matching behavior. A `VT04` end-to-end smoke test also proves the vtable is genuinely usable entirely on its own, never naming a single flat function.

5. **A resolved `$identifier` VarRef is fully transparent.** `anvil_value_get_type`/`get_text`/`get_count`/`get_element`/`get_statement` all report the *target's* own kind and content, never a distinct "this was a reference" type — a caller never has to know or care that a value came from a VarRef. An unresolved VarRef (missing target, or a reference cycle) reports as `ANVIL_VALUE_NULL`, matching the already-established policy that both cases are simply "unresolved," not an error.

6. **`get_text` on a `STRING` value resolves escape sequences; every other kind returns its raw source span.** A minimal, deterministic set — `\n \t \r \\ \"` — resolves to real bytes; an unrecognized escape (backslash followed by anything else) passes both characters through unchanged rather than erroring or silently dropping the backslash. Quotes are already excluded from `.text` by the parser itself. Chosen over a `get_number()`-style typed accessor deliberately: `NUMERIC` text is never interpreted by Anvil itself (a `UINT64_MAX`-scale value would lose precision as a `double`), so handing back the raw span and letting each language's own binding parse it however fits — `int64`, bignum, whatever — is the more "primitives not policy" choice.

## Implementation

**`Anvil` group — landed.** `anvil_load(filepath) -> anvil_document` runs the full pipeline in one call (`mod_ctx_initialize` → `doc_load_source` → `mod_ctx_register_doc` → `doc_scan_header` → `mod_load_imports` → `mod_ctx_create_arena` → `doc_parse_body` for every document in the import graph → `mod_resolve_context`), stopping at the first failing phase. Rather than translating whatever internal `anvl_err_code` a failing call happened to produce, `anvil_load` just records *which phase* it was in when something failed (`ANVIL_ERR_IO`/`HEADER`/`IMPORT`/`SYNTAX`/`RESOLVE`/`MEMORY`) directly on the handle — simpler than a translation table and immune to internal code churn by construction, not just by convention. A nested import's own header-scan failure is deliberately categorized as `ANVIL_ERR_IMPORT`, not `ANVIL_ERR_HEADER` — `mod_load_imports` scans each child's header as it recurses, and the *root* document's own header was fine, so "something went wrong resolving the import graph" is the more accurate framing from the caller's perspective.

`anvil_load` returns a non-NULL handle for every failure category above (`doc`/`ctx` stay alive and queryable, matching how the internal test helpers already behave after a failed parse) — `NULL` only for the one truly foundational case, the handle itself failing to allocate. One real ownership wrinkle worth remembering: if `doc_load_source` fails, the document was never registered with the context (`mod_ctx_register_doc` needs the source loaded first to compute its identity hash), so `mod_ctx_dispose` won't know to release it — `anvil_load` disposes it directly in that one path and clears the handle's reference, rather than leaking it. Valgrind confirmed this is the only such gap (0 leaks).

`include/anvil.h`'s old `Module`/`Anvl.load`/`.dispose`/`.get_root_path`/`.has_errors`/`.error_clear` — unimplemented stub, referenced only by the already-disabled `test_fixtures.c` — is retired. `Anvl.get_version()` stays (genuinely implemented, and `test_version.c` — which runs after every single test suite in this repo — depends on it); `anvil_get_version()` just delegates to it. This resolves the "replace in place or new file alongside" question below: new files (`anvil_types.h`, `anvil_flat.h`, `src/core/anvil_flat.c`) alongside a trimmed `anvil.h`, not a wholesale replacement — `AnvlMod`/`mod_initialize`/`mod_new`/etc. in `include/internal/module.h` are a *different*, still-active internal concept (an internal module wrapper, unrelated despite the similar name) and were never touched.

New fixtures: `anvil_err_header_dup_shebang.anvl` (duplicate `#!aml`), `anvil_err_resolve_duplicate.anvl` (imports `resolver_dup_import.anvl` and redeclares the same name) — reusing existing fixtures (`f01_bare_literal.anvl`, `body_err_unterminated_block.anvl`, `hdr_import_missing.anvl`) for the other categories.

**`Statement`/`Value` groups — landed.** `anvil_statement_get(doc, name)` looks up a top-level statement directly against `ctx->identifiers` (the resolver's own map — no separate lookup structure needed). `anvil_statement`/`anvil_value` are implemented as plain pointer casts of their internal counterparts (`anvl_statement`/`anvl_value`), not separately-allocated wrapper structs like `anvil_document` needed — safe because the caller can never dereference an opaque handle directly, and lifetime is already governed by the same arena/one-disposal-pass guarantee `.varref.resolved`'s pointer-aliasing relies on. `anvil_value_get_statement` reuses the exact same `Statement` accessors for walking an `OBJECT`'s nested fields as for top-level lookup — one consistent abstraction, not a separate name-based field accessor. `anvil_statement_get_name` exists specifically to make that nested case usable (a top-level caller already knows the name they looked up; an object-field walker doesn't).

**Found in the process — a real core-parser bug, not a public-API one.** `parse_string_literal`/`parse_blob_literal` (`src/core/parser.c`) both captured `.text.end` *after* consuming the closing delimiter, one byte too late — `.text` silently included the trailing `"`/`` ` `` itself. No existing body-parse test had ever asserted an exact `.text` length for a scalar to catch this; `anvil_value_get_text`'s precise byte-for-byte assertions (`ANV10`/`ANV11`) did immediately. Fixed in both (capture `end` into a local before consuming the delimiter) — see `document-body-parse.md` for the full writeup. Full regression (9 suites) stayed green after the fix, confirming nothing depended on the old, buggy behavior.

New fixture `anvil_accessors.anvl` covers every scalar/collection kind plus a VarRef to a scalar and a VarRef to an object, backing `ANV09`–`ANV17`.

**Vtable layer — landed.** `include/anvil_vtable.h` (types + `extern const anvil_i Anvil`/`anvil_statement_i Statement`/`anvil_value_i Value`) and `src/core/anvil_vtable.c` (each field assigned directly to its `anvil_flat.c` counterpart — a one-line literal per group, nothing else). Followed RED-first properly this time: the first stub populated every vtable field with `{0}` (all-`NULL`), which compiled fine but **segfaulted** rather than failing cleanly — `test_anvil_vtable.c`'s end-to-end smoke test called through a null function pointer. Not a genuine RED state per the established discipline ("must build and run, fail on assertions, not crash"), so the stub was corrected to point every field at a small safe placeholder function (mirroring how a stubbed *function* already gets a safe default body) before treating it as RED. New test file `test/unit/test_anvil_vtable.c` (`VT01`–`VT04`, 17 assertions) — pointer-identity checks per group plus one smoke test using only vtable-style calls.

**Document import iterator — landed, prerequisite for `notes/native-schema.md`'s types work.**
`anvil_document_get_imports(doc) -> anvil_document_iterator`, `anvil_document_iterator_next`,
`anvil_document_iterator_dispose` — a heapless HPS scan (`List.as_queryable` over
`doc->root->header->imports`) over a document's own *direct* imports only, mirroring
`anvil_statement_iterator`'s shape exactly, added because the public flat API previously only
ever exposed the root document — `import "file.types.anvl";` already fully loads and resolves
transitively today, but nothing let a caller reach an imported document's own statements/
attributes from outside core. First draft here also proposed a count+index pair before being
corrected back to an iterator — see the `anvil-prefer-sigma-hps-iterator` memory; this is the
second time that anti-pattern surfaced, worth staying alert for a third.

Real lifetime problem found and fixed before this shipped: each yielded `anvil_document` handle
shares its owning document's `module_context` (it has to, to read anything useful), but
`anvil_dispose` unconditionally tore down the *whole* context. A new `owns_ctx` bool on
`struct anvil_document_t` (true for every `anvil_load*`/`parse_value_fragment` handle, false for
one synthesized by the import iterator) makes `anvil_dispose` safe either way — true does the
full teardown exactly as before, false only frees the small handle itself. First implementation
attempt had the iterator itself track and free every handle it ever yielded, *in addition to*
documenting that a caller could safely dispose one directly — those two ownership models
collide: a test that disposed a yielded handle directly, then disposed the iterator, hit a
genuine double-free (caught immediately, not silently). Resolved by picking one model instead of
two: the iterator never tracks yielded handles at all; each one is the caller's own to dispose
via ordinary `anvil_dispose`, same as any other document, safe because of the `owns_ctx` flag.
`ANV32`/`ANV33` (`test_anvil_native.c`), `VT09` (`test_anvil_vtable.c`), Valgrind-clean, full
13-suite regression green.

**`anvil_statement_get_value` now supports anonymous OBJECT_BLOCK statements too.** Previously
NULL for `ident { ... };` (no `:=`) — its own doc comment said so, "not yet a supported
traversal." Raised while building `types.c`: the first `.types.anvl` fixture draft used exactly
this form (matching FlyWire's real `.meta.anvl` shape) and silently registered nothing. Fixed at
the source, not the accessor: `parse_statement` (`src/core/parser.c`) now synthesizes a real
`ANVL_VALUE_OBJECT`-kind value for an OBJECT_BLOCK statement too, right after its body parses,
`.object.statements` aliasing the exact same `list` as `.body` (never a copy) — the accessor
itself simplified back down to `if (!s) return NULL; return s->value;`, no kind gate needed at
all, since both statement forms now populate `.value` identically. This was already the
resolver's own internal model (`resolver.c`'s `own_fields()` already treated `.body` and
`.value->object.statements` as interchangeable) — the public API was the one place still treating
them as different.

**Real bug found and fixed along the way: a double-free from that same aliasing.**
`mod_ctx_dispose` runs two independent disposal passes — one walks every registered OBJECT-kind
*value* and disposes `.object.statements`; another walks every *statement* and disposes `.body`
directly. Aliasing the same list into both an OBJECT_BLOCK statement's `.body` and its new
synthesized value's `.object.statements` meant both passes now freed the identical pointer — a
crash caught immediately by the new test (`ANV34`, `test_anvil_native.c`), not a silent
corruption. Fixed with a one-line guard in the statement-walk pass: skip disposing `.body`
directly whenever `stmt->kind == ANVL_STMT_OBJECT_BLOCK && stmt->value` (the value-walk pass
already owns that dispose in the normal case), keeping the direct dispose only as a defensive
fallback for the case where synthesis somehow didn't happen. `ANV34`, Valgrind-clean, full
14-suite regression green (including a full backtrace confirming the crash site before the fix,
not just its absence after).

## Bindings — organization and sequencing

**Resolved — question 3 of the founding framing (audience sequencing) and question 3's "popular ABIs first" open item.** Node/JS is first, driven by a real, waiting consumer: `../flywire/` (a schema-aware binary data-transfer protocol) currently vendors a copy of `anvil.js`'s CJS build directly into `src/anvl/anvl-browser.js`, and that vendoring's own comment already states the intent — *"the native C binding / WASM path remains the longer-term migration target, deferred."* `anvil.js` itself is to be deprecated immediately once a real Node binding exists.

**Each binding gets its own repo**, `anvil.<lang>` (matching `anvil.js`/`anvil.net`'s existing naming and the `sigma.*` sibling-repo convention) — repo/package names follow each ecosystem's own norms (npm, PyPI, NuGet, CRAN, vcpkg/Conan), while the **marketing moniker stays uniform**: Anvil.C (this repo, the native parser), Anvil.CPP, Anvil.JS, Anvil.Py, Anvil.R, Anvil.Net.

**Binding tests stay in each binding's own repo**, written in that ecosystem's native test idiom (Jest, pytest, xUnit, Catch2, testthat) — scoped to the marshaling/wrapper layer only (handle lifecycle, type conversion, GC-safety). Parse correctness is never re-tested there — this repo's own suite already owns that, and a binding is *just* a thin layer over `anvil_flat.h`/`anvil_vtable.h`, so its own tests should prove nothing more than "the wrapper calls through correctly and doesn't leak/corrupt across the FFI boundary."

**Docs live with the code** — each binding repo owns its own install/quickstart/API-reference docs, with the anvldata.com site aggregating/rendering them rather than maintaining a separate, driftable copy. The website rework itself (moniker-consistent framing, Anvil.C front and center) is real and wanted but sequenced *after* the Node/JS work, not blocking it.

**Node/JS binding tech: N-API first, WASM second.** A native N-API addon (direct FFI-speed access to a compiled `libanvil`, matching flywire's actual context — server-side Node, not a browser) lands first; a WASM build follows for the browser use case `flywire-client.js` already anticipates (`window.anvl`) but — per that file's own comment — has "not been exercised yet, revisit when browser-side FlyWire code is actually built."

### What flywire's real usage revealed — three gaps to close before the binding work starts

Investigated `../flywire/`'s actual call sites (`src/table.js`, `src/schema-registry.js`, `src/anvl/index.js`) rather than assuming. The real, minimal surface flywire depends on:

```js
anvl.parse(source)        // full document parse — reads @[schema]-attributed .meta.anvl files
anvl.parseRawValue(text)  // Value Fragment decode — no VarRef support (anvil.js's own
                          // parseRawValueCore: "no registry here for a VarRef to resolve against")
anvl.lastError()          // error retrieval
```

Three things Anvil Native didn't have yet, closed in this order (each RED-first) — **all three now landed**, so the Node binding repo (N-API first) is next:

1. ~~**Buffer-based load.**~~ **Landed.** `anvil_load_buffer(source, length) -> anvil_document` shares its whole pipeline with `anvil_load` via a new internal `load_common(origin, source, length, register_label)` helper — the only difference between the two is `ANVL_SOURCE_FROM_FILE` vs. `ANVL_SOURCE_FROM_BUFFER` and what gets registered as the document's symbolic name (the real filepath for a file load; a fixed `"<buffer>"` placeholder for a buffer load, which has no real path). `length` is honored exactly — not NUL-terminator-dependent, confirmed by `ANV21` (a buffer with deliberately-invalid trailing content past `length`, which parses clean because that content is never read). Mirrored into the vtable too (`Anvil.load_buffer`) — a one-line addition to `anvil_i` easy to miss, caught immediately by `VT01`'s existing pointer-identity check once the field and its test assertion were added. `test/unit/test_anvil_native.c` `ANV18`–`ANV21` (4 cases, clean/syntax-error/NULL-source/length-respected). Full regression green, Valgrind-clean.
2. ~~**Module/document attributes.**~~ **Landed, and built generic as agreed** (not narrowly scoped to just flywire's `@[schema]` case — see the `native-schema.md` connection). `anvil_attribute` is a new opaque type (plain pointer-cast of the internal `anvl_attribute`, same reasoning as `anvil_statement`/`anvil_value`). Enumeration (`anvil_document_get_attribute_count`/`get_attribute`, `anvil_statement_get_attribute_count`/`get_attribute`) and by-key lookup (`anvil_document_find_attribute`, `anvil_statement_find_attribute`) both exist — enumeration matches the count+index idiom already used everywhere else in this API (arrays/tuples/objects), by-key lookup is `Source.slice_equals`-based and happens to be exactly flywire's own `@[schema]` check, without being schema-specific policy. Shared `anvil_attribute_get_key`/`get_value` readers work on either module- or statement-level attributes, since both are the same underlying `anvl_attribute_t{key, value}` shape — statement attributes got the same treatment as module attributes in this one pass, since it was the same mechanism either way (`stmt->attributes`, already fully parsed/tested internally, e.g. `AML12`, but never publicly exposed until now).

   Two new vtable groups fell out of this mechanically, following the naming law precisely (`anvil_document_*` → `Document`, `anvil_attribute_*` → `Attribute`) rather than overloading the existing `Anvil`/`Statement` groups — `Statement` also gained its 3 new attribute fields directly. `test/unit/test_anvil_native.c` `ANV22`–`ANV26` (26 assertions) plus `test_anvil_vtable.c` `VT05`/`VT06` (new groups) + 3 new `VT02` assertions. Full regression green, Valgrind-clean.

   **Found along the way — a real, previously-untested grammar gap, not a bug in this new code.** `f09_doc_attributes.anvl` (a pre-existing fixture, confirmed never actually used by any test before this) has attribute keys with hyphens/dots/colons (`@[doc-level]`, `@[hasDot.Notation, hasColon:Separator]`) — but `header_scan_attributes` (`src/core/document.c`) scans attribute keys with `Source.is_identifier_part` (alpha/digit/underscore only), not the more permissive `is_bare_literal_part` (which does allow `-`/`.`/`:`/`$`). So this fixture's header scan actually fails partway through (`ANVL_ERR_PARSER_UNEXPECTED_TOKEN`) once it hits the hyphen — never previously caught since nothing exercised it. Sidestepped for this slice with a new, clean fixture (`anvil_doc_attributes.anvl`, identifier-safe keys only) rather than deciding the grammar question unilaterally. **Open**, not resolved: should attribute keys be as permissive as bare literals (matching the evident intent of the orphaned fixture's naming), or is identifier-only correct and the fixture simply stale/aspirational? Not blocking — flywire's own actual need (`@[schema]`) is a plain identifier either way.
3. ~~**Value Fragment parsing.**~~ **Landed.** `anvil_parse_value_fragment(text, length) -> anvil_document` + `anvil_document_get_fragment_value(doc) -> anvil_value`, matching `anvil.js`'s `parseRawValueCore` precedent (flywire's real, currently-only usage is the bare-value shape — `[(v1,v2,...), ...]` — never the `name := value` shape `parseRawValue` also supports there, so only the bare-value shape was built here, honoring the scope this entry already recorded rather than adding the second shape speculatively).

   Two layers: an internal `anvl_parse_value_fragment(module_document, anvl_value *, anvl_err_code *)` (`src/core/parser.c`) that reuses `parse_value_body` completely unchanged — every existing grammar rule (empty collections, tuple arity, AMP's scalar-only element rule, AMP's object ban, unterminated strings...) is inherited for free, no second copy of that validation — wrapped by the public `anvil_parse_value_fragment` (`src/core/anvil_flat.c`), which spins up a minimal context/document/arena with no header scan or import loading at all (a fragment has no header) before calling in. `anvil_document_t` gained one new private field, `fragment_value`, invisible to every binding since the struct is fully opaque; every other document-producing path (`load_common`) explicitly zeroes it so an ordinary loaded document safely reports no fragment value rather than uninitialized garbage.

   A trailing `;` is tolerated but never required (a fragment isn't a statement); anything else trailing after the value is a hard error. No VarRef support at any nesting depth, enforced by reusing the flat `doc->context->values` index (every value node allocated during the parse, at any depth, lands there already — see `mod_ctx_dispose`'s own comment in `module.c`) rather than a hand-rolled recursive tree walk: after a successful parse, one pass over that index rejects if any node is `ANVL_VALUE_VARREF`, via a new dedicated `ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_FRAGMENT`. Failures surface publicly as `ANVIL_ERR_SYNTAX`, the same category `load_common` already uses for a body-parse failure — no new `anvil_err_code` category needed.

   Vtable mirror: `Anvil.parse_value_fragment`, `Document.get_fragment_value`. `test/unit/test_value_fragment.c` — `VFP01`–`VFP06` (internal layer, isolating the parser mechanism) + `VF01`–`VF12` (public layer: every scalar kind, array/tuple/object, whitespace tolerance, trailing-content and VarRef rejection at both depths, and a non-fragment document safely reporting no fragment value), plus `test_anvil_vtable.c` `VT01`/`VT05`/`VT04` extensions. Full regression green (13 suites), Valgrind-clean.

   **Process note**: the internal function was first written in full before a RED state was confirmed for it — caught and self-corrected before landing (stubbed it back out, confirmed genuine RED for all 6 `VFP*` cases, then restored the real implementation) rather than letting the ordering slide. Also surfaced and fixed a real, separate bug while wiring error propagation: `parser_set_error`'s own call into `Source.set_error` clobbers `parser.err_code` back to `ANVL_ERR_NONE` as a side effect (that out-param reports whether *recording* the error succeeded, not the error itself) — the same reason `doc_parse_body` already reads the real code back from `doc->context->errors` instead of trusting `parser.err_code` after a failure. This function now does the same (`last_context_error` helper), rather than the naive direct read that silently returned `ANVL_ERR_NONE` on every real failure.
4. ~~**Document-level statement enumeration.**~~ **Landed, as a heapless iterator, not count+index.** `anvil_document_get_statements(doc) -> anvil_statement_iterator`, then `anvil_statement_iterator_next(it, &out_stmt)` and `anvil_statement_iterator_dispose(it)`. Found while sketching `anvil.node` against flywire's real `schema-registry.js` usage: `anvil_statement_get(doc, name)` only ever supported look-up-by-name (via the resolved identifier map), so a caller needing every top-level field positionally (field names aren't known ahead of time when reading a `*.meta.anvl` schema) had no way to enumerate them.

   The first pass at this landed as a `get_statement_count(doc)`/`get_statement(doc, index)` pair — reverted before this note was even written, on repo-owner correction: Sigma's `Query`/`sc_queryable` mechanism (`include/sigma/query.h`, `FR-2603-sigma-collections-006`) is the established idiom for exactly this ("get a count, find an identifier, etc.," heapless — no allocation to scan), and the public ABI enumeration surface should be built on it directly rather than a hand-rolled count+index pair that only *internally* happened to reuse `FArray.capacity`/`FArray.get`. `anvil_statement_iterator_t` wraps one `sc_queryable` (produced by `FArray.as_queryable(doc->root->body, sizeof(anvl_statement))`) — the one necessary heap allocation is the opaque-handle box itself (consistent with every other handle here being a pointer typedef), not the scan mechanism, which stays genuinely heapless underneath. `doc->body` is frozen on both a successful *and* a failed body parse (partial results stay inspectable, by existing design) — so a failed parse still yields a real, usable iterator that simply exhausts immediately if nothing was captured before the failure; `NULL` is reserved for `doc` itself being invalid or never having reached body-parsing at all. `test/unit/test_anvil_native.c` `ANV27`/`ANV28`. Vtable mirror: `Document.get_statements`, plus a new `StatementIterator` group (`next`/`dispose`) (`test_anvil_vtable.c` `VT05`/`VT08`).
5. ~~**Detailed error diagnostics.**~~ **Landed.** New opaque `anvil_error` handle (matching `anvil_attribute`'s shape: its own dedicated wrapper struct, not a pointer-cast, since no internal type already bundles a stable category together with specific diagnostic detail) — `anvil_document_get_error(doc) -> anvil_error`, then `anvil_error_get_category`/`get_message`/`get_line`/`get_column`. Closes the other gap found sketching `anvil.node`: `anvil_get_error(doc)` only ever returned the coarse pipeline-phase category (`ANVIL_ERR_SYNTAX`, etc.), never a message or source position, even though the internal `anvl_err_state_t{code, message, line, column, file}` already carries all of it (confirmed "first-error-wins" — `anvl_error_set` is a no-op once one error exists for a document, so there is never more than one to retrieve). Deliberately does *not* expose the specific internal `anvl_err_code` number itself, only its static message text — internal code churn still can't become a public ABI break through this handle, preserving the original category-narrowing decision's whole point; `anvil_get_error(doc)` remains the one thing safe to branch logic on, `anvil_error` is richer, best-effort diagnostic detail layered on top. A category with no real source position (`ANVIL_ERR_IO` — a file-system failure has nothing to record a line/column against) still gets an `anvil_error` object, just with `line`/`column` reading `0` and `message` empty, rather than some categories having detail and others returning `NULL` outright. `test/unit/test_anvil_native.c` `ANV29`–`ANV31`. Vtable mirror: new `Error` group (`get_category`/`get_message`/`get_line`/`get_column`), plus `Document.get_error` (`test_anvil_vtable.c` `VT07`, `VT05` extension).
6. **`parser.err_code` clobbering, root-fixed** (found and traced while designing item 5, not a new feature). `parser_set_error` calls `Source.set_error(src, code, ..., &parser.err_code)`, and that out-param reports whether *recording* the error succeeded (always `ANVL_ERR_NONE` on success, including a first-error-wins no-op inside `anvl_error_set`), not the error itself — so it always resets `parser.err_code` back to `ANVL_ERR_NONE` as a side effect of the call, on *every* invocation. Two layers of this bit real code:
   - `parse_source`'s own failure branches called `Source.finish_body(ctx->src, ctx->statements, &parser.err_code)` immediately after a real failure was recorded — same clobbering pattern, second call site.
   - Once both of those were fixed (reasserting the real code after the call, and using a throwaway out-param for `finish_body`), a *third*, deeper issue surfaced: `parser.err_code` has no first-error-wins protection of its own, unlike `ctx->errors`. `parse_statement`'s unconditional generic fallback (`ANVL_ERR_PARSER_EXPECTED_IDENTIFIER`, fired whenever `parse_identifier` returns false *for any reason*) was silently overwriting a more specific error `parse_identifier` itself had just recorded (e.g. `ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD`) — exactly the scenario `anvl_error_set`'s own doc comment already warns about for `ctx->errors`, just happening a second time to the *other* copy of "the error." Fixed by giving `parser_set_error` the same first-error-wins discipline: capture `parser.err_code`'s value *before* calling `Source.set_error` (reading it *after* would only ever see the value that call just reset), and restore that prior value — not just decline to overwrite it — when one was already recorded.

   This had zero prior observable impact — the only existing caller of the parser's own `anvl_get_error()` (`test_body_amp.c`'s `AMP00c`) only ever checked the success case, where `parser.err_code` was correctly `ANVL_ERR_NONE` regardless. Caught only because a *new* caller (`anvil_parse_value_fragment`, gap #3) needed to trust `parser.err_code` directly and got `ANVL_ERR_NONE` on every real failure until this was found. `doc_parse_body`/`anvl_parse_value_fragment` already worked around the outer symptom by reading the real code back from `doc->context->errors` instead — this fix means new code no longer has to independently rediscover that workaround. New regression coverage: `test_body_amp.c` `AMP00b` (extended — now also asserts `anvl_get_error()` itself, not just `doc_parse_body`'s own out-param).

### FR — FlyWire's required `anvil-node`/`anvil-wasm` surface, post-parity

**ID:** FR-flywire-binding-surface-001
**Type:** Feature Request
**Owner:** anvil-node / anvil-wasm (one ask, identical surface — see "Why one ask, not two" below)
**Filed:** 2026-09-10
**Status:** closed — implemented and merged to `main` on both `anvil-node` and `anvil-wasm`
**Requested by:** FlyWire (`../flywire/`)
**Tags:** bindings, anvil-node, anvil-wasm, anvlnode, surface

#### Summary

Both bindings just reached parity on their initial planned surface (`getVersion`/`parseRawValue`/`parse`/`lastError`, `AnvlNode`'s `has`/`get`/`hasAttribute`/`entries`/`count`/`at`/`asString`/`asBool`/`asInt`). This FR is the follow-up round of exactly the investigation that produced items 1–6 above — re-verified against FlyWire's *current* real code (it's grown substantially since that investigation: full CRUD, a write-validation taxonomy, a schema-format redesign) rather than assumed still accurate. Method: every file in FlyWire that `require()`s its ANVL dependency was grepped for every call made on it, cross-checked by hand against false positives (plain JS objects that happen to share a method name — a schema field's own `.type`/`.field` properties, `Map.get()`, etc.) — not sampled, not remembered from an earlier pass.

**Net result: the currently-planned/built surface already covers everything FlyWire needs, with one real, precise exception (below).** This FR exists to (a) confirm that coverage with real, load-bearing usage samples per surface member, so it's clear this is a verified need and not a guess, and (b) name the one gap precisely enough to close it without further back-and-forth.

**Why one ask, not two:** FlyWire's own repo owner's framing — a browser client's needs are the same shape as the server's (parse a schema once, decode responses the same way) — so every sample below is real, currently-exercised *server-side* Node code, and the ask is that both bindings expose the identical surface, not that this FR is scoped to Node only.

#### The verified surface

**3 module-level functions** — already landed on both bindings, per the Status table in each README:

- `parse(text)`
- `parseRawValue(text)`
- `lastError()`

**10 `AnvlNode` instance members** — 9 already landed; `.type` is the one gap (see below):

- `.get(key)`, `.has(key)`, `.hasAttribute(key)`, `.entries()`, `.at(index)`, `.count`, `.asString()`, `.asInt()`, `.asBool()` — landed.
- `.type` — **not confirmed landed; see "The gap" below.**

**Confirmed, by the same grep, as *not* needed** — no real call site anywhere in FlyWire justifies `asFloat`, `isNull`, `keys`, `field`, `is`, `hasBase`, `baseIdentifier`, `asBuffer`, or the singular value-returning `attribute(key)` (only the boolean `hasAttribute(key)` check is ever used). Listed explicitly so silence isn't read as an oversight.

#### Real usage samples — this is load-bearing code today, not a wishlist

`parse()` + `hasAttribute()` — schema self-description check, `schema-registry.js`'s `loadFromText()` (the real client-side schema-fetch path, not just the server's local-file path):

```js
const root = anvl.parse(source);
if (!root) {
    const err = anvl.lastError();
    throw new Error(`Failed to parse ${sourceLabel}: ${err.message}`);
}
if (!root.hasAttribute('schema')) {
    throw new Error(`${sourceLabel} has no @[schema] attribute — not a *.meta.anvl schema file?`);
}
```

`entries()` + `has()`/`get()`/`asString()`/`asInt()`/`asBool()` — the real per-field schema-parsing loop, same file, reading a generated `*.meta.anvl`'s top-level anonymous-object fields:

```js
for (const [fieldName, fieldNode] of root.entries()) {
    if (fieldNode.has('pooled') && fieldNode.get('pooled').asBool()) {
        type = 'pooled';
    } else {
        type = fieldNode.get('type').asString();
        size = (type === 'int32' || type === 'date') ? 4 : fieldNode.get('size').asInt();
    }
    const required = fieldNode.has('required') && fieldNode.get('required').asBool();
    // ...
}
```

`at()` + `count` — reading a schema field's own `values`/`mask` array (an enum's legal values, or a bitmask's value set — both generated from a real Postgres `CHECK` constraint):

```js
if (fieldNode.has('values')) {
    const valuesNode = fieldNode.get('values');
    values = [];
    for (let i = 0; i < valuesNode.count; i++) values.push(valuesNode.at(i).asString());
}
```

`parseRawValue()` + `lastError()`'s real disambiguation contract — `table.js`'s `decode()`, and the exact reason a `null` return can't be trusted alone:

```js
decode(text, schema) {
    const rows = anvl.parseRawValue(text);
    if (rows === null) {
        const err = anvl.lastError();
        if (err) throw new Error(`Table.decode(): parse failed: ${err.message}`);
        return []; // encode()'s own empty-table case, not a failure — a genuine ANVL null decodes to JS null too
    }
    return rows.map(tupleValues => { /* ... */ });
}
```

#### The gap: `.type`

`harness/server.js`'s CREATE handler dispatches single-row vs. batch insert on the request body's own shape — real, currently-shipped code, not planned:

```js
if (setNode.type === 'array') {
    // Multi-row CREATE: set{} as an array of rows instead of one.
    // ...
} else {
    // Single-row CREATE.
}
```

`anvil.js` (the pure-JS parser both bindings are meant to deprecate) exposes this as a plain getter — `get type()`, returning `'object'`/`'array'`/`'tuple'`/`'scalar'`/`'blob'`, or `null` for an unset/invalid node — and FlyWire's code above depends on exactly that distinction (`'array'` vs. `'object'`) to know whether a request body is one row or many.

**Why this wasn't already in the ask:** item 1 above (and each binding's own README) describes the investigation as covering `src/table.js`, `src/schema-registry.js`, `src/anvl/index.js` — an accurate list *at the time*, but `harness/server.js` (where CREATE, and this dispatch, actually live) didn't exist yet when that investigation happened, or wasn't in scope. Not a mistake in the original work — a real gap that opened up as FlyWire grew past that snapshot, found now by re-verifying rather than assuming the old investigation still covers current reality.

**What's expected to close it:** `AnvlNode` (both bindings, identical) exposes a `.type` property (or `.getType()`/`.kind` if a property getter doesn't fit the binding's own idiom better — naming is the binding owner's call, not prescribed here) returning the same small, closed set of strings `anvil.js`'s `get type()` already returns for the *root* node and any node reachable via `.get()`/`.at()`. FlyWire's own real need only exercises `'array'` vs `'object'` at the moment, but the full set (including `'tuple'`/`'scalar'`/`'blob'`) costs nothing extra to expose uniformly, and closes exactly this kind of narrow-investigation gap from recurring on the next thing FlyWire builds.

#### Non-goals — deliberately excluded from this ask

- **A plain-JS-value → ANVL-text serializer** (the mirror of `parseRawValue()`, needed for safely constructing `set{}`/`where{}` request bodies client-side without hand-escaped string templating). Real gap, found in the same review — but there is no real consumer for it in FlyWire yet (no browser client exists; the one place that currently builds request text, `harness/client.js`, is test-harness code, not a production client), so per FlyWire's own standing rule against wishlist asks, it's tracked as FlyWire's own internal follow-up instead, revisited if/when a real client actually needs it.
- **AnvilSchema / the type system** — real and implemented at the `anvil` core layer, but nothing in FlyWire's current code needs it exposed through either binding yet. Not part of this ask.
- Everything in the "confirmed... not needed" list above.

### Response to FR-flywire-binding-surface-001 — `.type`, findings and chosen approach

**Confirmed and re-scoped first.** An earlier, informal read of this thread assumed FlyWire had
found real gaps in AnvilSchema/type-system exposure through the bindings. Re-checked directly
against this FR's own text: the "Non-goals" section above is explicit — *"AnvilSchema / the type
system — real and implemented at the anvil core layer, but nothing in FlyWire's current code
needs it exposed through either binding yet. Not part of this ask."* FlyWire confirmed this
directly when asked. The only actionable gap in this FR is `.type`. Everything below is scoped
to that alone.

**Why FlyWire has "been returning type all along," and why the bindings haven't.** `anvil.js`'s
own `AnvilNode` (`src/node.js`) wraps the parser's internal AST node object directly:

```js
export class AnvilNode {
   constructor(astNode) { this._ast = astNode; }
   get type() { return this._ast ? this._ast.type : null; }
```

Every AST node `anvil.js`'s pure-JS parser builds already carries a `.type` field as part of its
own shape — nothing was ever lost, because nothing ever got flattened. `anvil-node`/`anvil-wasm`
took a different path: their native-conversion code (`AnvilValueToJs` in `anvil-node`'s
`binding.c`, and its `anvil-wasm` counterpart) already reads Anvil Native's own
`anvil_value_get_type()` — which distinguishes `ARRAY`/`TUPLE`/`STRING`/`BLOB` just as precisely
as `anvil.js`'s own AST does — but only to *choose a conversion branch*, then discards the
result. `ARRAY` and `TUPLE` both become a bare JS array; `STRING`/`BLOB`/`IDENTIFIER` all become
a bare JS string. The distinction Anvil Native itself never loses gets thrown away one layer up,
inside each binding's own glue code.

**Why fixing the representation, not adding an inference method.** `.type` corresponds to a real
primitive Anvil Native already computes and exposes (`anvil_value_get_type()`) — this is data,
not behavior to invent. The alternative — a method that guesses a value's ANVL kind after the
fact from its already-converted JS shape (`Array.isArray()`, `typeof`, some heuristic to tell a
blob's string from an ordinary one) — would be manufacturing policy to compensate for a primitive
that should have been passed through in the first place, and it would be genuinely unreliable for
the exact distinctions that matter here (a converted array and a converted tuple are
indistinguishable JS values once conversion has already thrown the tag away; no amount of
post-hoc inspection recovers it). The fix is to stop discarding the primitive during conversion,
then expose it as a plain, honest property — the same shape `.count` already has today, backed by
real underlying data, not a derived guess.

**One clarification worth naming explicitly: "eager conversion" and "flatten to an untagged
value" were never actually the same decision, even though they landed together.**
`anvil-node`'s own README documents exactly why `parse()` converts eagerly — entirely to avoid
holding a live `anvil_document` handle across GC-uncertain timing, after a real bug surfaced
where that collided with Anvil Native's own import-registry dedup. That reasoning has nothing to
do with whether the *result* of eager conversion is a bare value or a lightly tagged one —
`anvil.js`'s own AST tree is proof: also fully eager, also plain JS, zero native handles, and it
tags every node anyway. Fixing `.type` doesn't touch eager conversion, doesn't reopen the
GC/registry finding, and doesn't touch Anvil Native's core at all — it's entirely inside each
binding's own existing conversion code.

**Chosen shape: tag every converted value, not just the root, collapsed to the 5-value set.**
`OBJECT → 'object'`, `ARRAY → 'array'`, `TUPLE → 'tuple'`, `BLOB → 'blob'`, everything else
(`NULL`/`BOOL`/`NUMERIC`/`STRING`/`IDENTIFIER`) `→ 'scalar'` — matching `anvil.js`'s own
`NodeType`/`ScalarKind` split exactly (this ask is `.type` only; the finer `ScalarKind` breakdown
stays out of scope, unrequested). An unresolved VarRef needs no special-casing — already
transparently `ANVIL_VALUE_NULL` by the time conversion sees it, an existing decided behavior.
Each binding's own native-conversion layer wraps every value (at every nesting level a `.get()`/
`.at()` can reach) as `{ type, value }` instead of a bare value — `anvil-node` via its existing
per-value N-API object construction, `anvil-wasm` via one extra key in the JSON string it already
builds in C. `AnvlNode`'s *existing* public methods (`.get`, `.has`, `.hasAttribute`, `.entries`,
`.at`, `.count`, `.asString`, `.asInt`, `.asBool`) keep identical signatures and behavior on both
bindings — this is purely additive, one new getter backed by richer internal representation.
Building the full 5-value set costs nothing beyond building the one proven distinction — the
collapse mapping produces all five for the same per-value cost as one, so there's no narrower
version of this mechanism worth building instead.

**What FlyWire gets — sketch against the real, already-written use case.** `harness/server.js`'s
CREATE handler (quoted in this FR above) already assumes this API exists:

```js
if (setNode.type === 'array') {
    // Multi-row CREATE: set{} as an array of rows instead of one.
} else {
    // Single-row CREATE.
}
```

No change needed on FlyWire's side — that code already works, unmodified, once this lands. The
full set is available for whatever comes next, not just the one case proven today:

```js
const root = anvl.parse(source);
root.type;                          // 'object' — every *.meta.anvl document's root
root.get('tags').type;              // 'array'  — set := [ ... ];
root.get('coords').type;            // 'tuple'  — set := ( ... );
root.get('label').type;             // 'scalar' — a string, number, bool, or null field
root.get('payload').type;           // 'blob'   — an @tag`...` value
root.get('missingField');           // null — .get() itself already reports absence this way,
                                     // never reaching a .type call in the first place
```

Not part of this response: an implementation timeline. This is the design this FR's ask
converges on; picking it up is a separate step.

#### Closed — implemented on both bindings

Picked up on branch `feat/anvlnode-type` in each repo, TDD throughout, merged to `main` on both
`anvil-node` (`31dba3d`) and `anvil-wasm` (`8e76131`). Implementation matches the response above
exactly — nothing changed in the design between sketch and code:

- `src/binding.c` in each binding gained a second, parallel conversion path used only by
  `parse()` (`AnvilValueToTaggedJs` in `anvil-node`; `append_anvil_value_json_tagged` in
  `anvil-wasm`) — `AnvilValueToJs`/`append_anvil_value_json`, `parseRawValue()`'s own
  conversion, is untouched and regression-tested to prove it.
- `AnvlNode` (`lib/index.js`, identical on both) now holds `{ type, value }` instead of a bare
  value; `.type` is a new getter, and `has`/`get`/`entries`/`count`/`at` read the explicit tag
  instead of inferring shape from `Array.isArray`/`typeof`.
- RED confirmed first on both (real assertion failures, not a build error) before any
  implementation; GREEN after: **29/29** (`anvil-node`, Valgrind-clean, 0 errors/0 leaked) and
  **30/30** (`anvil-wasm`, built and verified on the real Emscripten toolchain, not just
  locally).

**The practical use case this closes, restated for FlyWire directly** — `harness/server.js`'s
CREATE handler, quoted earlier in this FR, needs no changes at all:

```js
if (setNode.type === 'array') {
    // Multi-row CREATE: set{} as an array of rows instead of one.
} else {
    // Single-row CREATE.
}
```

That code already assumed this API; it now just works. The full closed set is available for
whatever comes next:

```js
const root = anvl.parse(source);
root.type;                  // 'object' — every *.meta.anvl document's root
root.get('tags').type;      // 'array'  — set := [ ... ];
root.get('coords').type;    // 'tuple'  — set := ( ... );
root.get('label').type;     // 'scalar' — a string, number, bool, or null field
root.get('payload').type;   // 'blob'   — an @tag`...` value
```

Both packages are unpublished (private, no npm registry) — pick up `main` on each repo directly
to get this.

**One real, small thing FlyWire found while verifying, fixed on `main` (`anvil-wasm`
`3b5975e`):** `anvil-wasm`'s own `package.json` ran `"test": "node --test test/"` — a trailing
directory argument that fails outright on Node 22 (`MODULE_NOT_FOUND`; `node --test` no longer
accepts a bare directory the way it used to). `anvil-node`'s equivalent script was already
correct (`"node --test"`, no path). The tests themselves were never wrong — confirmed 30/30
either way — this only broke the `npm test` wrapper silently, for anyone who ran it without
digging into why. Now matches `anvil-node`'s form on both repos.

## Open questions

- Anonymous (`OBJECT_BLOCK`) statement field traversal — `anvil_statement_get_value` returns NULL for one today (it genuinely has no `.value`, only `.body`); nobody's asked for this traversal yet, so it stays out of scope until they do.
- What the API deliberately does *not* expose (question 2 of the founding framing) — not yet worked through in concrete terms beyond "no raw structs" (decision 2 above already answers part of this).
- **Attribute value permissiveness** — values stay fully permissive today (any byte up to an unquoted `,`/`]`, e.g. a hyphenated value round-trips verbatim). Whether values should tighten further is still genuinely open, pending real usage data — no lean either way yet. (Key strictness is *not* open — see the resolved decision below; keys being tighter than values is intentional, not an asymmetry to fix.)

## Decisions (grammar)

7. **Attribute key character set is identifier-only, confirmed intentional.** `header_scan_attributes` (module-level) and `parse_attribute_list` (statement-level) are byte-for-byte identical: keys accept alpha/digit/underscore, never a leading digit (`is_identifier_start` excludes digits — a bare `1bad` key fails immediately with `ANVL_ERR_PARSER_INVALID_IDENTIFIER`, before any content is consumed); a leading underscore is fine (`_leading`), case is not discriminated (`CamelCase` passes), and a single-character key is fine (`x`). A hyphen, dot, colon, or slash anywhere in a key is rejected — the key scan truncates at the bad byte (e.g. `has-hyphen` captures a key of just `has`), then the leftover byte fails to match `=`/`,`/`]`, aborting the whole attribute list with `ANVL_ERR_PARSER_UNEXPECTED_TOKEN`. `f09_doc_attributes.anvl`'s hyphenated/dotted keys were simply stale/aspirational, not a design target — no change made to loosen keys toward `is_bare_literal_part`.

8. **A `'$'`-led attribute value is a hard parse error, not silently-accepted literal text.** Attribute values have no VarRef mechanism — `'$'` only triggers `parse_varref` inside `parse_value_body` for real statement values (`src/core/parser.c:608`), and `mod_resolve_context` never walks `doc->header->attributes` or `stmt->attributes` at all. Rather than let `@[owner=$admin]` silently become the literal 6-byte string `"$admin"` (a caller who typo'd a VarRef into an attribute would never find out), both attribute-value scanners now reject a leading `'$'` outright with a new dedicated code, `ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_ATTRIBUTE` (`errors.h`), at both module- and statement-level. The rejection is unconditional on what follows the `'$'` — `$5` is rejected exactly like `$var`; this is "no leading `$`, full stop," not "reject only things that look like a plausible VarRef attempt." Real VarRef support for attribute values was considered and explicitly not built: it would need new representation on `anvl_attribute` plus resolver changes it doesn't have today, real cost for a mechanism nothing currently requires — this decision only closes the silent-surprise gap, it doesn't rule out real support if a concrete need shows up later. Covered by `test/unit/test_attribute_grammar.c` `ATG_M11`/`ATG_M12`/`ATG_S11`/`ATG_S12` (24 cases / 68 assertions total in that file, all green, Valgrind-clean).

## Related notes

- `document-header-scan.md`, `document-body-parse.md`, `resolution-phase.md` — the four implemented pipeline phases this API needs to expose access to.
- `deferred-work.md` — for anything raised here that ends up deferred rather than decided now.
