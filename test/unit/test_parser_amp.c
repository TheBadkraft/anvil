/*
 * test_parser_amp.c - AMP dialect parser tests (strict scalar enforcement).
 * Converted from sigma.test to TestBit.
 */

#include "anvil.h"
#include "testbit.h"
#include <string.h>

static void td(void) { Anvil.error_clear(); }

/* ------------------------------------------------------------------ */
/* Local helpers                                                      */
/* ------------------------------------------------------------------ */
static context parse_source(const char *source, anvl_dialect dialect) {
    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, dialect);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "context created from source");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "source parsing succeeds");

    return ctx;
}

static context parse_source_with_err(const char *source, anvl_dialect dialect,
                                     const anvl_error_state **err_state) {
    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, dialect);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "context created from source");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "parser should fail for invalid AMP source");
    *err_state = Anvil.error_get();

    return ctx;
}

/* ================================================================== */
/* AM01 — scalar integer array parses successfully in AMP             */
/* ================================================================== */
static void test_am01_scalar_array(void) {
    const char *source = "#!amp\nids := [101, 204, 387]";
    context ctx = parse_source(source, ANVL_DIALECT_AMP);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "AM01: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "AM01: statement exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt),
                         "AM01: statement is assignment");

    Context.dispose(ctx);
}

/* ================================================================== */
/* AM04 — scalar integer tuple parses successfully in AMP             */
/* ================================================================== */
static void test_am04_scalar_tuple(void) {
    const char *source = "#!amp\ncoords := (128, 64, -37)";
    context ctx = parse_source(source, ANVL_DIALECT_AMP);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "AM04: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "AM04: statement exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt),
                         "AM04: statement is assignment");

    Context.dispose(ctx);
}

/* ================================================================== */
/* AM10 — nested array in AMP array → AMP_ARRAY_ELEMENT_NOT_SCALAR   */
/* ================================================================== */
static void test_am10_array_nested_array(void) {
    const char *source = "#!amp\nmatrix := [[1, 2], [3, 4]]";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AMP, &err);

    TestBit.is_not_null((void *)err, "AM10: error state set");
    TestBit.is_equal_int(ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR, (long long)err->code,
                         "AM10: error is AMP_ARRAY_ELEMENT_NOT_SCALAR for nested array in array");

    Context.dispose(ctx);
}

/* ================================================================== */
/* AM11 — nested tuple in AMP array → AMP_ARRAY_ELEMENT_NOT_SCALAR   */
/* ================================================================== */
static void test_am11_array_nested_tuple(void) {
    const char *source = "#!amp\npoints := [(1, 2), (3, 4)]";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AMP, &err);

    TestBit.is_not_null((void *)err, "AM11: error state set");
    TestBit.is_equal_int(ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR, (long long)err->code,
                         "AM11: error is AMP_ARRAY_ELEMENT_NOT_SCALAR for nested tuple in array");

    Context.dispose(ctx);
}

/* ================================================================== */
/* AM12 — nested tuple in AMP tuple → AMP_ARRAY_ELEMENT_NOT_SCALAR   */
/* ================================================================== */
static void test_am12_tuple_nested_tuple(void) {
    const char *source = "#!amp\nnested := ((1, 2), 3)";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AMP, &err);

    TestBit.is_not_null((void *)err, "AM12: error state set");
    TestBit.is_equal_int(ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR, (long long)err->code,
                         "AM12: error is AMP_ARRAY_ELEMENT_NOT_SCALAR for nested tuple in tuple");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("AM01_scalar_array",        NULL, test_am01_scalar_array,        td);
    TestBit.run_ex("AM04_scalar_tuple",        NULL, test_am04_scalar_tuple,        td);
    TestBit.run_ex("AM10_array_nested_array",  NULL, test_am10_array_nested_array,  td);
    TestBit.run_ex("AM11_array_nested_tuple",  NULL, test_am11_array_nested_tuple,  td);
    TestBit.run_ex("AM12_tuple_nested_tuple",  NULL, test_am12_tuple_nested_tuple,  td);

    return TestBit.report();
}
