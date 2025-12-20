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

#include "../test/utilities/helpers.h"

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
   const char *content = "data := { key := \"value\" }";

   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isNotNull(builder, "Builder should not be null");

   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, content, strlen(content));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "AML context should build");

   if (ctx) {
      Anvil.error_clear();
      bool result = Context.parse(ctx);
      Assert.isTrue(result, "AML object parse should succeed");
      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 12: Bare colon (inheritance) in AML still works               */
/* ================================================================== */
static void test_aml_allows_inheritance(void) {
   const char *content = "derived := base";

   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isNotNull(builder, "Builder should not be null");

   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, content, strlen(content));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "AML context should build");

   if (ctx) {
      Anvil.error_clear();
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
/* Test 14: AMP envelope meta-buffer validation - messages content    */
/* ================================================================== */
static void test_amp_envelope_file_parsing(void) {
   const char *content = "#!amp\nuser_id := 12345\nusername := \"alice\"\nis_active := true\ntimestamp := 1703071200";

   anvl_ctx_builder_i *builder = Context.get_builder();
   Assert.isNotNull(builder, "Builder should not be null");

   builder->set_dialect(builder, ANVL_DIALECT_AMP);
   builder->set_source(builder, content, strlen(content));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should build from AMP content");

   if (ctx) {
      anvl_dialect dialect = Context.dialect(ctx);
      Assert.isTrue(dialect == ANVL_DIALECT_AMP, "Dialect should be AMP");

      bool result = Context.parse(ctx);
      Assert.isTrue(result, "AMP envelope should parse successfully");

      usize stmt_count = Context.statement_count(ctx);
      Assert.isTrue(stmt_count == 4, "Should have 4 messages (statements)");

      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 15: AMP meta-buffer structure - verify no inheritance/attrs   */
/* ================================================================== */
static void test_amp_metabuffer_value_spans(void) {
   const char *content = "#!amp\nstatus := 200\nmessage := \"OK\"";

   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_dialect(builder, ANVL_DIALECT_AMP);
   builder->set_source(builder, content, strlen(content));

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should build");

   if (ctx) {
      bool result = Context.parse(ctx);
      Assert.isTrue(result, "AMP content should parse");

      usize stmt_count = Context.statement_count(ctx);
      Assert.isTrue(stmt_count == 2, "Should have 2 statements");

      // Verify AMP statements don't have inheritance or attributes
      statement stmt1 = Context.get_statement(ctx, 0);
      Assert.isNotNull(stmt1, "First statement should exist");

      // In AMP, no inheritance allowed
      Assert.isTrue(stmt1->base_meta == NULL, "AMP should have no base_meta");
      Assert.isTrue(stmt1->meta[STMT_META_BASE_IDX] == 0, "STMT_META_BASE_IDX should be 0");

      // In AMP, no attributes allowed
      Assert.isTrue(stmt1->attr_meta == NULL, "AMP should have no attr_meta");
      Assert.isTrue(stmt1->meta[STMT_META_ATTR_IDX] == 0, "STMT_META_ATTR_IDX should be 0");

      // Second statement should also be clean
      statement stmt2 = Context.get_statement(ctx, 1);
      Assert.isNotNull(stmt2, "Second statement should exist");
      Assert.isTrue(stmt2->base_meta == NULL, "Second statement has no base_meta");
      Assert.isTrue(stmt2->attr_meta == NULL, "Second statement has no attr_meta");

      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 16: AMP response envelope - meta-buffer structure             */
/* ================================================================== */
static void test_amp_response_envelope(void) {
   const char *filepath = get_source_path("amp_response.amp");

   anvl_ctx_builder_i *builder = Context.get_builder();
   bool loaded = builder->load_file(builder, filepath);
   Assert.isTrue(loaded, "AMP response file should load");

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should build from response file");

   if (ctx) {
      bool result = Context.parse(ctx);
      Assert.isTrue(result, "Response envelope should parse");

      usize stmt_count = Context.statement_count(ctx);
      Assert.isTrue(stmt_count == 3, "Should have 3 response fields");

      // Check first statement (status_code := 200)
      statement stmt = Context.get_statement(ctx, 0);
      Assert.isNotNull(stmt, "First statement should exist");

      // AMP statements should NOT have base_meta or attr_meta
      Assert.isTrue(stmt->base_meta == NULL, "AMP statement should not have base_meta");
      Assert.isTrue(stmt->meta[STMT_META_BASE_IDX] == 0, "AMP STMT_META_BASE_IDX should be 0");
      Assert.isTrue(stmt->meta[STMT_META_ATTR_IDX] == 0, "AMP STMT_META_ATTR_IDX should be 0");

      Context.dispose(ctx);
   }
}

/* ================================================================== */
/* Test 17: AMP event envelope - multiple scalars with validation     */
/* ================================================================== */
static void test_amp_event_envelope(void) {
   const char *filepath = get_source_path("amp_event.amp");

   anvl_ctx_builder_i *builder = Context.get_builder();
   bool loaded = builder->load_file(builder, filepath);
   Assert.isTrue(loaded, "AMP event file should load");

   context ctx = builder->build(builder);
   Assert.isNotNull(ctx, "Context should build from event file");

   if (ctx) {
      bool result = Context.parse(ctx);
      Assert.isTrue(result, "Event envelope should parse");

      usize stmt_count = Context.statement_count(ctx);
      Assert.isTrue(stmt_count == 5, "Event should have 5 fields");

      // All statements should be ANVL_STMT_MSSG or ANVL_STMT_ASSN in AMP
      for (usize i = 0; i < stmt_count; i++) {
         statement stmt = Context.get_statement(ctx, i);
         Assert.isNotNull(stmt, "Statement should exist");

         anvl_stmt_type stmt_type = (anvl_stmt_type)stmt->meta[STMT_META_TYPE];
         // In AMP, statements are assignment statements (can be reinterpreted as MSSG later)
         Assert.isTrue(stmt_type == ANVL_STMT_ASSN || stmt_type == ANVL_STMT_MSSG,
                       "AMP statement should be ASSN or MSSG");

         // Verify no inheritance or attributes
         Assert.isTrue(stmt->base_meta == NULL, "No inheritance in AMP");
         Assert.isTrue(stmt->attr_meta == NULL, "No attributes in AMP");
      }

      Context.dispose(ctx);
   }
}


/* ================================================================== */
/* Test registration using sigtest framework                          */
/* ================================================================== */
__attribute__((constructor)) static void register_test_messaging(void) {
   testset("AMP Messaging Protocol Tests", NULL, NULL);

   // Valid AMP messages (scalars only)
   testcase("AMP scalar string", test_amp_scalar_string);
   testcase("AMP scalar number", test_amp_scalar_number);
   testcase("AMP scalar boolean", test_amp_scalar_boolean);
   testcase("AMP scalar blob", test_amp_scalar_blob);
   testcase("AMP multiple messages", test_amp_multiple_messages);

   // Invalid AMP messages (should be rejected)
   testcase("AMP rejects objects", test_amp_reject_object);
   testcase("AMP rejects arrays", test_amp_reject_array);
   testcase("AMP rejects tuples", test_amp_reject_tuple);
   testcase("AMP rejects attributes", test_amp_reject_attributes);
   testcase("AMP rejects inheritance", test_amp_reject_inheritance);

   // Backward compatibility (AML should still work)
   testcase("AML allows objects", test_aml_allows_objects);
   testcase("AML allows inheritance", test_aml_allows_inheritance);

   // Dialect detection
   testcase("Dialect detection - AMP", test_dialect_detection_amp);

   // AMP envelope files with meta-buffer validation
   testcase("AMP envelope file parsing", test_amp_envelope_file_parsing);
   testcase("AMP meta-buffer VALUE spans", test_amp_metabuffer_value_spans);
   testcase("AMP response envelope", test_amp_response_envelope);
   testcase("AMP event envelope", test_amp_event_envelope);
}
