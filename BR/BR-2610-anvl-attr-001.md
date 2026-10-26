# BR-2610-anvl-attr-001: an attribute value's span includes trailing whitespace, and a newline there breaks decoding

**ID:** BR-2610-anvl-attr-001
**Type:** Bug Report (parser / attributes, wrong value)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Rivet Project Engineer (found by Rivet Coder in FR-2610-rivet-006)
**Status:** open
**Tags:** attributes, parser, whitespace, rivet-readiness

---

## Summary

An attribute's value span runs from the start of the value to the next delimiter (`,` or `]`),
not to the end of the value token. So any whitespace before the delimiter becomes part of the
**raw** value. When that whitespace contains a newline, the decoded value
(`anvil_attribute_get_value_handle`, FR-2610-anvl-public-api-004) falls back to BARE and keeps the
quotes. Reproduced in C against anvil `main` (`a134419`):

| Attribute list | raw `anvil_attribute_get_value` | decoded type / text |
|---|---|---|
| `@[a="1", b="2"\n]` | `a`: `"1"` ok; `b`: `"2"\n` | `b`: BARE `"2"` (quotes kept) |
| `@[\n  b="2"\n]` | `"2"\n` | BARE `"2"` |
| `@[b="2" ]` | `"2" ` | STRING `2` (decodes, but raw has a trailing space) |
| `@[b=bug\t]` | `bug\t` | BARE `bug` |
| `@[a="1" , b="2"]` | `a`: `"1" ` (**not only the last attribute**) | STRING `1` |

So there are two defects, and the first causes the second. Decoding trims spaces and tabs but not
newlines, so a quoted value with a newline after it doesn't parse as a whole scalar and takes the
BARE fallback.

## Impact

Multi-line attribute lists are common in real files (q-or records: `ID @[\n type=…,\n status=…\n] {`).
Rivet's re-import gets quoted values for 5 legacy records, and won't add a workaround. Every binding
reports the raw value with stray whitespace.

## Expected behavior

- The value span ends at the end of the value **token**: after the closing quote of a string, or
  the last character of a bare or numeric token. Whitespace (space, tab, CR, LF) between the value
  and `,`/`]` is skipped, not included. The same goes for whitespace between `=` and the value, if
  that has the same problem.
- Both scanners (statement attributes in `parse_attribute_list`, module attributes in
  `header_scan_attributes`) behave the same.
- Decoding then sees exactly the token, so `"2"` decodes as STRING `2` whatever whitespace follows.

## Test cases

1. Every row above: raw is exactly the token (`"2"`, `bug`, `"1"`) and decoded is STRING `2` /
   BARE `bug` / STRING `1`.
2. CRLF line endings inside the list (`@[a="1",\r\n b="2"\r\n]`).
3. Module-level attributes with the same layouts.
4. An escaped quote and a `]` inside a string value still work (ANV53 regression).
5. A bare value containing internal characters that aren't whitespace is unchanged (`@[range=1..5]`).
6. Full suite, Valgrind; changelog Fixed entry.

## Notes

The bindings report `value` and `decodedValue` straight from native, so no binding change is
needed beyond a re-bump. anvil.node and every other binding pick it up at their next pin.
