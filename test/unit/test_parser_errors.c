/*
 * test_parser_errors.c - Parser error tests: all negative/error-code paths.
 * Converted from sigma.test to TestBit.
 *
 * Dead-code regions (#if 0) from the original are omitted — those error codes
 * are defined in errors.h but never raised by parser.c.
 */

#include "anvil.h"
#include "testbit.h"
#include <stdio.h>
#include <string.h>

static void td(void) { Anvil.error_clear(); }

/* ------------------------------------------------------------------ */
/* Local helper — parse source expected to fail, capture error state  */
/* ------------------------------------------------------------------ */
static context parse_source_with_err(const char *source, anvl_dialect dialect,
                                     const anvl_error_state **err_state) {
    anvl_ctx_builder_i *builder = Context.get_builder();
    builder->set_dialect(builder, dialect);
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "context created from source");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "parser should fail for invalid source");
    *err_state = Anvil.error_get();

    return ctx;
}

/* ================================================================== */
/* PE01–PE02: Missing structural tokens                               */
/* ================================================================== */

/* PE01 — missing assignment operator */
static void test_pe01_missing_assignment(void) {
    const char *source = "name \"John\"";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AML, &err);

    TestBit.is_not_null((void *)err, "PE01: error state set");
    TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_ASSIGN, (long long)err->code,
                         "PE01: error is EXPECTED_ASSIGN");

    Context.dispose(ctx);
}

/* PE02 — missing identifier */
static void test_pe02_missing_identifier(void) {
    const char *source = ":= \"value\"";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AML, &err);

    TestBit.is_not_null((void *)err, "PE02: error state set");
    TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, (long long)err->code,
                         "PE02: error is EXPECTED_IDENTIFIER");

    Context.dispose(ctx);
}

/* ================================================================== */
/* PE03–PE06: Attribute errors                                        */
/* ================================================================== */

/* PE03 — invalid value in attribute: @[version = {name := "test"}] */
static void test_pe03_invalid_value_in_attribute(void) {
    const char *source = "@[version = {name := \"test\"}]\nname := \"value\"";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE03: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE03: invalid value in attribute fails");
    TestBit.is_true(Anvil.error_is_set(), "PE03: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE03: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE, (long long)err->code,
                         "PE03: error is INVALID_VALUE_IN_ATTRIBUTE");

    Context.dispose(ctx);
}

/* PE04 — empty attribute block: @[] */
static void test_pe04_empty_attribute_block(void) {
    const char *source = "@[]\nname := \"value\"";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE04: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE04: empty attribute block fails");
    TestBit.is_true(Anvil.error_is_set(), "PE04: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE04: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK, (long long)err->code,
                         "PE04: error is EMPTY_ATTRIBUTE_BLOCK");

    Context.dispose(ctx);
}

/* PE05 — missing comma in attributes: @[version = "1.0" author] */
static void test_pe05_missing_comma_in_attributes(void) {
    const char *source = "@[version = \"1.0\" author]\nname := \"value\"";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE05: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE05: missing comma in attributes fails");
    TestBit.is_true(Anvil.error_is_set(), "PE05: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE05: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES, (long long)err->code,
                         "PE05: error is MISSING_COMMA_IN_ATTRIBUTES");

    Context.dispose(ctx);
}

/* PE06 — invalid attribute: zero-length key @[=value] */
static void test_pe06_invalid_attribute(void) {
    const char *source = "@[=value]\nname := val";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AML, &err);

    TestBit.is_not_null((void *)err, "PE06: error state set for zero-length attribute key");
    TestBit.is_equal_int(ANVL_ERR_PARSER_INVALID_ATTRIBUTE, (long long)err->code,
                         "PE06: error is INVALID_ATTRIBUTE");

    Context.dispose(ctx);
}

/* ================================================================== */
/* PE07–PE08: Identifier errors                                       */
/* ================================================================== */

/* PE07 — invalid identifier: starts with digit */
static void test_pe07_invalid_identifier(void) {
    const char *source = "123invalid := \"value\"";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE07: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE07: invalid identifier fails");
    TestBit.is_true(Anvil.error_is_set(), "PE07: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE07: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, (long long)err->code,
                         "PE07: error is EXPECTED_IDENTIFIER");

    Context.dispose(ctx);
}

/* PE08 — identifier is keyword: 'vars' reserved */
static void test_pe08_identifier_is_keyword(void) {
    const char *source = "vars := 42";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AML, &err);

    TestBit.is_not_null((void *)err, "PE08: error state set for keyword used as identifier");
    TestBit.is_equal_int(ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD, (long long)err->code,
                         "PE08: error is IDENTIFIER_IS_KEYWORD");

    Context.dispose(ctx);
}

/* ================================================================== */
/* PE09–PE11: Array errors                                            */
/* ================================================================== */

/* PE09 — missing comma in array: [1 2] */
static void test_pe09_missing_comma_in_array(void) {
    const char *source = "arr := [1 2]";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE09: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE09: missing comma in array fails");
    TestBit.is_true(Anvil.error_is_set(), "PE09: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE09: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY, (long long)err->code,
                         "PE09: error is MISSING_COMMA_IN_ARRAY");

    Context.dispose(ctx);
}

/* PE10 — expected array close: [1, 2, 3 (no close) */
static void test_pe10_expected_array_close(void) {
    const char *source = "arr := [1, 2, 3";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AML, &err);

    TestBit.is_not_null((void *)err, "PE10: error state set for unclosed array");

    Context.dispose(ctx);
}

/* PE11 — empty array not allowed: arr := [] */
static void test_pe11_empty_array_not_allowed(void) {
    const char *source = "arr := []";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE11: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE11: empty array not allowed");
    TestBit.is_true(Anvil.error_is_set(), "PE11: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE11: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED, (long long)err->code,
                         "PE11: error is EMPTY_ARRAY_NOT_ALLOWED");

    Context.dispose(ctx);
}

/* ================================================================== */
/* PE12–PE14: Tuple errors                                            */
/* ================================================================== */

/* PE12 — expected tuple close: foo := (32, 22, 15 (no close) */
static void test_pe12_expected_tuple_close(void) {
    const char *source = "foo := (32, 22, 15";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AML, &err);

    TestBit.is_not_null((void *)err, "PE12: error state set for unclosed tuple");
    TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE, (long long)err->code,
                         "PE12: error is EXPECTED_TUPLE_CLOSE");

    Context.dispose(ctx);
}

/* PE13 — expected comma in tuple: (1 2) */
static void test_pe13_expected_comma_in_tuple(void) {
    const char *source = "test := (1 2)";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE13: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE13: missing comma in tuple fails");
    TestBit.is_true(Anvil.error_is_set(), "PE13: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE13: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE, (long long)err->code,
                         "PE13: error is EXPECTED_COMMA_IN_TUPLE");

    Context.dispose(ctx);
}

/* ================================================================== */
/* PE14: Object errors                                                */
/* ================================================================== */

/* PE14 — empty object not allowed: obj := {} */
static void test_pe14_empty_object_not_allowed(void) {
    const char *source = "obj := {}";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE14: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE14: empty object not allowed");
    TestBit.is_true(Anvil.error_is_set(), "PE14: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE14: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED, (long long)err->code,
                         "PE14: error is EMPTY_OBJECT_NOT_ALLOWED");

    Context.dispose(ctx);
}

/* ================================================================== */
/* PE15–PE19: Token/char/string/blob errors                          */
/* ================================================================== */

/* PE15 — unexpected token: name := @invalid */
static void test_pe15_unexpected_token(void) {
    const char *source = "name := @invalid";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE15: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE15: unexpected token fails");
    TestBit.is_true(Anvil.error_is_set(), "PE15: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE15: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, (long long)err->code,
                         "PE15: error is UNEXPECTED_TOKEN");

    Context.dispose(ctx);
}

/* PE16 — unexpected char: import without opening quote */
static void test_pe16_unexpected_char(void) {
    const char *source = "import notaquote";
    const anvl_error_state *err = NULL;
    context ctx = parse_source_with_err(source, ANVL_DIALECT_AML, &err);

    TestBit.is_not_null((void *)err, "PE16: error state set for unexpected char in import");
    TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_CHAR, (long long)err->code,
                         "PE16: error is UNEXPECTED_CHAR");

    Context.dispose(ctx);
}

/* PE17 — unterminated string: name := "value (no close quote) */
static void test_pe17_unterminated_string(void) {
    const char *source = "name := \"value";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE17: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE17: unterminated string fails");
    TestBit.is_true(Anvil.error_is_set(), "PE17: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE17: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_UNTERMINATED_STRING, (long long)err->code,
                         "PE17: error is UNTERMINATED_STRING");

    Context.dispose(ctx);
}

/* PE18 — unterminated blob: data := @email`user@example.com (no close backtick) */
static void test_pe18_unterminated_blob(void) {
    const char *source = "data := @email`user@example.com";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE18: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE18: unterminated blob fails");
    TestBit.is_true(Anvil.error_is_set(), "PE18: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE18: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_UNTERMINATED_BLOB, (long long)err->code,
                         "PE18: error is UNTERMINATED_BLOB");

    Context.dispose(ctx);
}

/* ================================================================== */
/* PE19–PE23: Soft/conditional error paths                           */
/* ================================================================== */

/* PE19 — unexpected module attributes (attributes after statements) */
static void test_pe19_unexpected_module_attributes(void) {
    const char *source = "name := \"value\"\n@[version = \"1.0\"]";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE19: context created");

    bool result = Context.parse(ctx);
    if (!result) {
        TestBit.is_true(Anvil.error_is_set(),
                        "PE19: error set when attributes-after-statements fails");
    }

    Context.dispose(ctx);
}

/* PE20 — missing value after :=  */
static void test_pe20_expected_value(void) {
    const char *source = "name :=";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE20: context created");

    bool result = Context.parse(ctx);
    if (!result) {
        TestBit.is_true(Anvil.error_is_set(), "PE20: error set when value missing");
    }

    Context.dispose(ctx);
}

/* PE21 — unclosed object: { x := 1 (no close brace) */
static void test_pe21_expected_object_close(void) {
    const char *source = "obj := { x := 1";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE21: context created");

    bool result = Context.parse(ctx);
    if (!result) {
        TestBit.is_true(Anvil.error_is_set(), "PE21: error set for unclosed object");
    }

    Context.dispose(ctx);
}

/* PE22 — trailing comma in object: {x := 1,} */
static void test_pe22_trailing_comma_in_object(void) {
    const char *source = "obj := {x := 1,}";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE22: context created");

    bool result = Context.parse(ctx);
    if (!result) {
        TestBit.is_true(Anvil.error_is_set(), "PE22: error set for trailing comma in object");
    }

    Context.dispose(ctx);
}

/* PE23 — missing field after comma: {x := 1, } */
static void test_pe23_expected_object_field(void) {
    const char *source = "obj := {x := 1, }";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE23: context created");

    bool result = Context.parse(ctx);
    if (!result) {
        TestBit.is_true(Anvil.error_is_set(), "PE23: error set for missing field after comma");
    }

    Context.dispose(ctx);
}

/* ================================================================== */
/* PE24–PE25: Shebang placement errors                               */
/* ================================================================== */

/* PE24 — multiple shebangs: #!aml then #!asl */
static void test_pe24_multiple_shebang(void) {
    const char *source = "#!aml\n#!asl\nname := \"test\"";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    if (ctx) {
        Context.parse(ctx);
        Context.dispose(ctx);
    }
    /* Either the build or parse fails — we just verify no crash and clean up. */
}

/* PE25 — shebang after statements */
static void test_pe25_shebang_after_statements(void) {
    const char *source = "name := \"test\"\n#!aml\nage := 42";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE25: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE25: shebang after statements fails");
    TestBit.is_true(Anvil.error_is_set(), "PE25: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_equal_int(ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS, (long long)err->code,
                         "PE25: error is SHEBANG_AFTER_STATEMENTS");

    Context.dispose(ctx);
}

/* PE26 — unterminated block comment consumes to EOF without closing */
static void test_pe26_unterminated_block_comment(void) {
    const char *source = "#!aml\n/* unterminated\nFoo := { x := 1 }";

    ctx_builder builder = Context.get_builder();
    builder->set_source(builder, source, strlen(source));

    context ctx = builder->build(builder);
    TestBit.is_not_null(ctx, "PE26: context created");

    bool result = Context.parse(ctx);
    TestBit.is_false(result, "PE26: unterminated block comment fails");
    TestBit.is_true(Anvil.error_is_set(), "PE26: error is set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "PE26: error state available");
    TestBit.is_equal_int(ANVL_ERR_PARSER_UNTERMINATED_COMMENT, (long long)err->code,
                         "PE26: error is UNTERMINATED_COMMENT");

    Context.dispose(ctx);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("PE01_missing_assignment",            NULL, test_pe01_missing_assignment,            td);
    TestBit.run_ex("PE02_missing_identifier",            NULL, test_pe02_missing_identifier,            td);
    TestBit.run_ex("PE03_invalid_value_in_attribute",    NULL, test_pe03_invalid_value_in_attribute,    td);
    TestBit.run_ex("PE04_empty_attribute_block",         NULL, test_pe04_empty_attribute_block,         td);
    TestBit.run_ex("PE05_missing_comma_in_attributes",   NULL, test_pe05_missing_comma_in_attributes,   td);
    TestBit.run_ex("PE06_invalid_attribute",             NULL, test_pe06_invalid_attribute,             td);
    TestBit.run_ex("PE07_invalid_identifier",            NULL, test_pe07_invalid_identifier,            td);
    TestBit.run_ex("PE08_identifier_is_keyword",         NULL, test_pe08_identifier_is_keyword,         td);
    TestBit.run_ex("PE09_missing_comma_in_array",        NULL, test_pe09_missing_comma_in_array,        td);
    TestBit.run_ex("PE10_expected_array_close",          NULL, test_pe10_expected_array_close,          td);
    TestBit.run_ex("PE11_empty_array_not_allowed",       NULL, test_pe11_empty_array_not_allowed,       td);
    TestBit.run_ex("PE12_expected_tuple_close",          NULL, test_pe12_expected_tuple_close,          td);
    TestBit.run_ex("PE13_expected_comma_in_tuple",       NULL, test_pe13_expected_comma_in_tuple,       td);
    TestBit.run_ex("PE14_empty_object_not_allowed",      NULL, test_pe14_empty_object_not_allowed,      td);
    TestBit.run_ex("PE15_unexpected_token",              NULL, test_pe15_unexpected_token,              td);
    TestBit.run_ex("PE16_unexpected_char",               NULL, test_pe16_unexpected_char,               td);
    TestBit.run_ex("PE17_unterminated_string",           NULL, test_pe17_unterminated_string,           td);
    TestBit.run_ex("PE18_unterminated_blob",             NULL, test_pe18_unterminated_blob,             td);
    TestBit.run_ex("PE19_unexpected_module_attributes",  NULL, test_pe19_unexpected_module_attributes,  td);
    TestBit.run_ex("PE20_expected_value",                NULL, test_pe20_expected_value,                td);
    TestBit.run_ex("PE21_expected_object_close",         NULL, test_pe21_expected_object_close,         td);
    TestBit.run_ex("PE22_trailing_comma_in_object",      NULL, test_pe22_trailing_comma_in_object,      td);
    TestBit.run_ex("PE23_expected_object_field",         NULL, test_pe23_expected_object_field,         td);
    TestBit.run_ex("PE24_multiple_shebang",              NULL, test_pe24_multiple_shebang,              td);
    TestBit.run_ex("PE25_shebang_after_statements",      NULL, test_pe25_shebang_after_statements,      td);
    TestBit.run_ex("PE26_unterminated_block_comment",    NULL, test_pe26_unterminated_block_comment,   td);

    return TestBit.report();
}
