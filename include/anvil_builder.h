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
 * anvil_builder.h - Public ABI: flat document builder for Anvil Native   *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Build a document as a tree, in any order, then emit it. Where the      *
 * streaming writer (anvil_writer.h) is forward-only, the builder lets    *
 * you create values first and place them afterwards, add to a document   *
 * or object incrementally, and re-emit after changing it.                *
 *                                                                        *
 *    b = anvil_builder_new(ANVIL_WRITER_AML);                            *
 *    anvil_node server = anvil_builder_object(b);                        *
 *    anvil_builder_add(b, server, "host", anvil_builder_bare(b, "h"));   *
 *    anvil_member m = anvil_builder_add(b, NULL, "server", server);      *
 *    anvil_builder_member_attribute(b, m, "env", "production");          *
 *    const char *text = anvil_builder_emit(b, &len);                     *
 *    anvil_builder_dispose(b);                                           *
 *                                                                        *
 * Emission goes through the streaming writer, so the output and the      *
 * grammar rules are exactly the writer's. A mistake that depends only on *
 * the call itself (bad name, bad numeric, an AMP-forbidden construct, a  *
 * node attached twice or into a cycle, a repeated top-level name) fails  *
 * at that call. What can only be known once the tree is complete (empty  *
 * arrays/objects, 1-tuples, nesting past the limit) fails at emit.       *
 *                                                                        *
 * Unlike the writer, errors are NOT sticky: a failed call changes        *
 * nothing, and anvil_builder_get_error reports the outcome of the most   *
 * recent call (ANVIL_WRITER_OK if it succeeded). Constructors return     *
 * NULL on failure. Error codes are the writer's (anvil_writer_types.h).  *
 * ********************************************************************** */
#pragma once

#include "anvil_builder_types.h"
#include "anvil_writer_types.h"

/** @brief Deepest array/tuple/object nesting emit will write (ANVIL_WRITER_ERR_DEPTH_EXCEEDED beyond). */
#define ANVIL_BUILDER_MAX_DEPTH 256

anvil_builder anvil_builder_new(anvil_writer_dialect dialect);
/** @brief Dispose the builder and everything made through it. Safe on NULL. */
void anvil_builder_dispose(anvil_builder b);
/** @brief Outcome of the most recent call on `b`. A NULL handle reports ANVIL_WRITER_ERR_INVALID_ARGUMENT. */
anvil_writer_err_code anvil_builder_get_error(anvil_builder b);

/* ---- header (AML only) --------------------------------------------------- */

/** @brief Module attribute; `value` is raw text or NULL for a flag. Same rules as anvil_writer_attribute. */
bool anvil_builder_attribute(anvil_builder b, const char *key, const char *value);
bool anvil_builder_attribute_string(anvil_builder b, const char *key, const char *text, size_t length);
bool anvil_builder_include(anvil_builder b, const char *path);

/* ---- values: created detached, owned by the builder ---------------------- */

anvil_node anvil_builder_null(anvil_builder b);
anvil_node anvil_builder_bool(anvil_builder b, bool value);
anvil_node anvil_builder_numeric(anvil_builder b, const char *text);
anvil_node anvil_builder_int(anvil_builder b, int64_t value);
anvil_node anvil_builder_double(anvil_builder b, double value);
/** @brief The text is copied. */
anvil_node anvil_builder_string(anvil_builder b, const char *text, size_t length);
anvil_node anvil_builder_bare(anvil_builder b, const char *text);
/** @brief `tag` NULL/empty for an untagged blob. The tag and data are copied. */
anvil_node anvil_builder_blob(anvil_builder b, const char *tag, const char *data, size_t length);
/** @brief AML only. */
anvil_node anvil_builder_varref(anvil_builder b, const char *name);
anvil_node anvil_builder_array(anvil_builder b);
anvil_node anvil_builder_tuple(anvil_builder b);
/** @brief AML only. */
anvil_node anvil_builder_object(anvil_builder b);

/* ---- structure ----------------------------------------------------------- */

/**
 * @brief Append `element` to an array or tuple. The element must be detached (never attached
 * before) and must not be `collection` or one of its ancestors. In AMP a collection element
 * must be a scalar.
 */
bool anvil_builder_append(anvil_builder b, anvil_node collection, anvil_node element);

/**
 * @brief Add a statement `name := value;` to an object, or to the document when `object` is NULL.
 * `value` must be detached. A top-level name may only be added once (ANVIL_WRITER_ERR_DUPLICATE_NAME);
 * names inside objects may repeat. Statements are emitted in the order added.
 * @return The statement handle (for a base or attributes), or NULL on failure.
 */
anvil_member anvil_builder_add(anvil_builder b, anvil_node object, const char *name, anvil_node value);

/** @brief Declare an inheritance base. The statement's value must be an object. AML only. */
bool anvil_builder_set_base(anvil_builder b, anvil_member member, const char *base);
/** @brief Statement attribute; same rules as anvil_writer_attribute. AML only. */
bool anvil_builder_member_attribute(anvil_builder b, anvil_member member, const char *key, const char *value);
bool anvil_builder_member_attribute_string(anvil_builder b, anvil_member member, const char *key,
                                           const char *text, size_t length);

/** @brief Find a statement by name in an object (or the document if NULL); first match, or NULL. */
anvil_member anvil_builder_find(anvil_builder b, anvil_node object, const char *name);

/* ---- output -------------------------------------------------------------- */

/**
 * @brief Stream the whole document into `w`: header, then every statement. `w` must be a fresh
 * writer of the same dialect; it is not finished - that, and reading its data, is the caller's.
 * On failure the writer's error is also reported by anvil_builder_get_error.
 */
bool anvil_builder_write(anvil_builder b, anvil_writer w);

/**
 * @brief Emit the document as text, NUL-terminated (the NUL is not counted in `*length`).
 * Borrowed: valid until the next emit or anvil_builder_dispose. NULL on error. May be called
 * again after changing the builder.
 */
const char *anvil_builder_emit(anvil_builder b, size_t *length);
