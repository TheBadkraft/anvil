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

**Reserved words apply here too.** `vars`, `include`, `import`, `true`, `false`,
and `null` can't be used as a statement name or a blob tag — the only two
identifier positions AMP has. See [AML Guide: Statements](AML-Guide.md#statements)
for the full list of reserved-word positions and the additional
value-position restriction on `vars`/`include`/`import`.

## Forbidden — hard parse errors, not silently ignored

AMP isn't AML-minus-features by convention; each of these is actively
rejected the instant the parser sees it. AMP has no grammar for them, so the parser
stops at the first token it can't place and reports
`ANVL_ERR_PARSER_UNEXPECTED_TOKEN` (3100); there is no dedicated code per construct.

| Construct | Rejected with |
|---|---|
| Objects (`{ ... }`) in value position | 3100 `ANVL_ERR_PARSER_UNEXPECTED_TOKEN` |
| `$` VarRef | 3100 `ANVL_ERR_PARSER_UNEXPECTED_TOKEN` |
| Attributes (`@[...]`), module-level or per-statement | 3100 `ANVL_ERR_PARSER_UNEXPECTED_TOKEN` |
| Inheritance (`Child : Base := ...`) | 3100 `ANVL_ERR_PARSER_UNEXPECTED_TOKEN` |

Each of these documents is rejected with 3100 (the parser stops at the first error, so each is its
own document):

<!-- example: error 3100 -->
```anvl
#!amp
bad := { x := 1; };   // an object
```

<!-- example: error 3100 -->
```anvl
#!amp
bad := $foo;          // a VarRef
```

<!-- example: error 3100 -->
```anvl
#!amp
@[doc_level]          // an attribute
ok := 1;
```

<!-- example: error 3100 -->
```anvl
#!amp
Child : Base := 1;    // inheritance
```

## Scalar-only collections — no nesting, ever

An AML array or tuple can contain anything, including other arrays and
objects. AMP's cannot — every element must itself be a scalar (or a blob). A
collection element that is not is `ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR` (4401),
including an object inside a tuple or an array:

<!-- example: error 4401 -->
```anvl
#!amp
bad := [[1, 2]];
```

<!-- example: error 4401 -->
```anvl
#!amp
bad := [(1, 2)];
```

<!-- example: error 4401 -->
```anvl
#!amp
bad := (1, { x := 2; });
```

## Why the distinction matters

The dialect is declared once, on the shebang line, and enforced immediately from that point on
— not inferred, not mixed, and not a runtime flag layered on top of one generic grammar. A
feature added to AML — say, a new collection type — can't accidentally become reachable from
AMP just because it shares a code path. This separation is enforced deliberately, not just by
convention.

See the [Bindings guide](Bindings-Guide.md) for how errors surface through each binding's own
API (`lastError()`'s `{ message, line, column }` shape).
