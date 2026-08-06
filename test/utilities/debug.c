/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * debug.c - Manual lifecycle helpers for atomized test troubleshooting
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/utilities/debug.c
 */

#include "debug.h"
#include "anvil.h"
#include "std.h"
#include "types.h"
#include "internal/source.h"
// -----------------------------------------------------------------
#include <sigma/strings.h>
#include <string.h>

doc_identity make_stub_doc_identity(const char *namespace_key, const char *filepath) {
   ssize_t id_size = sizeof(struct anvl_doc_identity_t);
   doc_identity identity = Allocator.alloc(id_size);
   if (!identity)
      return NULL;

   memset(identity, 0, id_size);
   identity->namespace = String.copy((char *)namespace_key);
   if (!identity->namespace) {
      Allocator.dispose(identity);
      return NULL;
   }
   identity->filepath = String.copy((char *)filepath);
   if (!identity->filepath) {
      String.dispose(identity->namespace);
      Allocator.dispose(identity);
      return NULL;
   }

   return identity;
}
module_document make_stub_doc(anvl_source source, struct anvl_mod_doc_i *op) {
   static ssize_t doc_size = sizeof(struct anvl_mod_doc_t);
   module_document doc = Allocator.alloc(doc_size);
   if (!doc)
      return NULL;

   memset(doc, 0, doc_size);
   doc->source = source;
   doc->op = op;

   return doc;
}
void dispose_doc_manual(module_document doc) {
   if (!doc)
      return;

   if (doc->source) {
      Source.dispose(doc->source);
      doc->source = NULL;
   }

   Allocator.dispose(doc);
}
void clear_docs_manual(list docs) {
   // iterate through the list and dispose of each module_document
   for (usize i = 0; i < List.size(docs); i++) {
      module_document doc = NULL;
      List.get(docs, i, (object *)&doc);
      if (doc) {
         dispose_doc_manual(doc);
      }
   }
   List.dispose(docs);
}
anvl_error make_stub_err(anvl_err_code code) {
   anvl_error err = Allocator.alloc(sizeof(anvl_err_state_t));
   if (!err)
      return NULL;

   memset(err, 0, sizeof(anvl_err_state_t));
   err->code = code;
   return err;
}
void dispose_err_manual(anvl_error err) {
   if (!err)
      return;

   Allocator.dispose(err);
}
/*
 * Clear a list of errors, disposing each error and then the list itself.
 */
void clear_errs_manual(list errors) {
   for (usize i = 0; i < List.size(errors); i++) {
      anvl_error err = NULL;
      List.get(errors, i, (object *)&err);
      if (err) {
         Allocator.dispose(err);
      }
   }
   List.dispose(errors);
}
/*
 * Conservative cleanup for contexts created in tests that do not
 * exercise mod_ctx_dispose() directly.
 */
void dispose_ctx_manual(module_context ctx) {
   if (!ctx)
      return;

   if (ctx->docs) {
      clear_docs_manual(ctx->docs);
      ctx->docs = NULL;
   }
   if (ctx->errors) {
      clear_errs_manual(ctx->errors);
      ctx->errors = NULL;
   }

   Allocator.dispose(ctx);
}
/*
 * Conservative cleanup for modules when mod_dispose() is not under test.
 */
void dispose_mod_manual(AnvlMod mod) {
   if (!mod)
      return;

   dispose_ctx_manual(mod->context);
   mod->context = NULL;
   mod->root = NULL;
   Allocator.dispose(mod);
}

/* ********************************************************************** *
 * Debug interface for manual lifecycle helpers.  These functions are     *
 * used in unit tests to create and dispose of module documents, errors,  *
 * and contexts without invoking the APIs under test.  This allows for    *
 * explicit teardown and isolation of failures during testing.            *
 * ********************************************************************** */
const anvl_debug_i Debug = {
   .stub_doc = make_stub_doc,
   .dispose_doc = dispose_doc_manual,
   .clear_docs = clear_docs_manual,
   .stub_err = make_stub_err,
   .dispose_err = dispose_err_manual,
   .clear_errs = clear_errs_manual,
   .dispose_mod = dispose_mod_manual,
   .dispose_ctx = dispose_ctx_manual,
};