/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_source.c - Unit tests for the Source interface                    *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_source.c                                          *
 * ********************************************************************** */

#include "anvil.h"
#include "types.h"
#include "internal/constants.h"
#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "testbit.h"
#include "std.h"
// ----------------
#include "../utilities/debug.h"
#include "../utilities/helpers.h"
#include <sigma/list.h>
#include <sigma/types.h>
#include <string.h>

static void ts(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

/* ---------------------------------------------------------------------- *
 * SRC00 — create and dispose
 * ---------------------------------------------------------------------- */
static void test_src00_create_dispose(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = Source.create(&src, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC00: create returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC00: err_code remains NONE");
   TestBit.is_not_null(src, "SRC00: source allocated");
   TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)Source.dialect(src),
                        "SRC00: default dialect is AML");
   TestBit.is_equal_int(0, (long long)Source.hash(src), "SRC00: hash is 0 for empty source");
   TestBit.is_equal_int(1, (long long)Source.line(src), "SRC00: line starts at 1");
   TestBit.is_equal_int(1, (long long)Source.column(src), "SRC00: column starts at 1");
   TestBit.is_true(Source.is_eof(src), "SRC00: empty source is at EOF");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC01 — create with null outputs
 * ---------------------------------------------------------------------- */
static void test_src01_create_null_outputs(void) {
   anvl_result res = Source.create(NULL, NULL);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC01: create with null outputs returns ERR");
}

/* ---------------------------------------------------------------------- *
 * SRC02 — dispose null is no-op
 * ---------------------------------------------------------------------- */
static void test_src02_dispose_null(void) {
   Source.dispose(NULL);
   TestBit.is_true(true, "SRC02: dispose(NULL) does not crash");
}

/* ---------------------------------------------------------------------- *
 * SRC03 — from_buffer copies content and computes hash
 * ---------------------------------------------------------------------- */
static void test_src03_from_buffer(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "name := test\n";
   usize len = strlen(buffer);

   anvl_result res = Source.create(&src, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC03: create succeeds");

   res = Source.from_buffer(&src, buffer, len, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC03: from_buffer returns OK");
   TestBit.is_equal_int(len, (long long)Source.length(src), "SRC03: length matches input");
   TestBit.is_true(Source.hash(src) != 0, "SRC03: hash is non-zero");
   TestBit.is_not_null(Source.data(src), "SRC03: data pointer is set");
   TestBit.is_true(memcmp(Source.data(src), buffer, len) == 0, "SRC03: data matches input buffer");
   TestBit.is_false(Source.is_eof(src), "SRC03: source not at EOF immediately");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC04 — from_buffer rejects null buffer
 * ---------------------------------------------------------------------- */
static void test_src04_from_buffer_null_buffer(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   anvl_result res = Source.from_buffer(&src, NULL, 5, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC04: from_buffer with null buffer returns ERR");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC05 — from_buffer accepts zero-length buffer
 * ---------------------------------------------------------------------- */
static void test_src05_from_buffer_zero_length(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   anvl_result res = Source.from_buffer(&src, "x", 0, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC05: from_buffer with zero length returns OK");
   TestBit.is_equal_int(0, (long long)Source.length(src), "SRC05: length is 0");
   TestBit.is_true(Source.is_eof(src), "SRC05: zero-length source is at EOF");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC06 — from_buffer rejects null out_src
 * ---------------------------------------------------------------------- */
static void test_src06_from_buffer_null_out_src(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = Source.from_buffer(NULL, "x", 1, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC06: from_buffer with null out_src returns ERR");
}

/* ---------------------------------------------------------------------- *
 * SRC07 — from_file loads fixture and derives dialect
 * ---------------------------------------------------------------------- */
static void test_src07_from_file(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = Source.create(&src, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC07: create succeeds");

   res = Source.from_file(&src, "../../test/fixtures/f01_bare_literal.anvl", &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC07: from_file returns OK");
   TestBit.is_true(Source.length(src) > 0, "SRC07: file content loaded");
   TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)Source.dialect(src),
                        "SRC07: .anvl fixture resolves to AML dialect");
   TestBit.is_true(Source.hash(src) != 0, "SRC07: file-loaded source has hash");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC08 — from_file rejects null path
 * ---------------------------------------------------------------------- */
static void test_src08_from_file_null_path(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   anvl_result res = Source.from_file(&src, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC08: from_file with null path returns ERR");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC09 — from_file rejects missing file
 * ---------------------------------------------------------------------- */
static void test_src09_from_file_missing(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   anvl_result res = Source.from_file(&src, "nonexistent_file.anvl", &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC09: from_file with missing file returns ERR");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC10 — hash is stable for identical content and different otherwise
 * ---------------------------------------------------------------------- */
static void test_src10_hash_stable_and_distinct(void) {
   anvl_source s1 = NULL;
   anvl_source s2 = NULL;
   anvl_source s3 = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&s1, &err_code);
   Source.create(&s2, &err_code);
   Source.create(&s3, &err_code);

   Source.from_buffer(&s1, "alpha", 5, &err_code);
   Source.from_buffer(&s2, "alpha", 5, &err_code);
   Source.from_buffer(&s3, "beta", 4, &err_code);

   TestBit.is_equal_int((long long)Source.hash(s1), (long long)Source.hash(s2),
                        "SRC10: identical content yields identical hashes");
   TestBit.is_true(Source.hash(s1) != Source.hash(s3),
                   "SRC10: different content yields different hashes");

   Source.dispose(s1);
   Source.dispose(s2);
   Source.dispose(s3);
}

/* ---------------------------------------------------------------------- *
 * SRC11 — position, line, and column tracking during consume
 * ---------------------------------------------------------------------- */
static void test_src11_position_tracking(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "ab\ncd";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   TestBit.is_equal_int(0, (long long)Source.position(src), "SRC11: position starts at 0");
   TestBit.is_equal_int(1, (long long)Source.line(src), "SRC11: line starts at 1");
   TestBit.is_equal_int(1, (long long)Source.column(src), "SRC11: column starts at 1");

   Source.consume(src, 3); // "ab\n"
   TestBit.is_equal_int(3, (long long)Source.position(src), "SRC11: position after ab\\n");
   TestBit.is_equal_int(2, (long long)Source.line(src), "SRC11: line advances after newline");
   TestBit.is_equal_int(1, (long long)Source.column(src), "SRC11: column resets after newline");

   Source.consume(src, 1); // "c"
   TestBit.is_equal_int(2, (long long)Source.column(src), "SRC11: column advances after c");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC12 — EOF and offset EOF
 * ---------------------------------------------------------------------- */
static void test_src12_eof_and_offset_eof(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "abc";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   TestBit.is_false(Source.is_eof(src), "SRC12: not at EOF at start");
   TestBit.is_false(Source.is_eof_offset(src, 2), "SRC12: offset 2 is valid");
   TestBit.is_true(Source.is_eof_offset(src, 3), "SRC12: offset 3 is at EOF");
   TestBit.is_true(Source.is_eof_offset(src, 4), "SRC12: offset 4 past EOF");

   Source.consume(src, 3);
   TestBit.is_true(Source.is_eof(src), "SRC12: at EOF after consuming all");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC13 — peek and peek_offset
 * ---------------------------------------------------------------------- */
static void test_src13_peek(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "abc";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   TestBit.is_equal_int('a', (long long)Source.peek(src), "SRC13: peek returns first char");
   TestBit.is_equal_int('b', (long long)Source.peek_offset(src, 1), "SRC13: peek_offset 1");
   TestBit.is_equal_int('c', (long long)Source.peek_offset(src, 2), "SRC13: peek_offset 2");
   TestBit.is_equal_int('\0', (long long)Source.peek_offset(src, 3), "SRC13: peek_offset past EOF");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC14 — match_length and match_operator
 * ---------------------------------------------------------------------- */
static void test_src14_match(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "import \"foo\"";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   TestBit.is_equal_int(6, (long long)Source.match_length(src, "import", 6),
                        "SRC14: match_length returns keyword length");
   TestBit.is_equal_int(0, (long long)Source.match_length(src, "IMPORT", 6),
                        "SRC14: case-sensitive mismatch returns 0");
   TestBit.is_equal_int(6, (long long)Source.match_operator(src, "import", 6),
                        "SRC14: match_operator delegates to match_length");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC15 — character classification helpers
 * ---------------------------------------------------------------------- */
static void test_src15_character_classification(void) {
   TestBit.is_true(Source.is_alpha('a'), "SRC15: 'a' is alpha");
   TestBit.is_true(Source.is_alpha('Z'), "SRC15: 'Z' is alpha");
   TestBit.is_false(Source.is_alpha('1'), "SRC15: '1' is not alpha");

   TestBit.is_true(Source.is_digit('0'), "SRC15: '0' is digit");
   TestBit.is_true(Source.is_digit('9'), "SRC15: '9' is digit");
   TestBit.is_false(Source.is_digit('a'), "SRC15: 'a' is not digit");

   TestBit.is_true(Source.is_hex_digit('f'), "SRC15: 'f' is hex digit");
   TestBit.is_true(Source.is_hex_digit('F'), "SRC15: 'F' is hex digit");
   TestBit.is_true(Source.is_hex_digit('9'), "SRC15: '9' is hex digit");
   TestBit.is_false(Source.is_hex_digit('g'), "SRC15: 'g' is not hex digit");

   TestBit.is_true(Source.is_identifier_start('_'), "SRC15: '_' starts identifier");
   TestBit.is_true(Source.is_identifier_start('a'), "SRC15: 'a' starts identifier");
   TestBit.is_false(Source.is_identifier_start('1'), "SRC15: '1' does not start identifier");

   TestBit.is_true(Source.is_identifier_part('_'), "SRC15: '_' is identifier part");
   TestBit.is_true(Source.is_identifier_part('a'), "SRC15: 'a' is identifier part");
   TestBit.is_true(Source.is_identifier_part('1'), "SRC15: '1' is identifier part");
   TestBit.is_false(Source.is_identifier_part('-'), "SRC15: '-' is not identifier part");
}

/* ---------------------------------------------------------------------- *
 * SRC16 — consume respects buffer bounds
 * ---------------------------------------------------------------------- */
static void test_src16_consume_bounds(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "abc";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   usize consumed = Source.consume(src, 10);
   TestBit.is_equal_int(3, (long long)consumed, "SRC16: consume capped at length");
   TestBit.is_true(Source.is_eof(src), "SRC16: EOF after capped consume");

   consumed = Source.consume(src, 5);
   TestBit.is_equal_int(0, (long long)consumed, "SRC16: no further consumption at EOF");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC17 — data and length accessors
 * ---------------------------------------------------------------------- */
static void test_src17_data_and_length(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "hello";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   TestBit.is_equal_int(5, (long long)Source.length(src), "SRC17: length returns 5");
   TestBit.is_not_null(Source.data(src), "SRC17: data is not null");
   TestBit.is_true(strncmp(Source.data(src), "hello", 5) == 0,
                   "SRC17: data points to buffer content");

   Source.dispose(src);
   TestBit.is_equal_int(0, (long long)Source.length(NULL), "SRC17: length on null is 0");
   TestBit.is_null((void *)Source.data(NULL), "SRC17: data on null is NULL");
}

/* ---------------------------------------------------------------------- *
 * SRC18 — set_position and reset
 * ---------------------------------------------------------------------- */
static void test_src18_set_position_and_reset(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "line1\nline2";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   Source.consume(src, 7); // "line1\n"
   TestBit.is_equal_int(2, (long long)Source.line(src), "SRC18: line is 2 after newline");

   Source.set_position(src, 0, 1, 1);
   TestBit.is_equal_int(0, (long long)Source.position(src), "SRC18: position reset to 0");
   TestBit.is_equal_int(1, (long long)Source.line(src), "SRC18: line reset to 1");
   TestBit.is_equal_int(1, (long long)Source.column(src), "SRC18: column reset to 1");

   Source.consume(src, 3);
   Source.reset(src);
   TestBit.is_equal_int(0, (long long)Source.position(src), "SRC18: reset returns to start");

   // set_position beyond length clamps to length
   Source.set_position(src, 100, 5, 5);
   TestBit.is_equal_int((long long)Source.length(src), (long long)Source.position(src),
                        "SRC18: set_position clamps to length");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC19 — skip whitespace and comments
 * ---------------------------------------------------------------------- */
static void test_src19_skip_whitespace_and_comments(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "  // line comment\n  /* block */ name";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   usize skipped = Source.skip_whitespace_and_comments(src);
   TestBit.is_true(skipped > 0, "SRC19: skipped non-zero characters");
   TestBit.is_equal_int('n', (long long)Source.peek(src), "SRC19: stopped at identifier");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC20 — is_shebang
 * ---------------------------------------------------------------------- */
static void test_src20_is_shebang(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   Source.from_buffer(&src, "#!aml\n", 6, &err_code);
   TestBit.is_true(Source.is_shebang(src), "SRC20: detects shebang");
   Source.dispose(src);

   Source.create(&src, &err_code);
   Source.from_buffer(&src, "name := x", 9, &err_code);
   TestBit.is_false(Source.is_shebang(src), "SRC20: no shebang for normal content");
   Source.dispose(src);

   TestBit.is_false(Source.is_shebang(NULL), "SRC20: null source is not shebang");
}

/* ---------------------------------------------------------------------- *
 * SRC21 — has_errors and set_error via registry
 * ---------------------------------------------------------------------- */
static void test_src21_has_errors_and_set_error(void) {
   module_context ctx = NULL;
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("SRC21: setup context failed");
      return;
   }

   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      TestBit.fail("SRC21: setup document failed");
      return;
   }

   Source.from_buffer(&doc->source, "name := test\n", 13, &err_code);
   mod_ctx_register_doc(ctx, doc, "src21.anvl", &err_code);

   TestBit.is_false(Source.has_errors(doc->source),
                    "SRC21: registered source reports no errors initially");

   anvl_result res = Source.set_error(doc->source, ANVL_ERR_PARSER_UNEXPECTED_TOKEN, 2, 5,
                                      "src21.anvl", &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC21: set_error returns OK");
   TestBit.is_true(Source.has_errors(doc->source), "SRC21: source reports errors");
   TestBit.is_equal_int(1, (long long)List.size(ctx->errors), "SRC21: one error recorded");

   anvl_error err = anvl_error_get(ctx->errors, 0);
   TestBit.is_not_null(err, "SRC21: error retrieved");
   if (err) {
      TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, (long long)err->code,
                           "SRC21: error code matches");
      TestBit.is_equal_int(2, (long long)err->line, "SRC21: error line matches");
      TestBit.is_equal_int(5, (long long)err->column, "SRC21: error column matches");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * SRC22 — set_error fails for unregistered source
 * ---------------------------------------------------------------------- */
static void test_src22_set_error_unregistered(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   Source.from_buffer(&src, "x", 1, &err_code);

   anvl_result res = Source.set_error(src, ANVL_ERR_PARSER_UNEXPECTED_CHAR, 1, 1, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC22: set_error fails for unregistered source");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("SRC00_create_dispose", NULL, test_src00_create_dispose, ts);
   TestBit.run_ex("SRC01_create_null_outputs", NULL, test_src01_create_null_outputs, ts);
   TestBit.run_ex("SRC02_dispose_null", NULL, test_src02_dispose_null, ts);
   TestBit.run_ex("SRC03_from_buffer", NULL, test_src03_from_buffer, ts);
   TestBit.run_ex("SRC04_from_buffer_null_buffer", NULL, test_src04_from_buffer_null_buffer, ts);
   TestBit.run_ex("SRC05_from_buffer_zero_length", NULL, test_src05_from_buffer_zero_length, ts);
   TestBit.run_ex("SRC06_from_buffer_null_out_src", NULL, test_src06_from_buffer_null_out_src, ts);
   TestBit.run_ex("SRC07_from_file", NULL, test_src07_from_file, ts);
   TestBit.run_ex("SRC08_from_file_null_path", NULL, test_src08_from_file_null_path, ts);
   TestBit.run_ex("SRC09_from_file_missing", NULL, test_src09_from_file_missing, ts);
   TestBit.run_ex("SRC10_hash_stable_and_distinct", NULL, test_src10_hash_stable_and_distinct, ts);
   TestBit.run_ex("SRC11_position_tracking", NULL, test_src11_position_tracking, ts);
   TestBit.run_ex("SRC12_eof_and_offset_eof", NULL, test_src12_eof_and_offset_eof, ts);
   TestBit.run_ex("SRC13_peek", NULL, test_src13_peek, ts);
   TestBit.run_ex("SRC14_match", NULL, test_src14_match, ts);
   TestBit.run_ex("SRC15_character_classification", NULL, test_src15_character_classification, ts);
   TestBit.run_ex("SRC16_consume_bounds", NULL, test_src16_consume_bounds, ts);
   TestBit.run_ex("SRC17_data_and_length", NULL, test_src17_data_and_length, ts);
   TestBit.run_ex("SRC18_set_position_and_reset", NULL, test_src18_set_position_and_reset, ts);
   TestBit.run_ex("SRC19_skip_whitespace_and_comments", NULL,
                  test_src19_skip_whitespace_and_comments, ts);
   TestBit.run_ex("SRC20_is_shebang", NULL, test_src20_is_shebang, ts);
   TestBit.run_ex("SRC21_has_errors_and_set_error", NULL, test_src21_has_errors_and_set_error, ts);
   TestBit.run_ex("SRC22_set_error_unregistered", NULL, test_src22_set_error_unregistered, ts);

   return TestBit.report();
}
