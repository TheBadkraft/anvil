# AML Guide

AML (declarative modelling) is the default dialect — used when a document
has no shebang, or an explicit `#!aml` first line.

## Statements

Every top-level entry is `identifier := value;`. The trailing `;` is
**required** on every statement — omitting it is a parse error
(`MISSING_STATEMENT_TERMINATOR`).

```anvl
#!aml
name := "Aria";
level := 12;
```

An identifier followed directly by `{` (no `:=`) is an **anonymous object**
statement — equivalent to `name := { ... };` for parsing purposes:

```anvl
#!aml
document {
    title := "ANVL For Dummies";
    author := "The Badkraft";
};
```

**Reserved words:** `vars`, `import`, `using`, `true`, `false`, and `null`
(exact lowercase only) can't be used as an identifier anywhere — a
statement name, an inheritance base, an object field name, an attribute
key, a `$` reference target, or a blob tag. Using one is a parse error
(`RESERVED_IDENTIFIER`).

`vars`/`import`/`using` are also reserved as a *value* — `foo := vars;` is
a parse error too, not a bare string. This build doesn't do anything with
these three words itself; they're reserved so a document written today
doesn't break once parsed by the eventual native/WASM replacement, which
does give them grammar meaning. `true`/`false`/`null` are different: they
already mean something in value position (see [Scalars](AML-Guide.md#scalars) below)
and remain perfectly valid there — only their use as an identifier is
restricted, closing a real ambiguity rather than hedging against the
future.

## Scalars

| Form | Example | Notes |
|---|---|---|
| Bare (unquoted) string | `hello`, `1.0.0`, `192.168.1.1` | Any token that isn't an int/float/bool/null/hex |
| Quoted string | `"hello world"` | Supports `\"`, `\\`, `\n`, `\t`, `\r` |
| Integer | `42`, `-7` | |
| Float | `3.14`, `5e+10`, `5.2e-3` | Requires a `.`, or exponent notation |
| Boolean | `true`, `false` | |
| Null | `null` | |
| Hex color | `#RRGGBB`, `#RGB` | 3-digit expands to 6; `.asInt()` gives the numeric value, `.asString()` gives the canonical uppercase 6-digit form |

A bare `#` that isn't a valid hex color is a parse error
(`BARE_HASH_NOT_HEX`) — it's never treated as a comment.

**Exponent notation requires a mandatory sign.** `5e+10` and `5e-10` are
valid; `5e10` is not — the `+`/`-` right after `e`/`E` is required, not
optional. Anything shaped like an attempted exponent (a numeric mantissa
immediately followed by `e`/`E`) that doesn't match the mandatory-sign form
is a parse error (`MALFORMED_EXPONENT`), never a silent fallback to a bare
string — an author who writes `5e10` clearly meant a number.

## Objects

```anvl
config := {
    host := localhost;
    port := 8080;
};
```

Every field requires its own trailing `;`. Empty objects (`{}`) are a parse
error.

## Arrays and tuples

AML allows arbitrary nesting — objects, arrays, and tuples can all contain
each other:

```anvl
grid   := [[1, 2, 3], [4, 5, 6]];
points := [(0, 64, 0), (10, 64, -5)];
player := (Aria, { health := 100; stamina := 50; }, warrior);
```

Empty arrays are a parse error. Tuples must have **at least two** elements —
empty and single-element tuples are both parse errors.

## Attributes

`@[...]` attaches metadata to a document, a statement, or an object field.
Flag attributes (no value) and `key=value` attributes can mix freely:

```anvl
@[doc-level]
server @[env = production, active] := {
    host @[required] := localhost;
};
```

```js
node.hasAttribute('doc-level'); // true -- module-level attributes are visible on the root itself

const server = node.get('server');
server.hasAttribute('env');    // true
server.attribute('env');       // "production"
server.hasAttribute('active'); // true (flag attribute — value is "")
```

Attribute values are always strings; the parser never interprets them.

## Inheritance

The `:=` is optional for an inheriting statement, exactly as it's optional
for a plain anonymous object — both forms below are equivalent:

```anvl
Base := { a := 1; };
Child : Base := { b := 2; };
```

```anvl
Base := { a := 1; };
Child : Base {
    b := 2;
};
```

```js
const child = node.get('Child');
child.hasBase();        // true
child.baseIdentifier(); // "Base"
```

**Inheritance always requires an object value.** `Child : Base := 42;`,
`Child : Base := [1, 2];`, and a blob are all parse errors
(`INHERITANCE_REQUIRES_OBJECT`) — a scalar, array, tuple, or blob isn't a
mutable container and can't participate in inheritance, regardless of
which of the two forms above you use.

Inheritance is single-inheritance only. **Field merging is not currently
implemented** — `hasBase()`/`baseIdentifier()` expose the relationship, but
accessing an inherited field through `.get()` on the derived node will not
walk up to the base. If your workflow needs merged fields, resolve the base
chain yourself via `baseIdentifier()`.

## VarRefs

`$identifier` references a **top-level** statement's value, resolved once,
statically, after parsing completes:

```anvl
base_var    := 42;
derived_var := $base_var;
```

Rules:

- Only **top-level** statement names are resolvable targets — a field
  inside an object is never a valid `$` target, even if you're referencing
  it from elsewhere in the same object.
- A `$` reference can appear anywhere a value can — nested in an object
  field, an array, or a tuple element — it just can't *point at* anything
  but a top-level name.
- An unmatched reference resolves to `null`, not an error:
  `foo := $missing;` parses fine; `foo`'s value is `null`.
- A circular reference (`a := $b; b := $a;`) also resolves to `null` for
  both names, without hanging.
- `$identifier` is strictly bare — no `$foo.bar`, no `$foo(...)`. Both are
  parse errors (`MALFORMED_VARREF`).
- `$` with no following identifier, or whitespace between `$` and the
  identifier, is a parse error.
- A literal string that needs to start with `$` must be quoted:
  `foo := "$bar";` is the literal string `$bar`, not a reference.

VarRefs are AML-only — `$` is a hard parse error in AMP (see the
[AMP Guide](AMP-Guide.md)).

## Blobs

```anvl
raw    := `literal content`;
tagged := @json`{"a":1}`;
```

Blob content is stored and round-tripped byte-for-byte — no re-escaping, no
whitespace normalization. `.asString()` returns the content (not the tag).

## Comments

```anvl
// single-line, to end of line
/* block comment,
   may span lines */
```

An unterminated block comment is a parse error
(`UNTERMINATED_COMMENT`).
