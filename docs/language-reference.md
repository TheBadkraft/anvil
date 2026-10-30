# ANVL Language Reference

Status: **covers what's actually implemented and tested.** Schema (§10) is fully implemented —
see `docs/schema/README.md` for the introduction and `notes/native-schema.md` for the full
technical record; this page's own §10 stays a short pointer rather than duplicating either.
Every code example on this page has been verified against the real parser
(`anvil_load_buffer`), not just written to look right.

## 1. What ANVL Is

ANVL (Attributed Node Variadic Language) is one grammar, one parser, answering three different
problems most stacks solve with three different tools: declarative data modeling, structured
messaging, and (eventually) embedded scripting. Which problem a given document solves is
declared once, on the first line, and enforced from that point on — not inferred, not mixed.

The parser's own internal state is integers — byte offsets and lengths into the buffer you hand
it. No string is copied during parsing, and blob payloads are skipped entirely (the parser
records where a blob starts and how long it is, then moves on without reading its content). This
isn't a performance detail — it means the parser cannot expose payload data under adversarial
conditions (heap dump, memory probe, fuzzer), because it never held any.

## 2. Dialects

| Dialect | Shebang | Problem It Solves |
|---|---|---|
| **AML** | `#!aml` | Declarative data modeling and configuration |
| **AMP** | `#!amp` | Structured messaging and transport |
| **ASL** | `#!asl` | Embedded scripting *(design only — not implemented)* |

The shebang on the first line selects the dialect; the parser enforces that dialect's rules
immediately, not as a post-hoc check. See [`dialect-ownership-matrix.md`](dialect-ownership-matrix.md)
for the full per-feature ownership table (which dialect owns which grammar construct).

## 3. Document Structure

A document is a flat or nested sequence of **statements**. A statement has an optional
inheritance base, optional attributes, a name, and a value — in one of two equivalent forms:

```anvl
#!aml
name := David;

label {
   field := 1;
   other := "value";
};
```

Both `ident := value;` (assignment form) and `ident { ... };` (bare block form) produce the same
thing: a statement whose value is an object. They're indistinguishable through the public API —
`anvil_statement_get_value` returns the same kind of value either way. Nesting works identically
in both forms: a statement's value can itself contain further statements, to any depth.

**The `;` stays on the value's line.** A statement ends with `;`, and between a finished value and its
`;` only spaces and tabs are allowed: `x := hello ;` is fine, but `x := hello` followed by a newline and
then `;` (or a comment, or a block comment in between) is an error, "Expected ';' to end the statement".
This is deliberate and applies to every kind of value. Between statements, and inside object bodies,
newlines, blank lines and comments are unrestricted, and a comment after the `;` is the usual place for
one: `x := 1; // note`. A `/* ... */` comment that is never closed swallows the rest of the document, so
it is an error ("Unterminated block comment") reported at the line and column where the comment opens.

**Names are unique, and keywords are not names.** A name repeated inside one object is an error
(`ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT`, 4001), at every depth and in either form: `x := 1; x { ... };`
counts as a repeat. A name repeated at the top level of the merged document is still
`ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER` (4053, category `ANVIL_ERR_RESOLVE`), which also catches a collision with an
included file. A statement may still *override* a field it inherits (§6); that is not a repeat. The reserved
words `include`, `import`, `vars`, `true`, `false` and `null` (lower case only, and only as a whole word, so `Vars`,
`vars2` and `True` are fine) are refused as a statement name or object field at every depth
(`ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD`, 4003), and as an attribute key (§5, 4004). `import` and `vars` belong to
AnvilScript; with the AML reader they are simply reserved.

## 4. Value Kinds

| Kind | Example | Notes |
|---|---|---|
| `null` | `empty := null;` | |
| Bool | `flag := true;` | `true` / `false`, reserved keywords |
| Numeric | `n := -1285;` / `n := 3.14;` / `n := 1.7976931348623157e+308;` | Grammar `-?digits(.digits)?([eE][+-]?digits)?`: optional leading `-`, a decimal point only when digits follow it, optional `e`/`E` exponent whose sign is optional (`1e3`, `1E3`, `1e+3` and `1e-3` are all numbers). `5.`, `-5.`, `5.e3`, `1e`, `1e+` and `1e-` are parse errors (`ANVL_ERR_PARSER_INVALID_NUMBER` / `ANVL_ERR_PARSER_INVALID_EXPONENT`) at every depth. A number must end at a value boundary; otherwise the token is a bare word (`007x`, `1ex`, `1.5.5`, `09-02-2026`). A document that wrote `1e5` as a bare word now reads a number; quote it (`"1e5"`) to keep it text. No hex literal form today. |
| String | `s := "line1\nline2 \"quoted\"";` | Double-quoted, standard backslash escapes |
| Blob | `created := @date\`2026-07-07\`;` | `@tag\`content\`` — tag is optional (`@\`raw content\`` is also legal); content is a raw span, never interpreted by the parser |
| Array | `tags := [ alpha, beta, gamma ];` | Variable length; empty arrays (`[]`) are invalid |
| Tuple | `coords := (10, 20);` | At least 2 elements; `(1)` is invalid |
| Object | `config := { host := localhost; port := 8080; };` | Nested statements |
| Bare/identifier literal | `status := active;` | An unquoted symbol — a literal string value, no implicit resolution |
| `$identifier` (VarRef) | `alias := $name;` | Resolve-once static alias in AML — see §7's note below. Transparent through the public API: reading `alias`'s value reports whatever `name` itself resolved to, never a distinct "this was a reference" kind. |

Anvil's default is a **weak type system** — nothing above requires or infers a declared shape.
`foo := 42;` is simply a numeric value.

## 5. Attributes

`@[key]` (a flag) or `@[key=value]` (comma-separated, mix freely) attach to *any* statement, at
any nesting depth, in AML:

```anvl
#!aml
server @[env=production, active] := {
   host := localhost;
};
```

Attributes are AML-only — AMP forbids them entirely (see §11). Module-level attributes (in the
document header, before any statements) use the identical `@[...]` syntax and are read the same
way through the public API (`anvil_document_get_attribute*` vs. `anvil_statement_get_attribute*`).

**Whitespace inside an attribute list is free.** Spaces, tabs and newlines are all allowed
anywhere between the tokens of an attribute list, so a multi-line list is valid, and splitting one
over several lines is purely a matter of style:

```anvl
ticket @[
   type=bug,
   status="open"
] {
   title := "Login fails";
};
```

An attribute's value never includes the whitespace around it: in `@[status="open" ]` the value is
`"open"`, and in `@[type=bug ,` it is `bug`. This is the opposite of the one place a newline *is*
significant, a statement's `;`, which must stay on its value's line (§3).

**Attribute rules.**

- A key may appear once per statement, and once across the module header's attribute blocks. A repeat is
  `ANVL_ERR_PARSER_DUPLICATE_ATTRIBUTE_KEY` (4013), reported at the repeated key (category `ANVIL_ERR_SYNTAX` on a
  statement, `ANVIL_ERR_HEADER` in the module header).
- A key must be an identifier, and not a keyword or reserved word (§3): `@[include]`, `@[true]` and `@[null=1]` are
  `ANVL_ERR_PARSER_ATTRIBUTE_IS_KEYWORD` (4004). A key that is not an identifier (`@[1a]`) is
  `ANVL_ERR_PARSER_INVALID_ATTRIBUTE` (4012).
- A value is a single scalar: a number, a string, a blob or a bare word. It can't be an array, an object or a tuple,
  and a `$` reference is refused too (2009). The attribute list ends at the first `,` or `]`, so an array, object or
  tuple that is cut there (`@[k=[1,2]]`, `@[k=(1,2)]`, `@[k={a:=1,b:=2}]`, or one that never closes) is
  `ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE` (2006), at the value. A bracketed value that closes before the
  next `,` or `]` (`@[k={a}]`, `@[k=(1)]`) is read as a bare word, text with the brackets in it.
- `@[]` is `ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK` (2008).
- Two keys with no comma between them (`@[a b]`) are `ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES` (3014).
- An `@` that does not open `@[` is `ANVL_ERR_PARSER_INVALID_ATTRIBUTE_BLOCK` (4011).
- Module attributes must come before the first statement; `@[...]` on its own line after a statement is
  `ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES` (3015).

## 6. Inheritance

A statement can declare a base with `: base`, merging the base's own fields into its own:

```anvl
#!aml
base := {
   x := 1;
};
derived : base := {
   y := 2;
};
```

`derived` ends up with both `x` (inherited) and `y` (its own) — fields are merged by aliasing the
ancestor's real field objects, not by copying them, and a closer override always wins over a
farther one in a transitive chain (`c : b : a`). Inheriting from an anonymous (bare `{ }`, no
name of its own to be a base) statement is a hard error, and an inheritance cycle is rejected —
both checked before any merge happens.

A base must be an **object value**, declared as `name := { ... }`. A base whose value is a scalar, array,
tuple, string, blob or bare word is an error, and so is a `$` reference, even one that points at an object
(`ANVL_ERR_PARSER_INHERITANCE_REQUIRES_OBJECT`, 4020; category `ANVIL_ERR_RESOLVE`, found when the document is
resolved, reported at the base name in the deriving statement). A base that doesn't exist is 4051, an inheritance
cycle 4050, and a base declared in the bare block form `name { ... }` is 4052 (the resolver treats that form as
anonymous for this purpose, although the two forms are otherwise the same statement; see §3). A
`name { ... }` statement can still *have* a base: `d : b { ... }` is fine.

## 7. Includes

```anvl
include "path/to/file.anvl";
```

One quoted path per include, no aliasing and no namespaces — an included document's statements
merge flat into the including document's own namespace. A name collision between an include and
the including document (or between two includes) is a hard error, not a silent override. Diamond
includes (the same file reached via two different paths) are deduplicated by content, not
re-parsed twice; include cycles are rejected. `$identifier` VarRefs (§4) resolve across the whole
merged include graph, not just within one file.

**`include` comes first.** The module header holds, in order, the shebang, then `include` declarations and module
attributes; the first statement ends it. An `include` after any statement is
`ANVL_ERR_INCLUDE_NOT_FIRST` (4201), reported at the line and column where the word `include` starts (category
`ANVIL_ERR_SYNTAX`, or `ANVIL_ERR_HEADER` when only header lines precede it). A second `#!` after an opening shebang,
wherever it is, is `ANVL_ERR_PARSER_MULTIPLE_SHEBANG` (2004); a `#!` that is not on the first line, when no
shebang opened the document, is `ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS` (2005).

## 8. Weak Typing (the Default)

Nothing in §3–§7 requires a value to declare its shape. A document that never needs more than
"here's some structured data" never has to think about types at all — that's the default, and it
never changes for anyone who doesn't opt into more.

## 9. The Opt-In Type System

Layered on top of the weak-typing default: name a reusable, constrained shape once
(`VIN := { type := String; size := 17; };`, in a document carrying the `@[types]` module
attribute), then reference it — `type := types.VIN;` — from anywhere that includes the file
defining it. Full reference, including the native primitive vocabulary, enums, and cross-file
resolution: [`types-reference.md`](types-reference.md).

## 10. Schema

AnvilSchema validates a *data* document's shape against declared, type-annotated fields,
building on the type system above. Implemented: `@[schema]` loading; required-field presence;
type-kind validation; nested `Object` and `Array` shapes (`fields`, `items`, `min_items`,
`max_items`) with full-path violations; `nullable`, `integer`, `format` and the `Blob` primitive
with `tag`; exact `size`/`min`/`max`/`values` checking (inherited from a resolved `types.X` custom
type when a field doesn't declare its own); validation of a bare object value; load diagnostics;
and read-back introspection with extension keys. Every violation is collected in one pass rather
than fail-fast. For why it's designed the way it is (same parser, zero new grammar, not a bolt-on
the way XSD/XSLT or JSON Schema are) and a worked example, see
[`schema/README.md`](schema/README.md). Full technical design record:
[`notes/native-schema.md`](../notes/native-schema.md).

## 11. AMP's Restrictions

AMP forbids objects, attributes, inheritance, and includes outright — rejected at parse time, no
separate runtime guard layer. AMP exists for structured messaging/transport, where the sender and
receiver already agree on shape out of band; the restrictions keep the wire format flat, keep the
parser's own security property (§1) simple to reason about, and make AMP equally at home on any
transport, including connectionless ones like UDP, since there's no session-level state a stricter
grammar would otherwise need.

## 12. Errors

A failed operation reports one of a small set of stable categories, never a raw internal error
code (so internal churn is never a public API break):

| Category | Meaning |
|---|---|
| `ANVIL_ERR_IO` | Couldn't read the source at all |
| `ANVIL_ERR_HEADER` | Shebang/include/attribute header scan failed |
| `ANVIL_ERR_INCLUDE` | Include graph loading failed |
| `ANVIL_ERR_SYNTAX` | Body/grammar error |
| `ANVIL_ERR_RESOLVE` | Identifier/base/inheritance resolution failed |
| `ANVIL_ERR_MEMORY` | Allocation failure |
| `ANVIL_ERR_INVALID_ARGUMENT` | Bad call into the API itself |

A failed `anvil_load`/`anvil_load_buffer` still returns a real, non-NULL document handle
(`anvil_has_errors`/`anvil_get_error` report the failure) — `NULL` is reserved for the one truly
foundational case, the handle itself failing to allocate. Where available, `anvil_document_get_error`
gives a richer detail object (message text, line, column) beyond the stable category alone.

### Specific error codes (internal)

The categories above are the public contract. Underneath, the reader records one specific code per reason, and
`anvil_document_get_error` and the conformance dumps carry it. The codes below were made specific for the grammar
rules in §3 to §7 (a rejected input keeps its category and place; only the code is sharper):

| Code | Name | Reported for |
|---|---|---|
| 2004 | `ANVL_ERR_PARSER_MULTIPLE_SHEBANG` | a second `#!` after an opening shebang |
| 2005 | `ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS` | a `#!` with no opening shebang on the first line |
| 2006 | `ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE` | an array, object or tuple as an attribute value (cut short or unclosed) |
| 2008 | `ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK` | `@[]` |
| 3001 | `ANVL_ERR_PARSER_EXPECTED_OBJECT_FIELD` | a missing field name in an object body |
| 3004 | `ANVL_ERR_PARSER_TRAILING_COMMA_IN_OBJECT` | a comma before `}` in an object body |
| 3014 | `ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES` | two attribute keys with no comma |
| 3015 | `ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES` | `@[...]` after a statement |
| 4001 | `ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT` | a field repeated in one object |
| 4002 | `ANVL_ERR_PARSER_INVALID_KEY_IN_OBJECT` | a token that cannot be a field name in an object body |
| 4003 | `ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD` | a keyword as a statement name, object field or base |
| 4004 | `ANVL_ERR_PARSER_ATTRIBUTE_IS_KEYWORD` | a keyword as an attribute key |
| 4007 | `ANVL_ERR_PARSER_EMPTY_TUPLE_ELEMENT` | an empty tuple element, `(1,,2)` or `(,1)` |
| 4011 | `ANVL_ERR_PARSER_INVALID_ATTRIBUTE_BLOCK` | an `@` that does not open `@[` |
| 4012 | `ANVL_ERR_PARSER_INVALID_ATTRIBUTE` | an attribute key that is not an identifier |
| 4013 | `ANVL_ERR_PARSER_DUPLICATE_ATTRIBUTE_KEY` | an attribute key repeated on a statement or in the module header |
| 4020 | `ANVL_ERR_PARSER_INHERITANCE_REQUIRES_OBJECT` | a base that is not an object value (category `ANVIL_ERR_RESOLVE`) |
| 4053 | `ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER` | a top-level name repeated (including across includes) |
| 4201 | `ANVL_ERR_INCLUDE_NOT_FIRST` | `include` after a statement |
| 5001 | `ANVL_ERR_PARSER_UNEXPECTED_CHAR` | a control character where a token belongs |

**Reserved, never reported.** These codes are declared in `include/errors.h` and have names, but no input produces
them, so a consumer can't see one:

- 2007 `ANVL_ERR_PARSER_INVALID_IDENTIFIER`: an invalid attribute key is now 4012, and an empty block 2008.
- 4015 `ANVL_ERR_ANON_BLOCK_REDECLARATION` and 4203 `ANVL_ERR_INCLUDE_DUPLICATE_ALIAS`: the language defines no such
  rule (a block and a name repeated at the top level is 4053; includes have no aliases). They stay reserved and
  unused.
- 3011 and 4005 (synonyms of 4009, an empty array, and 3013, a one-element tuple) and 3016 (no input fits; a missing
  `:=` is 2002).
- The AnvilScript-only codes (4101-4105, 4305-4307, 4701-4703, 5101-5105), which need the AnvilScript build.
- Leftover codes that the AML reader never reaches: 4008, 4010, 4014, 4501, 5004, 5006, 5007, 5011, 5012, 9001 and
  9999. A later change may report them; until then each mistake they name is reported under another code, or accepted.

The full table, with the first conformance case that reaches each code, is in `notes/conformance-error-coverage.md`.

## 13. The Public API and Language Bindings

### This repo's own public C surface

Anvil Native exposes two equivalent ways to call into it:

- **`anvil_flat.h`** — plain function calls (`anvil_load`, `anvil_statement_get_value`,
  `anvil_document_get_statements`, ...). This is the primary, most-used surface.
- **`anvil_vtable.h`** — the same functions grouped into `const` struct-of-function-pointers
  (`Anvil.load`, `Statement.get_value`, `Document.get_statements`, ...), for a caller that prefers
  a vtable-style call convention. Every field is pointer-identical to its flat counterpart —
  there are never two implementations to keep in sync.

Every public handle (`anvil_document`, `anvil_statement`, `anvil_value`, `anvil_attribute`,
`anvil_type_registry`, `anvil_type_def`, ...) is opaque — never dereferenced by the caller, only
ever passed to an accessor function. The guiding design principle throughout: **primitives, not
policy** — Anvil Native's job is to expose correct, general-purpose primitives; how a particular
language binding's runtime paradigm (garbage collection, RAII, reference counting, ...) adapts
those primitives idiomatically is that binding's own problem to solve, not something the core
library bends its own contracts to accommodate.

The core parser/resolver is mandatory; the opt-in type system (`anvil_types.c`) ships as part of
ANVL proper alongside it. AnvilSchema (`anvil_schema.h`) is a separate, optional add-on built
entirely on the same public API, and so are the streaming writer and document builder
(`anvil_writer.h`, `anvil_builder.h`), which emit canonical AML and AMP.

**The writer and builder refuse what the reader refuses.** Anything either of them accepts loads back without error.
Their error codes (`anvil_writer_err_code` in `anvil_writer_types.h`) are their own set, and the first error is sticky.
These cover the grammar rules above:

| Code | Value | Refused |
|---|---|---|
| `ANVIL_WRITER_ERR_RESERVED_WORD` | 6 | a keyword as a statement name, base, object field or attribute key |
| `ANVIL_WRITER_ERR_DUPLICATE_NAME` | 15 | a top-level name declared twice (the reader's 4053) |
| `ANVIL_WRITER_ERR_DUPLICATE_FIELD` | 17 | a field name repeated in one object, at any depth (the reader's 4001) |
| `ANVIL_WRITER_ERR_DUPLICATE_ATTRIBUTE_KEY` | 18 | an attribute key repeated on one statement, or in the module header (the reader's 4013) |

Codes 17 and 18 were appended, so the existing values keep their numbers. The writer can't see names that an
`include` brings in, so a collision with an included name is found only when the document is read.

### Real language bindings today

| Binding | Runtime |
|---|---|
| `anvil.node` | Node.js (N-API) |
| `anvil.wasm` | Browser and Node.js (WebAssembly/Emscripten) |
| `anvil.net` | .NET 9 (C#, or any CLR language) |
| `anvil.py` | CPython 3.11+ (`ctypes`) |
| `anvil.java` | Java (kept in step with the others; not distributed) |

Each binding builds on this library at a pinned version and wraps the public C API in its own
language's idiom. All of them read documents, statements and attributes (raw and decoded
values). Node.js, WebAssembly, .NET and Python also provide the type registry, schema validation,
and the writer and builder, and Node.js and WebAssembly share one API. Additional bindings can be added the same
way, without requiring any change to Anvil Native itself. Usage and downloads are in the
Bindings guide.

## 14. Where to Go Next

- [`types-reference.md`](types-reference.md) — the full opt-in type system reference.
- [`schema/README.md`](schema/README.md) — AnvilSchema, why it exists and how it's built.
- [`getting-started.md`](getting-started.md) — loading a document, walking statements, reading
  values end to end.
- `notes/` (in the main repo, not user-facing) — design-thread records for anything still in
  progress; the most current source of "why," even ahead of this page.
