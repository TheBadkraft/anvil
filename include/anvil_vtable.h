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
 * anvil_vtable.h - Public ABI: vtable convenience layer for Anvil Native *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * A single-level (never nested) `extern const` struct of function       *
 * pointers per logical group, mirroring anvil_flat.h one-for-one — every *
 * field here is the *exact same function* as its flat counterpart, not a *
 * separate implementation, verified by pointer identity in              *
 * test/unit/test_anvil_vtable.c. Deliberately includes only              *
 * anvil_types.h, never anvil_flat.h — a file that includes this header   *
 * alone has no way to see the individual flat function names, keeping    *
 * the two calling styles from mixing. No getter function: these are      *
 * plain exported data symbols, linkable the same way from every target   *
 * language's FFI as the internal codebase's own `Anvl` vtable already is *
 * from C. See notes/public-api.md.                                      *
 *                                                                        *
 * NOTE: `Anvil` (full word) here is a *different* symbol from the        *
 * legacy `Anvl` (missing the second 'i') declared in anvil.h — an        *
 * unfortunate but deliberate near-collision inherited from that file's   *
 * own retirement (anvil.h's own header comment explains why `Anvl` still *
 * exists). Do not conflate the two.                                     *
 * ********************************************************************** */
#pragma once

#include "anvil_types.h"
#include <stddef.h>

typedef struct anvil_i {
   anvil_document (*load)(const char *filepath);
   anvil_document (*load_buffer)(const char *source, size_t length);
   void (*dispose)(anvil_document doc);
   bool (*has_errors)(anvil_document doc);
   anvil_err_code (*get_error)(anvil_document doc);
   const char *(*get_version)(void);
   anvil_document (*parse_value_fragment)(const char *text, size_t length);
} anvil_i;
extern const anvil_i Anvil;

typedef struct anvil_statement_i {
   anvil_statement (*get)(anvil_document doc, const char *name);
   anvil_value (*get_value)(anvil_statement stmt);
   size_t (*get_name)(anvil_statement stmt, char *buf, size_t buflen);
   size_t (*get_attribute_count)(anvil_statement stmt);
   anvil_attribute (*get_attribute)(anvil_statement stmt, size_t index);
   anvil_attribute (*find_attribute)(anvil_statement stmt, const char *key);
} anvil_statement_i;
extern const anvil_statement_i Statement;

typedef struct anvil_document_i {
   size_t (*get_attribute_count)(anvil_document doc);
   anvil_attribute (*get_attribute)(anvil_document doc, size_t index);
   anvil_attribute (*find_attribute)(anvil_document doc, const char *key);
   anvil_value (*get_fragment_value)(anvil_document doc);
   anvil_statement_iterator (*get_statements)(anvil_document doc);
   anvil_document_iterator (*get_imports)(anvil_document doc);
   anvil_error (*get_error)(anvil_document doc);
} anvil_document_i;
extern const anvil_document_i Document;

typedef struct anvil_statement_iterator_i {
   bool (*next)(anvil_statement_iterator it, anvil_statement *out_stmt);
   void (*dispose)(anvil_statement_iterator it);
} anvil_statement_iterator_i;
extern const anvil_statement_iterator_i StatementIterator;

typedef struct anvil_document_iterator_i {
   bool (*next)(anvil_document_iterator it, anvil_document *out_doc);
   void (*dispose)(anvil_document_iterator it);
} anvil_document_iterator_i;
extern const anvil_document_iterator_i DocumentIterator;

typedef struct anvil_attribute_i {
   size_t (*get_key)(anvil_attribute attr, char *buf, size_t buflen);
   size_t (*get_value)(anvil_attribute attr, char *buf, size_t buflen);
} anvil_attribute_i;
extern const anvil_attribute_i Attribute;

typedef struct anvil_error_i {
   anvil_err_code (*get_category)(anvil_error err);
   size_t (*get_message)(anvil_error err, char *buf, size_t buflen);
   size_t (*get_line)(anvil_error err);
   size_t (*get_column)(anvil_error err);
} anvil_error_i;
extern const anvil_error_i Error;

typedef struct anvil_value_i {
   anvil_value_type (*get_type)(anvil_value val);
   size_t (*get_text)(anvil_value val, char *buf, size_t buflen);
   size_t (*get_count)(anvil_value val);
   anvil_value (*get_element)(anvil_value val, size_t index);
   anvil_statement (*get_statement)(anvil_value val, size_t index);
} anvil_value_i;
extern const anvil_value_i Value;
