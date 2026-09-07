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
 * anvil_types.h - Public ABI types for Anvil Native                      *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Opaque handles and stable enums shared by every public Anvil header.   *
 * Never includes anything from an internal header — nothing in here is   *
 * permitted to depend on, or leak, an internal representation. See       *
 * notes/public-api.md for the full design.                              *
 * ********************************************************************** */
#pragma once

#include <stdbool.h>

/**
 * @brief Opaque handle to a loaded, fully-resolved Anvil document.
 *
 * Not thread-safe: a single anvil_document must only be used from the
 * thread that obtained it, unless the caller provides its own external
 * synchronization. Never dereferenced by the caller — every operation on
 * it goes through an accessor function.
 */
typedef struct anvil_document_t *anvil_document;

/**
 * @brief Opaque handle to one statement (a declared name and what it's
 * assigned, or an anonymous OBJECT_BLOCK's namespace).
 *
 * Lifetime is tied to the anvil_document it came from — never valid after
 * that document is disposed.
 */
typedef struct anvil_statement_t *anvil_statement;

/**
 * @brief Opaque, heapless-scan cursor over a document's own top-level
 * statements — see anvil_document_get_statements/anvil_statement_iterator_next.
 *
 * Owns only its own scan position, never the statements it yields (those
 * follow anvil_statement's own lifetime, tied to the anvil_document). Never
 * valid after that document is disposed. Dispose with
 * anvil_statement_iterator_dispose once done.
 */
typedef struct anvil_statement_iterator_t *anvil_statement_iterator;

/**
 * @brief Opaque, heapless-scan cursor over a document's own direct imports
 * (`import "...";`, not the whole transitive graph) — see
 * anvil_document_get_imports/anvil_document_iterator_next.
 *
 * Owns only its own scan position, never the documents it yields — those are
 * the caller's own responsibility. A yielded anvil_document is a real, fully
 * usable handle (anvil_document_get_statements, anvil_document_get_attribute*,
 * etc. all work on it normally) but does not own the underlying parse
 * context — it shares that with the document anvil_document_get_imports was
 * called on. Dispose each yielded handle with anvil_dispose once done with
 * it, same as any other anvil_document — safe, since disposing one of these
 * only frees the small handle itself and never touches the shared context
 * (so disposing a yielded handle is fine even while the owning document, or
 * other handles sharing its context, are still in use). Never valid after
 * the owning document is disposed. Dispose the iterator itself with
 * anvil_document_iterator_dispose once done — this does not affect any
 * document handle it already yielded, disposed or not.
 */
typedef struct anvil_document_iterator_t *anvil_document_iterator;

/**
 * @brief Opaque handle to one value.
 *
 * Lifetime is tied to the anvil_document it came from — never valid after
 * that document is disposed. A resolved `$identifier` VarRef is always
 * transparent here: every accessor reports the target's own type/content,
 * never a distinct "this was a reference" type — see anvil_value_get_type.
 */
typedef struct anvil_value_t *anvil_value;

/**
 * @brief Opaque handle to one `@[key]`/`@[key=value]` attribute, attached
 * either to a document's header (module-level) or to an individual
 * statement.
 *
 * Lifetime is tied to the anvil_document it came from — never valid after
 * that document is disposed.
 */
typedef struct anvil_attribute_t *anvil_attribute;

/**
 * @brief Opaque handle to the diagnostic detail behind a document's
 * recorded error, if any — richer than the stable anvil_err_code category
 * alone (a specific message, plus source line/column), pushed the moment
 * the pipeline phase that produced anvil_get_error()'s category actually
 * failed. Deliberately does not expose the specific internal anvl_err_code
 * number itself, only its static message text — internal code churn still
 * can never become a public ABI break through this handle.
 *
 * Lifetime is tied to the anvil_document it came from — never valid after
 * that document is disposed.
 */
typedef struct anvil_error_t *anvil_error;

/**
 * @brief Stable public value-kind enum.
 *
 * Mirrors the internal anvl_value_type categories a caller can actually do
 * something useful with. Two are deliberately absent: VARREF (transparent —
 * see anvil_value, above — an unresolved reference just reports as
 * ANVIL_VALUE_NULL) and the internal NONE placeholder (never a real parsed
 * value's type).
 */
typedef enum {
   ANVIL_VALUE_NULL = 0,
   ANVIL_VALUE_BOOL,
   ANVIL_VALUE_NUMERIC,
   ANVIL_VALUE_STRING,
   ANVIL_VALUE_BLOB,
   ANVIL_VALUE_IDENTIFIER,
   ANVIL_VALUE_ARRAY,
   ANVIL_VALUE_TUPLE,
   ANVIL_VALUE_OBJECT,
} anvil_value_type;

/**
 * @brief Stable, deliberately small public error category.
 *
 * One category per document-pipeline phase, plus the cross-cutting
 * infrastructure cases. Distinct from (and never numerically tied to) the
 * internal anvl_err_code enum, which carries ~90 granular parser/resolver
 * codes that are free to change as the implementation evolves without
 * that ever being a public ABI break. Full detail (message text, line/
 * column) is reachable through separate accessors, not this enum.
 */
typedef enum {
   ANVIL_OK = 0,
   ANVIL_ERR_IO,                // couldn't read the source at all
   ANVIL_ERR_HEADER,            // phase 1 - shebang/import/attribute scan
   ANVIL_ERR_IMPORT,            // phase 2 - import graph loading
   ANVIL_ERR_SYNTAX,            // phase 3 - body/grammar
   ANVIL_ERR_RESOLVE,           // phase 4 - identifier/base/inheritance resolution
   ANVIL_ERR_MEMORY,            // allocation failure
   ANVIL_ERR_INVALID_ARGUMENT,  // bad call into the API itself
} anvil_err_code;
