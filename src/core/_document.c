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

#include "internal/files.h"
#include "internal/module.h"
#include "internal/source.h"

#include "std.h"

static size_t doc_size = sizeof(struct anvl_doc_t);

/* ----------------------------------------------------------------- *
 * Document Interface - for managing Anvil documents                 *
 * ----------------------------------------------------------------- */
bool doc_create_source(const char *filepath, anvl_doc *out_doc);
static void doc_set_parser(anvl_doc d, parser p);

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
   if (!doc_create_source(filepath, &doc)) {
      goto error;
   }
   // assign the interface vtable for document operations
   doc->interface = &IDocument;

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
void doc_dispose(anvl_doc self) {}
bool doc_create_source(const char *filepath, anvl_doc *out_doc) {
   bool success = !filepath || !out_doc || *out_doc == NULL;
   if (!success) {
      goto error;
   }
   // create the source for the document
   // utils load function reads the file into a buffer and returns it
   const char *data = NULL;
   usize len = 0;
   success = Files.load(filepath, &data, &len);
   if (!success) {
      anvl_error_set(NULL, ANVL_ERR_FILE_READ, 0, 0, filepath);
      goto error;
   }
   success = Source.create(out_doc, data, String.length((string)data));
   if (!success)
      goto error;

   return success;

error:
   return success;
}
void doc_dispose_source(anvl_doc self) {
   if (!self || !self->source) {
      return;
   }
   anvl_src src = self->source;
   if (src) {
      Source.dispose(src);
   }
}
bool doc_has_errors(anvl_doc self) {
   // TODO: needs module reference to check for specific document source errors
   if (!self || !self->context || !self->context->errors)
      return false;
   return List.size(self->context->errors) > 0;
}
void doc_set_error(anvl_doc self, anvl_error_code code, usize line, usize col,
                          const char *file) {
   if (!self || !self->context || !self->context->errors)
      return;
   anvl_error_set(self->context->errors, code, line, col,
                  file ? file : File.filename(self->filepath));
}

static void doc_set_parser(anvl_doc self, parser parser) {
   if (!self || !p)
      return;
   // set the parser for the document
   mod_ctx_set_parser(self->context, parser);
}

static const anvl_idoc IDocument = {
   .set_parser = doc_set_parser,
};