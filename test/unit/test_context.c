/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-13
 * File: test/test_context.c
 * ------------------------------------------------------------------
 * Description:
 * This file contains unit tests for the Anvil context interface.
 * ------------------------------------------------------------------
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "utilities/helpers.h"
#include <sigma.memory/memory.h>
#include <sigtest/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   // configure logging stream
   *logger = fopen("logs/test_context.log", "w");
}
static void set_teardown() {
   Anvil.cleanup();
}
static void teardown() {
   const anvl_error_state *err = Anvil.error_get();
   if (err->message)
      writelnf("Error: %s", err->message);

   Anvil.error_clear();
}

static void test_context_get_builder(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isNotNull(builder, "Context.get_builder should return a valid builder interface");
}

static void test_ctx_bldr_null_no_source(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   context ctx = builder->build(builder);
   Assert.isNull(ctx, "Builder.build should return a NULL context");

   // get error state
   const anvl_error_state *err = Anvil.error_get();
   Assert.isTrue(err != NULL, "Error state should be set after failed build");
   Assert.isTrue(err->code == ANVL_ERR_BUILDER_NO_SOURCE, "Error code should indicate invalid state");
   teardown();
}

static void test_ctx_bldr_set_source(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   const char *aml_source = "#!aml";
   usize len = strlen(aml_source);
   builder->set_source(builder, aml_source, len);

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Builder.build should return a valid context");
   Assert.isTrue(Context.dialect(ctx) == ANVL_DIALECT_AML, "Context should detect AML from shebang");
   Assert.isNotNull(ctx->source, "Context should have a source object");
   Assert.isTrue(memcmp(ctx->source->buffer.bucket, aml_source, len) == 0, "Context source data should match set source");

   usize actual_len = ctx->source->buffer.end - ctx->source->buffer.bucket;
   Assert.isTrue(actual_len == len, "Context source length should match set len");

   Context.dispose(ctx);
   teardown();
}

static void test_ctx_bldr_load_file_shebang(void) {
   const char *file_path = get_source_path("shebang.anvl");
   writelnf("Loading file: %s", file_path);

   anvl_ctx_builder_i *builder = Context.get_builder();
   bool loaded = builder->load_file(builder, file_path);
   Assert.isTrue(loaded, "Should load shebang.anvl file");

   if (!loaded)
      return; // skip rest if loading failed

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Builder.build should return a valid context");
   Assert.isTrue(Context.dialect(ctx) == ANVL_DIALECT_AML, "Context should detect AML from shebang in file");
   Assert.isNotNull(ctx->source, "Context should have a source object");

   Context.dispose(ctx);
   teardown();
}

static void test_ctx_bldr_load_file_no_shebang(void) {
   const char *file_path = get_source_path("generic.aurora");
   writelnf("Loading file: %s", file_path);

   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isTrue(builder->load_file(builder, file_path), "Should load generic.aurora file");

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Builder.build should return a valid context");
   Assert.isTrue(Context.dialect(ctx) == ANVL_DIALECT_ASL, "Context should default to ASL dialect");
   Assert.isNotNull(ctx->source, "Context should have a source object");

   Context.dispose(ctx);
}

static void test_ctx_bldr_chain(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   // Set user preference to ASL
   builder->set_dialect(builder, ANVL_DIALECT_ASL);
   // But source has AML shebang - shebang should win
   const char *aml_source = "#!aml";
   usize len = strlen(aml_source);
   builder->set_source(builder, aml_source, len);

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Builder.build should return a valid context");
   Assert.isTrue(Context.dialect(ctx) == ANVL_DIALECT_AML, "Shebang should override user dialect setting");
   Assert.isNotNull(ctx->source, "Context should have a source object");
   Assert.isTrue(memcmp(ctx->source->buffer.bucket, aml_source, len) == 0, "Context source should match set source");

   usize actual_len = ctx->source->buffer.end - ctx->source->buffer.bucket;
   Assert.isTrue(actual_len == len, "Context source length should match set len");

   Context.dispose(ctx);
   teardown();
}

static void test_context_statement_operations(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   const char *source_str = "test := value";
   builder->set_source(builder, source_str, strlen(source_str));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   // Initially no statements
   Assert.isTrue(Context.statement_count(ctx) == 0, "Should start with no statements");
   Assert.isNull(Context.get_statement(ctx, 0), "Getting statement 0 should return NULL");

   // Create a dummy statement (in real parser, this would be built properly)
   statement stmt = Allocator.alloc(sizeof(struct anvl_statement));
   if (stmt) memset(stmt, 0, sizeof(struct anvl_statement));
   stmt->meta[STMT_META_TYPE] = ANVL_STMT_ASSN;
   stmt->meta[STMT_META_IDENT_POS] = 0;
   stmt->meta[STMT_META_IDENT_LEN] = 4; // "test"
   stmt->meta[STMT_META_BASE_IDX] = 0;  // no base
   stmt->meta[STMT_META_ATTR_IDX] = 0;  // no attributes
   stmt->meta[STMT_META_VALUE_IDX] = 0; // value index (placeholder)
   // NOTE: Statement length is now in value_meta->len (not in meta[STMT_META_LEN] which was removed)

   // Add statement
   bool added = Context.add_statement(ctx, stmt);
   Assert.isTrue(added, "Should add statement successfully");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have 1 statement");
   Assert.isNotNull(Context.get_statement(ctx, 0), "Should get the statement back");
   Assert.isTrue((anvl_stmt_type)Context.get_statement(ctx, 0)->meta[STMT_META_TYPE] == ANVL_STMT_ASSN, "Statement type should match");

   Context.dispose(ctx);
   teardown();
}

static void test_context_attribute_operations(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   const char *source = "test";
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   // Initially no attributes
   Assert.isTrue(Context.attribute_count(ctx) == 0, "Should start with no attributes");
   Assert.isNull(Context.get_attribute(ctx, 0), "Getting attribute 0 should return NULL");

   // Create a dummy attribute
   attribute attr = Allocator.alloc(sizeof(struct anvl_attribute));
   if (attr) memset(attr, 0, sizeof(struct anvl_attribute));
   attr->key_pos = 0;
   attr->key_len = 4; // "test"
   attr->value_pos = 0;
   attr->value_len = 0; // no value

   // Add attribute
   bool added = Context.add_attribute(ctx, attr);
   Assert.isTrue(added, "Should add attribute successfully");
   Assert.isTrue(Context.attribute_count(ctx) == 1, "Should have 1 attribute");
   Assert.isNotNull(Context.get_attribute(ctx, 0), "Should get the attribute back");
   Assert.isTrue(Context.get_attribute(ctx, 0)->key_len == 4, "Attribute key_len should match");

   Context.dispose(ctx);
   teardown();
}

static void test_context_parse_basic(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   const char *source = "name := \"John\"";
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   // Parse
   bool parsed = Context.parse(ctx);
   Assert.isTrue(parsed, "Should parse successfully");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have 1 statement after parsing");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "Statement should exist");
   Assert.isTrue((anvl_stmt_type)stmt->meta[STMT_META_TYPE] == ANVL_STMT_ASSN, "Should be assignment");
   Assert.isTrue(stmt->meta[STMT_META_IDENT_LEN] == 4, "Identifier should be 'name'");
   // TODO: Value type check when value_meta is implemented
   // Assert.isTrue((anvl_value_type)stmt->meta[STMT_META_VALUE_IDX] == ANVL_VALUE_SCALAR, "Value should be scalar");

   Context.dispose(ctx);
   teardown();
}

static void test_context_multiple_statements(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   const char *source = "name := \"John\"\nage := 30\nactive := true";
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool parsed = Context.parse(ctx);
   Assert.isTrue(parsed, "Should parse multiple statements successfully");
   Assert.isTrue(Context.statement_count(ctx) == 3, "Should have 3 statements");

   // Check each statement
   statement stmt1 = Context.get_statement(ctx, 0);
   Assert.isTrue(stmt1->meta[STMT_META_IDENT_LEN] == 4, "First statement identifier length");
   statement stmt2 = Context.get_statement(ctx, 1);
   Assert.isTrue(stmt2->meta[STMT_META_IDENT_LEN] == 3, "Second statement identifier length");
   statement stmt3 = Context.get_statement(ctx, 2);
   Assert.isTrue(stmt3->meta[STMT_META_IDENT_LEN] == 6, "Third statement identifier length");

   Context.dispose(ctx);
   teardown();
}

static void test_context_get_statement_bounds(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   const char *source = "test := value";
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   bool parsed = Context.parse(ctx);
   Assert.isTrue(parsed, "Should parse successfully");

   // Test bounds
   Assert.isNotNull(Context.get_statement(ctx, 0), "Should get statement 0");
   Assert.isNull(Context.get_statement(ctx, 1), "Should return NULL for out of bounds");
   Assert.isNull(Context.get_statement(ctx, -1), "Should return NULL for negative index");

   Context.dispose(ctx);
   teardown();
}

static void test_context_add_statement_null(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   const char *source = "test";
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should be created");

   // Try to add NULL statement
   bool added = Context.add_statement(ctx, NULL);
   Assert.isFalse(added, "Should not add NULL statement");

   // Try to add to NULL context
   added = Context.add_statement(NULL, NULL);
   Assert.isFalse(added, "Should not add to NULL context");

   Context.dispose(ctx);
   teardown();
}
#if 0
#endif

__attribute__((constructor)) static void register_test_context(void) {
   testset("Context Interface", set_config, set_teardown);

   testcase("Ctx: Get Builder", test_context_get_builder);
   testcase("Ctx Bldr: Err Null No Source", test_ctx_bldr_null_no_source);

   testcase("Ctx Bldr: Set Source", test_ctx_bldr_set_source);
   testcase("Ctx Bldr: Load File Shebang", test_ctx_bldr_load_file_shebang);
   testcase("Ctx Bldr: Load File No Shebang", test_ctx_bldr_load_file_no_shebang);

   testcase("Ctx Bldr: Chain", test_ctx_bldr_chain);

   testcase("Ctx: Statement Operations", test_context_statement_operations);
   testcase("Ctx: Attribute Operations", test_context_attribute_operations);
   testcase("Ctx: Parse Basic", test_context_parse_basic);
   testcase("Ctx: Parse Multiple Statements", test_context_multiple_statements);
   testcase("Ctx: Get Statement Bounds", test_context_get_statement_bounds);
   testcase("Ctx: Add Statement Null", test_context_add_statement_null);
}