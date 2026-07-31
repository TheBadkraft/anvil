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
 * source.c - Minimal Source implementation (non-deprecated path)          *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-30                                                     *
 * File: src/core/source.c                                                 *
 * *********************************************************************** */

#include "internal/source.h"
#include "std.h"
// -----------------------------------------------------------------
#include <sigma/memory.h>

static ssize_t src_size = sizeof(struct anvl_source_t);

static anvl_result source_create(const char *filepath, anvl_source *out_src,
                                 anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_source src = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!out_src || !out_err_code) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   *out_src = NULL;

   if (!filepath) {
      err_code = ANVL_ERR_IO_INVALID_PATH;
      goto error;
   }

   src = Allocator.alloc(src_size);
   if (!src) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(src, 0, src_size);

   src->err_code = ANVL_ERR_NONE;
   src->dialect = ANVL_DIALECT_ASL;

   *out_src = src;
   return ANVL_RES_OK;

error:
   Allocator.dispose(src);
   src = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (out_src) {
      *out_src = src;
   }
   return ANVL_RES_ERR;
}
static void source_dispose(anvl_source src) {
   if (!src) {
      return;
   }

   Allocator.dispose(src);
}

const anvl_source_i Source = {
   .create = source_create,
   .dispose = source_dispose,
};
