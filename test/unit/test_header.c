/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_header.c - Unit tests for document header scanning                *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_header.c                                          *
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

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

/* ---------------------------------------------------------------------- *
 * HDR00 — empty header
 * ---------------------------------------------------------------------- */
static void test_hdr00_empty_header(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("name := test\n", &ctx);
   TestBit.is_not_null(doc, "HDR00: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR00: scan header returns OK");
   TestBit.is_equal_int(0, (long long)List.size(doc->header->imports),
                        "HDR00: imports list is empty");
   TestBit.is_equal_int(0, (long long)List.size(doc->header->attributes),
                        "HDR00: attributes list is empty");
   TestBit.is_equal_int(0, (long long)doc->source->pos, "HDR00: source position at start of body");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR01 — shebang detection
 * ---------------------------------------------------------------------- */
static void test_hdr01_shebang_detection(void) {
   module_context ctx = NULL;
   module_document doc = setup_registered_doc("#!amp\nname := a3f9\n", &ctx);
   TestBit.is_not_null(doc, "HDR01: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR01: scan header returns OK");
   TestBit.is_equal_int(ANVL_DIALECT_AMP, (long long)Source.dialect(doc->source),
                        "HDR01: shebang resolves dialect to AMP");
   TestBit.is_equal_int(0, (long long)List.size(doc->header->imports),
                        "HDR01: imports list is empty");
   TestBit.is_equal_int(6, (long long)doc->source->pos, "HDR01: source position at start of body");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR02 — multiple imports in order
 * ---------------------------------------------------------------------- */
static void test_hdr02_multiple_imports(void) {
   const char *buffer = "import \"base\";\nimport \"types\";\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR02: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR02: scan header returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->header->imports),
                        "HDR02: two imports captured");

   anvl_doc_import imp0 = NULL;
   List.get(doc->header->imports, 0, (object *)&imp0);
   TestBit.is_not_null(imp0, "HDR02: first import retrieved");
   if (imp0) {
      TestBit.is_true(slice_equals(doc->source, imp0->decl, "import \"base\""),
                      "HDR02: first import declaration captured");
      TestBit.is_true(slice_equals(doc->source, imp0->path, "\"base\""),
                      "HDR02: first import path captured");
   }

   anvl_doc_import imp1 = NULL;
   List.get(doc->header->imports, 1, (object *)&imp1);
   TestBit.is_not_null(imp1, "HDR02: second import retrieved");
   if (imp1) {
      TestBit.is_true(slice_equals(doc->source, imp1->decl, "import \"types\""),
                      "HDR02: second import declaration captured");
      TestBit.is_true(slice_equals(doc->source, imp1->path, "\"types\""),
                      "HDR02: second import path captured");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR03 — imports interleaved with comments
 * ---------------------------------------------------------------------- */
static void test_hdr03_imports_with_comments(void) {
   const char *buffer = "// load base\nimport \"base\";\n/* then types */\nimport \"types\";\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR03: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR03: scan header returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->header->imports),
                        "HDR03: two imports captured across comments");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR04 — missing semicolon after import
 * ---------------------------------------------------------------------- */
static void test_hdr04_import_missing_semicolon(void) {
   const char *buffer = "import \"base\"\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR04: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "HDR04: scan header returns ERR for missing semicolon");
   TestBit.is_true(Source.has_errors(doc->source), "HDR04: source reports an error");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR05 — missing quotes around import path
 * ---------------------------------------------------------------------- */
static void test_hdr05_import_missing_quotes(void) {
   const char *buffer = "import base;\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR05: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "HDR05: scan header returns ERR for unquoted path");
   TestBit.is_true(Source.has_errors(doc->source), "HDR05: source reports an error");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR06 — invalid shebang dialect
 * ---------------------------------------------------------------------- */
static void test_hdr06_invalid_shebang(void) {
   const char *buffer = "#!xyz\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR06: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "HDR06: scan header returns ERR for invalid shebang");
   TestBit.is_true(Source.has_errors(doc->source), "HDR06: source reports an error");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR07 — unterminated comment before shebang
 * ---------------------------------------------------------------------- */
static void test_hdr07_unterminated_comment_before_shebang(void) {
   const char *buffer = "/* shebang soon\n#!aml\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR07: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "HDR07: scan header returns ERR for unterminated comment");
   TestBit.is_true(Source.has_errors(doc->source), "HDR07: source reports an error");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR08 — module attributes after imports
 * ---------------------------------------------------------------------- */
static void test_hdr08_attributes_after_imports(void) {
   const char *buffer = "import \"base\";\n@[schema, schema_version=1]\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR08: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR08: scan header returns OK");
   TestBit.is_equal_int(1, (long long)List.size(doc->header->imports),
                        "HDR08: one import captured");
   TestBit.is_equal_int(2, (long long)List.size(doc->header->attributes),
                        "HDR08: two attributes captured");

   anvl_doc_attribute attr0 = NULL;
   List.get(doc->header->attributes, 0, (object *)&attr0);
   TestBit.is_not_null(attr0, "HDR08: first attribute retrieved");
   if (attr0) {
      TestBit.is_true(slice_equals(doc->source, attr0->key, "schema"),
                      "HDR08: first attribute key is schema");
      TestBit.is_true(slice_is_empty(attr0->value), "HDR08: first attribute is flag");
   }

   anvl_doc_attribute attr1 = NULL;
   List.get(doc->header->attributes, 1, (object *)&attr1);
   TestBit.is_not_null(attr1, "HDR08: second attribute retrieved");
   if (attr1) {
      TestBit.is_true(slice_equals(doc->source, attr1->key, "schema_version"),
                      "HDR08: second attribute key is schema_version");
      TestBit.is_true(slice_equals(doc->source, attr1->value, "1"),
                      "HDR08: second attribute value is 1");
   }

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR09 — import after attribute fails (ordering violation)
 * ---------------------------------------------------------------------- */
static void test_hdr09_import_after_attribute_fails(void) {
   const char *buffer = "@[schema]\nimport \"base\";\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR09: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "HDR09: scan header returns ERR for import after attribute");
   TestBit.is_true(Source.has_errors(doc->source), "HDR09: source reports an error");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR10 — first body statement terminates header scan
 * ---------------------------------------------------------------------- */
static void test_hdr10_body_statement_terminates_header(void) {
   const char *buffer = "import \"base\";\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR10: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR10: scan header returns OK");
   TestBit.is_equal_int(2, (long long)doc->source->line, "HDR10: source line at body start");
   TestBit.is_equal_int(1, (long long)doc->source->col, "HDR10: source column at body start");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("HDR00_empty_header", NULL, test_hdr00_empty_header, th);
   TestBit.run_ex("HDR01_shebang_detection", NULL, test_hdr01_shebang_detection, th);
   TestBit.run_ex("HDR02_multiple_imports", NULL, test_hdr02_multiple_imports, th);
   TestBit.run_ex("HDR03_imports_with_comments", NULL, test_hdr03_imports_with_comments, th);
   TestBit.run_ex("HDR04_import_missing_semicolon", NULL, test_hdr04_import_missing_semicolon, th);
   TestBit.run_ex("HDR05_import_missing_quotes", NULL, test_hdr05_import_missing_quotes, th);
   TestBit.run_ex("HDR06_invalid_shebang", NULL, test_hdr06_invalid_shebang, th);
   TestBit.run_ex("HDR07_unterminated_comment_before_shebang", NULL,
                  test_hdr07_unterminated_comment_before_shebang, th);
   TestBit.run_ex("HDR08_attributes_after_imports", NULL, test_hdr08_attributes_after_imports, th);
   TestBit.run_ex("HDR09_import_after_attribute_fails", NULL,
                  test_hdr09_import_after_attribute_fails, th);
   TestBit.run_ex("HDR10_body_statement_terminates_header", NULL,
                  test_hdr10_body_statement_terminates_header, th);

   return TestBit.report();
}
