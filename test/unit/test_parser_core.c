/*
 * test_parser_core.c - Core parser tests: basic assignment, scalars, collections
 * Converted from sigma.test to TestBit.
 */

#include "anvil.h"
#include "testbit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void td(void) { Anvil.error_clear(); }

/* ------------------------------------------------------------------ */
/* Local helper — parse a source string, asserting success            */
/* ------------------------------------------------------------------ */
static context parse_source(const char *source, anvl_dialect dialect) {
    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, dialect);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "context created from source");

    bool result = Context.parse(ctx);
    if (!result) {
        fprintf(stderr, "[DEBUG]: Failed to parse source: %s\n", source);
        const anvl_error_state *err = Anvil.error_get();
        if (err->message) fprintf(stderr, "Error: %s\n", err->message);
    }
    TestBit.is_true(result, "source parsing succeeds");

    return ctx;
}

/* ------------------------------------------------------------------ */
/* PC01 — empty source fails to build                                 */
/* ------------------------------------------------------------------ */
static void test_pc01_empty_source(void) {
    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, "", 0);

    context ctx = builder->build(builder);
    TestBit.is_null(ctx, "PC01: empty source build fails");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PC01: error state set after failed build");
    TestBit.is_equal_int(ANVL_ERR_BUILDER_NO_SOURCE, (long long)err->code,
                         "PC01: error is BUILDER_NO_SOURCE");
}

/* ------------------------------------------------------------------ */
/* PC02 — simple assignment                                           */
/* ------------------------------------------------------------------ */
static void test_pc02_simple_assignment(void) {
    const char *source = "name := \"John\"";
    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC02: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC02: statement exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt),
                         "PC02: statement is assignment");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC03 — multiple statements                                         */
/* ------------------------------------------------------------------ */
static void test_pc03_multiple_statements(void) {
    const char *source =
        "name := \"John\"\n"
        "age := 30\n"
        "active := true\n";
    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(3, (long long)Context.statement_count(ctx),
                         "PC03: three statements");

    statement stmt1 = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt1, "PC03: stmt[0] exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt1),
                         "PC03: stmt[0] is assignment");

    statement stmt2 = Context.get_statement(ctx, 1);
    TestBit.is_not_null(stmt2, "PC03: stmt[1] exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt2),
                         "PC03: stmt[1] is assignment");

    statement stmt3 = Context.get_statement(ctx, 2);
    TestBit.is_not_null(stmt3, "PC03: stmt[2] exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt3),
                         "PC03: stmt[2] is assignment");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC04 — array parsing                                               */
/* ------------------------------------------------------------------ */
static void test_pc04_array(void) {
    const char *source = "numbers := [1, 2, 3]";
    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC04: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC04: statement exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt),
                         "PC04: statement is assignment");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC05 — object parsing                                              */
/* ------------------------------------------------------------------ */
static void test_pc05_object(void) {
    const char *source = "person := { name := \"John\", age := 30 }";
    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC05: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC05: statement exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt),
                         "PC05: statement is assignment");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC06 — tuple parsing                                               */
/* ------------------------------------------------------------------ */
static void test_pc06_tuple(void) {
    const char *source = "point := (10, 20)";
    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC06: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC06: statement exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt),
                         "PC06: statement is assignment");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC07 — number parsing (int, negative, float, zero, large)          */
/* ------------------------------------------------------------------ */
static void test_pc07_numbers(void) {
    const char *source =
        "int_val := 42\n"
        "neg_int := -123\n"
        "float_val := 3.14\n"
        "zero_val := 0\n"
        "large_num := 999999\n";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC07: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC07: number parsing succeeds");
    TestBit.is_true(Context.statement_count(ctx) >= 5,
                    "PC07: at least five statements");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC07: stmt[0] exists");
    TestBit.is_not_null(stmt->value_meta, "PC07: stmt[0] has value_meta");
    usize len = stmt->value_meta->len;
    char *value_text = malloc(len + 1);
    Source.substring(ctx->source, stmt->value_meta->pos, len, value_text);
    TestBit.is_equal_str("42", value_text, "PC07: stmt[0] value is '42'");
    free(value_text);

    stmt = Context.get_statement(ctx, 1);
    len = stmt->value_meta->len;
    value_text = malloc(len + 1);
    Source.substring(ctx->source, stmt->value_meta->pos, len, value_text);
    TestBit.is_equal_str("-123", value_text, "PC07: stmt[1] value is '-123'");
    free(value_text);

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC08 — booleans and null                                           */
/* ------------------------------------------------------------------ */
static void test_pc08_booleans_and_null(void) {
    const char *source =
        "bool_true := true\n"
        "bool_false := false\n"
        "null_val := null\n";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC08: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC08: boolean/null parsing succeeds");
    TestBit.is_equal_int(3, (long long)Context.statement_count(ctx),
                         "PC08: three statements");

    statement stmt = Context.get_statement(ctx, 0);
    usize len = stmt->value_meta->len;
    char *value_text = malloc(len + 1);
    Source.substring(ctx->source, stmt->value_meta->pos, len, value_text);
    TestBit.is_equal_str("true", value_text, "PC08: stmt[0] value is 'true'");
    free(value_text);

    stmt = Context.get_statement(ctx, 1);
    len = stmt->value_meta->len;
    value_text = malloc(len + 1);
    Source.substring(ctx->source, stmt->value_meta->pos, len, value_text);
    TestBit.is_equal_str("false", value_text, "PC08: stmt[1] value is 'false'");
    free(value_text);

    stmt = Context.get_statement(ctx, 2);
    len = stmt->value_meta->len;
    value_text = malloc(len + 1);
    Source.substring(ctx->source, stmt->value_meta->pos, len, value_text);
    TestBit.is_equal_str("null", value_text, "PC08: stmt[2] value is 'null'");
    free(value_text);

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC09 — bare literals                                               */
/* ------------------------------------------------------------------ */
static void test_pc09_bare_literals(void) {
    const char *source =
        "bare1 := some_value\n"
        "bare2 := another.value\n"
        "bare3 := mixed_123\n";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC09: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC09: bare literal parsing succeeds");
    TestBit.is_equal_int(3, (long long)Context.statement_count(ctx),
                         "PC09: three statements");

    statement stmt = Context.get_statement(ctx, 0);
    usize len = stmt->value_meta->len;
    char *value_text = malloc(len + 1);
    Source.substring(ctx->source, stmt->value_meta->pos, len, value_text);
    TestBit.is_equal_str("some_value", value_text, "PC09: stmt[0] value is 'some_value'");
    free(value_text);

    stmt = Context.get_statement(ctx, 1);
    len = stmt->value_meta->len;
    value_text = malloc(len + 1);
    Source.substring(ctx->source, stmt->value_meta->pos, len, value_text);
    TestBit.is_equal_str("another.value", value_text, "PC09: stmt[1] value is 'another.value'");
    free(value_text);

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC10 — module attributes                                           */
/* ------------------------------------------------------------------ */
static void test_pc10_module_attributes(void) {
    const char *source = "@[version = \"1.0\", author]\nname := \"test\"";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC10: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC10: module attributes parse successfully");
    TestBit.is_equal_int(2, (long long)Context.attribute_count(ctx),
                         "PC10: two attributes");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC10: one statement");

    attribute attr1 = Context.get_attribute(ctx, 0);
    TestBit.is_not_null(attr1, "PC10: attr[0] exists");
    TestBit.is_true(attr1->key_len > 0, "PC10: attr[0] has key");
    TestBit.is_true(attr1->value_len > 0, "PC10: attr[0] has value");

    attribute attr2 = Context.get_attribute(ctx, 1);
    TestBit.is_not_null(attr2, "PC10: attr[1] exists");
    TestBit.is_true(attr2->key_len > 0, "PC10: attr[1] has key");
    TestBit.is_equal_int(0, (long long)attr2->value_len, "PC10: attr[1] has no value");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC11 — blob parsing                                                */
/* ------------------------------------------------------------------ */
static void test_pc11_blobs(void) {
    const char *source =
        "email := @email`user@example.com`\n"
        "url := @http`https://example.com`\n"
        "data := `raw data here`\n";

    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, ANVL_DIALECT_AML);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC11: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC11: blob parsing succeeds");
    TestBit.is_equal_int(3, (long long)Context.statement_count(ctx),
                         "PC11: three statements");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt->value_meta, "PC11: stmt[0] has value_meta");
    usize len = stmt->value_meta->len;
    char *value_text = malloc(len + 1);
    Source.substring(ctx->source, stmt->value_meta->pos, len, value_text);
    TestBit.is_true(strstr(value_text, "@email") != NULL || strstr(value_text, "user@example.com") != NULL,
                    "PC11: stmt[0] contains blob content");
    free(value_text);

    stmt = Context.get_statement(ctx, 1);
    TestBit.is_not_null(stmt->value_meta, "PC11: stmt[1] has value_meta");

    stmt = Context.get_statement(ctx, 2);
    TestBit.is_not_null(stmt->value_meta, "PC11: stmt[2] has value_meta");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC12 — mixed array                                                 */
/* ------------------------------------------------------------------ */
static void test_pc12_mixed_array(void) {
    const char *source = "mixed := [1, \"hello\", true]";

    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, ANVL_DIALECT_AML);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC12: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC12: mixed array parsing succeeds");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC12: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC12: statement exists");
    TestBit.is_not_null(stmt->value_meta, "PC12: has value_meta for mixed array");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC13 — nested arrays                                               */
/* ------------------------------------------------------------------ */
static void test_pc13_nested_arrays(void) {
    const char *source = "nested := [[1, 2], [3, 4]]";

    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, ANVL_DIALECT_AML);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC13: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC13: nested array parsing succeeds");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC13: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC13: statement exists");
    TestBit.is_not_null(stmt->value_meta, "PC13: has value metadata");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC14 — mixed tuple                                                 */
/* ------------------------------------------------------------------ */
static void test_pc14_mixed_tuple(void) {
    const char *source = "mixed_tuple := (1, \"hello\", true)";

    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, ANVL_DIALECT_AML);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC14: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC14: mixed tuple parsing succeeds");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC14: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC14: statement exists");
    TestBit.is_not_null(stmt->value_meta, "PC14: has value_meta for tuple");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC15 — array with objects                                          */
/* ------------------------------------------------------------------ */
static void test_pc15_array_with_objects(void) {
    const char *source = "objects := [{name := \"John\", age := 30}, {name := \"Jane\", age := 25}]";

    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, ANVL_DIALECT_AML);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC15: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC15: array with objects parsing succeeds");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC15: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC15: statement exists");
    TestBit.is_not_null(stmt->value_meta, "PC15: has value metadata");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* PC16 — object with array field                                     */
/* ------------------------------------------------------------------ */
static void test_pc16_object_with_array(void) {
    const char *source = "person := {name := \"John\", scores := [85, 92, 78]}";

    ctx_builder builder = Context.get_builder();
    builder->set_dialect(builder, ANVL_DIALECT_AML);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PC16: context created");

    bool result = Context.parse(ctx);
    TestBit.is_true(result, "PC16: object with array parsing succeeds");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PC16: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PC16: statement exists");
    TestBit.is_not_null(stmt->value_meta, "PC16: has value metadata");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("PC01_empty_source",        NULL, test_pc01_empty_source,        td);
    TestBit.run_ex("PC02_simple_assignment",   NULL, test_pc02_simple_assignment,   td);
    TestBit.run_ex("PC03_multiple_statements", NULL, test_pc03_multiple_statements, td);
    TestBit.run_ex("PC04_array",               NULL, test_pc04_array,               td);
    TestBit.run_ex("PC05_object",              NULL, test_pc05_object,              td);
    TestBit.run_ex("PC06_tuple",               NULL, test_pc06_tuple,               td);
    TestBit.run_ex("PC07_numbers",             NULL, test_pc07_numbers,             td);
    TestBit.run_ex("PC08_booleans_and_null",   NULL, test_pc08_booleans_and_null,   td);
    TestBit.run_ex("PC09_bare_literals",       NULL, test_pc09_bare_literals,       td);
    TestBit.run_ex("PC10_module_attributes",   NULL, test_pc10_module_attributes,   td);
    TestBit.run_ex("PC11_blobs",               NULL, test_pc11_blobs,               td);
    TestBit.run_ex("PC12_mixed_array",         NULL, test_pc12_mixed_array,         td);
    TestBit.run_ex("PC13_nested_arrays",       NULL, test_pc13_nested_arrays,       td);
    TestBit.run_ex("PC14_mixed_tuple",         NULL, test_pc14_mixed_tuple,         td);
    TestBit.run_ex("PC15_array_with_objects",  NULL, test_pc15_array_with_objects,  td);
    TestBit.run_ex("PC16_object_with_array",   NULL, test_pc16_object_with_array,   td);

    return TestBit.report();
}
