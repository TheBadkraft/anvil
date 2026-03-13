/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2026-03-12
 * File: test/unit/test_operators.c
 * ------------------------------------------------------------------
 * Description:
 * Unit tests for operators.c — operator lookup and identification.
 * ------------------------------------------------------------------
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   *logger = fopen("logs/test_operators.log", "w");
}
static void set_teardown() {
}

// ============================================================================
// OP01–OP04: anvl_operator_from_symbol
// ============================================================================

// OP01 — ":=" resolves to ANVL_OP_ASSIGN
static void test_operator_from_symbol_assign(void) {
   anvl_operator op = anvl_operator_from_symbol(":=", 2);
   Assert.isTrue(op == ANVL_OP_ASSIGN, "':=' should resolve to ANVL_OP_ASSIGN");
}

// OP02 — "=" resolves to ANVL_OP_EQUAL
static void test_operator_from_symbol_equal(void) {
   anvl_operator op = anvl_operator_from_symbol("=", 1);
   Assert.isTrue(op == ANVL_OP_EQUAL, "'=' should resolve to ANVL_OP_EQUAL");
}

// OP03 — "=>" resolves to ANVL_OP_ROCKET
static void test_operator_from_symbol_rocket(void) {
   anvl_operator op = anvl_operator_from_symbol("=>", 2);
   Assert.isTrue(op == ANVL_OP_ROCKET, "'=>' should resolve to ANVL_OP_ROCKET");
}

// OP04 — unknown symbol resolves to ANVL_OP_INVALID
static void test_operator_from_symbol_invalid(void) {
   anvl_operator op = anvl_operator_from_symbol("??", 2);
   Assert.isTrue(op == ANVL_OP_INVALID, "Unknown symbol should resolve to ANVL_OP_INVALID");
}

// ============================================================================
// OP05–OP07: anvl_operator_symbol / anvl_operator_length
// ============================================================================

// OP05 — ANVL_OP_ASSIGN symbol is ":="
static void test_operator_symbol_assign(void) {
   const char *sym = anvl_operator_symbol(ANVL_OP_ASSIGN);
   Assert.isNotNull((void *)sym, "ASSIGN symbol should not be NULL");
   Assert.isTrue(strcmp(sym, ":=") == 0, "ASSIGN symbol should be ':='");
}

// OP06 — ANVL_OP_ROCKET symbol is "=>"
static void test_operator_symbol_rocket(void) {
   const char *sym = anvl_operator_symbol(ANVL_OP_ROCKET);
   Assert.isNotNull((void *)sym, "ROCKET symbol should not be NULL");
   Assert.isTrue(strcmp(sym, "=>") == 0, "ROCKET symbol should be '=>'");
}

// OP07 — ANVL_OP_ASSIGN length is 2
static void test_operator_length_assign(void) {
   usize len = anvl_operator_length(ANVL_OP_ASSIGN);
   Assert.isTrue(len == 2, "ASSIGN operator length should be 2");
}

// ============================================================================
// OP08–OP10: anvl_is_operator
// ============================================================================

// OP08 — ":=" is recognised as an operator
static void test_is_operator_assign(void) {
   Assert.isTrue(anvl_is_operator(":=", 2), "':=' should be recognised as an operator");
}

// OP09 — "=>" is recognised as an operator
static void test_is_operator_rocket(void) {
   Assert.isTrue(anvl_is_operator("=>", 2), "'=>' should be recognised as an operator");
}

// OP10 — random non-operator text is not an operator
static void test_is_operator_false(void) {
   Assert.isFalse(anvl_is_operator("hello", 5), "'hello' should not be an operator");
   Assert.isFalse(anvl_is_operator("@", 1), "'@' should not be an operator");
}

// ============================================================================
// Registration
// ============================================================================

__attribute__((constructor)) static void register_test_operators(void) {
   testset("Operators", set_config, set_teardown);

   testcase("OP01 from_symbol := ASSIGN",         test_operator_from_symbol_assign);
   testcase("OP02 from_symbol = EQUAL",           test_operator_from_symbol_equal);
   testcase("OP03 from_symbol => ROCKET",         test_operator_from_symbol_rocket);
   testcase("OP04 from_symbol unknown INVALID",   test_operator_from_symbol_invalid);
   testcase("OP05 symbol for ASSIGN",             test_operator_symbol_assign);
   testcase("OP06 symbol for ROCKET",             test_operator_symbol_rocket);
   testcase("OP07 length for ASSIGN",             test_operator_length_assign);
   testcase("OP08 is_operator :=",                test_is_operator_assign);
   testcase("OP09 is_operator =>",                test_is_operator_rocket);
   testcase("OP10 is_operator rejects non-op",    test_is_operator_false);
}
