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
#include "internal/module.h"
// -----------------------------------------------------------------
#include <sigma/list.h>
#include <sigma/types.h>
#include <sigma/memory.h>
#include <sigma/strings.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------- *
 * Source structure                                                        *
 * ----------------------------------------------------------------- */
struct anvl_src_t {
   anvl_doc doc; // pointer to the anvl_doc that owns this source
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
   bool (*create)(const char *filepath, anvl_doc *out_doc);
   void (*dispose)(anvl_src src);
   anvl_dialect (*dialect)(anvl_doc doc);
   // Error handling - per source object
   bool (*has_errors)(anvl_doc doc);

   // Position & EOF
   usize (*position)(anvl_doc doc);
   usize (*line)(anvl_doc doc);
   usize (*column)(anvl_doc doc);
   bool (*is_eof)(anvl_doc doc);
   bool (*is_eof_offset)(anvl_doc doc, usize);

   // Character peek
   char (*peek)(anvl_doc doc);
   char (*peek_offset)(anvl_doc doc, usize);

   // String matching (returns length if matches, 0 if not)
   usize (*match_length)(anvl_doc doc, const char *, usize);
   usize (*match_operator)(anvl_doc doc, const char *, usize);

   // Character classification
   bool (*is_alpha)(char);
   bool (*is_digit)(char);
   bool (*is_hex_digit)(char);
   bool (*is_identifier_start)(char);
   bool (*is_identifier_part)(char);

   // Consume
   usize (*consume)(anvl_doc doc, usize);

   // Data access (for scanning without consuming)
   const char *(*data)(anvl_doc doc);
   usize (*length)(anvl_doc doc);

   // Substring extraction (caller-supplied buffer; FR-2603-anvil-002)
   void (*substring)(anvl_doc doc, usize start, usize len, char *out_buf);

   // Whitespace & comments
   usize (*skip_whitespace_and_comments)(anvl_doc doc);

   // Shebang
   bool (*is_shebang)(anvl_doc doc);

   // Dialect parsing
   anvl_dialect (*parse_dialect)(anvl_doc doc, anvl_dialect);

   // Position management
   void (*set_position)(anvl_doc doc, usize, usize, usize);
   void (*reset)(anvl_doc doc);
} anvl_source_i;
extern const anvl_source_i Source;
