# FR-2603-anvil-001 — Anonymous Top-Level Object Blocks

| Field       | Value                                      |
|-------------|--------------------------------------------|
| ID          | FR-2603-anvil-001                          |
| Status      | open                                       |
| Priority    | post-v1.0                                  |
| Tags        | parser, syntax, read-only, anonymous-block |
| Tracked in  | `docs/port-checklist.md` § E1             |

---

## Summary

Support `ident { ... }` as a first-class top-level declaration syntax for **read-only named
object blocks**, complementing the existing `vars { ... }` reserved-keyword form.

---

## Motivation

The pattern appears naturally in Anvil source files (e.g. `registry { ... }`, `changelog { ... }`)
as a way to group related data under a namespace without implying mutability. The current parser
requires `ident := { ... }`, which carries assignment semantics that are misleading for
definitional data.

---

## Syntax

```anvl
#!aml
changelog {
    version := 1.0.0
    date    := 2026-03-23
    author  := badkraft
}
```

Parsed identically whether or not a shebang is present; valid in both AML and ASL dialects.

---

## Semantics

### Read-only

Anonymous blocks are **immutable after parse**. Attempting to reassign the block name is a
compile-time error:

```anvl
changelog { ... }
changelog := something   // ERROR: ANVL_ERR_ANON_BLOCK_REDECLARATION
```

The `:=` operator is intentionally absent from the declaration syntax to signal immutability.
Consumers may read fields via `Context.get_field_by_name` / `Context.get_statement_by_name` but
the block cannot be patched through any API call.

### Not desugared to assignment

Anonymous blocks must **not** desugar internally to `ident := { ... }`. They warrant a distinct
stmt type so that:
- `Statement.type(s)` returns an unambiguous `ANVL_STMT_BLOCK` (or similar) value
- The read-only flag is encoded at the type level, not as a runtime attribute
- Serialization preserves the `ident { }` form faithfully (round-trip correctness)

### Module-global name

`ident` becomes a module-global name within its document scope. No other statement in the same
file may declare a conflicting top-level name:

```anvl
changelog { version := 1 }
changelog := 2   // ERROR: duplicate module-global name
```

### Import semantics

Importing a file that contains an anonymous block brings the block's fields into scope under
the block's name. Import collision rules (two imported files both exporting `changelog`) follow
the same resolution as ordinary statement name conflicts — TBD in the import subsystem.

### VarRef access

Fields inside an anonymous block are reachable by VarRef:

```anvl
changelog { version := 1.0.0 }
display_version := ${changelog.version}   // valid
```

---

## Parser Decision Tree

At top level, after consuming an identifier token:

```
next token == '{'       → parse as anonymous block (this FR)
next token == ':='      → parse as assignment statement (existing)
next token == ':'       → parse as typed declaration (future)
otherwise               → ANVL_ERR_PARSER_UNEXPECTED_TOKEN
```

Completely unambiguous — no lookahead beyond one token.

---

## Implementation Checklist

| Item | Status |
|------|--------|
| Decide `ANVL_STMT_BLOCK` enum value (after `ANVL_STMT_FUNC`?) | ❌ |
| New error code `ANVL_ERR_ANON_BLOCK_REDECLARATION` | ❌ |
| Parser: recognise `IDENT LBRACE` at top level → `ANVL_STMT_BLOCK` | ❌ |
| Parser: reject `:=` assignment to a previously declared anonymous block name | ❌ |
| `Statement.type()` returns `ANVL_STMT_BLOCK` | ❌ |
| Serializer: emit `ident { }` form (not `ident := { }`) for `ANVL_STMT_BLOCK` | ❌ |
| Import collision semantics for anonymous global names | ❌ |
| `Context.get_statement_by_name` already works — no changes needed | ✅ |
| `Context.get_field_by_name` / field traversal already works — no changes needed | ✅ |
| Tests | ❌ |

---

## Test Cases

### AB01 — basic parse
**Input:**
```anvl
#!aml
changelog {
    version := 1.0.0
    date    := 2026-03-23
}
```
**Assert:** parse succeeds; `Context.statement_count(ctx) == 1`;
`Statement.type(s) == ANVL_STMT_BLOCK`; `Context.field_count(ctx, s) == 2`.

### AB02 — field access by name
**Input:** same as AB01.
**Assert:** `Context.get_field_by_name(ctx, s, "version", 7) != NULL`;
field value span == `"1.0.0"`.

### AB03 — VarRef into anonymous block
**Input:**
```anvl
#!aml
changelog { version := 1.0.0 }
display := ${changelog.version}
```
**Assert:** parse succeeds; `display` statement value resolves to `"1.0.0"`.

### AB04 — redeclaration error
**Input:**
```anvl
#!aml
changelog { version := 1 }
changelog := 2
```
**Assert:** `Context.parse(ctx) == false`;
error code == `ANVL_ERR_ANON_BLOCK_REDECLARATION`.

### AB05 — duplicate block name error
**Input:**
```anvl
#!aml
changelog { version := 1 }
changelog { version := 2 }
```
**Assert:** `Context.parse(ctx) == false`;
error code == `ANVL_ERR_ANON_BLOCK_REDECLARATION`.

### AB06 — serializer round-trip
**Input:** parse AB01 source, then serialize back to string.
**Assert:** output contains `changelog {` (not `changelog := {`).

### AB07 — `get_statement_by_name` finds the block
**Input:** AB01 source.
**Assert:** `Context.get_statement_by_name(ctx, "changelog", 9) != NULL`.

### AB08 — no-shebang (permissive mode) also accepts the syntax
**Input:**
```
changelog {
    version := 1.0.0
}
```
**Assert:** parse succeeds; same assertions as AB01.
