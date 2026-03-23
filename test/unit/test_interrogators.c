/*
 * test_interrogators.c - Tests for source interrogator functions
 */

#include "anvil.h"
#include "utilities/helpers.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <string.h>

/* Forward declarations — QP query path primitive tests */
static void test_qp01_field_count_object(void);
static void test_qp02_field_count_non_object(void);
static void test_qp03_get_field_by_index(void);
static void test_qp04_get_field_oob(void);
static void test_qp05_get_field_by_name_hit(void);
static void test_qp06_get_field_by_name_miss(void);
static void test_qp07_get_field_by_name_non_object(void);
static void test_qp08_element_count_array(void);
static void test_qp09_element_count_tuple(void);
static void test_qp10_element_count_non_collection(void);
static void test_qp11_get_element_by_index(void);
static void test_qp12_get_element_oob(void);
static void test_qp13_field_count_null_guard(void);
static void test_qp14_element_count_null_guard(void);
static void test_qp15_nested_object_field_traversal(void);
static void test_qp16_get_statement_by_name_hit(void);
static void test_qp17_get_statement_by_name_miss(void);
static void test_qp18_get_statement_by_name_null_guard(void);

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

   // QP — Query path primitives (E3)
   testcase("QP01: field_count on object stmt", test_qp01_field_count_object);
   testcase("QP02: field_count on non-object returns 0", test_qp02_field_count_non_object);
   testcase("QP03: get_field by index", test_qp03_get_field_by_index);
   testcase("QP04: get_field out-of-range returns NULL", test_qp04_get_field_oob);
   testcase("QP05: get_field_by_name hit", test_qp05_get_field_by_name_hit);
   testcase("QP06: get_field_by_name miss returns NULL", test_qp06_get_field_by_name_miss);
   testcase("QP07: get_field_by_name on non-object returns NULL", test_qp07_get_field_by_name_non_object);
   testcase("QP08: element_count on array stmt", test_qp08_element_count_array);
   testcase("QP09: element_count on tuple stmt", test_qp09_element_count_tuple);
   testcase("QP10: element_count on non-collection returns 0", test_qp10_element_count_non_collection);
   testcase("QP11: get_element by index", test_qp11_get_element_by_index);
   testcase("QP12: get_element out-of-range returns NULL", test_qp12_get_element_oob);
   testcase("QP13: field_count NULL guard", test_qp13_field_count_null_guard);
   testcase("QP14: element_count NULL guard", test_qp14_element_count_null_guard);
   testcase("QP15: nested object field traversal", test_qp15_nested_object_field_traversal);
   testcase("QP16: get_statement_by_name hit", test_qp16_get_statement_by_name_hit);
   testcase("QP17: get_statement_by_name miss returns NULL", test_qp17_get_statement_by_name_miss);
   testcase("QP18: get_statement_by_name NULL guard", test_qp18_get_statement_by_name_null_guard);
}
__attribute__((constructor)) static void register_test_interrogators(void) {
   Tests.enqueue(_register);
}

/* ================================================================
 * QP — Query path primitive tests (E3)
 * Each builds a minimal inline source string, parses it, then exercises
 * the five new Context.* functions.
 * ================================================================ */

static context qp_parse(const char *src) {
   anvl_ctx_builder_i *b = Context.get_builder();
   b->set_source(b, src, strlen(src));
   context ctx = b->build(b);
   if (!ctx)
      return NULL;
   if (!Context.parse(ctx)) {
      Context.dispose(ctx);
      return NULL;
   }
   return ctx;
}

/* QP01 — field_count on a parsed object statement */
static void test_qp01_field_count_object(void) {
   context ctx = qp_parse("#!aml\nblock := { x := 1, y := 2, z := 3 }\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "statement exists");
   Assert.areEqual(&(usize){3}, &(usize){Context.field_count(ctx, stmt)}, INT,
                   "field_count == 3");
   Context.dispose(ctx);
}

/* QP02 — field_count on scalar statement returns 0 */
static void test_qp02_field_count_non_object(void) {
   context ctx = qp_parse("#!aml\nfoo := 42\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "statement exists");
   usize n = Context.field_count(ctx, stmt);
   Assert.areEqual(&(usize){0}, &n, INT, "scalar stmt field_count == 0");
   Context.dispose(ctx);
}

/* QP03 — get_field returns the i-th field with correct key span */
static void test_qp03_get_field_by_index(void) {
   context ctx = qp_parse("#!aml\nobj := { alpha := 1, beta := 2 }\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "statement exists");

   field f0 = Context.get_field(ctx, stmt, 0);
   field f1 = Context.get_field(ctx, stmt, 1);
   Assert.isNotNull(f0, "field[0] not null");
   Assert.isNotNull(f1, "field[1] not null");

   const char *raw = Source.data(ctx->source);
   Assert.isTrue(strncmp(raw + f0->key_pos, "alpha", f0->key_len) == 0, "field[0] key == alpha");
   Assert.isTrue(strncmp(raw + f1->key_pos, "beta", f1->key_len) == 0, "field[1] key == beta");
   Context.dispose(ctx);
}

/* QP04 — get_field beyond count returns NULL */
static void test_qp04_get_field_oob(void) {
   context ctx = qp_parse("#!aml\nobj := { x := 1 }\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   field f = Context.get_field(ctx, stmt, 5);
   Assert.isNull(f, "out-of-range index returns NULL");
   Context.dispose(ctx);
}

/* QP05 — get_field_by_name finds an existing key */
static void test_qp05_get_field_by_name_hit(void) {
   context ctx = qp_parse("#!aml\nobj := { id := 99, name := hello }\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   field f = Context.get_field_by_name(ctx, stmt, "name", 4);
   Assert.isNotNull(f, "field 'name' found");
   const char *raw = Source.data(ctx->source);
   Assert.isTrue(strncmp(raw + f->key_pos, "name", f->key_len) == 0, "key span == 'name'");
   Context.dispose(ctx);
}

/* QP06 — get_field_by_name returns NULL for absent key */
static void test_qp06_get_field_by_name_miss(void) {
   context ctx = qp_parse("#!aml\nobj := { x := 1 }\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   field f = Context.get_field_by_name(ctx, stmt, "missing", 7);
   Assert.isNull(f, "absent key returns NULL");
   Context.dispose(ctx);
}

/* QP07 — get_field_by_name on scalar statement returns NULL */
static void test_qp07_get_field_by_name_non_object(void) {
   context ctx = qp_parse("#!aml\nfoo := hello\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   field f = Context.get_field_by_name(ctx, stmt, "foo", 3);
   Assert.isNull(f, "non-object stmt returns NULL");
   Context.dispose(ctx);
}

/* QP08 — element_count on array statement */
static void test_qp08_element_count_array(void) {
   context ctx = qp_parse("#!aml\nnums := [10, 20, 30, 40]\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   usize n = Context.element_count(ctx, stmt);
   Assert.areEqual(&(usize){4}, &n, INT, "array element_count == 4");
   Context.dispose(ctx);
}

/* QP09 — element_count on tuple statement */
static void test_qp09_element_count_tuple(void) {
   context ctx = qp_parse("#!aml\ncoord := (1, 2, 3)\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   usize n = Context.element_count(ctx, stmt);
   Assert.areEqual(&(usize){3}, &n, INT, "tuple element_count == 3");
   Context.dispose(ctx);
}

/* QP10 — element_count on scalar returns 0 */
static void test_qp10_element_count_non_collection(void) {
   context ctx = qp_parse("#!aml\nfoo := bar\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   usize n = Context.element_count(ctx, stmt);
   Assert.areEqual(&(usize){0}, &n, INT, "scalar element_count == 0");
   Context.dispose(ctx);
}

/* QP11 — get_element returns correct metadata for the i-th element */
static void test_qp11_get_element_by_index(void) {
   context ctx = qp_parse("#!aml\nnums := [10, 20, 30]\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   struct anvl_element_meta *e0 = Context.get_element(ctx, stmt, 0);
   struct anvl_element_meta *e2 = Context.get_element(ctx, stmt, 2);
   Assert.isNotNull(e0, "element[0] not null");
   Assert.isNotNull(e2, "element[2] not null");
   Assert.isTrue(e0->type == ANVL_VALUE_SCALAR, "element[0] is scalar");
   Assert.isTrue(e2->type == ANVL_VALUE_SCALAR, "element[2] is scalar");
   const char *raw = Source.data(ctx->source);
   Assert.isTrue(strncmp(raw + e0->pos, "10", e0->len) == 0, "element[0] span == '10'");
   Assert.isTrue(strncmp(raw + e2->pos, "30", e2->len) == 0, "element[2] span == '30'");
   Context.dispose(ctx);
}

/* QP12 — get_element out-of-range returns NULL */
static void test_qp12_get_element_oob(void) {
   context ctx = qp_parse("#!aml\nnums := [1, 2]\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   struct anvl_element_meta *e = Context.get_element(ctx, stmt, 99);
   Assert.isNull(e, "out-of-range element index returns NULL");
   Context.dispose(ctx);
}

/* QP13 — field_count with NULL context/stmt guards */
static void test_qp13_field_count_null_guard(void) {
   context ctx = qp_parse("#!aml\nobj := { x := 1 }\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   Assert.areEqual(&(usize){0}, &(usize){Context.field_count(NULL, stmt)}, INT,
                   "NULL ctx returns 0");
   Assert.areEqual(&(usize){0}, &(usize){Context.field_count(ctx, NULL)}, INT,
                   "NULL stmt returns 0");
   Context.dispose(ctx);
}

/* QP14 — element_count with NULL guards */
static void test_qp14_element_count_null_guard(void) {
   context ctx = qp_parse("#!aml\narr := [1, 2]\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   Assert.areEqual(&(usize){0}, &(usize){Context.element_count(NULL, stmt)}, INT,
                   "NULL ctx returns 0");
   Assert.areEqual(&(usize){0}, &(usize){Context.element_count(ctx, NULL)}, INT,
                   "NULL stmt returns 0");
   Context.dispose(ctx);
}

/* QP15 — traverse nested object: statement with object fields that are themselves objects */
static void test_qp15_nested_object_field_traversal(void) {
   context ctx = qp_parse(
       "#!aml\n"
       "config := { server := { host := localhost, port := 8080 }, timeout := 30 }\n");
   Assert.isNotNull(ctx, "context builds");
   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "config stmt exists");
   usize fc = Context.field_count(ctx, stmt);
   Assert.areEqual(&(usize){2}, &fc, INT, "config has 2 fields");

   field f_server = Context.get_field_by_name(ctx, stmt, "server", 6);
   field f_timeout = Context.get_field_by_name(ctx, stmt, "timeout", 7);
   Assert.isNotNull(f_server, "'server' field found");
   Assert.isNotNull(f_timeout, "'timeout' field found");
   Assert.isNotNull(f_server->val, "'server' field has a value");
   Assert.isTrue(f_server->val->type == ANVL_VALUE_OBJECT, "'server' value is an object");
   Context.dispose(ctx);
}

/* QP16 — get_statement_by_name returns the matching top-level statement */
static void test_qp16_get_statement_by_name_hit(void) {
   context ctx = qp_parse("#!aml\nalpha := 1\nbeta := 2\ngamma := 3\n");
   Assert.isNotNull(ctx, "context builds");

   statement s = Context.get_statement_by_name(ctx, "beta", 4);
   Assert.isNotNull(s, "'beta' statement found");

   const char *raw = Source.data(ctx->source);
   Assert.isTrue(
       strncmp(raw + s->meta[STMT_META_IDENT_POS], "beta", s->meta[STMT_META_IDENT_LEN]) == 0,
       "identifier span matches 'beta'");
   Context.dispose(ctx);
}

/* QP17 — get_statement_by_name returns NULL for an unknown identifier */
static void test_qp17_get_statement_by_name_miss(void) {
   context ctx = qp_parse("#!aml\nfoo := 1\n");
   Assert.isNotNull(ctx, "context builds");

   statement s = Context.get_statement_by_name(ctx, "bar", 3);
   Assert.isNull(s, "unknown name returns NULL");
   Context.dispose(ctx);
}

/* QP18 — get_statement_by_name NULL / zero-length guards */
static void test_qp18_get_statement_by_name_null_guard(void) {
   context ctx = qp_parse("#!aml\nfoo := 1\n");
   Assert.isNotNull(ctx, "context builds");

   Assert.isNull(Context.get_statement_by_name(NULL, "foo", 3), "NULL ctx returns NULL");
   Assert.isNull(Context.get_statement_by_name(ctx, NULL, 3), "NULL name returns NULL");
   Assert.isNull(Context.get_statement_by_name(ctx, "foo", 0), "zero len returns NULL");
   Context.dispose(ctx);
}