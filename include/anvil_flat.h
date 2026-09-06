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
 * @brief Load, scan, import, parse, and resolve a document from an in-memory
 * buffer, rather than a file path — otherwise identical to anvil_load.
 * @param source The source text. Not required to be NUL-terminated; exactly
 * `length` bytes are read, and nothing beyond it.
 * @param length Length of `source` in bytes.
 * @return Same contract as anvil_load: a non-NULL handle for every failure
 * category (check anvil_has_errors()/anvil_get_error()), NULL only if the
 * handle itself could not be allocated.
 */
anvil_document anvil_load_buffer(const char *source, size_t length);

/**
 * @brief Parse a single, standalone value expression (a "Value Fragment")
 * with no enclosing document/statement context — no header, no imports, no
 * top-level statements. Unlike anvil_load/anvil_load_buffer, a '$identifier'
 * VarRef is never supported here, at any nesting depth: there is no
 * identifier map to resolve one against outside a real document.
 * @param text The fragment source text. Not required to be NUL-terminated;
 * exactly `length` bytes are read. A trailing ';' is tolerated but never
 * required; any other trailing content after the value is an error.
 * @param length Length of `text` in bytes.
 * @return Same contract as anvil_load: a non-NULL handle for every failure
 * category (check anvil_has_errors()/anvil_get_error()), NULL only if the
 * handle itself could not be allocated. Use
 * anvil_document_get_fragment_value() to retrieve the parsed value.
 */
anvil_document anvil_parse_value_fragment(const char *text, size_t length);

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
 * @brief The diagnostic detail behind this document's recorded error, if
 * any — richer than anvil_get_error()'s stable category alone.
 * @param doc The document to check. NULL, or a document with no recorded
 * error, returns NULL.
 */
anvil_error anvil_document_get_error(anvil_document doc);

/**
 * @brief Same category anvil_get_error(doc) would report — provided here
 * too so a caller holding only the anvil_error handle never needs the
 * document back to know it.
 * @param err The error to check. NULL returns ANVIL_OK.
 */
anvil_err_code anvil_error_get_category(anvil_error err);

/**
 * @brief A specific, human-readable message for this error, copied into a
 * caller-supplied buffer. Same buffer-sizing convention as
 * anvil_statement_get_name. Empty (returns 0) when the failure category has
 * no meaningful message beyond its category (e.g. ANVIL_ERR_IO).
 * @param err The error to read. NULL writes nothing and returns 0.
 */
size_t anvil_error_get_message(anvil_error err, char *buf, size_t buflen);

/**
 * @brief The 1-based source line this error was recorded at.
 * @param err The error to check. NULL, or a category with no meaningful
 * source position (e.g. ANVIL_ERR_IO), returns 0.
 */
size_t anvil_error_get_line(anvil_error err);

/**
 * @brief The 1-based source column this error was recorded at, same
 * contract as anvil_error_get_line.
 */
size_t anvil_error_get_column(anvil_error err);

/**
 * @brief Look up a top-level statement by name.
 * @param doc The document to search.
 * @param name The statement's declared name.
 * @return The statement handle, or NULL if doc/name is NULL, doc failed to
 * load, or no top-level statement has that name.
 */
anvil_statement anvil_statement_get(anvil_document doc, const char *name);

/**
 * @brief A heapless cursor over this document's own top-level statements, in
 * declaration order (never descending into nested/imported documents' own
 * statements) — backed by Sigma's Query/sc_queryable mechanism over
 * `module_document->body` (an farray), not a hand-rolled scan.
 * @param doc The document to iterate. A document whose body parse failed
 * partway still has a real (if partial or empty) body to iterate — the
 * pipeline freezes whatever was captured before the failure rather than
 * discarding it — so this only returns NULL for `doc` itself being NULL or
 * never having reached body-parsing at all, never for a mid-parse failure.
 * @return The iterator handle, or NULL as described above. Dispose with
 * anvil_statement_iterator_dispose once done — never valid after `doc`
 * itself is disposed.
 */
anvil_statement_iterator anvil_document_get_statements(anvil_document doc);

/**
 * @brief Pull the next statement from an iterator, in order.
 * @param it The iterator to advance (mutated in place).
 * @param out_stmt Set to the next statement on success; untouched otherwise.
 * @return true if a statement was yielded; false once exhausted, or if
 * it/out_stmt is NULL.
 */
bool anvil_statement_iterator_next(anvil_statement_iterator it, anvil_statement *out_stmt);

/**
 * @brief Release an iterator. Safe to call with NULL (no-op). Does not
 * affect the document it was created from.
 */
void anvil_statement_iterator_dispose(anvil_statement_iterator it);

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

/**
 * @brief The value parsed by anvil_parse_value_fragment.
 * @param doc The document to read. NULL, a document from anvil_load/
 * anvil_load_buffer (never a fragment), or a fragment that failed to parse
 * all return NULL.
 */
anvil_value anvil_document_get_fragment_value(anvil_document doc);

/**
 * @brief Module-level (`doc->header`) attribute count.
 * @param doc The document to check. NULL returns 0.
 */
size_t anvil_document_get_attribute_count(anvil_document doc);

/**
 * @brief The module-level attribute at `index`.
 * @param doc The document to index.
 * @param index Zero-based attribute index.
 * @return The attribute handle, or NULL if doc is NULL or index is out of
 * bounds.
 */
anvil_attribute anvil_document_get_attribute(anvil_document doc, size_t index);

/**
 * @brief The module-level attribute named `key`, if any.
 * @param doc The document to search.
 * @param key The attribute's key.
 * @return The attribute handle, or NULL if not found.
 */
anvil_attribute anvil_document_find_attribute(anvil_document doc, const char *key);

/**
 * @brief A statement's own `@[...]` attribute count.
 * @param stmt The statement to check. NULL returns 0.
 */
size_t anvil_statement_get_attribute_count(anvil_statement stmt);

/**
 * @brief The statement attribute at `index`.
 * @param stmt The statement to index.
 * @param index Zero-based attribute index.
 * @return The attribute handle, or NULL if stmt is NULL or index is out of
 * bounds.
 */
anvil_attribute anvil_statement_get_attribute(anvil_statement stmt, size_t index);

/**
 * @brief The statement attribute named `key`, if any.
 * @param stmt The statement to search.
 * @param key The attribute's key.
 * @return The attribute handle, or NULL if not found.
 */
anvil_attribute anvil_statement_find_attribute(anvil_statement stmt, const char *key);

/**
 * @brief An attribute's key, copied into a caller-supplied buffer. Same
 * buffer-sizing convention as anvil_statement_get_name.
 * @param attr The attribute to read. NULL writes nothing and returns 0.
 */
size_t anvil_attribute_get_key(anvil_attribute attr, char *buf, size_t buflen);

/**
 * @brief An attribute's value, copied into a caller-supplied buffer. Same
 * buffer-sizing convention as anvil_statement_get_name. A flag attribute
 * (`@[active]`, no `=value`) has no value — returns 0.
 * @param attr The attribute to read. NULL writes nothing and returns 0.
 */
size_t anvil_attribute_get_value(anvil_attribute attr, char *buf, size_t buflen);
