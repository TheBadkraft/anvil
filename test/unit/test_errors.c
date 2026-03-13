/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2026-03-12
 * File: test/unit/test_errors.c
 * ------------------------------------------------------------------
 * Description:
 * Unit tests for errors.c — error state management and code utilities.
 * ------------------------------------------------------------------
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   *logger = fopen("logs/test_errors.log", "w");
}
static void set_teardown() {
   Anvil.error_clear();
}

// ============================================================================
// ER01–ER04: error_set / error_get / error_is_set / error_clear
// ============================================================================

// ER01 — initially no error is set
static void test_error_initial_state(void) {
   Anvil.error_clear();
   Assert.isFalse(Anvil.error_is_set(), "Error should not be set initially");
   const anvl_error_state *err = Anvil.error_get();
   Assert.isTrue(err->code == ANVL_ERR_NONE, "Code should be NONE initially");
}

// ER02 — set an error and read it back
static void test_error_set_and_get(void) {
   anvl_error_set(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, "expected identifier", 3, 7, "test.anvl");
   Assert.isTrue(Anvil.error_is_set(), "Error should be set after anvl_error_set");

   const anvl_error_state *err = Anvil.error_get();
   Assert.isNotNull((void *)err, "Error state should not be NULL");
   Assert.isTrue(err->code == ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, "Code should match what was set");
   Assert.isTrue(err->line == 3, "Line should match");
   Assert.isTrue(err->column == 7, "Column should match");
   Assert.isTrue(strcmp(err->message, "expected identifier") == 0, "Message should match");
   Anvil.error_clear();
}

// ER03 — error_clear resets everything
static void test_error_clear(void) {
   anvl_error_set(ANVL_ERR_PARSER_EXPECTED_ASSIGN, "expected :=", 1, 5, "x.anvl");
   Assert.isTrue(Anvil.error_is_set(), "Should be set before clear");
   Anvil.error_clear();
   Assert.isFalse(Anvil.error_is_set(), "Should not be set after clear");
   const anvl_error_state *err = Anvil.error_get();
   Assert.isTrue(err->code == ANVL_ERR_NONE, "Code should be NONE after clear");
}

// ER04 — second set overwrites first
static void test_error_overwrite(void) {
   anvl_error_set(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, "first", 1, 1, "a.anvl");
   anvl_error_set(ANVL_ERR_PARSER_EXPECTED_VALUE, "second", 2, 2, "b.anvl");
   const anvl_error_state *err = Anvil.error_get();
   Assert.isTrue(err->code == ANVL_ERR_PARSER_EXPECTED_VALUE, "Second set should overwrite first");
   Assert.isTrue(strcmp(err->message, "second") == 0, "Message should be from second set");
   Anvil.error_clear();
}

// ============================================================================
// ER05–ER07: error_code_name
// ============================================================================

// ER05 — ANVL_ERR_NONE has a non-null name
static void test_error_code_name_none(void) {
   const char *name = anvl_error_code_name(ANVL_ERR_NONE);
   Assert.isNotNull((void *)name, "NONE code name should not be NULL");
}

// ER06 — known parser code has correct name
static void test_error_code_name_known(void) {
   const char *name = anvl_error_code_name(ANVL_ERR_PARSER_UNTERMINATED_STRING);
   Assert.isNotNull((void *)name, "Name should not be NULL for known code");
   Assert.isTrue(strlen(name) > 0, "Name should not be empty");
}

// ER07 — unknown/out-of-range code returns non-null fallback
static void test_error_code_name_unknown(void) {
   const char *name = anvl_error_code_name((anvl_error_code)99999);
   Assert.isNotNull((void *)name, "Unknown code should return a fallback string, not NULL");
}

// ============================================================================
// ER08–ER10: error_code_message
// ============================================================================

// ER08 — ANVL_ERR_NONE has a message
static void test_error_code_message_none(void) {
   const char *msg = anvl_error_code_message(ANVL_ERR_NONE);
   Assert.isNotNull((void *)msg, "NONE code message should not be NULL");
}

// ER09 — known code has a non-empty message
static void test_error_code_message_known(void) {
   const char *msg = anvl_error_code_message(ANVL_ERR_PARSER_UNTERMINATED_STRING);
   Assert.isNotNull((void *)msg, "Message should not be NULL for known code");
   Assert.isTrue(strlen(msg) > 0, "Message should not be empty");
}

// ER10 — code_message and code_name disagree (they serve different purposes)
static void test_error_code_message_vs_name(void) {
   const char *name = anvl_error_code_name(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
   const char *msg  = anvl_error_code_message(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
   Assert.isNotNull((void *)name, "Name should not be NULL");
   Assert.isNotNull((void *)msg,  "Message should not be NULL");
   // They are distinct strings serving different purposes
   Assert.isTrue(name != msg, "Name and message should be different string literals");
}

// ============================================================================
// Registration
// ============================================================================

__attribute__((constructor)) static void register_test_errors(void) {
   testset("Error Handling", set_config, set_teardown);

   testcase("ER01 Initial no-error state",       test_error_initial_state);
   testcase("ER02 Set and get error",             test_error_set_and_get);
   testcase("ER03 Clear resets state",            test_error_clear);
   testcase("ER04 Second set overwrites first",   test_error_overwrite);
   testcase("ER05 code_name for NONE",            test_error_code_name_none);
   testcase("ER06 code_name for known code",      test_error_code_name_known);
   testcase("ER07 code_name for unknown code",    test_error_code_name_unknown);
   testcase("ER08 code_message for NONE",         test_error_code_message_none);
   testcase("ER09 code_message for known code",   test_error_code_message_known);
   testcase("ER10 code_message vs code_name",     test_error_code_message_vs_name);
}
