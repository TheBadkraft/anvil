/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2026-03-12
 * File: test/unit/test_symbols.c
 * ------------------------------------------------------------------
 * Description:
 * Unit tests for symbols.c — symbol lookup and identification.
 * ------------------------------------------------------------------
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   *logger = fopen("logs/test_symbols.log", "w");
}
static void set_teardown() {
}

// ============================================================================
// SY01–SY05: anvl_symbol_from_symbol
// ============================================================================

// SY01 — "@" resolves to ANVL_SYM_AT
static void test_symbol_from_symbol_at(void) {
   anvl_symbol sym = anvl_symbol_from_symbol("@", 1);
   Assert.isTrue(sym == ANVL_SYM_AT, "'@' should resolve to ANVL_SYM_AT");
}

// SY02 — "{" resolves to ANVL_SYM_L_BRACE
static void test_symbol_from_symbol_lbrace(void) {
   anvl_symbol sym = anvl_symbol_from_symbol("{", 1);
   Assert.isTrue(sym == ANVL_SYM_L_BRACE, "'{' should resolve to ANVL_SYM_L_BRACE");
}

// SY03 — "}" resolves to ANVL_SYM_R_BRACE
static void test_symbol_from_symbol_rbrace(void) {
   anvl_symbol sym = anvl_symbol_from_symbol("}", 1);
   Assert.isTrue(sym == ANVL_SYM_R_BRACE, "'}' should resolve to ANVL_SYM_R_BRACE");
}

// SY04 — "," resolves to ANVL_SYM_COMMA
static void test_symbol_from_symbol_comma(void) {
   anvl_symbol sym = anvl_symbol_from_symbol(",", 1);
   Assert.isTrue(sym == ANVL_SYM_COMMA, "',' should resolve to ANVL_SYM_COMMA");
}

// SY05 — unknown character resolves to ANVL_SYM_INVALID
static void test_symbol_from_symbol_invalid(void) {
   anvl_symbol sym = anvl_symbol_from_symbol("?", 1);
   Assert.isTrue(sym == ANVL_SYM_INVALID, "'?' should resolve to ANVL_SYM_INVALID");
}

// ============================================================================
// SY06–SY08: anvl_symbol_symbol / anvl_symbol_length
// ============================================================================

// SY06 — ANVL_SYM_AT symbol string is "@"
static void test_symbol_symbol_at(void) {
   const char *s = anvl_symbol_symbol(ANVL_SYM_AT);
   Assert.isNotNull((void *)s, "AT symbol string should not be NULL");
   Assert.isTrue(strcmp(s, "@") == 0, "AT symbol string should be '@'");
}

// SY07 — ANVL_SYM_L_BRACKET symbol string is "["
static void test_symbol_symbol_lbracket(void) {
   const char *s = anvl_symbol_symbol(ANVL_SYM_L_BRACKET);
   Assert.isNotNull((void *)s, "L_BRACKET symbol string should not be NULL");
   Assert.isTrue(strcmp(s, "[") == 0, "L_BRACKET symbol string should be '['");
}

// SY08 — ANVL_SYM_COMMA length is 1
static void test_symbol_length_comma(void) {
   usize len = anvl_symbol_length(ANVL_SYM_COMMA);
   Assert.isTrue(len == 1, "COMMA symbol length should be 1");
}

// ============================================================================
// SY09–SY10: anvl_is_symbol
// ============================================================================

// SY09 — known single-char symbols are recognised
static void test_is_symbol_known(void) {
   Assert.isTrue(anvl_is_symbol("@", 1),  "'@' should be a symbol");
   Assert.isTrue(anvl_is_symbol("{", 1),  "'{' should be a symbol");
   Assert.isTrue(anvl_is_symbol("}", 1),  "'}' should be a symbol");
   Assert.isTrue(anvl_is_symbol("[", 1),  "'[' should be a symbol");
   Assert.isTrue(anvl_is_symbol("]", 1),  "']' should be a symbol");
   Assert.isTrue(anvl_is_symbol("(", 1),  "'(' should be a symbol");
   Assert.isTrue(anvl_is_symbol(")", 1),  "')' should be a symbol");
   Assert.isTrue(anvl_is_symbol(",", 1),  "',' should be a symbol");
}

// SY10 — non-symbol text is rejected
static void test_is_symbol_false(void) {
   Assert.isFalse(anvl_is_symbol("hello", 5), "'hello' should not be a symbol");
   Assert.isFalse(anvl_is_symbol(":=", 2),    "':=' should not be a symbol");
   Assert.isFalse(anvl_is_symbol("?", 1),     "'?' should not be a symbol");
}

// ============================================================================
// Registration
// ============================================================================

__attribute__((constructor)) static void register_test_symbols(void) {
   testset("Symbols", set_config, set_teardown);

   testcase("SY01 from_symbol @",                test_symbol_from_symbol_at);
   testcase("SY02 from_symbol {",                test_symbol_from_symbol_lbrace);
   testcase("SY03 from_symbol }",                test_symbol_from_symbol_rbrace);
   testcase("SY04 from_symbol ,",                test_symbol_from_symbol_comma);
   testcase("SY05 from_symbol unknown INVALID",  test_symbol_from_symbol_invalid);
   testcase("SY06 symbol string for AT",         test_symbol_symbol_at);
   testcase("SY07 symbol string for L_BRACKET",  test_symbol_symbol_lbracket);
   testcase("SY08 symbol length for COMMA",      test_symbol_length_comma);
   testcase("SY09 is_symbol known chars",        test_is_symbol_known);
   testcase("SY10 is_symbol rejects non-symbol", test_is_symbol_false);
}
