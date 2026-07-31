/* *********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * test_core.c - Core API tests: AnvlMod and AnvlDoc construction          *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * File: test/unit/test_core.c                                             *
 * *********************************************************************** */

#include "anvil.h"
#include "types.h"
#include "internal/module.h"
// --------------------------
#include "../utilities/helpers.h"
#include "testbit.h"
// --------------------------
#include <sigma/list.h>
#include <string.h>

void clear_docs(list docs);
void clear_errs(list errs);

static void td(void) {}

/* ----------------------------------------------------------------- *
 * CR01 — initialize module                                          *
 * ----------------------------------------------------------------- */
static void test_cr01_mod_ctx_init(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = mod_ctx_initialize(&ctx, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "CR01: mod_ctx_initialize returned ANVL_RES_OK");
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "CR01: mod_ctx_initialize returned ANVL_ERR_NONE");
   TestBit.is_not_null(ctx, "CR01: module_context is not NULL");

   // manual cleanup
   if (!ctx)
      return;

   if (ctx->docs) {
      clear_docs(ctx->docs);
      clear_errs(ctx->errors);
      ctx->docs = NULL;
      ctx->errors = NULL;
   }

   Allocator.dispose(ctx);
}
/* ----------------------------------------------------------------- *
 * CR02 — initialize root object                                      *
 * ----------------------------------------------------------------- */
// static void test_cr02_mod_init(void) {
//    AnvlMod mod = NULL;
//    // our convention lets us know that if the call result is OK, then the target
//    // object is not null
//    anvl_err_code err_code = ANVL_ERR_NONE;
//    anvl_result res = mod_initialize(&mod, &err_code);
//    TestBit.is_equal_int(ANVL_RES_OK, res, "CR02: mod_initialize returned ANVL_RES_OK");

//    mod_dispose(&mod);
// }
/* ----------------------------------------------------------------- *
 * CR03 — load; root doc has source                                   *
 * ----------------------------------------------------------------- */
// static void test_cr03_load_source(void) {
//   AnvlMod mod = Anvl.load(fixture_path("f01_bare_literal.anvl"));

//   // root document should have a source object
//   anvl_doc doc = NULL;
//   root_get_doc(mod->root, 0, &doc);
//   TestBit.is_not_null(doc, "CR03: root doc is successfully retrieved");
//   TestBit.is_not_null(doc->source, "CR03: root doc has source object");

//   // clean up
//   Anvl.dispose(mod);
// }
/* ----------------------------------------------------------------- *
 * CR04 — create ANVL parser                                          *
 * ----------------------------------------------------------------- */
// static void test_cr04_create_parser(void) {
//   anvl_doc doc = NULL;
//   if (!create_doc_with_parser(&doc, fixture_path("f01_bare_literal.anvl"))) {
//     // failed to create doc with parser
//     TestBit.fail("CR04: failed to create doc with parser");
//     return;
//   } else if (!doc->parser) {
//     TestBit.fail("CR04: doc->parser is NULL");
//     return;
//   }

//   // validate parser initialization
//   TestBit.is_not_null(doc->parser, "CR04: parser is successfully created");
//   TestBit.is_not_null(doc->parser->p,
//                       "CR04: parser->p is successfully created");

//   // clean up
//   root_dispose_doc(doc);
// }

/* ----------------------------------------------------------------- *
 * Test runner                                                        *
 * ----------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("CR01_mod_ctx_init", NULL, test_cr01_mod_ctx_init, td);
   // TestBit.run_ex("CR02_mod_init", NULL, test_cr02_mod_init, td);
   // TestBit.run_ex("CR02_mod_init", NULL, test_cr02_mod_init, td);
   // TestBit.run_ex("CR03_load_source", NULL, test_cr03_load_source, td);
   // TestBit.run_ex("CR04_create_parser", NULL, test_cr04_create_parser, td);

   return TestBit.report();
}

void clear_docs(list docs) {
   if (!docs)
      return;

   iterator it = List.create_iterator(docs);
   if (!it) {
      List.dispose(docs);
      return;
   }

   while (Iterator.next(it)) {
      object slot = Iterator.current(it);
      if (!slot)
         continue;

      module_document doc = *(module_document *)slot;
      if (doc)
         doc_dispose(doc);
   }

   Iterator.dispose(it);
   List.dispose(docs);
}
void clear_errs(list errs) {
   if (!errs)
      return;

   iterator it = List.create_iterator(errs);
   if (!it) {
      List.dispose(errs);
      return;
   }

   while (Iterator.next(it)) {
      object slot = Iterator.current(it);
      if (!slot)
         continue;

      anvl_error err = *(anvl_error *)slot;
      if (err)
         Allocator.dispose(err);
   }

   Iterator.dispose(it);
   List.dispose(errs);
}