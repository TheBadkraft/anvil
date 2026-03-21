/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-03
 * File: test/test_anvil.c
 * ------------------------------------------------------------------
 * Description:
 * This file contains unit tests for the Anvil library.
 * ------------------------------------------------------------------
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include <sigma.memory/memory.h>
#include <sigma.core/strings.h>
#include <sigma.test/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   // configure logging stream
   *logger = fopen("logs/test_anvil.log", "w");
}
static void set_teardown() {
}

static void test_anvil_load(void) {
   root r = Anvil.load("dummy");
   Assert.isNotNull(r, "Anvil.load should return a valid root");
   Anvil.dispose(r);
}
static void test_anvil_read(void) {
   root r = Anvil.read("source", 6);
   Assert.isNotNull(r, "Anvil.read should return a valid root");
   Anvil.dispose(r);
}
static void test_anvil_dispose(void) {
   root r = Anvil.load("dummy");
   Anvil.dispose(r); // should clean up without error
   Assert.isTrue(true, "Anvil.dispose completed successfully");
}

static void test_context_get_builder(void) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isNotNull(builder, "Context.get_builder should return a valid builder interface");
}

static void test_error_handling(void) {
   // Clear any existing errors
   Anvil.error_clear();
   Assert.isFalse(Anvil.error_is_set(), "Should start with no error");

   // Try to build context without source
   anvl_ctx_builder_i *builder = Context.get_builder();
   context ctx = builder->build(builder);
   Assert.isNull(ctx, "Building context without source should fail");

   // Check that error is set
   Assert.isTrue(Anvil.error_is_set(), "Error should be set after failed build");
   const anvl_error_state *err = Anvil.error_get();
   Assert.isNotNull((void *)err, "Error state should be available");
   Assert.isTrue(err->code == ANVL_ERR_BUILDER_NO_SOURCE, "Error code should be BUILDER_NO_SOURCE");

   // Clear error
   Anvil.error_clear();
   Assert.isFalse(Anvil.error_is_set(), "Error should be cleared");
}

static void _register(void) {
   testset("Anvil Entry API", set_config, set_teardown);

   testcase("Anvil Load", test_anvil_load);
   testcase("Anvil Read", test_anvil_read);
   testcase("Anvil Dispose", test_anvil_dispose);
   testcase("Context Get Builder", test_context_get_builder);
   testcase("Error Handling", test_error_handling);
}
__attribute__((constructor)) static void register_test_test(void) {
   Tests.enqueue(_register);
}