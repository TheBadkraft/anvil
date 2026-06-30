/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 * test_context.c — Context interface unit tests (TestBit)
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-13
 * File: test/unit/test_context.c
 * ------------------------------------------------------------------
 * Converted from sigma.test to TestBit.
 * Tests requiring Allocator.alloc() (statement/attribute manual
 * construction) are skipped — no heap allocation in TestBit.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "context.h"
#include "testbit.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Shared teardown — mirrors td() in test_amp_collections.c           */
/* ------------------------------------------------------------------ */
static void td(void) { Anvil.error_clear(); }

/* ------------------------------------------------------------------ */
/* CTX01 — Context.get_builder returns a valid builder                */
/* ------------------------------------------------------------------ */
static void test_ctx01_get_builder(void) {
    anvl_ctx_builder_i *builder = Context.get_builder();
    TestBit.is_not_null(builder, "CTX01: Context.get_builder returns non-null");
}

/* ------------------------------------------------------------------ */
/* CTX02 — build with no source sets ANVL_ERR_BUILDER_NO_SOURCE      */
/* ------------------------------------------------------------------ */
static void test_ctx02_null_no_source(void) {
    anvl_ctx_builder_i *builder = Context.get_builder();
    context ctx = builder->build(builder);

    TestBit.is_null(ctx, "CTX02: build with no source returns NULL");
    TestBit.is_true(Anvil.error_is_set(), "CTX02: error is set");
    TestBit.is_equal_int(ANVL_ERR_BUILDER_NO_SOURCE,
                         (long long)Anvil.error_get()->code,
                         "CTX02: error is BUILDER_NO_SOURCE");
}

/* ------------------------------------------------------------------ */
/* CTX03 — set_source / build / dialect detection                     */
/* ------------------------------------------------------------------ */
static void test_ctx03_set_source(void) {
    const char *src = "#!aml";
    anvl_ctx_builder_i *builder = Context.get_builder();
    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX03: context created");
    TestBit.is_false(Anvil.error_is_set(), "CTX03: no error");
    TestBit.is_equal_int(ANVL_DIALECT_AML,
                         (long long)Context.dialect(ctx),
                         "CTX03: dialect is AML");
    TestBit.is_not_null(ctx->source, "CTX03: source object present");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* CTX04 — load file with shebang → AML dialect                      */
/* ------------------------------------------------------------------ */
static void test_ctx04_load_file_shebang(void) {
    const char *file_path = "test/fixtures/shebang.anvl";

    CtxBuilder.load_file(&CtxBuilder, file_path);
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX04: context created from file");
    TestBit.is_false(Anvil.error_is_set(), "CTX04: no error");
    TestBit.is_equal_int(ANVL_DIALECT_AML,
                         (long long)Context.dialect(ctx),
                         "CTX04: shebang → AML dialect");
    TestBit.is_not_null(ctx->source, "CTX04: source object present");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* CTX05 — load file without shebang → ASL dialect                   */
/* ------------------------------------------------------------------ */
static void test_ctx05_load_file_no_shebang(void) {
    const char *file_path = "test/fixtures/generic.aurora";

    CtxBuilder.load_file(&CtxBuilder, file_path);
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX05: context created from file");
    TestBit.is_false(Anvil.error_is_set(), "CTX05: no error");
    TestBit.is_equal_int(ANVL_DIALECT_ASL,
                         (long long)Context.dialect(ctx),
                         "CTX05: no shebang → ASL dialect");
    TestBit.is_not_null(ctx->source, "CTX05: source object present");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* CTX06 — set_dialect(ASL) overridden by AML shebang in source      */
/* ------------------------------------------------------------------ */
static void test_ctx06_shebang_overrides_dialect(void) {
    const char *src = "#!aml";

    CtxBuilder.set_dialect(&CtxBuilder, ANVL_DIALECT_ASL);
    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX06: context created");
    TestBit.is_false(Anvil.error_is_set(), "CTX06: no error");
    TestBit.is_equal_int(ANVL_DIALECT_AML,
                         (long long)Context.dialect(ctx),
                         "CTX06: shebang overrides set_dialect");
    TestBit.is_not_null(ctx->source, "CTX06: source object present");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* CTX07 — manual statement construction                              */
/* ------------------------------------------------------------------ */
static void test_ctx07_statement_operations(void) {
    const char *src = "test := value";

    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX07: context created");
    TestBit.is_equal_int(0, (long long)Context.statement_count(ctx),
                         "CTX07: initially no statements");
    TestBit.is_null(Context.get_statement(ctx, 0),
                    "CTX07: get_statement(0) on empty → NULL");

    statement stmt = malloc(sizeof(struct anvl_statement));
    if (stmt) memset(stmt, 0, sizeof(struct anvl_statement));
    stmt->meta[STMT_META_TYPE]      = ANVL_STMT_ASSN;
    stmt->meta[STMT_META_IDENT_POS] = 0;
    stmt->meta[STMT_META_IDENT_LEN] = 4;  /* "test" */
    stmt->meta[STMT_META_BASE_IDX]  = 0;
    stmt->meta[STMT_META_ATTR_IDX]  = 0;
    stmt->meta[STMT_META_VALUE_IDX] = 0;

    bool added = Context.add_statement(ctx, stmt);
    TestBit.is_true(added, "CTX07: statement added");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "CTX07: one statement");
    TestBit.is_not_null(Context.get_statement(ctx, 0),
                        "CTX07: get_statement(0) returns non-null");
    TestBit.is_equal_int(ANVL_STMT_ASSN,
                         (long long)Context.get_statement(ctx, 0)->meta[STMT_META_TYPE],
                         "CTX07: statement type is ASSN");

    free(stmt);
    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* CTX08 — manual attribute construction                              */
/* ------------------------------------------------------------------ */
static void test_ctx08_attribute_operations(void) {
    const char *src = "test";

    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX08: context created");
    TestBit.is_equal_int(0, (long long)Context.attribute_count(ctx),
                         "CTX08: initially no attributes");
    TestBit.is_null(Context.get_attribute(ctx, 0),
                    "CTX08: get_attribute(0) on empty → NULL");

    attribute attr = malloc(sizeof(struct anvl_attribute));
    if (attr) memset(attr, 0, sizeof(struct anvl_attribute));
    attr->key_pos   = 0;
    attr->key_len   = 4;  /* "test" */
    attr->value_pos = 0;
    attr->value_len = 0;

    bool added = Context.add_attribute(ctx, attr);
    TestBit.is_true(added, "CTX08: attribute added");
    TestBit.is_equal_int(1, (long long)Context.attribute_count(ctx),
                         "CTX08: one attribute");
    TestBit.is_not_null(Context.get_attribute(ctx, 0),
                        "CTX08: get_attribute(0) returns non-null");
    TestBit.is_equal_int(4,
                         (long long)Context.get_attribute(ctx, 0)->key_len,
                         "CTX08: key_len is 4");

    free(attr);
    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* CTX09 — parse basic single assignment                              */
/* ------------------------------------------------------------------ */
static void test_ctx09_parse_basic(void) {
    const char *src = "name := \"John\"";

    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX09: context created");

    bool parsed = Context.parse(ctx);
    TestBit.is_true(parsed, "CTX09: parse succeeds");
    TestBit.is_equal_int(1, (long long)Context.statement_count(ctx),
                         "CTX09: one statement");

    statement stmt = Context.get_statement(ctx, 0);
    TestBit.is_not_null(stmt, "CTX09: statement exists");
    TestBit.is_equal_int(ANVL_STMT_ASSN,
                         (long long)stmt->meta[STMT_META_TYPE],
                         "CTX09: statement type is ASSN");
    TestBit.is_equal_int(4, (long long)stmt->meta[STMT_META_IDENT_LEN],
                         "CTX09: identifier length is 4 (\"name\")");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* CTX10 — parse multiple statements                                  */
/* ------------------------------------------------------------------ */
static void test_ctx10_parse_multiple(void) {
    const char *src = "name := \"John\"\nage := 30\nactive := true";

    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX10: context created");

    bool parsed = Context.parse(ctx);
    TestBit.is_true(parsed, "CTX10: parse succeeds");
    TestBit.is_equal_int(3, (long long)Context.statement_count(ctx),
                         "CTX10: three statements");

    TestBit.is_equal_int(4,
        (long long)Context.get_statement(ctx, 0)->meta[STMT_META_IDENT_LEN],
        "CTX10: stmt[0] ident len 4 (\"name\")");
    TestBit.is_equal_int(3,
        (long long)Context.get_statement(ctx, 1)->meta[STMT_META_IDENT_LEN],
        "CTX10: stmt[1] ident len 3 (\"age\")");
    TestBit.is_equal_int(6,
        (long long)Context.get_statement(ctx, 2)->meta[STMT_META_IDENT_LEN],
        "CTX10: stmt[2] ident len 6 (\"active\")");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* CTX11 — get_statement bounds                                       */
/* ------------------------------------------------------------------ */
static void test_ctx11_get_statement_bounds(void) {
    const char *src = "test := value";

    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX11: context created");

    bool parsed = Context.parse(ctx);
    TestBit.is_true(parsed, "CTX11: parse succeeds");

    TestBit.is_not_null(Context.get_statement(ctx, 0),  "CTX11: stmt[0] exists");
    TestBit.is_null    (Context.get_statement(ctx, 1),  "CTX11: stmt[1] out of bounds → NULL");
    TestBit.is_null    (Context.get_statement(ctx, -1), "CTX11: negative index → NULL");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* CTX12 — add_statement rejects NULL                                 */
/* ------------------------------------------------------------------ */
static void test_ctx12_add_statement_null(void) {
    const char *src = "test";

    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);

    TestBit.is_not_null(ctx, "CTX12: context created");

    TestBit.is_false(Context.add_statement(ctx,  NULL), "CTX12: NULL stmt rejected");
    TestBit.is_false(Context.add_statement(NULL, NULL), "CTX12: NULL ctx rejected");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("CTX01_get_builder",             NULL, test_ctx01_get_builder,              td);
    TestBit.run_ex("CTX02_null_no_source",          NULL, test_ctx02_null_no_source,           td);
    TestBit.run_ex("CTX03_set_source",              NULL, test_ctx03_set_source,               td);
    TestBit.run_ex("CTX04_load_file_shebang",       NULL, test_ctx04_load_file_shebang,        td);
    TestBit.run_ex("CTX05_load_file_no_shebang",    NULL, test_ctx05_load_file_no_shebang,     td);
    TestBit.run_ex("CTX06_shebang_overrides_dialect",NULL, test_ctx06_shebang_overrides_dialect, td);
    TestBit.run_ex("CTX07_statement_operations",    NULL, test_ctx07_statement_operations,     td);
    TestBit.run_ex("CTX08_attribute_operations",    NULL, test_ctx08_attribute_operations,     td);
    TestBit.run_ex("CTX09_parse_basic",             NULL, test_ctx09_parse_basic,              td);
    TestBit.run_ex("CTX10_parse_multiple",          NULL, test_ctx10_parse_multiple,           td);
    TestBit.run_ex("CTX11_get_statement_bounds",    NULL, test_ctx11_get_statement_bounds,     td);
    TestBit.run_ex("CTX12_add_statement_null",      NULL, test_ctx12_add_statement_null,       td);

    return TestBit.report();
}
