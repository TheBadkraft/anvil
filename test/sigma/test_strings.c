/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_strings.c - Infrastructure tests: String and StringBuilder
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/infra/test_strings.c
 */
#include "sigma/strings.h"
#include "testbit.h"

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IS01 — String.length                                               */
/* ----------------------------------------------------------------- */
static void test_is01_string_length(void) {
    TestBit.is_equal_int(5, (long long)String.length("hello"), "IS01: length of 'hello' is 5");
    TestBit.is_equal_int(0, (long long)String.length(NULL),    "IS01: length of NULL is 0");
    TestBit.is_equal_int(0, (long long)String.length(""),      "IS01: length of empty string is 0");
}

/* ----------------------------------------------------------------- */
/* IS02 — String.dupe                                                 */
/* ----------------------------------------------------------------- */
static void test_is02_string_dupe(void) {
    string s = String.dupe("anvl");
    TestBit.is_not_null(s, "IS02: dupe returns non-null");
    TestBit.is_equal_int(0, (long long)String.compare(s, "anvl"), "IS02: dupe matches original");
    String.dispose(s);
}

/* ----------------------------------------------------------------- */
/* IS03 — String.concat                                               */
/* ----------------------------------------------------------------- */
static void test_is03_string_concat(void) {
    string s = String.concat("hello", " world");
    TestBit.is_not_null(s, "IS03: concat returns non-null");
    TestBit.is_equal_int(0, (long long)String.compare(s, "hello world"), "IS03: concat correct");
    String.dispose(s);
}

/* ----------------------------------------------------------------- */
/* IS04 — String.compare                                              */
/* ----------------------------------------------------------------- */
static void test_is04_string_compare(void) {
    TestBit.is_equal_int(0, (long long)String.compare("abc", "abc"), "IS04: equal strings → 0");
    TestBit.is_true(String.compare("abc", "abd") < 0,                "IS04: abc < abd");
    TestBit.is_true(String.compare("abd", "abc") > 0,                "IS04: abd > abc");
    TestBit.is_true(String.compare(NULL, "abc") < 0,                 "IS04: NULL < string");
    TestBit.is_true(String.compare("abc", NULL) > 0,                 "IS04: string > NULL");
}

/* ----------------------------------------------------------------- */
/* IS05 — StringBuilder basic append and toString                     */
/* ----------------------------------------------------------------- */
static void test_is05_stringbuilder_basic(void) {
    string_builder sb = StringBuilder.new(16);
    TestBit.is_not_null(sb, "IS05: StringBuilder.new returns non-null");

    StringBuilder.append(sb, "hello");
    StringBuilder.append(sb, " world");
    TestBit.is_equal_int(11, (long long)StringBuilder.length(sb), "IS05: length is 11");

    string result = StringBuilder.toString(sb);
    TestBit.is_not_null(result, "IS05: toString returns non-null");
    TestBit.is_equal_int(0, (long long)String.compare(result, "hello world"), "IS05: content correct");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

/* ----------------------------------------------------------------- */
/* IS06 — StringBuilder.clear                                         */
/* ----------------------------------------------------------------- */
static void test_is06_stringbuilder_clear(void) {
    string_builder sb = StringBuilder.new(16);
    StringBuilder.append(sb, "some content");
    StringBuilder.clear(sb);
    TestBit.is_equal_int(0, (long long)StringBuilder.length(sb), "IS06: length zero after clear");

    string result = StringBuilder.toString(sb);
    TestBit.is_equal_int(0, (long long)String.compare(result, ""), "IS06: empty after clear");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

int main(void) {
    TestBit.run_ex("IS01_string_length",       NULL, test_is01_string_length,       td);
    TestBit.run_ex("IS02_string_dupe",         NULL, test_is02_string_dupe,         td);
    TestBit.run_ex("IS03_string_concat",       NULL, test_is03_string_concat,       td);
    TestBit.run_ex("IS04_string_compare",      NULL, test_is04_string_compare,      td);
    TestBit.run_ex("IS05_stringbuilder_basic", NULL, test_is05_stringbuilder_basic, td);
    TestBit.run_ex("IS06_stringbuilder_clear", NULL, test_is06_stringbuilder_clear, td);

    return TestBit.report();
}