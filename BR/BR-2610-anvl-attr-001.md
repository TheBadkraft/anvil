# BR-2610-anvl-attr-001: an attribute value's span includes trailing whitespace, and a newline there breaks decoding

**ID:** BR-2610-anvl-attr-001
**Type:** Bug Report (parser / attributes, wrong value)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Rivet Project Engineer (found by Rivet Coder in FR-2610-rivet-006)
**Status:** resolved (branch `fix/BR-2610-anvl-attr-001`, off main; not merged, not pushed)
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

## Resolution

Branch `fix/BR-2610-anvl-attr-001`, off `main` (`20ea23a`). Tests ANV64 and ANV65 (`test_anvil_native.c`),
written first and RED on every row of the ticket (raw values with a trailing space/tab/newline, decoded BARE
with quotes). Full unit regression green (native 64), `test_release`, `check-exports`, `check-reader-only`,
`check-tsan` green, Valgrind-clean on the 16 suites.

**Fix, in both scanners** (`parse_attribute_list` for statements, `header_scan_attributes` for the module):
the scan still runs to the delimiter to find where the value ends (it has to honor strings, escapes and
blobs), but the recorded span now ends at the last non-whitespace character before it: space, tab, CR and LF
are trimmed off the END. The cursor still stops at the `,` / `]`.

**The decode followed.** `anvl_attribute_decode` (FR-2610-anvl-public-api-004) used to skip trailing
same-line whitespace and compare against the span end; with a trimmed span that skip would have run past the
end into the real text, so it now requires the scalar to be exactly the span. (Its own trim of spaces and
tabs was only ever a patch for this bug and is kept as a defensive no-op.) That is also why a newline used to
break decoding: the old trim handled space and tab but not newlines, so `"2"` + newline didn't parse as a
whole scalar.

**Checked per the ticket:**
- Every row: `@[a="1", b="2"` + LF + `]`, `@[` LF `b="2"` LF `]`, `@[b="2" ]`, `@[b=bug<TAB>]`,
  `@[a="1" , b="2"]` (the one that shows it is not only the last attribute): raw is exactly the token and the
  decoded value is STRING `2` / BARE `bug` / STRING `1`.
- CRLF line endings inside the list, statement and module level.
- Module-level attributes with the same layouts (ANV65).
- Whitespace after `=` (spaces, tab, newline): not affected, because both scanners already skip it before
  recording the value start; pinned.
- Regressions: an escaped quote and a `]` inside a string (ANV53 and again in ANV64), `range=1..5`, numbers,
  an internal space (`"a b"`), `1.5`: all unchanged.
