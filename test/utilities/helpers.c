/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * helpers.c - Test utility functions
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/utilities/helpers.c
 */

#include "helpers.h"
#include "anvil.h"
// -----------------------------------------------------------------
#include <stdio.h>
#include <sigma/strings.h>

static char path_buffer[512];

const char *fixture_path(const char *name) {
  snprintf(path_buffer, sizeof(path_buffer), "../../test/fixtures/%s", name);
  return path_buffer;
}

/* ----------------------------------------------------------------- *
 * Create doc with parser boilerplate -                               *
 * This function will create just the `anvl_doc` object with a source *
 * buffer containing the provided source string and intialize the     *
 * the parser context for testing.                                    *
 * ----------------------------------------------------------------- */
bool create_doc_with_parser(anvl_doc *doc, const char *filepath) {
  // what do we need to just create a doc with a source buffer and parser
  // context? We need to allocate the doc, set the source buffer, and initialize
  // the parser context. allocate the doc
   ssize_t doc_size = sizeof(struct anvl_doc_t);
  *doc = Allocator.alloc(doc_size);
  if (!*doc) {
    return false;
  }

  memset(*doc, 0, doc_size);
  (*doc)->filepath = String.copy((string)filepath);
  // create the Source for the root anvl_doc
  if (!Source.create(filepath, doc)) {
    Allocator.dispose(*doc);

    return false;
  }

  // initialize the parser context
  if (!parser_new(*doc)) {
    Allocator.dispose(*doc);
    return false;
  }

  return *doc != NULL;
}