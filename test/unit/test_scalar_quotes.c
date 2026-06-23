/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * test_scalar_quotes.c — TDD: parse-time quote stripping
 * ------------------------------------------------------------------
 * Parity ref: Anvil.Net v1.5.0
 *   ParseStatement(): scalar values use stripped position/length
 *   ParseCollection(): array/tuple element positions exclude quotes
 *   Blob positions are raw (unchanged)
 *
 * Tests:
 *   QS01 — quoted string: value_meta pos/len exclude surrounding quotes
 *   QS02 — unquoted scalar: pos/len unchanged (regression guard)
 *   QS03 — empty quoted string: pos past opening quote, len == 0
 *   QS04 — attribute value: attr value_pos/value_len exclude quotes
 *   QS05 — blob: positions are raw, not stripped
 *   QS06 — array elements: element pos/len exclude quotes
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "context.h"
#include "types.h"
#include "testbit.h"

#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                           */
/* ------------------------------------------------------------------ */

static context parse(const char *src) {
    anvl_ctx_builder_i *builder = Context.get_builder();
    builder->set_dialect(builder, ANVL_DIALECT_AML);
    builder->set_source(builder, src, strlen(src));
    context ctx = builder->build(builder);
    if (!ctx) return NULL;
    if (!Context.parse(ctx)) {
        Context.dispose(ctx);
        return NULL;
    }
    return ctx;
}

static void teardown_state(void) {
    Anvil.cleanup();
    Anvil.error_clear();
}

/* ------------------------------------------------------------------ */
/* QS01 — quoted string scalar pos/len exclude surrounding quotes    */
/* ------------------------------------------------------------------ */
/*
 * source: `name := "hello"`
 *          0123456789...
 *  n=0 a=1 m=2 e=3 ' '=4 :=5 ==6 ' '=7 "=8 h=9 e=10 l=11 l=12 o=13 "=14
 *
 * After fix: value_meta->pos == 9 (points to 'h'), value_meta->len == 5
 * Current (broken): pos == 8 (includes '"'), len == 7 ('"hello"')
 */
static void test_qs01_quoted_string(void) {
    const char *src = "name := \"hello\"";
    context ctx = parse(src);

    TestBit.is_not_null(ctx, "QS01: context created");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "QS01: statement exists");

    struct anvl_value_meta *vm = stmt->value_meta;
    TestBit.is_not_null(vm, "QS01: value_meta exists");

    /* Content of "hello" starts at index 9, length 5 */
    TestBit.is_equal_int(9,  (long long)vm->pos, "QS01: pos excludes opening quote");
    TestBit.is_equal_int(5,  (long long)vm->len, "QS01: len excludes both quotes");

    /* Verify the span actually points to the content */
    const char *data = Source.data(ctx->source);
    TestBit.is_equal_int('h', (long long)data[vm->pos],     "QS01: first char is 'h'");
    TestBit.is_equal_int('o', (long long)data[vm->pos + 4], "QS01: last char is 'o'");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* QS02 — unquoted scalar: pos/len must not change (regression)     */
/* ------------------------------------------------------------------ */
/*
 * source: `age := 42`
 *          012345678
 *  a=0 g=1 e=2 ' '=3 :=4 ==5 ' '=6 4=7 2=8
 *
 * Expected: pos == 7, len == 2 (same before and after fix)
 */
static void test_qs02_unquoted_scalar(void) {
    const char *src = "age := 42";
    context ctx = parse(src);

    TestBit.is_not_null(ctx, "QS02: context created");

    statement stmt = Context.get_statement(ctx, 0);
    struct anvl_value_meta *vm = stmt->value_meta;

    TestBit.is_equal_int(7, (long long)vm->pos, "QS02: pos at start of number");
    TestBit.is_equal_int(2, (long long)vm->len, "QS02: len is 2 for '42'");

    const char *data = Source.data(ctx->source);
    TestBit.is_equal_int('4', (long long)data[vm->pos],     "QS02: first char is '4'");
    TestBit.is_equal_int('2', (long long)data[vm->pos + 1], "QS02: second char is '2'");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* QS03 — empty quoted string: pos past opening quote, len == 0     */
/* ------------------------------------------------------------------ */
/*
 * source: `empty := ""`
 *          0123456789 10
 *  e=0..y=4 ' '=5 :=6 ==7 ' '=8 "=9 "=10
 *
 * After fix: pos == 10, len == 0
 */
static void test_qs03_empty_string(void) {
    const char *src = "empty := \"\"";
    context ctx = parse(src);

    TestBit.is_not_null(ctx, "QS03: context created");

    statement stmt = Context.get_statement(ctx, 0);
    struct anvl_value_meta *vm = stmt->value_meta;

    TestBit.is_equal_int(0, (long long)vm->len, "QS03: len is 0 for empty string");
    /* pos should be 10: one past the opening quote at index 9 */
    TestBit.is_equal_int(10, (long long)vm->pos, "QS03: pos is one past opening quote");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* QS04 — attribute value: value_pos/value_len exclude quotes        */
/* ------------------------------------------------------------------ */
/*
 * source: `@[mod="anvil"] tag := true`
 *          0123456789...
 *  @=0 [=1 m=2 o=3 d=4 ==5 "=6 a=7 n=8 v=9 i=10 l=11 "=12 ]=13
 *
 * Module-level attribute value "anvil": value_pos should be 7, len 5
 */
static void test_qs04_attribute_value(void) {
    const char *src = "@[mod=\"anvil\"] tag := true";
    context ctx = parse(src);

    TestBit.is_not_null(ctx, "QS04: context created");
    TestBit.is_true(ctx->attr_list.count >= 1, "QS04: at least one attribute");

    attribute attr = ctx->attr_list.attributes[0];
    TestBit.is_not_null(attr, "QS04: attribute exists");

    /* "anvil" starts at index 7, length 5 */
    TestBit.is_equal_int(7, (long long)attr->value_pos, "QS04: value_pos excludes opening quote");
    TestBit.is_equal_int(5, (long long)attr->value_len, "QS04: value_len excludes both quotes");

    const char *data = Source.data(ctx->source);
    TestBit.is_equal_int('a', (long long)data[attr->value_pos],     "QS04: first char is 'a'");
    TestBit.is_equal_int('l', (long long)data[attr->value_pos + 4], "QS04: last char is 'l'");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* QS05 — blob: positions are raw content, not stripped              */
/* ------------------------------------------------------------------ */
/*
 * source: `data := @md` + "`raw`"
 * Blob positions point to the content inside backticks.
 * The parser stores the content span, not the backtick delimiters.
 * This test confirms blobs are not affected by the scalar quote fix.
 */
static void test_qs05_blob_unchanged(void) {
    /* Blobs use backticks, not quotes — quote stripping must not affect them.
     * By design, AML blob element_meta has pos=0,len=0; content span lives in
     * value_meta->pos/len covering the full blob expression (@md`raw`).
     * We verify: value_meta->pos points to '@', not a '"' character. */
    const char *src = "data := @md`raw`";
    context ctx = parse(src);

    TestBit.is_not_null(ctx, "QS05: context created");

    statement stmt = Context.get_statement(ctx, 0);
    struct anvl_value_meta *vm = stmt->value_meta;

    TestBit.is_not_null(vm, "QS05: value_meta exists");
    TestBit.is_true(vm->type == ANVL_VALUE_ARRAY || vm->type == ANVL_VALUE_BLOB,
                    "QS05: blob stored as ARRAY or BLOB");

    /* value_meta covers the whole blob expression — first char must be '@' */
    const char *data = Source.data(ctx->source);
    TestBit.is_equal_int('@', (long long)data[vm->pos],
                         "QS05: value_meta->pos points to '@' (not a quote)");
    TestBit.is_true(vm->len > 0, "QS05: value_meta->len > 0");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* QS06 — array elements: quoted element pos/len exclude quotes      */
/* ------------------------------------------------------------------ */
/*
 * source: `names := ["alice", "bob"]`
 *          0         1         2
 *          0123456789012345678901234
 *  names(0..4) ' '(5) :(6) =(7) ' '(8) [(9) "(10) a(11)..e(15) "(16) ,(17)
 *  ' '(18) "(19) b(20) o(21) b(22) "(23) ](24)
 *
 * Element 0: pos==11, len==5 ("alice" content)
 * Element 1: pos==20, len==3 ("bob" content)
 */
static void test_qs06_array_elements(void) {
    const char *src = "names := [\"alice\", \"bob\"]";
    context ctx = parse(src);

    TestBit.is_not_null(ctx, "QS06: context created");

    statement stmt = Context.get_statement(ctx, 0);
    struct anvl_value_meta *vm = stmt->value_meta;

    TestBit.is_not_null(vm, "QS06: value_meta exists");
    TestBit.is_equal_int(ANVL_VALUE_ARRAY, (long long)vm->type, "QS06: value is array");
    TestBit.is_equal_int(2, (long long)vm->data.collection.element_count, "QS06: two elements");

    const char *data = Source.data(ctx->source);

    struct anvl_element_meta *e0 = &vm->data.collection.elements[0];
    TestBit.is_equal_int(5,   (long long)e0->len, "QS06: element 0 len is 5");
    TestBit.is_equal_int('a', (long long)data[e0->pos], "QS06: element 0 starts with 'a'");

    struct anvl_element_meta *e1 = &vm->data.collection.elements[1];
    TestBit.is_equal_int(3,   (long long)e1->len, "QS06: element 1 len is 3");
    TestBit.is_equal_int('b', (long long)data[e1->pos], "QS06: element 1 starts with 'b'");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                       */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("QS01_quoted_string",    NULL, test_qs01_quoted_string, teardown_state);
    TestBit.run_ex("QS02_unquoted_scalar",  NULL, test_qs02_unquoted_scalar, teardown_state);
    TestBit.run_ex("QS03_empty_string",     NULL, test_qs03_empty_string, teardown_state);
    TestBit.run_ex("QS04_attribute_value",  NULL, test_qs04_attribute_value, teardown_state);
    TestBit.run_ex("QS05_blob_unchanged",   NULL, test_qs05_blob_unchanged, teardown_state);
    TestBit.run_ex("QS06_array_elements",   NULL, test_qs06_array_elements, teardown_state);

    return TestBit.report();
}
