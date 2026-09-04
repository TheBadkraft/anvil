# Public API — Anvil Native

## Status

**Design sketch, just started.** All four document-pipeline phases (header scan, import loading, body parse, resolution) are implemented and GREEN — see `document-header-scan.md`, `document-body-parse.md`, `resolution-phase.md`. But `include/anvil.h`'s declared public interface (`Anvl.load`, `Module.parse`, `Anvl.has_errors`, `Anvl.dispose`, etc.) is almost entirely unimplemented stub — `src/core/anvil.c` only implements `get_version`. There is currently no real entry point for a caller outside the test suite to run a document through the pipeline. This note is where that gets designed.

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
   - **Header split**: `include/anvil_native.h` (exact name TBD) holds the flat exports — the actual stable ABI contract — plus the vtable struct types and getter(s), since the getter is itself just another flat export.
   - **Verification**: since the mirror is mechanical, it should be mechanically checked (even a simple script) that every vtable entry has a correctly-named flat counterpart, catching drift as the surface grows — not left to manual discipline alone.

## Open questions

- The actual, enumerated vtable/flat-function surface itself — what `Anvil`, `Document`, `Value` (and whatever else) concretely contain. Not yet drafted.
- What the API deliberately does *not* expose (question 2 of the founding framing) — not yet worked through in concrete terms beyond "no raw structs" (decision 2 above already answers part of this).
- Exact public header filename(s) and how they relate to the existing (currently-stub) `include/anvil.h`/`src/core/anvil.c` — replace in place, or a new file alongside?

## Related notes

- `document-header-scan.md`, `document-body-parse.md`, `resolution-phase.md` — the four implemented pipeline phases this API needs to expose access to.
- `deferred-work.md` — for anything raised here that ends up deferred rather than decided now.
