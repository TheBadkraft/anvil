/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * test_using.c — TDD: using declarations
 * ------------------------------------------------------------------
 * Step 4 parity item: 'using <identifier>' declarations.
 *
 * Reference: Anvil.Net v1.3.0
 *
 * Behaviour:
 *   - using must precede vars and statements (may follow imports)
 *   - Multiple usings allowed
 *   - First using escalates dialect AML → ASL
 *   - using in #!amp → error 4306
 *   - using after vars/statements → error 4307
 *   - Identifier stored as pos/len in context using_list
 *
 * Tests:
 *   US01 — single using, dialect escalates to ASL
 *   US02 — multiple usings all stored
 *   US03 — using + vars + statements: all valid ordering
 *   US04 — using in AMP → error 4306
 *   US05 — using after statements → error 4307
 *   US06 — using after vars → error 4307
 *   US07 — using identifier stored correctly (pos/len)
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
/* US01 — single using escalates dialect to ASL                      */
/* ------------------------------------------------------------------ */
static void test_us01_dialect_escalation(void) {
    const char *src = "#!aml\nusing sys_math\nx := 1";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "US01: context created");
    TestBit.is_false(Anvil.error_is_set(), "US01: no parse error");

    /* After parsing, source dialect should have escalated to ASL */
    TestBit.is_equal_int(ANVL_DIALECT_ASL,
                         (long long)Source.dialect(ctx->source),
                         "US01: dialect escalated to ASL");

    /* Statement still parsed correctly */
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "US01: one statement after using");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* US02 — multiple usings all stored, dialect still ASL              */
/* ------------------------------------------------------------------ */
static void test_us02_multiple_usings(void) {
    const char *src = "#!aml\nusing sys_math\nusing sys_str\nx := 1";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "US02: context created");
    TestBit.is_false(Anvil.error_is_set(), "US02: no parse error");

    TestBit.is_equal_int(ANVL_DIALECT_ASL,
                         (long long)Source.dialect(ctx->source),
                         "US02: dialect is ASL");
    TestBit.is_equal_int(2, (long long)ctx->using_list.count,
                         "US02: two usings stored");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* US03 — using → vars → statements: valid ordering                  */
/* ------------------------------------------------------------------ */
static void test_us03_valid_ordering(void) {
    const char *src = "#!aml\nusing sys_math\nvars { pi := 3.14 }\nx := 1";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "US03: context created");
    TestBit.is_false(Anvil.error_is_set(), "US03: no parse error");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "US03: one statement");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* US04 — using in AMP → error 4306                                  */
/* ------------------------------------------------------------------ */
static void test_us04_using_in_amp(void) {
    const char *src = "#!amp\nusing sys_math\nmsg := 1";
    context ctx = parse_src(src);

    TestBit.is_true(Anvil.error_is_set(), "US04: error is set");
    TestBit.is_equal_int(ANVL_ERR_USING_IN_AMP,
                         (long long)Anvil.error_get()->code,
                         "US04: error is 4306 USING_IN_AMP");

    if (ctx) Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* US05 — using after statements → error 4307                        */
/* ------------------------------------------------------------------ */
static void test_us05_using_after_statements(void) {
    const char *src = "#!aml\nx := 1\nusing sys_math";
    context ctx = parse_src(src);

    TestBit.is_true(Anvil.error_is_set(), "US05: error is set");
    TestBit.is_equal_int(ANVL_ERR_USING_AFTER_STATEMENTS,
                         (long long)Anvil.error_get()->code,
                         "US05: error is 4307 USING_AFTER_STATEMENTS");

    if (ctx) Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* US06 — using after vars → error 4307                              */
/* ------------------------------------------------------------------ */
static void test_us06_using_after_vars(void) {
    const char *src = "#!aml\nvars { pi := 3.14 }\nusing sys_math";
    context ctx = parse_src(src);

    TestBit.is_true(Anvil.error_is_set(), "US06: error is set");
    TestBit.is_equal_int(ANVL_ERR_USING_AFTER_STATEMENTS,
                         (long long)Anvil.error_get()->code,
                         "US06: error is 4307 after vars");

    if (ctx) Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* US07 — using identifier stored with correct pos/len               */
/* ------------------------------------------------------------------ */
static void test_us07_identifier_stored(void) {
    const char *src = "#!aml\nusing sys_io\nx := 1";
    context ctx = parse_src(src);

    TestBit.is_not_null(ctx, "US07: context created");
    TestBit.is_false(Anvil.error_is_set(), "US07: no error");
    TestBit.is_equal_int(1, (long long)ctx->using_list.count,
                         "US07: one using stored");

    struct anvl_using_decl *u = &ctx->using_list.decls[0];
    TestBit.is_true(span_matches(src, u->name_pos, u->name_len, "sys_io"),
                    "US07: identifier is 'sys_io'");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("US01_dialect_escalation",    NULL, test_us01_dialect_escalation,    td);
    TestBit.run_ex("US02_multiple_usings",       NULL, test_us02_multiple_usings,       td);
    TestBit.run_ex("US03_valid_ordering",        NULL, test_us03_valid_ordering,        td);
    TestBit.run_ex("US04_using_in_amp",          NULL, test_us04_using_in_amp,          td);
    TestBit.run_ex("US05_using_after_statements",NULL, test_us05_using_after_statements,td);
    TestBit.run_ex("US06_using_after_vars",      NULL, test_us06_using_after_vars,      td);
    TestBit.run_ex("US07_identifier_stored",     NULL, test_us07_identifier_stored,     td);

    return TestBit.report();
}
