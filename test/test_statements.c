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
   stmt->meta[STMT_META_TYPE] = ANVL_STMT_ASSN;
   stmt->meta[STMT_META_LEN] = 10;
   stmt->meta[STMT_META_IDENT_POS] = 0;
   stmt->meta[STMT_META_IDENT_LEN] = 4;
   stmt->attrib_count = 0;
   stmt->meta[STMT_META_VALUE_TYPE] = ANVL_VALUE_SCALAR;
   stmt->value.scalar.pos = 7;
   stmt->value.scalar.len = 3;

   // Verify fields
   Assert.isTrue((anvl_stmt_type)stmt->meta[STMT_META_TYPE] == ANVL_STMT_ASSN, "Statement type should be assignment");
   Assert.isTrue(stmt->meta[STMT_META_LEN] == 10, "Statement length should be 10");
   Assert.isTrue(stmt->meta[STMT_META_IDENT_POS] == 0, "Identifier position should be 0");
   Assert.isTrue(stmt->meta[STMT_META_IDENT_LEN] == 4, "Identifier length should be 4");
   Assert.isTrue(stmt->attrib_count == 0, "Attributes count should be 0");
   Assert.isTrue((anvl_value_type)stmt->meta[STMT_META_VALUE_TYPE] == ANVL_VALUE_SCALAR, "Value type should be scalar");
   Assert.isTrue(stmt->value.scalar.pos == 7, "Value position should be 7");
   Assert.isTrue(stmt->value.scalar.len == 3, "Value length should be 3");

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
   stmt->meta[STMT_META_TYPE] = ANVL_STMT_ASSN;
   stmt->meta[STMT_META_LEN] = 15;
   stmt->meta[STMT_META_IDENT_POS] = 0;
   stmt->meta[STMT_META_IDENT_LEN] = 5;
   stmt->attrib_count = 0;
   stmt->meta[STMT_META_VALUE_TYPE] = ANVL_VALUE_SCALAR;
   stmt->value.scalar.pos = 8;
   stmt->value.scalar.len = 7;

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
   Assert.isTrue((anvl_stmt_type)retrieved->meta[STMT_META_TYPE] == ANVL_STMT_ASSN, "Should be assignment statement");
   Assert.isTrue(retrieved->meta[STMT_META_IDENT_LEN] == 5, "Identifier length should be 5");
   Assert.isTrue((anvl_value_type)retrieved->meta[STMT_META_VALUE_TYPE] == ANVL_VALUE_SCALAR, "Should be scalar value");

   Context.dispose(ctx);
}

// Test 4: Manual statement builder API
static void test_manual_statement_builder(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, "dummy", 5); // dummy source

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   // Manually build a statement
   stmt_builder bldr = Context.begin_statement(ctx, ANVL_STMT_ASSN);
   Assert.isNotNull(bldr, "Builder should be created");

   // Set identifier
   bldr->identifier(bldr, 0, 4); // "test" from dummy source

   // Set value
   bldr->begin_value(bldr, ANVL_VALUE_SCALAR);
   bldr->value_scalar(bldr, 0, 4); // "test"
   bldr->end_value(bldr);

   // End statement
   Context.end_statement(ctx, bldr, ANVL_STMT_ASSN);

   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have one statement");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue(Statement.type(stmt) == ANVL_STMT_ASSN, "Statement should be assignment");
   Assert.isTrue(Statement.value_type(stmt) == ANVL_VALUE_SCALAR, "Value should be scalar");

   Context.dispose(ctx);
}

// Test 4: Statement with attributes
static void test_statement_with_attributes(void) {
   // Create statement with attributes
   statement stmt = Memory.alloc(sizeof(struct anvl_statement), true);
   stmt->meta[STMT_META_TYPE] = ANVL_STMT_ASSN;
   stmt->meta[STMT_META_LEN] = 25;
   stmt->meta[STMT_META_IDENT_POS] = 0;
   stmt->meta[STMT_META_IDENT_LEN] = 3;
   stmt->attrib_count = 1;
   stmt->meta[STMT_META_VALUE_TYPE] = ANVL_VALUE_OBJECT;
   stmt->value.object.field_count = 0;
   stmt->value.object.fields = NULL;

   // Verify statement fields
   Assert.isTrue((anvl_stmt_type)stmt->meta[STMT_META_TYPE] == ANVL_STMT_ASSN, "Should be assignment statement");
   Assert.isTrue(stmt->meta[STMT_META_IDENT_LEN] == 3, "Identifier should be 3 chars");
   Assert.isTrue(stmt->attrib_count == 1, "Should have 1 attribute");
   Assert.isTrue((anvl_value_type)stmt->meta[STMT_META_VALUE_TYPE] == ANVL_VALUE_OBJECT, "Should be object value");
   Assert.isTrue(stmt->value.object.field_count == 0, "Object should have no fields");

   Memory.dispose(stmt);
}

__attribute__((constructor)) static void register_test_statements(void) {
   testset("Statement Buffers", set_config, set_teardown);

   testcase("Create & Dispose", test_statement_create_dispose);
   testcase("Context Management", test_context_statement_management);
   testcase("Statement with Attributes", test_statement_with_attributes);
   testcase("Manual Statement Builder", test_manual_statement_builder);
}