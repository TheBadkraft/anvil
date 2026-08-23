# Resolution Phase (phase 4)

A new iteration, not yet started. This note collects resolver design reasoning that had accumulated inside `document-body-parse.md` while body-parse was being designed — moved out because Resolution is its own iteration, not part of body-parse. `document-body-parse.md` keeps only short boundary statements ("this is the resolver's concern") with pointers back here.

## Context

Resolution is the fourth and final phase of the document pipeline:

```
1. Header scan     — doc_scan_header
2. Import loading  — mod_load_imports
3. Body parse      — doc_parse_body
4. Resolution       — this note
```

It runs once every document in the import graph has been body-parsed — not per-document, and not in any particular order (see "Why document processing order doesn't matter" below).

## Design sketch: a global identifier map, not order-dependent traversal

`derived : base { ... };` resolves by looking `base` up in a map that's already complete by the time any resolution happens, because Resolution only starts once every document in the import graph has been body-parsed. As long as that phase boundary holds — all bodies parsed before any resolving begins — order among documents doesn't matter; the resolver only fails when a lookup misses, never because of processing order.

This is a design *sketch*, not a finished mechanism — the map's actual construction, key scheme (bare name? qualified by document/namespace?), and lookup/merge algorithm are all still undesigned. Static value reference resolution (below) presumably reuses the lazy-merge-cache pattern `anvl_resolver_build_state` already used for inheritance in the legacy (`anvil.bak`) implementation, but that reuse hasn't been designed either — it's a lead, not a plan.

## Why document processing order doesn't matter

Body-parse only reads its own document's source (see `document-body-parse.md` § *Responsibilities of the body parse*). Under the global-identifier-map design above, this makes `aml-import-namespace-rules.md` § 7's "parse bodies in reverse dependency order" requirement unnecessary under this design, not just unproven — the map is complete before any resolving starts, so no document needs another document's body to already be parsed.

`notes/document-header-scan.md` § *Deferred to Resolution phase* separately corrects `aml-import-namespace-rules.md` § *Import graph order*'s claim that reversing DFS discovery (pre-order) gives a valid bottom-up order — that reasoning is still wrong under diamond imports (this project explicitly supports them, `HDR13`) regardless of whether anything ends up needing the order fixed.

## Open questions

1. **"Cannot be inherited but can be derived"**: whether something is a legal target to inherit/derive from is entirely the resolver's concern — `doc_parse_body` just captures `base` syntactically and never judges it. The distinction itself is still not understood — an earlier working theory for it turned out to depend on a since-corrected assumption about the grammar. Needs to be asked plainly when resolver design starts, not guessed at again.
2. **Static value reference mechanism**: `derived_var := base_var;` (`ANVL_VALUE_IDENTIFIER`) resolves once at Resolution time via the global identifier map above. The resolver mechanism itself — construction, lookup, error reporting on a miss — is not yet designed.
3. **Resolver-vs-body-parse boundary, for reference**: `doc_parse_body` captures `base` as a slice and nothing more; it doesn't care what it points to or whether inheriting from it is allowed. That boundary is settled (see `document-body-parse.md` § *Resolved questions*) — what's open here is everything on the resolver's side of it.

## Related notes

- `document-body-parse.md` — the phase this one follows; defines the statement/value tree Resolution will consume.
- `document-header-scan.md` § *Deferred to Resolution phase* and § *Import-graph processing order — resolved as unnecessary* — the import-graph-order side of this same "order doesn't matter" conclusion.
- `deferred-work.md` § *Deferred to Resolution phase* — index entry pointing here.
