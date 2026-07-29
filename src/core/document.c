/* *********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium,    *
 * is strictly prohibited without express written permission from the      *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * document.c - Implementation of Anvil document                           *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-13                                                     *
 * File: src/core/document.c                                               *
 * *********************************************************************** */

// NOTE: use kernal pattern error/exit handling for all functions

#include "internal/module.h"
#include "internal/source.h"

#include "std.h"

static size_t doc_size = sizeof(struct anvl_doc_t);

/* ----------------------------------------------------------------- *
 * Document Interface - for managing Anvil documents                 *
 * ----------------------------------------------------------------- */
static void doc_set_parser(anvl_doc d, parser p);
static bool doc_create_source(const char *filepath, anvl_doc *out_doc);

bool doc_initialize(const char *filepath, anvl_doc *out_doc) {
  if (!filepath || !out_doc)
    goto error;
  ;
  *out_doc = NULL;
  anvl_doc doc = Allocator.alloc(doc_size);
  if (!doc) {
    goto error;
  }
  memset(doc, 0, doc_size);
  doc->filepath = String.copy((string)filepath);
  // create the Source for the root anvl_doc
  if (!doc->filepath) {
    goto error;
  }
  if (!Source.create(filepath, &doc)) {
    goto error;
  }
  // assign the function delegates for document operations
  doc->set_parser = &doc_set_parser;
  (*out_doc) = doc;
  return true;

error:
  if (out_doc) {
    *out_doc = NULL;
  }
  if (doc) {
    Allocator.dispose(doc);
  }
  return false;
}
void doc_dispose(anvl_doc doc) {}

static bool doc_create_source(const char *filepath, anvl_doc *out_doc) {
  if (!filepath || !out_doc || *out_doc == NULL)
    return false;
  // create the source for the document
  // utils load function reads the file into a buffer and returns it
  const char *data = NULL;
  usize len = 0;
  if (!load_source(filepath, &data, &len)) {
    anvl_error_set(NULL, ANVL_ERR_FILE_READ, 0, 0, filepath);
    return false;
  }
  anvl_src src = source_create(*out_doc, data, String.length((string)data));
  free((void *)data);
  if (!src)
    return false;

  // doc is already allocated by the caller, just set the source
  (*out_doc)->source = src;

  return true;
}
static void doc_dispose_source(anvl_src src) {
  if (src) {
    source_dispose(src);
  }
}
static bool doc_has_errors(anvl_doc doc) {
  // TODO: needs module reference to check for specific document source errors
  if (!doc || !doc->context || !doc->context->errors)
    return false;
  return List.size(doc->context->errors) > 0;
}
static void doc_set_error(anvl_doc doc, anvl_error_code code, usize line,
                          usize col, const char *file) {
  if (!doc || !doc->context || !doc->context->errors)
    return;
  anvl_error_set(doc->context->errors, code, line, col, file ? file : File.filename(doc->filepath));
}
static void doc_set_parser(anvl_doc d, parser p) {
  if (!d || !p)
    return;
  // set the parser for the document
  mod_ctx_set_parser(d->context, p);
}
