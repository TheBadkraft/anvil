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
 * SRC14 — match_length and match_token
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
   TestBit.is_equal_int(6, (long long)Source.match_token(src, "import", 6),
                        "SRC14: match_token delegates to match_length");

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
 * SRC23 — get_arena via registry, success path
 * Commentary: same "source is the only handle" registry-lookup pattern as
 * has_errors/set_error, but returning the owning context's shared arena
 * instead of routing an error. ctx->arena is populated directly here via
 * Allocator.create_bump — mod_ctx_create_arena is a separate function
 * (test_module.c CR17) and get_arena's correctness shouldn't depend on it.
 * ---------------------------------------------------------------------- */
static void test_src23_get_arena(void) {
   module_context ctx = NULL;
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("SRC23: setup context failed");
      return;
   }
   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      TestBit.fail("SRC23: setup document failed");
      return;
   }

   Source.from_buffer(&doc->source, "name := test\n", 13, &err_code);
   mod_ctx_register_doc(ctx, doc, "src23.anvl", &err_code);

   ctx->arena = Allocator.create_bump(256);
   TestBit.is_not_null(ctx->arena, "SRC23: context arena created directly for this test");

   err_code = ANVL_ERR_NONE;
   bump_allocator found = Source.get_arena(doc->source, &err_code);
   TestBit.is_true(found == ctx->arena, "SRC23: get_arena returns the context's own arena");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC23: err_code stays NONE on success");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * SRC24 — get_arena on an unregistered source
 * ---------------------------------------------------------------------- */
static void test_src24_get_arena_unregistered(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   Source.from_buffer(&src, "x", 1, &err_code);

   err_code = ANVL_ERR_NONE;
   TestBit.is_null(Source.get_arena(src, &err_code),
                   "SRC24: get_arena is NULL for an unregistered source");
   TestBit.is_equal_int(ANVL_ERR_CONTEXT_INVALID, err_code,
                        "SRC24: err_code reports CONTEXT_INVALID for an unregistered source");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC25 — get_arena on a NULL source
 * ---------------------------------------------------------------------- */
static void test_src25_get_arena_null_source(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   TestBit.is_null(Source.get_arena(NULL, &err_code), "SRC25: get_arena is NULL for a NULL source");
   TestBit.is_equal_int(ANVL_ERR_SOURCE_NOT_FOUND, err_code,
                        "SRC25: err_code reports SOURCE_NOT_FOUND for a NULL source");
}
/* ---------------------------------------------------------------------- *
 * SRC26 — get_arena on a registered source whose context has no arena yet
 * ---------------------------------------------------------------------- */
static void test_src26_get_arena_not_initialized(void) {
   module_context ctx = NULL;
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("SRC26: setup context failed");
      return;
   }
   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      TestBit.fail("SRC26: setup document failed");
      return;
   }

   Source.from_buffer(&doc->source, "name := test\n", 13, &err_code);
   mod_ctx_register_doc(ctx, doc, "src26.anvl", &err_code);
   TestBit.is_null(ctx->arena, "SRC26: context arena not yet created");

   err_code = ANVL_ERR_NONE;
   TestBit.is_null(Source.get_arena(doc->source, &err_code),
                   "SRC26: get_arena is NULL before the context creates one");
   TestBit.is_equal_int(ANVL_ERR_ARENA_NOT_INITIALIZED, err_code,
                        "SRC26: err_code reports ARENA_NOT_INITIALIZED");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * SRC27 — new_node via registry, success path
 * Commentary: RED until Source.new_node's stub (src/core/source.c) is
 * replaced with the real registry lookup + ctx->arena->alloc + index-append.
 * See notes/document-body-parse.md "Arena node iteration". Covers both node
 * kinds landing in the right index list.
 * ---------------------------------------------------------------------- */
static void test_src27_new_node(void) {
   module_context ctx = NULL;
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("SRC27: setup context failed");
      return;
   }
   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      TestBit.fail("SRC27: setup document failed");
      return;
   }

   Source.from_buffer(&doc->source, "name := test\n", 13, &err_code);
   mod_ctx_register_doc(ctx, doc, "src27.anvl", &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, mod_ctx_create_arena(ctx, 256, &err_code),
                        "SRC27: create_arena returns OK");

   err_code = ANVL_ERR_NONE;
   void *stmt_node = Source.new_node(doc->source, ANVL_NODE_STATEMENT, &err_code);
   TestBit.is_not_null(stmt_node, "SRC27: new_node returns a statement node");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC27: err_code stays NONE for statement node");
   TestBit.is_equal_int(1, (long long)List.size(ctx->statements),
                        "SRC27: statement node appended to ctx->statements");
   TestBit.is_equal_int(0, (long long)List.size(ctx->values),
                        "SRC27: ctx->values untouched by a statement allocation");

   err_code = ANVL_ERR_NONE;
   void *value_node = Source.new_node(doc->source, ANVL_NODE_VALUE, &err_code);
   TestBit.is_not_null(value_node, "SRC27: new_node returns a value node");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC27: err_code stays NONE for value node");
   TestBit.is_equal_int(1, (long long)List.size(ctx->values),
                        "SRC27: value node appended to ctx->values");
   TestBit.is_equal_int(1, (long long)List.size(ctx->statements),
                        "SRC27: ctx->statements untouched by a value allocation");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * SRC28 — new_node on an unregistered source
 * ---------------------------------------------------------------------- */
static void test_src28_new_node_unregistered(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   Source.from_buffer(&src, "x", 1, &err_code);

   err_code = ANVL_ERR_NONE;
   TestBit.is_null(Source.new_node(src, ANVL_NODE_STATEMENT, &err_code),
                   "SRC28: new_node is NULL for an unregistered source");
   TestBit.is_equal_int(ANVL_ERR_CONTEXT_INVALID, err_code,
                        "SRC28: err_code reports CONTEXT_INVALID for an unregistered source");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC29 — new_node on a NULL source
 * ---------------------------------------------------------------------- */
static void test_src29_new_node_null_source(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   TestBit.is_null(Source.new_node(NULL, ANVL_NODE_STATEMENT, &err_code),
                   "SRC29: new_node is NULL for a NULL source");
   TestBit.is_equal_int(ANVL_ERR_SOURCE_NOT_FOUND, err_code,
                        "SRC29: err_code reports SOURCE_NOT_FOUND for a NULL source");
}
/* ---------------------------------------------------------------------- *
 * SRC30 — new_node on a registered source whose context has no arena yet
 * ---------------------------------------------------------------------- */
static void test_src30_new_node_not_initialized(void) {
   module_context ctx = NULL;
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("SRC30: setup context failed");
      return;
   }
   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      TestBit.fail("SRC30: setup document failed");
      return;
   }

   Source.from_buffer(&doc->source, "name := test\n", 13, &err_code);
   mod_ctx_register_doc(ctx, doc, "src30.anvl", &err_code);
   TestBit.is_null(ctx->arena, "SRC30: context arena not yet created");

   err_code = ANVL_ERR_NONE;
   TestBit.is_null(Source.new_node(doc->source, ANVL_NODE_STATEMENT, &err_code),
                   "SRC30: new_node is NULL before the context creates an arena");
   TestBit.is_equal_int(ANVL_ERR_ARENA_NOT_INITIALIZED, err_code,
                        "SRC30: err_code reports ARENA_NOT_INITIALIZED");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * SRC31 — init_slice sets data to the buffer base
 * Commentary: init_slice only fills `.data` (the buffer base, via
 * Source.data(src)) — start/end are set by the caller afterward, matching
 * how parse_identifier/parse_numeric_literal (src/core/parser.c) actually
 * use it. This is the simpler, shipped version of the "centralize slice
 * construction" idea from notes/document-body-parse.md.
 * ---------------------------------------------------------------------- */
static void test_src31_init_slice(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "name := test\n";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   anvl_slice slice = {0};
   Source.init_slice(src, &slice);

   TestBit.is_true(slice.data == Source.data(src),
                   "SRC31: init_slice sets data to the buffer base");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC32 — init_slice with a NULL out_slice does not crash
 * ---------------------------------------------------------------------- */
static void test_src32_init_slice_null_out(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   Source.from_buffer(&src, "x", 1, &err_code);

   Source.init_slice(src, NULL);
   TestBit.is_true(true, "SRC32: init_slice with a NULL out_slice does not crash");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC33 — finish_body freezes a scratch list into doc->body, in order
 * Commentary: RED until the stub is replaced. finish_body takes ownership
 * of `scratch` regardless of outcome (see its doc comment) — this test
 * deliberately never disposes `scratch` itself, even in the current RED
 * state where the stub doesn't yet honor that and `scratch` leaks. That
 * leak is expected and temporary: it's testing the real contract, not the
 * stub's, and resolves to 0 leaks the moment the real implementation lands.
 * ---------------------------------------------------------------------- */
static void test_src33_finish_body(void) {
   module_context ctx = NULL;
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("SRC33: setup context failed");
      return;
   }
   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      TestBit.fail("SRC33: setup document failed");
      return;
   }

   Source.from_buffer(&doc->source, "name := test\n", 13, &err_code);
   mod_ctx_register_doc(ctx, doc, "src33.anvl", &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, mod_ctx_create_arena(ctx, 256, &err_code),
                        "SRC33: create_arena returns OK");

   anvl_statement stmt1 = Source.new_node(doc->source, ANVL_NODE_STATEMENT, &err_code);
   anvl_statement stmt2 = Source.new_node(doc->source, ANVL_NODE_STATEMENT, &err_code);
   TestBit.is_not_null(stmt1, "SRC33: first statement node allocated");
   TestBit.is_not_null(stmt2, "SRC33: second statement node allocated");

   list scratch = List.new(4, sizeof(anvl_statement));
   List.append(scratch, stmt1);
   List.append(scratch, stmt2);

   err_code = ANVL_ERR_NONE;
   anvl_result res = Source.finish_body(doc->source, scratch, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC33: finish_body returns OK");
   TestBit.is_not_null(doc->body, "SRC33: doc->body is set");
   if (doc->body) {
      TestBit.is_equal_int(2, FArray.capacity(doc->body, sizeof(anvl_statement)),
                           "SRC33: doc->body has two entries");
      anvl_statement got1 = NULL;
      anvl_statement got2 = NULL;
      FArray.get(doc->body, 0, sizeof(anvl_statement), &got1);
      FArray.get(doc->body, 1, sizeof(anvl_statement), &got2);
      TestBit.is_true(got1 == stmt1, "SRC33: first entry matches, in order");
      TestBit.is_true(got2 == stmt2, "SRC33: second entry matches, in order");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * SRC34 — finish_body on an unregistered source
 * Commentary: same ownership-transfer leak note as SRC33 applies to
 * `scratch` here in the current RED state.
 * ---------------------------------------------------------------------- */
static void test_src34_finish_body_unregistered(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   Source.from_buffer(&src, "x", 1, &err_code);

   list scratch = List.new(1, sizeof(anvl_statement));

   err_code = ANVL_ERR_NONE;
   anvl_result res = Source.finish_body(src, scratch, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC34: finish_body fails for an unregistered source");
   TestBit.is_equal_int(ANVL_ERR_CONTEXT_INVALID, err_code,
                        "SRC34: err_code reports CONTEXT_INVALID");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC35 — slice_equals: matching slice and string
 * ---------------------------------------------------------------------- */
static void test_src35_slice_equals_match(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "import := 1\n";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   anvl_slice slice = {0};
   Source.init_slice(src, &slice);
   slice.start = Source.data(src);
   slice.end = slice.start + 6;

   TestBit.is_true(Source.slice_equals(slice, "import"),
                   "SRC35: slice_equals matches equal slice and string");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC36 — slice_equals: different length is not equal
 * ---------------------------------------------------------------------- */
static void test_src36_slice_equals_length_mismatch(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "import := 1\n";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   anvl_slice slice = {0};
   Source.init_slice(src, &slice);
   slice.start = Source.data(src);
   slice.end = slice.start + 6;

   TestBit.is_false(Source.slice_equals(slice, "import2"),
                    "SRC36: slice_equals rejects a different length");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC37 — slice_equals: same length, different content is not equal
 * ---------------------------------------------------------------------- */
static void test_src37_slice_equals_content_mismatch(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "vars12 := 1\n";

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, strlen(buffer), &err_code);

   anvl_slice slice = {0};
   Source.init_slice(src, &slice);
   slice.start = Source.data(src);
   slice.end = slice.start + 6;

   TestBit.is_false(Source.slice_equals(slice, "using1"),
                    "SRC37: slice_equals rejects same-length different content");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC38 — slice_equals: NULL-safety
 * ---------------------------------------------------------------------- */
static void test_src38_slice_equals_null_safety(void) {
   anvl_slice empty = {0};

   TestBit.is_false(Source.slice_equals(empty, "import"),
                    "SRC38: slice_equals is false for an invalid (empty) slice");
   TestBit.is_false(Source.slice_equals(empty, NULL),
                    "SRC38: slice_equals is false for a NULL expected string");
}

/* ---------------------------------------------------------------------- *
 * SRC39 - shebang dialect resolves by fixed length (ANVL_SHEBANG_LEN),
 * never by scanning for a terminating newline — so a minified document
 * (no newline, not even a space, right after the shebang) still resolves
 * the correct dialect, identically to the conventional "#!aml\n" form.
 * ---------------------------------------------------------------------- */
static void test_src39_shebang_fixed_length_no_separator_required(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   const char *conventional = "#!aml\nname := 1;";
   Source.create(&src, &err_code);
   Source.from_buffer(&src, conventional, strlen(conventional), &err_code);
   TestBit.is_equal_int(ANVL_DIALECT_AML, Source.dialect(src),
                        "SRC39: conventional '#!aml\\n' resolves to AML");
   Source.dispose(src);

   const char *space_no_newline = "#!aml name := 1;";
   Source.create(&src, &err_code);
   Source.from_buffer(&src, space_no_newline, strlen(space_no_newline), &err_code);
   TestBit.is_equal_int(ANVL_DIALECT_AML, Source.dialect(src),
                        "SRC39: '#!aml ' (space, no newline) still resolves to AML");
   Source.dispose(src);

   const char *zero_separator = "#!amlname := 1;";
   Source.create(&src, &err_code);
   Source.from_buffer(&src, zero_separator, strlen(zero_separator), &err_code);
   TestBit.is_equal_int(ANVL_DIALECT_AML, Source.dialect(src),
                        "SRC39: zero-separator '#!amlname' still resolves to AML");
   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC40 - an unrecognized shebang dialect token is a real error state
 * (ANVL_DIALECT_ERROR), not a silent fall-through to AML.
 * ---------------------------------------------------------------------- */
static void test_src40_shebang_invalid_dialect_is_error(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   const char *garbage = "#!xyz\nname := 1;";
   Source.create(&src, &err_code);
   Source.from_buffer(&src, garbage, strlen(garbage), &err_code);
   TestBit.is_true(Source.is_shebang(src), "SRC40: still detected as a shebang");
   TestBit.is_equal_int(ANVL_DIALECT_ERROR, Source.dialect(src),
                        "SRC40: unrecognized dialect token is ANVL_DIALECT_ERROR");
   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC41 - a shebang with fewer than ANVL_SHEBANG_LEN bytes available is
 * an error state too, never an out-of-bounds read.
 * ---------------------------------------------------------------------- */
static void test_src41_shebang_too_short_is_error(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   const char *too_short = "#!am";
   Source.create(&src, &err_code);
   Source.from_buffer(&src, too_short, strlen(too_short), &err_code);
   TestBit.is_true(Source.is_shebang(src), "SRC41: still detected as a shebang");
   TestBit.is_equal_int(ANVL_DIALECT_ERROR, Source.dialect(src),
                        "SRC41: too-short shebang is ANVL_DIALECT_ERROR, not a crash");
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
   TestBit.run_ex("SRC23_get_arena", NULL, test_src23_get_arena, ts);
   TestBit.run_ex("SRC24_get_arena_unregistered", NULL, test_src24_get_arena_unregistered, ts);
   TestBit.run_ex("SRC25_get_arena_null_source", NULL, test_src25_get_arena_null_source, ts);
   TestBit.run_ex("SRC26_get_arena_not_initialized", NULL, test_src26_get_arena_not_initialized,
                  ts);
   TestBit.run_ex("SRC27_new_node", NULL, test_src27_new_node, ts);
   TestBit.run_ex("SRC28_new_node_unregistered", NULL, test_src28_new_node_unregistered, ts);
   TestBit.run_ex("SRC29_new_node_null_source", NULL, test_src29_new_node_null_source, ts);
   TestBit.run_ex("SRC30_new_node_not_initialized", NULL, test_src30_new_node_not_initialized, ts);
   TestBit.run_ex("SRC31_init_slice", NULL, test_src31_init_slice, ts);
   TestBit.run_ex("SRC32_init_slice_null_out", NULL, test_src32_init_slice_null_out, ts);
   TestBit.run_ex("SRC33_finish_body", NULL, test_src33_finish_body, ts);
   TestBit.run_ex("SRC34_finish_body_unregistered", NULL, test_src34_finish_body_unregistered, ts);
   TestBit.run_ex("SRC35_slice_equals_match", NULL, test_src35_slice_equals_match, ts);
   TestBit.run_ex("SRC36_slice_equals_length_mismatch", NULL, test_src36_slice_equals_length_mismatch, ts);
   TestBit.run_ex("SRC37_slice_equals_content_mismatch", NULL, test_src37_slice_equals_content_mismatch, ts);
   TestBit.run_ex("SRC38_slice_equals_null_safety", NULL, test_src38_slice_equals_null_safety, ts);
   TestBit.run_ex("SRC39_shebang_fixed_length_no_separator_required", NULL,
                  test_src39_shebang_fixed_length_no_separator_required, ts);
   TestBit.run_ex("SRC40_shebang_invalid_dialect_is_error", NULL,
                  test_src40_shebang_invalid_dialect_is_error, ts);
   TestBit.run_ex("SRC41_shebang_too_short_is_error", NULL, test_src41_shebang_too_short_is_error,
                  ts);

   return TestBit.report();
}
