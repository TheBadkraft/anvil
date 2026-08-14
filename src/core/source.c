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

#include "internal/files.h"
#include "internal/source.h"
#include "std.h"
// -----------------------------------------------------------------
#include <sigma/memory.h>

// FNV-1a 64-bit hash constants
#define FNV1A_OFFSET UINT64_C(14695981039346656037)
#define FNV1A_PRIME UINT64_C(1099511628211)

static ssize_t src_size = sizeof(struct anvl_source_t);

static uint64_t source_compute_hash(const char *data, usize len) {
   uint64_t hash = FNV1A_OFFSET;
   for (usize i = 0; i < len; i++) {
      hash ^= (uint8_t)data[i];
      hash *= FNV1A_PRIME;
   }
   return hash;
}

static anvl_result source_create(anvl_source *out_src, anvl_err_code *out_err_code) {
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

   src = Allocator.alloc(src_size);
   if (!src) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(src, 0, src_size);

   /*
      I don't know why we would have `err_code` in the source struct.
      For now, we will just set the dialect to a default value.
    */
   // src->err_code = ANVL_ERR_NONE;
   src->dialect = ANVL_DIALECT_AML; // default dialect after refactor

   *out_src = src;
   return ANVL_RES_OK;

error: {
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
}
static void source_dispose(anvl_source src) {
   if (!src) {
      return;
   }

   Allocator.dispose(src->buffer.bucket);
   src->buffer.bucket = NULL;
   src->buffer.end = NULL;
   Allocator.dispose(src);
}
/*
 * Loads source content from a file into the source object. The file is read into a buffer, which is
 * then copied into a new allocated buffer within the source object. The source object's dialect is
 * determined from the file extension using the Files.dialect_hint function. Returns ANVL_RES_OK
 * on success, or ANVL_RES_ERR on failure. If the source object is NULL, or if the file cannot be
 * read, an error code is returned. The output error code is set to ANVL_ERR_NONE on success, or to
 * an appropriate error code on failure. The source object's hash is computed using the FNV-1a
 * 64-bit algorithm.
 */
static anvl_result source_from_file(anvl_source *out_src, const char *filepath,
                                    anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = ANVL_RES_OK;

   // use Files.load to read the file into a buffer, then call source_from_buffer
   size_t len = 0;
   const char *buffer = NULL;
   res = Files.load(filepath, &buffer, &len, &err_code);
   if (res != ANVL_RES_OK) {
      goto error;
   }

   // now call source_from_buffer with the loaded buffer
   res = Source.from_buffer(out_src, buffer, len, &err_code);
   if (res != ANVL_RES_OK) {
      goto error;
   }

   // now get the dialect hint from the file extension and set it in the source object
   anvl_dialect dialect_hint = Files.dialect_hint(filepath);
   if (dialect_hint == ANVL_DIALECT_ERROR) {
      err_code = ANVL_ERR_IO_INVALID_PATH;
      goto error;
   }
   (*out_src)->dialect = dialect_hint;

   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   // we don't want to deallocate the user's source object.
   return ANVL_RES_ERR;
}
/*
 * Loads source content from a memory buffer into the source object. The buffer is copied into a new
 * allocated buffer within the source object. The source object's dialect is set to ANVL_DIALECT_AML
 * by default. Returns ANVL_RES_OK on success, or ANVL_RES_ERR on failure. If the source object is
 * NULL, or if the buffer is NULL, an error code is returned. If len is 0, then the buffer is
 * considered empty. The output error code is set to ANVL_ERR_NONE on success, or to an appropriate
 * error code on failure. The source object's hash is computed using the FNV-1a 64-bit algorithm.
 */
static anvl_result source_from_buffer(anvl_source *out_src, const char *buffer, usize len,
                                      anvl_err_code *out_err_code) {
   // copy buffer content into a new allocated buffer in the source object
   anvl_err_code err_code = ANVL_ERR_NONE;
   // allocate a new buffer for the source object
   void *bucket = NULL;
   void *end = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!out_src || !*out_src) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   if (!buffer) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   bucket = Allocator.alloc(len + 1); // +1 for null terminator
   if (!bucket) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   memcpy(bucket, buffer, len);
   ((char *)bucket)[len] = '\0';
   end = (char *)bucket + len;

   (*out_src)->buffer.bucket = bucket;
   (*out_src)->buffer.end = end;
   (*out_src)->length = len;
   (*out_src)->stride = 1;                 // byte buffer stride
   (*out_src)->dialect = ANVL_DIALECT_AML; // default dialect after refactor
   (*out_src)->hash = source_compute_hash(bucket, len);

   // atomically update the output source pointer and error code
   if (out_err_code) {
      *out_err_code = err_code;
   }

   return ANVL_RES_OK;

error:
   if (bucket) {
      Allocator.dispose(bucket); // Prevent leak if alloc succeeded but function failed
   }
   if (out_err_code) {
      *out_err_code = err_code;
   }
   // we don't want to deallocate the user's source object.
   return ANVL_RES_ERR;
}
/*
 * Retrieves the dialect of the source object. Returns ANVL_DIALECT_ERROR if the source is NULL.
 */
static anvl_dialect source_dialect(anvl_source src) {
   if (!src) {
      return ANVL_DIALECT_ERROR;
   }

   return src->dialect;
}
/*
 * Retrieves the FNV-1a 64-bit hash of the source content. Returns 0 if the source is NULL or if no
 * content has been loaded.
 */
static uint64_t source_hash(anvl_source src) {
   if (!src) {
      return 0;
   }

   return src->hash;
}

const anvl_source_i Source = {
   .create = source_create,
   .from_file = source_from_file,
   .from_buffer = source_from_buffer,
   .dispose = source_dispose,
   .dialect = source_dialect,
   .hash = source_hash,
};
