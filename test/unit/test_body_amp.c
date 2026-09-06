/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_body_amp.c - Unit tests for AMP-legal body parsing                *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_body_amp.c                                        *
 * ---------------------------------------------------------------------- *
 * AMP is a strict grammar subset of AML: everything proven here also     *
 * holds in AML, so test_body_aml.c never re-proves scalar/array          *
 * assignment. This suite carries both the AMP-legal positive cases and   *
 * the AMP-illegal negative (rejection) cases, since those rejections are *
 * exactly what makes it a subset. See notes/document-body-parse.md.      *
 *                                                                        *
 * Status: complete and GREEN against the real parser (src/core/parser.c) *
 * — all cases pass. AMP17 is deliberately absent (reserved for a future  *
 * positive tuple-in-AMP case once parse_tuple is implemented). AML       *
 * grammar (test_body_aml.c) is the next phase of work, still RED.        *
 * ********************************************************************** */

#include "anvil.h"
#include "types.h"
#include "internal/constants.h"
#include "internal/module.h"
#include "internal/parser.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "testbit.h"
#include "std.h"
// ----------------
#include "../utilities/debug.h"
#include "../utilities/helpers.h"
#include <sigma/collections.h>
#include <sigma/list.h>
#include <sigma/types.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
   anvl_cleanup();
}

/* ---------------------------------------------------------------------- *
 * AMP00a — keyword literal assignments: true, false, null
 * Commentary: the start of a broader "reserved keyword" invariant group —
 * true/false/null are reserved keywords, not ordinary identifiers, and
 * only distinguishable from a bare IDENTIFIER reference by scanning the
 * full identifier-shaped token and comparing its text (see
 * notes/document-body-parse.md). Grouped here alongside AMP00b (import
 * rejected as an identifier) as the same invariant from the other side:
 * these keywords work as values, but cannot work as identifiers.
 * ---------------------------------------------------------------------- */
static void test_amp00a_keyword_literal_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "flag_true := true;\n"
                                       "flag_false := false;\n"
                                       "empty := null;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP00a: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP00a: parse body returns OK");
   TestBit.is_equal_int(3, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP00a: three statements captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP00a: true statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_BOOL, (long long)stmt->value->type,
                           "AMP00a: first value is BOOL");
      TestBit.is_true(slice_equals(stmt->value->text, "true"),
                      "AMP00a: first value text is 'true'");
   }

   stmt = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP00a: false statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_BOOL, (long long)stmt->value->type,
                           "AMP00a: second value is BOOL");
      TestBit.is_true(slice_equals(stmt->value->text, "false"),
                      "AMP00a: second value text is 'false'");
   }

   stmt = NULL;
   FArray.get(doc->body, 2, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP00a: null statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_NULL, (long long)stmt->value->type,
                           "AMP00a: third value is NULL");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP00b — 'import' rejected as an identifier
 * Commentary: `import` is reserved by the header scanner for the
 * `import "path";` construct; using it as an ordinary statement name
 * must be rejected the same way, not silently accepted as a bare
 * identifier just because we're past the header and into the body.
 * `import` deliberately isn't the *first* statement here — the header
 * scanner's own import-loop only scans leading imports and stops for
 * good at the first non-import content, so `import` as a first
 * statement in an AMP document would instead trip
 * ANVL_ERR_IMPORT_AMP_FORBIDDEN at header-scan time (AMP forbids
 * imports entirely), never reaching body-parse at all. Putting a real
 * statement first ensures this genuinely exercises the body parser's
 * own keyword check, not a header-scan collision.
 * ---------------------------------------------------------------------- */
static void test_amp00b_import_rejected_as_identifier(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "first := 1;\n"
                                       "import := 2;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP00b: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP00b: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP00b: document reports an error");
   TestBit.is_equal_int(ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD, err_code,
                        "AMP00b: err_code reports IDENTIFIER_IS_KEYWORD");
   // parser_set_error's own call into Source.set_error clobbers parser.err_code back to
   // ANVL_ERR_NONE as a side effect (that out-param reports whether *recording* the error
   // succeeded, not the error itself) unless parser_set_error reasserts the real code last.
   // AMP00c already proves the success case (parser.err_code correctly ANVL_ERR_NONE); this is
   // the matching failure-case proof that the global mirrors doc_parse_body's own out-param
   // rather than reading back as ANVL_ERR_NONE on every real failure.
   TestBit.is_equal_int(ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD, anvl_get_error(),
                        "AMP00b: parser state error code also reports IDENTIFIER_IS_KEYWORD");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP00c — empty body
 * ---------------------------------------------------------------------- */
static void test_amp00c_empty_body(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n", &ctx);
   TestBit.is_not_null(doc, "AMP00c: document loaded");
   if (!doc) {
      TestBit.fail("AMP00c: document load failed; cannot continue test");
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   anvl_err_code exp_parser_err = ANVL_ERR_NONE;

   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP00c: parse body returns OK");
   TestBit.is_equal_int(exp_parser_err, err_code, "AMP00c: parse body error code is NONE");
   TestBit.is_equal_int(exp_parser_err, anvl_get_error(),
                        "AMP00c: parser state error code is NONE");

   TestBit.is_not_null(doc->body, "AMP00c: body list allocated");
   if (doc->body) {
      TestBit.is_equal_int(0, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                           "AMP00c: body list is empty");
   } else {
      TestBit.fail("AMP00c: body list is NULL; cannot check size");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP01a — integer assignment: positive, negative, UINT64_MAX-scale
 * ---------------------------------------------------------------------- */
static void test_amp01a_integer_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "age := 42;\n"
                                       "negint := -1285;\n"
                                       "large := 18446744073709551615;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP01a: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP01a: parse body returns OK");
   TestBit.is_equal_int(3, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP01a: three statements captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP01a: first statement retrieved");
   if (stmt) {
      TestBit.is_equal_int(ANVL_STMT_ASSIGN, (long long)stmt->kind,
                           "AMP01a: first statement is ASSIGN");
      TestBit.is_not_null(stmt->value, "AMP01a: first statement has a value");
      if (stmt->value) {
         TestBit.is_equal_int(ANVL_VALUE_NUMERIC, (long long)stmt->value->type,
                              "AMP01a: first value is NUMERIC");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP01b — float assignment: positive, negative
 * ---------------------------------------------------------------------- */
static void test_amp01b_float_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "pi := 3.14159;\n"
                                       "negflt := -12.85;\n"
                                       "large := 184467440737.09551615;\n"
                                       "tiny := 0.000000000000001;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP01b: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP01b: parse body returns OK");
   TestBit.is_equal_int(4, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP01b: four statements captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP01b: first statement retrieved");
   if (stmt) {
      TestBit.is_equal_int(ANVL_STMT_ASSIGN, (long long)stmt->kind,
                           "AMP01b: first statement is ASSIGN");
      TestBit.is_not_null(stmt->value, "AMP01b: first statement has a value");
      if (stmt->value) {
         TestBit.is_equal_int(ANVL_VALUE_NUMERIC, (long long)stmt->value->type,
                              "AMP01b: first value is NUMERIC");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP02 — scientific-notation assignment: positive, negative
 * ---------------------------------------------------------------------- */
static void test_amp02_float_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "exp := 1.7976931348623157e+308;\n"
                                       "negexp := -2.2250738585072014E-308;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP02: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP02: parse body returns OK");
   TestBit.is_equal_int(2, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP02: two statements captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP02: first statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_NUMERIC, (long long)stmt->value->type,
                           "AMP02: first value is NUMERIC");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP03a — (quoted) string assignment
 * ---------------------------------------------------------------------- */
static void test_amp03a_string_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "greet := \"hello\";\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP03a: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP03a: parse body returns OK");
   TestBit.is_equal_int(1, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP03a: one statement captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP03a: statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_STRING, (long long)stmt->value->type,
                           "AMP03a: value is STRING");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP03b — bare literal assignment
 * Commentary: exercises the full "Bare literal grammar" section of
 * notes/document-body-parse.md — plain word, underscore/hyphen/dot
 * mid-token, a leading `.` and `/`, a hyphen-heavy UUID shape, a
 * digit-leading literal that must fall through a *declined* numeric
 * attempt (`date_like`, exercising the parse_numeric_literal boundary-
 * decline TODO in the same notes section), a mid-token `:` and `//`
 * (`url`), and a literal `//` lead — both directly (`unc_path`, now that
 * the `:=`-to-value gap is same-line-whitespace-only and can no longer
 * misread it as a comment) and via the now-unnecessary-but-still-valid
 * `.`-prefixed convention (`unc_path_dot`).
 * ---------------------------------------------------------------------- */
static void test_amp03b_bare_literal_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "greet := hello;\n"
                                       // some invariants
                                       "mush := apple_sauce;\n"
                                       "pie := cherry-pie;\n"
                                       "polka := dance.a.lot;\n"
                                       "abs_path := /path/to/dir/;\n"
                                       "rel_path := ../alt/path/;\n"
                                       "uuid := cf3ce71d-83d7-46f5-9454-d4c0ea2141d0;\n"
                                       "date_like := 09-02-2026;\n"
                                       "url := http://example.com;\n"
                                       "unc_path := //server/share;\n"
                                       "unc_path_dot := .//server/share;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP03b: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP03b: parse body returns OK");

   static const char *expected[] = {
      "hello",
      "apple_sauce",
      "cherry-pie",
      "dance.a.lot",
      "/path/to/dir/",
      "../alt/path/",
      "cf3ce71d-83d7-46f5-9454-d4c0ea2141d0",
      "09-02-2026",
      "http://example.com",
      "//server/share",
      ".//server/share",
   };
   usize expected_count = sizeof(expected) / sizeof(expected[0]);
   TestBit.is_equal_int((long long)expected_count,
                        (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP03b: all bare-literal statements captured");

   for (usize i = 0; i < expected_count; i++) {
      anvl_statement stmt = NULL;
      FArray.get(doc->body, i, sizeof(anvl_statement), (object *)&stmt);
      TestBit.is_not_null(stmt, "AMP03b: statement retrieved");
      if (stmt && stmt->value) {
         TestBit.is_equal_int(ANVL_VALUE_IDENTIFIER, (long long)stmt->value->type,
                              "AMP03b: value is IDENTIFIER");
         TestBit.is_true(slice_equals(stmt->value->text, expected[i]),
                         "AMP03b: value text matches expected bare literal");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP04 — blob assignment: tagged and untagged
 * ---------------------------------------------------------------------- */
static void test_amp04_blob_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "created := @date`2026-07-07`;\n"
                                       "raw := `payload`;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP04: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP04: parse body returns OK");
   TestBit.is_equal_int(2, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP04: two statements captured");

   anvl_statement tagged = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&tagged);
   TestBit.is_not_null(tagged, "AMP04: tagged statement retrieved");
   if (tagged && tagged->value) {
      TestBit.is_equal_int(ANVL_VALUE_BLOB, (long long)tagged->value->type,
                           "AMP04: tagged value is BLOB");
      TestBit.is_false(Source.slice_is_empty(tagged->value->blob.tag),
                       "AMP04: tagged blob has a tag");
   }

   anvl_statement untagged = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&untagged);
   TestBit.is_not_null(untagged, "AMP04: untagged statement retrieved");
   if (untagged && untagged->value) {
      TestBit.is_equal_int(ANVL_VALUE_BLOB, (long long)untagged->value->type,
                           "AMP04: untagged value is BLOB");
      TestBit.is_true(Source.slice_is_empty(untagged->value->blob.tag),
                      "AMP04: untagged blob has an empty tag");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP05a — scalar array assignment
 * Commentary: two statements — an all-numeric array, and a bare-literal
 * array whose fixture deliberately ends in a trailing comma before `]`
 * (`array ::= '[' (value (',' value)* ','?)? ']'`, per notes/document-
 * body-parse.md and the JS reference parser's mirrored grammar) — so
 * this doubles as the trailing-comma coverage, not just element type.
 * ---------------------------------------------------------------------- */
static void test_amp05a_scalar_array_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "tags := [1, 2, 3];\n"
                                       "cars := [\n"
                                       "    Toyota,\n"
                                       "    Honda,\n"
                                       "    Ford,\n" // trailing comma before ']' is legal
                                       "];",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP05a: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP05a: parse body returns OK");
   TestBit.is_equal_int(2, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP05a: two statements captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP05a: tags statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_ARRAY, (long long)stmt->value->type,
                           "AMP05a: tags value is ARRAY");
      TestBit.is_equal_int(3, (long long)List.size(stmt->value->collection.items),
                           "AMP05a: tags array has three elements");
      anvl_value elem = NULL;
      List.get(stmt->value->collection.items, 0, (object *)&elem);
      if (elem) {
         TestBit.is_equal_int(ANVL_VALUE_NUMERIC, (long long)elem->type,
                              "AMP05a: tags first element is NUMERIC");
      }
   }

   stmt = NULL;
   FArray.get(doc->body, 1, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP05a: cars statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_ARRAY, (long long)stmt->value->type,
                           "AMP05a: cars value is ARRAY");
      TestBit.is_equal_int(3, (long long)List.size(stmt->value->collection.items),
                           "AMP05a: cars array has three elements despite trailing comma");
      anvl_value elem = NULL;
      List.get(stmt->value->collection.items, 0, (object *)&elem);
      if (elem) {
         TestBit.is_equal_int(ANVL_VALUE_IDENTIFIER, (long long)elem->type,
                              "AMP05a: cars first element is IDENTIFIER (bare literal)");
         TestBit.is_true(slice_equals(elem->text, "Toyota"),
                         "AMP05a: cars first element is 'Toyota'");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP05b — mixed scalar value types in a single array
 * Commentary: proves array elements aren't restricted to one shape —
 * numeric, string, bool, null, and bare literal side by side, exactly
 * the set AMP allows ("all scalars", notes/document-body-parse.md
 * "Dialect scope"). Nothing here is AMP-illegal, since none of these
 * are structural (array/tuple/object) — that rejection is AMP05c below.
 * ---------------------------------------------------------------------- */
static void test_amp05b_mixed_array_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "mixed := [1, \"two\", true, null, three];\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP05b: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP05b: parse body returns OK");
   TestBit.is_equal_int(1, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP05b: one statement captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP05b: statement retrieved");
   if (!stmt || !stmt->value) {
      mod_ctx_dispose(ctx);
      return;
   }
   TestBit.is_equal_int(ANVL_VALUE_ARRAY, (long long)stmt->value->type, "AMP05b: value is ARRAY");

   static const anvl_value_type expected_types[] = {
      ANVL_VALUE_NUMERIC, ANVL_VALUE_STRING,     ANVL_VALUE_BOOL,
      ANVL_VALUE_NULL,    ANVL_VALUE_IDENTIFIER,
   };
   usize expected_count = sizeof(expected_types) / sizeof(expected_types[0]);
   TestBit.is_equal_int((long long)expected_count,
                        (long long)List.size(stmt->value->collection.items),
                        "AMP05b: array has five mixed-type elements");

   for (usize i = 0; i < expected_count; i++) {
      anvl_value elem = NULL;
      List.get(stmt->value->collection.items, i, (object *)&elem);
      TestBit.is_not_null(elem, "AMP05b: element retrieved");
      if (elem) {
         TestBit.is_equal_int((long long)expected_types[i], (long long)elem->type,
                              "AMP05b: element type matches expected");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP06 — multiple statements captured in order
 * ---------------------------------------------------------------------- */
static void test_amp06_multiple_statements_in_order(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "first := 1;\n"
                                       "second := 2;\n"
                                       "third := 3;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP06: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP06: parse body returns OK");
   TestBit.is_equal_int(3, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP06: three statements captured");

   // Showcase: Sigma's generic Iterator over a non-owning collection view of
   // doc->body (an farray), instead of manual FArray.get(doc->body, i, ...)
   // indexing. FArray.as_collection wraps Collections.create_view(..., false) —
   // "false" means the view never owns/copies the farray's backing storage, so
   // disposing it below only frees the small view/iterator wrappers themselves.
   collection view = FArray.as_collection(doc->body, sizeof(anvl_statement));
   iterator it = Collections.create_iterator(view);
   TestBit.is_not_null(it, "AMP06: iterator created");

   const char *expected_names[3] = {"first", "second", "third"};
   usize idx = 0;

   while (Iterator.next(it)) {
      void *slot = Iterator.current(it);
      TestBit.is_not_null(slot, "AMP06: iterator slot is non-null");
      if (slot && idx < 3) {
         anvl_statement stmt = *(anvl_statement *)slot;
         TestBit.is_not_null(stmt, "AMP06: statement retrieved via iterator");
         if (stmt) {
            TestBit.is_true(slice_equals(stmt->name, expected_names[idx]),
                            "AMP06: statement name matches expected order");
         }
      }
      idx++;
   }
   TestBit.is_equal_int(3, (long long)idx, "AMP06: iterator visited all three statements");

   Iterator.dispose(it);
   Collections.dispose(view);

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP07 — missing value after ':=' reports error
 * ---------------------------------------------------------------------- */
static void test_amp07_missing_value_after_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "name := ;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP07: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP07: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP07: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP08 — unterminated array reports error
 * ---------------------------------------------------------------------- */
static void test_amp08_unterminated_array(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "tags := [1, 2, 3;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP08: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP08: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP08: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP09 — invalid blob tag reports error
 * ---------------------------------------------------------------------- */
static void test_amp09_invalid_blob_tag(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "bad := @1abc`content`;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP09: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP09: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP09: document reports an error");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * AMP10 — identifier followed by neither ':=' nor '{' reports error
 * ---------------------------------------------------------------------- */
static void test_amp10_bare_identifier_statement(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "name;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP10: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP10: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP10: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP11 — ':base' on ASSIGN rejected in AMP
 * ---------------------------------------------------------------------- */
static void test_amp11_base_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "base := 1;\n"
                                       "derived : base := 2;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP11: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP11: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP11: document reports an error");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "AMP11: err_code reports UNEXPECTED_TOKEN (the actual AMP-base gate)");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP12 — ANVL_STMT_OBJECT_BLOCK rejected in all three forms
 * ---------------------------------------------------------------------- */
static void test_amp12_object_block_rejected(void) {
   const char *buffers[3] = {
      "#!amp\n"
      "label {\n"
      "   field := 1;\n"
      "};\n",
      "#!amp\n"
      "config @[attrib] {\n"
      "   addr := 1;\n"
      "};\n",
      "#!amp\n"
      "base := 1;\n"
      "derived : base {\n"
      "   override := 2;\n"
      "};\n",
   };

   for (usize i = 0; i < 3; i++) {
      module_context ctx = NULL;
      module_document doc = setup_amp_doc(buffers[i], &ctx);
      TestBit.is_not_null(doc, "AMP12: document loaded");
      if (!doc) {
         continue;
      }

      anvl_err_code err_code = ANVL_ERR_NONE;
      anvl_result res = doc_parse_body(doc, &err_code);
      TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP12: object-block form rejected");
      TestBit.is_true(doc_has_errors(doc), "AMP12: document reports an error");

      mod_ctx_dispose(ctx);
   }
}
/* ---------------------------------------------------------------------- *
 * AMP13 — 'vars { ... }' rejected in AMP
 * ---------------------------------------------------------------------- */
static void test_amp13_vars_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "vars {\n"
                                       "   host := 1;\n"
                                       "};\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP13: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP13: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP13: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP14 — 'using "path";' rejected in AMP
 * ---------------------------------------------------------------------- */
static void test_amp14_using_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "using \"somewhere\";\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP14: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP14: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP14: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP15 — statement-level '@[...]' attributes rejected in AMP
 * Commentary: err_code is asserted specifically (not just ERR + has_errors)
 * because this test used to pass for the wrong reason — before statement
 * attributes were implemented, `@` right after an identifier just failed
 * the generic `:=` check (ANVL_ERR_PARSER_EXPECTED_ASSIGN), which also
 * happened to satisfy the old loose assertions without ever exercising
 * the actual AMP-attribute-forbidden gate. Same gotcha shape as AMP00b.
 * ---------------------------------------------------------------------- */
static void test_amp15_statement_attributes_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "server @[active] := 1;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP15: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP15: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP15: document reports an error");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                        "AMP15: err_code reports UNEXPECTED_TOKEN (the actual AMP-attribute gate)");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP16 — object-typed ASSIGN value rejected in AMP
 * ---------------------------------------------------------------------- */
static void test_amp16_object_value_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "config := { host := 1; };\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP16: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP16: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP16: document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP17 — scalar tuple assignment (resolved AMP-legal — see notes/
 * document-body-parse.md "Dialect scope"). Same footing as AMP05a's
 * scalar array, now that parse_tuple is implemented.
 * ---------------------------------------------------------------------- */
static void test_amp17_scalar_tuple_assign(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "coords := (10, 20, 30);\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP17: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP17: parse body returns OK");
   TestBit.is_equal_int(1, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP17: one statement captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP17: statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_TUPLE, (long long)stmt->value->type, "AMP17: value is TUPLE");
      TestBit.is_equal_int(3, (long long)List.size(stmt->value->collection.items),
                           "AMP17: tuple has three elements");
      anvl_value elem = NULL;
      List.get(stmt->value->collection.items, 0, (object *)&elem);
      if (elem) {
         TestBit.is_equal_int(ANVL_VALUE_NUMERIC, (long long)elem->type,
                              "AMP17: first element is NUMERIC");
      }
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP18 — empty array rejected
 * ---------------------------------------------------------------------- */
static void test_amp18_empty_array_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "tags := [];\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP18: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP18: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP18: document reports an error");
   TestBit.is_equal_int(ANVL_ERR_PARSER_ARRAY_CANNOT_BE_EMPTY, err_code,
                        "AMP18: err_code reports ARRAY_CANNOT_BE_EMPTY");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP19 — missing comma between array elements rejected
 * ---------------------------------------------------------------------- */
static void test_amp19_missing_comma_in_array_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "tags := [1 2 3];\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP19: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP19: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP19: document reports an error");
   TestBit.is_equal_int(ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY, err_code,
                        "AMP19: err_code reports MISSING_COMMA_IN_ARRAY");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP20 — nested collection as an array element rejected in AMP
 * Commentary: exercises parse_array's "reject on sight of the leading
 * symbol" behavior (see notes/document-body-parse.md) — rejected the
 * instant a `[`/`(`/`{` is seen in element position, never by generically
 * parsing the nested structure and checking its type afterward.
 * ---------------------------------------------------------------------- */
static void test_amp20_array_element_not_scalar_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "grid := [1, [2, 3]];\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP20: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP20: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP20: document reports an error");
   TestBit.is_equal_int(ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR, err_code,
                        "AMP20: err_code reports AMP_ARRAY_ELEMENT_NOT_SCALAR");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP21 — reserved keyword ('import'/'vars'/'using') rejected as a bare
 * value, not just as an identifier
 * Commentary: true/false/null are fine as values (AMP00a) — this is the
 * other half of the same invariant, for the RESERVED group specifically
 * (see notes/document-body-parse.md "Bare literal grammar"). `import`
 * deliberately isn't the document's first statement, for the same
 * header-scan-collision reason documented on AMP00b.
 * ---------------------------------------------------------------------- */
static void test_amp21_reserved_keyword_value_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "first := 1;\n"
                                       "second := import;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP21: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP21: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP21: document reports an error");
   TestBit.is_equal_int(ANVL_ERR_PARSER_VALUE_IS_KEYWORD, err_code,
                        "AMP21: err_code reports VALUE_IS_KEYWORD");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP22 — malformed exponent (sign with no digit after it) rejected
 * ---------------------------------------------------------------------- */
static void test_amp22_malformed_exponent_rejected(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "bad := 5e+;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP22: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP22: parse body returns ERR");
   TestBit.is_true(doc_has_errors(doc), "AMP22: document reports an error");
   TestBit.is_equal_int(ANVL_ERR_PARSER_INVALID_EXPONENT, err_code,
                        "AMP22: err_code reports INVALID_EXPONENT");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP23 — a second decimal point declines to a bare literal, not an error
 * Commentary: `1.5.5` isn't malformed — parse_numeric_literal declines and
 * rewinds rather than truncating to `1.5`, since this could be a version-
 * style bare literal instead (see notes/document-body-parse.md "Resolved
 * — decline-and-rewind vs. hard error"). Confirms the whole, untouched
 * span reaches parse_bare_literal rather than being partially consumed.
 * ---------------------------------------------------------------------- */
static void test_amp23_second_decimal_point_declines_to_bare(void) {
   module_context ctx = NULL;
   module_document doc = setup_amp_doc("#!amp\n"
                                       "version := 1.5.5;\n",
                                       &ctx);
   TestBit.is_not_null(doc, "AMP23: document loaded");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_parse_body(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "AMP23: parse body returns OK");
   TestBit.is_equal_int(1, (long long)FArray.capacity(doc->body, sizeof(anvl_statement)),
                        "AMP23: one statement captured");

   anvl_statement stmt = NULL;
   FArray.get(doc->body, 0, sizeof(anvl_statement), (object *)&stmt);
   TestBit.is_not_null(stmt, "AMP23: statement retrieved");
   if (stmt && stmt->value) {
      TestBit.is_equal_int(ANVL_VALUE_IDENTIFIER, (long long)stmt->value->type,
                           "AMP23: value declined numeric, parsed as IDENTIFIER instead");
      TestBit.is_true(slice_equals(stmt->value->text, "1.5.5"),
                      "AMP23: value text is the whole, untouched '1.5.5' span");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * AMP24 — '@'/'`' mid-bare-literal rejected, not silently truncated
 * Commentary: both are blob-dispatch leading symbols and were explicitly
 * decided *not* to be legal bare-literal continuation characters (see
 * notes/document-body-parse.md "Bare literal grammar"). err_code is
 * asserted specifically because the pre-fix behavior wasn't silent
 * acceptance — it was a real rejection, but for the *wrong* reason
 * (a truncated 'bar' bare literal "succeeded", then the leftover
 * '@baz'/'`baz' tripped an unrelated, confusing UNTERMINATED_STATEMENT
 * error instead of naming the actual problem).
 * ---------------------------------------------------------------------- */
static void test_amp24_bare_literal_rejects_at_and_backtick(void) {
   const char *buffers[2] = {
      "#!amp\n"
      "foo := bar@baz;\n",
      "#!amp\n"
      "foo := bar`baz;\n",
   };

   for (usize i = 0; i < 2; i++) {
      module_context ctx = NULL;
      module_document doc = setup_amp_doc(buffers[i], &ctx);
      TestBit.is_not_null(doc, "AMP24: document loaded");
      if (!doc) {
         continue;
      }

      anvl_err_code err_code = ANVL_ERR_NONE;
      anvl_result res = doc_parse_body(doc, &err_code);
      TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP24: parse body returns ERR");
      TestBit.is_true(doc_has_errors(doc), "AMP24: document reports an error");
      TestBit.is_equal_int(ANVL_ERR_PARSER_INVALID_BARE_LITERAL, err_code,
                           "AMP24: err_code reports INVALID_BARE_LITERAL");

      mod_ctx_dispose(ctx);
   }
}
/* ---------------------------------------------------------------------- *
 * AMP25 — '$identifier' VarRef rejected entirely in AMP, at both the
 * top-level value position and nested as an array element — matching
 * every other AMP-forbidden construct (base/attributes/object).
 * ---------------------------------------------------------------------- */
static void test_amp25_varref_rejected(void) {
   const char *buffers[2] = {
      "#!amp\n"
      "alias := $name;\n",
      "#!amp\n"
      "wrapped := [$name];\n",
   };

   for (usize i = 0; i < 2; i++) {
      module_context ctx = NULL;
      module_document doc = setup_amp_doc(buffers[i], &ctx);
      TestBit.is_not_null(doc, "AMP25: document loaded");
      if (!doc) {
         continue;
      }

      anvl_err_code err_code = ANVL_ERR_NONE;
      anvl_result res = doc_parse_body(doc, &err_code);
      TestBit.is_equal_int(ANVL_RES_ERR, res, "AMP25: parse body returns ERR");
      TestBit.is_true(doc_has_errors(doc), "AMP25: document reports an error");
      TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, err_code,
                           "AMP25: err_code reports UNEXPECTED_TOKEN");

      mod_ctx_dispose(ctx);
   }
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   // Instrumentation: report throughput for every parse in this run, attributed
   // to whichever test is currently running (via TestBit.log). Registered once,
   // process-wide, matching the rest of parser.c's current global state.
   anvl_parser_set_hook(report_throughput, NULL);

   TestBit.run_ex("AMP00a_keyword_literal_assign", NULL, test_amp00a_keyword_literal_assign, th);
   TestBit.run_ex("AMP00b_import_rejected_as_identifier", NULL,
                  test_amp00b_import_rejected_as_identifier, th);
   TestBit.run_ex("AMP00c_empty_body", NULL, test_amp00c_empty_body, th);
   TestBit.run_ex("AMP01a_integer_assign", NULL, test_amp01a_integer_assign, th);
   TestBit.run_ex("AMP01b_float_assign", NULL, test_amp01b_float_assign, th);
   TestBit.run_ex("AMP02_float_assign", NULL, test_amp02_float_assign, th);
   TestBit.run_ex("AMP03a_string_assign", NULL, test_amp03a_string_assign, th);
   TestBit.run_ex("AMP03b_bare_literal_assign", NULL, test_amp03b_bare_literal_assign, th);
   TestBit.run_ex("AMP04_blob_assign", NULL, test_amp04_blob_assign, th);
   TestBit.run_ex("AMP05a_scalar_array_assign", NULL, test_amp05a_scalar_array_assign, th);
   TestBit.run_ex("AMP05b_mixed_array_assign", NULL, test_amp05b_mixed_array_assign, th);
   TestBit.run_ex("AMP06_multiple_statements_in_order", NULL,
                  test_amp06_multiple_statements_in_order, th);
   TestBit.run_ex("AMP07_missing_value_after_assign", NULL, test_amp07_missing_value_after_assign,
                  th);
   TestBit.run_ex("AMP08_unterminated_array", NULL, test_amp08_unterminated_array, th);

   TestBit.run_ex("AMP09_invalid_blob_tag", NULL, test_amp09_invalid_blob_tag, th);

   TestBit.run_ex("AMP10_bare_identifier_statement", NULL, test_amp10_bare_identifier_statement,
                  th);
   TestBit.run_ex("AMP11_base_rejected", NULL, test_amp11_base_rejected, th);
   TestBit.run_ex("AMP12_object_block_rejected", NULL, test_amp12_object_block_rejected, th);
   TestBit.run_ex("AMP13_vars_rejected", NULL, test_amp13_vars_rejected, th);
   TestBit.run_ex("AMP14_using_rejected", NULL, test_amp14_using_rejected, th);
   TestBit.run_ex("AMP15_statement_attributes_rejected", NULL,
                  test_amp15_statement_attributes_rejected, th);
   TestBit.run_ex("AMP16_object_value_rejected", NULL, test_amp16_object_value_rejected, th);
   TestBit.run_ex("AMP17_scalar_tuple_assign", NULL, test_amp17_scalar_tuple_assign, th);
   TestBit.run_ex("AMP18_empty_array_rejected", NULL, test_amp18_empty_array_rejected, th);
   TestBit.run_ex("AMP19_missing_comma_in_array_rejected", NULL,
                  test_amp19_missing_comma_in_array_rejected, th);
   TestBit.run_ex("AMP20_array_element_not_scalar_rejected", NULL,
                  test_amp20_array_element_not_scalar_rejected, th);
   TestBit.run_ex("AMP21_reserved_keyword_value_rejected", NULL,
                  test_amp21_reserved_keyword_value_rejected, th);
   TestBit.run_ex("AMP22_malformed_exponent_rejected", NULL, test_amp22_malformed_exponent_rejected,
                  th);
   TestBit.run_ex("AMP23_second_decimal_point_declines_to_bare", NULL,
                  test_amp23_second_decimal_point_declines_to_bare, th);
   TestBit.run_ex("AMP24_bare_literal_rejects_at_and_backtick", NULL,
                  test_amp24_bare_literal_rejects_at_and_backtick, th);
   TestBit.run_ex("AMP25_varref_rejected", NULL, test_amp25_varref_rejected, th);

   return TestBit.report();
}
