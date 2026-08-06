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
//
#include <sigma/memory.h>
#include <sigma/strings.h>

static ssize_t doc_size = sizeof(struct anvl_mod_doc_t);

/* ----------------------------------------------------------------------- *
 * module_document management                                              *
 * ----------------------------------------------------------------------- */
anvl_result doc_initialize(const char *filepath, module_document *out_doc,
                           anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   module_document doc = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!out_doc || !out_err_code) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   *out_doc = NULL;

   if (!filepath) {
      err_code = ANVL_ERR_IO_INVALID_PATH;
      goto error;
   }

   doc = Allocator.alloc(doc_size);
   if (!doc) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(doc, 0, doc_size);

   // finish initializing document struct

   *out_doc = doc;
   return ANVL_RES_OK;

error:
   Allocator.dispose(doc);
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
   // dispose source and filepath
   if (doc->source) {
      Source.dispose(doc->source);
      doc->source = NULL;
   }

   Allocator.dispose(doc);
}