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
 * module.h - Root & Document structures                                   *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-13                                                     *
 * File: include/internal/module.h                                         *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * This header defines the root and document structures for Anvil.         *
 * *********************************************************************** */
#pragma once

#include "types.h"
#include "errors.h"
// -----------------------------------------------------------------
#include <sigma/list.h>
#include <sigma/types.h>

/* ----------------------------------------------------------------------- *
 * Forward Declarations:                                                   *
 *   - objects                                                             *
 * ----------------------------------------------------------------------- */
typedef struct anvl_source_t *anvl_source;
typedef struct anvl_parser_t *anvl_parser;
/* ----------------------------------------------------------------------- *
 *   - functions/delegates                                                 *
 * ----------------------------------------------------------------------- */
// typedef function: set parser for the module context
typedef void (*set_parser_fn)(module_document, anvl_parser);

struct anvl_mod_ctx_t {
   list docs;          // list of module_document
   list errors;        // list of anvl_error encountered during scanning/parsing
   anvl_parser parser; // parser context for the module
};
/* ----------------------------------------------------------------------- *
 * Document structure                                                      *
 * ----------------------------------------------------------------------- */
typedef struct anvl_doc_i {
   set_parser_fn set_parser; // function to set the parser for the document
} anvl_doc_i;
extern const anvl_doc_i Document;
struct anvl_doc_t {
   module_context context; // pointer to the module context
   string filepath;        // path to the loaded module
   anvl_source source;     // source for the document
   const anvl_doc_i *op;   // operations for the document
};

/* ----------------------------------------------------------------------- *
 * Internal module functions                                               *
 * ----------------------------------------------------------------------- */
bool mod_init(AnvlMod *);
void mod_dispose(AnvlMod *);
bool mod_ctx_init(module_context *);
void mod_ctx_dispose(module_context);
void mod_ctx_add_doc(module_context, module_document);
void mod_ctx_set_parser(module_context, anvl_parser);
void mod_docs_clear(list);
void mod_errs_clear(list);

/* ----------------------------------------------------------------------- *
 * Internal document functions                                             *
 * ----------------------------------------------------------------------- */
bool doc_initialize(const char *, module_document *);
void doc_dispose(module_document);
bool doc_create_source(const char *, module_document *);
void doc_dispose_source(module_document);
bool doc_has_errors(module_document);
void doc_set_error(module_document, anvl_err_code, usize, usize, const char *);

/* ----------------------------------------------------------------------- *
 * Internal root functions - deprecated                                    *
 * ----------------------------------------------------------------------- */
bool root_load_source(module_context, const char *);
bool root_get_doc(module_context, usize, module_document *);
void root_dispose_doc(module_document);