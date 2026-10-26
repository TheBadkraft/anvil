# BR-2610-anvl-parser-001: a bare value that begins with `null`, `true` or `false` is rejected

**ID:** BR-2610-anvl-parser-001
**Type:** Bug Report (parser, data rejected)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Rivet Project Engineer (Rivet's q-or importer)
**Status:** open
**Tags:** parser, bare-literal, reserved-words, rivet-readiness

---

## Summary

A bare (unquoted) value whose text *begins* with a reserved literal fails to load. Reproduced in C
against `libanvil.so` (`f9ee783`):

| Document body | Result |
|---|---|
| `x := nullable;` | `ANVIL_ERR` category 4, line 2 col 10, message "Unknown error" |
| `x := nulls;` | same, col 10 |
| `x := truely;` | same, col 10 |
| `x := falsey;` | same, col 11 |
| `x := includes;` | loads (BARE) |
| `x := "nullable";` | loads (STRING) |

The error column is exactly the end of `null`/`true`/`false`. So the scalar parser matches the
reserved literal as a **prefix**, accepts it as NULL/BOOL, and then fails expecting `;`. The bare
literal grammar (`notes/document-body-parse.md`, "Bare literal grammar") makes `nullable` an
ordinary bare word. Only the exact tokens `null`/`true`/`false` are literals.

## Impact

q-or's files, and Rivet's own data, use bare words like `nullable` as values. The documents are
valid ANVL, and they fail to load with a message that doesn't say why. Rivet quotes such values
as a workaround and carries this ID as a TODO.

## Expected behavior

`null`, `true` and `false` are recognized only as whole tokens: the next character isn't a
bare-literal continuation character (`source_is_bare_literal_part`). `nullable`, `nulls`, `truely`
and `falsey` load as BARE with their full text. Exact `null`/`true`/`false` are unchanged.

## Test cases

1. The four failing rows above load as BARE, `anvil_value_get_text` gives the full word, and
   `anvil_value_get_type` is BARE.
2. `null`, `true` and `false` are still NULL/BOOL, including directly followed by `;`, `,`, `]`, `)`,
   `}` and whitespace.
3. The same in array elements, tuple elements, object fields, and attribute values (attribute
   values are now decoded through the same scalar grammar, FR-2610-anvl-public-api-004).
4. The writer agrees: `anvil_writer_bare("nullable")` is accepted and round-trips as BARE. Check its
   "would read back as BARE" rule, which may have been encoding the same bug.
5. Full suite, Valgrind; changelog Fixed entry.
