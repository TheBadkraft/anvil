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
   anvl_err_code err_code;
   // farray-compatible buffer structure
   struct {
      void *bucket; // pointer to first element (raw bytes)
      void *end;    // one past allocated memory
   } buffer;
   anvl_dialect dialect;
   usize stride; // size of each element in bytes (1 for byte arrays)
   usize pos;
   usize line;
   usize col;
};

/* ----------------------------------------------------------------- *
 * AnvlDoc->Source Interface - for interrogating source content            *
 * ----------------------------------------------------------------- */
typedef struct anvl_source_i {
   /**
    * @brief Create a source object from input text.
    * @param[in] filepath Input source file path.
    * @param[out] out_src Receives the created source object on success.
    * @param[out] out_err_code Receives ANVL_ERR_NONE on success, otherwise a failure code.
    * @return Anvl result indicating success or failure.
    */
   anvl_result (*create)(const char *, anvl_source *, anvl_err_code *);
   void (*dispose)(anvl_source);
   anvl_dialect (*dialect)(anvl_source);
   // Error handling - per source object
   bool (*has_errors)(anvl_source);

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
