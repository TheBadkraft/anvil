/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_document.c - Unit tests for document/source lifecycle             *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_document.c                                        *
 * ********************************************************************** */

#include "anvil.h"
#include "types.h"
#include "internal/constants.h"
#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "internal/files.h"
#include "testbit.h"
#include "std.h"
// ----------------
#include "../utilities/debug.h"
#include "../utilities/helpers.h"
#include <sigma/list.h>
#include <sigma/map.h>
#include <sigma/memory.h>
#include <sigma/types.h>

static void td(void) { (void)reset_context_spec_defaults(NULL); }

/* ---------------------------------------------------------------------- *
 * SRC00 — source_create empty shell
 * Commentary: verifies that a source object can be created without a
 * filepath and that it reports the default dialect (AML after refactor).
 * ---------------------------------------------------------------------- */
static void test_src00_source_create_empty(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = Source.create(&src, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC00: source_create returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC00: err_code remains NONE");
   TestBit.is_not_null(src, "SRC00: source object is allocated");
   TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)src->dialect, "SRC00: default dialect is AML");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC01 — source_create null outputs
 * ---------------------------------------------------------------------- */
static void test_src01_source_create_null_outputs(void) {
   anvl_result res = Source.create(NULL, NULL);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC01: source_create returns ERR for null outputs");
}
/* ---------------------------------------------------------------------- *
 * SRC02 — source_dispose null is no-op
 * ---------------------------------------------------------------------- */
static void test_src02_source_dispose_null(void) {
   Source.dispose(NULL);
   TestBit.is_true(true, "SRC02: source_dispose(NULL) does not crash");
}
/* ---------------------------------------------------------------------- *
 * SRC03 — source_from_buffer copies content
 * ---------------------------------------------------------------------- */
static void test_src03_source_from_buffer(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "#!aml\n\nname := test\n";
   usize len = strlen(buffer);

   Source.create(&src, &err_code);

   anvl_result res = Source.from_buffer(&src, buffer, len, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC03: source_from_buffer returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC03: err_code remains NONE");
   TestBit.is_not_null(src, "SRC03: source object is allocated");
   TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)Source.dialect(src),
                        "SRC03: dialect is AML after shebang");
   TestBit.is_equal_int((long long)len, (long long)src->length,
                        "SRC03: source length matches buffer length");
   // check source buffer stride
   TestBit.is_equal_int(1, (long long)src->stride, "SRC03: source stride is 1 for byte buffer");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC04 — source_from_buffer invariants:
 *   [a] null buffer returns error
 *   [b] 0 length returns ok
 *   [c] null out_src returns error
 * ---------------------------------------------------------------------- */
static void test_src04a_source_from_buffer_null_buffer(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);

   // invarant: null buffer test must have a valid length (non-zero) to trigger the null buffer
   // check
   anvl_result res = Source.from_buffer(&src, NULL, 5, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "SRC04a: source_from_buffer returns ERR for null buffer");
   TestBit.is_equal_int(ANVL_ERR_INVALID_ARGUMENT, err_code,
                        "SRC04a: err_code is INVALID_ARGUMENT");
   TestBit.is_not_null(src, "SRC04a: out_src is left unchanged on null buffer failure");

   Source.dispose(src);
}
static void test_src04b_source_from_buffer_zero_length(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);

   // invarant: zero length test must have a valid buffer to trigger the zero length check
   const char *buffer = "";
   anvl_result res = Source.from_buffer(&src, buffer, 0, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC04b: source_from_buffer returns OK for zero length");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC04b: err_code is NONE for zero length");
   TestBit.is_not_null(src, "SRC04b: out_src is left unchanged on zero length failure");
   TestBit.is_equal_int(0, (long long)src->length,
                        "SRC04b: source length is 0 for zero length buffer");
   TestBit.is_true(((char *)src->buffer.bucket)[0] == '\0',
                   "SRC04b: source buffer is null-terminated for zero length buffer");

   Source.dispose(src);
}
static void test_src04c_source_from_buffer_null_out_src(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "foo";
   usize len = strlen(buffer);

   anvl_result res = Source.from_buffer(&src, buffer, len, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "SRC04c: source_from_buffer returns ERR for null out_src");
   TestBit.is_equal_int(ANVL_ERR_INVALID_ARGUMENT, err_code,
                        "SRC04c: err_code is INVALID_ARGUMENT for null out_src");

   // src is NULL, no need to dispose
}

/* ---------------------------------------------------------------------- *
 * SRC05 — source_from_file loads fixture invariants:
 *   - [a] `.anvl` extension  HINT: `ANVL_DIALECT_AML`
 *   - [b] `.aml` extension   HINT: `ANVL_DIALECT_AML`
 *   - [c] `.amp` extension   HINT: `ANVL_DIALECT_AMP`
 * ---------------------------------------------------------------------- */
static void test_src05a_source_from_file(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *filepath = "../../test/fixtures/f01_bare_literal.anvl";
   usize exp_len = Files.length(filepath);

   Source.create(&src, &err_code);

   anvl_result res = Source.from_file(&src, filepath, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC05a: source_from_file returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC05a: err_code remains NONE");
   TestBit.is_not_null(src, "SRC05a: source object is allocated");
   TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)Source.dialect(src),
                        "SRC05a: dialect is AML for .anvl fixture");
   TestBit.is_equal_int(exp_len, src->length, "SRC05a: source length is correct for fixture");
   // check source buffer stride
   TestBit.is_equal_int(1, (long long)src->stride, "SRC05a: source stride is 1 for byte buffer");

   Source.dispose(src);
}
static void test_src05b_source_from_file_aml(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *filepath = "../../test/fixtures/f00c_hint.aml";
   usize exp_len = Files.length(filepath);

   Source.create(&src, &err_code);

   anvl_result res = Source.from_file(&src, filepath, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC05b: source_from_file returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC05b: err_code remains NONE");
   TestBit.is_not_null(src, "SRC05b: source object is allocated");
   TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)Source.dialect(src),
                        "SRC05b: dialect is AML for .aml fixture");
   TestBit.is_equal_int(exp_len, src->length, "SRC05b: source length is correct for fixture");
   // check source buffer stride
   TestBit.is_equal_int(1, (long long)src->stride, "SRC05b: source stride is 1 for byte buffer");

   Source.dispose(src);
}
static void test_src05c_source_from_file_amp(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *filepath = "../../test/fixtures/f00d_hint.amp";
   usize exp_len = Files.length(filepath);

   Source.create(&src, &err_code);

   anvl_result res = Source.from_file(&src, filepath, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC05c: source_from_file returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "SRC05c: err_code remains NONE");
   TestBit.is_not_null(src, "SRC05c: source object is allocated");
   TestBit.is_equal_int(ANVL_DIALECT_AMP, (long long)Source.dialect(src),
                        "SRC05c: dialect is AMP for .amp fixture");
   TestBit.is_equal_int(exp_len, src->length, "SRC05c: source length is correct for fixture");
   // check source buffer stride
   TestBit.is_equal_int(1, (long long)src->stride, "SRC05c: source stride is 1 for byte buffer");

   Source.dispose(src);
}
/* ---------------------------------------------------------------------- *
 * SRC06 — source_from_file invariants:
 *   [a] null path returns error
 *   [b] non-existent file returns error
 *   [c] null out_src returns error
 * ---------------------------------------------------------------------- */
static void test_src06a_source_from_file_null_path(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);

   anvl_result res = Source.from_file(&src, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC06a: source_from_file returns ERR for null path");
   TestBit.is_equal_int(ANVL_ERR_IO_INVALID_PATH, err_code, "SRC06a: err_code is INVALID_ARGUMENT");
   TestBit.is_not_null(src, "SRC06a: out_src is left unchanged on null path failure");

   Source.dispose(src);
}
static void test_src06b_source_from_file_nonexistent(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);

   anvl_result res = Source.from_file(&src, "nonexistent_file.anvl", &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res,
                        "SRC06b: source_from_file returns ERR for non-existent file");
   TestBit.is_equal_int(ANVL_ERR_IO_FILE_NOT_FOUND, err_code, "SRC06b: err_code is FILE_NOT_FOUND");
   TestBit.is_not_null(src, "SRC06b: out_src is left unchanged on non-existent file failure");

   Source.dispose(src);
}
static void test_src06c_source_from_file_null_out_src(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *filepath = "../../test/fixtures/f01_bare_literal.anvl";

   anvl_result res = Source.from_file(NULL, filepath, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC06c: source_from_file returns ERR for null out_src");
   TestBit.is_equal_int(ANVL_ERR_INVALID_ARGUMENT, err_code,
                        "SRC06c: err_code is INVALID_ARGUMENT for null out_src");

   // src is NULL, no need to dispose
}

/* ---------------------------------------------------------------------- *
 * SRC07 — source content hash
 * Commentary: every source with loaded content carries a stable FNV-1a
 * 64-bit hash. empty/unloaded sources report 0.
 * ---------------------------------------------------------------------- */
static void test_src07a_source_hash_empty(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   TestBit.is_equal_int(0, (long long)Source.hash(src),
                        "SRC07a: hash is 0 before content is loaded");

   Source.dispose(src);
}
static void test_src07b_source_hash_stable(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "name := test\n";
   usize len = strlen(buffer);

   Source.create(&src, &err_code);
   Source.from_buffer(&src, buffer, len, &err_code);

   uint64_t h1 = Source.hash(src);
   TestBit.is_true(h1 != 0, "SRC07b: hash is non-zero after buffer load");

   anvl_source src2 = NULL;
   Source.create(&src2, &err_code);
   Source.from_buffer(&src2, buffer, len, &err_code);
   TestBit.is_equal_int((long long)h1, (long long)Source.hash(src2),
                        "SRC07b: identical buffers produce identical hashes");

   Source.dispose(src);
   Source.dispose(src2);
}
static void test_src07c_source_hash_different_content(void) {
   anvl_source src1 = NULL;
   anvl_source src2 = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src1, &err_code);
   Source.create(&src2, &err_code);
   Source.from_buffer(&src1, "alpha", 5, &err_code);
   Source.from_buffer(&src2, "beta", 4, &err_code);

   TestBit.is_true(Source.hash(src1) != Source.hash(src2),
                   "SRC07c: different buffers produce different hashes");

   Source.dispose(src1);
   Source.dispose(src2);
}
static void test_src07d_source_hash_from_file(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *filepath = "../../test/fixtures/f01_bare_literal.anvl";

   Source.create(&src, &err_code);
   Source.from_file(&src, filepath, &err_code);

   TestBit.is_true(Source.hash(src) != 0, "SRC07d: file-loaded source has non-zero hash");

   Source.dispose(src);
}

/* ---------------------------------------------------------------------- *
 * SRC08 — source error routing via registry lookup
 * Commentary: parser/scanner code works with anvl_source handles. Error
 * state is stored on the owning document's context, recovered through the
 * global source hash registry.
 * ---------------------------------------------------------------------- */
static void test_src08a_source_has_errors_no_errors(void) {
   module_context ctx = NULL;
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("SRC08a: setup context failed");
      return;
   }

   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      TestBit.fail("SRC08a: setup document failed");
      return;
   }

   const char *buffer = "name := test\n";
   Source.from_buffer(&doc->source, buffer, strlen(buffer), &err_code);
   mod_ctx_register_doc(ctx, doc, "foo.anvl", &err_code);

   TestBit.is_false(Source.has_errors(doc->source),
                    "SRC08a: registered source reports no errors initially");

   mod_ctx_dispose(ctx);
}
static void test_src08b_source_set_error_and_has_errors(void) {
   module_context ctx = NULL;
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("SRC08b: setup context failed");
      return;
   }

   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      TestBit.fail("SRC08b: setup document failed");
      return;
   }

   const char *buffer = "name := test\n";
   Source.from_buffer(&doc->source, buffer, strlen(buffer), &err_code);
   mod_ctx_register_doc(ctx, doc, "foo.anvl", &err_code);

   anvl_result res =
      Source.set_error(doc->source, ANVL_ERR_PARSER_UNEXPECTED_TOKEN, 2, 5, "foo.anvl", &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "SRC08b: set_error returns OK for registered source");
   TestBit.is_true(Source.has_errors(doc->source),
                   "SRC08b: registered source reports errors after set_error");
   TestBit.is_equal_int(1, (long long)List.size(ctx->errors),
                        "SRC08b: context error list contains one error");

   anvl_error err = anvl_error_get(ctx->errors, 0);
   TestBit.is_not_null(err, "SRC08b: error at index 0 is not null");
   TestBit.is_equal_int(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, (long long)err->code,
                        "SRC08b: error code matches");
   TestBit.is_equal_int(2, (long long)err->line, "SRC08b: error line matches");
   TestBit.is_equal_int(5, (long long)err->column, "SRC08b: error column matches");

   mod_ctx_dispose(ctx);
}
static void test_src08c_source_set_error_unregistered_fails(void) {
   anvl_source src = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   Source.create(&src, &err_code);
   Source.from_buffer(&src, "x", 1, &err_code);

   anvl_result res = Source.set_error(src, ANVL_ERR_PARSER_UNEXPECTED_CHAR, 1, 1, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC08c: set_error returns ERR for unregistered source");

   Source.dispose(src);
}
static void test_src08d_source_has_errors_null_source(void) {
   TestBit.is_false(Source.has_errors(NULL), "SRC08d: NULL source reports no errors");
}
static void test_src08e_source_set_error_null_source_fails(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = Source.set_error(NULL, ANVL_ERR_PARSER_UNEXPECTED_CHAR, 1, 1, NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "SRC08e: set_error returns ERR for NULL source");
}

/* ---------------------------------------------------------------------- *
 * DOC00 — doc_initialize success path
 * Commentary: verifies document allocation.
 * ---------------------------------------------------------------------- */
static void test_doc00_doc_initialize_success(void) {
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = doc_initialize(&doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "DOC00: doc_initialize returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "DOC00: err_code remains NONE");
   TestBit.is_not_null(doc, "DOC00: document object is allocated");
   TestBit.is_not_null(doc->source, "DOC00: source is allocated");
   TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)doc->source->dialect,
                        "DOC00: default source dialect is AML after doc_initialize");
   TestBit.is_null(doc->context, "DOC00: context starts NULL");

   doc_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * DOC01 — doc_initialize null out_doc
 * ---------------------------------------------------------------------- */
static void test_doc01_doc_initialize_null_out_doc(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_initialize(NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "DOC01: doc_initialize returns ERR for null out_doc");
   TestBit.is_equal_int(ANVL_ERR_INVALID_ARGUMENT, err_code, "DOC01: err_code is INVALID_ARGUMENT");
}
/* ---------------------------------------------------------------------- *
 * DOC02 — doc_dispose null is no-op
 * ---------------------------------------------------------------------- */
static void test_doc02_doc_dispose_null(void) {
   doc_dispose(NULL);
   TestBit.is_true(true, "DOC02: doc_dispose(NULL) does not crash");
}

/* ---------------------------------------------------------------------- *
 * DOC03 — doc_load_source from buffer
 * ---------------------------------------------------------------------- */
static void test_doc03_doc_load_source_buffer(void) {
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   const char *buffer = "#!amp\n\nmsg_id := a3f9\n";
   usize len = strlen(buffer);

   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      TestBit.fail("DOC03: setup document failed");
      return;
   }

   anvl_result res = doc_load_source(doc, ANVL_SOURCE_FROM_BUFFER, buffer, len, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "DOC03: doc_load_source from buffer returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "DOC03: err_code remains NONE");
   TestBit.is_not_null(doc->source, "DOC03: source is attached to document");

   doc_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * DOC04 — doc_load_source from file
 * Commentary: document source is created from filepath and attached.
 * ---------------------------------------------------------------------- */
static void test_doc04_doc_load_source_file(void) {
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      TestBit.fail("DOC04: setup document failed");
      return;
   }

   anvl_result res = doc_load_source(doc, ANVL_SOURCE_FROM_FILE,
                                     "../../test/fixtures/f01_bare_literal.anvl", 0, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "DOC04: doc_load_source from file returns OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "DOC04: err_code remains NONE");
   TestBit.is_not_null(doc->source, "DOC04: source is attached to document");
   TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)Source.dialect(doc->source),
                        "DOC04: source dialect is AML for .anvl fixture");

   doc_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * DOC05 — doc_load_source null document
 * ---------------------------------------------------------------------- */
static void test_doc05_doc_load_source_null_doc(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = doc_load_source(NULL, ANVL_SOURCE_FROM_FILE, "foo.anvl", 0, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "DOC05: doc_load_source returns ERR for null doc");
   TestBit.is_equal_int(ANVL_ERR_INVALID_ARGUMENT, err_code, "DOC05: err_code is INVALID_ARGUMENT");
}
/* ---------------------------------------------------------------------- *
 * DOC06 — doc_unload_source detaches source
 * ---------------------------------------------------------------------- */
static void test_doc06_doc_unload_source(void) {
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      TestBit.fail("DOC06: setup document failed");
      return;
   }

   if (ANVL_RES_OK != doc_load_source(doc, ANVL_SOURCE_FROM_FILE,
                                      "../../test/fixtures/f01_bare_literal.anvl", 0, &err_code)) {
      doc_dispose(doc);
      TestBit.fail("DOC06: setup source failed");
      return;
   }

   // unloading source returns the `source` object to a pristine state (creates a new source
   // object).
   doc_unload_source(doc);
   TestBit.is_not_null(doc->source, "DOC06: source is cleared after doc_unload_source");
   TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)Source.dialect(doc->source),
                        "DOC06: source dialect remains AML after doc_unload_source");
   TestBit.is_equal_int(0, (long long)doc->source->length,
                        "DOC06: source length is 0 after doc_unload_source");
   TestBit.is_equal_int(0, (long long)doc->source->stride,
                        "DOC06: source stride is 0 after doc_unload_source");
   TestBit.is_null(doc->source->buffer.bucket,
                   "DOC06: source buffer is NULL after doc_unload_source");

   // call doc_dispose to clean up the document and its source
   doc_dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * DOC09 — mod_ctx_register_doc creates identity
 * ---------------------------------------------------------------------- */
static void test_doc09_register_doc_identity(void) {
   module_context ctx = NULL;
   module_document doc = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("DOC09: setup context failed");
      return;
   }

   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      TestBit.fail("DOC09: setup document failed");
      return;
   }

   const char *buffer = "name := test\n";
   doc_load_source(doc, ANVL_SOURCE_FROM_BUFFER, buffer, strlen(buffer), &err_code);

   const char *filepath = "foo.anvl";
   anvl_result res = mod_ctx_register_doc(ctx, doc, filepath, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "DOC09: register_doc returns OK");
   if (res != ANVL_RES_OK) {
      doc_dispose(doc);
      mod_ctx_dispose(ctx);
      return;
   }

   TestBit.is_not_null(doc->context, "DOC09: document context back-pointer is set");
   TestBit.is_not_null(doc->filepath, "DOC09: document filepath is set");
   TestBit.is_equal_int(1, (long long)List.size(ctx->docs),
                        "DOC09: context docs list contains one document");
   TestBit.is_not_null(Registry.find(Source.hash(doc->source)),
                       "DOC09: document is registered by source hash");

   mod_ctx_dispose(ctx);
}

/* ---------------------------------------------------------------------- *
 * DOC10 — mod_ctx_register_doc rejects duplicate source hash
 * ---------------------------------------------------------------------- */
static void test_doc10_register_doc_duplicate_hash(void) {
   module_context ctx = NULL;
   module_document doc1 = NULL;
   module_document doc2 = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      TestBit.fail("DOC10: setup context failed");
      return;
   }

   if (ANVL_RES_OK != doc_initialize(&doc1, &err_code) || !doc1) {
      mod_ctx_dispose(ctx);
      TestBit.fail("DOC10: setup first document failed");
      return;
   }

   if (ANVL_RES_OK != doc_initialize(&doc2, &err_code) || !doc2) {
      doc_dispose(doc1);
      mod_ctx_dispose(ctx);
      TestBit.fail("DOC10: setup second document failed");
      return;
   }

   const char *buffer = "name := test\n";
   Source.from_buffer(&doc1->source, buffer, strlen(buffer), &err_code);
   Source.from_buffer(&doc2->source, buffer, strlen(buffer), &err_code);

   anvl_result res = mod_ctx_register_doc(ctx, doc1, "a.anvl", &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "DOC10: first register_doc returns OK");

   res = mod_ctx_register_doc(ctx, doc2, "b.anvl", &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "DOC10: duplicate hash register_doc fails");

   doc_dispose(doc2);
   mod_ctx_dispose(ctx);
}

int main(void) {
   TestBit.run_ex("SRC00_source_create_empty", NULL, test_src00_source_create_empty, td);
   TestBit.run_ex("SRC01_source_create_null_outputs", NULL, test_src01_source_create_null_outputs,
                  td);
   TestBit.run_ex("SRC02_source_dispose_null", NULL, test_src02_source_dispose_null, td);
   TestBit.run_ex("SRC03_source_from_buffer", NULL, test_src03_source_from_buffer, td);
   TestBit.run_ex("SRC04a_source_from_buffer_null_buffer", NULL,
                  test_src04a_source_from_buffer_null_buffer, td);
   TestBit.run_ex("SRC04b_source_from_buffer_zero_length", NULL,
                  test_src04b_source_from_buffer_zero_length, td);
   TestBit.run_ex("SRC04c_source_from_buffer_null_out_src", NULL,
                  test_src04c_source_from_buffer_null_out_src, td);

   TestBit.run_ex("SRC05a_source_from_file", NULL, test_src05a_source_from_file, td);
   TestBit.run_ex("SRC05b_source_from_file_aml", NULL, test_src05b_source_from_file_aml, td);
   TestBit.run_ex("SRC05c_source_from_file_amp", NULL, test_src05c_source_from_file_amp, td);
   TestBit.run_ex("SRC06a_source_from_file_null_path", NULL, test_src06a_source_from_file_null_path,
                  td);
   TestBit.run_ex("SRC06b_source_from_file_nonexistent", NULL,
                  test_src06b_source_from_file_nonexistent, td);
   TestBit.run_ex("SRC06c_source_from_file_null_out_src", NULL,
                  test_src06c_source_from_file_null_out_src, td);

   TestBit.run_ex("SRC07a_source_hash_empty", NULL, test_src07a_source_hash_empty, td);
   TestBit.run_ex("SRC07b_source_hash_stable", NULL, test_src07b_source_hash_stable, td);
   TestBit.run_ex("SRC07c_source_hash_different_content", NULL,
                  test_src07c_source_hash_different_content, td);
   TestBit.run_ex("SRC07d_source_hash_from_file", NULL, test_src07d_source_hash_from_file, td);

   TestBit.run_ex("SRC08a_source_has_errors_no_errors", NULL,
                  test_src08a_source_has_errors_no_errors, td);
   TestBit.run_ex("SRC08b_source_set_error_and_has_errors", NULL,
                  test_src08b_source_set_error_and_has_errors, td);
   TestBit.run_ex("SRC08c_source_set_error_unregistered_fails", NULL,
                  test_src08c_source_set_error_unregistered_fails, td);
   TestBit.run_ex("SRC08d_source_has_errors_null_source", NULL,
                  test_src08d_source_has_errors_null_source, td);
   TestBit.run_ex("SRC08e_source_set_error_null_source_fails", NULL,
                  test_src08e_source_set_error_null_source_fails, td);

   TestBit.run_ex("DOC00_doc_initialize_success", NULL, test_doc00_doc_initialize_success, td);
   TestBit.run_ex("DOC01_doc_initialize_null_out_doc", NULL, test_doc01_doc_initialize_null_out_doc,
                  td);
   TestBit.run_ex("DOC02_doc_dispose_null", NULL, test_doc02_doc_dispose_null, td);

   TestBit.run_ex("DOC03_doc_load_source_buffer", NULL, test_doc03_doc_load_source_buffer, td);
   TestBit.run_ex("DOC04_doc_load_source_file", NULL, test_doc04_doc_load_source_file, td);
   TestBit.run_ex("DOC05_doc_load_source_null_doc", NULL, test_doc05_doc_load_source_null_doc, td);
   TestBit.run_ex("DOC06_doc_unload_source", NULL, test_doc06_doc_unload_source, td);

   TestBit.run_ex("DOC09_register_doc_identity", NULL, test_doc09_register_doc_identity, td);
   TestBit.run_ex("DOC10_register_doc_duplicate_hash", NULL, test_doc10_register_doc_duplicate_hash,
                  td);

   return TestBit.report();
}
