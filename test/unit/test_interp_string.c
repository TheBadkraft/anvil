/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * test_interp_string.c — TDD: interpolated string parsing
 * ------------------------------------------------------------------
 * Step 3 parity item: $"…{ident}…" interpolated strings.
 *
 * Hole syntax: {bareident} — no dot prefix, no dotted paths inside hole.
 * Reference: Anvil.Net rc4 — BR_0326_001 (dot-prefix removed)
 *
 * Tests:
 *   IS01 — simple hole: $"Hello {name}"
 *   IS02 — multiple holes: $"{first} {last}"
 *   IS03 — no holes (pure literal): $"no holes"
 *   IS04 — hole mid-string: $"val={x} end"
 *   IS05 — empty interp string: $""
 *   IS06 — escaped braces: $"{{literal}}"
 *   IS07 — unterminated string → error
 *   IS08 — non-identifier after { → error
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "context.h"
#include "errors.h"
#include "testbit.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */
static context parse_src(const char *src) {
    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);
    if (!ctx) return NULL;
    Context.parse(ctx);
    return ctx;
}

static void td(void) { Anvil.error_clear(); }

static bool span_matches(const char *src, usize pos, usize len, const char *want) {
    return len == strlen(want) && memcmp(src + pos, want, len) == 0;
}

/* ------------------------------------------------------------------ */
/* IS01 — simple hole: $"Hello {name}"                               */
/* ------------------------------------------------------------------ */
static void test_is01_simple_hole(void) {
    const char *src = "#!aml\ngreeting := $\"Hello {name}\"";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "IS01: context created");
    TestBit.is_false(Anvil.error_is_set(), "IS01: no parse error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt->value_meta, "IS01: value_meta exists");
    TestBit.is_equal_int(ANVL_VALUE_INTERP_STRING, (long long)stmt->value_meta->type,
                         "IS01: value is INTERP_STRING");

    usize nseg = stmt->value_meta->data.interp_string.segment_count;
    struct anvl_interp_segment *segs = stmt->value_meta->data.interp_string.segments;
    TestBit.is_equal_int(2, (long long)nseg, "IS01: two segments");

    /* seg[0]: literal "Hello " */
    TestBit.is_false(segs[0].is_ref, "IS01: seg[0] is literal");
    TestBit.is_true(span_matches(src, segs[0].pos, segs[0].len, "Hello "),
                    "IS01: seg[0] is 'Hello '");

    /* seg[1]: ref "name" */
    TestBit.is_true(segs[1].is_ref, "IS01: seg[1] is ref");
    TestBit.is_true(span_matches(src, segs[1].pos, segs[1].len, "name"),
                    "IS01: seg[1] is 'name'");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* IS02 — multiple holes: $"{first} {last}"                          */
/* ------------------------------------------------------------------ */
static void test_is02_multiple_holes(void) {
    const char *src = "#!aml\nfull := $\"{first} {last}\"";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "IS02: context created");
    TestBit.is_false(Anvil.error_is_set(), "IS02: no error");

    statement stmt = Context.get_statement(ctx, 0);
    usize nseg = stmt->value_meta->data.interp_string.segment_count;
    struct anvl_interp_segment *segs = stmt->value_meta->data.interp_string.segments;

    /* {first} literal " " {last} → 3 segments */
    TestBit.is_equal_int(3, (long long)nseg, "IS02: three segments");

    TestBit.is_true(segs[0].is_ref, "IS02: seg[0] is ref 'first'");
    TestBit.is_true(span_matches(src, segs[0].pos, segs[0].len, "first"),
                    "IS02: seg[0] span is 'first'");

    TestBit.is_false(segs[1].is_ref, "IS02: seg[1] is literal ' '");
    TestBit.is_true(span_matches(src, segs[1].pos, segs[1].len, " "),
                    "IS02: seg[1] span is ' '");

    TestBit.is_true(segs[2].is_ref, "IS02: seg[2] is ref 'last'");
    TestBit.is_true(span_matches(src, segs[2].pos, segs[2].len, "last"),
                    "IS02: seg[2] span is 'last'");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* IS03 — no holes: $"no holes"                                       */
/* ------------------------------------------------------------------ */
static void test_is03_no_holes(void) {
    const char *src = "#!aml\nmsg := $\"no holes\"";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "IS03: context created");
    TestBit.is_false(Anvil.error_is_set(), "IS03: no error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_equal_int(ANVL_VALUE_INTERP_STRING, (long long)stmt->value_meta->type,
                         "IS03: value is INTERP_STRING");

    usize nseg = stmt->value_meta->data.interp_string.segment_count;
    TestBit.is_equal_int(1, (long long)nseg, "IS03: one segment");
    TestBit.is_false(stmt->value_meta->data.interp_string.segments[0].is_ref,
                     "IS03: segment is literal");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* IS04 — hole mid-string: $"val={x} end"                            */
/* ------------------------------------------------------------------ */
static void test_is04_hole_mid_string(void) {
    const char *src = "#!aml\nout := $\"val={x} end\"";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "IS04: context created");
    TestBit.is_false(Anvil.error_is_set(), "IS04: no error");

    statement stmt = Context.get_statement(ctx, 0);
    usize nseg = stmt->value_meta->data.interp_string.segment_count;
    struct anvl_interp_segment *segs = stmt->value_meta->data.interp_string.segments;

    /* "val=" {x} " end" → 3 segments */
    TestBit.is_equal_int(3, (long long)nseg, "IS04: three segments");
    TestBit.is_false(segs[0].is_ref, "IS04: seg[0] literal");
    TestBit.is_true(segs[1].is_ref,  "IS04: seg[1] ref");
    TestBit.is_false(segs[2].is_ref, "IS04: seg[2] literal");
    TestBit.is_true(span_matches(src, segs[1].pos, segs[1].len, "x"),
                    "IS04: ref is 'x'");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* IS05 — empty interp string: $""                                   */
/* ------------------------------------------------------------------ */
static void test_is05_empty(void) {
    const char *src = "#!aml\nempty := $\"\"";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "IS05: context created");
    TestBit.is_false(Anvil.error_is_set(), "IS05: no error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_equal_int(ANVL_VALUE_INTERP_STRING, (long long)stmt->value_meta->type,
                         "IS05: type is INTERP_STRING");
    TestBit.is_equal_int(0, (long long)stmt->value_meta->data.interp_string.segment_count,
                         "IS05: zero segments");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* IS06 — escaped braces: $"{{literal}}"                             */
/* ------------------------------------------------------------------ */
static void test_is06_escaped_braces(void) {
    const char *src = "#!aml\nbrace := $\"{{literal}}\"";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "IS06: context created");
    TestBit.is_false(Anvil.error_is_set(), "IS06: no error");

    statement stmt = Context.get_statement(ctx, 0);
    /* {{  → literal '{',  literal "literal", }} → literal '}' → 3 segments */
    usize nseg = stmt->value_meta->data.interp_string.segment_count;
    TestBit.is_true(nseg >= 1, "IS06: at least one segment");

    /* All segments must be literals (no refs) */
    struct anvl_interp_segment *segs = stmt->value_meta->data.interp_string.segments;
    bool all_literal = true;
    for (usize i = 0; i < nseg; i++)
        if (segs[i].is_ref) { all_literal = false; break; }
    TestBit.is_true(all_literal, "IS06: all segments are literals (no refs)");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* IS07 — unterminated string → error                                */
/* ------------------------------------------------------------------ */
static void test_is07_unterminated(void) {
    const char *src = "#!aml\nbad := $\"no closing quote";
    context ctx = parse_src(src);

    TestBit.is_true(Anvil.error_is_set(), "IS07: error is set");

    if (ctx) Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* IS08 — non-identifier start after { → error                       */
/* ------------------------------------------------------------------ */
static void test_is08_invalid_hole(void) {
    const char *src = "#!aml\nbad := $\"hello {123}\"";
    context ctx = parse_src(src);

    TestBit.is_true(Anvil.error_is_set(), "IS08: error is set for invalid hole");

    if (ctx) Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("IS01_simple_hole",       NULL, test_is01_simple_hole,       td);
    TestBit.run_ex("IS02_multiple_holes",    NULL, test_is02_multiple_holes,    td);
    TestBit.run_ex("IS03_no_holes",          NULL, test_is03_no_holes,          td);
    TestBit.run_ex("IS04_hole_mid_string",   NULL, test_is04_hole_mid_string,   td);
    TestBit.run_ex("IS05_empty",             NULL, test_is05_empty,             td);
    TestBit.run_ex("IS06_escaped_braces",    NULL, test_is06_escaped_braces,    td);
    TestBit.run_ex("IS07_unterminated",      NULL, test_is07_unterminated,      td);
    TestBit.run_ex("IS08_invalid_hole",      NULL, test_is08_invalid_hole,      td);

    return TestBit.report();
}
