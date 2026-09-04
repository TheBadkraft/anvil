/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_anvil_native.c - Unit tests for the public ABI (Anvil Native)     *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_anvil_native.c                                    *
 * ---------------------------------------------------------------------- *
 * anvil_load / anvil_dispose / anvil_has_errors / anvil_get_error — the  *
 * minimal Anvil lifecycle slice. See notes/public-api.md.                *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_types.h"
#include "internal/source_registry.h"
#include "testbit.h"
// ----------------
#include "../utilities/helpers.h"

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

/* ---------------------------------------------------------------------- *
 * ANV01 — a clean, error-free document loads with no errors
 * ---------------------------------------------------------------------- */
static void test_anv01_load_clean_document(void) {
   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "ANV01: document handle returned");
   if (!doc) {
      return;
   }
   TestBit.is_false(anvil_has_errors(doc), "ANV01: no errors recorded");
   TestBit.is_equal_int(ANVIL_OK, anvil_get_error(doc), "ANV01: error category is ANVIL_OK");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV02 — a body syntax error reports ANVIL_ERR_SYNTAX
 * ---------------------------------------------------------------------- */
static void test_anv02_body_syntax_error(void) {
   anvil_document doc = anvil_load(fixture_path("body_err_unterminated_block.anvl"));
   TestBit.is_not_null(doc, "ANV02: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV02: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_SYNTAX, anvil_get_error(doc),
                        "ANV02: error category is ANVIL_ERR_SYNTAX");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV03 — a nonexistent file reports ANVIL_ERR_IO
 * ---------------------------------------------------------------------- */
static void test_anv03_file_not_found(void) {
   anvil_document doc = anvil_load(fixture_path("anvil_nonexistent_file_xyz.anvl"));
   TestBit.is_not_null(doc, "ANV03: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV03: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_IO, anvil_get_error(doc), "ANV03: error category is ANVIL_ERR_IO");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV04 — a missing import target reports ANVIL_ERR_IMPORT
 * ---------------------------------------------------------------------- */
static void test_anv04_import_error(void) {
   anvil_document doc = anvil_load(fixture_path("hdr_import_missing.anvl"));
   TestBit.is_not_null(doc, "ANV04: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV04: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_IMPORT, anvil_get_error(doc),
                        "ANV04: error category is ANVIL_ERR_IMPORT");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV05 — a resolver-phase duplicate identifier reports ANVIL_ERR_RESOLVE
 * ---------------------------------------------------------------------- */
static void test_anv05_resolve_error(void) {
   anvil_document doc = anvil_load(fixture_path("anvil_err_resolve_duplicate.anvl"));
   TestBit.is_not_null(doc, "ANV05: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV05: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_RESOLVE, anvil_get_error(doc),
                        "ANV05: error category is ANVIL_ERR_RESOLVE");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV06 — a duplicate shebang (header-scan failure) reports ANVIL_ERR_HEADER
 * ---------------------------------------------------------------------- */
static void test_anv06_header_error(void) {
   anvil_document doc = anvil_load(fixture_path("anvil_err_header_dup_shebang.anvl"));
   TestBit.is_not_null(doc, "ANV06: document handle still returned despite the error");
   if (!doc) {
      return;
   }
   TestBit.is_true(anvil_has_errors(doc), "ANV06: errors recorded");
   TestBit.is_equal_int(ANVIL_ERR_HEADER, anvil_get_error(doc),
                        "ANV06: error category is ANVIL_ERR_HEADER");

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * ANV07 — dispose(NULL) and the accessors are safe on a NULL handle
 * ---------------------------------------------------------------------- */
static void test_anv07_null_handle_safety(void) {
   anvil_dispose(NULL); // must not crash
   TestBit.is_false(anvil_has_errors(NULL), "ANV07: has_errors(NULL) is false");
   TestBit.is_equal_int(ANVIL_ERR_INVALID_ARGUMENT, anvil_get_error(NULL),
                        "ANV07: get_error(NULL) is ANVIL_ERR_INVALID_ARGUMENT");
}
/* ---------------------------------------------------------------------- *
 * ANV08 — get_version reports a non-empty string
 * ---------------------------------------------------------------------- */
static void test_anv08_get_version(void) {
   const char *version = anvil_get_version();
   TestBit.is_not_null(version, "ANV08: version string returned");
   if (version) {
      TestBit.is_true(version[0] != '\0', "ANV08: version string is non-empty");
   }
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("ANV01_load_clean_document", NULL, test_anv01_load_clean_document, th);
   TestBit.run_ex("ANV02_body_syntax_error", NULL, test_anv02_body_syntax_error, th);
   TestBit.run_ex("ANV03_file_not_found", NULL, test_anv03_file_not_found, th);
   TestBit.run_ex("ANV04_import_error", NULL, test_anv04_import_error, th);
   TestBit.run_ex("ANV05_resolve_error", NULL, test_anv05_resolve_error, th);
   TestBit.run_ex("ANV06_header_error", NULL, test_anv06_header_error, th);
   TestBit.run_ex("ANV07_null_handle_safety", NULL, test_anv07_null_handle_safety, th);
   TestBit.run_ex("ANV08_get_version", NULL, test_anv08_get_version, th);

   return TestBit.report();
}
