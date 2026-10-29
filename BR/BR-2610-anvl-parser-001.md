# BR-2610-anvl-parser-001: a bare value that begins with `null`, `true` or `false` is rejected

**ID:** BR-2610-anvl-parser-001
**Type:** Bug Report (parser, data rejected)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Rivet Project Engineer (Rivet's q-or importer)
**Status:** resolved (branch `fix/BR-2610-anvl-parser-001`, stacked on the integrated stack; not merged, not pushed)
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

## Resolution

Branch `fix/BR-2610-anvl-parser-001`, stacked on the integrated stack tip (`f0c71eb`). Tests ANV54-ANV57
(`test_anvil_native.c`) and `RT_LITERAL_PREFIXED_BARE_WORDS` (`test_writer_roundtrip.c`), written first and
RED (every prefixed word failed to load). Full unit regression green (native 56, writer_roundtrip 14,
others unchanged), `test_release` and `check-exports` green, Valgrind-clean on native, body_aml, body_amp,
attribute_grammar, writer_roundtrip, value_fragment.

**Cause and fix:** `parse_reserved_literal` used `Source.match_token`, which is `match_length`, a plain
prefix compare. It now also requires that the character after the match is not a bare-literal continuation
(`Source.is_bare_literal_part`), i.e. the literal must be a whole token. Nothing else needed to change:
the bare-literal parser that runs next already reads the full word.

**Checked, as the ticket asked:**
- Whole-token cases: `nullable`, `nulls`, `truely`, `falsey`, `nullx`, `true_`, `false1`, `null-ish`, `true/x`,
  `null.v`, `falsehood` all load as BARE with their full text (ANV54); `"nullable"` (STRING) and `includes`
  (BARE) still load.
- Exact literals are unchanged and still terminate correctly before `;` (with and without a space), `,`, `]`,
  `)`, `}` and whitespace, in statements, arrays, tuples and object fields (ANV55, ANV56). One thing my own
  test assumed wrongly: a newline between `true` and `;` is not valid ANVL (before or after this fix), so the
  case uses `true ;`.
- AMP uses the same scalar grammar and now loads `nullable` too (ANV56).
- **Attribute values** are decoded through the same grammar (FR-2610-anvl-public-api-004): `@[a=nullable]`
  is BARE `nullable`, `@[b=true]` BOOL, `@[c=null]` NULL (ANV57). Before this fix an attribute with a
  prefixed value fell back to a BARE of the raw text via the undecodable-value path, so it did not fail to
  load, but its decode now goes through the proper path.
- **The writer** was never encoding the bug: `wg_check_bare` reserves only the exact keywords (`strcmp`),
  so `nullable` was accepted and wrote `x := nullable;`, which the reader then rejected. The round trip now
  holds, and `null`/`true`/`false` as bare words are still rejected.
