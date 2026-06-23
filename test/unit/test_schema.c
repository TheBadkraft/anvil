/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * test_schema.c — TDD: anvil.schema module (converted to TestBit)
 */
#include "anvil.h"
#include "context.h"
#include "errors.h"
#include "schema.h"
#include "testbit.h"
#include <string.h>

static context parse_src(const char *src) {
    CtxBuilder.set_source(&CtxBuilder, src, strlen(src));
    context ctx = CtxBuilder.build(&CtxBuilder);
    if (!ctx) return NULL;
    Context.parse(ctx);
    return ctx;
}
static void td(void) { Anvil.error_clear(); }

static void test_sc01_resolve_object(void) {
    const char *src = "#!aml\n@[schema]\nVehicle := { id := 0, make := \"\", active := true }";
    context ctx = parse_src(src);
    TestBit.is_not_null(ctx, "SC01: context");
    anvl_schema_ruleset_t *rules = Schema.resolve(ctx);
    TestBit.is_not_null(rules, "SC01: ruleset");
    TestBit.is_equal_int(1, (long long)rules->count, "SC01: one type");
    anvl_schema_type_t *t = Schema.get_type(rules, "Vehicle");
    TestBit.is_not_null(t, "SC01: Vehicle found");
    TestBit.is_equal_int(ANVL_SCHEMA_OBJECT, (long long)t->kind, "SC01: OBJECT");
    TestBit.is_equal_int(3, (long long)t->field_count, "SC01: 3 fields");
    TestBit.is_equal_str("id",     t->fields[0].name, "SC01: field id");
    TestBit.is_equal_str("make",   t->fields[1].name, "SC01: field make");
    TestBit.is_equal_str("active", t->fields[2].name, "SC01: field active");
    Schema.ruleset_free(rules); Context.dispose(ctx);
}

static void test_sc02_resolve_enum(void) {
    const char *src = "#!aml\n@[schema]\nStatus : enum := (active, inactive, pending)";
    context ctx = parse_src(src);
    anvl_schema_ruleset_t *rules = Schema.resolve(ctx);
    TestBit.is_not_null(rules, "SC02: ruleset");
    anvl_schema_type_t *t = Schema.get_type(rules, "Status");
    TestBit.is_not_null(t, "SC02: Status");
    TestBit.is_equal_int(ANVL_SCHEMA_ENUM, (long long)t->kind, "SC02: ENUM");
    TestBit.is_equal_int(3, (long long)t->value_count, "SC02: 3 values");
    TestBit.is_equal_str("active",   t->values[0], "SC02: active");
    TestBit.is_equal_str("inactive", t->values[1], "SC02: inactive");
    TestBit.is_equal_str("pending",  t->values[2], "SC02: pending");
    Schema.ruleset_free(rules); Context.dispose(ctx);
}

static void test_sc03_resolve_flags(void) {
    const char *src = "#!aml\n@[schema]\nPerms : flags := (read, write, execute)";
    context ctx = parse_src(src);
    anvl_schema_ruleset_t *rules = Schema.resolve(ctx);
    TestBit.is_not_null(rules, "SC03: ruleset");
    anvl_schema_type_t *t = Schema.get_type(rules, "Perms");
    TestBit.is_not_null(t, "SC03: Perms");
    TestBit.is_equal_int(ANVL_SCHEMA_FLAGS, (long long)t->kind, "SC03: FLAGS");
    TestBit.is_equal_int(3, (long long)t->value_count, "SC03: 3 flags");
    TestBit.is_equal_str("read",    t->values[0], "SC03: read");
    TestBit.is_equal_str("execute", t->values[2], "SC03: execute");
    Schema.ruleset_free(rules); Context.dispose(ctx);
}

static void test_sc04_missing_schema_attr(void) {
    const char *src = "#!aml\nVehicle := { id := 0 }";
    context ctx = parse_src(src);
    anvl_schema_ruleset_t *rules = Schema.resolve(ctx);
    TestBit.is_null(rules, "SC04: NULL without @[schema]");
    TestBit.is_equal_int(ANVL_ERR_SCHEMA_ATTR_MISSING,
                         (long long)Anvil.error_get()->code, "SC04: 4601");
    Context.dispose(ctx);
}

static void test_sc05_get_type(void) {
    const char *src = "#!aml\n@[schema]\nFoo := { x := 0 }";
    context ctx = parse_src(src);
    anvl_schema_ruleset_t *rules = Schema.resolve(ctx);
    TestBit.is_not_null(Schema.get_type(rules, "Foo"), "SC05: Foo found");
    TestBit.is_null(Schema.get_type(rules, "Bar"),     "SC05: Bar not found");
    TestBit.is_null(Schema.get_type(rules, NULL),      "SC05: NULL name");
    Schema.ruleset_free(rules); Context.dispose(ctx);
}

static void test_sc06_validate_ok(void) {
    context schema_ctx = parse_src("#!aml\n@[schema]\nRecord := { id := 0, name := \"\" }");
    context data_ctx   = parse_src("#!aml\nentry : Record := { id := 1, name := \"Alice\" }");
    anvl_schema_ruleset_t *rules  = Schema.resolve(schema_ctx);
    anvl_schema_result_t  *result = Schema.validate(rules, data_ctx, NULL);
    TestBit.is_not_null(result, "SC06: result");
    TestBit.is_true(result->is_valid, "SC06: valid");
    TestBit.is_equal_int(0, (long long)result->error_count, "SC06: 0 errors");
    Schema.result_free(result); Schema.ruleset_free(rules);
    Context.dispose(schema_ctx); Context.dispose(data_ctx);
}

static void test_sc07_missing_field(void) {
    context schema_ctx = parse_src("#!aml\n@[schema]\nRecord := { id := 0, name := \"\" }");
    context data_ctx   = parse_src("#!aml\nentry : Record := { id := 1 }");
    anvl_schema_ruleset_t *rules  = Schema.resolve(schema_ctx);
    anvl_schema_result_t  *result = Schema.validate(rules, data_ctx, NULL);
    TestBit.is_false(result->is_valid, "SC07: invalid");
    TestBit.is_equal_int(1, (long long)result->error_count, "SC07: 1 error");
    TestBit.is_equal_int(ANVL_ERR_SCHEMA_VALIDATION_REQUIRED,
                         (long long)result->errors[0].code, "SC07: 4604");
    Schema.result_free(result); Schema.ruleset_free(rules);
    Context.dispose(schema_ctx); Context.dispose(data_ctx);
}

static void test_sc08_type_mismatch(void) {
    /* Schema expects ARRAY for 'tags'; data provides SCALAR — type mismatch */
    context schema_ctx = parse_src("#!aml\n@[schema]\nRecord := { tags := [0] }");
    context data_ctx   = parse_src("#!aml\nentry : Record := { tags := \"oops\" }");
    anvl_schema_ruleset_t *rules  = Schema.resolve(schema_ctx);
    TestBit.is_not_null(rules, "SC08: ruleset");
    anvl_schema_result_t  *result = Schema.validate(rules, data_ctx, NULL);
    TestBit.is_false(result->is_valid, "SC08: invalid");
    TestBit.is_equal_int(ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH,
                         (long long)result->errors[0].code, "SC08: 4605");
    Schema.result_free(result); Schema.ruleset_free(rules);
    Context.dispose(schema_ctx); Context.dispose(data_ctx);
}

static void test_sc09_multi_error(void) {
    context schema_ctx = parse_src("#!aml\n@[schema]\nRecord := { a := 0, b := 0, c := 0 }");
    context data_ctx   = parse_src("#!aml\nentry : Record := { x := 1 }");
    anvl_schema_ruleset_t *rules  = Schema.resolve(schema_ctx);
    anvl_schema_result_t  *result = Schema.validate(rules, data_ctx, NULL);
    TestBit.is_false(result->is_valid, "SC09: invalid");
    TestBit.is_equal_int(3, (long long)result->error_count, "SC09: all 3 errors");
    Schema.result_free(result); Schema.ruleset_free(rules);
    Context.dispose(schema_ctx); Context.dispose(data_ctx);
}

static void test_sc10_unknown_base_skipped(void) {
    context schema_ctx = parse_src("#!aml\n@[schema]\nRecord := { id := 0 }");
    context data_ctx   = parse_src("#!aml\nentry : Unknown := { id := 1 }");
    anvl_schema_ruleset_t *rules  = Schema.resolve(schema_ctx);
    anvl_schema_result_t  *result = Schema.validate(rules, data_ctx, NULL);
    TestBit.is_true(result->is_valid, "SC10: valid (unknown base skipped)");
    TestBit.is_equal_int(0, (long long)result->error_count, "SC10: 0 errors");
    Schema.result_free(result); Schema.ruleset_free(rules);
    Context.dispose(schema_ctx); Context.dispose(data_ctx);
}

int main(void) {
    TestBit.run_ex("SC01_resolve_object",       NULL, test_sc01_resolve_object,       td);
    TestBit.run_ex("SC02_resolve_enum",         NULL, test_sc02_resolve_enum,         td);
    TestBit.run_ex("SC03_resolve_flags",        NULL, test_sc03_resolve_flags,        td);
    TestBit.run_ex("SC04_missing_schema_attr",  NULL, test_sc04_missing_schema_attr,  td);
    TestBit.run_ex("SC05_get_type",             NULL, test_sc05_get_type,             td);
    TestBit.run_ex("SC06_validate_ok",          NULL, test_sc06_validate_ok,          td);
    TestBit.run_ex("SC07_missing_field",        NULL, test_sc07_missing_field,        td);
    TestBit.run_ex("SC08_type_mismatch",        NULL, test_sc08_type_mismatch,        td);
    TestBit.run_ex("SC09_multi_error",          NULL, test_sc09_multi_error,          td);
    TestBit.run_ex("SC10_unknown_base_skipped", NULL, test_sc10_unknown_base_skipped, td);
    return TestBit.report();
}
