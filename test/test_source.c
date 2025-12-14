/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-13
 * File: test/test_source.c
 * ------------------------------------------------------------------
 * Description:
 * This file contains unit tests for the Anvil source interface.
 * ------------------------------------------------------------------
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "utils.h"
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>
#include <stdlib.h>
#include <string.h>

static void set_config(FILE **logger) {
   // configure logging stream
   *logger = fopen("logs/test_source.log", "w");
   Memory.init();
}
static void set_teardown() {
   Memory.teardown();
}

// Source dialect detection tests
static void test_source_dialect_aml_shebang(void) {
   // Create source with AML shebang
   const char *content = "#!aml";
   usize len = strlen(content);

   source src = Source.create(content, len);

   anvl_dialect result = Source.dialect(src);
   Assert.isTrue(result == ANVL_DIALECT_AML, "Source with #!aml shebang should detect AML dialect");

   Source.dispose(src);
}

/*
static void test_source_dialect_asl_shebang(void) {
   // Create source with ASL shebang
   const char *content = "#!asl";
   usize len = strlen(content);

   source src = Source.create(content, len);

   anvl_dialect result = Source.dialect(src);
   Assert.isTrue(result == ANVL_DIALECT_AML, "Source with #!asl shebang should detect ASL dialect");

   Source.dispose(src);
}
*/

static void test_source_dialect_invalid_shebang(void) {
   // Create source with invalid shebang
   const char *content = "#!invalid";
   usize len = strlen(content);

   source src = Source.create(content, len);

   anvl_dialect result = Source.dialect(src);
   Assert.isTrue(result == ANVL_DIALECT_ASL, "Source with invalid shebang should default to ASL dialect");

   Source.dispose(src);
}

__attribute__((constructor)) static void register_test_source(void) {
   testset("Source Interface", set_config, set_teardown);

   testcase("Source Dialect AML Shebang", test_source_dialect_aml_shebang);
   testcase("Source Dialect Invalid Shebang", test_source_dialect_invalid_shebang);
}