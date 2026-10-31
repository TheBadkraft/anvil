/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * grammar.h - Internal: ANVL grammar rules shared by the writer/builder  *
 * ---------------------------------------------------------------------- *
 * Not a public header. The one place the writer add-on states what the   *
 * grammar allows, so the streaming writer and the document builder can't *
 * disagree. Every check returns ANVIL_WRITER_OK or the error to report.  *
 * Restated from the reader (src/core/source.c, parser.c) because the     *
 * reader isn't linked; test_writer_roundtrip.c catches drift.            *
 * ********************************************************************** */
#pragma once

#include "anvil_writer_types.h"

/** @brief Statement name, base or varref target: an identifier that isn't a reserved word. */
anvil_writer_err_code wg_check_name(const char *s);
/** @brief Attribute key: an identifier (reserved words are allowed, as in the parser). */
anvil_writer_err_code wg_check_attribute_key(const char *s);
/** @brief Raw attribute value text, emitted verbatim. */
anvil_writer_err_code wg_check_attribute_value(const char *raw);
/** @brief Text to be emitted inside double quotes as an attribute value. */
anvil_writer_err_code wg_check_attribute_text(const char *text, size_t length);
anvil_writer_err_code wg_check_include_path(const char *path);
/** @brief ANVL numeric text: -?digits(.digits)?([eE][+-]digits)? */
anvil_writer_err_code wg_check_numeric(const char *text);
/** @brief Text that can be written unquoted and read back as BARE. */
anvil_writer_err_code wg_check_bare(const char *text);
/** @brief Blob tag (NULL/empty for none) and content. */
anvil_writer_err_code wg_check_blob(const char *tag, const char *data, size_t length);
anvil_writer_err_code wg_check_string(const char *text, size_t length);

/** @brief Shortest text that reads back as `value`. False for a non-finite value. */
bool wg_format_double(double value, char out[40]);

/* A set of names, for rejecting a top-level name declared twice (the reader's resolver does). */
typedef struct {
   char **slots; // each name is preceded in memory by a size_t, the value of wg_nameset_put (0 for wg_nameset_add)
   size_t capacity; // power of two, or 0 before the first add
   size_t count;
} wg_nameset;

/** The name set's hash: 64-bit FNV-1a on every target (exposed for the tests; BR-2610-anvl-writer-001). */
uint64_t wg_hash_name(const char *s);

/** @return 1 if added, 0 if already present, -1 on allocation failure. */
int wg_nameset_add(wg_nameset *set, const char *name);
/** As wg_nameset_add, and records `value` (non-zero) with the name when it is added. @return 1 added, 0 already present, -1 on allocation failure. */
int wg_nameset_put(wg_nameset *set, const char *name, size_t value);
/** @return the value recorded for `name` by wg_nameset_put, or 0 if the name is absent. */
size_t wg_nameset_get(const wg_nameset *set, const char *name);
void wg_nameset_free(wg_nameset *set);
