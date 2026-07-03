/*
 * test_parser_samples.c - Sample file and stress tests.
 * Converted from sigma.test to TestBit.
 *
 * Requires fixture files under test/fixtures/:
 *   arrays.anvl, assignments.anvl, attributes.anvl, inherits.anvl,
 *   modpack.anvl, objects.anvl, tuples.anvl, generic.aurora,
 *   vars.anvl, schema.asch
 *
 * get_source_path() is from utilities/helpers.h; it resolves a filename
 * relative to the test/fixtures/ directory.
 *
 * The deep-nesting stress test uses a locally generated source string.
 * The real-world-data stress test is skipped — no network in TestBit.
 */

#include "anvil.h"
#include "../utilities/helpers.h"
#include "testbit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void td(void) { Anvil.error_clear(); }

/* ================================================================== */
/* SF01–SF10: Sample file tests                                       */
/* ================================================================== */

static void test_sf01_arrays_sample(void) {
    context ctx = parse_fixture_file_ok("arrays.anvl", ANVL_DIALECT_AML, 43, 5, 1);

    TestBit.is_true(Context.statement_count(ctx) > 10,
                    "SF01: arrays.anvl has many statements");

    Context.dispose(ctx);
}

static void test_sf02_assignments_sample(void) {
    context ctx = parse_fixture_file_ok("assignments.anvl", ANVL_DIALECT_AML, 35, 4, 1);

    TestBit.is_equal_int(15, (long long)Context.statement_count(ctx),
                         "SF02: assignments.anvl has 15 statements");

    Context.dispose(ctx);
}

static void test_sf03_attributes_sample(void) {
    context ctx = parse_fixture_file_ok("attributes.anvl", ANVL_DIALECT_AML, 83, 7, 1);

    TestBit.is_true(Context.statement_count(ctx) > 0,
                    "SF03: attributes.anvl has statements");

    Context.dispose(ctx);
}

static void test_sf04_inherits_sample(void) {
    context ctx = parse_fixture_file_ok("inherits.anvl", ANVL_DIALECT_AML, 58, 5, 1);

    TestBit.is_true(Context.statement_count(ctx) > 0,
                    "SF04: inherits.anvl has statements");

    Context.dispose(ctx);
}

static void test_sf05_modpack_sample(void) {
    context ctx = parse_fixture_file_ok("modpack.anvl", ANVL_DIALECT_AML, 6, 2, 1);

    TestBit.is_true(Context.statement_count(ctx) > 0,
                    "SF05: modpack.anvl has statements");

    Context.dispose(ctx);
}

static void test_sf06_objects_sample(void) {
    context ctx = parse_fixture_file_ok("objects.anvl", ANVL_DIALECT_AML, 7, 3, 1);

    TestBit.is_true(Context.statement_count(ctx) > 1,
                    "SF06: objects.anvl has statements");

    usize stmt_count = Context.statement_count(ctx);
    bool has_object = false;
    for (usize i = 0; i < stmt_count; i++) {
        statement stmt = Context.get_statement(ctx, i);
        if (stmt && stmt->value_meta) {
            has_object = true;
            break;
        }
    }
    TestBit.is_true(has_object, "SF06: objects.anvl contains value metadata");

    Context.dispose(ctx);
}

static void test_sf07_tuples_sample(void) {
    context ctx = parse_fixture_file_ok("tuples.anvl", ANVL_DIALECT_AML, 24, 4, 1);

    TestBit.is_true(Context.statement_count(ctx) > 3,
                    "SF07: tuples.anvl has several statements");

    Context.dispose(ctx);
}

static void test_sf08_generic_aurora(void) {
    context ctx = parse_fixture_file_ok("generic.aurora", ANVL_DIALECT_ASL, 18, 2, 1);

    TestBit.is_equal_int(0, (long long)Context.statement_count(ctx),
                         "SF08: generic.aurora (comment-only) has 0 statements");

    Context.dispose(ctx);
}

static void test_sf09_vars_sample(void) {
    context ctx = parse_fixture_file_ok("vars.anvl", ANVL_DIALECT_AML, 7, 3, 1);

    TestBit.is_equal_int(3, (long long)Context.statement_count(ctx),
                         "SF09: vars.anvl has 3 statements (label, desc, mod)");

    Context.dispose(ctx);
}

static void test_sf10_schema_asch(void) {
    context ctx = parse_fixture_file_ok("schema.asch", ANVL_DIALECT_AML, 6, 2, 1);

    TestBit.is_equal_int(4, (long long)Context.statement_count(ctx),
                         "SF10: schema.asch has 4 type definitions");

    Context.dispose(ctx);
}

/* ================================================================== */
/* SF11: Deep-nesting stress test                                     */
/* ================================================================== */

static void test_sf11_deep_nesting(void) {
    char *source = generate_deep_nested_structure();
    TestBit.is_not_null(source, "SF11: stress source generated");

    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, ANVL_DIALECT_AML);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "SF11: context created for deep structure");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "SF11: deep nesting parsing succeeds");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "SF11: one statement");

    free(source);
    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("SF01_arrays_sample",    NULL, test_sf01_arrays_sample,    td);
    TestBit.run_ex("SF02_assignments",      NULL, test_sf02_assignments_sample, td);
    TestBit.run_ex("SF03_attributes",       NULL, test_sf03_attributes_sample, td);
    TestBit.run_ex("SF04_inherits",         NULL, test_sf04_inherits_sample,   td);
    TestBit.run_ex("SF05_modpack",          NULL, test_sf05_modpack_sample,    td);
    TestBit.run_ex("SF06_objects",          NULL, test_sf06_objects_sample,    td);
    TestBit.run_ex("SF07_tuples",           NULL, test_sf07_tuples_sample,     td);
    TestBit.run_ex("SF08_generic_aurora",   NULL, test_sf08_generic_aurora,    td);
    TestBit.run_ex("SF09_vars",             NULL, test_sf09_vars_sample,       td);
    TestBit.run_ex("SF10_schema_asch",      NULL, test_sf10_schema_asch,       td);
    TestBit.run_ex("SF11_deep_nesting",     NULL, test_sf11_deep_nesting,      td);

    return TestBit.report();
}
