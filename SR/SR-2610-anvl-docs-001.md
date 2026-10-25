# SR-2610-anvl-docs-001: types/schema documentation drift — status claims and stale code comments

**ID:** SR-2610-anvl-docs-001
**Type:** Service Request (documentation maintenance)
**Owner:** anvl
**Filed:** 2026-10-25
**Filed by:** anvil
**Requested by:** rivet (downstream consumer)
**Status:** resolved
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

## Resolution

Branch `docs/SR-2610-anvl-docs-001`, merged to `main`; items landed with the tickets they map to.

1. `docs/types-reference.md` (and its site mirror): the "design-only" status and the "not-yet-started
   schema module" bullet are replaced with the implemented state. Done here.
2. `README.md`: "first phase" removed from the types row and the schema doc pointer; the schema paragraph
   lists what is now implemented. Done here.
3. Done with BR-2610-anvl-types-001 (both `read_numeric` helpers and their comments were removed);
   `grep -rn "no numeric accessor" src docs notes site/assets/docs` finds nothing.
4. `size` counts bytes (with the `"héllo"` example) and closed-schema semantics are documented, and
   pinned by SCH81. Done here.
5. Done with FR-2610-anvl-types-002 (transitive includes).

Test cases 1-4 verified by grep/inspection. Case 5: `site/assets/docs/types-reference.md` is
byte-identical to `docs/types-reference.md`. The site's `README.md` mirror is *generated* by
`site/scripts/build-site.js` from the root README with text substitutions and was already out of step
with it before this change (different title block), so it was not hand-edited; the next site build
regenerates it. Not run here (needs the site toolchain and would rewrite unrelated files).
