/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 *                                                                        *
 * This software is proprietary and confidential. Unauthorized copying,   *
 * distribution, modification, or use of this software, via any medium,   *
 * is strictly prohibited without express written permission from the     *
 * copyright holder.                                                      *
 *                                                                        *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * anvil_flat.c - Public ABI implementation: flat exported functions      *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * anvil_load runs the whole internal pipeline (header scan, import      *
 * loading, arena creation, body parse of every document in the import    *
 * graph, resolution) and records which *phase* first failed, if any, on  *
 * the returned handle — not a translated internal anvl_err_code, so      *
 * internal error-code churn never becomes a public ABI change. See      *
 * notes/public-api.md.                                                  *
 * ********************************************************************** */

#include "anvil.h"
#include "anvil_flat.h"
#include "internal/module.h"
// ----------------
#include <sigma/allocator.h>
#include <sigma/list.h>
#include <stddef.h>

struct anvil_document_t {
   module_context ctx;
   module_document root; // NULL once disposed, or if never successfully registered
   anvil_err_code error; // ANVIL_OK if the whole pipeline succeeded
};

anvil_document anvil_load(const char *filepath) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   module_context ctx = NULL;
   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      return NULL; // truly foundational failure - nothing to hand back
   }

   module_document doc = NULL;
   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      return NULL;
   }

   struct anvil_document_t *handle = Allocator.alloc(sizeof(struct anvil_document_t));
   if (!handle) {
      doc_dispose(doc);
      mod_ctx_dispose(ctx);
      return NULL;
   }
   handle->ctx = ctx;
   handle->root = doc;
   handle->error = ANVIL_OK;

   if (ANVL_RES_OK != doc_load_source(doc, ANVL_SOURCE_FROM_FILE, filepath, 0, &err_code)) {
      // Never registered with ctx - mod_ctx_dispose won't reach it, so dispose it directly
      // here to avoid a leak, and clear the handle's reference to it.
      doc_dispose(doc);
      handle->root = NULL;
      handle->error = ANVIL_ERR_IO;
      return handle;
   }

   if (ANVL_RES_OK != mod_ctx_register_doc(ctx, doc, filepath, &err_code)) {
      doc_dispose(doc);
      handle->root = NULL;
      handle->error = ANVIL_ERR_IO;
      return handle;
   }

   if (ANVL_RES_OK != doc_scan_header(doc, &err_code)) {
      handle->error = ANVIL_ERR_HEADER;
      return handle;
   }

   usize size_hint = 0;
   if (ANVL_RES_OK != mod_load_imports(ctx, doc, &size_hint, &err_code)) {
      // Also covers a nested import's own header-scan failure (mod_load_imports scans each
      // child's header as it recurses) - the root document's own header was fine, so
      // ANVIL_ERR_IMPORT ("something went wrong resolving the import graph") is the more
      // accurate category from this caller's perspective, not ANVIL_ERR_HEADER.
      handle->error = ANVIL_ERR_IMPORT;
      return handle;
   }

   usize capacity = mod_ctx_arena_size_hint(size_hint);
   if (ANVL_RES_OK != mod_ctx_create_arena(ctx, capacity, &err_code)) {
      handle->error = ANVIL_ERR_MEMORY;
      return handle;
   }

   usize doc_count = List.size(ctx->docs);
   for (usize i = 0; i < doc_count; i++) {
      module_document d = NULL;
      List.get(ctx->docs, i, (object *)&d);
      if (!d) {
         continue;
      }
      if (ANVL_RES_OK != doc_parse_body(d, &err_code)) {
         handle->error = ANVIL_ERR_SYNTAX;
         return handle;
      }
   }

   if (ANVL_RES_OK != mod_resolve_context(ctx, &err_code)) {
      handle->error = ANVIL_ERR_RESOLVE;
      return handle;
   }

   return handle;
}

void anvil_dispose(anvil_document doc) {
   if (!doc) {
      return;
   }
   mod_ctx_dispose(doc->ctx); // disposes every registered document too (root included, once registered)
   Allocator.dispose(doc);
}

bool anvil_has_errors(anvil_document doc) {
   if (!doc) {
      return false;
   }
   return doc->error != ANVIL_OK;
}

anvil_err_code anvil_get_error(anvil_document doc) {
   if (!doc) {
      return ANVIL_ERR_INVALID_ARGUMENT;
   }
   return doc->error;
}

const char *anvil_get_version(void) {
   return Anvl.get_version();
}
