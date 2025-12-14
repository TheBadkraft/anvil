/*
 * test_statements.c - Tests for statement buffer parsing
 */

#include "anvil.h"
#include "utilities/helpers.h"
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   // configure logging stream
   *logger = fopen("logs/test_statements.log", "w");
   Memory.init();
}

static void set_teardown() {
   Anvil.cleanup();
   Memory.teardown();
}

// Test 1: Basic statement creation and disposal
static void test_statement_create_dispose(void) {
   // Create a simple statement
   statement stmt = Memory.alloc(sizeof(struct anvl_statement), true);

   // Initialize basic fields
   stmt->type = ANVL_STMT_ASSN;
   stmt->stmt_len = 10;
   stmt->ident_pos = 0;
   stmt->ident_len = 4;
   stmt->attribs_len = 0;
   stmt->attribs_count = 0;
   stmt->value_type = ANVL_VALUE_SCALAR;
   stmt->value_pos = 7;
   stmt->value_len = 3;

   // Verify fields
   Assert.isTrue(stmt->type == ANVL_STMT_ASSN, "Statement type should be assignment");
   Assert.isTrue(stmt->stmt_len == 10, "Statement length should be 10");
   Assert.isTrue(stmt->ident_pos == 0, "Identifier position should be 0");
   Assert.isTrue(stmt->ident_len == 4, "Identifier length should be 4");
   Assert.isTrue(stmt->attribs_len == 0, "Attributes length should be 0");
   Assert.isTrue(stmt->attribs_count == 0, "Attributes count should be 0");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_SCALAR, "Value type should be scalar");
   Assert.isTrue(stmt->value_pos == 7, "Value position should be 7");
   Assert.isTrue(stmt->value_len == 3, "Value length should be 3");

   // Dispose
   Memory.dispose(stmt);
}

// Test 2: Context statement management
static void test_context_statement_management(void) {
   // Create context
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, "dummy", 5);
   context ctx = builder->build(builder);

   // Create a statement
   statement stmt = Memory.alloc(sizeof(struct anvl_statement), true);
   stmt->type = ANVL_STMT_ASSN;
   stmt->stmt_len = 15;
   stmt->ident_pos = 0;
   stmt->ident_len = 5;
   stmt->attribs_len = 0;
   stmt->attribs_count = 0;
   stmt->value_type = ANVL_VALUE_SCALAR;
   stmt->value_pos = 8;
   stmt->value_len = 7;

   // Add statement to context
   bool added = Context.add_statement(ctx, stmt);
   Assert.isTrue(added, "Should add statement successfully");

   // Check statement count
   usize count = Context.statement_count(ctx);
   Assert.isTrue(count == 1, "Should have 1 statement");

   // Check statement retrieval
   statement retrieved = Context.get_statement(ctx, 0);
   Assert.isNotNull(retrieved, "Statement should not be null");
   Assert.isTrue(retrieved == stmt, "Should return the same statement");
   Assert.isTrue(retrieved->type == ANVL_STMT_ASSN, "Should be assignment statement");
   Assert.isTrue(retrieved->ident_len == 5, "Identifier length should be 5");
   Assert.isTrue(retrieved->value_type == ANVL_VALUE_SCALAR, "Should be scalar value");

   Context.dispose(ctx);
}

// Test 3: Statement with attributes
static void test_statement_with_attributes(void) {
   // Create statement with attributes
   statement stmt = Memory.alloc(sizeof(struct anvl_statement), true);
   stmt->type = ANVL_STMT_ASSN;
   stmt->stmt_len = 25;
   stmt->ident_pos = 0;
   stmt->ident_len = 3;
   stmt->attribs_len = 15; // @[version="1.0"]
   stmt->attribs_count = 1;
   stmt->value_type = ANVL_VALUE_OBJECT;
   stmt->value_pos = 20;
   stmt->value_len = 2; // {}

   // Verify statement fields
   Assert.isTrue(stmt->type == ANVL_STMT_ASSN, "Should be assignment statement");
   Assert.isTrue(stmt->ident_len == 3, "Identifier should be 3 chars");
   Assert.isTrue(stmt->attribs_len == 15, "Attributes length should be 15");
   Assert.isTrue(stmt->attribs_count == 1, "Should have 1 attribute");
   Assert.isTrue(stmt->value_type == ANVL_VALUE_OBJECT, "Should be object value");
   Assert.isTrue(stmt->value_len == 2, "Value should be '{}' (2 chars)");

   Memory.dispose(stmt);
}

__attribute__((constructor)) static void register_test_statements(void) {
   testset("Statement Buffers", set_config, set_teardown);

   testcase("Create & Dispose", test_statement_create_dispose);
   testcase("Context Management", test_context_statement_management);
   testcase("Statement with Attributes", test_statement_with_attributes);
}