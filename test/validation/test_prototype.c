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
 * test_prototype.c - Validation of compact blob buffer layout prototype
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-15
 * File: test/validation/test_prototype.c
 * ------------------------------------------------------------------
 *
 * DESIGN NOTE:
 *   This file validates the prototype concept for compacting statement
 *   data into a flat usize[9] blob buffer. Each slot holds either a
 *   scalar field (type, position, length) or a pointer-as-integer to
 *   a heap-allocated sub-buffer (base, attribs, value). The goal is to
 *   explore whether the parser's statement representation can be reduced
 *   to a single contiguous allocation with pointer slots, minimising
 *   struct overhead and improving cache locality.
 *
 *   This is a concept validation, not coverage for production src/ code.
 *   If the compact-buffer approach is adopted, these tests should migrate
 *   to the appropriate unit test file and this file retired.
 * ------------------------------------------------------------------
 */

#include "../utilities/prototype.h"
#include "anvil.h"
#include <sigma.test/sigtest.h>

// ============================================================================
// Tests
// ============================================================================

// Validate basic allocation, header fields, and sub-buffer slot assignment
static void test_stmt_buffer_basic(void) {
   stmt_buffer sb = stmt_buffer_alloc();
   Assert.isNotNull(sb, "Statement buffer should allocate");

   stmt_buffer_set_header(sb, ANVL_STMT_ASSN, 10, 20, 15, 5, ANVL_VALUE_SCALAR);

   Assert.isTrue(STMT_TYPE(sb) == ANVL_STMT_ASSN, "Type should match");
   Assert.isTrue(STMT_START(sb) == 10, "Start should match");
   Assert.isTrue(STMT_LEN(sb) == 20, "Length should match");

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

// Verify the flat buffer is truly contiguous in memory
static void test_stmt_buffer_contiguity(void) {
   stmt_buffer sb = stmt_buffer_alloc();

   usize *buf = sb->buffer;
   for (int i = 0; i < 9; i++) {
      Assert.isTrue(&buf[i] == &sb->buffer[i], "Buffer should be contiguous");
   }

   stmt_buffer_free(sb);
}

// Validate attribs sub-buffer slot: count, len, and variadic pairs
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

// ============================================================================
// Registration
// ============================================================================

static void _register(void) {
   testset("Compact Blob Buffer Prototype", NULL, NULL);

   testcase("PB01 Header fields and sub-buffer slots", test_stmt_buffer_basic);
   testcase("PB02 Flat buffer memory contiguity", test_stmt_buffer_contiguity);
   testcase("PB03 Attribs sub-buffer pairs", test_stmt_buffer_attribs);
}
__attribute__((constructor)) static void register_test_prototype(void) {
   Tests.enqueue(_register);
}
