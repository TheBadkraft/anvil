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

#include "std.h"
#include "types.h"
#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
//
#include <sigma/memory.h>
#include <sigma/strings.h>

static ssize_t doc_size = sizeof(struct anvl_mod_doc_t);

/* ----------------------------------------------------------------------- *
 * module_document management
 * ----------------------------------------------------------------------- */
anvl_result doc_initialize(module_document *out_doc, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   module_document doc = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!out_doc) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   *out_doc = NULL;

   doc = Allocator.alloc(doc_size);
   if (!doc) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(doc, 0, doc_size);

   // finish initializing document struct
   Source.create(&doc->source, &err_code);
   if (err_code != ANVL_ERR_NONE) {
      goto error;
   }

   *out_doc = doc;
   return ANVL_RES_OK;

error:
   doc_dispose(doc);
   doc = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (out_doc) {
      *out_doc = doc;
   }
   return ANVL_RES_ERR;
}
void doc_dispose(module_document doc) {
   if (!doc) {
      return;
   }

   if (doc->source) {
      Registry.remove(Source.hash(doc->source));
      Source.dispose(doc->source);
      doc->source = NULL;
   }

   if (doc->filepath) {
      String.dispose(doc->filepath);
      doc->filepath = NULL;
   }

   doc->context = NULL;
   Allocator.dispose(doc);
}

/* ----------------------------------------------------------------------- *
 * document loading and unloading
 * ----------------------------------------------------------------------- */
anvl_result doc_load_source(module_document doc, anvl_source_origin origin, const char *source,
                            size_t len, anvl_err_code *out_err_code) {
   // set up locals
   anvl_err_code err_code = ANVL_ERR_NONE;
   // ok if NULL, just won't report errors to caller
   if (out_err_code) {
      *out_err_code = err_code;
   }

   // validate parameters
   if (!doc || !source) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   // does document have a source object; if not, create one
   if (!doc->source || ANVL_RES_OK != Source.create(&doc->source, &err_code)) {
      goto error;
   }

   // switch on origin to load source from file or buffer
   switch (origin) {
   case ANVL_SOURCE_FROM_FILE:
      if (ANVL_RES_OK != Source.from_file(&doc->source, source, &err_code)) {
         goto error;
      }
      break;
   case ANVL_SOURCE_FROM_BUFFER:
      if (ANVL_RES_OK != Source.from_buffer(&doc->source, source, len, &err_code)) {
         goto error;
      }
      break;
   default:
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }

   return ANVL_RES_ERR;
}
void doc_unload_source(module_document doc) {
   // unload the source return the source object to a pristine state (creates a new source object).
   if (!doc || !doc->source) {
      return;
   }
   Source.dispose(doc->source);
   anvl_err_code err_code = ANVL_ERR_NONE;
   Source.create(&doc->source, &err_code);

   if (err_code != ANVL_ERR_NONE) {
      // if we can't create a new source object, set the document's source to NULL
      doc->source = NULL;
      // set doc error code to indicate failure to create new source object
   }
}

/* ----------------------------------------------------------------------- *
 * document error handling
 * ----------------------------------------------------------------------- */
bool doc_has_errors(module_document doc) {
   if (!doc || !doc->context) {
      return false;
   }

   return anvl_error_is_set(doc->context->errors);
}
bool doc_set_error(module_document doc, anvl_err_code code, usize line, usize col,
                   const char *file) {
   if (!doc || !doc->context) {
      return false;
   }

   return anvl_error_set(doc->context->errors, code, line, col, file, NULL) == ANVL_RES_OK;
}
