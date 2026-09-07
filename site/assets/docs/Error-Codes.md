# Error Codes

Every `AnvilError` (from `lastError()`) carries a numeric `code` from
`ErrorCode` (`src/errors.js`), plus `message`, `line`, `column`, and `file`.

## Parser errors (4001–4099) — AML + AMP

| Code | Name | Example trigger |
|---|---|---|
| 4001 | `EXPECTED_IDENTIFIER` | A statement, field, base, or attribute name is missing where one is required |
| 4002 | `EXPECTED_ASSIGN` | `foo bar;` — no `:=` or `{` after an identifier |
| 4003 | `EXPECTED_VALUE` | An attribute's `=` isn't followed by a value |
| 4004 | `UNEXPECTED_TOKEN` | A collection isn't closed where expected (e.g. missing `]`, `)`, `}`) |
| 4005 | `EXPECTED_SCALAR` | A value position has a character that starts nothing valid |
| 4006 | `UNTERMINATED_STRING` | `foo := "never closes;` |
| 4007 | `UNTERMINATED_COMMENT` | `/* never ends` with no closing `*/` |
| 4008 | `BARE_HASH_NOT_HEX` | `foo := #nothex;` or `foo := #GGGGGG;` — `#` that isn't valid hex is never a comment |
| 4009 | `MALFORMED_VARREF` | `foo := $;`, `foo := $ bar;`, or `foo := $bar.baz;` |
| 4010 | `MISSING_STATEMENT_TERMINATOR` | `foo := 1` with no trailing `;` |
| 4011 | `EMPTY_OBJECT_NOT_ALLOWED` | `foo := {};` |
| 4012 | `EMPTY_ARRAY_NOT_ALLOWED` | `foo := [];` |
| 4013 | `EMPTY_TUPLE_NOT_ALLOWED` | `foo := ();` |
| 4014 | `TUPLE_TOO_FEW_ELEMENTS` | `foo := (1);` — tuples need at least two elements |
| 4015 | `INHERITANCE_REQUIRES_OBJECT` | `Child : Base := 42;` — inheritance requires an object value; scalars, arrays, tuples, and blobs can't be inherited |
| 4016 | `MALFORMED_EXPONENT` | `foo := 5e10;` (missing sign) or `foo := 5e+;` (missing digit after sign) — the `+`/`-` right after `e`/`E` is mandatory |
| 4017 | `RESERVED_IDENTIFIER` | `vars := 1;`, `true := 1;`, etc. — `vars`/`import`/`using`/`true`/`false`/`null` are reserved everywhere an identifier is read (statement name, field name, inheritance base, attribute key, `$` target, blob tag). `vars`/`import`/`using` are additionally reserved as a *value* — `foo := vars;` also fails this way, at any nesting depth; `true`/`false`/`null` remain legitimate values |

## AMP dialect errors (4401–4405) — AMP only

| Code | Name | Example trigger |
|---|---|---|
| 4401 | `AMP_ARRAY_ELEMENT_NOT_SCALAR` | `foo := [[1, 2]];` or `foo := [(1, 2)];` |
| 4402 | `AMP_VARREF_FORBIDDEN` | `foo := $bar;` under `#!amp` |
| 4403 | `AMP_OBJECT_FORBIDDEN` | `foo := { x := 1; };` under `#!amp` — also raised for an object found inside an AMP array/tuple, taking precedence over 4401 |
| 4404 | `AMP_ATTRIBUTE_FORBIDDEN` | `@[doc]` (module-level) or `foo @[active] := 1;` (statement-level) under `#!amp` |
| 4405 | `AMP_INHERITANCE_FORBIDDEN` | `Child : Base := 1;` under `#!amp` |

## I/O errors (100x)

| Code | Name | Meaning |
|---|---|---|
| 1002 | `FILE_NOT_FOUND` | `load(path)` couldn't read the file |
| 1003 | `INVALID_PATH` | `load()` called outside Node.js (no `node:fs/promises`) |

## General

| Code | Name | Meaning |
|---|---|---|
| 0 | `NONE` | No error |
| 9999 | `NOT_IMPLEMENTED` | Reserved for unimplemented paths — should not occur in normal use |

## Looking up a name from a code

```js
import { ErrorNames } from './src/errors.js';
ErrorNames[4009]; // "MALFORMED_VARREF"
```
