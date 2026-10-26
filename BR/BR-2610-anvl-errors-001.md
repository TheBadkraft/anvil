# BR-2610-anvl-errors-001: parse errors report "Unknown error", and a repeated top-level name reports no position

**ID:** BR-2610-anvl-errors-001
**Type:** Bug Report (diagnostics)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Rivet Project Engineer (Rivet's q-or importer, 156 normalized files)
**Status:** open
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
