/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * test_amp_collections.c — TDD: AMP scalar arrays/tuples
 * ------------------------------------------------------------------
 * Step 2 parity item: #!amp dialect must allow scalar-only arrays
 * and tuples; nesting must be rejected with ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR (4401).
 *
 * Reference: Anvil.Net v0.4.3-alpha
 *
 * Tests:
 *   AM01 — #!amp scalar integer array parses cleanly
 *   AM02 — #!amp scalar tuple parses cleanly
 *   AM03 — #!amp mixed scalars (int, bool, string) in array
 *   AM04 — #!amp nested array rejected (error 4401)
 *   AM05 — #!amp nested tuple rejected (error 4401)
 *   AM06 — #!amp object in array rejected (unexpected token)
 *   AM07 — AML arrays are unaffected (regression guard)
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
static context parse_ok(const char *src) {
    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);
    if (!ctx) return NULL;
    Context.parse(ctx);
    return ctx;
}

static void td(void) { Anvil.error_clear(); }

/* ------------------------------------------------------------------ */
/* AM01 — scalar integer array in AMP                                */
/* ------------------------------------------------------------------ */
static void test_am01_amp_int_array(void) {
    const char *src = "#!amp\nmsg := [1, 2, 3]";
    context ctx = parse_ok(src);

    TestBit.is_not_null(ctx, "AM01: context created");
    TestBit.is_false(Anvil.error_is_set(), "AM01: no parse error");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "AM01: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt->value_meta, "AM01: value_meta exists");
    TestBit.is_equal_int(ANVL_VALUE_ARRAY, (long long)stmt->value_meta->type,
                         "AM01: value is ARRAY");
    TestBit.is_equal_int(3, (long long)stmt->value_meta->data.collection.element_count,
                         "AM01: three elements");

    struct anvl_element_meta *elems = stmt->value_meta->data.collection.elements;
    TestBit.is_equal_int(ANVL_VALUE_SCALAR, (long long)elems[0].type, "AM01: elem[0] is SCALAR");
    TestBit.is_equal_int(ANVL_VALUE_SCALAR, (long long)elems[1].type, "AM01: elem[1] is SCALAR");
    TestBit.is_equal_int(ANVL_VALUE_SCALAR, (long long)elems[2].type, "AM01: elem[2] is SCALAR");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AM02 — scalar tuple in AMP                                        */
/* ------------------------------------------------------------------ */
static void test_am02_amp_scalar_tuple(void) {
    const char *src = "#!amp\ncoord := (10, 20, 30)";
    context ctx = parse_ok(src);

    TestBit.is_not_null(ctx, "AM02: context created");
    TestBit.is_false(Anvil.error_is_set(), "AM02: no parse error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt->value_meta, "AM02: value_meta exists");
    TestBit.is_equal_int(ANVL_VALUE_TUPLE, (long long)stmt->value_meta->type,
                         "AM02: value is TUPLE");
    TestBit.is_equal_int(3, (long long)stmt->value_meta->data.collection.element_count,
                         "AM02: three elements");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AM03 — mixed scalar types (int, bool, quoted string) in AMP array */
/* ------------------------------------------------------------------ */
static void test_am03_amp_mixed_scalars(void) {
    const char *src = "#!amp\npkt := [42, true, \"hello\"]";
    context ctx = parse_ok(src);

    TestBit.is_not_null(ctx, "AM03: context created");
    TestBit.is_false(Anvil.error_is_set(), "AM03: no parse error");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_equal_int(3, (long long)stmt->value_meta->data.collection.element_count,
                         "AM03: three elements");

    /* Quoted string element must also be quote-stripped (Step 1 regression) */
    struct anvl_element_meta *e2 = &stmt->value_meta->data.collection.elements[2];
    const char *data = Source.data(ctx->source);
    TestBit.is_equal_int('h', (long long)data[e2->pos],
                         "AM03: string elem starts with 'h' (quotes stripped)");
    TestBit.is_equal_int(5, (long long)e2->len,
                         "AM03: string elem len is 5");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AM04 — nested array in AMP → rejected (error 4401)               */
/* ------------------------------------------------------------------ */
static void test_am04_amp_nested_array_rejected(void) {
    const char *src = "#!amp\nbad := [[1, 2]]";
    context ctx = parse_ok(src);

    TestBit.is_true(Anvil.error_is_set(), "AM04: error is set");
    TestBit.is_equal_int(ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR,
                         (long long)Anvil.error_get()->code,
                         "AM04: error is 4401 AMP_ARRAY_ELEMENT_NOT_SCALAR");

    if (ctx) Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AM05 — nested tuple in AMP → rejected (error 4401)               */
/* ------------------------------------------------------------------ */
static void test_am05_amp_nested_tuple_rejected(void) {
    const char *src = "#!amp\nbad := [(1, 2)]";
    context ctx = parse_ok(src);

    TestBit.is_true(Anvil.error_is_set(), "AM05: error is set");
    TestBit.is_equal_int(ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR,
                         (long long)Anvil.error_get()->code,
                         "AM05: error is 4401 AMP_ARRAY_ELEMENT_NOT_SCALAR");

    if (ctx) Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AM06 — object value in AMP → rejected                             */
/* ------------------------------------------------------------------ */
static void test_am06_amp_object_rejected(void) {
    const char *src = "#!amp\nbad := {key := 1}";
    context ctx = parse_ok(src);

    TestBit.is_true(Anvil.error_is_set(), "AM06: error is set for object in AMP");

    if (ctx) Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* AM07 — AML arrays with nesting work fine (regression guard)       */
/* ------------------------------------------------------------------ */
static void test_am07_aml_nested_array_ok(void) {
    const char *src = "#!aml\nmatrix := [[1, 2], [3, 4]]";
    context ctx = parse_ok(src);

    TestBit.is_not_null(ctx, "AM07: context created");
    TestBit.is_false(Anvil.error_is_set(), "AM07: no error in AML");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx), "AM07: one statement");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("AM01_amp_int_array",            NULL, test_am01_amp_int_array,             td);
    TestBit.run_ex("AM02_amp_scalar_tuple",         NULL, test_am02_amp_scalar_tuple,          td);
    TestBit.run_ex("AM03_amp_mixed_scalars",        NULL, test_am03_amp_mixed_scalars,         td);
    TestBit.run_ex("AM04_amp_nested_array_rejected",NULL, test_am04_amp_nested_array_rejected, td);
    TestBit.run_ex("AM05_amp_nested_tuple_rejected",NULL, test_am05_amp_nested_tuple_rejected, td);
    TestBit.run_ex("AM06_amp_object_rejected",      NULL, test_am06_amp_object_rejected,       td);
    TestBit.run_ex("AM07_aml_nested_array_ok",      NULL, test_am07_aml_nested_array_ok,       td);

    return TestBit.report();
}
