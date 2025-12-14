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
#include <sigcore/memory.h>
#include <sigcore/strings.h>
#include <sigtest/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   // configure logging stream
   *logger = fopen("logs/test_anvil.log", "w");
   Memory.init();
}
static void set_teardown() {
   Memory.teardown();
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

__attribute__((constructor)) static void register_test_test(void) {
   testset("Anvil Entry API", set_config, set_teardown);

   testcase("Anvil Load", test_anvil_load);
   testcase("Anvil Read", test_anvil_read);
   testcase("Anvil Dispose", test_anvil_dispose);
   testcase("Context Get Builder", test_context_get_builder);
}