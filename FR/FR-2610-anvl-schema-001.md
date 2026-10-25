# FR-2610-anvl-schema-001: nested validation — Object field rules, Array element rules, and field paths in violations

**ID:** FR-2610-anvl-schema-001
**Type:** Feature Request (native library)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** open — not started
**Tags:** schema, nested, arrays, objects, violations, rivet-readiness

---

## Summary

`anvil_schema_validate` checks only a data document's top-level statements: required/kind/size/
range/values per top-level field, plus unknown top-level fields. For `type := Object` it confirms
only that the value *is* an object. It can't describe or check the object's own fields. For
`type := Array` it can't say what the elements must be. Violations carry a flat field name with
no path.

## Motivation

A ticket is not flat: `labels` is an array of enum labels, `assignee` is an object, and a create
request may carry `links := [ { kind := blocks; target := 42; }, ... ]`. Today all of that passes
unchecked. FlyWire's request bodies are themselves nested (`set := { ... }`, `where := { and := [...] }`).

## Design

No new grammar: rules are ordinary nested objects, exactly like top-level rules.

- **Object rules:** a field rule with `type := Object` may carry `fields := { <name> := { <rule> }; ... };`.
  The nested rules use the identical vocabulary (`type`, `required`, `size`, `min`, `max`,
  `values`, plus later additions) and apply recursively, unknown-field checking included.
- **Array rules:** a field rule with `type := Array` may carry `items := { <rule> };`, applied to
  every element, plus optional `min_items` / `max_items`.
- **Types:** `type := types.X` where X is an Object type may carry `fields` in its definition, so
  a shape is reusable. (This extends `@[types]` definitions from flat to "flat or with `fields`".
  Types still never inherit from one another.)
- **Field paths:** violations report a dotted/indexed path, e.g. `links[2].target`.
  `anvil_schema_violation_get_field` returns the full path; top-level fields read exactly as
  today (no leading dot), so existing callers see no change.
- **Depth:** recursion is bounded by the data's own depth (no cycles are possible in a parsed
  value tree). A schema-side `fields` that references its own type is a load diagnostic
  (FR-2610-anvl-schema-004).
- New category `ANVIL_SCHEMA_ERR_VALIDATION_ITEM_COUNT` for `min_items`/`max_items` (appended).

## Test cases

1. Rule `assignee := { type := Object; fields := { id := { type := Numeric; required := true; }; }; };`
   with data `assignee := { name := bob; };` → two violations: `assignee.id` REQUIRED, `assignee.name` UNKNOWN_FIELD.
2. Rule `labels := { type := Array; items := { type := String; values := [bug, feature]; }; };`
   with data `[bug, nope, feature]` → one VALUES violation at `labels[1]`.
3. `items := { type := Object; fields := {...}; }` validates each element, and paths are `links[0].kind` etc.
4. `max_items := 2` with 3 elements → one ITEM_COUNT violation at `labels`.
5. An Object type in `@[types]` with `fields` is reusable: two schema fields referencing it both
   validate their nested contents.
6. All existing top-level-only tests pass unchanged, including violation field strings.
7. Violation order is deterministic: schema order, depth-first.
8. Valgrind-clean; full suite green.

## Dependencies

None hard. Nested `fields` inherits the BR-2610-anvl-types-001 and BR-2610-anvl-schema-001 fixes.
