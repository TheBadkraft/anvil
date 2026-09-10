# AMP Guide

AMP (restricted messaging) is a deliberately narrow dialect for message
payloads. Use it with an explicit `#!amp` shebang as the document's first
line.

## Allowed

- All scalar forms (see [AML Guide: Scalars](AML-Guide.md#scalars) — the
  scalar grammar is identical in both dialects)
- Arrays of scalars
- Tuples of scalars (minimum two elements)
- Direct assignment: `identifier := value;`
- Tagged and untagged blobs

```anvl
#!amp
status := ok;
code   := 200;
tags   := [alpha, beta, gamma];
point  := (10, 20);
raw    := `payload bytes`;
```

**Reserved words apply here too.** `vars`, `import`, `using`, `true`, `false`,
and `null` can't be used as a statement name or a blob tag — the only two
identifier positions AMP has. See [AML Guide: Statements](AML-Guide.md#statements)
for the full list of reserved-word positions and the additional
value-position restriction on `vars`/`import`/`using`.

## Forbidden — hard parse errors, not silently ignored

AMP isn't AML-minus-features by convention; each of these is actively
rejected the instant the parser sees it, with a dedicated error code:

| Construct | Error code |
|---|---|
| Objects (`{ ... }`) anywhere in value position | `AMP_OBJECT_FORBIDDEN` |
| `$` VarRef, in any position | `AMP_VARREF_FORBIDDEN` |
| Attributes (`@[...]`), module-level or per-statement | `AMP_ATTRIBUTE_FORBIDDEN` |
| Inheritance (`Child : Base := ...`) | `AMP_INHERITANCE_FORBIDDEN` |

```anvl
#!amp
bad := { x := 1; };   // AMP_OBJECT_FORBIDDEN
bad := $foo;          // AMP_VARREF_FORBIDDEN
```

## Scalar-only collections — no nesting, ever

An AML array or tuple can contain anything, including other arrays and
objects. AMP's cannot — every element must itself be a scalar (or a blob).

```anvl
#!amp
bad := [[1, 2]];          // AMP_ARRAY_ELEMENT_NOT_SCALAR
bad := [(1, 2)];          // AMP_ARRAY_ELEMENT_NOT_SCALAR
bad := (1, { x := 2; });  // AMP_OBJECT_FORBIDDEN (object takes precedence over the generic "not scalar" code)
```

## Why the distinction matters

The dialect is declared once, on the shebang line, and enforced immediately from that point on
— not inferred, not mixed, and not a runtime flag layered on top of one generic grammar. A
feature added to AML — say, a new collection type — can't accidentally become reachable from
AMP just because it shares a code path. This separation is enforced deliberately, not just by
convention.

See the [Bindings guide](Bindings-Guide.md) for how errors surface through each binding's own
API (`lastError()`'s `{ message, line, column }` shape).
