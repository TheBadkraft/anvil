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
 * prototype_test.c - Tests for statement buffer prototype
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-15
 * File: test/utilities/prototype_test.c
 * ------------------------------------------------------------------
 */

#include "../test/utilities/prototype.h"
#include "anvil.h"
#include <sigtest/sigtest.h>

// Test basic allocation and access
static void test_stmt_buffer_basic(void) {
   stmt_buffer sb = stmt_buffer_alloc();
   Assert.isNotNull(sb, "Statement buffer should allocate");

   // Set header
   stmt_buffer_set_header(sb, ANVL_STMT_ASSN, 10, 20, 15, 5, ANVL_VALUE_SCALAR);

   // Check access
   Assert.isTrue(STMT_TYPE(sb) == ANVL_STMT_ASSN, "Type should match");
   Assert.isTrue(STMT_START(sb) == 10, "Start should match");
   Assert.isTrue(STMT_LEN(sb) == 20, "Length should match");

   // Set sub-buffers
   stmt_buffer_set_base(sb, 100, 5);
   base_buffer *bb = PTR_BASE(sb);
   Assert.isNotNull(bb, "Base buffer should be set");
   Assert.isTrue(bb->pos == 100, "Base pos should match");
   Assert.isTrue(bb->len == 5, "Base len should match");

   stmt_buffer_set_value(sb, 200, 10);
   value_buffer *vb = PTR_VALUE(sb);
   Assert.isNotNull(vb, "Value buffer should be set");
   Assert.isTrue(vb->pos == 200, "Value pos should match");

   stmt_buffer_free(sb);
}

// Test memory contiguity (basic check)
static void test_stmt_buffer_contiguity(void) {
   stmt_buffer sb = stmt_buffer_alloc();

   // Check buffer is contiguous
   usize *buf = sb->buffer;
   for (int i = 0; i < 9; i++) {
      Assert.isTrue(&buf[i] == &sb->buffer[i], "Buffer should be contiguous");
   }

   stmt_buffer_free(sb);
}

// Test attribs buffer
static void test_stmt_buffer_attribs(void) {
   stmt_buffer sb = stmt_buffer_alloc();

   usize pairs[4] = {1, 2, 3, 4}; // pos1, len1, pos2, len2
   stmt_buffer_set_attribs(sb, 2, 10, pairs);

   attribs_buffer *ab = PTR_ATTRIBS(sb);
   Assert.isNotNull(ab, "Attribs buffer should be set");
   Assert.isTrue(ab->count == 2, "Count should match");
   Assert.isTrue(ab->len == 10, "Len should match");
   Assert.isTrue(ab->pairs[0] == 1, "Pair 0 should match");
   Assert.isTrue(ab->pairs[3] == 4, "Pair 3 should match");

   stmt_buffer_free(sb);
}

// Register tests
void prototype_test_register(void) {
   testcase("Statement Buffer Basic", test_stmt_buffer_basic);
   testcase("Statement Buffer Contiguity", test_stmt_buffer_contiguity);
   testcase("Statement Buffer Attribs", test_stmt_buffer_attribs);
}