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
 * anvil_writer_types.h - Handle and enums shared by the writer headers   *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * The writer is a separate add-on from the reader: nothing in any        *
 * anvil_writer*.h header includes, or is included by, a reader header    *
 * (anvil_types.h, anvil_flat.h, anvil_vtable.h), and src/writer/ links   *
 * against no reader code. A build may therefore ship the reader alone    *
 * (`make WITH_WRITER=0`) or the writer alone, and the two never share    *
 * an enum or handle. See FR/FR-2609-anvl-writer-001.md.                  *
 * ********************************************************************** */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Opaque handle to a streaming document writer.
 *
 * Not thread-safe: use from one thread, or synchronize externally. Owns the
 * output buffer; dispose with anvil_writer_dispose.
 */
typedef struct anvil_writer_t *anvil_writer;

/** @brief Which dialect a writer emits (and therefore which grammar it enforces). */
typedef enum {
   ANVIL_WRITER_AML = 0, // #!aml - declarative data; the full grammar
   ANVIL_WRITER_AMP,     // #!amp - messaging; scalar-only arrays/tuples, no attributes/objects/base/include
} anvil_writer_dialect;

/**
 * @brief Stable writer error categories - the writer's own set, independent of the
 * reader's anvil_err_code. The first error is sticky: every later call fails with it.
 */
typedef enum {
   ANVIL_WRITER_OK = 0,
   ANVIL_WRITER_ERR_INVALID_ARGUMENT,            // NULL handle/argument, or an unknown enum value
   ANVIL_WRITER_ERR_MEMORY,                      // allocation failed
   ANVIL_WRITER_ERR_STATE,                       // call isn't legal at this point in the document
   ANVIL_WRITER_ERR_DIALECT,                     // construct is forbidden in the writer's dialect (AMP)
   ANVIL_WRITER_ERR_INVALID_IDENTIFIER,          // statement/base/attribute-key/blob-tag/varref name
   ANVIL_WRITER_ERR_RESERVED_WORD,               // an identifier or bare literal is a reserved keyword
   ANVIL_WRITER_ERR_INVALID_NUMERIC,             // text isn't ANVL numeric grammar, or a non-finite double
   ANVIL_WRITER_ERR_INVALID_BARE,                // text can't be emitted unquoted and read back as BARE
   ANVIL_WRITER_ERR_INVALID_BLOB,                // blob content contains the closing backtick
   ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE,     // attribute value can't be represented
   ANVIL_WRITER_ERR_INVALID_INCLUDE_PATH,        // include path is empty or contains a quote/newline
   ANVIL_WRITER_ERR_EMPTY_COLLECTION,            // empty array/object, or a tuple with fewer than 2 elements
   ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT, // a statement with a base must have an object value
   ANVIL_WRITER_ERR_UNFINISHED,                  // finish called with an open statement/array/tuple/object
   ANVIL_WRITER_ERR_DUPLICATE_NAME,              // a top-level name declared twice (the reader's resolver rejects it)
   ANVIL_WRITER_ERR_DEPTH_EXCEEDED,              // nesting deeper than the builder will emit
} anvil_writer_err_code;
