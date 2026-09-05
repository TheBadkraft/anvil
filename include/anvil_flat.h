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
 * anvil_flat.h - Public ABI: flat exported functions for Anvil Native    *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * The guaranteed-universal Anvil Native ABI surface — every function     *
 * here is an independently-callable, individually-documented extern      *
 * symbol, reachable via ordinary FFI from any language (JNI, ctypes,     *
 * N-API, extern "C" linking, P/Invoke) with no extra machinery. A        *
 * vtable convenience layer will mirror this surface later, one entry     *
 * per function here, but this header is never conditional on that layer *
 * existing. See notes/public-api.md for the full design.                *
 * ********************************************************************** */
#pragma once

#include "anvil_types.h"
#include <stddef.h>

/**
 * @brief Load, scan, import, parse, and resolve a document from a file path.
 * @param filepath Path to the root .anvl/.aml/.amp source file.
 * @return A document handle on success. Also returns a valid, non-NULL
 * handle when a later pipeline phase fails (bad path, syntax error, import
 * or resolve failure) — check anvil_has_errors()/anvil_get_error() to know
 * whether the document actually loaded cleanly. Returns NULL only when the
 * handle itself could not be allocated.
 */
anvil_document anvil_load(const char *filepath);

/**
 * @brief Release a document and everything it owns (every imported
 * document, the shared arena, all recorded errors).
 * @param doc The document to dispose. Safe to call with NULL (no-op).
 */
void anvil_dispose(anvil_document doc);

/**
 * @brief Whether any error was recorded while loading this document.
 * @param doc The document to check. NULL returns false.
 */
bool anvil_has_errors(anvil_document doc);

/**
 * @brief The category of the first error recorded while loading this
 * document, if any.
 * @param doc The document to check. NULL returns ANVIL_ERR_INVALID_ARGUMENT.
 * @return ANVIL_OK if anvil_has_errors(doc) is false.
 */
anvil_err_code anvil_get_error(anvil_document doc);

/**
 * @brief The Anvil Native library version string.
 */
const char *anvil_get_version(void);

/**
 * @brief Look up a top-level statement by name.
 * @param doc The document to search.
 * @param name The statement's declared name.
 * @return The statement handle, or NULL if doc/name is NULL, doc failed to
 * load, or no top-level statement has that name.
 */
anvil_statement anvil_statement_get(anvil_document doc, const char *name);

/**
 * @brief A statement's own value.
 * @param stmt The statement to read.
 * @return The value handle, or NULL if stmt is NULL or is an anonymous
 * OBJECT_BLOCK (which has no value of its own — only a body; not yet a
 * supported traversal, see notes/public-api.md).
 */
anvil_value anvil_statement_get_value(anvil_statement stmt);

/**
 * @brief A statement's declared name, copied into a caller-supplied buffer.
 * @param stmt The statement to read. NULL writes nothing and returns 0.
 * @param buf Destination buffer, or NULL to only query the required length.
 * @param buflen Size of buf in bytes, including room for the NUL terminator.
 * @return The full name length (excluding NUL), regardless of buflen — call
 * once with buf NULL/buflen 0 to size a buffer, matching snprintf's
 * convention. If buf is non-NULL, the copy is NUL-terminated and truncated
 * to fit buflen if the name doesn't fit (compare the return value against
 * buflen to detect truncation).
 */
size_t anvil_statement_get_name(anvil_statement stmt, char *buf, size_t buflen);

/**
 * @brief A value's kind. Transparent through a resolved $identifier VarRef
 * — reports the target's own type, never a distinct "this was a reference"
 * kind. An unresolved VarRef (missing target, or a reference cycle) reports
 * as ANVIL_VALUE_NULL.
 * @param val The value to check. NULL returns ANVIL_VALUE_NULL.
 */
anvil_value_type anvil_value_get_type(anvil_value val);

/**
 * @brief A scalar value's text, copied into a caller-supplied buffer.
 * Transparent through a resolved VarRef, same as anvil_value_get_type.
 *
 * For ANVIL_VALUE_STRING specifically, this resolves escape sequences
 * (\n, \t, \r, \\, \") to their real byte values first — an unrecognized
 * escape (backslash followed by anything else) passes both characters
 * through unchanged, deterministic either way. Every other kind (including
 * ARRAY/TUPLE/OBJECT) returns its raw source span as-is, unprocessed.
 * @param val The value to read. NULL writes nothing and returns 0.
 * @param buf Destination buffer, or NULL to only query the required length.
 * @param buflen Size of buf in bytes, including room for the NUL terminator.
 * @return The full text length (excluding NUL), same buffer-sizing
 * convention as anvil_statement_get_name.
 */
size_t anvil_value_get_text(anvil_value val, char *buf, size_t buflen);

/**
 * @brief Element count for an ARRAY/TUPLE, or statement count for an
 * OBJECT. Transparent through a resolved VarRef.
 * @param val The value to check. NULL, or anything else, returns 0.
 */
size_t anvil_value_get_count(anvil_value val);

/**
 * @brief The element at `index` of an ARRAY/TUPLE. Transparent through a
 * resolved VarRef.
 * @param val The array/tuple to index.
 * @param index Zero-based element index.
 * @return The element's value handle, or NULL if val isn't an ARRAY/TUPLE
 * or index is out of bounds.
 */
anvil_value anvil_value_get_element(anvil_value val, size_t index);

/**
 * @brief The statement at `index` of an OBJECT's nested field list.
 * Transparent through a resolved VarRef.
 * @param val The object to index.
 * @param index Zero-based statement index.
 * @return The statement handle, or NULL if val isn't an OBJECT or index is
 * out of bounds.
 */
anvil_statement anvil_value_get_statement(anvil_value val, size_t index);
