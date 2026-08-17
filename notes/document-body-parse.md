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

## Value structure

Values form a recursive tree. Every value node has a type, a source span, and type-specific children.

```c
typedef enum {
   ANVL_VALUE_NONE,
   ANVL_VALUE_NULL,
   ANVL_VALUE_BOOL,
   ANVL_VALUE_INTEGER,
   ANVL_VALUE_FLOAT,
   ANVL_VALUE_STRING,
   ANVL_VALUE_BLOB,        // standard scalar; tag stored separately, content in text span
   ANVL_VALUE_ARRAY,
   ANVL_VALUE_TUPLE,
   ANVL_VALUE_OBJECT,      // nested statement list, not key/value pairs — see below
   ANVL_VALUE_IDENTIFIER,  // bare symbol: static reference to another statement's value
} anvl_value_type;

typedef struct anvl_doc_value_t {
   anvl_value_type type;
   anvl_slice text;         // full source span of the value
   anvl_slice tag;          // blob tag (e.g. @date, @sel); empty for non-blobs or untagged blobs
   union {
      struct {
         list items;        // array/tuple: list of anvl_doc_value_t *
      } collection;
      struct {
         list statements;   // object: list of anvl_doc_statement pointers — same shape as OBJECT_BLOCK's body
      } object;
   };
} anvl_doc_value_t;
typedef anvl_doc_value_t *anvl_doc_value;
```

Notes:

- Scalar values (null, bool, integer, float, string) are fully described by `type` and `text`.
- `BLOB` is a standard scalar, not a special case — it's the escape hatch for embedded payloads of any kind (JSON, binary, GLSL, markdown, whatever). Syntax is backtick-delimited content with an optional `@tag` prefix: `` @date`2026-07-07` ``, `` @sel`#input-date` ``, or untagged `` `raw content` `` (empty `tag` slice). Legal in AMP as well as AML.
- Array and tuple children live in `collection.items`.
- **There is no `PAIR`/key-value object representation.** `config := { host := localhost; port := 8080; };` uses ordinary `:=` statements inside the braces — confirmed by `f07_object.anvl` — not `key: value` JSON-style pairs as an earlier draft assumed. So a `:=`-assigned object value and an `ANVL_STMT_OBJECT_BLOCK`'s body are the exact same shape: a nested `list` of `anvl_doc_statement` pointers. `object.statements` here is that same nested list, reached via the value tree instead of directly off a statement.
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

Observed tolerance:

- Optional leading `-` sign on both integers and floats.
- A plain digit sequence classifies as `INTEGER`.
- A `.` followed by digits classifies as `FLOAT`.
- Scientific notation (`e`/`E`, optional `+`/`-` exponent sign, digit sequence) classifies as `FLOAT`.
- Integer literals must round-trip values beyond signed 64-bit range — `18446744073709551615` is `UINT64_MAX`. Since the body parser never canonicalizes values (see Responsibilities), this only requires the raw span to be captured intact; no overflow check happens at parse time. Classification is purely syntactic.

**Open**: `hex := #12D4F;` — is a bare `#<hex-digits>` token an `INTEGER` (base-16), or does it need its own value type? The `#` prefix is already overloaded elsewhere (bare `#` alone is rejected; `#RRGGBB`/`#identifier` are accepted per the legacy parser's bare-`#` rule), so this needs an explicit decision before a hex-literal BODY test case is written.

## Storage details

`statements` is a Sigma list with `sizeof(void *)` stride, storing pointers to heap-allocated `anvl_doc_statement_t` entries. Each statement owns:

- Its own scalar fields and slices.
- Its `value` tree (ASSIGN) or `body` list (OBJECT_BLOCK), which recursively own their child statements/values — an `ANVL_VALUE_OBJECT`'s `object.statements` list owns its statements exactly like an `OBJECT_BLOCK`'s `body` does.
- Its `attributes` list (if any).

Statement disposal iterates the body list, frees each statement, recursively frees the attached value tree or nested statement body, and disposes the list.

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

**Resolved — no processing order is needed at all.** Body-parse only reads its own document's source, as this doc's **Responsibilities** section states. The resolver is designed around a global identifier map, not order-dependent traversal: `derived : base { ... };` resolves by looking `base` up in a map that's already complete by the time any resolution happens, because Resolution (phase 4) only starts once every document in the import graph has been body-parsed. As long as that phase boundary holds — all bodies parsed before any resolving begins, order among them doesn't matter — the resolver only fails when a lookup misses, never because of processing order. This makes `aml-import-namespace-rules.md` § 7's "parse bodies in reverse dependency order" requirement unnecessary under this design, not just unproven.

`notes/document-header-scan.md` § *Deferred to Resolution phase* separately corrects `aml-import-namespace-rules.md` § *Import graph order*'s claim that reversing DFS discovery (pre-order) gives a valid bottom-up order — that reasoning is still wrong under diamond imports (this project explicitly supports them, HDR13) regardless of whether anything ends up needing the order fixed.

## Resolved questions

1. **Statement terminator**: `;` is mandatory after every statement, adopted for minimization. Only `array`/`tuple`/`object` elements are comma-separated; statements never are.
2. **Two statement forms, not one, and `base` is valid on both**: `ident [: base] [@[...]] := value;` (`ASSIGN`) and `ident [: base] [@[...]] { statements };` (`OBJECT_BLOCK`) — distinct grammars (one ends in `:= value`, the other in a bare `{ }` block), but both accept an optional `base` and `@[...]` in the same position. Derived objects aren't required to be anonymous blocks — `derived : base := { override := 2; };` is equally valid to `derived : base { override := 2; };`. There is no bare `name : base;` with nothing after it in either form — that's always a parse error.
3. **Base implies object-shaped, deterministically**: seeing `: base` tells the parser this statement is object-shaped before it even reaches `:=`/`{`, so `: base :=` requires `{` immediately as a grammar production, not as a type check performed after generically parsing a value. `derived : base := 42;` fails the same way any other unexpected token does.
4. **The parser never judges `base`'s legality** — whether something is a valid target to inherit/derive from (the "cannot be inherited but can be derived" distinction) is entirely the post-parse resolver's concern, once it resolves static value references. `doc_parse_body` captures `base` as a slice and nothing more; it doesn't care what it points to or whether that's allowed. Moved to `notes/deferred-work.md` § *Deferred to Resolution phase* since it no longer blocks body-parse.
5. **Attribute placement**: between `[: base]` and `{`/`:=`, not leading the statement — confirmed against `f08_attributes.anvl` (`server @[active] := localhost;`) and `f07_object.anvl`.
6. **No `PAIR`/key-value object representation**: an object *value* (`name := { a := 1; b := 2; };`) is a nested statement list, exactly like `OBJECT_BLOCK`'s `body` — confirmed by `f07_object.anvl`. `ANVL_VALUE_PAIR` is removed from the grammar.
7. **Dialect gating**: enforced inline inside `doc_parse_body` via `Source.dialect(doc->source)`, failing fast through `Source.set_error` — the same pattern the header scanner already uses for AMP import/attribute rejection.
8. **AMP body grammar**: AMP is a flat message packet, not a module. Only `ASSIGN` with scalar values and scalar `array` is AMP-legal; `OBJECT_BLOCK` is entirely AMP-illegal (no namespace containers, no immutable objects, no inheritance), as are `vars`, `using`, and statement attributes. (`tuple` still open — see below.)
9. **Blob is a standard scalar**, not a special case — the escape hatch for arbitrary embedded payloads (JSON, binary, GLSL, markdown, etc.), legal in AMP as well as AML.
10. **No interpolation or dynamic var-refs in AML/AMP**: `ANVL_VALUE_VARREF` is removed from this grammar entirely. `${...}`/`$"..."` interpolation and dynamic value substitution are reserved for AnvilScript. AML/AMP support only *static*, resolve-once value reference via bare `ANVL_VALUE_IDENTIFIER` (`derived_var := base_var;`) — valid syntax, resolved a single time at Resolution, not evaluated dynamically.
11. **Compilation boundary**: `doc_parse_body` has no knowledge of `.anvlo` compilation; that's a separate post-parse concern for a future `anvilc` CLI tool.
12. **Body-parse processing order and topological sort are both moot.** The resolver design is map-based, not order-dependent: every document in the import graph gets body-parsed (in any order) before Resolution starts, so by the time anything resolves a `base`/`IDENTIFIER` reference, every statement in the whole graph is already registered in an identifier map. Resolution only fails when a lookup misses, never because of processing order. This resolves the conflict previously noted here against `aml-import-namespace-rules.md` § 7 — that note's bottom-up-order requirement isn't necessary either, given this resolver design. See `notes/document-header-scan.md` § *Deferred to Resolution phase* for the corresponding update (the DFS-pre-order-vs-diamond-imports bug I found there is still a true bug in that note's reasoning, but fixing it is no longer motivated by an ordering requirement — nothing needs the order fixed).

## Open questions

1. Should `vars` blocks be parsed as a statement kind with a nested statement list, or as a single value tree?
2. Is `tuple` AMP-legal? Still undecided.
3. Is a bare `#<hex-digits>` token (`hex := #12D4F;`) an `INTEGER`, or does it need its own value type?
4. Since object "keys" are now just nested `ASSIGN`/`OBJECT_BLOCK` statement names (no more `PAIR`), this question dissolves — a statement name is already an identifier by construction. Kept only as a marker in case some other key-like construct turns out to be needed.
5. **Fixture staleness**: `f35_full.anvl`, `f11_varref.anvl`, `f12_interpolated.anvl`, and `f13_import.anvl` all predate this session's grammar and/or the no-interpolation decision (the latter three use `$var`/`$"..."` syntax that no longer exists in AML). See **Fixture coverage** above — safe to retire or repurpose as future AnvilScript fixtures, not to treat as current syntax references.
