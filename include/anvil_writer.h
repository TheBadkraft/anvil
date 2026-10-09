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
 * anvil_writer.h - Public ABI: flat streaming writer for Anvil Native    *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * A forward-only, event-style writer that emits canonical ANVL text.     *
 * Every call validates against the dialect's real grammar, so what the   *
 * writer emits is always something the reader parses back to the same    *
 * kinds and text (round-trip is tested in test/unit/test_writer_roundtrip.c).*
 * Output is canonical, not layout-preserving: 3-space indentation, one   *
 * `name := value;` statement per line, no comments.                      *
 *                                                                        *
 * Call shape (AML):                                                      *
 *    w = anvil_writer_new(ANVIL_WRITER_AML);                             *
 *    anvil_writer_attribute(w, "doc", NULL);       // module attribute   *
 *    anvil_writer_include(w, "base.anvl");                               *
 *    anvil_writer_statement(w, "server", NULL);    // name [, base]      *
 *    anvil_writer_attribute(w, "env", "production");// statement attr    *
 *    anvil_writer_begin_object(w);                                       *
 *       anvil_writer_statement(w, "host", NULL);                         *
 *       anvil_writer_bare(w, "localhost");                               *
 *    anvil_writer_end_object(w);                                         *
 *    anvil_writer_finish(w);                                             *
 *    const char *text = anvil_writer_data(w, &len);                      *
 *    anvil_writer_dispose(w);                                            *
 *                                                                        *
 * A statement is completed by exactly one value call (a scalar, or a     *
 * begin_X and end_X pair). Inside an array/tuple, value calls add elements   *
 * and begin_object adds an anonymous object. Inside an object, only      *
 * anvil_writer_statement is legal.                                       *
 *                                                                        *
 * Every function returns false once the writer has errored (the first    *
 * error is sticky - see anvil_writer_get_error). Output after an error   *
 * is undefined and anvil_writer_data returns NULL.                       *
 * ********************************************************************** */
#pragma once

#include "anvil_writer_types.h"

/** @brief Create a writer for a dialect; its shebang line is emitted immediately. NULL on failure. */
anvil_writer anvil_writer_new(anvil_writer_dialect dialect);

/** @brief Dispose a writer and its buffer. Safe on NULL. */
void anvil_writer_dispose(anvil_writer w);

/** @brief First error recorded, or ANVIL_WRITER_OK. A NULL handle reports ANVIL_WRITER_ERR_INVALID_ARGUMENT. */
anvil_writer_err_code anvil_writer_get_error(anvil_writer w);

/** @brief Static, human-readable name for an error code. Never NULL. */
const char *anvil_writer_error_message(anvil_writer_err_code code);

/* ---- header (AML only; before the first statement) --------------------- */

/**
 * @brief Add an attribute. Before the first statement this is a module-level attribute
 * (`@[key=value]` on its own line); right after anvil_writer_statement it is a statement-level
 * attribute (grouped into one `@[...]`). AML only.
 * @param key Identifier. @param value Raw attribute text, or NULL for a flag attribute. It is
 * emitted verbatim, so it must have no `,` or `]` outside double quotes, no leading `$`, and no
 * leading/trailing whitespace; use anvil_writer_attribute_string for text that needs quoting.
 */
bool anvil_writer_attribute(anvil_writer w, const char *key, const char *value);

/** @brief As anvil_writer_attribute, but the value is emitted as a double-quoted string. `"` in text is rejected. */
bool anvil_writer_attribute_string(anvil_writer w, const char *key, const char *text, size_t length);

/** @brief Add `include "path";` (module header, AML only). */
bool anvil_writer_include(anvil_writer w, const char *path);

/* ---- statements -------------------------------------------------------- */

/**
 * @brief Begin a statement at the top level or inside an object. Always emitted as
 * `name := value;`. Complete it with one value call.
 * A top-level name may only be declared once (the reader's resolver rejects a repeat), so a repeat
 * fails with ANVIL_WRITER_ERR_DUPLICATE_NAME; names inside objects may repeat. The writer can't see
 * names pulled in by an include.
 * @param name Identifier, not a reserved word. @param base Optional inheritance base
 * (identifier, not reserved), or NULL. A base requires an object value; AML only.
 */
bool anvil_writer_statement(anvil_writer w, const char *name, const char *base);

/* ---- scalar values ----------------------------------------------------- */

bool anvil_writer_null(anvil_writer w);
bool anvil_writer_bool(anvil_writer w, bool value);

/** @brief Numeric from ANVL numeric text: `-?digits(.digits)?([eE][+-]digits)?`. */
bool anvil_writer_numeric(anvil_writer w, const char *text);
bool anvil_writer_int(anvil_writer w, int64_t value);
/** @brief Shortest text that reads back to the same double. Non-finite values are rejected. */
bool anvil_writer_double(anvil_writer w, double value);

/** @brief Quoted string; `"`, `\`, newline, tab and carriage return are escaped. */
bool anvil_writer_string(anvil_writer w, const char *text, size_t length);

/**
 * @brief Unquoted literal. Rejected unless it reads back as BARE: first char alpha, `_`, `.`, `/`
 * or digit; the rest alnum or one of `_-./:$`; not a reserved word; not a complete numeric;
 * not starting with a comment opener (two slashes, or slash-star).
 */
bool anvil_writer_bare(anvil_writer w, const char *text);

/** @brief Blob `@tag`content``. `tag` is NULL/empty for an untagged blob, else an identifier of at most 31 chars. */
bool anvil_writer_blob(anvil_writer w, const char *tag, const char *data, size_t length);

/** @brief `$name` reference (AML only). Statement values and collection elements; never attributes. */
bool anvil_writer_varref(anvil_writer w, const char *name);

/* ---- collections ------------------------------------------------------- */

bool anvil_writer_begin_array(anvil_writer w);
bool anvil_writer_end_array(anvil_writer w);   // at least 1 element
bool anvil_writer_begin_tuple(anvil_writer w);
bool anvil_writer_end_tuple(anvil_writer w);   // at least 2 elements
bool anvil_writer_begin_object(anvil_writer w); // AML only
bool anvil_writer_end_object(anvil_writer w);   // at least 1 statement

/* ---- finishing --------------------------------------------------------- */

/** @brief Close the document. Fails with ANVIL_WRITER_ERR_UNFINISHED if anything is still open. */
bool anvil_writer_finish(anvil_writer w);

/**
 * @brief The emitted text, NUL-terminated (the NUL is not counted in `*length`). Only available
 * after a successful anvil_writer_finish; NULL otherwise. Borrowed: valid until the writer is disposed.
 */
const char *anvil_writer_data(anvil_writer w, size_t *length);
