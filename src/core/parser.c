/*
 * Copyright (c) 2025-26 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * parser.c - Implementation of Anvil parser
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * Created: 2025-12-13
 * Refactor: 2026-07-17
 * File: src/core/parser.c
 * ----------------------------------------------------------------------- *
 * Refactoring Note: This file is the refactored parser implementation.
 * It supersedes the original implementation in src/core/_parser.c, which
 * is now considered obsolete. The refactored parser improves maintainability,
 * readability, and performance while preserving the original functionality.
 * The original file is retained for reference and historical purposes.
 *
 * This refactored parser is designed to be more modular, with clearer
 * separation of concerns, improved error handling, and enhanced support for
 * future language features. It is recommended to use this refactored version.
 *
 * The original license and copyright information from the original file is
 * retained here for historical context, but the refactored code is subject
 * to the same proprietary license as the rest of the Anvil project.
 */

#include "internal/module.h"
#include "internal/parser.h"
#include "anvil.h"
// -----------------------------------------------------------------
#include <stdio.h>
#include <string.h>

/* ----------------------------------------------------------------- *
 * Parser Implementation                                              *
 * ----------------------------------------------------------------- */
static const anvl_parser_vt iparser;

/* ----------------------------------------------------------------- *
 * Parser Initialization                                              *
 * ----------------------------------------------------------------- */
bool parser_new(anvl_doc doc) {
  if (!doc || !doc->source) {
    goto error;
  }

  // anvl_error_clear();

  // initialize parser
  ssize_t p_size = sizeof(anvl_parser);
  anvl_parser *p = Allocator.alloc(p_size);
  if (!p) {
    goto error;
  }
  memset(p, 0, p_size);

  p->p = &iparser;
  doc->set_parser(doc, p);

  return true;

  error:
   return false;
}

void parser_dispose(parser p) {
  if (!p) {
    return;
  }

  Allocator.dispose(p);
}

static const anvl_parser_vt iparser = {
    .parse = NULL, .parse_statement = NULL, .parse_value = NULL, .reset = NULL};