# Document Body Parse

Implementation reference for the body-parse step that follows header scanning and import loading in AML.

See `notes/deferred-work.md` for anything raised here that's deferred to a later phase or still an open decision.

## Context

AML parsing proceeds in three phases:

1. **Header scan** — discover shebang, imports, and module attributes.
2. **Import loading** — expand the import graph recursively.
3. **Body parse** — parse statements and build a document-level statement/value tree.
4. **Resolution** — resolve inheritance, references, and imports after all bodies are parsed.

This document focuses on phase 3.

## Responsibilities of the body parse

The body parser walks the source from where `doc_scan_header` left off (`doc->source->pos` at the first body character) and produces a list of statements for the document.

The parser does **not**:

- Resolve symbol references or inheritance targets.
- Evaluate expressions.
- Allocate output/canonical values.
- Validate that referenced identifiers exist.
- Know anything about compilation. `.anvlo` generation is a separate, post-parse concern owned by a future `anvilc` CLI tool that consumes the parsed (and later resolved) tree; the parser stays fully agnostic to it.

The parser **does**:

- Recognize statement kinds: assignment, object block, `vars`, `using`.
- Capture source spans for every identifier, base, value, and statement.
- Build a recursive value tree for the right-hand side of assignments.
- Attach statement-level attributes.
- Enforce dialect-legal grammar inline, failing fast via `Source.set_error` the moment a construct isn't legal for the source's dialect — the same pattern the header scanner already uses for `ANVL_ERR_IMPORT_AMP_FORBIDDEN` and AMP attribute rejection. See **Dialect scope** below.
- Report malformed syntax through `Source.set_error`.

## Dialect scope (AMP vs AML)

`doc_parse_body` is one function for both dialects; it reads `Source.dialect(doc->source)` and rejects constructs that aren't legal for that dialect as it encounters them, rather than deferring validation to a later pass.

AMP is a flat, minimal message-packet format, not a module — its header can never carry imports or attributes (`header_scan_imports`/`header_scan_attributes` already enforce this), so its body grammar is correspondingly narrow:

- **AMP-legal**: `ANVL_STMT_ASSIGN` only (`ident := value;`). Values: all scalars (`null`, `bool`, `integer`, `float`, `string`, `blob`) plus `array` of scalars.
- **AMP-illegal**: `ANVL_STMT_OBJECT_BLOCK` entirely (no namespace containers, no immutable objects, no inheritance — see **Statement structure**), `vars`, `using`, statement-level `@[...]` attributes, `object`-typed values.
- **Open**: whether `tuple` is AMP-legal is still undecided — resolve before writing the AMP tuple test case.
- **AML-only**: everything else — the full statement/value grammar below.

The parser has no notion of "compilable" vs "not." AMP simply never reaches a compilation step because nothing downstream ever invokes `anvilc` on an `.amp` source — that's a caller-level decision, not something encoded in `doc_parse_body`.

## Public function boundary

```c
anvl_result doc_parse_body(module_document doc, anvl_err_code *out_err_code);
```

- Input: a document whose header has been scanned and whose imports have been loaded.
- Output: `doc->body` populated with a list of `anvl_doc_statement_t` pointers.
- Returns `ANVL_RES_OK` on success, `ANVL_RES_ERR` on failure.

This function is exposed for independent testing and is called internally after `mod_load_imports` succeeds.

## Slice conventions

The body parser uses the same `anvl_slice` type as the header scanner (`include/internal/module.h`):

```c
typedef struct anvl_src_slice_t {
   char *data;   // base pointer to the source allocation
   char *start;  // first byte of the slice
   char *end;    // one-past-the-end byte of the slice
} anvl_slice;
typedef anvl_slice *slice;
```

A slice is self-referential and empty when `start == end`; text length is `end - start`. Both are derived on demand via `Source.slice_is_empty`/`Source.slice_length` rather than stored. Slices are no-copy references into `doc->source` and become invalid if the source is disposed.

## Statement structure

There are **two** statement forms, and — corrected again — `base` is valid on **both**, not `OBJECT_BLOCK`-only as I previously had it. A derived object is not required to be written as a bare block; it can equally be a `:=` assignment whose value is an object:

```
ident [: base] [@[...]] := value;   // inheritance only valid when `value` is an object — not array, tuple, or scalar
ident [: base] [@[...]] { statements... };
```

Both forms accept an optional `base` and optional `@[...]` attributes in the same position (between the identifier and the `:=`/`{`). They differ only in what follows: an explicit `:= value` (any value type, but `base` is only *semantically* legal when that value is an object) versus a bare `{ statements }` block (always object-shaped, so `base` is unconditionally legal there). `test/fixtures/f07_object.anvl` and `docs/changelog.md`'s v0.5.6 `ANVL_ANON_OBJECT` entry (`Ident : Base { ... }` / `Ident : Base @[...] { ... }`) both corroborate the block form; your fresh examples confirm the assignment form also takes `base`.

```anvl
// namespace / scoping container — no base, no attribs
label {
   // statements, objects, etc
};

// immutable object with attributes — no base
config @[attrib] {
   addr := 0.0.0.127;
   port := 8080;
};

// composited: assigned base, then derived via the block form
user := badkraft;
base := {
   name := base;
};
derived : base {
   name := user;
   created := null;
};
```

```c
typedef enum {
   ANVL_STMT_ASSIGN,       // ident [: base] [@[...]] := value;
   ANVL_STMT_OBJECT_BLOCK, // ident [: base] [@[...]] { statements };
   ANVL_STMT_VARS,         // vars { ... }
   ANVL_STMT_USING,        // using "path";
} anvl_stmt_kind;

typedef struct anvl_doc_statement_t {
   anvl_stmt_kind kind;
   anvl_slice span;           // full source span of the statement
   anvl_slice name;           // declared identifier; always present except VARS/USING
   anvl_slice base;           // valid on ASSIGN and OBJECT_BLOCK; empty when absent
   anvl_doc_value value;      // ASSIGN only; NULL otherwise
   list body;                 // OBJECT_BLOCK only; nested list of anvl_doc_statement pointers
   list attributes;           // anvl_doc_attribute pointers, or NULL
} anvl_doc_statement_t;
typedef anvl_doc_statement_t *anvl_doc_statement;
```

Notes:

- `name` is empty only for `VARS` and `USING`; both `ASSIGN` and `OBJECT_BLOCK` always have one — a genuinely nameless statement never existed as real syntax.
- `base` may appear on either `ASSIGN` or `OBJECT_BLOCK`. `attributes` sits between `[: base]` and `{`/`:=` in both forms.
- Exactly one of `value` (ASSIGN) or `body` (OBJECT_BLOCK) is populated at a time; both are `NULL`/empty for `USING`.

**Resolved — deterministic, not a post-hoc check**: seeing `: base` tells the parser this statement is object-shaped *before* it reaches `:=`/`{`. So when `base` is set and the next token is `:=`, the parser doesn't generically parse "any value" and check its type afterward — it goes straight into object-body parsing and requires `{` immediately: `: base :=` with anything other than `{` next is a parse error at that point, the same way an unexpected token anywhere else is. This makes `derived : base := 42;` fail as an ordinary syntax error (expected `{`, got `42`), not a semantic type-mismatch check bolted on afterward. `doc_parse_body` is still not responsible for judging *whether* `base` is a legal thing to inherit from (i.e. whether it's "inheritable" at all) — that's entirely the post-parse resolver's job, once it's resolving static value references. The parser only cares about the shape that `: base` implies, never about what `base` resolves to.

### Design rationale, vs. legacy `struct anvl_statement`

`anvil.bak/include/types.h` (the pre-rewrite legacy source) shaped a statement around a fixed `usize meta[9]` index buffer (type, ident pos/len, a base-present flag, an attribute count, a value-present flag, several reserved slots) plus separate heap-allocated `base_meta`/`attr_meta`/`value_meta` satellite structs, filled in lazily and referenced by index. `anvl_doc_statement_t` doesn't carry that indirection forward, deliberately:

- **No `meta[]` index buffer.** Legacy's flat array made sense as a compact, pointer-free block. This project's model is already slice-based and no-copy end to end (the header scanner, the whole `anvl_slice` design), so there's no equivalent pressure here to pack scalar fields into an untyped index array — plain named fields on the struct are just as compact and far more legible.
- **`base` lives directly on the statement**, not behind a nullable `base_meta` pointer with a separate presence flag (legacy's `meta[4]`). Because `base` is valid on *both* `ASSIGN` and `OBJECT_BLOCK` in this grammar (not just an "inheritance statement" subtype), it's common enough not to warrant its own satellite allocation — and an empty slice (`start == end`) already means "absent," so no separate presence flag is needed at all.
- **`value` and `body` are mutually exclusive** (`ASSIGN` uses `value`; `OBJECT_BLOCK` uses `body`; `VARS`/`USING` use neither) — a deliberate trade of a few unused pointer bytes per statement against not needing a tagged union just to distinguish "value tree" from "nested statement list." Could union them later if the waste ever matters; not worth the complexity now.
- **No `value_meta`-equivalent satellite struct.** `anvl_doc_value_t` (what `value` points to) *is* the value struct directly, recursive on itself — `ARRAY`/`TUPLE` hold a `list` of `anvl_doc_value`, `OBJECT` holds a `list` of `anvl_doc_statement` (the same shape `OBJECT_BLOCK`'s own `body` uses). No `PAIR` variant, no `VARREF` — both were cut once this session confirmed AML/AMP have no interpolation and no key/value-pair object encoding (see **Value structure** below).
- **`attributes` reuses the header's own `anvl_doc_attribute` list type** rather than introducing a separate `attr_meta` array type.

Net effect: legacy optimized for a fixed-size flat metadata block with lazily-filled satellite pointers; this shape leans into the slice-based, no-copy model that's already the backbone of the header scanner, so there's less need for index-based indirection. If a concrete reason shows up later to want something closer to `meta[]` — memory footprint per statement, cache locality walking a large `body` list — that's worth reopening, but nothing about the current design assumes it won't happen; it just isn't motivated yet.

## Value structure

Values form a recursive tree. Every value node has a type, a source span, and type-specific children. Current shape (`include/internal/module.h`; `anvl_doc_value`/`anvl_doc_statement` naming from earlier drafts of this note has since been renamed to `anvl_value`/`anvl_statement` throughout the actual code — this note now matches):

```c
typedef enum {
   ANVL_VALUE_NONE,
   ANVL_VALUE_NULL,
   ANVL_VALUE_BOOL,
   ANVL_VALUE_NUMERIC,     // integer, float, hex, exponential/scientific — one type, see below
   ANVL_VALUE_STRING,
   ANVL_VALUE_BLOB,
   ANVL_VALUE_ARRAY,
   ANVL_VALUE_TUPLE,
   ANVL_VALUE_OBJECT,      // nested statement list, not key/value pairs — see below
   ANVL_VALUE_IDENTIFIER,  // bare symbol: static reference to another statement's value
} anvl_value_type;

typedef struct anvl_value_t {
   anvl_value_type type;
   anvl_slice text;   // full source span of the value
   union {
      struct {
         anvl_slice tag;  // blob tag (e.g. @date, @sel); empty for non-blobs or untagged blobs
      } blob;
      struct {
         list items;      // array/tuple: list of anvl_value pointers
      } collection;
      struct {
         list statements; // object: list of anvl_statement pointers — same shape as OBJECT_BLOCK's body
      } object;
   };
} anvl_value_t;
typedef anvl_value_t *anvl_value;
```

Notes:

- Scalar values (null, bool, numeric, string) are fully described by `type` and `text`.
- `BLOB` is a standard scalar, not a special case — it's the escape hatch for embedded payloads of any kind (JSON, binary, GLSL, markdown, whatever). Syntax is backtick-delimited content with an optional `@tag` prefix: `` @date`2026-07-07` ``, `` @sel`#input-date` ``, or untagged `` `raw content` `` (empty `tag` slice). Legal in AMP as well as AML.
- **`tag` moved from a top-level field into the union, under `blob`.** Resolved this iteration: a top-level `tag` meant every non-blob value (the large majority) carried an unused 24-byte `anvl_slice` (`data`/`start`/`end`, 3 pointers) alongside an already-present union. Moving it into the union means it shares space with `collection.items`/`object.statements` instead of sitting beside them — a value is exactly one variant at a time, so this is a legitimate union use, not a hack, and it shrinks `sizeof(anvl_value_t)` from 64 to 56 bytes (confirmed via a direct `sizeof` check: the union's own size grows from 8 bytes, `list`, to 24 bytes, `anvl_slice`, but that growth *replaces* the dedicated always-present field rather than adding to it).
- Array and tuple children live in `collection.items`.
- **There is no `PAIR`/key-value object representation.** `config := { host := localhost; port := 8080; };` uses ordinary `:=` statements inside the braces — confirmed by `f07_object.anvl` — not `key: value` JSON-style pairs as an earlier draft assumed. So a `:=`-assigned object value and an `ANVL_STMT_OBJECT_BLOCK`'s body are the exact same shape: a nested `list` of `anvl_statement` pointers. `object.statements` here is that same nested list, reached via the value tree instead of directly off a statement.
- `IDENTIFIER` values keep their raw text for **static** resolution only — a bare identifier referencing another statement's value (e.g. `derived_var := base_var;`), resolved at Resolution time. AML and AMP have no interpolation and no dynamic var-refs (`${...}`/`$"..."`); all of that is reserved entirely for AnvilScript. There is no `VARREF` value kind in this document's grammar.

### Numeric literal grammar

`test/fixtures/f03_numerics.anvl` is the concrete reference for how permissive numeric literals need to be:

```anvl
age := 42;
negint := -1285;
pi := 3.14;
decimal := -3.402823466;
exp := 1.7976931348623157e+308;
large := 18446744073709551615;
hex := #12D4F;
```

**Resolved — `NUMERIC` is one value type, not split `INTEGER`/`FLOAT`.** Superseded this iteration: earlier drafts of this note had separate `ANVL_VALUE_INTEGER`/`ANVL_VALUE_FLOAT` types classified by shape (plain digits vs. `.`/exponent). Collapsed into a single `ANVL_VALUE_NUMERIC` covering integer, float, hex (`#12D4F`), and scientific/exponential notation alike — the body parser doesn't canonicalize or interpret numeric values anyway (see **Responsibilities**), so a syntactic split that nothing downstream uses to make a decision was overhead without payoff. `parse_numeric_literal` (`src/core/parser.c`) currently only handles plain digit sequences; hex/exponential/scientific forms are real, scoped-but-not-yet-implemented work for that same function to grow into, not a design question.

- Optional leading `-` sign.
- Integer literals must round-trip values beyond signed 64-bit range — `18446744073709551615` is `UINT64_MAX`. Since the body parser never canonicalizes values, this only requires the raw span to be captured intact; no overflow check happens at parse time. Classification is purely syntactic (and now, with `NUMERIC` unified, there's no classification decision to make at all — just "does this look like a number").
- `hex := #12D4F;` resolves the previously-open question here: it's `NUMERIC`, same as everything else in this section (Open questions #3, resolved: "Anvil weak type is numeric"). The `#` prefix's other legacy-parser meanings (`#RRGGBB`, bare `#identifier`) are a separate, unrelated grammar concern from this specific hex-digit-sequence case.

## Storage details

`statements` is a Sigma list with `sizeof(void *)` stride, storing pointers to heap-allocated `anvl_doc_statement_t` entries. Each statement owns:

- Its own scalar fields and slices.
- Its `value` tree (ASSIGN) or `body` list (OBJECT_BLOCK), which recursively own their child statements/values — an `ANVL_VALUE_OBJECT`'s `object.statements` list owns its statements exactly like an `OBJECT_BLOCK`'s `body` does.
- Its `attributes` list (if any).

Statement disposal iterates the body list, frees each statement, recursively frees the attached value tree or nested statement body, and disposes the list.

## Arena-backed allocation

**Infrastructure complete and proven end-to-end; only the production sequencing call site remains.** Everything below this point — arena create/dispose, `Source.get_arena`, the sizing heuristic (now real code, not just a formula), `Source.new_node` and the `statements`/`values` node indexes (see "Arena node iteration" below) — is implemented and covered by tests (`CR17`–`CR22` in `test_module.c`, `SRC23`–`SRC30` in `test_source.c`, `HDR19`–`HDR20` in `test_header.c`), all Valgrind-clean. `CR21`/`CR22` specifically chain every real piece together by hand on the diamond-import fixture (header scan → import loading → size hint → arena creation → `Source.new_node` from both the root and a child document) and prove the whole thing holds — including that the arena is genuinely reachable and shared from any document in the import graph, the original design goal. Finding and fixing that proof also surfaced and fixed a real, previously-latent bug in `doc_dispose`/the source registry — see `notes/document-header-scan.md` § *Duplicate document deduplication*.

**What's still missing**: no production code actually calls `mod_load_imports` → `mod_ctx_arena_size_hint` → `mod_ctx_create_arena` in sequence yet — today that chain only exists inside tests. `doc_parse_body`/`parser.c` still need to call `Source.new_node` to build real nodes; until then, `doc_parse_body` remains a stub. Below is the original design record from when this started; historical reasoning and rejected alternatives are kept as-is even where later sections in this same block mark them resolved.

Statements and values used to get allocated one at a time via `Allocator.alloc`/`Allocator.dispose`, matching the header scanner's `anvl_doc_import`/`anvl_doc_attribute` pattern. The plan was to drop `doc_parse_body`'s entire output — every `anvl_statement_t` and `anvl_value_t` it builds — into a single arena instead, freed in one bulk operation instead of a per-node tree-walk. Motivation: per-node `malloc`/`free` is typically the dominant cost in a tree-building parser, well ahead of the scanning/tokenizing itself, and bulk disposal is O(1) instead of O(n).

**Resolved — the mechanism already exists; nothing new needs to be built.** `src/sigma/memory.c` already implements a complete, working chained-block bump allocator, wired into the `Allocator` vtable and unused by anything so far — ported forward from `anvil.bak`'s `bump_allocator`/`Allocator.create_bump` (`anvil.bak/include/context.h:113` had a `bump_allocator arena` field on the legacy parse context; `anvil.bak/include/sigma/allocator.h` declared the same `create_bump`/`release` shape). The doc comment on `sc_bump_ctrl_s` (`include/sigma/memory.h`) literally anticipates `ctx->arena->alloc(ctx->arena, size)` as its access pattern.

- `Allocator.create_bump(initial_size)` → a `bump_allocator` handle backed by one block (`initial_size`, or a 64 KB default). **Grows automatically** — chains a new block when the current one fills, no manual growth logic needed. This is why the sizing heuristic above doesn't need to be precise: an undersized estimate just costs one more chained block, not a failure, consistent with "treat the estimate as sized to avoid reallocation in the common case, not a ceiling."
- `arena->alloc(arena, size)` → bump-allocates and zeroes `size` bytes from the current block.
- `Allocator.release((sc_ctrl_base_s *)arena)` → frees every chained block, then the controller itself. One call, everything gone — the bulk disposal the module-wide scope (below) was designed around.

**Concrete integration points** — all done:
- `bump_allocator arena;` on `struct anvl_mod_ctx_t` (`include/internal/module.h`).
- `mod_ctx_create_arena(ctx, size, out_err_code)` (`src/core/module.c`) — takes the already-computed size, tested by `CR17`.
- `mod_ctx_dispose` calls `mod_ctx_dispose_arena` → `Allocator.release((sc_ctrl_base_s *)ctx->arena)`, replacing the per-document `doc->body` tree-walk disposal — tested by `CR18`.

Still open: nothing calls `mod_ctx_create_arena` from real (non-test) code yet — see the status note at the top of this section.

**How the parser reaches the arena — a `Source` accessor, matching the `set_error`/`has_errors` pattern.** `anvl_parse`/`doc_parse_body` are only ever handed an `anvl_source`, never a `module_document`/`module_context` directly — deliberately; the parser doesn't know those types exist, the same way it doesn't know TestBit exists (see the instrumentation-hook design earlier this session). `Source.set_error`/`Source.has_errors` already solve this exact "I only have a source, but I need something off its owning document/context" problem by looking the owner up through the source-hash registry (`Registry.find(src->hash)` → `module_document` → `doc->context`). `Source.get_arena(src, out_err_code)` mirrors that same pattern (`src/core/source.c`) — tested by `SRC23`–`SRC26` (success, unregistered source, `NULL` source, arena not yet created).

Pure lookup, not a lazy-create — the arena still only gets created once, via `mod_ctx_create_arena`, at the sequencing point above where the summed size is actually known. Calling this before that point (or on an unregistered source) returns `NULL` with the corresponding error code; callers need to handle it the same way `has_errors` already does.

**Resolved — naming**: `get_arena`, not `get_page` — matches the codebase's existing vocabulary (`sc_arena_block_s`, the `ctx->arena->alloc(...)` doc comment in `sigma/memory.h`, "bump arena" in `memory.c`'s file header).

**Initial sizing heuristic — implemented, working default until real data replaces it**: `max(sum(Source.length()) × ANVL_ARENA_SIZE_MULTIPLIER, ANVL_ARENA_MIN_SIZE)`, where the sum runs across every document in `ctx->docs`. Both constants live in `include/constants.h` as named Anvil settings (`ANVL_ARENA_SIZE_MULTIPLIER = 2`, `ANVL_ARENA_MIN_SIZE = 64 KB`) — deliberately placed there rather than in `internal/constants.h`, matching where `ANVL_VERSION_*`/`ANVL_SHEBANG_*` already live as public Anvil settings. The formula itself is `mod_ctx_arena_size_hint(usize summed_source_length)` (`src/core/module.c`), a pure function with no failure mode, tested by `CR20` (floor wins, multiplier wins, exact boundary, zero-sum degenerate case). Reasoning:

- Every `anvl_slice` is fixed-size (3 pointers, 24 bytes) regardless of how much source text it spans, and `anvl_statement_t`/`anvl_value_t` are themselves fixed-size (~104 and ~64 bytes respectively) — so arena usage scales with *node count*, not source bytes directly, and the two only loosely track each other.
  - A minimal statement (`name := 42;`, 11 source bytes) produces one statement node plus one value node — roughly 170 bytes of arena usage, over 15x its own source length. Short, terse, densely-packed statements push the ratio well above 1x.
  - A document dominated by long string/blob content has the opposite ratio — one small `anvl_value_t` can represent a multi-KB span, pushing the ratio toward or below 1x.
- A flat **`ANVL_ARENA_SIZE_MULTIPLIER` (2x)** on the raw byte sum is a defensible working guess for that overhead, chosen for being cheap (one multiplication, no extra scanning pass — this is why the pre-scan idea above was set aside) rather than for being precise. No empirical basis yet for whether 2x is the right constant, or even the right *shape* of correction — the instrumentation hook already built (`anvl_parser_set_hook`, `Time.elapsed`, `report_throughput`) is positioned to measure actual arena usage against source size across a range of real fixtures once real parsing exists, and the multiplier should be replaced by whatever that data says, not kept as a permanent guess.
- **Floor at `ANVL_ARENA_MIN_SIZE` (64 KB) explicitly**, its own named Anvil constant rather than a re-export of `sigma/memory.c`'s private `DEFAULT_BLOCK_SZ` (that macro isn't exposed via `sigma/memory.h`, so it isn't reachable from `constants.h` anyway) — the two happen to match today but are independent settings. `Allocator.create_bump(initial)` uses `initial` as the block size *verbatim* when non-zero — it does not floor to its own default. A small document's 2x'd estimate could still land under 64 KB and end up worse than the library's own sensible default, forcing avoidable early growth.
- None of this affects correctness either way — the arena chains a new block automatically when the current one fills (see `bump_alloc` in `src/sigma/memory.c`), so a wrong guess only costs one extra allocation, never a failure.
- **Resolved — arena scope is per-module, shared across every document the module owns.** Not per-`module_document` — one arena for the whole import graph, so every document's statements and values end up contiguous in the same block. Corrected from an earlier draft of this note that read "module-based" as per-document; the actual intent is the reverse — one arena, all documents, deliberately for locality when navigating the combined tree.

  **Ownership hierarchy, clarified**: `AnvlMod` (the module) is the owning container; `module_context` is "an inner organizer" beneath it (owns the `docs`/`errors`/`parser` bookkeeping — the practical aggregate of the whole import graph); `module_document` belongs to both, one per source file. A document is named `module_document` because it belongs to a module, not because it *is* one — the ambiguity that prompted this correction. Worth revisiting the naming at some point; not urgent.

  **Sizing implication**: capacity should be the *sum* of `Source.length()` across every document in `ctx->docs`, not one document's length alone. This is computable up front — by the time `doc_parse_body` runs for any document, the whole import graph is already loaded (`mod_load_imports` has fully expanded it), so the complete document set — and the complete sum — is known before body-parsing starts for the first one.

  **Resolved — coupled lifetime is intentional, not a limitation.** A document's parsed body was never independently useful once something it references (an import, a `base`) is gone — the coupling just makes an already-true dependency explicit rather than imposing a new one. `module_context` owns the arena (not the individual document): `module_document` already carries a `context` back-pointer (`doc->context`), and `mod_ctx_dispose` already tears down `ctx->docs` as one atomic unit — adding the arena there means teardown gets *simpler* than today, not more complex. Today `doc_dispose` walks `doc->body` disposing each statement individually; once the arena lands, `mod_ctx_dispose` just frees the one arena and every document's parsed output goes with it in one shot, which is the actual point of going arena-backed in the first place.

  **Sequencing**: the arena can't be sized (and shouldn't be allocated) until the full document set is known — which is exactly true right after `mod_load_imports` finishes expanding the graph. "Size and allocate `ctx`'s arena" is one new step inserted between import-loading and body-parsing, not something each document coordinates individually. All pieces below are implemented and tested individually (and chained together by hand in `CR21`/`CR22`) — what's missing is this exact sequence appearing in real (non-test) code:
  ```
  mod_load_imports(ctx, root, &size_hint, &err)      // full doc graph now known
  capacity = mod_ctx_arena_size_hint(size_hint)      // apply the multiplier/floor
  mod_ctx_create_arena(ctx, capacity, &err)
  for each doc in ctx->docs:
     doc_parse_body(doc, &err)                       // draws from ctx->arena via Source.new_node
  ```

  **Open**: does "one shot" disposal require the `list` backing storage itself (Sigma `list`'s dynamic array behind `doc->body`, `value->collection.items`, `value->object.statements`, `statement->attributes`) to also be carved from the arena, or do those stay on the regular heap via `List.new`? If lists stay on the heap, bulk-freeing the arena reclaims every *node*, but the (much smaller number of) *list* structures still need individually disposing — fewer objects to walk than today, but not literally zero. Worth deciding when the arena's actual allocation API takes shape; not blocking the design above.

  Header-scan data (`doc->header`'s `anvl_doc_import`/`anvl_doc_attribute` lists) stays **outside** the arena regardless of scope, on its existing `Allocator.alloc`/`List.new` path — the arena is specifically for `doc_parse_body`'s output (statements and values), not header metadata.

**Considered and set aside: a pre-scan pass counting `;`/`,` for a more precise node estimate than raw byte length.** The idea: count statement terminators (`;`) for statement-node count, and comma-separated elements (`,`, +1 per array/tuple) to also account for array/tuple elements, which a pure semicolon count would otherwise miss (elements are comma-delimited, not statement-terminated — nested object-body statements *are* already covered by the `;` count, since object bodies are just nested statement lists). Correct in principle, but weighed against two costs and set aside for now rather than adopted:

- **A naive raw-byte count is unreliable.** `;`/`,` occurring inside a quoted string or — especially — inside `BLOB` content (explicitly "the escape hatch for embedded payloads of any kind: JSON, binary, GLSL, markdown, whatever," per **Value structure** above) would be miscounted as real delimiters. Embedded JSON/GLSL/C-like payloads are exactly the kind of content that's dense with semicolons and commas, so this isn't a rare edge case for blob-heavy documents — it could inflate the estimate substantially.
- **A correct pre-scan (skipping string/blob/comment content, matching `header_skip_ws_comments`'s approach) fixes that, but costs a full second pass over the source** — roughly doubling total scan time to shave off some number of arena growth events.
- **Given the arena is already growable (not a hard cap), precision doesn't matter for correctness** — only for minimizing how many growth/reallocation events happen. That's a minor performance-tuning question, not one worth a second full-document scan to answer speculatively. Revisit only if the instrumentation data (once real parsing exists) shows growth-reallocation is an actual measurable cost, not a hypothetical one.

### Arena node iteration — a per-kind index list

**Resolved.** Distinct from the "Open" question above (whether `doc->body`-style *list-backed* fields move into the arena) — this is about walking *everything* the arena holds, independent of any one document's tree, for cases like the Resolution phase needing "every statement" or "every value" without a recursive walk. The arena's own memory isn't the right thing to iterate directly: it's heterogeneous (statement and value nodes interleaved in allocation order, different sizes) and, once it grows past one block, chained across non-contiguous blocks — not a single uniform-stride region.

The answer is a separate index, not a view into the arena's raw bytes: `struct anvl_mod_ctx_t` gets two more `list` fields, `statements` and `values`, holding `anvl_statement`/`anvl_value` pointers — created in `mod_ctx_initialize` alongside `docs`/`errors`, appended to at the same call site that hands out the arena-backed node, disposed (container only, not the pointed-to nodes — those go with the arena in one shot) in `mod_ctx_dispose`.

**Why `list`, not `FArray`** (the newly-ported flex-array from `../sigma.collections/src/`, see below): `list` is already backed by `Collections`' contiguous, growable storage (`list.c`: `lst->coll = collection_new(...)`, doubling-realloc growth on `List.append`) — not a linked list despite the name, despite an earlier pass through this exact question in conversation assuming otherwise. `FArray` as ported has no append/grow operation at all, only fixed-capacity indexed `set`/`get` — a real limitation for an index whose final size isn't known until parsing finishes. `list` already provides the contiguous-storage property that mattered, plus growth `FArray` doesn't have, and matches the idiom already used for `docs`/`errors`/`doc->body`/etc. `FArray`/`array_base` are still legitimate, tested infrastructure now in the tree (`src/sigma/farray.c`, `src/sigma/array_base.c`) — just not the right fit for this particular job.

**`Source.new_node` — the single allocation entry point, tying arena and index together.** One new accessor on `anvl_source_i`, alongside `get_arena`/`set_error`/`has_errors`, same registry-lookup pattern (`Registry.find(src->hash)` → `doc->context`):

```c
typedef enum {
   ANVL_NODE_STATEMENT = 0,
   ANVL_NODE_VALUE,
} anvl_node_kind;

void *Source.new_node(anvl_source src, anvl_node_kind kind, anvl_err_code *out_err_code);
```

Looks up the owning context, requires `ctx->arena` to already exist (`ANVL_ERR_ARENA_NOT_INITIALIZED` otherwise — same failure mode `get_arena` already reports), sizes the allocation by `kind` (`sizeof(anvl_statement_t)` or `sizeof(anvl_value_t)`), calls `ctx->arena->alloc(ctx->arena, size)`, and appends the returned pointer to `ctx->statements` or `ctx->values` before returning it. One call site does both the allocation and the indexing, deliberately — so the two can never drift apart the way `mod_ctx_add_doc`/`mod_ctx_register_doc` did earlier this iteration (two paths into `ctx->docs`, one forgotten). The parser never touches `get_arena` directly for node construction; `get_arena` remains useful in its own right (already covered by SRC23–26) for testing and any future direct-arena consumer that isn't building statement/value nodes specifically.

Two separate lists rather than one combined/tagged index, deliberately: statements and values are already distinctly typed, Resolution processes them differently (inheritance/`base` resolution vs. identifier-reference resolution), and it mirrors the existing `docs`/`errors` split on the same struct rather than introducing a new pattern.

### Node self-reference — `anvl_source source` on statement/value nodes

**Deferred, not going forward for now.** Raised while chasing down a real bug (a raw pointer-cast `*(anvl_source *)node = src` that would have silently corrupted `kind`/`type`), but the bug itself turned out to be a mix-up between "node" and "slice" — the actual, motivating problem was `Source.new_slice` below (a slice's `.data` field set wrong), not anything about the arena node lacking a source reference. Once that was untangled, adding `anvl_source source;` to both node structs stood on its own as a nice-to-have with no current caller, not something the immediate problem needed — seems redundant for now, and worth reconsidering inside `new_node` again later if a real need for it shows up (Resolution, error reporting tied back to a specific node), rather than adding it speculatively.

### `Source.init_slice` — centralizing slice construction

**Implemented, tested (`SRC31`, `SRC32` in `test/unit/test_source.c`), GREEN.** Shipped smaller than the original `Source.new_slice(src, start, end, out_slice)` sketch: `Source.init_slice(anvl_source src, anvl_slice *out_slice)` only fills `out_slice->data` (the buffer base, via `Source.data(src)`) — `start`/`end` are set by the caller afterward, same as before. Still kills the actual bug class (`.data` set wrong or inconsistently at each call site), just with a narrower function than originally sketched. `parse_identifier`/`parse_numeric_literal` (`src/core/parser.c`) both call `Source.init_slice(src, out)` first, then set `.start`/`.end` themselves using `Source.at(src)`. Guards a `NULL out_slice` (added alongside `SRC32`, which exercises it — the initial version didn't and would have crashed the same way the unwired `Source.at` did, below).

### Document body — accumulate then freeze

**Implemented, tested (`SRC33`, `SRC34` in `test/unit/test_source.c`; end-to-end via `AMP00`/`AMP01` in `test_body_amp.c`), GREEN — supersedes the earlier index-range idea below.** `doc->body`'s field type is `farray` (`include/internal/module.h`), not `list`; `doc_parse_body` no longer pre-creates it (`src/core/document.c`), and `doc_dispose` calls `FArray.dispose` on it. The handoff function is `Source.finish_body`.

`parser_ctx` (`src/core/parser.c`, private to that file) carries the accumulator: a `list statements` field, created in `parser_create_ctx` alongside `src`/`start`/`end`, and disposed in `parser_dispose_ctx` *only* if `parse_source` never handed it off (i.e. only reachable via the very first `!ctx || !ctx->src` guard, before the loop even starts — every other exit path hands off). `parse_source`'s loop appends each successfully-parsed statement directly (`List.append(ctx->statements, stmt)`) — `parse_statement` itself never needed to change, since the loop already holds both `ctx` and the freshly-parsed `stmt`. `Source.finish_body(ctx->src, ctx->statements, &parser.err_code)` is called once on every one of `parse_source`'s three exit points (both failure returns inside the loop, and the success path after it), with `ctx->statements` set back to `NULL` immediately after each call so `parser_dispose_ctx` never double-frees an already-transferred list.

An earlier draft of this note proposed making `doc->body` a `(start_index, count)` view into `ctx->statements` (the flat, whole-import-graph index above — note: unrelated `ctx`, that one's `module_context`, this one's `parser_ctx`). Rejected: that only stays correct because nested statement parsing (`OBJECT_BLOCK` bodies) doesn't exist yet — a document's top-level statements happen to land as one contiguous run in the *module-context* index today only because nothing else is being allocated while it parses. The moment nested parsing lands, nested statements land in that same shared, flat index too, interleaved with the top-level ones, and a plain index range can no longer tell top-level apart from nested without an additional marker. The shipped design doesn't have that dependency — the accumulator lives on `parser_ctx`, populated only by `parse_source`'s own top-level loop; nested statements, whenever `OBJECT_BLOCK` parsing exists, will get appended to their *parent statement's* `body` list by different code entirely and never touch this accumulator. Correct by construction, not by relying on ordering in a structure shared with anything else.

- **Frozen shape is `farray`, not `list`.** `list`'s growability is exactly what accumulation needs and exactly what a *finished* body doesn't — once parsing for a document is done, `doc->body` is never appended to again, so the denser, no-further-growth `FArray` (ported earlier this iteration, `src/sigma/farray.c`) is the more honest final type. `identifiers()`/`values()`-style convenience iterators (thin wrappers walking the frozen array, pulling `.name`/`.value` off each entry) are the natural next layer on top once real multi-statement parsing exists to exercise them — not built yet.
- **One handoff call, not one call per statement.** Only a single registry lookup is needed, at the very end, not once per statement — cheaper, and simpler than threading a `Source.append_statement`-per-statement call through the loop (an earlier, discarded sketch of this same idea).
- **Freeze happens on both the success and failure exit paths, not success only** — matching `mod_load_imports`'s `out_body_size_hint` precedent (set regardless of the overall result). Confirmed working exactly as designed: `AMP01` (`age := 42; negint := -1285; large := ...;`) currently fails on its second statement (`parse_numeric_literal` doesn't yet handle a leading `-`, a separate, expected grammar gap — not part of this piece), and `doc->body` still ends up with the first statement correctly captured rather than empty. This is partial-result visibility, not statement-level error recovery/resynchronization (skipping to the next statement boundary and continuing) — a different, unscoped feature nothing here implies.

## Inspecting statement and value data

**Strategy: plain field access, no accessor API.** `anvl_doc_statement`/`anvl_doc_value` are populated once by the parser and then only ever read by consumers (the future resolver, tests, eventual bindings) — there's no internal state machine or invariant-preserving logic behind them, unlike `List`/`Source`/`Time`/`Registry` (which earn their vtable interfaces by wrapping real behavior: dynamic growth, hashing, a monotonic clock, refcounting). `anvl_doc_statement`/`anvl_doc_value` are data records, the same category as `anvl_doc_import`/`anvl_doc_attribute`, and those are already accessed by direct field reference everywhere in this codebase (`imp->resolved`, `attr->key`) — no getter functions. Follow the same convention here: `stmt->kind`, `stmt->value->type`, `Source.slice_is_empty(stmt->base)`, etc., directly.

**The one real invariant, and it's not enforced by the compiler**: both structs use a discriminated union — `anvl_doc_statement.value` vs. `.body` depending on `.kind`, and `anvl_doc_value`'s `.collection`/`.object` depending on `.type`. Reading the wrong union member for the current tag is undefined behavior; C won't stop you. The rule to follow everywhere a statement or value gets inspected:

- Check `kind` before touching `value` or `body` — `value` is only meaningful for `ANVL_STMT_ASSIGN`, `body` only for `ANVL_STMT_OBJECT_BLOCK`. Both are unset for `VARS`/`USING`.
- Check `type` before touching the union — `collection.items` only for `ANVL_VALUE_ARRAY`/`ANVL_VALUE_TUPLE`, `object.statements` only for `ANVL_VALUE_OBJECT`, `blob.tag` only for `ANVL_VALUE_BLOB` (moved into the union from a top-level field — see **Value structure** below for why). Every other type is fully described by `text` alone, no union access needed.

If bindings (C#, Python, etc. — see `bindings/` precedent from the pre-rewrite project) eventually need to inspect statements from outside C, a stable accessor API becomes worth it then, since it can hide field layout and absorb struct changes without breaking an ABI. Not needed for internal C consumers now.

## Document structure additions

`struct anvl_mod_doc_t` gains a `body` field:

```c
struct anvl_mod_doc_t {
   anvl_source source;               // source for the document
   module_context context;           // owning context, set on registration
   string filepath;                  // normalized path to the loaded module
   struct anvl_doc_header_t *header; // parsed header metadata
   list body;                        // list of anvl_doc_statement pointers
};
```

## Parsing rules

Body statements are parsed sequentially until EOF. The parser skips whitespace and comments between statements.

Statements are terminated by `;` — adopted for minimization, not newline-significant. `,` is used only to separate elements *inside* a collection (`array`, `tuple`, `object`); it never separates statements.

Before dispatching on statement or value kind, the parser checks `Source.dialect(doc->source)` and fails immediately via `Source.set_error` if the construct isn't legal for that dialect (see **Dialect scope**) — the same fail-fast pattern the header scanner uses today.

For each statement:

1. Read the identifier.
2. If next is `:` (and not `:=`), parse the inheritance `base` identifier. `base` is legal on either form — it doesn't determine which one follows.
3. If next is `@[`, collect statement-level attributes into `statement->attributes`. AML-only — AMP forbids statement-level attributes entirely.
4. Require either `:=` (→ `ASSIGN`; parse the value into `statement->value` — if `base` was set in step 2, `{` is now required immediately, deterministically, not checked after the fact) or `{` (→ `OBJECT_BLOCK`; recursively parse a nested statement list into `statement->body`, applying this same five-step process per nested statement until the matching `}`). Anything else here is a parse error — including a `: base` with neither `:=` nor `{` following, and `: base :=` followed by anything other than `{`.
5. Record the full source span from the identifier through the terminating `;` (`ASSIGN`) or the `;` after the closing `}` (`OBJECT_BLOCK`).

`vars { ... }` and `using "path";` are the two exceptions to this dispatch — they're their own statement kinds, not `ASSIGN`/`OBJECT_BLOCK` variants, and both are valid in **AnvilScript** only.

**Confirmed** — attribute placement is between `[: base]` and `{`/`:=`, not leading the statement. `test/fixtures/f08_attributes.anvl` matches this for the `ASSIGN` form:

```anvl
server @[active] := localhost;
endpoint @[env=production] := localhost;
server @[env=production, active] := { host := localhost; port := 8080; };
```

`test/fixtures/f35_full.anvl` does not match this or the always-`:=`-for-`ASSIGN` rule (space-separated `: =` instead of `:=`, a single-quoted unterminated `import 'other.anvl'`, and a bare `document { ... }` block that happens to accidentally look right under the *new* rule but was written under neither rule) — treating it as stale/pre-rewrite and safe to retire or rewrite, not as a syntax reference.

Value parsing (inside `ASSIGN`'s `value`):

- Parse literals (null, bool, integer, float, string, blob) as scalar value nodes. See **Numeric literal grammar** below for number classification and the open `#hex` question.
- Parse arrays `[ ... ]` (scalars only for AMP; any value for AML) as collection value nodes. Tuples `( ... )` follow the same shape but are AML-only pending the AMP-tuple decision.
- Parse an object literal `{ ... }` as an `ANVL_VALUE_OBJECT` node whose `object.statements` is a nested statement list — the exact same recursive parse as `OBJECT_BLOCK`'s `body` (step 4 above), just reached through `:=` instead of directly. There is no `PAIR`/key-value form.
- Bare identifiers are parsed as `IDENTIFIER` nodes — a static reference to another statement's value, resolved later. No varrefs or interpolation exist in this grammar.

## Error handling

Errors route through `Source.set_error(doc->source, ...)`. Common error cases:

- Unexpected token at statement start.
- Neither `:=` nor `{` follows the identifier (or an optional `: base` / `@[...]`) — includes a bare `name : base;` with nothing after it.
- Missing value after `:=`.
- Unterminated string, array, tuple, object-block/object-value `{ }`, or attribute block.
- Malformed blob tag or content.
- Invalid identifier in a static value reference.
- Dialect-illegal construct (e.g. `: base`, `vars`, `using`, statement attributes, or `object` in an AMP document).

## Testing strategy

**Status: written, RED-confirmed.** `test/unit/test_body_amp.c` (17 cases, AMP00–AMP16) and `test/unit/test_body_aml.c` (12 cases, AML00–AML11) both build clean and run clean — every case fails on a real assertion, none crash, both are Valgrind-clean (0 errors, 0 leaks) against the `doc_parse_body` stub in `src/core/document.c`. `AMP17` (tuple-in-AMP) was left out rather than guessed at, since that question is still open above. Types added to support this: `anvl_stmt_kind`, `anvl_value_type`, `anvl_doc_value_t`/`anvl_doc_value`, `anvl_doc_statement_t`/`anvl_doc_statement` in `include/internal/module.h`, plus `list body` on `anvl_mod_doc_t` and the `doc_parse_body` prototype. `doc_dispose` now frees `doc->body` (list-only for now — the stub never populates entries, so there's nothing per-element to free yet; extend this once real parsing owns statement/value allocations). Makefile targets `body_amp`/`body_aml` follow the existing `make <suite> <action>` convention. Ready for GREEN.

Two separate test files/suites, not one — matching the fact that AMP is a strict grammar subset of AML: anything AMP-legal behaves identically in AML, so the AML suite only needs to cover what AML adds, never re-proving what the AMP suite already proved. AMP's suite carries both its positive cases *and* its negative (AMP-illegal-construct-rejected) cases, since those rejections are exactly what makes it a subset.

### AMP suite — `test/unit/test_body_amp.c` (new), cases AMP00+

Positive — everything AMP-legal (`ASSIGN` only, no `base`, scalars + scalar `array`):

- **AMP00** — empty body returns OK and an empty statement list.
- **AMP01** — integer assignment `name := 42;`, including negative (`-1285`) and `UINT64_MAX`-scale (`18446744073709551615`).
- **AMP02** — float assignment, including negative and scientific-notation (`1.7976931348623157e+308`).
- **AMP03** — string assignment `name := "hello";`.
- **AMP04** — blob assignment, both tagged (`` name := @date`2026-07-07`; ``) and untagged (`` name := `raw`; ``).
- **AMP05** — scalar array assignment `name := [1, 2, 3];`.
- **AMP06** — multiple statements captured in order.
- **AMP07** — missing value after `:=` reports error.
- **AMP08** — unterminated array reports error.
- **AMP09** — invalid blob tag reports error.
- **AMP10** — an identifier followed by neither `:=` nor `{` (e.g. bare `name;`) reports a parse error.

Negative — AMP-illegal constructs rejected, one assertion per construct, mirroring the HDR AMP-forbidden test pattern:

- **AMP11** — `: base` on `ASSIGN` rejected.
- **AMP12** — `ANVL_STMT_OBJECT_BLOCK` rejected in all three forms (namespace, immutable+attribs, inherit-with-override).
- **AMP13** — `vars { ... }` rejected.
- **AMP14** — `using "path";` rejected.
- **AMP15** — statement-level `@[...]` attributes rejected.
- **AMP16** — an `object`-typed `ASSIGN` value (`name := { ... };`) rejected.
- **AMP17** — tuple rejected, *if* the open tuple-AMP-legality question resolves "no."

### AML suite — `test/unit/test_body_aml.c` (new), cases AML00+

Only the constructs AML adds beyond AMP — no re-testing of scalar/array assignment, which AMP01–06 already cover and which behave identically in AML:

- **AML00** — tuple assignment `name := (a, b, c);` (`f06_tuple.anvl`) — moves here permanently, or merges into AMP17 as a positive case, once the AMP-tuple question resolves either way.
- **AML01** — object-as-value assignment (`f07_object.anvl`'s `config := { host := localhost; port := 8080; };`) — `ANVL_VALUE_OBJECT` wrapping a nested statement list, not `PAIR`s.
- **AML02** — static value reference (`f11_static_ref.anvl`) — `IDENTIFIER` value node, no interpolation.
- **AML03** — inheritance via the `ASSIGN` form (`body_assign_inherit.anvl`) — `base` set, value is an object.
- **AML04** — `derived : base := 42;` (`body_err_base_non_object.anvl`) reports a parse error — deterministic per **Statement structure**, not a post-hoc type check.
- **AML05** — namespace/scoping container (`body_block_namespace.anvl`).
- **AML06** — immutable object with attributes, no base (`f12_immutable_object.anvl`).
- **AML07** — inheritance with override via the block form (`body_block_inherit.anvl`).
- **AML08** — base *and* attributes together (`body_block_inherit_attrs.anvl`).
- **AML09** — unterminated object-block `{` reports error (`body_err_unterminated_block.anvl`).
- **AML10** — a bare `name : base;` with nothing after `base` reports a parse error (`body_err_bare_base.anvl`).
- **AML11** — import + static reference into the flat merged namespace (`f13_import.anvl`).

## Fixture coverage

Reviewed every file in `test/fixtures/` against the confirmed grammar; gaps found and fixed:

- **AMP suite uses inline buffers, not fixture files** — confirmed. AMP has no header, so there's nothing an on-disk fixture buys over a literal string in `test_body_amp.c`. `f00d_hint.amp` remains as-is (it's a dialect-detection stub for the header/source suites, unrelated to body-parse).
- **Stale fixtures cleaned up**:
  - `f11_varref.anvl` (used `$host` var-refs) → replaced with `f11_static_ref.anvl` (`base_var := 42; derived_var := base_var;`) — the real AML equivalent, a static resolve-once identifier reference.
  - `f12_interpolated.anvl` (used `$"Welcome, {user}"`) → replaced with `f12_immutable_object.anvl` (`config @[attrib] { addr := 0.0.0.127; port := 8080; };`) — reuses the slot for a previously-missing `OBJECT_BLOCK` case instead of trying to find a nonexistent AML equivalent for string interpolation.
  - `f13_import.anvl` → rewritten to valid `import "f01_bare_literal.anvl";` (double-quoted, semicolon-terminated) plus a static reference into the imported document's flat namespace (`alias := name;`), instead of a single-quoted unterminated import combined with a var-ref.
  - `f35_full.anvl` is still stale (space-separated `: =`, unterminated single-quoted import) and intentionally untouched this pass — it's a larger kitchen-sink fixture and wasn't in scope this round; still tracked as an open question.
- **New `OBJECT_BLOCK` fixtures added**, filling the gap where only `f07_object.anvl`'s `document { ... }` (namespace form) existed:
  - `body_block_namespace.anvl` — namespace/scoping container, no base, no attributes.
  - `body_block_inherit.anvl` — inheritance with override, block form (`derived : base { ... };`).
  - `body_assign_inherit.anvl` — inheritance with override, assignment form (`derived : base := { ... };`).
  - `body_block_inherit_attrs.anvl` — base and attributes together.
  - Object-as-value assignment (`name := { ... };`) needed no new fixture — `f07_object.anvl`'s `config := { host := localhost; port := 8080; };` already covers it.
- **New error fixtures added**, matching the `f28`–`f34` error-fixture pattern:
  - `body_err_unterminated_block.anvl` — `{` never closed.
  - `body_err_base_non_object.anvl` — `derived : base := 42;`, exercising the deterministic base-implies-object grammar rule.
  - `body_err_bare_base.anvl` — `name : base;` with nothing after `base`.
- `f01_bare_literal.anvl`'s `name := David;` is exactly the `IDENTIFIER`-value case, just under the older informal name "bare literal" — no gap, just a terminology note.

## Relationship to header scan and import loading

The body parser runs after:

```
doc_load_source(doc, origin, source, len, err)
mod_ctx_register_doc(ctx, doc, filepath, err)
doc_scan_header(doc, err)
mod_load_imports(ctx, doc, err)
doc_parse_body(doc, err)
```

If `doc_scan_header` or `mod_load_imports` reports an error, body parsing is skipped for that document.

**Resolved — no document processing order is needed.** Body-parse only reads its own document's source (see **Responsibilities of the body parse** above), so no document ever needs another document's body to already be parsed. Why that holds is a Resolution-phase design question, not a body-parse one — see `notes/resolution-phase.md` § *Why document processing order doesn't matter*.

## Resolved questions

1. **Statement terminator**: `;` is mandatory after every statement, adopted for minimization. Only `array`/`tuple` elements are comma-separated; statements — including nested ones inside an `object` value or `OBJECT_BLOCK` body — are always `;`-terminated, never comma-separated (superseding this entry's original "and `object`" wording, from before object bodies were confirmed to be nested statement lists rather than comma-separated pairs).
2. **Two statement forms, not one, and `base` is valid on both**: `ident [: base] [@[...]] := value;` (`ASSIGN`) and `ident [: base] [@[...]] { statements };` (`OBJECT_BLOCK`) — distinct grammars (one ends in `:= value`, the other in a bare `{ }` block), but both accept an optional `base` and `@[...]` in the same position. Derived objects aren't required to be anonymous blocks — `derived : base := { override := 2; };` is equally valid to `derived : base { override := 2; };`. There is no bare `name : base;` with nothing after it in either form — that's always a parse error.
3. **Base implies object-shaped, deterministically**: seeing `: base` tells the parser this statement is object-shaped before it even reaches `:=`/`{`, so `: base :=` requires `{` immediately as a grammar production, not as a type check performed after generically parsing a value. `derived : base := 42;` fails the same way any other unexpected token does.
4. **The parser never judges `base`'s legality** — `doc_parse_body` captures `base` as a slice and nothing more; it doesn't care what it points to or whether inheriting/deriving from it is allowed. That's entirely the Resolution phase's concern — see `notes/resolution-phase.md`.
5. **Attribute placement**: between `[: base]` and `{`/`:=`, not leading the statement — confirmed against `f08_attributes.anvl` (`server @[active] := localhost;`) and `f07_object.anvl`.
6. **No `PAIR`/key-value object representation**: an object *value* (`name := { a := 1; b := 2; };`) is a nested statement list, exactly like `OBJECT_BLOCK`'s `body` — confirmed by `f07_object.anvl`. `ANVL_VALUE_PAIR` is removed from the grammar.
7. **Dialect gating**: enforced inline inside `doc_parse_body` via `Source.dialect(doc->source)`, failing fast through `Source.set_error` — the same pattern the header scanner already uses for AMP import/attribute rejection.
8. **AMP body grammar**: AMP is a flat message packet, not a module. Only `ASSIGN` with scalar values and scalar `array` is AMP-legal; `OBJECT_BLOCK` is entirely AMP-illegal (no namespace containers, no immutable objects, no inheritance), as are `vars`, `using`, and statement attributes. (`tuple` still open — see below.)
9. **Blob is a standard scalar**, not a special case — the escape hatch for arbitrary embedded payloads (JSON, binary, GLSL, markdown, etc.), legal in AMP as well as AML.
10. **No interpolation or dynamic var-refs in AML/AMP**: `ANVL_VALUE_VARREF` is removed from this grammar entirely. `${...}`/`$"..."` interpolation and dynamic value substitution are reserved for AnvilScript. AML/AMP support only *static*, resolve-once value reference via bare `ANVL_VALUE_IDENTIFIER` (`derived_var := base_var;`) — valid syntax, resolved a single time at Resolution, not evaluated dynamically.
11. **Compilation boundary**: `doc_parse_body` has no knowledge of `.anvlo` compilation; that's a separate post-parse concern for a future `anvilc` CLI tool.
12. **Arena sizing multiplier and floor**: `max(sum(Source.length()) × ANVL_ARENA_SIZE_MULTIPLIER, ANVL_ARENA_MIN_SIZE)`, both named constants in `include/constants.h`, implemented as `mod_ctx_arena_size_hint` (`src/core/module.c`) and tested by `CR20`. Still a reasoned-but-unmeasured guess pending real instrumentation data (see "Initial sizing heuristic" above) — resolved as the *current working default*, not as a final, precisely-tuned value.
13. **Body-parse processing order and topological sort are both moot** — see § *Relationship to header scan and import loading* above and `notes/resolution-phase.md` for why.

## Open questions

1. Should `vars` blocks be parsed as a statement kind with a nested statement list, or as a single value tree?
    - (deferred) `vars` is an **AnvilScript** feature.
2. Is `tuple` AMP-legal? Still undecided.
3. Is a bare `#<hex-digits>` token (`hex := #12D4F;`) an `INTEGER`, or does it need its own value type?
    - it is an integer (Anvil weak type is _`numeric`_)
4. Since object "keys" are now just nested `ASSIGN`/`OBJECT_BLOCK` statement names (no more `PAIR`), this question dissolves — a statement name is already an identifier by construction. Kept only as a marker in case some other key-like construct turns out to be needed.
5. **Fixture staleness**: `f35_full.anvl`, `f11_varref.anvl`, `f12_interpolated.anvl`, and `f13_import.anvl` all predate this session's grammar and/or the no-interpolation decision (the latter three use `$var`/`$"..."` syntax that no longer exists in AML). See **Fixture coverage** above — safe to retire or repurpose as future AnvilScript fixtures, not to treat as current syntax references.
    - `f35_full.anvl`: let's remove interpolation
    - `f11_varref.anvl`: we need to discuss _`static`_ & _`dynamic`_ `varref`
    - `f12_interpolated.anvl`: definitely repurpose for **AnvilScript**
    - `f13_import.anvl`: expand concern here

## Dev [BadKraft]

1. moved `setup_amp_doc` to `utilities/helpers.*`
2. added a `parser state` object to initialize parser
3. created a `parser context` wrapper for source with start/end timestamp values