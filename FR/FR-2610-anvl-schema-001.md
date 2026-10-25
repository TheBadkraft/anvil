# FR-2610-anvl-schema-001: nested validation — Object field rules, Array element rules, and field paths in violations

**ID:** FR-2610-anvl-schema-001
**Type:** Feature Request (native library)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** resolved
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

## Resolution

Branch `feat/FR-2610-anvl-schema-001`, commit `4477bc1`, merged to `main`. Tests SCH41–SCH50 (`test_schema.c`) and TYP27
(`test_types.c`); fixtures `schema_nested_*`, `schema_selfref_*`. Full unit regression green with
unchanged counts elsewhere (SCH01–SCH40 are the regression for top-level behavior and path strings),
`test_release` green, Valgrind-clean on schema, types, anvil_native, anvil_vtable, document, resolver.

Design:
- Schema fields are now a tree (`children` + `items`); one recursive `build_rule` reads a rule and
  one `validate_members` / `check_value` pair checks it, so the later per-rule tickets (nullable,
  integer, format, ...) slot into `check_value` once.
- **A type's `fields` is stored as source text** in the registry (internal accessor
  `anvil_type_def_fields_text`) and re-parsed by the schema with `anvil_parse_value_fragment`. This
  keeps the registry independent of the rule vocabulary and of document lifetimes (the registry
  contract is that it outlives its documents). Cost: one small re-parse per referencing field.
- Self-reference is caught with an expansion stack of type defs, so mutual recursion (A -> B -> A)
  is caught too, not only direct.
- `required`, `skip_required` and `allow_unknown` apply at every depth. An array element rule is
  checked with the same `check_value` as a declared field (it just has no `required`).
- `fields` on a rule that isn't `type := Object` (or `items` on a non-array) still applies when the
  data value is an object/array; the kind check is separate.

Findings:
- The AML grammar rejects an empty array literal (`labels := [];` is a parse error), so the
  "too few items" case is exercised with one element against `min_items := 2`.
- A type's `items` / `min_items` / `max_items` are not inherited (the ticket only specifies `fields`
  for types). Tuples are not given `items` rules.
- Vtable/binding exposure of the new category and any introspection of nested rules: FR-2610-anvl-schema-005
  and FR-2610-anvl-public-api-003.
