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
 * internal/source.h - Source API for Anvil                                *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-13                                                     *
 * File: include/internal/source.h                                         *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * This header defines the source API for Anvil.                           *
 * *********************************************************************** */
#pragma once

#include "anvil.h"
#include "std.h"
#include "internal/module.h"
// -----------------------------------------------------------------
#include <sigma/list.h>
#include <sigma/types.h>
#include <sigma/memory.h>
#include <sigma/strings.h>

/* ----------------------------------------------------------------- *
 * Source structure                                                        *
 * ----------------------------------------------------------------- */
struct anvl_source_t {
   // anvl_err_code err_code; (why would we have this here ... ???)

   // farray-compatible buffer structure
   struct {
      void *bucket; // pointer to first element (raw bytes)
      void *end;    // one past allocated memory
   } buffer;

   // metadata for source content
   usize length; // length of the buffer in bytes
   anvl_dialect dialect;
   usize stride; // size of each element in bytes (1 for byte arrays)
   usize pos;
   usize line;
   usize col;
   uint64_t hash; // FNV-1a 64-bit content hash; 0 = no content loaded
};

/* ----------------------------------------------------------------- *
 * AnvlDoc->Source Interface - for interrogating source content            *
 * ----------------------------------------------------------------- */
typedef struct anvl_source_i {
   /**
    * @brief Create an empty source object.
    * @param[out] out_src Receives the created source object on success.
    * @param[out] out_err_code Receives ANVL_ERR_NONE on success, otherwise a failure code.
    * @return Anvil result indicating success or failure.
    */
   anvl_result (*create)(anvl_source *, anvl_err_code *);
   /**
    * @brief Create a source object from a file path.
    * @param[in] filepath Input source file path.
    * @param[out] out_src Receives the created source object on success.
    * @param[out] out_err_code Receives ANVL_ERR_NONE on success, otherwise a failure code.
    * @return Anvil result indicating success or failure.
    */
   anvl_result (*from_file)(anvl_source *, const char *, anvl_err_code *);
   /**
    * @brief Create a source object from a memory buffer.
    * @param[in] buffer Input source buffer.
    * @param[in] len Length of the input buffer.
    * @param[out] out_src Receives the created source object on success.
    * @param[out] out_err_code Receives ANVL_ERR_NONE on success, otherwise a failure code.
    * @return Anvil result indicating success or failure.
    */
   anvl_result (*from_buffer)(anvl_source *, const char *, size_t, anvl_err_code *);
   /**
    * @brief Dispose of a source object, releasing all associated resources.
    * @param src The source object to dispose.
    * @details This function disposes of the specified source object, releasing its buffer and any
    * other associated resources. After calling this function, the source object should not be used.
    */
   void (*dispose)(anvl_source);
   /**
    * @brief Get the source dialect (AMP, AML, ASL, or error) for the given source object.
    * @param src The source object to query.
    * @return The dialect of the source object.
    * @details This function retrieves the dialect of the specified source object. The dialect can
    * be one of the following: ANVL_DIALECT_AMP, ANVL_DIALECT_AML, ANVL_DIALECT_ASL, or
    * ANVL_DIALECT_ERROR.
    */
   anvl_dialect (*dialect)(anvl_source);
   /**
    * @brief Get the FNV-1a 64-bit content hash for the given source object.
    * @param src The source object to query.
    * @return The content hash, or 0 if no content has been loaded.
    */
   uint64_t (*hash)(anvl_source);

   // Error handling - per source object
   bool (*has_errors)(anvl_source);
   /**
    * @brief Record a parser/scanner error against the source's owning document.
    * @param src The source object where the error occurred.
    * @param code The Anvil error code.
    * @param line The 1-based line number of the error.
    * @param column The 1-based column number of the error.
    * @param file The file path associated with the error (may be NULL).
    * @param[out] out_err_code Receives a failure code if the error cannot be recorded.
    * @return `ANVL_RES_OK` on success; otherwise `ANVL_RES_ERR`.
    * @details Looks up the source in the global registry via its content hash and
    * appends the error to the owning document's context error list.
    */
   anvl_result (*set_error)(anvl_source, anvl_err_code, usize, usize, const char *,
                            anvl_err_code *);

   // Position & EOF
   usize (*position)(anvl_source);
   usize (*line)(anvl_source);
   usize (*column)(anvl_source);
   bool (*is_eof)(anvl_source);
   bool (*is_eof_offset)(anvl_source, usize);

   // Character peek
   char (*peek)(anvl_source);
   char (*peek_offset)(anvl_source, usize);

   // String matching (returns length if matches, 0 if not)
   usize (*match_length)(anvl_source, const char *, usize);
   usize (*match_operator)(anvl_source, const char *, usize);

   // Character classification
   bool (*is_alpha)(char);
   bool (*is_digit)(char);
   bool (*is_hex_digit)(char);
   bool (*is_identifier_start)(char);
   bool (*is_identifier_part)(char);

   // Consume
   usize (*consume)(anvl_source, usize);

   // Data access (for scanning without consuming)
   const char *(*data)(anvl_source);
   usize (*length)(anvl_source);

   // Substring extraction (caller-supplied buffer; FR-2603-anvil-002)
   void (*substring)(anvl_source, usize, usize, char *);

   // Whitespace & comments
   usize (*skip_whitespace_and_comments)(anvl_source);

   // Shebang
   bool (*is_shebang)(anvl_source);

   // Dialect parsing
   anvl_dialect (*parse_dialect)(anvl_source, anvl_dialect);

   // Position management
   void (*set_position)(anvl_source, usize, usize, usize);
   void (*reset)(anvl_source);
} anvl_source_i;
extern const anvl_source_i Source;
