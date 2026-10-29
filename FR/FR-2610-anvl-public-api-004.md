# FR-2610-anvl-public-api-004: read an attribute's value as a value, not only as raw source text

**ID:** FR-2610-anvl-public-api-004
**Type:** Feature Request (native library / public API)
**Owner:** anvl, worked by the ANVL coder agent; binding accessors follow (Bindings agent)
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Bindings agent's FR-2610-anvl-bindings-003 finding
**Requested by:** rivet (q-or importer reads `status="done"`-style attributes)
**Status:** resolved (branch `feat/FR-2610-anvl-public-api-004`, off main; not merged, not pushed)
**Tags:** public-api, attributes, parity, rivet-readiness

---

## Summary

`anvil_attribute_get_value` is the only way to read an attribute's value, and it copies the
**source text uninterpreted**. `@[status="done"]` reads as `"done"`, quotes included, and a quoted
value's escapes stay escaped. A bare value (`type=bug`) and a number read the same either way. So
every consumer that wants the *value* has to unquote and unescape itself. That's grammar
knowledge, and the project's rule (FR-2609-anvl-writer-001's reasoning) is that grammar knowledge
lives in native once, never per binding. All four bindings that expose attributes (py, net, node,
wasm) return the raw text today, consistently. The Bindings agent deliberately didn't add a
per-binding unquote.

## Design

`anvil_attribute_get_value` stays unchanged: it's the raw-span reader, the attribute analogue of
`anvil_value_get_bytes`.

Preferred: **give the attribute's value as an `anvil_value`.**

```c
anvil_value anvil_attribute_get_value_handle(anvil_attribute attr);  // NULL for a flag attribute
```

The whole value API then applies with no new accessors: `anvil_value_get_type` (STRING / BARE /
NUMERIC / BOOL / NULL), `anvil_value_get_text` (escapes resolved, delimiters stripped),
`anvil_value_get_numeric`, and `anvil_value_get_bytes`. Check first whether the parser already
holds attribute values as values internally; `parse_scalar_value` is likely reusable. If they're
only stored as slices, parse the slice once with the existing value-fragment machinery at load,
and record the cost.

Fallback, if a value handle isn't practical: `anvil_attribute_get_value_type(attr)` and
`anvil_attribute_get_text(attr, buf, buflen)` with `get_text` semantics. Record which was chosen
and why.

Name: pick one that reads well next to `anvil_attribute_get_value`, which keeps its meaning.
Append it to the flat header and to the `Attribute` vtable (or whichever vtable carries attribute
accessors) last. The completeness test (VT11) and `check-exports` enforce both.

## Test cases

1. `@[status="done"]` → type STRING, text `done`. `@[note="a\"b"]` → text `a"b`.
2. `@[type=bug]` → BARE, text `bug`. `@[n=42]` → NUMERIC, and `get_numeric` gives 42.
3. A flag attribute `@[active]` → NULL handle.
4. `anvil_attribute_get_value` is unchanged for all of the above (regression: still returns `"done"`).
5. Module and statement attributes both work; the handle lives as long as its document.
6. A NUL inside a quoted attribute value survives (BR-2610-anvl-nul-001 rules).
7. VT11 completeness, `check-exports`, full suite, Valgrind; a changelog Added entry.

## After this lands

A Bindings follow-up adds the decoded accessor to **all four** bindings at once, e.g.
`AnvilAttribute.text` / `.Text`, and a typed form where the binding has one. `value` stays raw in
every binding.

## Resolution

Branch `feat/FR-2610-anvl-public-api-004`, off `main`. Tests ANV48-ANV53 (`test_anvil_native.c`) and the
`Attribute` rows of VT06/VT11 (`test_anvil_vtable.c`), written first and RED (the accessor was a NULL
stub; ANV48 and ANV53 also failed to *load*, which is the escaped-quote bug below). Full unit regression
green with counts unchanged except native 49; `test_release`, `check-exports` (164 symbols) and
`check-reader-only` green; Valgrind-clean on native, vtable, body_aml, body_amp, attribute_grammar,
header, module, document, schema and both roundtrip suites.

**Design chosen: the value handle** `anvil_value anvil_attribute_get_value_handle(attr)` (NULL for a flag
attribute), appended last in `anvil_flat.h` and in `anvil_attribute_i`; `get_value` is untouched. The
fallback in the FR (`get_value_type` + `get_text`) was not needed.

**Did the parser already hold attribute values as values? No.** Both attribute scanners (`parse_attribute_list`
for statements, `header_scan_attributes` for the module) keep only a raw *slice* of `key` and `value`. So
the value is now parsed **once, at load**, through the existing scalar grammar (`parse_scalar_value`), and
stored on the attribute (`anvl_attribute_t::decoded`, an arena-owned value node; nothing new to free).
Cost: one extra scalar parse per valued attribute at load, none for flag attributes; nothing at read time,
and the document stays immutable after load. Statement attributes are decoded as they are parsed; module
attributes at the start of the document's body parse, because the arena that owns value nodes does not
exist during the header scan.

**Decode can never fail a load that scanning accepted.** The trial parse saves and restores the source
position, truncates any error it recorded, and restores the parser's last-error code. The value must be
consumed whole (up to trailing whitespace in the slice); otherwise (`1..5`, `1 2`) the attribute is exposed
as a **BARE value of its trimmed raw text**. Only an allocation failure fails the load.

**A bug found on the way and fixed (the FR's test 1 needs it):** both scanners toggled "in string" on every
`"` regardless of escapes, so `@[note="a\"b"]` left the string open and the scan ran on past the closing
`]`. Both are escape-aware now (`ANV53` pins `,` / `]` / escaped quotes inside strings and whitespace
around `=` and `,`). Any document that previously relied on the broken behavior could not have loaded.

**NUL (BR-2610-anvl-nul-001):** a NUL inside a quoted attribute value survives; `get_text` and `get_bytes`
report the exact length (ANV51, module and statement attributes).

**Source-to-document lookup:** isolated in one helper in `parser.c` (`source_document`), which returns the
source's `owner` back-pointer (BR-2610-anvl-registry-001); this branch is stacked on that one.

**After this lands (Bindings):** one decoded accessor in all four bindings (`value` stays raw); module and
statement attributes both work.
