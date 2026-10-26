# BR-2610-anvl-errors-001: parse errors report "Unknown error", and a repeated top-level name reports no position

**ID:** BR-2610-anvl-errors-001
**Type:** Bug Report (diagnostics)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Rivet Project Engineer (Rivet's q-or importer, 156 normalized files)
**Status:** resolved (branch `fix/BR-2610-anvl-errors-001`, stacked on parser-001; not merged, not pushed)
**Tags:** errors, diagnostics, parser, resolver, rivet-readiness

---

## Summary

Reproduced in C against `libanvil.so` (`f9ee783`):

| Document body | Category | Position | Message |
|---|---|---|---|
| `x := 1` then `y := 2;` (missing `;`) | 4 | line 2 col 7 | **"Unknown error"** |
| `x := nullable;` (BR-2610-anvl-parser-001) | 4 | line 2 col 10 | **"Unknown error"** |
| `x := 1; ]` (stray token) | 4 | line 2 col 9 | "Expected identifier" (fine) |
| `x := 1;` then `x := 2;` (repeated top-level name) | 5 | **line 0 col 0** | "Duplicate top-level identifier" |

So two defects:

1. **Missing messages.** Some parser paths record an internal code with no entry in
   `src/core/errors.c`'s message table, or record no code at all, and the public detail falls back
   to "Unknown error". The position is right, so the caller knows *where* but not *what*.
2. **The resolver's duplicate-name error has no position.** It's raised after parsing, from the
   symbol table, without the statement's source location.

## Expected behavior

- Every internal error code the parser or resolver can record has a specific message. Add a
  completeness test, the errors analogue of VT11: iterate every `anvl_err_code` value and require a
  non-empty, non-"Unknown error" message. A missing-`;` error reads like "Expected ';' after
  statement" (find the real recording site; it may record the wrong code, not merely an unmapped one).
- The duplicate top-level name error carries the line/column of the **second** occurrence. If
  practical, the message also names the identifier and the first occurrence's line.
- "Unknown error" is reachable only for a genuinely unknown code (defensive), never from a
  documented parse failure.

## Test cases

1. Missing `;` → a specific message, same position as today.
2. The completeness test over the message table (fails today; verify by hand).
3. Repeated top-level name → line/column of the second occurrence, plus the name in the message.
4. A sample of every parser error-recording site gives a specific message (pick one input per
   code that's reachable from the public API).
5. Full suite, Valgrind; changelog Fixed entry.

## Resolution

Branch `fix/BR-2610-anvl-errors-001`, stacked on `fix/BR-2610-anvl-parser-001`. Tests REG10
(`test_registry.c`), ANV58-ANV60 (`test_anvil_native.c`), written first and RED. Full unit regression green
(registry 11, native 59, others unchanged); `test_release`, `check-exports` green; Valgrind-clean on registry,
native, resolver, module, header, document, source, body_aml, body_amp.

**1. Missing messages.** The completeness test (REG10) found them all at once: it reads every
`ANVL_ERR_* = n` from `errors.h` and requires a message other than "Unknown error" and a name other than
`ANVL_ERR_UNKNOWN`. Eight codes had no message (`PARSER_EXPECTED_COLON`, `PARSER_UNTERMINATED_STATEMENT`,
`SERIALIZER_AMP_OBJECT_NOT_ALLOWED`, `VARS_INVALID_VARREF`, `ARENA_NOT_INITIALIZED`, `SOURCE_NOT_FOUND`,
`CONTEXT_INVALID`, `NOT_IMPLEMENTED`) and twelve had no name (those, plus `PARSER_INITIALIZATION_FAILED`,
`EMPTY_TUPLE_NOT_ALLOWED`, `TUPLE_TOO_FEW_ELEMENTS`, `UNTERMINATED_COMMENT`). All added. The missing-`;` case
was not the wrong code: the parser records `UNTERMINATED_STATEMENT` correctly (twice, `parser.c`), it just had
no message. Verified by hand: REG10 fails listing exactly those codes before the fix.

**2. Duplicate top-level name.** The resolver recorded the error with line 0 / column 0 from the symbol
table. It now locates the statement name's pointer in the owning document's buffer (line and column are
1-based from the start of the file, the way the parser reports them) and reports the **second** occurrence,
across includes as well as within one file. The message is built for this error: `Duplicate top-level
identifier 'x' (first declared at line 2)`. To carry a per-error message, `anvl_err_state_t` gained an owned
`detail` string and there are two new internal functions, `anvl_error_set_message` and
`anvl_error_dispose` (every place that freed an error now uses the latter; internal API only, no public
change).

**3. Sample of every reachable parser error site (ANV60):** 16 inputs (missing `;`, stray token,
unterminated string, empty array, one-element tuple, empty tuple, empty object, bad identifier, keyword as
identifier and as value, unterminated header comment, bare `$`, object in AMP, broken attribute list,
unterminated blob, duplicate name) each give an error with a specific message and a non-zero line.

**Found, not fixed (a separate defect):** an unterminated block comment AFTER the first statement is
silently accepted: `x := 1; /* never closed` loads with no error. `ANVL_ERR_PARSER_UNTERMINATED_COMMENT` is
only raised by the header scanner, so ANV60 uses the header form. Worth its own ticket.
