# SR-2610-anvl-docs-001: types/schema documentation drift — status claims and stale code comments

**ID:** SR-2610-anvl-docs-001
**Type:** Service Request (documentation maintenance)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** open
**Tags:** docs, types, schema, maintenance, rivet-readiness

---

## Summary

The docs disagree with each other and with the code about AnvilSchema and AnvilTypes. A new
consumer, or a coding agent, reading `docs/types-reference.md` first concludes that schema
doesn't exist.

## Items

1. `docs/types-reference.md` (and its site mirror `site/assets/docs/types-reference.md`): the
   opening status says schema is "design-only — nothing is implemented for it yet", and
   "What's not built yet" lists "Everything about `@[schema]` and schema validation". Both are
   false. `README.md`, `docs/schema/README.md` and `include/anvil_schema.h` all describe it as implemented.
2. `README.md` describes the opt-in type system as "first phase". Reconcile it with the
   types-reference status after item 1.
3. `src/anvil_types.c` and `src/schema/schema.c` both say no numeric accessor exists on
   `anvil_value`. `anvil_value_get_numeric` exists. (BR-2610-anvl-types-001 replaces the code that
   comment justifies, so the comment goes with it. This SR checks that nothing else repeats the claim.)
4. Document constraint semantics that are currently implicit: `size` counts **bytes** of the value's
   text (not characters), and unknown data fields are always violations (closed schemas) unless a
   caller opts out (FR-2610-anvl-schema-002).
5. `docs/types-reference.md` "Using a type from another document": update when
   FR-2610-anvl-types-002 changes direct-only to transitive.

## Test cases

1. `grep -ri "design-only" docs/types-reference.md site/assets/docs/types-reference.md` finds nothing about schema.
2. `grep -rn "no numeric accessor" src/ docs/ notes/` matches nothing outside historical design-record notes.
3. The types-reference and README status lines for types and schema agree with each other.
4. `docs/types-reference.md` states that `size` is a byte length, with a multi-byte UTF-8 example.
5. `site/assets/docs/` mirrors match `docs/` for every file changed here.
