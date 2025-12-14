/*
 * test_parser.c - Comprehensive tests for Anvil parser
 */

#include "anvil.h"
#include "utilities/helpers.h"
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   *logger = fopen("logs/test_parser.log", "w");
   Memory.init();
}

static void set_teardown() {
   Anvil.cleanup();
   Memory.teardown();
}

// Test 1: Empty source
static void test_parse_empty_source(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, "", 0);

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created for empty source");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Empty source should parse successfully");
   Assert.isTrue(Context.statement_count(ctx) == 0, "Empty source should have no statements");

   Context.dispose(ctx);
}

// Test 2: Simple assignment
static void test_parse_simple_assignment(void) {
   const char *source = "name := \"John\"";
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Simple assignment should parse successfully");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->type == ANVL_STMT_ASSN, "Should be assignment statement");
   Assert.isTrue(stmt->ident_len == 4, "Identifier should be 4 chars (name)");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_SCALAR, "Value should be scalar");

   Context.dispose(ctx);
}

// Test 3: Multiple statements
static void test_parse_multiple_statements(void) {
   const char *source =
       "name := \"John\"\n"
       "age := 30\n"
       "active := true\n";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Multiple statements should parse successfully");
   Assert.isTrue(Context.statement_count(ctx) == 3, "Should have three statements");

   // Validate first statement (name := "John")
   statement stmt1 = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt1, "First statement should exist");
   Assert.isTrue(stmt1->type == ANVL_STMT_ASSN, "First statement should be assignment");
   Assert.isTrue(stmt1->ident_len == 4, "Identifier length should be 4");
   Assert.isTrue(stmt1->value_type == ANVL_VALUE_SCALAR, "Value should be scalar");

   // Validate second statement (age := 30)
   statement stmt2 = Context.get_statement(ctx, 1);
   Assert.isNotNull(stmt2, "Second statement should exist");
   Assert.isTrue(stmt2->type == ANVL_STMT_ASSN, "Second statement should be assignment");
   Assert.isTrue(stmt2->ident_len == 3, "Identifier length should be 3");
   Assert.isTrue(stmt2->value_type == ANVL_VALUE_SCALAR, "Value should be scalar");

   // Validate third statement (active := true)
   statement stmt3 = Context.get_statement(ctx, 2);
   Assert.isNotNull(stmt3, "Third statement should exist");
   Assert.isTrue(stmt3->type == ANVL_STMT_ASSN, "Third statement should be assignment");
   Assert.isTrue(stmt3->ident_len == 6, "Identifier length should be 6");
   Assert.isTrue(stmt3->value_type == ANVL_VALUE_SCALAR, "Value should be scalar");

   Context.dispose(ctx);
}

static void test_parse_array(void) {
   const char *source = "numbers := [1, 2, 3]";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Array parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->type == ANVL_STMT_ASSN, "Statement should be assignment");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_ARRAY, "Value should be array");

   Context.dispose(ctx);
}

// Test 4: Object parsing (placeholder - will be implemented)
static void test_parse_object_placeholder(void) {
   const char *source = "person := { name := \"John\", age := 30 }";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   // For now, this should fail with unexpected token
   bool result = Context.parse(ctx);
   Assert.isFalse(result, "Object parsing should fail (not implemented yet)");
   Assert.isTrue(Anvil.error_is_set(), "Error should be set for unimplemented feature");

   Context.dispose(ctx);
   Anvil.error_clear();
}

// Test 5: Error handling - missing assignment operator
static void test_parse_missing_assignment(void) {
   const char *source = "name \"John\"";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isFalse(result, "Missing assignment should fail");
   Assert.isTrue(Anvil.error_is_set(), "Error should be set");

   const anvl_error_state *err = Anvil.error_get();
   Assert.isNotNull((void *)err, "Error state should be available");
   Assert.isTrue(err->code == ANVL_ERR_PARSER_EXPECTED_ASSIGN, "Should be EXPECTED_ASSIGN error");

   Context.dispose(ctx);
   Anvil.error_clear();
}

// Test 6: Error handling - missing identifier
static void test_parse_missing_identifier(void) {
   const char *source = ":= \"value\"";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isFalse(result, "Missing identifier should fail");
   Assert.isTrue(Anvil.error_is_set(), "Error should be set");

   Context.dispose(ctx);
   Anvil.error_clear();
}

// Test 7: Inheritance syntax (placeholder)
static void test_parse_inheritance_placeholder(void) {
   const char *source = "child : parent := { }";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   // For now, this should fail with unexpected token
   bool result = Context.parse(ctx);
   Assert.isFalse(result, "Inheritance parsing should fail (not implemented yet)");

   Context.dispose(ctx);
   Anvil.error_clear();
}

// Test 7: Number parsing
static void test_parse_numbers(void) {
   const char *source =
       "int_val := 42\n"
       "neg_int := -123\n"
       "float_val := 3.14\n"
       "sci_float := 1.23e-4\n"
       "hex_val := #FFAA00\n"
       "short_hex := #ABC\n"
       "hex_prefix := 0xFF\n"
       "long_hex := 0x123ABC\n"
       "binary_val := 0b1010\n"
       "byte_array := 0hDEADBEEF\n"
       "zero_val := 0\n"
       "large_num := 999999\n";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Number parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 12, "Should have twelve statements");

   // Validate that we can extract the actual values using substring   // Check a few key values
   statement stmt;

   // Check int_val := 42
   stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "First statement should exist");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_SCALAR, "Should be scalar value");
   char *value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isNotNull(value_text, "Should be able to extract value text");
   Assert.isTrue(strcmp(value_text, "42") == 0, "Value should be '42'");
   Memory.dispose(value_text);

   // Check neg_int := -123
   stmt = Context.get_statement(ctx, 1);
   value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isTrue(strcmp(value_text, "-123") == 0, "Value should be '-123'");
   Memory.dispose(value_text);

   // Check hex_prefix := 0xFF
   stmt = Context.get_statement(ctx, 6);
   value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isTrue(strcmp(value_text, "0xFF") == 0, "Value should be '0xFF'");
   Memory.dispose(value_text);

   // Check binary_val := 0b1010
   stmt = Context.get_statement(ctx, 8);
   value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isTrue(strcmp(value_text, "0b1010") == 0, "Value should be '0b1010'");
   Memory.dispose(value_text);

   // Check byte_array := 0hDEADBEEF
   stmt = Context.get_statement(ctx, 9);
   value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isTrue(strcmp(value_text, "0hDEADBEEF") == 0, "Value should be '0hDEADBEEF'");
   Memory.dispose(value_text);

   Context.dispose(ctx);
}

// Test 9: Boolean and null parsing
static void test_parse_booleans_and_null(void) {
   const char *source =
       "bool_true := true\n"
       "bool_false := false\n"
       "null_val := null\n";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Boolean/null parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 3, "Should have three statements");

   // Validate substring extraction
   statement stmt;

   stmt = Context.get_statement(ctx, 0); // bool_true := true
   char *value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isTrue(strcmp(value_text, "true") == 0, "Value should be 'true'");
   Memory.dispose(value_text);

   stmt = Context.get_statement(ctx, 1); // bool_false := false
   value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isTrue(strcmp(value_text, "false") == 0, "Value should be 'false'");
   Memory.dispose(value_text);

   stmt = Context.get_statement(ctx, 2); // null_val := null
   value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isTrue(strcmp(value_text, "null") == 0, "Value should be 'null'");
   Memory.dispose(value_text);

   Context.dispose(ctx);
}

// Test 10: Bare literals
static void test_parse_bare_literals(void) {
   const char *source =
       "bare1 := some_value\n"
       "bare2 := another.value\n"
       "bare3 := mixed_123\n";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Bare literal parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 3, "Should have three statements");

   // Validate substring extraction
   statement stmt;

   stmt = Context.get_statement(ctx, 0); // bare1 := some_value
   char *value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isTrue(strcmp(value_text, "some_value") == 0, "Value should be 'some_value'");
   Memory.dispose(value_text);

   stmt = Context.get_statement(ctx, 1); // bare2 := another.value
   value_text = Source.substring(ctx->source, stmt->value_pos, stmt->value_len);
   Assert.isTrue(strcmp(value_text, "another.value") == 0, "Value should be 'another.value'");
   Memory.dispose(value_text);

   Context.dispose(ctx);
}

// Test 11: Module attributes
static void test_parse_module_attributes(void) {
   const char *source = "@[version = \"1.0\", author]\nname := \"test\"";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Module attributes should parse successfully");
   Assert.isTrue(Context.attribute_count(ctx) == 2, "Should have two attributes");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");

   // Check first attribute (with value)
   attribute attr1 = Context.get_attribute(ctx, 0);
   Assert.isNotNull(attr1, "First attribute should exist");
   Assert.isTrue(attr1->key_len > 0, "First attribute should have key");
   Assert.isTrue(attr1->value_len > 0, "First attribute should have value");

   // Check second attribute (no value)
   attribute attr2 = Context.get_attribute(ctx, 1);
   Assert.isNotNull(attr2, "Second attribute should exist");
   Assert.isTrue(attr2->key_len > 0, "Second attribute should have key");
   Assert.isTrue(attr2->value_len == 0, "Second attribute should have no value");

   Context.dispose(ctx);
}

__attribute__((constructor)) static void register_test_parser(void) {
   testset("Parser Tests", set_config, set_teardown);

#if 0
   testcase("Empty Source", test_parse_empty_source);
   testcase("Simple Assignment", test_parse_simple_assignment);
#endif

   testcase("Multiple Statements", test_parse_multiple_statements);

   testcase("Array Parsing", test_parse_array);

#if 0
   testcase("Object Placeholder", test_parse_object_placeholder);
   testcase("Missing Assignment Error", test_parse_missing_assignment);
   testcase("Missing Identifier Error", test_parse_missing_identifier);
   testcase("Inheritance Placeholder", test_parse_inheritance_placeholder);
   testcase("Number Parsing", test_parse_numbers);
   testcase("Boolean and Null Parsing", test_parse_booleans_and_null);
   testcase("Bare Literals", test_parse_bare_literals);
   testcase("Module Attributes", test_parse_module_attributes);
#endif
}