# FR-2610-anvl-public-api-004: read an attribute's value as a value, not only as raw source text

**ID:** FR-2610-anvl-public-api-004
**Type:** Feature Request (native library / public API)
**Owner:** anvl, worked by the ANVL coder agent; binding accessors follow (Bindings agent)
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Bindings agent's FR-2610-anvl-bindings-003 finding
**Requested by:** rivet (q-or importer reads `status="done"`-style attributes)
**Status:** open — not started
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
