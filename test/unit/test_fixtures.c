/* *********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * test_fixtures.c - Core Fixtures API and parsing tests                   *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * File: test/unit/test_fixtures.c                                         *
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
 * F00a — test comments: single- & multi-line                         *
 * ----------------------------------------------------------------- */
static void test_f00a_comments(void) {
  AnvlMod mod = Anvl.load(fixture_path("f00a_comments.anvl"));

  // root document should have a source object
  anvl_doc doc = NULL;
  root_get_doc(mod->root, 0, &doc);
  TestBit.is_not_null(doc, "F00a: root doc is successfully retrieved");
  TestBit.is_not_null(doc->source, "F00a: root doc has source object");

  // check for errors in the document
  TestBit.is_false(Anvl.has_errors(mod), "F00a: no errors on mod");
  TestBit.is_false(Anvl.has_errors(doc), "F00a: no errors on doc");

  // clean up
  root_dispose_doc(doc);
}

/* ----------------------------------------------------------------- *
 * Test runner                                                        *
 * ----------------------------------------------------------------- */
int main(void) {
  TestBit.run_ex("F00a_comments", NULL, test_f00a_comments, td);

  return TestBit.report();
}