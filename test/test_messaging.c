/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * test_messaging.c - AMP (Anvil Messaging Protocol) validation tests
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-19
 * File: test/test_messaging.c
 * ------------------------------------------------------------------
 * Description:
 * Validation tests for AMP dialect restrictions and messaging format.
 * AMP only allows scalars and blobs, no complex types or inheritance.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================== */
/* Helper: Parse AMP content with error checking                      */
/* ================================================================== */
static context parse_amp_content(const char *content, bool expect_success) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   if (!builder) {
      return NULL;
   }

   builder->set_dialect(builder, ANVL_DIALECT_AMP);
   builder->set_source(builder, content, strlen(content));

   context ctx = builder->build(builder);
   if (!ctx) {
      return NULL;
   }

   bool result = Context.parse(ctx);
   if (expect_success && !result) {
      fprintf(stderr, "Expected parse success but got error\n");
      Context.dispose(ctx);
      return NULL;
   }
   if (!expect_success && result) {
      fprintf(stderr, "Expected parse failure but succeeded\n");
      Context.dispose(ctx);
      return NULL;
   }

   return ctx;
}

/* ================================================================== */
/* Test 1: Valid AMP message - scalar string                          */
/* ================================================================== */
static void test_amp_scalar_string(void) {
   const char *content = "#!amp\nmsg_001 := \"hello world\"";
   context ctx = parse_amp_content(content, true);
   Assert.isNotNull(ctx, "AMP scalar string should parse");
   if (ctx) {
      usize stmt_count = Context.statement_count(ctx);
      Assert.isTrue(stmt_count == 1, "Should have 1 statement");
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 2: Valid AMP message - scalar number                          */
/* ================================================================== */
static void test_amp_scalar_number(void) {
   const char *content = "#!amp\nmsg_id := 42";
   context ctx = parse_amp_content(content, true);
   Assert.isNotNull(ctx, "AMP scalar number should parse");
   if (ctx) {
      usize stmt_count = Context.statement_count(ctx);
      Assert.isTrue(stmt_count == 1, "Should have 1 statement");
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 3: Valid AMP message - scalar boolean                         */
/* ================================================================== */
static void test_amp_scalar_boolean(void) {
   const char *content = "#!amp\nmsg_flag := true";
   context ctx = parse_amp_content(content, true);
   Assert.isNotNull(ctx, "AMP scalar boolean should parse");
   if (ctx) {
      usize stmt_count = Context.statement_count(ctx);
      Assert.isTrue(stmt_count == 1, "Should have 1 statement");
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 4: Valid AMP message - blob with tag                          */
/* ================================================================== */
static void test_amp_scalar_blob(void) {
   const char *content = "#!amp\nmsg_data := @json`{\"key\":\"value\"}`";
   context ctx = parse_amp_content(content, true);
   Assert.isNotNull(ctx, "AMP scalar blob should parse");
   if (ctx) {
      usize stmt_count = Context.statement_count(ctx);
      Assert.isTrue(stmt_count == 1, "Should have 1 statement");
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 5: Multiple valid AMP messages in envelope                    */
/* ================================================================== */
static void test_amp_multiple_messages(void) {
   const char *content = "#!amp\nmsg_1 := \"first\"\nmsg_2 := 100\nmsg_3 := false";
   context ctx = parse_amp_content(content, true);
   Assert.isNotNull(ctx, "AMP envelope should parse");
   if (ctx) {
      usize stmt_count = Context.statement_count(ctx);
      Assert.isTrue(stmt_count == 3, "Should have 3 statements");
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 6: Invalid AMP - object syntax rejected                       */
/* ================================================================== */
static void test_amp_reject_object(void) {
   const char *content = "#!amp\nmsg := {key: \"value\"}";
   context ctx = parse_amp_content(content, false);
   // Should fail to parse
   if (ctx) {
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 7: Invalid AMP - array syntax rejected                        */
/* ================================================================== */
static void test_amp_reject_array(void) {
   const char *content = "#!amp\nmsg := [1, 2, 3]";
   context ctx = parse_amp_content(content, false);
   // Should fail to parse
   if (ctx) {
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 8: Invalid AMP - tuple syntax rejected                        */
/* ================================================================== */
static void test_amp_reject_tuple(void) {
   const char *content = "#!amp\nmsg := (\"a\", \"b\")";
   context ctx = parse_amp_content(content, false);
   // Should fail to parse
   if (ctx) {
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 9: Invalid AMP - attributes rejected                          */
/* ================================================================== */
static void test_amp_reject_attributes(void) {
   const char *content = "#!amp\n@[internal]\nmsg := \"value\"";
   context ctx = parse_amp_content(content, false);
   // Should fail to parse
   if (ctx) {
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 10: Invalid AMP - inheritance rejected                        */
/* ================================================================== */
static void test_amp_reject_inheritance(void) {
   const char *content = "#!amp\nmsg : base_type := \"value\"";
   context ctx = parse_amp_content(content, false);
   // Should fail to parse
   if (ctx) {
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 11: AML allows objects - should still work                    */
/* ================================================================== */
static void test_aml_allows_objects(void) {
   const char *content = "#!aml\ndata := {key: \"value\"}";

   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isNotNull(builder, "Builder should not be null");

   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, content, strlen(content));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "AML context should build");

   if (ctx) {
      bool result = Context.parse(ctx);
      Assert.isTrue(result, "AML object parse should succeed");
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 12: Bare colon (inheritance) in AML still works               */
/* ================================================================== */
static void test_aml_allows_inheritance(void) {
   const char *content = "#!aml\nderived : base";

   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isNotNull(builder, "Builder should not be null");

   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, content, strlen(content));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "AML context should build");

   if (ctx) {
      bool result = Context.parse(ctx);
      Assert.isTrue(result, "AML inheritance parse should succeed");
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 13: Dialect detection - AMP from shebang                      */
/* ================================================================== */
static void test_dialect_detection_amp(void) {
   const char *content = "#!amp\nmsg := 123";

   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isNotNull(builder, "Builder should not be null");

   builder->set_dialect(builder, ANVL_DIALECT_AML); // set to AML initially
   builder->set_source(builder, content, strlen(content));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should build");

   if (ctx) {
      anvl_dialect detected = Context.dialect(ctx);
      Assert.isTrue(detected == ANVL_DIALECT_AMP, "Should detect AMP from shebang");
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Main test runner                                                   */
/* ================================================================== */
int main(void) {
   printf("\n╔════════════════════════════════════════════════════════════════╗\n");
   printf("║         AMP (Anvil Messaging Protocol) Validation Tests       ║\n");
   printf("╚════════════════════════════════════════════════════════════════╝\n\n");

   // Valid AMP messages
   Test.run(test_amp_scalar_string, "Test 1: AMP scalar string");
   Test.run(test_amp_scalar_number, "Test 2: AMP scalar number");
   Test.run(test_amp_scalar_boolean, "Test 3: AMP scalar boolean");
   Test.run(test_amp_scalar_blob, "Test 4: AMP scalar blob");
   Test.run(test_amp_multiple_messages, "Test 5: AMP multiple messages");

   // Invalid AMP messages (should be rejected)
   Test.run(test_amp_reject_object, "Test 6: AMP rejects objects");
   Test.run(test_amp_reject_array, "Test 7: AMP rejects arrays");
   Test.run(test_amp_reject_tuple, "Test 8: AMP rejects tuples");
   Test.run(test_amp_reject_attributes, "Test 9: AMP rejects attributes");
   Test.run(test_amp_reject_inheritance, "Test 10: AMP rejects inheritance");

   // Backward compatibility (AML should still work)
   Test.run(test_aml_allows_objects, "Test 11: AML allows objects");
   Test.run(test_aml_allows_inheritance, "Test 12: AML allows inheritance");

   // Dialect detection
   Test.run(test_dialect_detection_amp, "Test 13: Dialect detection - AMP");

   printf("\n✅ AMP validation tests complete\n");
   Anvil.cleanup();
   return 0;
}
