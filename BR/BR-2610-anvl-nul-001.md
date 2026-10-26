# BR-2610-anvl-nul-001: the type registry and schema truncate or drop strings containing a NUL

**ID:** BR-2610-anvl-nul-001
**Type:** Bug Report (silent data loss, native)
**Owner:** anvl, worked by the ANVL coder agent
**Filed:** 2026-10-26
**Filed by:** anvil (planning), from the Bindings agent's cross-binding NUL audit (BR-2610-anvl-bindings-002)
**Status:** open
**Tags:** types, schema, nul, silent-data-loss, rivet-readiness

---

## Summary

The core reader is correct. A document whose string and blob values contain NUL bytes loads intact
through `anvil_load_buffer` with an explicit length (verified from C: string `61 00 62`, blob
`78 00 79`, later statements present). The loss happens where the **type registry and schema copy
text into their own storage as C strings**. Each read since BR-2610-anvl-schema-001 queries the full
length (`include/internal/text_alloc.h`), but the stored copy is then treated as NUL-terminated.

| Accessor | Input | Observed |
|---|---|---|
| `anvil_type_def_get_value` | enum label `"a\0b"` in a `@[types]` doc | 1 byte (`61`): truncated at the NUL |
| `anvil_schema_field_get_value` | schema enum label `"a\0b"` | the same truncation |
| `anvil_schema_field_get_extra_value` | extension value `note := "a\0b";` | a **NULL-typed** value, length 0, so the value is lost entirely (`"ab"` works) |

Found by the Bindings agent; it did not work around any of these. anvil.node, anvil.wasm and
anvil.net carry todo/skipped tests that flip to passing when this lands.

## Likely further impact (check, then pin)

- **`values` membership comparison.** If stored labels are C strings, a data value `"a\0b"` is
  likely compared as `"a"`. That would be a false accept against a label `"a"`, or a false reject
  against a label `"a\0b"`. Pin it either way.
- **`format` and `tag`.** FR-2610-anvl-types-004 already defines a NUL inside a `format`-checked
  payload as invalid. Confirm the stored *format name* and *tag* are length-carried (or that a NUL
  in them is a `MALFORMED_CONSTRAINT` diagnostic), not truncated.
- **Diagnostics and violation messages** that quote a value should also carry their length.
- **Extras.** The FR-2610-anvl-schema-005 resolution re-wraps a string extra "from its exact bytes"
  through `anvil_parse_value_fragment`. Apparently a NUL in those bytes is lost or misread in that
  re-wrap, so check how the fragment text is built (a C string passed where a length is needed).

## Expected behavior

- Every stored copy is a (pointer, length) pair. The public getters return the full length per the
  existing snprintf-style convention, so a caller sizing from the returned length gets every byte,
  NULs included.
- Comparisons (`values`, `tag`, type names) compare by length (`memcmp` after a length check), never `strcmp`.
- An identifier-like position (type name, rule key, format name, tag) that contains a NUL is either
  carried exactly or reported as malformed. Choose per position and record the choice.

## Repro

From C, link against `libanvil.so`:

```c
const char doc[] = "#!aml\n@[schema]\nx := { type := String; note := \"a\0b\"; };\n";
anvil_document d = anvil_load_buffer(doc, sizeof doc - 1);
anvil_schema   s = anvil_schema_load(d);
anvil_value    v = anvil_schema_field_get_extra_value(anvil_schema_find_field(s, "x"), 0);
// today: anvil_value_get_type(v) == ANVIL_VALUE_NULL; expected: STRING, text 61 00 62
```

The Bindings agent's full probes (`nul.c`–`nul4.c`, covering the type-def enum, the schema enum and
extras) are in its session scratchpad at
`/tmp/claude-1000/-home-badkraft-Projects-repos-anvil/936e4d4d-04cb-43f9-906a-876f089e8b8e/scratchpad/nulprobe/`.
That's temporary storage, so turn them into unit tests rather than relying on the files.

## Test cases

1. `anvil_type_def_get_value` returns 3 bytes `61 00 62` for the label `"a\0b"`.
2. `anvil_schema_field_get_value` returns the same, inline and inherited from a type.
3. `anvil_schema_field_get_extra_value` returns a STRING with text `61 00 62`.
4. `values := ["a"]` rejects data `"a\0b"`, and `values := ["a\0b"]` accepts it and rejects `"a"`.
5. The decisions for identifier-like positions are pinned (exact, or a diagnostic).
6. Full unit suite green; Valgrind-clean on types and schema; `test_release`/`check-exports` green.
7. Changelog entry under `## [Unreleased]` → `### Fixed`.
