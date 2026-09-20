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

#define ENABLED 0

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
   TestBit.is_equal_int(0, (long long)List.size(doc->header->includes),
                        "HDR00: includes list is empty");
   TestBit.is_equal_int(0, (long long)List.size(doc->header->attributes),
                        "HDR00: attributes list is empty");
   TestBit.is_equal_int(0, (long long)doc->source->pos, "HDR00: source position at start of body");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR02 — single include statement
 * ---------------------------------------------------------------------- */
static void test_hdr01_single_include(void) {
   const char *buffer = "include \"base\";\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR01: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR01: scan header returns OK");
   TestBit.is_equal_int(1, (long long)List.size(doc->header->includes),
                        "HDR01: one include captured");

   /*
      typedef struct anvl_include_t {
         anvl_slice decl;          // "include \"path\"" (no trailing ';')
         anvl_slice path;          // "\"path\"" (quotes included)
         module_document resolved; // child document after include graph expansion; NULL until loaded
      } anvl_include_t;
      typedef anvl_include_t *anvl_include;
    */
   const char *exp_decl = "include \"base\"";
   const char *exp_path = "\"base\"";
   anvl_include inc = NULL;
   List.get(doc->header->includes, 0, (object *)&inc);
   TestBit.is_not_null(inc, "HDR01: include retrieved");
   if (inc) {
      // +1 headroom on each: Source.substring null-terminates at out_buffer[len], so a buffer
      // sized to the content length exactly overflows by one byte (a latent, pre-existing bug
      // here -- silent with the shorter "import" keyword, but a real stack overflow once the
      // content grew by one character to "include").
      char act_decl[16] = {0};
      Source.substring(inc->decl, act_decl);
      char act_path[8] = {0};
      Source.substring(inc->path, act_path);

      TestBit.is_equal_str(exp_decl, act_decl, "HDR01: include declaration captured");
      TestBit.is_equal_str(exp_path, act_path, "HDR01: include path captured");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR02 — multiple includes in order
 * ---------------------------------------------------------------------- */
static void test_hdr02_multiple_includes(void) {
   const char *buffer = "include \"base\";\ninclude \"types\";\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR02: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR02: scan header returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->header->includes),
                        "HDR02: two includes captured");

   anvl_include inc0 = NULL;
   List.get(doc->header->includes, 0, (object *)&inc0);
   TestBit.is_not_null(inc0, "HDR02: first include retrieved");
   if (inc0) {
      TestBit.is_true(slice_equals(inc0->decl, "include \"base\""),
                      "HDR02: first include declaration captured");
      TestBit.is_true(slice_equals(inc0->path, "\"base\""), "HDR02: first include path captured");
   }

   anvl_include inc1 = NULL;
   List.get(doc->header->includes, 1, (object *)&inc1);
   TestBit.is_not_null(inc1, "HDR02: second include retrieved");
   if (inc1) {
      TestBit.is_true(slice_equals(inc1->decl, "include \"types\""),
                      "HDR02: second include declaration captured");
      TestBit.is_true(slice_equals(inc1->path, "\"types\""), "HDR02: second include path captured");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *            \
 * HDR03 — includes interleaved with comments                                         \
 * ---------------------------------------------------------------------- */
static void test_hdr03_includes_with_comments(void) {
   const char *buffer = "// load base\ninclude \"base\";\n/* then types */\ninclude \"types\";\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR03: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR03: scan header returns OK");
   TestBit.is_equal_int(2, (long long)List.size(doc->header->includes),
                        "HDR03: two includes captured across comments");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR04 — missing semicolon after include
 * ---------------------------------------------------------------------- */
static void test_hdr04_include_missing_semicolon(void) {
   const char *buffer = "include \"base\"\nname := test\n";
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
 * HDR05 — missing quotes around include path
 * ---------------------------------------------------------------------- */
static void test_hdr05_include_missing_quotes(void) {
   const char *buffer = "include base;\nname := test\n";
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
 * HDR08 — module attributes after includes
 * ---------------------------------------------------------------------- */
static void test_hdr08_attributes_after_includes(void) {
   const char *buffer = "include \"base\";\n@[schema, schema_version=1]\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR08: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR08: scan header returns OK");
   TestBit.is_equal_int(1, (long long)List.size(doc->header->includes),
                        "HDR08: one include captured");
   TestBit.is_equal_int(2, (long long)List.size(doc->header->attributes),
                        "HDR08: two attributes captured");

   anvl_attribute attr0 = NULL;
   List.get(doc->header->attributes, 0, (object *)&attr0);
   TestBit.is_not_null(attr0, "HDR08: first attribute retrieved");
   if (attr0) {
      TestBit.is_true(slice_equals(attr0->key, "schema"), "HDR08: first attribute key is schema");
      TestBit.is_true(Source.slice_is_empty(attr0->value), "HDR08: first attribute is flag");
   }

   anvl_attribute attr1 = NULL;
   List.get(doc->header->attributes, 1, (object *)&attr1);
   TestBit.is_not_null(attr1, "HDR08: second attribute retrieved");
   if (attr1) {
      TestBit.is_true(slice_equals(attr1->key, "schema_version"),
                      "HDR08: second attribute key is schema_version");
      TestBit.is_true(slice_equals(attr1->value, "1"), "HDR08: second attribute value is 1");
   }

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR09 — include after attribute fails (ordering violation)
 * ---------------------------------------------------------------------- */
static void test_hdr09_include_after_attribute_fails(void) {
   const char *buffer = "@[schema]\ninclude \"base\";\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR09: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "HDR09: scan header returns ERR for include after attribute");
   TestBit.is_true(Source.has_errors(doc->source), "HDR09: source reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR10 — first body statement terminates header scan
 * ---------------------------------------------------------------------- */
static void test_hdr10_body_statement_terminates_header(void) {
   const char *buffer = "include \"base\";\nname := test\n";
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
 * HDR11 — include loader: single include resolves
 * ---------------------------------------------------------------------- */
static void test_hdr11_include_loader_single(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_include_single.anvl", &ctx);
   TestBit.is_not_null(root, "HDR11: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(root, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR11: scan header returns OK");

   res = mod_load_includes(ctx, root, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR11: load includes returns OK");
   TestBit.is_equal_int(1, (long long)List.size(root->header->includes),
                        "HDR11: one include captured");

   anvl_include inc = NULL;
   List.get(root->header->includes, 0, (object *)&inc);
   TestBit.is_not_null(inc, "HDR11: include retrieved");
   if (inc) {
      TestBit.is_not_null(inc->resolved, "HDR11: include resolved to child document");
      if (inc->resolved) {
         TestBit.is_not_null(inc->resolved->source, "HDR11: resolved document has source");
      }
   }
   TestBit.is_equal_int(2, (long long)List.size(ctx->docs),
                        "HDR11: two documents registered (root + child)");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR12 — include loader: nested includes resolve recursively
 * ---------------------------------------------------------------------- */
static void test_hdr12_include_loader_nested(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_include_nested.anvl", &ctx);
   TestBit.is_not_null(root, "HDR12: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(root, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR12: scan header returns OK");

   res = mod_load_includes(ctx, root, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR12: load includes returns OK");
   TestBit.is_equal_int(1, (long long)List.size(root->header->includes),
                        "HDR12: root has one include");

   anvl_include root_inc = NULL;
   List.get(root->header->includes, 0, (object *)&root_inc);
   TestBit.is_not_null(root_inc, "HDR12: root include retrieved");
   if (root_inc) {
      TestBit.is_not_null(root_inc->resolved, "HDR12: root include resolved to types document");
   }
   if (!root_inc || !root_inc->resolved) {
      mod_ctx_dispose(ctx);
      return;
   }

   module_document types_doc = root_inc->resolved;
   TestBit.is_equal_int(1, (long long)List.size(types_doc->header->includes),
                        "HDR12: types document has one include");

   anvl_include types_inc = NULL;
   List.get(types_doc->header->includes, 0, (object *)&types_inc);
   TestBit.is_not_null(types_inc, "HDR12: types include retrieved");
   if (types_inc) {
      TestBit.is_not_null(types_inc->resolved, "HDR12: nested include resolved to base document");
   }
   TestBit.is_equal_int(3, (long long)List.size(ctx->docs),
                        "HDR12: three documents registered (root + types + base)");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR13 — include loader: diamond include reuses the same document
 * ---------------------------------------------------------------------- */
static void test_hdr13_include_loader_diamond(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_include_diamond.anvl", &ctx);
   TestBit.is_not_null(root, "HDR13: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(root, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR13: scan header returns OK");

   res = mod_load_includes(ctx, root, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR13: load includes returns OK");
   TestBit.is_equal_int(2, (long long)List.size(root->header->includes),
                        "HDR13: root has two includes");

   anvl_include base_inc = NULL;
   anvl_include types_inc = NULL;
   List.get(root->header->includes, 0, (object *)&base_inc);
   List.get(root->header->includes, 1, (object *)&types_inc);
   TestBit.is_not_null(base_inc, "HDR13: base include retrieved");
   TestBit.is_not_null(types_inc, "HDR13: types include retrieved");
   if (base_inc) {
      TestBit.is_not_null(base_inc->resolved, "HDR13: base include resolved to child document");
   }
   if (types_inc) {
      TestBit.is_not_null(types_inc->resolved, "HDR13: types include resolved to child document");
   }
   if (!base_inc || !base_inc->resolved || !types_inc || !types_inc->resolved) {
      mod_ctx_dispose(ctx);
      return;
   }

   anvl_include types_base_inc = NULL;
   List.get(types_inc->resolved->header->includes, 0, (object *)&types_base_inc);
   TestBit.is_not_null(types_base_inc, "HDR13: types->base include retrieved");
   if (types_base_inc) {
      TestBit.is_true(base_inc->resolved == types_base_inc->resolved,
                      "HDR13: diamond base include resolves to the same document");
   }
   TestBit.is_equal_int(3, (long long)List.size(ctx->docs),
                        "HDR13: three documents registered (root + base + types)");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR14 — include loader: cyclic include is rejected
 * ---------------------------------------------------------------------- */
static void test_hdr14_include_loader_cycle(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_include_self.anvl", &ctx);
   TestBit.is_not_null(root, "HDR14: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(root, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR14: scan header returns OK");

   res = mod_load_includes(ctx, root, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "HDR14: cyclic include returns ERR");
   TestBit.is_true(doc_has_errors(root), "HDR14: root document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR15 — include loader: missing include file is reported
 * ---------------------------------------------------------------------- */
static void test_hdr15_include_loader_missing(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_include_missing.anvl", &ctx);
   TestBit.is_not_null(root, "HDR15: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(root, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR15: scan header returns OK");

   res = mod_load_includes(ctx, root, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "HDR15: missing include returns ERR");
   TestBit.is_true(doc_has_errors(root), "HDR15: root document reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR16 — include loader: buffer-rooted document (no real file behind it)
 * resolves includes relative to the process's current working directory
 * ---------------------------------------------------------------------- */
static void test_hdr16_include_loader_buffer_root_cwd(void) {
   module_context ctx = NULL;
   module_document root =
      setup_registered_doc("include \"../fixtures/hdr_include_base.anvl\";\n", &ctx);
   TestBit.is_not_null(root, "HDR16: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(root, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR16: scan header returns OK");

   res = mod_load_includes(ctx, root, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR16: buffer-rooted include resolves relative to CWD");

   anvl_include inc = NULL;
   List.get(root->header->includes, 0, (object *)&inc);
   TestBit.is_not_null(inc, "HDR16: include retrieved");
   if (inc) {
      TestBit.is_not_null(inc->resolved, "HDR16: include resolved to child document");
   }
   TestBit.is_equal_int(2, (long long)List.size(ctx->docs),
                        "HDR16: two documents registered (root + child)");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR17 — include loader: '../' resolves relative to the includeing file's
 * own directory, not the original root's
 * ---------------------------------------------------------------------- */
static void test_hdr17_include_loader_dotdot(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_include_sub/hdr_include_dotdot.anvl", &ctx);
   TestBit.is_not_null(root, "HDR17: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(root, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR17: scan header returns OK");

   res = mod_load_includes(ctx, root, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR17: '../' include resolves");

   anvl_include inc = NULL;
   List.get(root->header->includes, 0, (object *)&inc);
   TestBit.is_not_null(inc, "HDR17: include retrieved");
   if (inc) {
      TestBit.is_not_null(inc->resolved, "HDR17: '../' include resolved to child document");
   }
   TestBit.is_equal_int(2, (long long)List.size(ctx->docs),
                        "HDR17: two documents registered (root + child)");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR18 — a repeated shebang after the one already reconciled at
 * source-load time is rejected
 * ---------------------------------------------------------------------- */
static void test_hdr18_repeated_shebang_rejected(void) {
   const char *buffer = "#!aml\n#!aml\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR18: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "HDR18: scan header returns ERR for repeated shebang");
   TestBit.is_equal_int(ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS, err_code,
                        "HDR18: err_code is SHEBANG_AFTER_STATEMENTS");
   TestBit.is_true(Source.has_errors(doc->source), "HDR18: source reports an error");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * HDR19 — include loader: body size hint sums every registered document
 * exactly once, including diamond-shared includes.
 * Commentary: rather than hardcoding fixture byte counts (fragile if the
 * fixtures change), the expected total is computed independently by
 * walking ctx->docs and summing Source.length() per document. That walk
 * can't double-count a diamond include (ctx->docs only ever holds one entry
 * per registered document), so it's exactly the ground truth the threaded
 * accumulator needs to match.
 * ---------------------------------------------------------------------- */
static void test_hdr19_include_loader_body_size_hint(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_include_diamond.anvl", &ctx);
   TestBit.is_not_null(root, "HDR19: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(root, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR19: scan header returns OK");

   usize size_hint = 0;
   res = mod_load_includes(ctx, root, &size_hint, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "HDR19: load includes returns OK");
   TestBit.is_equal_int(3, (long long)List.size(ctx->docs),
                        "HDR19: three documents registered (root + base + types)");

   usize expected = 0;
   for (usize i = 0; i < List.size(ctx->docs); i++) {
      module_document doc = NULL;
      List.get(ctx->docs, i, (object *)&doc);
      if (doc) {
         expected += Source.length(doc->source);
      }
   }
   TestBit.is_true(expected > 0, "HDR19: expected total is non-zero");
   TestBit.is_equal_int((long long)expected, (long long)size_hint,
                        "HDR19: size hint matches the sum over ctx->docs, diamond counted once");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR20 — registry survives disposal of a discarded diamond duplicate
 * Commentary: when types.anvl re-includes base.anvl, include_load_child
 * registers a fresh `child` document, gets ANVL_ERR_PARSER_DUPLICATE_FIELD_
 * IN_OBJECT back (base.anvl's content hash is already registered), reuses
 * the *original* base document, and disposes the discarded duplicate via
 * doc_dispose. doc_dispose unconditionally calls Registry.remove(hash) for
 * whatever source it's disposing — since the duplicate shares base.anvl's
 * exact content hash, this removes the registry entry for the *original*
 * base document too, even though that original is still alive in ctx->docs.
 * From that point, any Source.*-registry-lookup call (get_arena, new_node,
 * set_error, has_errors) silently fails for the original base document,
 * even though it's never been disposed. This is the direct pressure-point
 * test: after a diamond include fully resolves, the surviving document must
 * still be findable in the registry by its own hash.
 * ---------------------------------------------------------------------- */
static void test_hdr20_registry_survives_diamond_duplicate_dispose(void) {
   module_context ctx = NULL;
   module_document root = setup_registered_file("hdr_include_diamond.anvl", &ctx);
   TestBit.is_not_null(root, "HDR20: root document loaded");
   if (!root) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, doc_scan_header(root, &err_code),
                        "HDR20: scan header returns OK");
   TestBit.is_equal_int(ANVL_RES_OK, mod_load_includes(ctx, root, NULL, &err_code),
                        "HDR20: load includes returns OK");

   anvl_include base_inc = NULL;
   List.get(root->header->includes, 0, (object *)&base_inc);
   TestBit.is_not_null(base_inc, "HDR20: base include retrieved");
   if (!base_inc || !base_inc->resolved) {
      mod_ctx_dispose(ctx);
      return;
   }

   module_document base = base_inc->resolved;
   uint64_t base_hash = Source.hash(base->source);
   TestBit.is_true(base_hash != 0, "HDR20: base document has a non-zero content hash");

   module_document found = Registry.find(base_hash);
   TestBit.is_not_null(found,
                       "HDR20: base document is still findable in the registry after the "
                       "diamond re-include's discarded duplicate was disposed");
   TestBit.is_true(found == base,
                   "HDR20: the registry entry is the surviving base document, not a stale "
                   "or wrong pointer");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * HDR21 — an unrecognized shebang dialect is a real header error, not a
 * silent fall-through to AML-permissive behavior
 * ---------------------------------------------------------------------- */
static void test_hdr21_invalid_shebang_dialect_rejected(void) {
   const char *buffer = "#!xyz\nname := test\n";
   module_context ctx = NULL;
   module_document doc = setup_registered_doc(buffer, &ctx);
   TestBit.is_not_null(doc, "HDR21: registered document allocated");
   if (!doc) {
      return;
   }

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_scan_header(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "HDR21: scan header returns ERR for invalid dialect");
   TestBit.is_equal_int(ANVL_ERR_PARSER_INVALID_SHEBANG_DIALECT, err_code,
                        "HDR21: err_code is INVALID_SHEBANG_DIALECT");
   TestBit.is_true(Source.has_errors(doc->source), "HDR21: source reports an error");

   mod_ctx_dispose(ctx);
}
/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("HDR00_empty_header", NULL, test_hdr00_empty_header, th);
   TestBit.run_ex("HDR01_single_include", NULL, test_hdr01_single_include, th);

   TestBit.run_ex("HDR02_multiple_includes", NULL, test_hdr02_multiple_includes, th);
   TestBit.run_ex("HDR03_includes_with_comments", NULL, test_hdr03_includes_with_comments, th);
   TestBit.run_ex("HDR04_include_missing_semicolon", NULL, test_hdr04_include_missing_semicolon, th);
   TestBit.run_ex("HDR05_include_missing_quotes", NULL, test_hdr05_include_missing_quotes, th);
   TestBit.run_ex("HDR07_unterminated_comment_before_shebang", NULL,
                  test_hdr07_unterminated_comment_before_shebang, th);
   TestBit.run_ex("HDR08_attributes_after_includes", NULL, test_hdr08_attributes_after_includes, th);
   TestBit.run_ex("HDR09_include_after_attribute_fails", NULL,
                  test_hdr09_include_after_attribute_fails, th);
   TestBit.run_ex("HDR10_body_statement_terminates_header", NULL,
                  test_hdr10_body_statement_terminates_header, th);
   TestBit.run_ex("HDR11_include_loader_single", NULL, test_hdr11_include_loader_single, th);
   TestBit.run_ex("HDR12_include_loader_nested", NULL, test_hdr12_include_loader_nested, th);
   TestBit.run_ex("HDR13_include_loader_diamond", NULL, test_hdr13_include_loader_diamond, th);
   TestBit.run_ex("HDR14_include_loader_cycle", NULL, test_hdr14_include_loader_cycle, th);
   TestBit.run_ex("HDR15_include_loader_missing", NULL, test_hdr15_include_loader_missing, th);
   TestBit.run_ex("HDR16_include_loader_buffer_root_cwd", NULL,
                  test_hdr16_include_loader_buffer_root_cwd, th);
   TestBit.run_ex("HDR17_include_loader_dotdot", NULL, test_hdr17_include_loader_dotdot, th);
   TestBit.run_ex("HDR18_repeated_shebang_rejected", NULL, test_hdr18_repeated_shebang_rejected,
                  th);
   TestBit.run_ex("HDR19_include_loader_body_size_hint", NULL,
                  test_hdr19_include_loader_body_size_hint, th);
   TestBit.run_ex("HDR20_registry_survives_diamond_duplicate_dispose", NULL,
                  test_hdr20_registry_survives_diamond_duplicate_dispose, th);
   TestBit.run_ex("HDR21_invalid_shebang_dialect_rejected", NULL,
                  test_hdr21_invalid_shebang_dialect_rejected, th);

   return TestBit.report();
}
