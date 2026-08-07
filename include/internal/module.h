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
 * module.h - Root & Document structures                                  *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * Created: 2026-07-13                                                    *
 * File: include/internal/module.h                                        *
 * ---------------------------------------------------------------------- *
 * Description:                                                           *
 * This header defines the root and document structures for Anvil.        *
 * ********************************************************************** */
#pragma once

#include "types.h"
#include "constants.h"
#include "errors.h"
// ----------------
#include <sigma/list.h>
#include <sigma/map.h>
#include <sigma/types.h>

/* ---------------------------------------------------------------------- *
 * Forward Declarations:
 * ---------------------------------------------------------------------- */
//   - objects
typedef struct anvl_source_t *anvl_source;
typedef struct anvl_parser_t *anvl_parser;
//   - functions/delegates
typedef void (*set_parser_fn)(module_document, anvl_parser);

struct anvl_mod_ctx_t {
   list docs;          // list of module_document
   list errors;        // list of anvl_error encountered during scanning/parsing
   map doc_map;        // namespace key -> (addr)doc_identity pointer
   anvl_parser parser; // parser context for the module
};
/* ---------------------------------------------------------------------- *
 * Document structure
 * ---------------------------------------------------------------------- */
typedef struct anvl_mod_doc_i {
   set_parser_fn set_parser; // function to set the parser for the document
} anvl_mod_doc_i;
extern const anvl_mod_doc_i Document;

struct anvl_mod_doc_t {
   anvl_source source;       // source for the document
   const anvl_mod_doc_i *op; // operations for the document
};
struct anvl_doc_identity_t {
   string namespace;    // namespace key used in module_context.doc_map
   string filepath;     // normalized path to the loaded module
   module_document doc; // owned/linked document handle for this identity
};

/* ---------------------------------------------------------------------- *
 * Internal module function prototypes
 * ---------------------------------------------------------------------- */
#if 1 // module functions
/**
 * @brief Initialize a module context.
 * @param[out] out_mod Pointer to the module object to initialize.
 * @param[out] out_err_code Pointer to the error code if initialization fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 */
anvl_result mod_initialize(AnvlMod *, anvl_err_code *);
/**
 * @brief Initialize a minimal module object with default values and `NULL` context.
 * @param[out] out_mod Pointer to the module object to initialize.
 * @param[out] out_err_code Pointer to the error code if initialization fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function allocates a new module object and initializes its members to default
 * values. The context is set to `NULL` and must be initialized separately using
 * `mod_attach_context()`. If allocation fails, an error code is returned.
 */
anvl_result mod_new(AnvlMod *, anvl_err_code *);
/**
 * @brief Dispose of a module, releasing all associated resources.
 * @param mod Pointer to the module to dispose.
 */
void mod_dispose(AnvlMod *);
/**
 * @brief Attach a context to a module.
 * @param mod Pointer to the module to attach the context to.
 * @param ctx The context to attach.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function attaches a context to the specified module. If the module already has a
 * context, it will be replaced. The caller is responsible for managing the lifetime of the context
 * and ensuring it is properly disposed of when no longer needed.
 */
anvl_result mod_attach_context(AnvlMod *, module_context);
/* ---------------------------------------------------------------------- *
 * Internal context function prototypes
 * ---------------------------------------------------------------------- */
/**
 * @brief Resolve the context specification, returning the default if NULL is provided.
 * @param[out] spec_ptr Pointer to the context specification to resolve.
 * @param[out] out_err_code Pointer to the error code if resolution fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function resolves the context specification for module initialization. If the
 * provided spec_ptr is NULL (no variable provided), it returns an error state. Otherwise, it
 * returns the default context specification. If a valid spec_ptr is provided, it updates the base
 * context specification with any non-zero values from the provided spec.
 */
anvl_result resolve_context_spec(context_spec *, anvl_err_code *);
/**
 * @brief Initialize a module context with the default context spec.
 * @param[out] out_ctx Pointer to the module context to initialize.
 * @param[out] out_err_code Pointer to the error code if initialization fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function initializes a module context based on the default context spec.
 */
anvl_result mod_ctx_initialize(context_spec, module_context *, anvl_err_code *);
/**
 * @brief Dispose of a module context, releasing all associated resources.
 * @param ctx The module context to dispose.
 */
void mod_ctx_dispose(module_context);
void mod_ctx_add_doc(module_context, module_document);
void mod_ctx_set_parser(module_context, anvl_parser);
/**
 * @brief Clear all documents from the module context.
 * @param docs List of documents to clear.
 */
void mod_ctx_clear_docs(list);
/**
 * @brief Clear all errors from the module context.
 * @param errors List of errors to clear.
 */
void mod_ctx_clear_errs(list);
#endif

/* ---------------------------------------------------------------------- *
 * Internal document functions
 * ---------------------------------------------------------------------- */
anvl_result doc_initialize(const char *, module_document *, anvl_err_code *);
void doc_dispose(module_document);
anvl_result doc_create_source(const char *, module_document *, anvl_err_code *);
void doc_dispose_source(module_document);
bool doc_has_errors(module_document);
void doc_set_error(module_document, anvl_err_code, usize, usize, const char *);

/* ---------------------------------------------------------------------- *
 * Internal root functions - deprecated
 * ---------------------------------------------------------------------- */
anvl_result root_load_source(module_context, const char *);
anvl_result root_get_doc(module_context, usize, module_document *);