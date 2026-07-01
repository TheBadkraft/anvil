/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2026-03-12
 * File: test/unit/test_errors.c
 * ------------------------------------------------------------------
 * Description:
 * Unit tests for errors.c — error state management and code utilities.
 * Converted from sigma.test to TestBit.
 * ------------------------------------------------------------------
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "testbit.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Shared teardown                                                     */
/* ------------------------------------------------------------------ */
static void td(void) { Anvil.error_clear(); }

/* ============================================================== */
/* ER01–ER04: error_set / error_get / error_is_set / error_clear  */
/* ============================================================== */

/* ER01 — initially no error is set */
static void test_er01_initial_state(void) {
    Anvil.error_clear();
    TestBit.is_false(Anvil.error_is_set(), "ER01: error not set initially");
    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_equal_int(ANVL_ERR_NONE, (long long)err->code,
                         "ER01: code is NONE initially");
}

/* ER02 — set an error and read it back */
static void test_er02_set_and_get(void) {
    anvl_error_set(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, "expected identifier", 3, 7, "test.anvl");
    TestBit.is_true(Anvil.error_is_set(), "ER02: error set after anvl_error_set");

    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_not_null((void *)err, "ER02: error state non-null");
    TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, (long long)err->code,
                         "ER02: code matches what was set");
    TestBit.is_equal_int(3, (long long)err->line, "ER02: line matches");
    TestBit.is_equal_int(7, (long long)err->column, "ER02: column matches");
    TestBit.is_equal_str("expected identifier", err->message,
                        "ER02: message matches");
}

/* ER03 — error_clear resets everything */
static void test_er03_clear(void) {
    anvl_error_set(ANVL_ERR_PARSER_EXPECTED_ASSIGN, "expected :=", 1, 5, "x.anvl");
    TestBit.is_true(Anvil.error_is_set(), "ER03: set before clear");
    Anvil.error_clear();
    TestBit.is_false(Anvil.error_is_set(), "ER03: not set after clear");
    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_equal_int(ANVL_ERR_NONE, (long long)err->code,
                         "ER03: code is NONE after clear");
}

/* ER04 — second set overwrites first */
static void test_er04_overwrite(void) {
    anvl_error_set(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, "first", 1, 1, "a.anvl");
    anvl_error_set(ANVL_ERR_PARSER_EXPECTED_VALUE, "second", 2, 2, "b.anvl");
    const anvl_error_state *err = Anvil.error_get();
    TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_VALUE, (long long)err->code,
                         "ER04: second set overwrites first");
    TestBit.is_equal_str("second", err->message,
                        "ER04: message is from second set");
}

/* ============================================================== */
/* ER05–ER07: error_code_name                                     */
/* ============================================================== */

/* ER05 — ANVL_ERR_NONE has a non-null name */
static void test_er05_code_name_none(void) {
    const char *name = anvl_error_code_name(ANVL_ERR_NONE);
    TestBit.is_not_null((void *)name, "ER05: NONE code name non-null");
}

/* ER06 — known parser code has correct name */
static void test_er06_code_name_known(void) {
    const char *name = anvl_error_code_name(ANVL_ERR_PARSER_UNTERMINATED_STRING);
    TestBit.is_not_null((void *)name, "ER06: name non-null for known code");
    TestBit.is_true(strlen(name) > 0, "ER06: name non-empty");
}

/* ER07 — unknown/out-of-range code returns non-null fallback */
static void test_er07_code_name_unknown(void) {
    const char *name = anvl_error_code_name((anvl_error_code)99999);
    TestBit.is_not_null((void *)name, "ER07: unknown code returns fallback, not NULL");
}

/* ============================================================== */
/* ER08–ER10: error_code_message                                  */
/* ============================================================== */

/* ER08 — ANVL_ERR_NONE has a message */
static void test_er08_code_message_none(void) {
    const char *msg = anvl_error_code_message(ANVL_ERR_NONE);
    TestBit.is_not_null((void *)msg, "ER08: NONE code message non-null");
}

/* ER09 — known code has a non-empty message */
static void test_er09_code_message_known(void) {
    const char *msg = anvl_error_code_message(ANVL_ERR_PARSER_UNTERMINATED_STRING);
    TestBit.is_not_null((void *)msg, "ER09: message non-null for known code");
    TestBit.is_true(strlen(msg) > 0, "ER09: message non-empty");
}

/* ER10 — code_message and code_name disagree (different purposes) */
static void test_er10_code_message_vs_name(void) {
    const char *name = anvl_error_code_name(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
    const char *msg = anvl_error_code_message(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER);
    TestBit.is_not_null((void *)name, "ER10: name non-null");
    TestBit.is_not_null((void *)msg, "ER10: message non-null");
    TestBit.is_true(name != msg, "ER10: name and message are distinct string literals");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(void) {
    TestBit.run_ex("ER01_initial_state",         NULL, test_er01_initial_state,         td);
    TestBit.run_ex("ER02_set_and_get",           NULL, test_er02_set_and_get,           td);
    TestBit.run_ex("ER03_clear",                 NULL, test_er03_clear,                 td);
    TestBit.run_ex("ER04_overwrite",             NULL, test_er04_overwrite,             td);
    TestBit.run_ex("ER05_code_name_none",        NULL, test_er05_code_name_none,        td);
    TestBit.run_ex("ER06_code_name_known",       NULL, test_er06_code_name_known,       td);
    TestBit.run_ex("ER07_code_name_unknown",     NULL, test_er07_code_name_unknown,     td);
    TestBit.run_ex("ER08_code_message_none",     NULL, test_er08_code_message_none,     td);
    TestBit.run_ex("ER09_code_message_known",    NULL, test_er09_code_message_known,    td);
    TestBit.run_ex("ER10_code_message_vs_name",  NULL, test_er10_code_message_vs_name,  td);

    return TestBit.report();
}
