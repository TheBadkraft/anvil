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
#include "internal/source_registry.h"
#include "errors.h"
#include "std.h"
// -----------------------------------------------------------------
#include <sigma/memory.h>
#include <string.h>

// FNV-1a 64-bit hash constants
#define FNV1A_OFFSET UINT64_C(14695981039346656037)
#define FNV1A_PRIME UINT64_C(1099511628211)

static ssize_t src_size = sizeof(struct anvl_source_t);

/* Forward declarations */
static void source_parse_shebang(anvl_source);
static bump_allocator source_get_arena(anvl_source, anvl_err_code *);

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
   src->line = 1;
   src->col = 1;

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

   // Determine the dialect from the file extension. The shebang, if present, takes precedence and
   // is parsed inside source_from_buffer.
   anvl_dialect dialect_hint = Files.dialect_hint(filepath);
   if (dialect_hint == ANVL_DIALECT_ERROR) {
      err_code = ANVL_ERR_IO_INVALID_PATH;
      goto error;
   }

   // now call source_from_buffer with the loaded buffer
   res = Source.from_buffer(out_src, buffer, len, &err_code);
   if (res != ANVL_RES_OK) {
      goto error;
   }

   // free the intermediate file buffer now that source has its own copy
   Allocator.dispose((void *)buffer);
   buffer = NULL;

   // Apply the extension hint only when the source did not provide a shebang.
   if (!(*out_src)->has_shebang) {
      (*out_src)->dialect = dialect_hint;
   }

   return ANVL_RES_OK;

error: {
   if (buffer) {
      Allocator.dispose((void *)buffer);
   }
   if (out_err_code) {
      *out_err_code = err_code;
   }
   // we don't want to deallocate the user's source object.
   return ANVL_RES_ERR;
}
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
   (*out_src)->line = 1;
   (*out_src)->col = 1;
   (*out_src)->has_shebang = false;

   // Detect and consume an optional leading shebang. This positions the source at the first
   // header/body token and resolves the dialect before the header scanner runs.
   source_parse_shebang(*out_src);

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

/* ----------------------------------------------------------------------- *
 * Source position and data access
 * ----------------------------------------------------------------------- */
static usize source_position(anvl_source src) { return src ? src->pos : 0; }

static usize source_line(anvl_source src) { return src ? src->line : 0; }

static usize source_column(anvl_source src) { return src ? src->col : 0; }

static bool source_is_eof(anvl_source src) {
   if (!src || !src->buffer.bucket) {
      return true;
   }
   return src->pos >= src->length;
}

static bool source_is_eof_offset(anvl_source src, usize offset) {
   if (!src || !src->buffer.bucket) {
      return true;
   }
   return src->pos + offset >= src->length;
}

static char source_peek_offset(anvl_source src, usize offset) {
   if (!src || !src->buffer.bucket || src->pos + offset >= src->length) {
      return '\0';
   }
   return ((const char *)src->buffer.bucket)[src->pos + offset];
}

static char source_peek(anvl_source src) { return source_peek_offset(src, 0); }

static const char *source_data(anvl_source src) {
   if (!src) {
      return NULL;
   }
   return (const char *)src->buffer.bucket;
}
/*
 * Pointer to the source's current cursor position — Source.data(src) + Source.position(src),
 * computed directly rather than via those two calls.
 */
static const char *source_at(anvl_source src) {
   if (!src || !src->buffer.bucket) {
      return NULL;
   }
   return (const char *)src->buffer.bucket + src->pos;
}

static usize source_length(anvl_source src) {
   if (!src) {
      return 0;
   }
   return src->length;
}

static void source_set_position(anvl_source src, usize pos, usize line, usize col) {
   if (!src) {
      return;
   }
   if (pos > src->length) {
      pos = src->length;
   }
   src->pos = pos;
   src->line = line;
   src->col = col;
}

static void source_reset(anvl_source src) { source_set_position(src, 0, 1, 1); }

/* ----------------------------------------------------------------------- *
 * Character classification
 * ----------------------------------------------------------------------- */
static bool source_is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

static bool source_is_digit(char c) { return c >= '0' && c <= '9'; }

static bool source_is_hex_digit(char c) {
   return source_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool source_is_identifier_start(char c) { return source_is_alpha(c) || c == '_'; }

static bool source_is_identifier_part(char c) {
   return source_is_alpha(c) || source_is_digit(c) || c == '_';
}

/* ----------------------------------------------------------------------- *
 * Consume and matching
 * ----------------------------------------------------------------------- */
static usize source_consume(anvl_source src, usize count) {
   if (!src || !src->buffer.bucket) {
      return 0;
   }

   usize consumed = 0;
   const char *data = (const char *)src->buffer.bucket;
   while (consumed < count && src->pos < src->length) {
      char c = data[src->pos];
      src->pos++;
      consumed++;
      if (c == '\n') {
         src->line++;
         src->col = 1;
      } else {
         src->col++;
      }
   }
   return consumed;
}

/*
 * Parse an optional leading shebang (`#!dialect`) on the source. Leading whitespace is skipped so
 * that indented or padded shebangs are accepted. The dialect token is validated against the known
 * AML/AMP/ASL dialects; an invalid token leaves `src->dialect` set to `ANVL_DIALECT_ERROR` and
 * `src->has_shebang` set so the header scanner can report the error. On success the source position
 * is advanced past the shebang line; if no shebang is present the position is unchanged (aside from
 * any leading whitespace that was skipped).
 */
static void source_parse_shebang(anvl_source src) {
   if (!src || !src->buffer.bucket || src->length < 2) {
      return;
   }

   const char *data = (const char *)src->buffer.bucket;

   // Skip leading whitespace only; leave comments for the header scanner so it can report
   // unterminated-comment errors in the proper document context.
   while (src->pos < src->length) {
      char c = data[src->pos];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
         source_consume(src, 1);
      } else {
         break;
      }
   }

   if (src->pos + 1 >= src->length || data[src->pos] != '#' || data[src->pos + 1] != '!') {
      return;
   }

   src->has_shebang = true;
   source_consume(src, 2); // consume '#!'

   usize dialect_start = src->pos;
   while (src->pos < src->length && data[src->pos] != '\n') {
      source_consume(src, 1);
   }
   usize dialect_len = src->pos - dialect_start;

   anvl_dialect dialect = ANVL_DIALECT_ERROR;
   if (dialect_len == 3 && memcmp(data + dialect_start, "aml", 3) == 0) {
      dialect = ANVL_DIALECT_AML;
   } else if (dialect_len == 3 && memcmp(data + dialect_start, "amp", 3) == 0) {
      dialect = ANVL_DIALECT_AMP;
   } else if (dialect_len == 3 && memcmp(data + dialect_start, "asl", 3) == 0) {
      dialect = ANVL_DIALECT_ASL;
   }

   src->dialect = dialect;

   // Consume the terminating newline if present.
   if (src->pos < src->length && data[src->pos] == '\n') {
      source_consume(src, 1);
   }
}

static usize source_match_length(anvl_source src, const char *s, usize len) {
   if (!src || !s || len == 0) {
      return 0;
   }

   for (usize i = 0; i < len; i++) {
      if (source_peek_offset(src, i) != s[i]) {
         return 0;
      }
   }
   return len;
}

static usize source_match_operator(anvl_source src, const char *op, usize len) {
   return source_match_length(src, op, len);
}

/* ----------------------------------------------------------------------- *
 * Whitespace and comment skipping
 * ----------------------------------------------------------------------- */
static usize source_skip_whitespace_and_comments(anvl_source src) {
   if (!src || !src->buffer.bucket) {
      return 0;
   }

   usize skipped = 0;
   const char *data = (const char *)src->buffer.bucket;

   while (src->pos < src->length) {
      char c = data[src->pos];

      // whitespace
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
         source_consume(src, 1);
         skipped++;
         continue;
      }

      // line comment
      if (c == '/' && src->pos + 1 < src->length && data[src->pos + 1] == '/') {
         while (src->pos < src->length && data[src->pos] != '\n') {
            source_consume(src, 1);
            skipped++;
         }
         continue;
      }

      // block comment
      if (c == '/' && src->pos + 1 < src->length && data[src->pos + 1] == '*') {
         source_consume(src, 2);
         skipped += 2;
         while (src->pos < src->length) {
            if (data[src->pos] == '*' && src->pos + 1 < src->length && data[src->pos + 1] == '/') {
               source_consume(src, 2);
               skipped += 2;
               break;
            }
            source_consume(src, 1);
            skipped++;
         }
         continue;
      }

      break;
   }

   return skipped;
}

static bool source_is_shebang(anvl_source src) {
   if (!src || !src->buffer.bucket) {
      return false;
   }
   return src->has_shebang;
}

/*
 * Returns true if the source's owning document has accumulated errors.
 * Uses the global source registry to locate the owning document.
 */
static bool source_has_errors(anvl_source src) {
   if (!src || src->hash == 0) {
      return false;
   }

   module_document doc = Registry.find(src->hash);
   if (!doc || !doc->context) {
      return false;
   }

   return List.size(doc->context->errors) > 0;
}
/*
 * Records an error on the source's owning document by looking it up in the
 * global source registry and appending to the context's error list.
 */
static anvl_result source_set_error(anvl_source src, anvl_err_code code, usize line, usize column,
                                    const char *file, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!src || src->hash == 0) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   module_document doc = Registry.find(src->hash);
   if (!doc || !doc->context) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   return anvl_error_set(doc->context->errors, code, line, column, file, out_err_code);

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}
/*
 * Retrieves the bump allocator (arena) associated with the source's owning document. Uses the
 * global source registry to locate the document and its context. Returns NULL if the source is
 * NULL, if the source is not registered, or if the context's arena is not initialized. The output
 * error code is set to ANVL_ERR_NONE on success, or to an appropriate error code on failure. The
 * caller is responsible for checking the error code to determine the reason for failure.
 */
static bump_allocator source_get_arena(anvl_source src, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   // 1. guard source
   if (!src || src->hash == 0) {
      err_code = ANVL_ERR_SOURCE_NOT_FOUND;
      goto error;
   }
   // 2. guard registry lookup
   module_document doc = Registry.find(src->hash);
   if (!doc || !doc->context) {
      err_code = ANVL_ERR_CONTEXT_INVALID;
      goto error;
   }
   // 3. guard context arena
   if (!doc->context->arena) {
      err_code = ANVL_ERR_ARENA_NOT_INITIALIZED;
      goto error;
   }

   // 4. return the context's arena
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return doc->context->arena;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }

   return NULL;
}
/*
 * Constructs a new node (statement or value) in the source's owning document's context. Uses the
 * global source registry to locate the document and its context. Allocates the node from the
 * context's bump allocator (arena). Returns a pointer to the new node on success, or NULL on
 * failure. The output error code is set to ANVL_ERR_NONE on success, or to an appropriate error
 * code on failure. The caller is responsible for checking the error code to determine the reason
 * for failure. The node is automatically appended to the context's statement or value list based on
 * the kind. The caller is responsible for initializing the node's fields after allocation. The
 * node's source reference is cached in the node itself for later retrieval. The node's memory is
 * managed by the context's arena and will be freed when the context is disposed. The caller should
 * not attempt to free the node manually.
 */
static void *source_new_node(anvl_source src, anvl_node_kind kind, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   if (!src || src->hash == 0) {
      err_code = ANVL_ERR_SOURCE_NOT_FOUND;
      goto error;
   }

   // get the document from the source lookup
   module_document doc = Registry.find(src->hash);
   module_context context = doc ? doc->context : NULL;
   if (!doc || !context) {
      err_code = ANVL_ERR_CONTEXT_INVALID;
      goto error;
   }

   bump_allocator arena = context->arena;
   if (!arena) {
      err_code = ANVL_ERR_ARENA_NOT_INITIALIZED;
      goto error;
   }

   // allocate a new node from the arena based on the kind
   usize size = 0;
   switch (kind) {
   case ANVL_NODE_STATEMENT:
      size = sizeof(anvl_statement_t);
      break;
   case ANVL_NODE_VALUE:
      size = sizeof(anvl_value_t);
      break;
   default:
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   // allocate from the arena
   void *node = arena->alloc(arena, size);
   if (!node) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   // 1. cache the node's source reference
   // 2. append the new node to the appropriate index list
   switch (kind) {
   case ANVL_NODE_STATEMENT:

      List.append(context->statements, node);
      break;
   case ANVL_NODE_VALUE:
      List.append(context->values, node);
      break;
   default:
      break; // unreachable — already guarded above
   }

   if (out_err_code) {
      *out_err_code = err_code;
   }
   return node;

error: {
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return NULL;
}
}
/*
 * Simple slice initializer. Fills in the slice's `data` field with the source's buffer base.
 */
static void source_init_slice(anvl_source src, anvl_slice *out_slice) {
   if (!out_slice) {
      return;
   }
   out_slice->data = Source.data(src);
}
/*
 * Freezes a parser's finished top-level statement list into the owning document's
 * body as a farray, then disposes `statements`. Takes ownership of `statements`
 * regardless of outcome — see the doc comment on anvl_source_i.finish_body.
 */
static anvl_result source_finish_body(anvl_source src, list statements,
                                      anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (!src || src->hash == 0) {
      err_code = ANVL_ERR_SOURCE_NOT_FOUND;
      goto error;
   }

   module_document doc = Registry.find(src->hash);
   if (!doc || !doc->context) {
      err_code = ANVL_ERR_CONTEXT_INVALID;
      goto error;
   }

   usize count = List.size(statements);
   farray arr = FArray.new(count, sizeof(anvl_statement));
   if (!arr) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   for (usize i = 0; i < count; i++) {
      anvl_statement stmt = NULL;
      List.get(statements, i, (object *)&stmt);
      FArray.set(arr, i, sizeof(anvl_statement), &stmt);
   }

   doc->body = arr;

   List.dispose(statements);
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_OK;

error:
   List.dispose(statements);
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}
/* ----------------------------------------------------------------------- *
 * Slice interrogation
 * ----------------------------------------------------------------------- */
/*
 * Get slice length
 */
static usize source_slice_length(anvl_slice slice) {
   if (!slice.data || !slice.start || !slice.end || slice.start > slice.end) {
      return 0;
   }
   return (usize)(slice.end - slice.start);
}
/*
 * Check if slice is empty
 */
static bool source_slice_is_empty(anvl_slice slice) {
   if (!slice.data || !slice.start || !slice.end || slice.start > slice.end) {
      return true;
   }
   return slice.start == slice.end;
}
/*
 * Get source sub-string from slice
 */
static usize source_substring(anvl_slice slice, char *out_buffer) {
   usize len = source_slice_length(slice);
   if (!slice.data || len == 0 || !out_buffer) {
      return len;
   }

   // copy slice to out_buffer
   memcpy(out_buffer, slice.start, len);
   out_buffer[len] = '\0';

   return len;
}

const anvl_source_i Source = {
   // Management
   .create = source_create,
   .from_file = source_from_file,
   .from_buffer = source_from_buffer,
   .dispose = source_dispose,

   // Properties
   .dialect = source_dialect,
   .hash = source_hash,

   // Extensions methods
   .has_errors = source_has_errors,
   .set_error = source_set_error,
   .get_arena = source_get_arena,
   .new_node = source_new_node,
   .init_slice = source_init_slice,
   .finish_body = source_finish_body,

   // Position management
   .position = source_position,
   .line = source_line,
   .column = source_column,
   .is_eof = source_is_eof,
   .is_eof_offset = source_is_eof_offset,

   // Scanning
   .peek = source_peek,
   .peek_offset = source_peek_offset,
   .match_length = source_match_length,
   .match_operator = source_match_operator,
   .is_alpha = source_is_alpha,
   .is_digit = source_is_digit,
   .is_hex_digit = source_is_hex_digit,
   .is_identifier_start = source_is_identifier_start,
   .is_identifier_part = source_is_identifier_part,
   .consume = source_consume,
   .data = source_data,
   .at = source_at,
   .length = source_length,
   .set_position = source_set_position,
   .reset = source_reset,
   .skip_whitespace_and_comments = source_skip_whitespace_and_comments,
   .is_shebang = source_is_shebang,

   // Slice interrogation
   .slice_length = source_slice_length,
   .slice_is_empty = source_slice_is_empty,
   .substring = source_substring,
};
