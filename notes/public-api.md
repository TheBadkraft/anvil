# Public API — Anvil Native

## Status

**First slice implemented and GREEN.** The `Anvil` lifecycle group — `anvil_load`/`anvil_dispose`/`anvil_has_errors`/`anvil_get_error`/`anvil_get_version` — is real, tested (`test/unit/test_anvil_native.c`, 8/8, 22 assertions, Valgrind-clean), and runs the whole four-phase pipeline in one call. See "Implementation" below for what shipped. Still ahead: `Statement`/`Value` accessors (nothing to query a loaded document's contents *with* yet), the vtable convenience layer, and everything else in "Open questions" below.

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
   - **Vtables** — a handful of named, *single-level* (not nested) `extern const` structs of function pointers, one per logical group (`Anvil`, `Document`, `Value`, ...), mirroring the pattern the codebase's own internal code already uses (`Anvl`, `Module`). Deliberately not deep struct-of-structs nesting — that's fine for C/C++/Rust/.NET but genuinely painful to marshal from Java/Python, and one level keeps the convenience-layer languages happy without making anything worse for anyone else. Even the vtable *getter* (e.g. `anvil_get_vtable()`) is itself a flat export — obtaining the vtable costs nothing extra beyond one ordinary ABI call.
   - **Flat exports** — every vtable entry has a mechanically-named, independently-documented, individually-callable `extern "C"` mirror: `Group.method` → `group_method`, e.g. `Anvil.load` → `anvil_load`. This is the guaranteed-universal baseline every language can always fall back to via ordinary, mature FFI (JNI, ctypes, N-API, `extern "C"` linking, P/Invoke) — not "vtable plumbing" demoted to an implementation detail. A Java binding, for instance, never needs to touch the vtable at all (JNI has no clean story for holding/invoking a raw C function pointer pulled out of a struct without extra machinery like JNA or the Java 21+ Foreign Function & Memory API) — it just binds the flat exports directly, at zero extra cost versus a world where the vtable didn't exist. That's what makes leading with the vtable in documentation/examples safe: it's pure upside for the languages that like it, and a genuine zero-cost opt-out for the ones that don't.
   - **Naming**: public ABI uses the full `anvil_` prefix, deliberately distinct from the internal codebase's existing abbreviated `anvl_` convention — `anvil_*` is the external, marketed surface; `anvl_*` stays internal-only. Never mix the two in a public header.
   - **Header split (implemented)**: three headers, not two — the vtable getter needs the vtable struct *type* to declare its own return type, so it can't sit in a header that knows nothing about vtables. `include/anvil_types.h` (opaque handles + the public `anvil_err_code` enum, includes nothing internal) → `include/anvil_flat.h` (the flat exports, includes only `anvil_types.h`) → `include/anvil_vtable.h` (vtable struct types + getter, not yet written — will include only `anvil_types.h`, deliberately *not* `anvil_flat.h`, so including it alone never exposes individual flat names, and vice versa — a file is forced to pick one style, not drift into mixing them).
   - **Verification**: since the mirror is mechanical, it should be mechanically checked (even a simple script) that every vtable entry has a correctly-named flat counterpart, catching drift as the surface grows — not left to manual discipline alone.

## Implementation

**`Anvil` group — landed.** `anvil_load(filepath) -> anvil_document` runs the full pipeline in one call (`mod_ctx_initialize` → `doc_load_source` → `mod_ctx_register_doc` → `doc_scan_header` → `mod_load_imports` → `mod_ctx_create_arena` → `doc_parse_body` for every document in the import graph → `mod_resolve_context`), stopping at the first failing phase. Rather than translating whatever internal `anvl_err_code` a failing call happened to produce, `anvil_load` just records *which phase* it was in when something failed (`ANVIL_ERR_IO`/`HEADER`/`IMPORT`/`SYNTAX`/`RESOLVE`/`MEMORY`) directly on the handle — simpler than a translation table and immune to internal code churn by construction, not just by convention. A nested import's own header-scan failure is deliberately categorized as `ANVIL_ERR_IMPORT`, not `ANVIL_ERR_HEADER` — `mod_load_imports` scans each child's header as it recurses, and the *root* document's own header was fine, so "something went wrong resolving the import graph" is the more accurate framing from the caller's perspective.

`anvil_load` returns a non-NULL handle for every failure category above (`doc`/`ctx` stay alive and queryable, matching how the internal test helpers already behave after a failed parse) — `NULL` only for the one truly foundational case, the handle itself failing to allocate. One real ownership wrinkle worth remembering: if `doc_load_source` fails, the document was never registered with the context (`mod_ctx_register_doc` needs the source loaded first to compute its identity hash), so `mod_ctx_dispose` won't know to release it — `anvil_load` disposes it directly in that one path and clears the handle's reference, rather than leaking it. Valgrind confirmed this is the only such gap (0 leaks).

`include/anvil.h`'s old `Module`/`Anvl.load`/`.dispose`/`.get_root_path`/`.has_errors`/`.error_clear` — unimplemented stub, referenced only by the already-disabled `test_fixtures.c` — is retired. `Anvl.get_version()` stays (genuinely implemented, and `test_version.c` — which runs after every single test suite in this repo — depends on it); `anvil_get_version()` just delegates to it. This resolves the "replace in place or new file alongside" question below: new files (`anvil_types.h`, `anvil_flat.h`, `src/core/anvil_flat.c`) alongside a trimmed `anvil.h`, not a wholesale replacement — `AnvlMod`/`mod_initialize`/`mod_new`/etc. in `include/internal/module.h` are a *different*, still-active internal concept (an internal module wrapper, unrelated despite the similar name) and were never touched.

New fixtures: `anvil_err_header_dup_shebang.anvl` (duplicate `#!aml`), `anvil_err_resolve_duplicate.anvl` (imports `resolver_dup_import.anvl` and redeclares the same name) — reusing existing fixtures (`f01_bare_literal.anvl`, `body_err_unterminated_block.anvl`, `hdr_import_missing.anvl`) for the other categories.

## Open questions

- The `Statement`/`Value` accessor surface — nothing to query a loaded document's contents with yet, beyond knowing whether/how it failed to load. Natural next slice.
- The vtable convenience layer (`include/anvil_vtable.h`) — not yet written; the flat baseline needed to exist and be proven first.
- What the API deliberately does *not* expose (question 2 of the founding framing) — not yet worked through in concrete terms beyond "no raw structs" (decision 2 above already answers part of this).
- Should we maintain popular ABIs first - C++, .Net, Python, and Node? Building a community to take up the call to add new paradigm support?

## Related notes

- `document-header-scan.md`, `document-body-parse.md`, `resolution-phase.md` — the four implemented pipeline phases this API needs to expose access to.
- `deferred-work.md` — for anything raised here that ends up deferred rather than decided now.
