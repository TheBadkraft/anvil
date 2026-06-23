/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * test_resolver.c — TDD: inheritance graph resolver (anvil.resolver)
 * ------------------------------------------------------------------
 * Converted from sigma.test to TestBit.
 * Original test contracts from ResolverTests.cs (Anvil.Net.Api).
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "context.h"
#include "errors.h"
#include "resolver.h"
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

static usize find_stmt(context ctx, const char *name) {
    const char *raw = Source.data(ctx->source);
    for (usize i = 0; i < ctx->stmt_list.count; i++) {
        statement s = ctx->stmt_list.statements[i];
        usize p = s->meta[STMT_META_IDENT_POS], l = s->meta[STMT_META_IDENT_LEN];
        if (l == strlen(name) && memcmp(raw + p, name, l) == 0) return i;
    }
    return (usize)-1;
}

static field find_field(const anvl_field_list_t *fl, context ctx, const char *name) {
    const char *raw = Source.data(ctx->source);
    for (usize i = 0; i < fl->count; i++) {
        field f = fl->fields[i];
        if (f->key_len == strlen(name) && memcmp(raw + f->key_pos, name, f->key_len) == 0)
            return f;
    }
    return NULL;
}

static bool has_field(const anvl_field_list_t *fl, context ctx, const char *name) {
    return find_field(fl, ctx, name) != NULL;
}

static bool field_val_eq(field f, context ctx, const char *expected) {
    if (!f || !f->val) return false;
    const char *raw = Source.data(ctx->source);
    usize pos = f->val->data.scalar.pos, len = f->val->data.scalar.len;
    return len == strlen(expected) && memcmp(raw + pos, expected, len) == 0;
}

/* ------------------------------------------------------------------ */
/* RS01 — no inheritance: fast-path NULL, no error                   */
/* ------------------------------------------------------------------ */
static void test_rs01_no_inheritance(void) {
    const char *src = "#!aml\nx := 1\ny := 2";
    context ctx = parse_src(src);
    TestBit.is_not_null(ctx, "RS01: context");
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_null(st, "RS01: NULL fast-path (no inheritance)");
    TestBit.is_false(Anvil.error_is_set(), "RS01: no error");
    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS02 — single-level: unoverridden base fields inherited           */
/* ------------------------------------------------------------------ */
static void test_rs02_single_inherits(void) {
    const char *src = "#!aml\n"
                      "Base := { a := 1, b := 2 }\n"
                      "Derived : Base := { c := 3 }";
    context ctx = parse_src(src);
    TestBit.is_not_null(ctx, "RS02: context");
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_not_null(st, "RS02: state built");
    usize di = find_stmt(ctx, "Derived");
    const anvl_field_list_t *m = anvl_node_state_get_merged_fields(st, di);
    TestBit.is_not_null(m, "RS02: merged fields");
    TestBit.is_true(has_field(m, ctx, "a"), "RS02: has 'a' from Base");
    TestBit.is_true(has_field(m, ctx, "b"), "RS02: has 'b' from Base");
    TestBit.is_true(has_field(m, ctx, "c"), "RS02: has 'c' from Derived");
    anvl_node_state_dispose(st); Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS03 — derived field overrides base field (derived wins)          */
/* ------------------------------------------------------------------ */
static void test_rs03_override(void) {
    const char *src = "#!aml\n"
                      "Base := { hp := 100 }\n"
                      "Derived : Base := { hp := 200 }";
    context ctx = parse_src(src);
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_not_null(st, "RS03: state");
    usize di = find_stmt(ctx, "Derived");
    const anvl_field_list_t *m = anvl_node_state_get_merged_fields(st, di);
    TestBit.is_not_null(m, "RS03: merged");
    TestBit.is_equal_int(1, (long long)m->count, "RS03: one field after override");
    TestBit.is_true(field_val_eq(m->fields[0], ctx, "200"), "RS03: derived hp=200 wins");
    anvl_node_state_dispose(st); Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS04 — base unchanged by derived                                  */
/* ------------------------------------------------------------------ */
static void test_rs04_base_unchanged(void) {
    const char *src = "#!aml\n"
                      "Base := { hp := 100 }\n"
                      "Derived : Base := { hp := 200 }";
    context ctx = parse_src(src);
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_not_null(st, "RS04: state");
    usize bi = find_stmt(ctx, "Base");
    const anvl_field_list_t *bm = anvl_node_state_get_merged_fields(st, bi);
    TestBit.is_not_null(bm, "RS04: base merged");
    TestBit.is_true(field_val_eq(bm->fields[0], ctx, "100"), "RS04: base hp still 100");
    anvl_node_state_dispose(st); Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS05 — three-level chain: grandchild gets all ancestors           */
/* ------------------------------------------------------------------ */
static void test_rs05_three_level(void) {
    const char *src = "#!aml\n"
                      "A := { x := 1 }\n"
                      "B : A := { y := 2 }\n"
                      "C : B := { z := 3 }";
    context ctx = parse_src(src);
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_not_null(st, "RS05: state");
    usize ci = find_stmt(ctx, "C");
    const anvl_field_list_t *m = anvl_node_state_get_merged_fields(st, ci);
    TestBit.is_not_null(m, "RS05: merged");
    TestBit.is_true(has_field(m, ctx, "x"), "RS05: has 'x' from A");
    TestBit.is_true(has_field(m, ctx, "y"), "RS05: has 'y' from B");
    TestBit.is_true(has_field(m, ctx, "z"), "RS05: has 'z' from C");
    anvl_node_state_dispose(st); Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS06 — forward reference: base declared after derived             */
/* ------------------------------------------------------------------ */
static void test_rs06_forward_ref(void) {
    const char *src = "#!aml\n"
                      "Derived : Base := { c := 3 }\n"
                      "Base := { a := 1, b := 2 }";
    context ctx = parse_src(src);
    TestBit.is_not_null(ctx, "RS06: context");
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_not_null(st, "RS06: state (forward ref resolved)");
    usize di = find_stmt(ctx, "Derived");
    const anvl_field_list_t *m = anvl_node_state_get_merged_fields(st, di);
    TestBit.is_not_null(m, "RS06: merged");
    TestBit.is_true(has_field(m, ctx, "a"), "RS06: has 'a' from Base");
    TestBit.is_true(has_field(m, ctx, "c"), "RS06: has 'c' from Derived");
    anvl_node_state_dispose(st); Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS07 — cycle detected                                             */
/* ------------------------------------------------------------------ */
static void test_rs07_cycle(void) {
    const char *src = "#!aml\n"
                      "A : B := { x := 1 }\n"
                      "B : A := { y := 2 }";
    context ctx = parse_src(src);
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_null(st, "RS07: NULL on cycle");
    TestBit.is_equal_int(ANVL_ERR_RESOLVER_CYCLE_DETECTED,
                         (long long)Anvil.error_get()->code,
                         "RS07: CYCLE_DETECTED error");
    if (ctx) Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS08 — warm_all idempotent                                        */
/* ------------------------------------------------------------------ */
static void test_rs08_warm_all(void) {
    const char *src = "#!aml\n"
                      "Base := { x := 1 }\n"
                      "A : Base := { y := 2 }\n"
                      "B : Base := { z := 3 }";
    context ctx = parse_src(src);
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_not_null(st, "RS08: state");
    TestBit.is_true(anvl_node_state_warm_all(st), "RS08: warm_all pass 1");
    TestBit.is_false(Anvil.error_is_set(), "RS08: no error");
    TestBit.is_true(anvl_node_state_warm_all(st), "RS08: warm_all pass 2 (idempotent)");
    anvl_node_state_dispose(st); Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS09 — missing base: lazy error on get_merged_fields              */
/* ------------------------------------------------------------------ */
static void test_rs09_missing_base(void) {
    const char *src = "#!aml\nDerived : Ghost := { x := 1 }";
    context ctx = parse_src(src);
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    if (st) {
        usize di = find_stmt(ctx, "Derived");
        const anvl_field_list_t *m = anvl_node_state_get_merged_fields(st, di);
        TestBit.is_null(m, "RS09: NULL on missing base");
        TestBit.is_equal_int(ANVL_ERR_RESOLVER_MISSING_BASE,
                             (long long)Anvil.error_get()->code,
                             "RS09: MISSING_BASE error");
        anvl_node_state_dispose(st);
    } else {
        TestBit.is_true(Anvil.error_is_set(), "RS09: error at build_state");
    }
    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS10 — get_own_fields: no base fields included                    */
/* ------------------------------------------------------------------ */
static void test_rs10_own_fields(void) {
    const char *src = "#!aml\n"
                      "Base := { a := 1 }\n"
                      "Derived : Base := { b := 2 }";
    context ctx = parse_src(src);
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_not_null(st, "RS10: state");
    usize di = find_stmt(ctx, "Derived");
    const anvl_field_list_t *own = anvl_node_state_get_own_fields(st, di);
    TestBit.is_not_null(own, "RS10: own fields");
    TestBit.is_equal_int(1, (long long)own->count, "RS10: only own field");
    TestBit.is_true(has_field(own, ctx, "b"),  "RS10: own has 'b'");
    TestBit.is_false(has_field(own, ctx, "a"), "RS10: own excludes 'a' from Base");
    anvl_node_state_dispose(st); Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS11 — get_base_index returns correct index                       */
/* ------------------------------------------------------------------ */
static void test_rs11_base_index(void) {
    const char *src = "#!aml\n"
                      "Base := { x := 1 }\n"
                      "Derived : Base := { y := 2 }";
    context ctx = parse_src(src);
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    TestBit.is_not_null(st, "RS11: state");
    usize bi = find_stmt(ctx, "Base");
    usize di = find_stmt(ctx, "Derived");
    usize base_of_derived = anvl_node_state_get_base_index(st, di);
    TestBit.is_equal_int((long long)bi, (long long)base_of_derived,
                         "RS11: base index of Derived == Base index");
    usize base_of_base = anvl_node_state_get_base_index(st, bi);
    TestBit.is_equal_int((long long)(usize)-1, (long long)base_of_base,
                         "RS11: Base has no base ((usize)-1)");
    anvl_node_state_dispose(st); Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* RS12 — cannot inherit from anonymous block                        */
/* ------------------------------------------------------------------ */
static void test_rs12_no_inherit_from_anon(void) {
    const char *src = "#!aml\n"
                      "Anon { x := 1 }\n"
                      "Derived : Anon := { y := 2 }";
    context ctx = parse_src(src);
    TestBit.is_not_null(ctx, "RS12: context");
    anvl_node_state_t *st = anvl_resolver_build_state(ctx, ctx->source);
    if (st) {
        usize di = find_stmt(ctx, "Derived");
        const anvl_field_list_t *m = anvl_node_state_get_merged_fields(st, di);
        TestBit.is_null(m, "RS12: cannot resolve anon base");
        TestBit.is_true(Anvil.error_is_set(), "RS12: error set");
        anvl_node_state_dispose(st);
    } else {
        TestBit.is_true(Anvil.error_is_set(), "RS12: error at build_state");
    }
    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("RS01_no_inheritance",   NULL, test_rs01_no_inheritance,  td);
    TestBit.run_ex("RS02_single_inherits",  NULL, test_rs02_single_inherits, td);
    TestBit.run_ex("RS03_override",         NULL, test_rs03_override,        td);
    TestBit.run_ex("RS04_base_unchanged",   NULL, test_rs04_base_unchanged,  td);
    TestBit.run_ex("RS05_three_level",      NULL, test_rs05_three_level,     td);
    TestBit.run_ex("RS06_forward_ref",      NULL, test_rs06_forward_ref,     td);
    TestBit.run_ex("RS07_cycle",            NULL, test_rs07_cycle,           td);
    TestBit.run_ex("RS08_warm_all",         NULL, test_rs08_warm_all,        td);
    TestBit.run_ex("RS09_missing_base",     NULL, test_rs09_missing_base,    td);
    TestBit.run_ex("RS10_own_fields",       NULL, test_rs10_own_fields,      td);
    TestBit.run_ex("RS11_base_index",       NULL, test_rs11_base_index,      td);
    TestBit.run_ex("RS12_no_inherit_anon",  NULL, test_rs12_no_inherit_from_anon, td);

    return TestBit.report();
}
