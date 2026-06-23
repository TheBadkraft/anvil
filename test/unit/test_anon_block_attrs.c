/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * test_anon_block_attrs.c — TDD: anonymous block optional @[attrs]
 * ------------------------------------------------------------------
 * Step 5 parity item: anonymous block syntax extended to support
 * optional @[...] attributes between the identifier and {.
 *
 * Reference: Anvil.Net v1.3.0 — AB07-AB09
 *
 * Grammar:
 *   identifier @[attr, key=val, ...] { fields }    ← new
 *   identifier { fields }                           ← existing (backward compat)
 *
 * Tests:
 *   AB01 — plain anon block still works (regression)
 *   AB02 — anon block with single attribute
 *   AB03 — anon block with key=value attribute
 *   AB04 — anon block with multiple attributes
 *   AB05 — attribute stored on stmt->attr_meta
 *   AB06 — backward compat: stmt->attr_meta NULL when no attrs
 *   AB07 — anon block attrs followed by fields
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
/* AB01 — plain anon block: regression guard                         */
/* ------------------------------------------------------------------ */
static void test_ab01_plain_anon_block(void) {
    const char *src = "#!aml\ndefaults { hp := 100 }";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "AB01: context created");
    TestBit.is_false(Anvil.error_is_set(), "AB01: no error");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "AB01: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_equal_int(ANVL_ANON_OBJECT, (long long)stmt->meta[STMT_META_TYPE],
                         "AB01: type is ANON_OBJECT");
    TestBit.is_null(stmt->attr_meta, "AB01: no attrs (attr_meta is NULL)");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AB02 — anon block with single flag attribute                      */
/* ------------------------------------------------------------------ */
static void test_ab02_single_attr(void) {
    const char *src = "#!aml\ndefaults @[immutable] { hp := 100 }";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "AB02: context created");
    TestBit.is_false(Anvil.error_is_set(), "AB02: no error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_equal_int(ANVL_ANON_OBJECT, (long long)stmt->meta[STMT_META_TYPE],
                         "AB02: type is ANON_OBJECT");
    TestBit.is_not_null(stmt->attr_meta, "AB02: attr_meta present");
    TestBit.is_equal_int(1, (long long)stmt->meta[STMT_META_ATTR_IDX],
                         "AB02: one attribute");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AB03 — anon block with key=value attribute                        */
/* ------------------------------------------------------------------ */
static void test_ab03_key_value_attr(void) {
    const char *src = "#!aml\ndefaults @[tier=1] { hp := 100 }";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "AB03: context created");
    TestBit.is_false(Anvil.error_is_set(), "AB03: no error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt->attr_meta, "AB03: attr_meta present");
    TestBit.is_equal_int(1, (long long)stmt->meta[STMT_META_ATTR_IDX],
                         "AB03: one attribute");

    /* Key span should be 'tier' */
    TestBit.is_true(span_matches(src, stmt->attr_meta[0].pos,
                                 stmt->attr_meta[0].len, "tier"),
                    "AB03: attr key is 'tier'");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AB04 — anon block with multiple attributes                        */
/* ------------------------------------------------------------------ */
static void test_ab04_multiple_attrs(void) {
    const char *src = "#!aml\nbase @[tier=1, core] { x := 0 }";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "AB04: context created");
    TestBit.is_false(Anvil.error_is_set(), "AB04: no error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt->attr_meta, "AB04: attr_meta present");
    TestBit.is_equal_int(2, (long long)stmt->meta[STMT_META_ATTR_IDX],
                         "AB04: two attributes");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AB05 — attribute value stored correctly on attr_meta              */
/* ------------------------------------------------------------------ */
static void test_ab05_attr_value_stored(void) {
    const char *src = "#!aml\nblock @[tag=\"v1\"] { x := 1 }";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "AB05: context created");
    TestBit.is_false(Anvil.error_is_set(), "AB05: no error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt->attr_meta, "AB05: attr_meta present");

    /* Value should be quote-stripped: 'v1' */
    TestBit.is_true(span_matches(src, stmt->attr_meta[0].value_pos,
                                 stmt->attr_meta[0].value_len, "v1"),
                    "AB05: attr value is 'v1' (quotes stripped)");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AB06 — no attrs: attr_meta NULL, STMT_META_ATTR_IDX == 0          */
/* ------------------------------------------------------------------ */
static void test_ab06_no_attrs_null(void) {
    const char *src = "#!aml\nblock { x := 1 }";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "AB06: context created");
    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_null(stmt->attr_meta, "AB06: attr_meta is NULL");
    TestBit.is_equal_int(0, (long long)stmt->meta[STMT_META_ATTR_IDX],
                         "AB06: ATTR_IDX == 0");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AB07 — attrs followed by fields parses correctly                  */
/* ------------------------------------------------------------------ */
static void test_ab07_attrs_and_fields(void) {
    const char *src = "#!aml\nconfig @[env=\"prod\"] { port := 8080, debug := false }";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "AB07: context created");
    TestBit.is_false(Anvil.error_is_set(), "AB07: no error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_equal_int(ANVL_ANON_OBJECT, (long long)stmt->meta[STMT_META_TYPE],
                         "AB07: type is ANON_OBJECT");
    TestBit.is_not_null(stmt->attr_meta, "AB07: has attrs");
    TestBit.is_equal_int(2, (long long)stmt->value_meta->data.object.field_count,
                         "AB07: two fields");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("AB01_plain_anon_block",  NULL, test_ab01_plain_anon_block,  td);
    TestBit.run_ex("AB02_single_attr",       NULL, test_ab02_single_attr,       td);
    TestBit.run_ex("AB03_key_value_attr",    NULL, test_ab03_key_value_attr,    td);
    TestBit.run_ex("AB04_multiple_attrs",    NULL, test_ab04_multiple_attrs,    td);
    TestBit.run_ex("AB05_attr_value_stored", NULL, test_ab05_attr_value_stored, td);
    TestBit.run_ex("AB06_no_attrs_null",     NULL, test_ab06_no_attrs_null,     td);
    TestBit.run_ex("AB07_attrs_and_fields",  NULL, test_ab07_attrs_and_fields,  td);

    return TestBit.report();
}
