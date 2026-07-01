/*
 * test_asl.c — TDD tests for port/asl (anvil.asl)
 *
 * Contracts derived from AslParser.cs + AslEvaluator.cs (Anvil.Net v0.4.0).
 *
 * Test groups:
 *   A01–A05  Parser structural: parse returns a valid AST
 *   B01–B05  Exec literals: int, float, string, bool, null
 *   C01–C05  Arithmetic: +, *, -, %, string concat
 *   D01–D05  Control flow: if-taken, else-taken, for-loop, break, continue
 *   E01–E05  Scope & dispatch: locals, $varref, comparison, logical &&, params
 *
 * Design notes:
 *   - Each source string begins with '{' at body_start=0, so body_length=strlen(src).
 *   - Outer braces are stripped by the parser internally.
 *   - Parameters test uses source "x y { return x + y; }" with explicit spans.
 */

#include "anvil.h"
#include "asl.h"
#include "utilities/helpers.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */
static void test_a01_parse_simple_return_non_null(void);
static void test_a02_parse_assignment_non_null(void);
static void test_a03_parse_if_else_non_null(void);
static void test_a04_parse_for_loop_non_null(void);
static void test_a05_parse_root_is_block(void);

static void test_b01_exec_int_literal(void);
static void test_b02_exec_float_literal(void);
static void test_b03_exec_string_literal(void);
static void test_b04_exec_bool_true(void);
static void test_b05_exec_null_literal(void);

static void test_c01_exec_add(void);
static void test_c02_exec_mul(void);
static void test_c03_exec_sub(void);
static void test_c04_exec_mod(void);
static void test_c05_exec_string_concat(void);

static void test_d01_exec_if_taken(void);
static void test_d02_exec_else_taken(void);
static void test_d03_exec_for_accumulator(void);
static void test_d04_exec_break(void);
static void test_d05_exec_continue(void);

static void test_e01_exec_local_scope(void);
static void test_e02_exec_varref_external(void);
static void test_e03_exec_comparison(void);
static void test_e04_exec_logical_and(void);
static void test_e05_exec_parameters(void);

/* ------------------------------------------------------------------ */
/* Test set setup / teardown                                          */
/* ------------------------------------------------------------------ */
static void set_config(FILE **logger) {
   *logger = fopen("logs/test_asl.log", "w");
}
static void set_teardown(void) {
   Anvil.cleanup();
}
static void teardown(void) {
   Anvil.error_clear();
}

/* ------------------------------------------------------------------ */
/* Helper: build a zero-param function meta for a body-only source   */
/* The caller provides the full source starting with '{'.             */
/* body_start=0, body_length=strlen(src) (outer braces are included). */
/* ------------------------------------------------------------------ */
static asl_function_meta_t make_meta(const char *src) {
   return (asl_function_meta_t){
       .name_start = 0,
       .name_length = 0,
       .param_count = 0,
       .param_spans = NULL,
       .body_start = 0,
       .body_length = (int)strlen(src),
   };
}

/* ================================================================
 * A01 — Parse a simple return statement → non-NULL
 * ================================================================ */
static void test_a01_parse_simple_return_non_null(void) {
   const char *src = "{ return 42; }";
   asl_function_meta_t meta = make_meta(src);

   asl_node_t *root = Asl.parse(&meta, src);
   Assert.isNotNull(root, "parse returns non-NULL");

   Asl.node_free(root);
   teardown();
}

/* ================================================================
 * A02 — Parse an assignment statement → non-NULL
 * ================================================================ */
static void test_a02_parse_assignment_non_null(void) {
   const char *src = "{ x = 5; }";
   asl_function_meta_t meta = make_meta(src);

   asl_node_t *root = Asl.parse(&meta, src);
   Assert.isNotNull(root, "parse assignment returns non-NULL");

   Asl.node_free(root);
   teardown();
}

/* ================================================================
 * A03 — Parse an if/else block → non-NULL
 * ================================================================ */
static void test_a03_parse_if_else_non_null(void) {
   const char *src = "{ if (1) { } else { } }";
   asl_function_meta_t meta = make_meta(src);

   asl_node_t *root = Asl.parse(&meta, src);
   Assert.isNotNull(root, "parse if/else returns non-NULL");

   Asl.node_free(root);
   teardown();
}

/* ================================================================
 * A04 — Parse a for loop → non-NULL
 * ================================================================ */
static void test_a04_parse_for_loop_non_null(void) {
   const char *src = "{ for (i = 0; i < 3; i++) { } }";
   asl_function_meta_t meta = make_meta(src);

   asl_node_t *root = Asl.parse(&meta, src);
   Assert.isNotNull(root, "parse for loop returns non-NULL");

   Asl.node_free(root);
   teardown();
}

/* ================================================================
 * A05 — Parse root node kind is BLOCK
 * ================================================================ */
static void test_a05_parse_root_is_block(void) {
   const char *src = "{ return 1; }";
   asl_function_meta_t meta = make_meta(src);

   asl_node_t *root = Asl.parse(&meta, src);
   Assert.isNotNull(root, "parse returns non-NULL");
   Assert.isTrue(root->kind == ASL_NODE_BLOCK, "root kind is BLOCK");

   Asl.node_free(root);
   teardown();
}

/* ================================================================
 * B01 — exec { return 42; } → ASL_INT 42
 * ================================================================ */
static void test_b01_exec_int_literal(void) {
   const char *src = "{ return 42; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 42, "result is 42");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * B02 — exec { return 3.14; } → ASL_FLOAT ~3.14
 * ================================================================ */
static void test_b02_exec_float_literal(void) {
   const char *src = "{ return 3.14; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_FLOAT, "result kind is FLOAT");
   Assert.isTrue(result.double_value > 3.13 && result.double_value < 3.15,
                 "result is ~3.14");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * B03 — exec { return "hello"; } → ASL_STRING "hello"
 * ================================================================ */
static void test_b03_exec_string_literal(void) {
   const char *src = "{ return \"hello\"; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_STRING, "result kind is STRING");
   Assert.isTrue(strcmp(result.string_value, "hello") == 0, "result is 'hello'");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * B04 — exec { return true; } → ASL_BOOL true
 * ================================================================ */
static void test_b04_exec_bool_true(void) {
   const char *src = "{ return true; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_BOOL, "result kind is BOOL");
   Assert.isTrue(result.bool_value == 1, "result is true");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * B05 — exec { return null; } → ASL_NULL
 * ================================================================ */
static void test_b05_exec_null_literal(void) {
   const char *src = "{ return null; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_NULL, "result kind is NULL");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * C01 — exec { return 3 + 4; } → 7
 * ================================================================ */
static void test_c01_exec_add(void) {
   const char *src = "{ return 3 + 4; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 7, "3 + 4 = 7");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * C02 — exec { return 2 * 5; } → 10
 * ================================================================ */
static void test_c02_exec_mul(void) {
   const char *src = "{ return 2 * 5; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 10, "2 * 5 = 10");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * C03 — exec { return 9 - 4; } → 5
 * ================================================================ */
static void test_c03_exec_sub(void) {
   const char *src = "{ return 9 - 4; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 5, "9 - 4 = 5");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * C04 — exec { return 10 % 3; } → 1
 * ================================================================ */
static void test_c04_exec_mod(void) {
   const char *src = "{ return 10 % 3; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 1, "10 %% 3 = 1");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * C05 — exec { return "foo" + "bar"; } → "foobar"
 * ================================================================ */
static void test_c05_exec_string_concat(void) {
   const char *src = "{ return \"foo\" + \"bar\"; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_STRING, "result kind is STRING");
   Assert.isTrue(strcmp(result.string_value, "foobar") == 0,
                 "concat is 'foobar'");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * D01 — if (1) { return 1; } return 0; → 1 (branch taken)
 * ================================================================ */
static void test_d01_exec_if_taken(void) {
   const char *src = "{ if (1) { return 1; } return 0; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 1, "if branch taken → 1");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * D02 — if (0) { return 1; } else { return 2; } → 2 (else taken)
 * ================================================================ */
static void test_d02_exec_else_taken(void) {
   const char *src = "{ if (0) { return 1; } else { return 2; } }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 2, "else branch taken → 2");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * D03 — for loop accumulator: sum 0+1+2 = 3
 * ================================================================ */
static void test_d03_exec_for_accumulator(void) {
   const char *src =
       "{ x = 0; for (i = 0; i < 3; i++) { x = x + i; } return x; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 3, "0+1+2 = 3");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * D04 — break exits the for loop early; last x = 2
 * ================================================================ */
static void test_d04_exec_break(void) {
   const char *src =
       "{ x = -1;"
       " for (i = 0; i < 10; i++) {"
       "   if (i == 3) { break; }"
       "   x = i;"
       " }"
       " return x; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 2, "last x before break is 2");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * D05 — continue skips one iteration; 4 of 5 iters complete
 * ================================================================ */
static void test_d05_exec_continue(void) {
   const char *src =
       "{ x = 0;"
       " for (i = 0; i < 5; i++) {"
       "   if (i == 2) { continue; }"
       "   x = x + 1;"
       " }"
       " return x; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 4, "4 iters complete (i=2 skipped)");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * E01 — local scope: x = 5; y = x * 2; return y; → 10
 * ================================================================ */
static void test_e01_exec_local_scope(void) {
   const char *src = "{ x = 5; y = x * 2; return y; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 10, "5 * 2 = 10");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * E02 — $varref resolved via external lookup → "world"
 * ================================================================ */
static bool mock_ext_lookup(const char *key, asl_value_t *out, void *ud) {
   (void)ud;
   if (strcmp(key, "name") == 0) {
      const char *val = "world";
      size_t len = strlen(val);
      char *s = Allocator.alloc(len + 1);
      if (!s)
         return false;
      memcpy(s, val, len + 1);
      out->kind = ASL_STRING;
      out->string_value = s;
      return true;
   }
   return false;
}

static void test_e02_exec_varref_external(void) {
   const char *src = "{ return $name; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, mock_ext_lookup, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_STRING, "result kind is STRING");
   Assert.isTrue(strcmp(result.string_value, "world") == 0,
                 "result is 'world'");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * E03 — comparison: return 3 > 2; → ASL_BOOL true
 * ================================================================ */
static void test_e03_exec_comparison(void) {
   const char *src = "{ return 3 > 2; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_BOOL, "result kind is BOOL");
   Assert.isTrue(result.bool_value == 1, "3 > 2 is true");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * E04 — logical &&: return 1 == 1 && 2 == 2; → ASL_BOOL true
 * ================================================================ */
static void test_e04_exec_logical_and(void) {
   const char *src = "{ return 1 == 1 && 2 == 2; }";
   asl_function_meta_t meta = make_meta(src);
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, NULL, 0, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_BOOL, "result kind is BOOL");
   Assert.isTrue(result.bool_value == 1, "1==1 && 2==2 is true");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * E05 — function parameters: x + y with x=3, y=4 → 7
 *
 * Source layout: "x y { return x + y; }"
 *                 0 1 2 3 4              20
 * param_spans[0]={0,1} → "x"  param_spans[1]={2,1} → "y"
 * body_start=4, body_length=17  ("{" at 4, "}" at 20)
 * ================================================================ */
static void test_e05_exec_parameters(void) {
   const char *src = "x y { return x + y; }";
   asl_param_span_t pspans[2] = {{0, 1}, {2, 1}};
   asl_function_meta_t meta = {
       .name_start = 0,
       .name_length = 0,
       .param_count = 2,
       .param_spans = pspans,
       .body_start = 4,
       .body_length = 17,
   };
   asl_value_t args[2] = {
       [0] = {.kind = ASL_INT, .long_value = 3},
       [1] = {.kind = ASL_INT, .long_value = 4},
   };
   asl_value_t result = {0};

   bool ok = Asl.exec(&meta, src, args, 2, &result,
                      NULL, 0, NULL, NULL, NULL, NULL);

   Assert.isTrue(ok, "exec ok");
   Assert.isTrue(result.kind == ASL_INT, "result kind is INT");
   Assert.isTrue(result.long_value == 7, "x(3) + y(4) = 7");

   Asl.value_free(&result);
   teardown();
}

/* ================================================================
 * Registration
 * ================================================================ */
static void _register(void) {
   testset("ASL Tests", set_config, set_teardown);

   testcase("A01: Parse simple return non-null", test_a01_parse_simple_return_non_null);
   testcase("A02: Parse assignment non-null", test_a02_parse_assignment_non_null);
   testcase("A03: Parse if/else non-null", test_a03_parse_if_else_non_null);
   testcase("A04: Parse for loop non-null", test_a04_parse_for_loop_non_null);
   testcase("A05: Parse root is BLOCK", test_a05_parse_root_is_block);

   testcase("B01: Exec int literal 42", test_b01_exec_int_literal);
   testcase("B02: Exec float literal 3.14", test_b02_exec_float_literal);
   testcase("B03: Exec string literal 'hello'", test_b03_exec_string_literal);
   testcase("B04: Exec bool literal true", test_b04_exec_bool_true);
   testcase("B05: Exec null literal", test_b05_exec_null_literal);

   testcase("C01: Exec add 3+4=7", test_c01_exec_add);
   testcase("C02: Exec mul 2*5=10", test_c02_exec_mul);
   testcase("C03: Exec sub 9-4=5", test_c03_exec_sub);
   testcase("C04: Exec mod 10%%3=1", test_c04_exec_mod);
   testcase("C05: Exec string concat 'foobar'", test_c05_exec_string_concat);

   testcase("D01: Exec if branch taken", test_d01_exec_if_taken);
   testcase("D02: Exec else branch taken", test_d02_exec_else_taken);
   testcase("D03: Exec for-loop accumulator=3", test_d03_exec_for_accumulator);
   testcase("D04: Exec break exits loop early", test_d04_exec_break);
   testcase("D05: Exec continue skips iteration", test_d05_exec_continue);

   testcase("E01: Exec local-scope vars", test_e01_exec_local_scope);
   testcase("E02: Exec $varref external lookup", test_e02_exec_varref_external);
   testcase("E03: Exec comparison 3>2=true", test_e03_exec_comparison);
   testcase("E04: Exec logical && true", test_e04_exec_logical_and);
   testcase("E05: Exec function parameters", test_e05_exec_parameters);
}
__attribute__((constructor)) static void register_test_asl(void) {
   Tests.enqueue(_register);
}
