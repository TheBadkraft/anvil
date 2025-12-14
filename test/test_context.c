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
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   // configure logging stream
   *logger = fopen("logs/test_context.log", "w");
   Memory.init();
}
static void set_teardown() {
   Anvil.cleanup();
   Memory.teardown();
}

static void test_context_get_builder(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isNotNull(builder, "Context.get_builder should return a valid builder interface");
}

static void test_ctx_bldr_set_dialect_default_asl(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Builder.build should return a valid context");
   Assert.isTrue(Context.dialect(ctx) == ANVL_DIALECT_ASL, "Context should default to ASL dialect");

   Context.dispose(ctx);
}

static void test_ctx_bldr_set_dialect_aml(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AML);

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Builder.build should return a valid context");
   // Note: context dialect now comes from source, not builder
   Assert.isNotNull(ctx->source, "Context should have a source object");

   Context.dispose(ctx);
}

static void test_ctx_bldr_set_dialect_asl(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_ASL);

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Builder.build should return a valid context");
   // Note: context dialect now comes from source, not builder
   Assert.isNotNull(ctx->source, "Context should have a source object");

   Context.dispose(ctx);
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

__attribute__((constructor)) static void register_test_context(void) {
   testset("Context Interface", set_config, set_teardown);

   testcase("Ctx: Get Builder", test_context_get_builder);
   testcase("Ctx Bldr: Default Dialect ASL", test_ctx_bldr_set_dialect_default_asl);
   testcase("Ctx Bldr: Set Dialect AML", test_ctx_bldr_set_dialect_aml);
   testcase("Ctx Bldr: Set Dialect ASL", test_ctx_bldr_set_dialect_asl);
   testcase("Ctx Bldr: Set Source", test_ctx_bldr_set_source);
   testcase("Ctx Bldr: Chain", test_ctx_bldr_chain);

   testcase("Ctx Bldr: Load File Shebang", test_ctx_bldr_load_file_shebang);
#if 0
   // Disabled to isolate memory leaks
   testcase("Ctx Bldr: Load File No Shebang", test_ctx_bldr_load_file_no_shebang);
#endif
}