/*
 * test_interrogators.c - Tests for source interrogator functions
 */

#include "anvil.h"
#include "utilities/helpers.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   // configure logging stream
   *logger = fopen("logs/test_interrogators.log", "w");
}

static void set_teardown() {
   Anvil.cleanup();
}

// Test basic position and EOF functions
static void test_position_and_eof(void) {
   source src = Source.create("hello", 5);

   Assert.isTrue(Source.position(src) == 0, "Initial position should be 0");
   Assert.isTrue(Source.line(src) == 1, "Initial line should be 1");
   Assert.isTrue(Source.column(src) == 1, "Initial column should be 1");
   Assert.isFalse(Source.is_eof(src), "Should not be EOF at start");
   Assert.isTrue(Source.is_eof_offset(src, 5), "Should be EOF at offset 5");
   Assert.isFalse(Source.is_eof_offset(src, 4), "Should not be EOF at offset 4");

   Source.dispose(src);
}

// Test character peeking
static void test_peek(void) {
   source src = Source.create("hello", 5);

   Assert.isTrue(Source.peek(src) == 'h', "Should peek 'h'");
   Assert.isTrue(Source.peek_offset(src, 1) == 'e', "Should peek 'e' at offset 1");
   Assert.isTrue(Source.peek_offset(src, 5) == '\0', "Should peek null at EOF");

   Source.dispose(src);
}

// Test string matching
static void test_match_length(void) {
   source src = Source.create("hello world", 11);

   Assert.isTrue(Source.match_length(src, "hello", 5) == 5, "Should match 'hello'");
   Assert.isTrue(Source.match_length(src, "world", 5) == 0, "Should not match 'world' at start");
   Assert.isTrue(Source.match_length(src, "", 0) == 0, "Should return 0 for empty string");
   Assert.isTrue(Source.match_length(src, NULL, 5) == 0, "Should return 0 for NULL string");

   Source.dispose(src);
}

// Test character classification
static void test_character_classification(void) {
   Assert.isTrue(Source.is_alpha('a'), "'a' should be alpha");
   Assert.isTrue(Source.is_alpha('Z'), "'Z' should be alpha");
   Assert.isTrue(Source.is_alpha('_'), "'_' should be alpha");
   Assert.isFalse(Source.is_alpha('1'), "'1' should not be alpha");

   Assert.isTrue(Source.is_digit('5'), "'5' should be digit");
   Assert.isFalse(Source.is_digit('a'), "'a' should not be digit");

   Assert.isTrue(Source.is_hex_digit('f'), "'f' should be hex digit");
   Assert.isTrue(Source.is_hex_digit('A'), "'A' should be hex digit");
   Assert.isFalse(Source.is_hex_digit('g'), "'g' should not be hex digit");

   Assert.isTrue(Source.is_identifier_start('a'), "'a' should be identifier start");
   Assert.isFalse(Source.is_identifier_start('1'), "'1' should not be identifier start");

   Assert.isTrue(Source.is_identifier_part('.'), "'.' should be identifier part");
   Assert.isFalse(Source.is_identifier_part('@'), "'@' should not be identifier part");
}

// Test consuming characters
static void test_consume(void) {
   source src = Source.create("hello\nworld", 12);

   Assert.isTrue(Source.position(src) == 0, "Initial position");
   usize new_pos = Source.consume(src, 3);
   Assert.isTrue(new_pos == 3, "Should return new position 3");
   Assert.isTrue(Source.position(src) == 3, "Position should be 3");
   Assert.isTrue(Source.peek(src) == 'l', "Should peek 'l'");

   // Test newline handling
   new_pos = Source.consume(src, 3); // consume "lo\n"
   Assert.isTrue(Source.line(src) == 2, "Should be on line 2");
   Assert.isTrue(Source.column(src) == 1, "Should be at column 1");

   Source.dispose(src);
}

// Test whitespace and comments
static void test_whitespace_and_comments(void) {
   source src = Source.create("  \t// comment\n/* block */\nhello", 31);
   Source.reset(src);

   usize skip_len = Source.skip_whitespace_and_comments(src);
   Assert.isTrue(skip_len == 26, "Should skip all whitespace and comments");

   Source.consume(src, skip_len);
   Assert.isTrue(Source.match_length(src, "hello", 5) == 5, "Should be at 'hello'");

   Source.dispose(src);
}

// Test shebang detection
static void test_shebang(void) {
   source src1 = Source.create("#!aml\ncontent", 14);
   Source.reset(src1);
   Assert.isTrue(Source.is_shebang(src1), "Should detect AML shebang");

   source src2 = Source.create("#!asl\ncontent", 14);
   Source.reset(src2);
   Assert.isTrue(Source.is_shebang(src2), "Should detect ASL shebang");

   source src3 = Source.create("no shebang", 10);
   Source.reset(src3);
   Assert.isFalse(Source.is_shebang(src3), "Should not detect shebang");

   Source.dispose(src1);
   Source.dispose(src2);
   Source.dispose(src3);
}

// Test dialect parsing
static void test_parse_dialect(void) {
   source src1 = Source.create("  // comment\n#!aml\ncontent", 28);
   Source.reset(src1);
   anvl_dialect dialect1 = Source.parse_dialect(src1, ANVL_DIALECT_ASL);
   Assert.isTrue(dialect1 == ANVL_DIALECT_AML, "Should parse AML dialect");

   source src2 = Source.create("#!asl\ncontent", 14);
   Source.reset(src2);
   anvl_dialect dialect2 = Source.parse_dialect(src2, ANVL_DIALECT_AML);
   Assert.isTrue(dialect2 == ANVL_DIALECT_ASL, "Should parse ASL dialect");

   source src3 = Source.create("no shebang", 10);
   Source.reset(src3);
   anvl_dialect dialect3 = Source.parse_dialect(src3, ANVL_DIALECT_AML);
   Assert.isTrue(dialect3 == ANVL_DIALECT_AML, "Should return hint when no shebang");

   Source.dispose(src1);
   Source.dispose(src2);
   Source.dispose(src3);
}

// Test dialect detection during source creation
static void test_source_creation_dialect(void) {
   // Test AML with whitespace and comments before shebang
   source src1 = Source.create("  \t\n// comment\n/* block */\n#!aml\ncontent", 45);
   Assert.isTrue(Source.dialect(src1) == ANVL_DIALECT_AML, "Should detect AML dialect with whitespace/comments");

   // Test ASL with whitespace before shebang
   source src2 = Source.create("   \n#!asl\ncontent", 18);
   Assert.isTrue(Source.dialect(src2) == ANVL_DIALECT_ASL, "Should detect ASL dialect with whitespace");

   // Test default ASL when no shebang
   source src3 = Source.create("no shebang content", 18);
   Assert.isTrue(Source.dialect(src3) == ANVL_DIALECT_ASL, "Should default to ASL when no shebang");

   // Test default ASL for empty source
   source src4 = Source.create("", 0);
   Assert.isTrue(Source.dialect(src4) == ANVL_DIALECT_ASL, "Should default to ASL for empty source");

   Source.dispose(src1);
   Source.dispose(src2);
   Source.dispose(src3);
   Source.dispose(src4);
}

// Test position management
static void test_position_management(void) {
   source src = Source.create("hello\nworld", 12);

   Source.set_position(src, 5, 2, 1);
   Assert.isTrue(Source.position(src) == 5, "Position should be set to 5");
   Assert.isTrue(Source.line(src) == 2, "Line should be set to 2");
   Assert.isTrue(Source.column(src) == 1, "Column should be set to 1");

   Source.reset(src);
   Assert.isTrue(Source.position(src) == 0, "Should reset to position 0");
   Assert.isTrue(Source.line(src) == 1, "Should reset to line 1");
   Assert.isTrue(Source.column(src) == 1, "Should reset to column 1");

   Source.dispose(src);
}

// Test operator and symbol classification
static void test_operator_symbol_classification(void) {
   // Test operators
   source src1 = Source.create(":=", 2);
   Assert.isTrue(Source.match_operator(src1, ":=", 2) == 2, ":= should match ASSIGN operator");
   Source.dispose(src1);

   source src2 = Source.create("=", 1);
   Assert.isTrue(Source.match_operator(src2, "=", 1) == 1, "= should match EQUAL operator");
   Source.dispose(src2);

   source src3 = Source.create("=>", 2);
   Assert.isTrue(Source.match_operator(src3, "=>", 2) == 2, "=> should match ROCKET operator");
   Source.dispose(src3);

   source src4 = Source.create("hello", 5);
   Assert.isTrue(Source.match_operator(src4, ":=", 2) == 0, "hello should not match ASSIGN operator");
   Source.dispose(src4);

   // Test symbols (free functions — no source object needed)
   Assert.isTrue(anvl_symbol_from_symbol("@", 1) == ANVL_SYM_AT, "@ should be AT symbol");
   Assert.isTrue(anvl_symbol_from_symbol("{", 1) == ANVL_SYM_L_BRACE, "{ should be L_BRACE symbol");
   Assert.isTrue(anvl_symbol_from_symbol("..", 2) == ANVL_SYM_DOT_DOT, ".. should be DOT_DOT symbol");
   Assert.isTrue(anvl_symbol_from_symbol("xyz", 3) == ANVL_SYM_INVALID, "xyz should not be a symbol");
}

static void _register(void) {
   testset("Source Interrogators", set_config, set_teardown);

   testcase("Position & EOF", test_position_and_eof);
   testcase("Character Peek", test_peek);
   testcase("String Matching", test_match_length);
   testcase("Character Classification", test_character_classification);
   testcase("Operator & Symbol Classification", test_operator_symbol_classification);
   testcase("Consume", test_consume);
   testcase("Whitespace & Comments", test_whitespace_and_comments);
   testcase("Shebang Detection", test_shebang);
   testcase("Dialect Parsing", test_parse_dialect);
   testcase("Source Creation Dialect", test_source_creation_dialect);
   testcase("Position Management", test_position_management);
}
__attribute__((constructor)) static void register_test_interrogators(void) {
   Tests.enqueue(_register);
}