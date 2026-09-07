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
 * module.c - Module implementation for Anvil                              *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2025-07-22                                                     *
 * File: src/core/module.c                                                 *
 * *********************************************************************** */

#include "internal/constants.h"
#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "types.h"
#include "constants.h"
#include "std.h"
#include <sigma/math.h>
#include <sigma/strings.h>

static ssize_t mod_size = sizeof(struct anvl_mod_t);
static ssize_t ctx_size = sizeof(struct anvl_mod_ctx_t);

/* ----------------------------------------------------------------------- *
 * Context Specification
 * ----------------------------------------------------------------------- */
static const anvl_ctx_spec ANVL_CTX_DEFAULTS = {
   .docs_cap = ANVL_CTX_DEFAULT_DOC_CAP,
   .errs_cap = ANVL_CTX_DEFAULT_ERR_CAP,
   .map_cap = ANVL_CTX_DEFAULT_MAP_CAP, // must be power-of-two for map
};
// modify base if context initialized with custom spec
static anvl_ctx_spec ANVL_CTX_BASE = ANVL_CTX_DEFAULTS; // struct copy of defaults

/* ----------------------------------------------------------------------- *
 * Internal context specification resolution functions
 * ----------------------------------------------------------------------- */
// Resolves context specification, returning the default if NULL is provided
anvl_result resolve_context_spec(context_spec *spec_ptr, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_ctx_spec candidate;
   usize normalized_map_cap = 0;
   if (out_err_code) {
      *out_err_code = err_code;
   }
   // if the spec_ptr is NULL, return an error state
   if (!spec_ptr) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   // if no spec is provided, return the default spec
   if (!*spec_ptr) {
      *spec_ptr = &ANVL_CTX_BASE;
      return ANVL_RES_OK;
   }

   /* Transactional merge target. */
   candidate = ANVL_CTX_BASE;

   if ((*spec_ptr)->docs_cap) {
      candidate.docs_cap = (*spec_ptr)->docs_cap;
   }
   if ((*spec_ptr)->errs_cap) {
      candidate.errs_cap = (*spec_ptr)->errs_cap;
   }
   if ((*spec_ptr)->map_cap) {
      candidate.map_cap = (*spec_ptr)->map_cap;
   }

   if (Math.normalize_pow_2_min_checked(candidate.map_cap, 8, &normalized_map_cap) != SC_MATH_OK) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   candidate.map_cap = normalized_map_cap;

   /* Validation gate: fail before commit. */
   if (candidate.docs_cap == 0 || candidate.errs_cap == 0 || candidate.map_cap == 0) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   ANVL_CTX_BASE = candidate; // commit
   *spec_ptr = &ANVL_CTX_BASE;
   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}

/* ----------------------------------------------------------------- *
 * Module Function Declarations
 * ----------------------------------------------------------------- */
void mod_ctx_dispose_arena(bump_allocator);

/* ----------------------------------------------------------------------- *
 * AnvlMod management
 * ----------------------------------------------------------------------- */
#if 1 // module management
anvl_result mod_initialize(AnvlMod *out_mod, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   AnvlMod mod = NULL;

   module_context ctx;
   // we treat a NULL ctx_spec as the user saying they do not want a copy returned
   context_spec spec = NULL;
   if (mod_new(out_mod ? &mod : NULL, out_err_code) == ANVL_RES_OK &&
       mod_ctx_initialize(spec, &ctx, out_err_code) == ANVL_RES_OK &&
       mod_attach_context(&mod, ctx) == ANVL_RES_OK) {

      mod->root = NULL;
      *out_mod = mod;
   } else {
      err_code = out_err_code ? *out_err_code : ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   if (!mod) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   return ANVL_RES_OK;

error: {
   mod_dispose(&mod);

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (out_mod) {
      *out_mod = mod;
   }
   return ANVL_RES_ERR;
}
}
anvl_result mod_new(AnvlMod *out_mod, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   AnvlMod mod = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!out_mod || !out_err_code) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   *out_mod = NULL;

   mod = Allocator.alloc(mod_size);
   if (!mod) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   memset(mod, 0, mod_size);
   mod->context = NULL;
   mod->root = NULL;

   // Ensure the process-wide source registry exists before any document is
   // registered. The registry is cleared in mod_dispose().
   Registry.init();

   *out_mod = mod;
   return ANVL_RES_OK;

error: {
   Allocator.dispose(mod);
   mod = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (out_mod) {
      *out_mod = mod;
   }
   return ANVL_RES_ERR;
}
}
void mod_dispose(AnvlMod *mod_ptr) {
   if (!mod_ptr || !*mod_ptr) {
      return;
   }
   AnvlMod mod = *mod_ptr;
   if (mod->context) {
      mod_ctx_dispose(mod->context);
   }

   // Release this module's reference to the global source registry. When the
   // reference count reaches zero, the registry is cleared. Documents
   // unregister themselves individually during context disposal.
   Registry.release();

   Allocator.dispose(mod);
   *mod_ptr = NULL;
}
anvl_result mod_attach_context(AnvlMod *mod, module_context ctx) {
   module_context old_ctx = NULL;
   if (!mod || !*mod || !ctx) {
      goto error;
   }

   if ((*mod)->context == ctx) {
      return ANVL_RES_OK;
   }

   old_ctx = (*mod)->context;
   (*mod)->context = ctx;

   if (old_ctx) {
      mod_ctx_dispose(old_ctx);
   }

   return ANVL_RES_OK;

error:
   return ANVL_RES_ERR;
}
#endif

/* ----------------------------------------------------------------------- *
 * Context management
 * ----------------------------------------------------------------------- */
#if 1 // module_context management
// if context_spec is NULL, use default values
anvl_result mod_ctx_initialize(context_spec ctx_spec, module_context *out_ctx,
                               anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   module_context ctx = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!out_ctx || !out_err_code) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   *out_ctx = NULL;

   if (ANVL_RES_OK != resolve_context_spec(&ctx_spec, &err_code) || !ctx_spec) {
      goto error;
   }

   ctx = Allocator.alloc(ctx_size);
   if (!ctx) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(ctx, 0, ctx_size);

   ctx->docs = List.new(ctx_spec->docs_cap, sizeof(module_document));
   ctx->errors = List.new(ctx_spec->errs_cap, sizeof(anvl_error));
   ctx->statements = List.new(ANVL_CTX_DEFAULT_NODE_CAP, sizeof(anvl_statement));
   ctx->values = List.new(ANVL_CTX_DEFAULT_NODE_CAP, sizeof(anvl_value));
   if (!ctx->docs || !ctx->errors || !ctx->statements || !ctx->values) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   *out_ctx = ctx;
   *out_err_code = err_code;
   return ANVL_RES_OK;

error: {
   if (ctx) {
      List.dispose(ctx->docs);
      List.dispose(ctx->errors);
      List.dispose(ctx->statements);
      List.dispose(ctx->values);
      ctx->docs = NULL;
      ctx->errors = NULL;
      ctx->statements = NULL;
      ctx->values = NULL;
      Allocator.dispose(ctx);
   }
   ctx = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (out_ctx) {
      *out_ctx = ctx;
   }
   return ANVL_RES_ERR;
}
}
void mod_ctx_dispose(module_context ctx) {
   if (!ctx) {
      return;
   }
   mod_ctx_clear_docs(ctx->docs);
   mod_ctx_clear_errs(ctx->errors);
   ctx->docs = NULL;
   ctx->errors = NULL;

   // statements/values are non-owning indexes into the arena (see
   // notes/document-body-parse.md "Arena node iteration") — the arena release
   // below frees the pointed-to nodes themselves. But a value's own
   // collection/object list (.collection.items for ARRAY/TUPLE,
   // .object.statements for OBJECT) is a separate list, heap-allocated via
   // List.new — not part of the arena, so the arena release doesn't touch it,
   // and nothing else walks the value tree to free it. No tree recursion
   // needed to reach every one, though: source_new_node already registers
   // every value node — at any nesting depth — in this same flat ctx->values
   // index, so one pass over it here reaches them all.
   usize value_count = List.size(ctx->values);
   for (usize i = 0; i < value_count; i++) {
      anvl_value val = NULL;
      List.get(ctx->values, i, (object *)&val);
      if (!val) {
         continue;
      }
      if (val->type == ANVL_VALUE_ARRAY || val->type == ANVL_VALUE_TUPLE) {
         List.dispose(val->collection.items);
      } else if (val->type == ANVL_VALUE_OBJECT) {
         List.dispose(val->object.statements);
      }
   }

   // Same reasoning as the value-tree pass above, for a statement's own .attributes: each
   // anvl_attribute is its own individual heap allocation (not arena-owned, not just a list
   // container to release), so it needs disposing per-element, not just List.dispose on the
   // container. ctx->statements already indexes every statement node flatly, same as values.
   usize stmt_count = List.size(ctx->statements);
   for (usize i = 0; i < stmt_count; i++) {
      anvl_statement stmt = NULL;
      List.get(ctx->statements, i, (object *)&stmt);
      if (!stmt) {
         continue;
      }
      if (stmt->attributes) {
         usize attr_count = List.size(stmt->attributes);
         for (usize j = 0; j < attr_count; j++) {
            anvl_attribute attr = NULL;
            List.get(stmt->attributes, j, (object *)&attr);
            Allocator.dispose(attr);
         }
         List.dispose(stmt->attributes);
      }
      // OBJECT_BLOCK's own body — a nested statement list reached directly, not through a
      // value. Its member statements are already indexed flatly in ctx->statements (any
      // nesting depth), same as array/object values' items are in ctx->values, so this is
      // just releasing the list container, not walking into it again.
      //
      // OBJECT_BLOCK also synthesizes its own OBJECT-kind .value aliasing this same .body
      // list (parser.c, so anvil_statement_get_value works uniformly for both statement
      // forms — see notes/public-api.md), and the value-tree pass above already disposes
      // that same list via .value->object.statements — disposing it again here would
      // double-free. Skip whenever .value was synthesized (the normal case); the direct
      // dispose below is now only a defensive fallback for an OBJECT_BLOCK statement that
      // somehow never got its value synthesized.
      if (stmt->body && !(stmt->kind == ANVL_STMT_OBJECT_BLOCK && stmt->value)) {
         List.dispose(stmt->body);
      }
   }

   List.dispose(ctx->statements);
   List.dispose(ctx->values);
   ctx->statements = NULL;
   ctx->values = NULL;

   // identifiers' keys borrow statement-name slices (arena-owned) and its values are
   // anvl_statement pointers (also arena-owned) — Map.dispose only releases the map's own
   // bucket storage, nothing it points to, matching statements/values' non-owning relationship
   // to the arena above.
   if (ctx->identifiers) {
      Map.dispose(ctx->identifiers);
      ctx->identifiers = NULL;
   }

   mod_ctx_dispose_arena(ctx->arena);

   Allocator.dispose(ctx);
}
/*
 * Register a document with the context and global source registry. The document's
 * source must be loaded and have a non-zero hash. If the document is already
 * registered, the call fails with ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT.
 * If a filepath is provided, it is copied into the document's filepath field.
 * The document is appended to the context's docs list.
 * Returns ANVL_RES_OK on success, or ANVL_RES_ERR on failure.
 */
anvl_result mod_ctx_register_doc(module_context ctx, module_document doc, const char *filepath,
                                 anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!ctx || !doc || !doc->source) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   if (Source.hash(doc->source) == 0) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   if (Registry.find(Source.hash(doc->source)) != NULL) {
      err_code = ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT;
      goto error;
   }

   doc->context = ctx;
   if (filepath) {
      if (doc->filepath) {
         String.dispose(doc->filepath);
      }
      doc->filepath = String.copy((string)filepath);
      if (!doc->filepath) {
         err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
         goto error;
      }
   }

   if (Registry.add(doc, &err_code) != ANVL_RES_OK) {
      goto error;
   }

   List.append(ctx->docs, (object)doc);

   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}
/*
 * Set the parser for the context.
 */
void mod_ctx_set_parser(module_context ctx, anvl_parser parser) {
   // make sure we have a valid context and document
   if (!ctx || !parser) {
      return;
   }
   ctx->parser = parser;
}
/*
 * Get arena-backed bump allocator for the context. Sets out_err_code to ANVL_ERR_INVALID_ARGUMENT
 * if ctx is NULL, or ANVL_ERR_MEMORY_ALLOC_FAILED if the arena is not initialized. If allocation
 * fails, the context's arena is NULL.
 */
anvl_result mod_ctx_create_arena(module_context ctx, usize size, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!ctx || size == 0) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   ctx->arena = NULL;

   // get arena memory allocation
   bump_allocator arena = Allocator.create_bump(size);
   if (!arena) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   ctx->arena = arena;

   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}
/*
 * Calculate a size hint for the context's arena based on the summed length of all source files. The
 * size hint is the greater of the summed source length multiplied by ANVL_ARENA_SIZE_MULTIPLIER or
 * ANVL_ARENA_MIN_SIZE. This function does not allocate memory; it only computes a size value. The
 * caller is responsible for creating the arena with the returned size hint using
 * mod_ctx_create_arena(). Returns the calculated size hint.
 */
usize mod_ctx_arena_size_hint(usize summed_source_length) {
   // calculate arena size hint
   usize hint = summed_source_length * ANVL_ARENA_SIZE_MULTIPLIER;
   // ensure hint is at least the minimum size
   return hint > ANVL_ARENA_MIN_SIZE ? hint : ANVL_ARENA_MIN_SIZE;
}
/*
 * Dispose context's arena.
 */
void mod_ctx_dispose_arena(bump_allocator arena) {
   if (!arena) {
      return;
   }
   Allocator.release((sc_ctrl_base_s *)arena);
}
/* ----------------------------------------------------------------------- *
 * Import graph expansion
 * ----------------------------------------------------------------------- */
#define IMPORT_PATH_MAX 1024

static anvl_result import_load_dependencies(module_context ctx, module_document doc, list stack,
                                            usize *out_size, anvl_err_code *out_err_code);

static void import_resolve_dir(const char *path, char *out_dir, usize out_size) {
   if (!path || out_size == 0) {
      if (out_size > 0) {
         out_dir[0] = '\0';
      }
      return;
   }

   const char *last_slash = strrchr(path, '/');
   if (!last_slash) {
      if (out_size > 0) {
         out_dir[0] = '.';
         out_dir[1] = '\0';
      }
      return;
   }

   usize len = (usize)(last_slash - path);
   if (len >= out_size) {
      len = out_size - 1;
   }
   memcpy(out_dir, path, len);
   out_dir[len] = '\0';
}

static bool import_path_on_stack(list stack, const char *path) {
   if (!stack || !path) {
      return false;
   }

   for (usize i = 0; i < List.size(stack); i++) {
      const char *entry = NULL;
      List.get(stack, i, (object *)&entry);
      if (entry && strcmp(entry, path) == 0) {
         return true;
      }
   }
   return false;
}

static anvl_result import_load_child(module_context ctx, module_document parent, anvl_import imp,
                                     list stack, usize *out_size, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   usize path_len = Source.slice_length(imp->path);
   if (path_len < 2) {
      err_code = ANVL_ERR_IMPORT_FILE_NOT_FOUND;
      goto error;
   }

   // Strip surrounding quotes from the path slice.
   path_len -= 2;
   if (path_len >= IMPORT_PATH_MAX) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   char import_name[IMPORT_PATH_MAX];
   memcpy(import_name, imp->path.start + 1, path_len);
   import_name[path_len] = '\0';

   // Resolve relative to the parent document's directory.
   char resolved[IMPORT_PATH_MAX];
   if (import_name[0] == '/') {
      if (strlen(import_name) >= IMPORT_PATH_MAX) {
         err_code = ANVL_ERR_INVALID_ARGUMENT;
         goto error;
      }
      strcpy(resolved, import_name);
   } else {
      char parent_dir[IMPORT_PATH_MAX];
      import_resolve_dir(parent->filepath, parent_dir, sizeof(parent_dir));

      int written = snprintf(resolved, sizeof(resolved), "%s/%s", parent_dir[0] ? parent_dir : ".",
                             import_name);
      if (written < 0 || (usize)written >= sizeof(resolved)) {
         err_code = ANVL_ERR_INVALID_ARGUMENT;
         goto error;
      }
   }

   // Cycle detection: the resolved path must not already be on the recursion stack.
   if (import_path_on_stack(stack, resolved)) {
      err_code = ANVL_ERR_IMPORT_CYCLIC;
      goto error;
   }

   // Attempt to load the child source. If the file is missing or unreadable, report it.
   module_document child = NULL;
   if (ANVL_RES_OK != doc_initialize(&child, &err_code) || !child) {
      goto error;
   }

   anvl_result res = doc_load_source(child, ANVL_SOURCE_FROM_FILE, resolved, 0, &err_code);
   if (res != ANVL_RES_OK) {
      if (err_code == ANVL_ERR_IO_FILE_NOT_FOUND || err_code == ANVL_ERR_IO_FILE_READ ||
          err_code == ANVL_ERR_IO_INVALID_PATH) {
         err_code = ANVL_ERR_IMPORT_FILE_NOT_FOUND;
      }
      goto error_child;
   }

   // Try to register. If the source hash is already registered, this is a diamond: reuse the
   // existing document and discard the freshly loaded duplicate.
   res = mod_ctx_register_doc(ctx, child, resolved, &err_code);
   if (res != ANVL_RES_OK) {
      if (err_code == ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT) {
         module_document existing = Registry.find(Source.hash(child->source));
         if (existing) {
            imp->resolved = existing;
            doc_dispose(child);
            return ANVL_RES_OK;
         }
      }
      goto error_child;
   }

   // New document registered — count its source toward the module's body-parse
   // arena size hint before recursing into its own imports.
   if (out_size) {
      *out_size += Source.length(child->source);
   }

   // Scan its header and recurse into its imports.
   res = doc_scan_header(child, &err_code);
   if (res != ANVL_RES_OK) {
      goto error;
   }

   imp->resolved = child;

   res = import_load_dependencies(ctx, child, stack, out_size, &err_code);
   if (res != ANVL_RES_OK) {
      goto error;
   }

   return ANVL_RES_OK;

error_child:
   doc_dispose(child);
error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (err_code != ANVL_ERR_NONE) {
      doc_set_error(parent, err_code, 1, 1, parent->filepath);
   }
   return ANVL_RES_ERR;
}

static anvl_result import_load_dependencies(module_context ctx, module_document doc, list stack,
                                            usize *out_size, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (!doc || !doc->filepath) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   // Push this document onto the recursion stack for cycle detection.
   List.append(stack, (object)doc->filepath);

   for (usize i = 0; i < List.size(doc->header->imports); i++) {
      anvl_import imp = NULL;
      List.get(doc->header->imports, i, (object *)&imp);
      if (!imp) {
         continue;
      }

      if (ANVL_RES_OK != import_load_child(ctx, doc, imp, stack, out_size, &err_code)) {
         goto error_pop;
      }
   }

   List.remove(stack, List.size(stack) - 1);
   return ANVL_RES_OK;

error_pop:
   List.remove(stack, List.size(stack) - 1);
error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}

anvl_result mod_load_imports(module_context ctx, module_document root, usize *out_body_size_hint,
                             anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!ctx || !root) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   // Root counts toward the arena size hint too — it never passes through
   // import_load_child, so it isn't picked up by the recursion below.
   usize total = Source.length(root->source);

   list stack = List.new(8, sizeof(const char *));
   if (!stack) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   anvl_result res = import_load_dependencies(ctx, root, stack, &total, &err_code);

   List.dispose(stack);

   if (out_body_size_hint) {
      *out_body_size_hint = total;
   }
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return res;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}
void mod_ctx_clear_docs(list docs) {
   // iterate through the list and dispose of each module_document
   for (usize i = 0; i < List.size(docs); i++) {
      module_document doc = NULL;
      List.get(docs, i, (object *)&doc);
      if (doc) {
         doc_dispose(doc);
      }
   }
   List.dispose(docs);
}
void mod_ctx_clear_errs(list errors) {
   for (usize i = 0; i < List.size(errors); i++) {
      anvl_error err = NULL;
      List.get(errors, i, (object *)&err);
      if (err) {
         Allocator.dispose(err);
      }
   }
   List.dispose(errors);
}

#endif