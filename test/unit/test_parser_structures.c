/*
 * test_parser_structures.c - Structural parser tests: inheritance,
 * nested collections, tuple variants, blob tags.
 * Converted from sigma.test to TestBit.
 */

#include "anvil.h"
#include "testbit.h"
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
    TestBit.is_true(result, "source parsing succeeds");

    return ctx;
}

/* ================================================================== */
/* INHERITANCE                                                        */
/* ================================================================== */

/* PS01 — basic inheritance: Child : Parent */
static void test_ps01_inheritance_basic(void) {
    const char *source =
        "Parent := { x := 10, y := 20 }\n"
        "Child : Parent := { x := 15 }";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_true(Context.statement_count(ctx) >= 2,
                    "PS01: at least 2 statements");

    statement stmt = Context.get_statement(ctx, 1);
    TestBit.is_not_null(stmt, "PS01: stmt[1] exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt),
                         "PS01: stmt[1] is assignment");
    TestBit.is_not_null(stmt->base_meta,
                        "PS01: stmt[1] has base_meta for inheritance");

    Context.dispose(ctx);
}

/* PS02 — inheritance with attributes on child and base */
static void test_ps02_inheritance_with_attributes(void) {
    const char *source =
        "Base @[version=1] := { health := 100 }\n"
        "Child : Base @[difficulty=hard] := { health := 150, level := 5 }";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_true(Context.statement_count(ctx) >= 2,
                    "PS02: at least 2 statements");

    statement stmt = Context.get_statement(ctx, 1);
    TestBit.is_not_null(stmt, "PS02: stmt[1] exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN, (long long)Statement.type(stmt),
                         "PS02: stmt[1] is assignment with inheritance");
    TestBit.is_not_null(stmt->base_meta,
                        "PS02: stmt[1] has base_meta");

    Context.dispose(ctx);
}

/* PS03 — multi-level inheritance chain */
static void test_ps03_inheritance_chain(void) {
    const char *source =
        "Base := { value := 1 }\n"
        "Middle : Base := { value := 2 }\n"
        "Derived : Middle := { value := 3 }";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_true(Context.statement_count(ctx) >= 3,
                    "PS03: at least 3 statements in inheritance chain");

    Context.dispose(ctx);
}

/* ================================================================== */
/* NESTED STRUCTURES                                                  */
/* ================================================================== */

/* PS04 — array containing objects: [{x := 1}, {y := 2}] */
static void test_ps04_nested_object_in_array(void) {
    const char *source = "data := [{ x := 1 }, { y := 2 }]";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PS04: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "PS04: statement exists");

    Context.dispose(ctx);
}

/* PS05 — object with array field: { items := [1, 2, 3] } */
static void test_ps05_nested_array_in_object(void) {
    const char *source = "config := { items := [1, 2, 3], name := \"test\" }";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PS05: one statement");

    Context.dispose(ctx);
}

/* PS06 — deeply nested: [{a := [{b := [{c := 1}]}]}] */
static void test_ps06_deeply_nested_structures(void) {
    const char *source = "deep := [{ a := [{ b := [{ c := 1 }] }] }]";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PS06: one statement for deeply nested structure");

    Context.dispose(ctx);
}

/* PS07 — array with objects and scalars mixed: [1, {x := 2}, "text"] */
static void test_ps07_array_with_mixed_contents(void) {
    const char *source = "mixed := [1, { x := 2 }, \"text\"]";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PS07: one statement");

    Context.dispose(ctx);
}

/* ================================================================== */
/* TUPLE VARIANTS                                                     */
/* ================================================================== */

/* PS08 — tuple with mixed scalar types: (1, "text", true, null) */
static void test_ps08_tuple_with_mixed_scalars(void) {
    const char *source = "coords := (1, \"text\", true, null)";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PS08: one statement");

    Context.dispose(ctx);
}

/* PS09 — tuple containing objects: ({x := 1}, {y := 2}) */
static void test_ps09_tuple_with_objects(void) {
    const char *source = "pair := ({x := 1}, {y := 2})";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PS09: one statement");

    Context.dispose(ctx);
}

/* PS10 — tuple containing arrays: ([1, 2], [3, 4]) */
static void test_ps10_tuple_with_arrays(void) {
    const char *source = "pair := ([1, 2], [3, 4])";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PS10: one statement");

    Context.dispose(ctx);
}

/* PS11 — single-element tuple: (1) — behavior is parser-defined */
static void test_ps11_tuple_single_element(void) {
    const char *source = "single := (1)";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PS11: context created");

    /* Parser may accept (as scalar) or reject (tuple too short) — either is valid.
       We verify only that the parser doesn't crash and context is disposable. */
    Context.parse(ctx);
    TestBit.is_not_null(ctx, "PS11: context still valid after parse attempt");

    Context.dispose(ctx);
}

/* ================================================================== */
/* BLOB TAG VARIANTS                                                  */
/* ================================================================== */

/* PS12 — blobs with various tags: @json, @base64, @md */
static void test_ps12_blob_with_various_tags(void) {
    const char *source =
        "json_data    := @json`{\"key\": \"value\"}`\n"
        "base64_data  := @base64`aGVsbG8gd29ybGQ=`\n"
        "markdown     := @md`# Header\nText`";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_true(Context.statement_count(ctx) >= 3,
                    "PS12: at least 3 statements for multiple blob tags");

    Context.dispose(ctx);
}

/* PS13 — bare blob (no tag): `raw content` */
static void test_ps13_bare_blob(void) {
    const char *source = "raw := `some raw content here`";

    context ctx = parse_source(source, ANVL_DIALECT_AML);

    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "PS13: one statement for bare blob");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("PS01_inheritance_basic",           NULL, test_ps01_inheritance_basic,           td);
    TestBit.run_ex("PS02_inheritance_with_attributes", NULL, test_ps02_inheritance_with_attributes, td);
    TestBit.run_ex("PS03_inheritance_chain",           NULL, test_ps03_inheritance_chain,           td);
    TestBit.run_ex("PS04_nested_object_in_array",      NULL, test_ps04_nested_object_in_array,      td);
    TestBit.run_ex("PS05_nested_array_in_object",      NULL, test_ps05_nested_array_in_object,      td);
    TestBit.run_ex("PS06_deeply_nested_structures",    NULL, test_ps06_deeply_nested_structures,    td);
    TestBit.run_ex("PS07_array_with_mixed_contents",   NULL, test_ps07_array_with_mixed_contents,   td);
    TestBit.run_ex("PS08_tuple_with_mixed_scalars",    NULL, test_ps08_tuple_with_mixed_scalars,    td);
    TestBit.run_ex("PS09_tuple_with_objects",          NULL, test_ps09_tuple_with_objects,          td);
    TestBit.run_ex("PS10_tuple_with_arrays",           NULL, test_ps10_tuple_with_arrays,           td);
    TestBit.run_ex("PS11_tuple_single_element",        NULL, test_ps11_tuple_single_element,        td);
    TestBit.run_ex("PS12_blob_with_various_tags",      NULL, test_ps12_blob_with_various_tags,      td);
    TestBit.run_ex("PS13_bare_blob",                   NULL, test_ps13_bare_blob,                   td);

    return TestBit.report();
}
