# Public API — Anvil Native

## Status

**Design sketch, just started.** All four document-pipeline phases (header scan, import loading, body parse, resolution) are implemented and GREEN — see `document-header-scan.md`, `document-body-parse.md`, `resolution-phase.md`. But `include/anvil.h`'s declared public interface (`Anvl.load`, `Module.parse`, `Anvl.has_errors`, `Anvl.dispose`, etc.) is almost entirely unimplemented stub — `src/core/anvil.c` only implements `get_version`. There is currently no real entry point for a caller outside the test suite to run a document through the pipeline. This note is where that gets designed.

## Founding framing (repo owner, verbatim/near-verbatim)

This is **Anvil Native** — the one-truth implementation. Once in production, no other one-off language-specific implementation will be supported; all other languages come to this native library. The project builds primitives here, not policy — the job is to give users the tools to use this library, not to hand-hold them through using it well. Callers are free to write APIs native to their own paradigms on top of this, and they're free to get that wrong; Anvil Native doesn't prevent that, it isn't its job to.

Three questions frame the sketch:

1. **What does the API expose, and why?**
2. **What does the API deliberately *not* expose, and why?**
3. **Who is the audience?** — .NET, Java, Node, Python, Rust, C++, or some subset/none of these, and what does that imply about the shape of the surface (a stable C ABI other languages bind to, presumably, given "primitives not policy" — but that's to be confirmed, not assumed).

## Open questions

Nothing decided yet — this section exists to be filled in as the sketch develops.

## Related notes

- `document-header-scan.md`, `document-body-parse.md`, `resolution-phase.md` — the four implemented pipeline phases this API needs to expose access to.
- `deferred-work.md` — for anything raised here that ends up deferred rather than decided now.
