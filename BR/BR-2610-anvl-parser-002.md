# BR-2610-anvl-parser-002: an unterminated block comment after the first statement is silently accepted

**ID:** BR-2610-anvl-parser-002
**Type:** Bug Report (parser, silent data loss)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the coder's BR-2610-anvl-errors-001 finding
**Status:** resolved (branch `fix/BR-2610-anvl-parser-002`, on the chain tip; not merged, not pushed)
**Tags:** parser, comments, diagnostics, data-loss, rivet-readiness

---

## Summary

`ANVL_ERR_PARSER_UNTERMINATED_COMMENT` is raised only by the header scanner (`header_skip_ws_comments`).
In the body, `Source.skip_whitespace_and_comments` consumes a `/*` that never closes right through to the
end of the source and reports nothing:

| Document body | Result |
|---|---|
| `x := 1; /* never closed` | loads, **no error** |
| `x := 1; /* never closed` then `y := 2;` on the next line | loads, `y` **silently lost** |
| inside an object, array, tuple or attribute list | a misleading error at the END of the file ("Expected '}' to close object", "Expected ']' ...", "Invalid identifier format"), not at the comment |

A typo like a missing `*/` therefore drops everything after it with no diagnostic, or blames the
wrong place.

## Expected behavior

- An unterminated block comment is an error wherever it opens (after any statement, between
  statements, inside an object, array, tuple or attribute list, in AML and AMP): code
  `ANVL_ERR_PARSER_UNTERMINATED_COMMENT`, its specific message ("Unterminated block comment"), at the
  line and column where the comment OPENS (`/*`), reported by the body parser and not only the header scan.
- A comment that closes is never an error, including one that closes as the last bytes of the file.
  (The header scanner's check was "at EOF and not looking at `*/`", which is also true right after a
  comment that closed at EOF; check it.)

## Also investigate (raised in the coder's BR-2610-anvl-parser-001 report)

A newline between a finished value and its `;` (`x := true` newline `;`) is a parse error. Check
`docs/language-reference.md` and `notes/document-body-parse.md`. If AML is documented as
newline-insensitive there, it is a bug: fix it and pin it for every scalar kind. If it is deliberate,
cite the rule in the Resolution.

## Test cases

1. `x := 1; /* never closed` (EOF), the same before a later statement, inside an object, an array, a
   tuple, an attribute list, on its own line, and in AMP: each is an error with the specific message at
   the comment's opening line/col.
2. Closed comments still load: ending exactly at EOF, a document that is only a comment, between
   statements, inside an array / object / attribute list, a multi-line one; a `//` comment at EOF.
3. The newline-before-`;` question is answered and pinned for every value kind.
4. Full suite, Valgrind; changelog Fixed entry.

## Resolution

Branch `fix/BR-2610-anvl-parser-002`, on `62ff5fd`. Tests ANV61-ANV63 (`test_anvil_native.c`), written first;
ANV61 (and the closed-at-EOF cases of ANV62) failed before the fix, ANV63 pins existing behavior. Full unit
regression green (native 62), `test_release`, `check-exports`, `check-reader-only`, `check-tsan` green,
Valgrind-clean (see the report for the list).

**Fix.** `Source.skip_whitespace_and_comments` is the one place every body-parser skip goes through. When a
`/*` reaches the end of the source without closing, it now records `ANVL_ERR_PARSER_UNTERMINATED_COMMENT`
on the source's document at the position where the comment OPENED (line and column captured before the
`/*` is consumed). `parse_source` fails the parse when an error was recorded only by skipping (the case
where every statement before it parsed cleanly). First-error-wins keeps this the reported error even when
the parser then runs into the end of the source inside an object or array. Sources with no owner (a
standalone source) behave as before. The attribute-value trial parse rolls back any error it records, so
it is unaffected.

**Also fixed here:** the header scanner decided "unterminated" with "at EOF and not looking at `*/`",
which is also true right after a comment that closed as the last bytes of the file, so a document that was
only a closed comment (`#!aml` + `/* c */`) was rejected. It now tracks whether the comment closed.

**Newline before `;`: deliberate, not a bug.** `notes/document-body-parse.md`, "Resolved questions" #1:
"The terminator must stay on the same line as the value it closes. `parse_value` ... allows only same-line
whitespace (space/tab) between a finished value and its `;`; a newline or a comment in that gap is a parse
error (`ANVL_ERR_PARSER_UNTERMINATED_STATEMENT`) ... deliberate, not an oversight". The stated reason:
skipping comments in that gap would reopen the leading-`/`-versus-comment ambiguity already settled for
UNC-style bare literals. Between statements and inside object bodies, comments and blank lines are
unrestricted. `docs/language-reference.md` was silent on this, so a paragraph now states the rule (and
the unterminated-comment error), in section 3. ANV63 pins it for numeric, float, string, bare, bool, null,
blob, array, tuple and object values: `value ;` and `value<TAB>;` load, `value<NL>;` fails with the "Expected
';'" message; a comment after the `;` is fine, a comment before it is an error.
