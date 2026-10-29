/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_body_asl.c - Unit tests for AnvlScript (ASL) body parsing         *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_body_asl.c                                       *
 * ---------------------------------------------------------------------- *
 * Step 1 scope only: bare function-declaration recognition -- zero       *
 * parameters, a whitespace/comment-only body. See                       *
 * notes/anvilscript-design.md. Built with WITH_ASL=1 (see Makefile) --   *
 * under the default WITH_ASL=0 build this suite wouldn't exercise       *
 * anything (the '#!asl' guard rejects every fixture here before body-   *
 * parse even runs; see test_anvil_native.c's ANV45).                    *
 * ********************************************************************** */

#include "anvil.h"
#include "types.h"
#include "internal/constants.h"
#include "internal/asl_parser.h"
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

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
}

/* Loads a fixture through header-scan + include-loading, ready for doc_parse_body. */
static module_document setup_asl_doc(const char *fixture_name, module_context *out_ctx) {
   module_document doc = setup_registered_file(fixture_name, out_ctx);
   if (!doc) {
      return NULL;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   if (ANVL_RES_OK != doc_scan_header(doc, &err_code)) {
      return doc;
   }
   usize size_hint = 0;
   (void)mod_load_includes(*out_ctx, doc, &size_hint, &err_code);
   usize capacity = mod_ctx_arena_size_hint(size_hint);
   mod_ctx_create_arena(*out_ctx, capacity, &err_code);
   return doc;
}

/* ---------------------------------------------------------------------- *
 * ASL01 — an ordinary AML-shaped statement still parses fine under
 * '#!asl' (regression guard: adding the function-declaration branch to
 * parse_statement doesn't disturb anything else).
 * ---------------------------------------------------------------------- */
static void test_asl01_ordinary_statement_under_asl_dialect(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("anvil_err_header_asl_not_supported.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL01: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL01: parse body returns OK");
   TestBit.is_equal_int(1, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "ASL01: one statement captured");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL02 — 'foo () => { };' under '#!asl' parses as ANVL_STMT_FUNCTION.
 * ---------------------------------------------------------------------- */
static void test_asl02_bare_function_decl(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_empty.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL02: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL02: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL02: 'foo' statement retrieved");
   if (stmt) {
      TestBit.is_equal_int(ANVL_STMT_FUNCTION, (long long)stmt->kind,
                           "ASL02: statement kind is ANVL_STMT_FUNCTION");
      TestBit.is_equal_int(3, (long long)Source.slice_length(stmt->name),
                           "ASL02: name span length is 3");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL03 — the exact same source under '#!aml' is rejected: function
 * declarations are AnvlScript-exclusive (docs/dialect-ownership-matrix.md).
 * ---------------------------------------------------------------------- */
static void test_asl03_function_decl_rejected_in_aml(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_in_aml.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL03: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "ASL03: parse body returns ERR");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ASL03: err_code reports UNEXPECTED_TOKEN");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL04 — the exact same source under '#!amp' is rejected the same way.
 * ---------------------------------------------------------------------- */
static void test_asl04_function_decl_rejected_in_amp(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_in_amp.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL04: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "ASL04: parse body returns ERR");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ASL04: err_code reports UNEXPECTED_TOKEN");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL05 — the function's value is ANVL_VALUE_FUNCTION, with zero params.
 * ---------------------------------------------------------------------- */
static void test_asl05_function_value_shape(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_empty.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL05: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL05: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL05: 'foo' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_FUNCTION, (long long)stmt->value->type,
                           "ASL05: value type is ANVL_VALUE_FUNCTION");
      TestBit.is_equal_int(0, (long long)List.size(stmt->value->function.params),
                           "ASL05: zero parameters");
      TestBit.is_null(stmt->value->function.ast, "ASL05: ast is NULL (lazily compiled)");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL06 — the function's body span is exactly the empty interior of
 * '{ };' -- zero bytes, start == end. Precise span assertion, same style
 * as ANV10/ANV11's exact-length scalar checks.
 * ---------------------------------------------------------------------- */
static void test_asl06_function_body_span(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_empty.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL06: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL06: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL06: 'foo' statement retrieved");
   if (stmt && stmt->value) {
      anvl_slice body = stmt->value->function.body;
      TestBit.is_true(body.start == body.end, "ASL06: body span is empty (start == end)");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL07 — 'foo (a, b, c) => { };' captures three plain-identifier
 * parameters, in declaration order.
 * ---------------------------------------------------------------------- */
static void test_asl07_multiple_params(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_params.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL07: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL07: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL07: 'foo' statement retrieved");
   if (stmt && stmt->value) {
      list params = stmt->value->function.params;
      TestBit.is_equal_int(3, (long long)List.size(params), "ASL07: three parameters captured");

      const char *expected[] = {"a", "b", "c"};
      for (usize i = 0; i < 3; i++) {
         anvl_asl_param param = NULL;
         List.get(params, i, (object *)&param);
         TestBit.is_not_null(param, "ASL07: parameter slot retrieved");
         if (param) {
            TestBit.is_true(Source.slice_equals(param->name, expected[i]),
                            "ASL07: parameter name matches declaration order");
            TestBit.is_null(param->default_value, "ASL07: no default literal");
         }
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL08 — 'foo (a, a) => { };' is rejected: two parameters sharing a
 * name. notes/anvilscript-design.md: "the ANVL parser... validates
 * parameter uniqueness."
 * ---------------------------------------------------------------------- */
static void test_asl08_duplicate_param_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_dup_param.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL08: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "ASL08: parse body returns ERR");
   TestBit.is_equal_int(ANVL_ERR_ASL_DUPLICATE_PARAMETER, err_code,
                        "ASL08: err_code reports ASL_DUPLICATE_PARAMETER");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL09 — 'foo (a,) => { };' is rejected: a trailing comma with nothing
 * after it is not a second parameter, just a malformed list.
 * ---------------------------------------------------------------------- */
static void test_asl09_trailing_comma_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_trailing_comma.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL09: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "ASL09: parse body returns ERR");
   TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, err_code,
                        "ASL09: err_code reports EXPECTED_IDENTIFIER");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL10 — 'greet (name, greeting = "Hello") => { };' — a parameter may
 * carry a default literal; one without '=' still has a NULL default.
 * ---------------------------------------------------------------------- */
static void test_asl10_string_default(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_default_string.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL10: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL10: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL10: 'greet' statement retrieved");
   if (stmt && stmt->value) {
      list params = stmt->value->function.params;
      TestBit.is_equal_int(2, (long long)List.size(params), "ASL10: two parameters captured");

      anvl_asl_param name_param = NULL;
      List.get(params, 0, (object *)&name_param);
      TestBit.is_not_null(name_param, "ASL10: 'name' parameter retrieved");
      if (name_param) {
         TestBit.is_true(Source.slice_equals(name_param->name, "name"),
                         "ASL10: first parameter is 'name'");
         TestBit.is_null(name_param->default_value, "ASL10: 'name' has no default");
      }

      anvl_asl_param greeting_param = NULL;
      List.get(params, 1, (object *)&greeting_param);
      TestBit.is_not_null(greeting_param, "ASL10: 'greeting' parameter retrieved");
      if (greeting_param) {
         TestBit.is_true(Source.slice_equals(greeting_param->name, "greeting"),
                         "ASL10: second parameter is 'greeting'");
         TestBit.is_not_null(greeting_param->default_value, "ASL10: 'greeting' has a default");
         if (greeting_param->default_value) {
            TestBit.is_equal_int(ANVL_VALUE_STRING, (long long)greeting_param->default_value->type,
                                 "ASL10: default is a STRING literal");
            TestBit.is_true(Source.slice_equals(greeting_param->default_value->text, "Hello"),
                            "ASL10: default text is 'Hello' (quotes excluded)");
         }
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL11 — 'foo (x, y = 42) => { };' — a numeric default literal.
 * ---------------------------------------------------------------------- */
static void test_asl11_numeric_default(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_default_numeric.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL11: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL11: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL11: 'foo' statement retrieved");
   if (stmt && stmt->value) {
      anvl_asl_param y_param = NULL;
      List.get(stmt->value->function.params, 1, (object *)&y_param);
      TestBit.is_not_null(y_param, "ASL11: 'y' parameter retrieved");
      if (y_param && y_param->default_value) {
         TestBit.is_equal_int(ANVL_VALUE_NUMERIC, (long long)y_param->default_value->type,
                              "ASL11: default is a NUMERIC literal");
         TestBit.is_true(Source.slice_equals(y_param->default_value->text, "42"),
                         "ASL11: default text is '42'");
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL12 — 'foo (x = [1, 2]) => { };' is rejected: a default must be a
 * scalar literal (notes/anvilscript-design.md's EBNF `literal` production
 * -- numeric, string, bool, or null), not a collection.
 * ---------------------------------------------------------------------- */
static void test_asl12_non_literal_default_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_default_invalid.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL12: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "ASL12: parse body returns ERR");
   TestBit.is_equal_int(ANVL_ERR_PARSER_EXPECTED_VALUE, err_code,
                        "ASL12: err_code reports EXPECTED_VALUE");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL13 — 'sum (args []) => { };' — a single parameter-array parameter.
 * ---------------------------------------------------------------------- */
static void test_asl13_single_vararg(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_vararg.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL13: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL13: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL13: 'sum' statement retrieved");
   if (stmt && stmt->value) {
      list params = stmt->value->function.params;
      TestBit.is_equal_int(1, (long long)List.size(params), "ASL13: one parameter captured");

      anvl_asl_param param = NULL;
      List.get(params, 0, (object *)&param);
      TestBit.is_not_null(param, "ASL13: 'args' parameter retrieved");
      if (param) {
         TestBit.is_true(Source.slice_equals(param->name, "args"), "ASL13: name is 'args'");
         TestBit.is_true(param->is_vararg, "ASL13: marked as a parameter-array");
         TestBit.is_null(param->default_value, "ASL13: no default (mutually exclusive with []"
                                                ")");
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL14 — 'sum (first, args []) => { };' — a parameter-array may follow
 * fixed parameters, as long as it's last.
 * ---------------------------------------------------------------------- */
static void test_asl14_vararg_after_fixed_param(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_vararg_with_fixed.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL14: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL14: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL14: 'sum' statement retrieved");
   if (stmt && stmt->value) {
      list params = stmt->value->function.params;
      TestBit.is_equal_int(2, (long long)List.size(params), "ASL14: two parameters captured");

      anvl_asl_param first_param = NULL;
      List.get(params, 0, (object *)&first_param);
      TestBit.is_not_null(first_param, "ASL14: 'first' parameter retrieved");
      if (first_param) {
         TestBit.is_false(first_param->is_vararg, "ASL14: 'first' is not a parameter-array");
      }

      anvl_asl_param args_param = NULL;
      List.get(params, 1, (object *)&args_param);
      TestBit.is_not_null(args_param, "ASL14: 'args' parameter retrieved");
      if (args_param) {
         TestBit.is_true(args_param->is_vararg, "ASL14: 'args' is a parameter-array");
      }
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL15 — 'foo (args [], extra) => { };' is rejected: a parameter-array
 * marker on a non-final parameter. notes/anvilscript-design.md: "a final
 * parameter may be marked as a parameter-array."
 * ---------------------------------------------------------------------- */
static void test_asl15_vararg_not_last_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_vararg_not_last.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL15: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "ASL15: parse body returns ERR");
   TestBit.is_equal_int(ANVL_ERR_ASL_VARARG_NOT_LAST, err_code,
                        "ASL15: err_code reports ASL_VARARG_NOT_LAST");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL16 — 'foo (args [5]) => { };' is rejected: the marker is a bare
 * '[]', nothing may appear between the brackets.
 * ---------------------------------------------------------------------- */
static void test_asl16_vararg_malformed_brackets_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_vararg_malformed.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL16: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "ASL16: parse body returns ERR");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "ASL16: err_code reports UNEXPECTED_TOKEN");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL17 — 'foo () => {{}};' — the body-span scan is brace-depth-aware: a
 * nested '{}' doesn't make the scan stop at the inner '}'.
 * ---------------------------------------------------------------------- */
static void test_asl17_body_span_nested_braces(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_nested_braces.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL17: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL17: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL17: 'foo' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_true(Source.slice_equals(stmt->value->function.body, "{}"),
                      "ASL17: body spans the full nested '{}', not just to the inner '}'");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL18 — 'foo () => {"}"};' — a '}' inside a string literal doesn't end
 * the body scan early.
 * ---------------------------------------------------------------------- */
static void test_asl18_body_span_brace_in_string(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_brace_in_string.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL18: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "ASL18: parse body returns OK");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "ASL18: 'foo' statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_true(Source.slice_equals(stmt->value->function.body, "\"}\""),
                      "ASL18: body spans the full string literal, '}' inside it not miscounted");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * ASL19 — 'foo () => {' with no matching '}' at all is rejected, not an
 * infinite loop or a crash.
 * ---------------------------------------------------------------------- */
static void test_asl19_unterminated_body_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_asl_doc("asl_function_decl_unterminated_body.anvl", &ctx);
   TestBit.is_not_null(doc, "ASL19: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "ASL19: parse body returns ERR");
   TestBit.is_equal_int(ANVL_ERR_ASL_UNTERMINATED_BODY, err_code,
                        "ASL19: err_code reports ASL_UNTERMINATED_BODY");

   mod_ctx_dispose(ctx);
}

int main(void) {
   TestBit.run_ex("ASL01_ordinary_statement_under_asl_dialect", NULL,
                  test_asl01_ordinary_statement_under_asl_dialect, th);
   TestBit.run_ex("ASL02_bare_function_decl", NULL, test_asl02_bare_function_decl, th);
   TestBit.run_ex("ASL03_function_decl_rejected_in_aml", NULL,
                  test_asl03_function_decl_rejected_in_aml, th);
   TestBit.run_ex("ASL04_function_decl_rejected_in_amp", NULL,
                  test_asl04_function_decl_rejected_in_amp, th);
   TestBit.run_ex("ASL05_function_value_shape", NULL, test_asl05_function_value_shape, th);
   TestBit.run_ex("ASL06_function_body_span", NULL, test_asl06_function_body_span, th);
   TestBit.run_ex("ASL07_multiple_params", NULL, test_asl07_multiple_params, th);
   TestBit.run_ex("ASL08_duplicate_param_rejected", NULL, test_asl08_duplicate_param_rejected, th);
   TestBit.run_ex("ASL09_trailing_comma_rejected", NULL, test_asl09_trailing_comma_rejected, th);
   TestBit.run_ex("ASL10_string_default", NULL, test_asl10_string_default, th);
   TestBit.run_ex("ASL11_numeric_default", NULL, test_asl11_numeric_default, th);
   TestBit.run_ex("ASL12_non_literal_default_rejected", NULL,
                  test_asl12_non_literal_default_rejected, th);
   TestBit.run_ex("ASL13_single_vararg", NULL, test_asl13_single_vararg, th);
   TestBit.run_ex("ASL14_vararg_after_fixed_param", NULL, test_asl14_vararg_after_fixed_param, th);
   TestBit.run_ex("ASL15_vararg_not_last_rejected", NULL, test_asl15_vararg_not_last_rejected, th);
   TestBit.run_ex("ASL16_vararg_malformed_brackets_rejected", NULL,
                  test_asl16_vararg_malformed_brackets_rejected, th);
   TestBit.run_ex("ASL17_body_span_nested_braces", NULL, test_asl17_body_span_nested_braces, th);
   TestBit.run_ex("ASL18_body_span_brace_in_string", NULL, test_asl18_body_span_brace_in_string, th);
   TestBit.run_ex("ASL19_unterminated_body_rejected", NULL, test_asl19_unterminated_body_rejected,
                  th);

   return TestBit.report();
}
