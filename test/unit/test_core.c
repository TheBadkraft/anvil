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
// -----------------------------------------------------------------
#include "../utilities/helpers.h"
#include "internal/root.h"
#include "testbit.h"
// -----------------------------------------------------------------
#include <string.h>

static void td(void) {}

/* ----------------------------------------------------------------- *
 * CR01 — initialize anvl_mod_context                                 *
 * ----------------------------------------------------------------- */
static void test_cr01_mod_ctx_init(void) {
  const char *filepath = fixture_path("f01_bare_literal.anvl");
  anvl_root root = NULL;
  // our convention lets us know that if the call is true, then the target
  // object is not null
  TestBit.is_true(root_init(&root), "CR01: anvl_root initialized");
  TestBit.is_not_null(root->docs,
                      "CR01: root doc list is successfully initialized");
  TestBit.is_false(Anvl.has_errors(root), "CR01: no errors on root");

  root_dispose(root);
}
/* ----------------------------------------------------------------- *
 * CR02 — initialize root object                                      *
 * ----------------------------------------------------------------- */
// static void test_cr02_mod_init(void) {
//   AnvlMod mod = NULL;
//   TestBit.is_true(mod_init(&mod), "CR02: AnvlMod initialized");

//   // root should be initialized
//   TestBit.is_not_null(mod->root, "CR02: AnvlMod root is initialized");

//   Anvl.dispose(mod);
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
//   TestBit.run_ex("CR02_mod_init", NULL, test_cr02_mod_init, td);
//   TestBit.run_ex("CR03_load_source", NULL, test_cr03_load_source, td);
//   TestBit.run_ex("CR04_create_parser", NULL, test_cr04_create_parser, td);

  return TestBit.report();
}