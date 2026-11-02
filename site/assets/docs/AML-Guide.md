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
statement. It parses like `name := { ... };` and reads back the same, but it is
read-only and cannot be inherited from (see Inheritance below):

```anvl
#!aml
document {
    title := "ANVL For Dummies";
    author := "The Badkraft";
};
```

**Reserved words:** `vars`, `include`, `import`, `true`, `false`, and `null`
(exact lowercase only) can't be used as an identifier anywhere — a
statement name, an inheritance base, an object field name, an attribute
key, a `$` reference target, or a blob tag. Using one is a parse error
(`RESERVED_IDENTIFIER`).

`vars`/`include`/`import` are also reserved as a *value* — `foo := vars;` is
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

The `:=` form and the block form both parse, and both read back as the same object. They are
not interchangeable: the block form `name { ... };` is an anonymous, **read-only** block, and it
can never be a base. Only the `:=` form can be inherited from.

```anvl
Base := { a := 1; };
Child : Base := { b := 2; };
Grand : Child := { c := 3; };
```

This parses. With the block form for `Child`, the same document is rejected where `Grand` tries
to inherit from it:

```anvl
Base := { a := 1; };
Child : Base { b := 2; };
Grand : Child := { c := 3; };
```

```
Cannot inherit from anonymous object (use ':=' for inheritable declarations)
```

That is `ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS` (4052). The error is reported at the statement
that inherits, not at the statement that chose the block form, so choose the block form only for
an object nothing will inherit from. A block can still have a base (`Child : Base { ... };`); it
just can't be one.

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

A statement has at most one base, but the base can itself have one (`Grand : Child`, `Child : Base`).
**Fields are merged when the document is resolved.** The derived object holds its own fields and every
field it inherits, through the whole chain, and a field it defines itself wins over the same name in a
base (the nearest definition wins). Reading the merged object with the parsed example above:

```js
const grand = doc.get('Grand');
grand.count;        // 3
grand.get('c');     // 3  (its own)
grand.get('b');     // 2  (from Child)
grand.get('a');     // 1  (from Base, two levels up)
```

An override replaces the inherited field rather than adding a second one. With
`Base := { a := 1; }; Child : Base := { a := 9; b := 2; };`, `Child` has two fields, `a` is `9` and `b` is
`2`; `Grand : Child := { c := 3; }` then has three: `c`, then `a` (`9`) and `b` (`2`). Own fields come first
in iteration order, followed by the inherited ones. The base itself is unchanged. Anvil Native and anvil.js
(v0.13.1-rc) give the same result.

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
