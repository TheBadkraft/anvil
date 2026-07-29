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
typedef struct anvl_src_t *anvl_src;
typedef struct anvl_parser *parser;
/* ----------------------------------------------------------------------- *
 *   - functions                                                           *
 * ----------------------------------------------------------------------- */
// typedef function: set parser for the module context
typedef void (*set_parser_fn)(anvl_doc d, parser p);

struct anvl_mod_ctx_t {
  list docs;       // list of anvl_doc
  list errors;       // list of errors encountered during scanning/parsing
  parser parser; // parser context for the module
};
/* ----------------------------------------------------------------------- *
 * Document structure                                                      *
 * ----------------------------------------------------------------------- */
struct anvl_doc_t {
  anvl_ctx context; // pointer to the module context
  string filepath; // path to the loaded module
  anvl_src source; // source for the document
  const set_parser_fn set_parser; // virtual table for document operations
};

/* ----------------------------------------------------------------------- *
 * Internal module functions                                               *
 * ----------------------------------------------------------------------- */
bool mod_init(AnvlMod *out_mod);
void mod_dispose(AnvlMod *mod);
bool mod_ctx_init(anvl_ctx *out_ctx);
void mod_ctx_dispose(anvl_ctx ctx);
void mod_ctx_add_doc(anvl_ctx ctx, anvl_doc doc);
void mod_ctx_set_parser(anvl_ctx ctx, parser p);
void mod_docs_clear(list docs);
void mod_errs_clear(list errors);

/* ----------------------------------------------------------------------- *
 * Internal root functions                                                 *
 * ----------------------------------------------------------------------- */
bool root_load_source(anvl_ctx ctx, const char *filepath);
bool root_get_doc(anvl_ctx ctx, usize index, anvl_doc *out_doc);
void root_dispose_doc(anvl_doc doc);

/* ----------------------------------------------------------------------- *
 * Internal document functions                                             *
 * ----------------------------------------------------------------------- */
bool doc_initialize(const char *filepath, anvl_doc *out_doc);
void doc_dispose(anvl_doc doc);
bool doc_has_errors(anvl_doc doc);
void doc_set_error(anvl_doc doc, anvl_error_code code, usize line, usize col, const char *file);