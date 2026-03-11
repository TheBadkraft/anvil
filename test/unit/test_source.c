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
#include <sigma.memory/memory.h>
#include <sigtest/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   // configure logging stream
   *logger = fopen("logs/test_source.log", "w");
}
static void set_teardown() {
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
static void test_source_dialect_invalid_shebang(void) {
   // Create source with invalid shebang
   const char *content = "#!invalid";
   usize len = strlen(content);

   source src = Source.create(content, len);

   anvl_dialect result = Source.dialect(src);
   Assert.isTrue(result == ANVL_DIALECT_ASL, "Source with invalid shebang should default to ASL dialect");

   Source.dispose(src);
}
static void test_source_get_position(void) {
   // always use valid minified content
   const char *content = "foo:=\"this is a test\", bar:=12345";
   usize len = strlen(content);

   source src = Source.create(content, len);

   // Initial position
   usize pos = Source.position(src);
   usize line = Source.line(src);
   usize col = Source.column(src);
   Assert.isTrue(pos == 0 && line == 1 && col == 1, "Initial position should be (0, 1, 1)");

   // Consume some characters
   Source.consume(src, 6); // Consume "foo:=\""

   pos = Source.position(src);
   line = Source.line(src);
   col = Source.column(src);
   Assert.isTrue(pos == 6 && line == 1 && col == 7, "Position after consuming 'foo:=\"' should be (6, 1, 7)");

   Source.dispose(src);
}

static void test_source_create(void) {
   // Test valid content
   const char *content = "test";
   usize len = strlen(content);
   source src = Source.create(content, len);
   Assert.isNotNull(src, "Source should be created with valid content");
   Source.dispose(src);

   // Test empty string
   src = Source.create("", 0);
   Assert.isNull(src, "Source should not be created with empty content");

   // Test null data with len > 0
   src = Source.create(NULL, 5);
   Assert.isNull(src, "Source should not be created with null data and len > 0");

   // Test null data with len 0
   src = Source.create(NULL, 0);
   Assert.isNull(src, "Source should not be created with null data and len 0");
}

static void test_source_is_eof(void) {
   const char *content = "ab";
   usize len = strlen(content);
   source src = Source.create(content, len);

   Assert.isFalse(Source.is_eof(src), "Should not be EOF at start");

   Source.consume(src, 2);
   Assert.isTrue(Source.is_eof(src), "Should be EOF after consuming all");

   Source.dispose(src);

   // Empty source
   src = Source.create("", 0);
   Assert.isNull(src, "Empty source is null");
}

static void test_source_is_eof_offset(void) {
   const char *content = "abc";
   usize len = strlen(content);
   source src = Source.create(content, len);

   Assert.isFalse(Source.is_eof_offset(src, 0), "Not EOF at offset 0");
   Assert.isFalse(Source.is_eof_offset(src, 2), "Not EOF at offset 2");
   Assert.isTrue(Source.is_eof_offset(src, 3), "EOF at offset 3");

   Source.dispose(src);
}

static void test_source_peek(void) {
   const char *content = "abc";
   usize len = strlen(content);
   source src = Source.create(content, len);

   Assert.isTrue(Source.peek(src) == 'a', "Peek should return 'a'");
   Source.consume(src, 1);
   Assert.isTrue(Source.peek(src) == 'b', "Peek should return 'b' after consume");

   Source.dispose(src);
}

static void test_source_peek_offset(void) {
   const char *content = "abc";
   usize len = strlen(content);
   source src = Source.create(content, len);

   Assert.isTrue(Source.peek_offset(src, 0) == 'a', "Peek offset 0 should be 'a'");
   Assert.isTrue(Source.peek_offset(src, 1) == 'b', "Peek offset 1 should be 'b'");
   Assert.isTrue(Source.peek_offset(src, 3) == '\0', "Peek beyond end should be '\\0'");

   Source.dispose(src);
}

static void test_source_match_length(void) {
   const char *content = "hello world";
   usize len = strlen(content);
   source src = Source.create(content, len);

   usize match = Source.match_length(src, "hello", 5);
   Assert.isTrue(match == 5, "Should match 'hello'");

   match = Source.match_length(src, "hell", 4);
   Assert.isTrue(match == 4, "Should match 'hell'");

   match = Source.match_length(src, "world", 5);
   Assert.isTrue(match == 0, "Should not match 'world' at start");

   Source.dispose(src);
}

static void test_source_match_operator(void) {
   const char *content = ":=";
   usize len = strlen(content);
   source src = Source.create(content, len);

   usize match = Source.match_operator(src, ":=", 2);
   Assert.isTrue(match == 2, "Should match ':='");

   Source.dispose(src);
}

static void test_source_is_alpha(void) {
   Assert.isTrue(Source.is_alpha('a'), "'a' is alpha");
   Assert.isTrue(Source.is_alpha('Z'), "'Z' is alpha");
   Assert.isTrue(Source.is_alpha('_'), "'_' is alpha");
   Assert.isFalse(Source.is_alpha('1'), "'1' is not alpha");
   Assert.isFalse(Source.is_alpha(' '), "' ' is not alpha");
}

static void test_source_is_digit(void) {
   Assert.isTrue(Source.is_digit('0'), "'0' is digit");
   Assert.isTrue(Source.is_digit('9'), "'9' is digit");
   Assert.isFalse(Source.is_digit('a'), "'a' is not digit");
   Assert.isFalse(Source.is_digit(' '), "' ' is not digit");
}

static void test_source_is_hex_digit(void) {
   Assert.isTrue(Source.is_hex_digit('0'), "'0' is hex digit");
   Assert.isTrue(Source.is_hex_digit('9'), "'9' is hex digit");
   Assert.isTrue(Source.is_hex_digit('a'), "'a' is hex digit");
   Assert.isTrue(Source.is_hex_digit('F'), "'F' is hex digit");
   Assert.isFalse(Source.is_hex_digit('g'), "'g' is not hex digit");
   Assert.isFalse(Source.is_hex_digit(' '), "' ' is not hex digit");
}

static void test_source_is_identifier_start(void) {
   Assert.isTrue(Source.is_identifier_start('a'), "'a' is identifier start");
   Assert.isTrue(Source.is_identifier_start('_'), "'_' is identifier start");
   Assert.isFalse(Source.is_identifier_start('1'), "'1' is not identifier start");
   Assert.isFalse(Source.is_identifier_start(' '), "' ' is not identifier start");
}

static void test_source_is_identifier_part(void) {
   Assert.isTrue(Source.is_identifier_part('a'), "'a' is identifier part");
   Assert.isTrue(Source.is_identifier_part('1'), "'1' is identifier part");
   Assert.isTrue(Source.is_identifier_part('.'), "'.' is identifier part");
   Assert.isFalse(Source.is_identifier_part(' '), "' ' is not identifier part");
}

static void test_source_consume(void) {
   const char *content = "a\nb";
   usize len = strlen(content);
   source src = Source.create(content, len);

   Source.consume(src, 2); // 'a\n'
   Assert.isTrue(Source.position(src) == 2, "Position should be 2");
   Assert.isTrue(Source.line(src) == 2, "Line should be 2");
   Assert.isTrue(Source.column(src) == 1, "Column should be 1");

   Source.dispose(src);
}

static void test_source_substring(void) {
   const char *content = "hello world";
   usize len = strlen(content);
   source src = Source.create(content, len);

   char *sub = Source.substring(src, 0, 5);
   Assert.isNotNull(sub, "Substring should be created");
   Assert.isTrue(strcmp(sub, "hello") == 0, "Substring should be 'hello'");
   Allocator.dispose(sub);

   sub = Source.substring(src, 6, 5);
   Assert.isTrue(strcmp(sub, "world") == 0, "Substring should be 'world'");
   Allocator.dispose(sub);

   sub = Source.substring(src, 100, 5);
   Assert.isNull(sub, "Substring beyond end should be null");

   Source.dispose(src);
}

static void test_source_skip_whitespace_and_comments(void) {
   const char *content = "  // comment\n/* block */  hello";
   usize len = strlen(content);
   source src = Source.create(content, len);

   Source.skip_whitespace_and_comments(src);
   Assert.isTrue(Source.peek(src) == 'h', "Should skip to 'h'");

   Source.dispose(src);
}

static void test_source_is_shebang(void) {
   const char *content1 = "#!asl";
   usize len1 = strlen(content1);
   source src1 = Source.create(content1, len1);
   Source.reset(src1); // Reset to test at start
   Assert.isTrue(Source.is_shebang(src1), "Should detect ASL shebang");
   Source.dispose(src1);

   const char *content2 = "#!aml";
   usize len2 = strlen(content2);
   source src2 = Source.create(content2, len2);
   Source.reset(src2);
   Assert.isTrue(Source.is_shebang(src2), "Should detect AML shebang");
   Source.dispose(src2);

   const char *content3 = "#!invalid";
   usize len3 = strlen(content3);
   source src3 = Source.create(content3, len3);
   Source.reset(src3);
   Assert.isFalse(Source.is_shebang(src3), "Should not detect invalid shebang");
   Source.dispose(src3);
}

static void test_source_parse_dialect(void) {
   const char *content1 = "#!aml";
   usize len1 = strlen(content1);
   source src1 = Source.create(content1, len1);
   Source.reset(src1);
   anvl_dialect d1 = Source.parse_dialect(src1, ANVL_DIALECT_ASL);
   Assert.isTrue(d1 == ANVL_DIALECT_AML, "Should parse AML");
   Source.dispose(src1);

   const char *content2 = "#!asl";
   usize len2 = strlen(content2);
   source src2 = Source.create(content2, len2);
   Source.reset(src2);
   anvl_dialect d2 = Source.parse_dialect(src2, ANVL_DIALECT_AML);
   Assert.isTrue(d2 == ANVL_DIALECT_ASL, "Should parse ASL");
   Source.dispose(src2);

   const char *content3 = "no shebang";
   usize len3 = strlen(content3);
   source src3 = Source.create(content3, len3);
   Source.reset(src3);
   anvl_dialect d3 = Source.parse_dialect(src3, ANVL_DIALECT_ASL);
   Assert.isTrue(d3 == ANVL_DIALECT_ASL, "Should return hint if no shebang");
   Source.dispose(src3);
}

static void test_source_set_position(void) {
   const char *content = "a\nb";
   usize len = strlen(content);
   source src = Source.create(content, len);

   Source.set_position(src, 2, 2, 1);
   Assert.isTrue(Source.position(src) == 2, "Position should be set to 2");
   Assert.isTrue(Source.line(src) == 2, "Line should be set to 2");
   Assert.isTrue(Source.column(src) == 1, "Column should be set to 1");

   Source.dispose(src);
}

static void test_source_reset(void) {
   const char *content = "hello";
   usize len = strlen(content);
   source src = Source.create(content, len);

   Source.consume(src, 3);
   Assert.isTrue(Source.position(src) == 3, "Position should be 3");

   Source.reset(src);
   Assert.isTrue(Source.position(src) == 0, "Position should be reset to 0");
   Assert.isTrue(Source.line(src) == 1, "Line should be reset to 1");
   Assert.isTrue(Source.column(src) == 1, "Column should be reset to 1");

   Source.dispose(src);
}

__attribute__((constructor)) static void register_test_source(void) {
   testset("Source Interface", set_config, set_teardown);

   testcase("Create", test_source_create);
   testcase("Is EOF", test_source_is_eof);
   testcase("Is EOF Offset", test_source_is_eof_offset);
   testcase("Peek", test_source_peek);
   testcase("Peek Offset", test_source_peek_offset);
   testcase("Match Length", test_source_match_length);
   testcase("Match Operator", test_source_match_operator);
   testcase("Is Alpha", test_source_is_alpha);
   testcase("Is Digit", test_source_is_digit);
   testcase("Is Hex Digit", test_source_is_hex_digit);
   testcase("Is Identifier Start", test_source_is_identifier_start);
   testcase("Is Identifier Part", test_source_is_identifier_part);
   testcase("Consume", test_source_consume);
   testcase("Substring", test_source_substring);
   testcase("Skip Whitespace and Comments", test_source_skip_whitespace_and_comments);
   testcase("Is Shebang", test_source_is_shebang);
   testcase("Parse Dialect", test_source_parse_dialect);
   testcase("Set Position", test_source_set_position);
   testcase("Reset", test_source_reset);

   testcase("Dialect: AML Shebang", test_source_dialect_aml_shebang);
   testcase("Dialect: Invalid Shebang", test_source_dialect_invalid_shebang);

   testcase("Get Position", test_source_get_position);
}