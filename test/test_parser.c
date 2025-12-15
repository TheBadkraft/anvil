/*
 * test_parser.c - Comprehensive tests for Anvil parser
 */

#include "anvil.h"
#include "utilities/helpers.h"
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static context test_parse_sample_file(const char *, anvl_dialect, usize, usize, usize);

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
   Assert.isTrue(Context.value_count(ctx) == 3, "Should have three values");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->type == ANVL_STMT_ASSN, "Statement should be assignment");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_ARRAY, "Value should be array");
   Assert.isTrue(stmt->value_pos == 0, "Array should start at value index 0");
   Assert.isTrue(stmt->value_len == 3, "Array should have 3 elements");

   // Check individual array elements
   value elem0 = Context.get_value(ctx, 0);
   Assert.isNotNull(elem0, "Element 0 should exist");
   Assert.isTrue(elem0->type == ANVL_VALUE_SCALAR, "Element 0 should be scalar");

   value elem1 = Context.get_value(ctx, 1);
   Assert.isNotNull(elem1, "Element 1 should exist");
   Assert.isTrue(elem1->type == ANVL_VALUE_SCALAR, "Element 1 should be scalar");

   value elem2 = Context.get_value(ctx, 2);
   Assert.isNotNull(elem2, "Element 2 should exist");
   Assert.isTrue(elem2->type == ANVL_VALUE_SCALAR, "Element 2 should be scalar");

   Context.dispose(ctx);
}

#if 0  // pending parsing tests for these features - review
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

// additional negative tests to capture every error code (if possible)
#endif // pending parsing tests for these features - review

// Test 8: Number parsing
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

// Test 5: Tuple parsing
static void test_parse_tuple(void) {
   const char *source = "point := (10, 20)";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Tuple parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");
   Assert.isTrue(Context.value_count(ctx) == 2, "Should have two values");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->type == ANVL_STMT_ASSN, "Statement should be assignment");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_TUPLE, "Value should be tuple");
   Assert.isTrue(stmt->value_pos == 0, "Tuple should start at value index 0");
   Assert.isTrue(stmt->value_len == 2, "Tuple should have 2 elements");

   // Check individual tuple elements
   value elem0 = Context.get_value(ctx, 0);
   Assert.isNotNull(elem0, "Element 0 should exist");
   Assert.isTrue(elem0->type == ANVL_VALUE_SCALAR, "Element 0 should be scalar");

   value elem1 = Context.get_value(ctx, 1);
   Assert.isNotNull(elem1, "Element 1 should exist");
   Assert.isTrue(elem1->type == ANVL_VALUE_SCALAR, "Element 1 should be scalar");

   Context.dispose(ctx);
}

// Test 6: Object parsing
static void test_parse_object(void) {
   const char *source = "person := { name := \"John\", age := 30 }";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Object parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");
   Assert.isTrue(Context.field_count(ctx) == 2, "Should have two fields");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->type == ANVL_STMT_ASSN, "Statement should be assignment");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_OBJECT, "Value should be object");
   Assert.isTrue(stmt->value_pos == 0, "Object should start at field index 0");
   Assert.isTrue(stmt->value_len == 2, "Object should have 2 fields");

   // Check individual object fields
   field fld0 = Context.get_field(ctx, 0);
   Assert.isNotNull(fld0, "Field 0 should exist");
   Assert.isTrue(fld0->key_len == 4, "Field 0 key should be 4 chars (name)");
   Assert.isTrue(fld0->value_type == ANVL_VALUE_SCALAR, "Field 0 value should be scalar");

   field fld1 = Context.get_field(ctx, 1);
   Assert.isNotNull(fld1, "Field 1 should exist");
   Assert.isTrue(fld1->key_len == 3, "Field 1 key should be 3 chars (age)");
   Assert.isTrue(fld1->value_type == ANVL_VALUE_SCALAR, "Field 1 value should be scalar");

   Context.dispose(ctx);
}

// Test 7: Mixed types in array
static void test_parse_mixed_array(void) {
   const char *source = "mixed := [1, \"hello\", true]";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Mixed array parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");
   Assert.isTrue(Context.value_count(ctx) == 3, "Should have three values");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_ARRAY, "Value should be array");
   Assert.isTrue(stmt->value_len == 3, "Array should have 3 elements");

   // Check element types
   value elem0 = Context.get_value(ctx, 0);
   Assert.isTrue(elem0->type == ANVL_VALUE_SCALAR, "Element 0 should be scalar");

   value elem1 = Context.get_value(ctx, 1);
   Assert.isTrue(elem1->type == ANVL_VALUE_SCALAR, "Element 1 should be scalar");

   value elem2 = Context.get_value(ctx, 2);
   Assert.isTrue(elem2->type == ANVL_VALUE_SCALAR, "Element 2 should be scalar");

   Context.dispose(ctx);
}

// Test 8: Nested arrays
static void test_parse_nested_arrays(void) {
   const char *source = "nested := [[1, 2], [3, 4]]";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Nested array parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");
   Assert.isTrue(Context.value_count(ctx) == 6, "Should have six values (2 arrays + 4 scalars)");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_ARRAY, "Value should be array");
   Assert.isTrue(stmt->value_len == 2, "Outer array should have 2 elements");

   // Check that elements are arrays - they should be at indices 2 and 5
   value elem0 = Context.get_value(ctx, 2);
   Assert.isTrue(elem0->type == ANVL_VALUE_ARRAY, "Element 0 should be array");
   Assert.isTrue(elem0->pos == 0, "Element 0 should start at value index 0");
   Assert.isTrue(elem0->len == 2, "Element 0 should have 2 elements");

   value elem1 = Context.get_value(ctx, 5);
   Assert.isTrue(elem1->type == ANVL_VALUE_ARRAY, "Element 1 should be array");
   Assert.isTrue(elem1->pos == 3, "Element 1 should start at value index 3");
   Assert.isTrue(elem1->len == 2, "Element 1 should have 2 elements");

   // Also check the scalar elements
   value scalar0 = Context.get_value(ctx, 0);
   Assert.isTrue(scalar0->type == ANVL_VALUE_SCALAR, "Index 0 should be scalar");

   value scalar1 = Context.get_value(ctx, 1);
   Assert.isTrue(scalar1->type == ANVL_VALUE_SCALAR, "Index 1 should be scalar");

   value scalar2 = Context.get_value(ctx, 3);
   Assert.isTrue(scalar2->type == ANVL_VALUE_SCALAR, "Index 3 should be scalar");

   value scalar3 = Context.get_value(ctx, 4);
   Assert.isTrue(scalar3->type == ANVL_VALUE_SCALAR, "Index 4 should be scalar");

   Context.dispose(ctx);
}

// Test 9: Mixed types in tuple
static void test_parse_mixed_tuple(void) {
   const char *source = "mixed_tuple := (1, \"hello\", true)";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Mixed tuple parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");
   Assert.isTrue(Context.value_count(ctx) == 3, "Should have three values");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_TUPLE, "Value should be tuple");
   Assert.isTrue(stmt->value_len == 3, "Tuple should have 3 elements");

   Context.dispose(ctx);
}

// Test 10: Array with objects
static void test_parse_array_with_objects(void) {
   const char *source = "people := [{name := \"John\"}, {name := \"Jane\"}]";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Array with objects parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");
   Assert.isTrue(Context.value_count(ctx) == 2, "Should have two values (the objects)");
   Assert.isTrue(Context.field_count(ctx) == 2, "Should have two fields (one per object)");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_ARRAY, "Value should be array");
   Assert.isTrue(stmt->value_len == 2, "Array should have 2 elements");

   // Check that elements are objects
   value elem0 = Context.get_value(ctx, 0);
   Assert.isTrue(elem0->type == ANVL_VALUE_OBJECT, "Element 0 should be object");
   Assert.isTrue(elem0->pos == 0, "Element 0 should start at field index 0");
   Assert.isTrue(elem0->len == 1, "Element 0 should have 1 field");

   value elem1 = Context.get_value(ctx, 1);
   Assert.isTrue(elem1->type == ANVL_VALUE_OBJECT, "Element 1 should be object");
   Assert.isTrue(elem1->pos == 1, "Element 1 should start at field index 1");
   Assert.isTrue(elem1->len == 1, "Element 1 should have 1 field");

   Context.dispose(ctx);
}

// Test 11: Object with array field
static void test_parse_object_with_array(void) {
   const char *source = "config := {items := [1, 2, 3]}";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Object with array parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");
   Assert.isTrue(Context.value_count(ctx) == 3, "Should have three values (array elements)");
   Assert.isTrue(Context.field_count(ctx) == 1, "Should have one field");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_OBJECT, "Value should be object");
   Assert.isTrue(stmt->value_len == 1, "Object should have 1 field");

   // Check the field
   field fld = Context.get_field(ctx, 0);
   Assert.isNotNull(fld, "Field should exist");
   Assert.isTrue(fld->key_len == 5, "Field key should be 5 chars (items)");
   Assert.isTrue(fld->value_type == ANVL_VALUE_ARRAY, "Field value should be array");
   Assert.isTrue(fld->value_pos == 0, "Field value should start at value index 0");
   Assert.isTrue(fld->value_len == 3, "Field value should have 3 elements");

   Context.dispose(ctx);
}

// Test 12: Moderate stress test
static void test_parse_moderate_stress(void) {
   // Generate a moderately complex structure
   const char *source =
       "data := {\n"
       "  users := [\n"
       "    {id := 1, name := \"Alice\", scores := [95, 87, 92]},\n"
       "    {id := 2, name := \"Bob\", scores := [88, 91, 85]},\n"
       "    {id := 3, name := \"Charlie\", scores := [92, 89, 94]}\n"
       "  ],\n"
       "  metadata := {\n"
       "    total_users := 3,\n"
       "    average_score := 90.5,\n"
       "    tags := [\"student\", \"graded\", \"final\"]\n"
       "  }\n"
       "}\n";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created for moderate stress test");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Moderate stress test parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_OBJECT, "Value should be object");

   // Validate structure - just check that we have reasonable counts
   usize field_count = Context.field_count(ctx);
   usize value_count = Context.value_count(ctx);
   Assert.isTrue(field_count > 10, "Should have many fields");
   Assert.isTrue(value_count > 10, "Should have many values");
   Assert.isTrue(field_count == 14, "Should have exactly 14 fields");
   Assert.isTrue(value_count == 15, "Should have exactly 15 values");

   // Check users array field (field[9])
   field users_field = Context.get_field(ctx, 9);
   Assert.isTrue(users_field->value_type == ANVL_VALUE_ARRAY, "Users should be array");
   Assert.isTrue(users_field->value_pos == 0, "Users array should start at value index 0");
   Assert.isTrue(users_field->value_len == 3, "Users array should have 3 elements");

   // Check metadata object field (field[13])
   field meta_field = Context.get_field(ctx, 13);
   Assert.isTrue(meta_field->value_type == ANVL_VALUE_OBJECT, "Metadata should be object");

   Context.dispose(ctx);
}

static void test_parse_arrays_sample(void) {
   context ctx = test_parse_sample_file("arrays.anvl", ANVL_DIALECT_AML, 43, 5, 1);
   // Validate arrays.anvl specific content
   usize stmt_count = Context.statement_count(ctx);
   usize value_count = Context.value_count(ctx);
   usize field_count = Context.field_count(ctx);
   Assert.isTrue(stmt_count > 10, "arrays.anvl should have many statements");
   Assert.isTrue(value_count > 20, "arrays.anvl should have many values");
   Assert.isTrue(field_count > 0, "arrays.anvl should have some fields (from objects)");
   Context.dispose(ctx);
}

#if 1
static void test_parse_assignments_sample(void) {
   context ctx = test_parse_sample_file("assignments.anvl", ANVL_DIALECT_AML, 33, 5, 1);
   // validate some aspects of the parsed context
   usize stmt_count = Context.statement_count(ctx);
   usize field_count = Context.field_count(ctx);
   Assert.isTrue(stmt_count > 0, "assignments.anvl should have statements");
   Assert.isTrue(field_count > 0, "assignments.anvl should have fields");
   Context.dispose(ctx);
}

static void test_parse_attributes_sample(void) {
   context ctx = test_parse_sample_file("attributes.anvl", ANVL_DIALECT_AML, 56, 5, 1);
   // validate some aspects of the parsed context
   usize stmt_count = Context.statement_count(ctx);
   usize field_count = Context.field_count(ctx);
   Assert.isTrue(stmt_count > 0, "attributes.anvl should have statements");
   Assert.isTrue(field_count > 0, "attributes.anvl should have fields");
   Context.dispose(ctx);
}

static void test_parse_inherits_sample(void) {
   context ctx = test_parse_sample_file("inherits.anvl", ANVL_DIALECT_AML, 54, 5, 1);
   // validate some aspects of the parsed context
   usize stmt_count = Context.statement_count(ctx);
   usize field_count = Context.field_count(ctx);
   Assert.isTrue(stmt_count > 0, "inherits.anvl should have statements");
   Assert.isTrue(field_count > 0, "inherits.anvl should have fields");
   Context.dispose(ctx);
}

static void test_parse_modpack_sample(void) {
   context ctx = test_parse_sample_file("modpack.anvl", ANVL_DIALECT_AML, 164, 5, 1);
   // validate some aspects of the parsed context
   usize stmt_count = Context.statement_count(ctx);
   usize field_count = Context.field_count(ctx);
   Assert.isTrue(stmt_count > 0, "modpack.anvl should have statements");
   Assert.isTrue(field_count > 0, "modpack.anvl should have fields");
   Context.dispose(ctx);
}

static void test_parse_objects_sample(void) {
   context ctx = test_parse_sample_file("objects.anvl", ANVL_DIALECT_AML, 6, 5, 1);
   // Validate objects.anvl specific content
   usize stmt_count = Context.statement_count(ctx);
   usize field_count = Context.field_count(ctx);
   Assert.isTrue(stmt_count > 1, "objects.anvl should have statements");
   Assert.isTrue(field_count > 5, "objects.anvl should have many fields");
   // Check that some statements have object values
   bool has_object = false;
   for (usize i = 0; i < stmt_count; i++) {
      statement stmt = Context.get_statement(ctx, i);
      if (stmt && stmt->value_type == ANVL_VALUE_OBJECT) {
         has_object = true;
         break;
      }
   }
   Assert.isTrue(has_object, "objects.anvl should contain object values");
   Context.dispose(ctx);
}

static void test_parse_tuples_sample(void) {
   context ctx = test_parse_sample_file("tuples.anvl", ANVL_DIALECT_AML, 22, 5, 1);
   // Validate tuples.anvl specific content
   usize stmt_count = Context.statement_count(ctx);
   usize value_count = Context.value_count(ctx);
   Assert.isTrue(stmt_count > 3, "tuples.anvl should have several statements");
   Assert.isTrue(value_count > 10, "tuples.anvl should have many values (nested tuples)");
   // Check that some values are tuples
   bool has_tuple = false;
   for (usize i = 0; i < value_count; i++) {
      value val = Context.get_value(ctx, i);
      if (val && val->type == ANVL_VALUE_TUPLE) {
         has_tuple = true;
         break;
      }
   }
   Assert.isTrue(has_tuple, "tuples.anvl should contain tuple values");
   Context.dispose(ctx);
}

static void test_parse_generic_aurora(void) {
   context ctx = test_parse_sample_file("generic.aurora", ANVL_DIALECT_ASL, 0, 1, 1);
   // validate some aspects of the parsed context
   usize stmt_count = Context.statement_count(ctx);
   Assert.isTrue(true, "generic.aurora should parse without error");
   Context.dispose(ctx);
}
#endif

// Test 13: Stress test - Deep nesting
static void test_parse_deep_nesting(void) {
   // Generate deeply nested structure (10+ levels deep)
   char *source = generate_deep_nested_structure();

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created for deep structure");

   bool result = Context.parse(ctx);
   Assert.isTrue(result, "Deep nesting parsing should succeed");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");

   free(source);
   Context.dispose(ctx);
}

// Test 14: Stress test - Real world data conversion
static void test_parse_real_world_data(void) {
   // Fetch real data and convert to Anvil format
   char *source = fetch_and_convert_real_data();

   if (source) {
      anvl_ctx_builder_i *builder = Context.get_builder();
      builder->set_dialect(builder, ANVL_DIALECT_AML);
      builder->set_source(builder, source, strlen(source));

      context ctx = builder->build(builder);
      Assert.isNotNull(ctx, "Context should be created for real data");

      bool result = Context.parse(ctx);
      Assert.isTrue(result, "Real world data parsing should succeed");
      Assert.isTrue(Context.statement_count(ctx) > 0, "Should have statements");

      free(source);
      Context.dispose(ctx);
   } else {
      // If we can't fetch data, just pass the test
      Assert.isTrue(true, "Real data test skipped (no network)");
   }
}

__attribute__((constructor)) static void register_test_parser(void) {
   testset("Parser Tests", set_config, set_teardown);

   testcase("Empty Source", test_parse_empty_source);
   testcase("Simple Assignment", test_parse_simple_assignment);
   testcase("Multiple Statements", test_parse_multiple_statements);
   testcase("Number Parsing", test_parse_numbers);
   testcase("Array Parsing", test_parse_array);
   testcase("Tuple Parsing", test_parse_tuple);
   testcase("Object Parsing", test_parse_object);
   testcase("Mixed Array", test_parse_mixed_array);
   testcase("Nested Arrays", test_parse_nested_arrays);
   testcase("Mixed Tuple", test_parse_mixed_tuple);
   testcase("Array with Objects", test_parse_array_with_objects);
   testcase("Object with Array", test_parse_object_with_array);
   testcase("Moderate Stress Test", test_parse_moderate_stress);
   testcase("Arrays Sample", test_parse_arrays_sample);

#if 1
   testcase("Assignments Sample", test_parse_assignments_sample);
   testcase("Attributes Sample", test_parse_attributes_sample);
   testcase("Inherits Sample", test_parse_inherits_sample);
   testcase("Modpack Sample", test_parse_modpack_sample);
   testcase("Objects Sample", test_parse_objects_sample);
   testcase("Tuples Sample", test_parse_tuples_sample);
   testcase("Generic Aurora", test_parse_generic_aurora);
#endif

   testcase("Boolean and Null Parsing", test_parse_booleans_and_null);
   testcase("Bare Literals", test_parse_bare_literals);
   testcase("Module Attributes", test_parse_module_attributes);
   testcase("Deep Nesting", test_parse_deep_nesting);
   testcase("Real World Data", test_parse_real_world_data);
}

// Test sample files from test/samples/
static context test_parse_sample_file(const char *filename, anvl_dialect exp_dialect, usize exp_pos, usize exp_line, usize exp_col) {
   const char *filepath = get_source_path(filename);

   anvl_ctx_builder_i *builder = Context.get_builder();
   bool loaded = builder->load_file(builder, filepath);
   if (!loaded) {
      writelnf("[DEBUG]: Failed to load file: %s", filepath);
   }
   Assert.isTrue(loaded, "Sample file should load successfully");

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created for sample file");

   anvl_dialect act_dialect = Context.dialect(ctx);
   Assert.areEqual(&exp_dialect, &act_dialect, INT, "Dialect should match expected");

   // Check that source is positioned correctly (past shebang and comments)
   // Assert.areEqual(&exp_pos, &ctx->source->pos, INT, "Source should be positioned past preamble");
   // Assert.areEqual(&exp_line, &ctx->source->line, INT, "Source should be at correct line");
   // Assert.areEqual(&exp_col, &ctx->source->col, INT, "Source should be at correct column");

   bool result = Context.parse(ctx);
   if (!result) {
      // Check error state when parsing unexpectedly fails
      Assert.isTrue(Anvil.error_is_set(), "Error should be set when parsing fails");
      const anvl_error_state *err = Anvil.error_get();
      Assert.isNotNull((void *)err, "Error state should be available");
      writelnf("[DEBUG]: Unexpected parse failure for %s: %s at line %ld, col %ld\n",
               filename, err->message, err->line, err->column);
      Anvil.error_clear();
   }
   Assert.isTrue(result, "Sample file parsing should succeed");

   return ctx;
}