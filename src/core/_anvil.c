/*
 * Copyright (c) 2025,2026 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * anvil.c - Implementation of Anvil public API
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * Created: 2025-12-13
 * File: src/core/anvil.c
 * ----------------------------------------------------------------------- *
 */

// NOTE: use kernal pattern happy path/error/exit handling for all functions

#include "anvil.h"
#include "types.h"
// #include "context.h"
#include "internal/module.h"
#include "internal/parser.h"
#include "internal/source.h"
#include "utils.h"
// -----------------------------------------------------------------
#include <sigma/list.h>
#include <sigma/memory.h>
#include <sigma/strings.h>
#include <sigma/types.h>
// -----------------------------------------------------------------
#include <stdio.h>
#include <string.h>

void mod_docs_clear(list docs) {
  // iterate through the list and dispose of each anvl_doc
  for (usize i = 0; i < List.size(docs); i++) {
    anvl_doc doc = NULL;
    List.get(docs, i, (object *)&doc);
    if (doc) {
      root_dispose_doc(doc);
    }
  }
  List.dispose(docs);
}
void mod_errs_clear(list errors) {
  for (usize i = 0; i < List.size(errors); i++) {
    anvl_err err = NULL;
    List.get(errors, i, (object *)&err);
    if (err) {
      Allocator.dispose(err);
    }
  }
  List.dispose(errors);
}
bool root_get_doc(anvl_ctx ctx, usize index, anvl_doc *out_doc) {
  // we do not expect out_doc to be initialized, but we will set it to NULL
  if (!ctx || !ctx->docs) {
    *out_doc = NULL;
    return false;
  }

  *out_doc = NULL; // initialize to NULL in case of failure
  if (index >= List.size(ctx->docs))
    return false;

  List.get(ctx->docs, index, (object *)out_doc);
  return true;
}
void root_dispose_doc(anvl_doc doc) {
  if (!doc) {
    return;
  }
  // dispose source and filepath
  if (doc->source) {
    Source.dispose(doc->source);
  }
  if (doc->filepath) {
    String.dispose(doc->filepath);
    doc->filepath = NULL;
  }
  Allocator.dispose(doc);
}

/* ----------------------------------------------------------------- *
 * AnvlMod interface implementations                                  *
 * ----------------------------------------------------------------- */
static AnvlMod anvl_mod_load(const char *filepath) {
  // this is the initial entry point for loading an AnvlMod from a file path
  AnvlMod mod = NULL;
  anvl_doc root = NULL;

  // initialize the module context
  if (!mod_init(&mod))
    goto error;
  if (!doc_initialize(filepath, &root)) {
    goto error;
  }

  mod_ctx_add_doc(mod->context, root);
  return mod;

error:
  mod_dispose(&mod);
  mod = NULL;
  return mod;
}

// static anvl_mod_root_t anvil_read(const char *source, usize len) {
//    (void)source;
//    (void)len;
//    anvl_mod_root_t r = Allocator.alloc(sizeof(struct anvl_mod_root_t)); //
//    minimal if (r)
//       memset(r, 0, sizeof(struct anvl_mod_root_t));
//    return r;
// }

// static void anvil_cleanup(void) {
//    // currently no global resources to clean up
//    // make sure no residual builder state lingers
//    anvl_ctx_builder_i *builder = Context.get_builder();
//    builder->dialect = ANVL_DIALECT_ASL;
//    if (builder->source) {
//       Source.dispose(builder->source);
//       builder->source = NULL;
//    }
// }

static void anvl_mod_dispose(AnvlMod mod) {
  if (!mod)
    return;
  if (mod->context) {
    mod_ctx_dispose(mod->context);
    mod->context = NULL;
  }

  Allocator.dispose(mod);
}
static const char *anvl_get_root_path(AnvlMod mod) {
  if (!mod || !mod->root)
    goto error;

  anvl_doc doc = mod->root;
  if (!doc->filepath)
    goto error;

  return (const char *)doc->filepath;

error:
  return NULL;
}
static bool anvl_has_errors(AnvlMod mod) {
  bool has_errors = false;

  // guard the main object
  if (!mod)
    goto error;

  // extract and protext context
  anvl_ctx ctx = mod->context;
  if (!ctx)
    goto error;
  // short-circuit if errors not initialized
  if (!ctx->errors)
    return has_errors;

  // clean sequential happy path
  usize err_count = List.size(ctx->errors);
  has_errors = (err_count > 0);

  return has_errors;

error:
  return has_errors;
}
static void anvl_clear_errors(AnvlMod mod) {
  // guard the main object
  if (!mod)
    goto error;

  // extract and protext context
  anvl_ctx ctx = mod->context;
  if (!ctx)
    goto error;
  // short-circuit if errors not initialized
  if (!ctx->errors)
    return;

  usize err_count = List.size(ctx->errors);
  for (usize i = 0; i < err_count; i++) {
    anvl_err err = NULL;
    List.get(ctx->errors, i, (object *)&err);
    Allocator.dispose(err);
    err = NULL;
  }
  anvl_error_clear(ctx->errors);

error:
  return;
}

// static const anvl_error_state *anvil_error_get(void) {
//    return anvl_error_get();
// }

static const char *anvl_get_version(void) {
  static char version[32];
  snprintf(version, sizeof(version), "%d.%d.%d+%d-%s", ANVL_VERSION_MAJOR,
           ANVL_VERSION_MINOR, ANVL_VERSION_PATCH, ANVL_BUILD,
           ANVL_VERSION_TAG);
  return version;
}

const anvl_i Anvl = {
    .load = anvl_mod_load,
    //  .read = anvil_read,
    .dispose = anvl_mod_dispose,
    .get_root_path = anvl_get_root_path,
    //  .cleanup = anvl_cleanup,
    .has_errors = anvl_has_errors,
    //  .error_get = anvil_error_get,
    .error_clear = anvl_clear_errors,
    .get_version = anvl_get_version,
};