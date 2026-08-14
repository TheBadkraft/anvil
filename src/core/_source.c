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
 * source.c - Implementation of Anvil source                               *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-13                                                     *
 * File: src/core/source.c                                                 *
 * *********************************************************************** */

// LEGACY (2026-08-09): pre-refactor implementation kept for comparison.
// Do not compile. Active implementation is in src/core/source.c.

#include "internal/source.h"
#include "internal/module.h"
#include "utils.h"

/* *********************************************************************** *
 * Source forward declarations                                             *
 * *********************************************************************** */
// (??) bool source_is_empty(anvl_src);
static bool source_is_shebang(anvl_src);
static usize source_scan_match(anvl_src, const char *, usize);
static void source_reset(anvl_src);
static usize source_skip_whitespace_and_comments(anvl_src);
static anvl_dialect source_parse_dialect(anvl_src, anvl_dialect);
static usize source_consume(anvl_src, usize);
static usize source_length(anvl_src self);
static void source_substring(anvl_src, usize, usize, char *);
static usize source_scan_whitespace(anvl_src, usize);
static usize source_scan_line_comment(anvl_src, usize);
static usize source_scan_block_comment(anvl_src, usize);

bool source_validate(anvl_src src) {
   bool is_valid = src && (source_length(src) > 0) && src->buffer && src->doc;
   anvl_error_code err_code;

   if (is_valid) {
      anvl_doc doc = src->doc;

      // leading whitespace and comments are valid
      usize skipped = source_skip_whitespace_and_comments(src);
      if (!doc_has_errors(doc)) {
         source_consume(src, skipped);
      }
      if (source_is_shebang(src)) {
         src->dialect = source_parse_dialect(src, src->dialect);
         skipped = source_skip_whitespace_and_comments(src);
         if (!doc_has_errors(doc)) {
            source_consume(src, skipped);
         }
         // detect duplicate shebang (actually, any further '#!' is a syntax error)
         // we should be able to remove this check
         if (source_is_shebang(src)) {
            err_code = ANVL_ERR_PARSER_MULTIPLE_SHEBANG;
            free(src->buffer.bucket);
            Allocator.dispose(src);
            is_valid = false;
            goto error;
         }
      }
      skipped = source_skip_whitespace_and_comments(src);
      if (!doc_has_errors(doc)) {
         source_consume(src, skipped);
      }
   }

   return is_valid;

error:
   doc_set_error(doc, err_code, src->line, src->col, __FILE__);
   return is_valid;
}

static bool source_create(anvl_doc *doc, const char *data, usize len) {
   len = data ? String.length((string)data) : 0;
   bool success = len > 0;
   if (!success)
      goto error;

   ssize_t src_size = sizeof(struct anvl_src_t);
   anvl_src src = Allocator.alloc(src_size);
   if (!src) {
      success = false;
      goto error;
   }

   memset(src, 0, src_size);
   if (data && len > 0) {
      char *buffer = malloc(len + 1);
      if (!buffer) {
         Allocator.dispose(src);
         success = false;
         goto error;
      }
      // fill buffer with data and null-terminate
      memcpy(buffer, data, len);
      buffer[len] = '\0';
      // set buffer and end pointers in source
      src->buffer.bucket = buffer;
      src->buffer.end = buffer + len;
   } else {
      // empty file (set warning?) - no mechanism for warnings
      src->buffer.bucket = NULL;
      src->buffer.end = NULL;
   }

   src->stride = sizeof(char);
   src->doc = *doc;
   src->pos = 0;
   src->line = 1;
   src->col = 1;
   src->dialect = ANVL_DIALECT_AML; // default dialect

   if (!source_validate(src)) {
      success = false;
      goto error;
   }

   // creating source takes ownership of data having copied it to buffer
   free(*data);
   (*data) = NULL;
   (*doc)->source = src;
   return success;

error:
   // TODO: set error condition
   return success;
}
static void source_dispose(anvl_src self) {
   if (!self)
      return;
   if (self->buffer.bucket) {
      free(self->buffer.bucket);
   }
   Allocator.dispose(self);
}
static anvl_dialect source_get_dialect(anvl_src self) {
   if (!self)
      return ANVL_DIALECT_ERROR;
   return self->dialect;
}
static bool source_has_errors() {}
static usize source_position(anvl_src self) { return self->pos; }
static usize source_line(anvl_src self) { return self->line; }
static usize source_column(anvl_src self) { return self->col; }
static bool source_is_eof(anvl_src self) {
   return self->pos >= (usize)(self->buffer.end - self->buffer.bucket);
}
static bool source_is_eof_offset(anvl_src self, usize offset) {
   return self->pos + offset >= (usize)(self->buffer.end - self->buffer.bucket);
}
static char source_peek_offset(anvl_src self, usize offset) {
   usize idx = self->pos + offset;
   usize len = (usize)(self->buffer.end - self->buffer.bucket);
   return idx < len ? ((char *)self->buffer.bucket)[idx] : '\0';
}
static char source_peek(anvl_src self) { return source_peek_offset(self, 0); }
static usize source_scan_match(anvl_src self, const char *s, usize slen) {
   if (!s || slen == 0)
      return 0;
   for (usize i = 0; i < slen; i++) {
      if (source_peek_offset(self, i) != s[i])
         return 0;
   }
   return self->pos + slen <= (usize)(self->buffer.end - self->buffer.bucket) ? slen : 0;
}
static usize source_match_operator(anvl_src self, const char *op, usize oplen) {
   return source_scan_match(self, op, oplen);
}
static bool source_is_alpha(char c) {
   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static bool source_is_digit(char c) { return c >= '0' && c <= '9'; }
static bool source_is_hex_digit(char c) {
   return source_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static bool source_is_identifier_start(char c) { return source_is_alpha(c); }
static bool source_is_identifier_part(char c) {
   return source_is_alpha(c) || source_is_digit(c) || c == '.' || c == '-';
}
static usize source_consume(anvl_src self, usize count) {
   for (usize i = 0; i < count; i++) {
      if (source_is_eof(self))
         return i;

      char c = source_peek(self);
      self->pos++;

      if (c == '\n') {
         self->line++;
         self->col = 1;
      } else {
         self->col++;
      }
   }
   return count;
}
static const char *source_data(anvl_src self) { return self->buffer.bucket; }
static usize source_length(anvl_src self) {
   // does this work if bucket
   if (!self || !self->buffer.bucket)
      return 0;
   return (usize)(self->buffer.end - self->buffer.bucket);
}
static void source_substring(anvl_src self, usize start, usize len, char *out_buf) {
   if (!out_buf) {
      return;
   }

   if (!self || start >= (usize)(self->buffer.end - self->buffer.bucket) || len == 0) {
      out_buf[0] = '\0';
      return;
   }

   usize available = (usize)(self->buffer.end - self->buffer.bucket) - start;
   usize actual_len = len < available ? len : available;

   memcpy(out_buf, self->buffer.bucket + start, actual_len);
   out_buf[actual_len] = '\0';
}
static usize source_scan_whitespace(anvl_src self, usize offset) {
   usize pos = offset;
   char c = source_peek_offset(self, pos);
   while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      pos++;
      c = source_peek_offset(self, pos);
   }
   return pos;
}
static usize source_scan_line_comment(anvl_src self, usize offset) {
   if (source_peek_offset(self, offset) == '/' && source_peek_offset(self, offset + 1) == '/') {
      usize pos = offset + 2;
      char c = source_peek_offset(self, pos);
      while (c != '\n' && c != '\0') {
         pos++;
         c = source_peek_offset(self, pos);
      }
      if (c == '\n') {
         pos++;
      }
      return pos;
   }
   return offset;
}
static usize source_scan_block_comment(anvl_src self, usize offset) {
   if (source_peek_offset(self, offset) == '/' && source_peek_offset(self, offset + 1) == '*') {
      usize pos = offset + 2;
      char c = source_peek_offset(self, pos);
      while (!(c == '*' && source_peek_offset(self, pos + 1) == '/') && c != '\0') {
         pos++;
         c = source_peek_offset(self, pos);
      }
      if (c == '\0') {
         anvl_error_code code = ANVL_ERR_PARSER_UNTERMINATED_COMMENT;
         doc_set_error(self->doc, code, source_line(self), source_column(self), __FILE__);

         return pos;
      }
      if (c != '\0') {
         pos += 2;
      }
      return pos;
   }
   return offset;
}
static usize source_skip_whitespace_and_comments(anvl_src self) {
   usize pos = 0;
   bool changed = true;

   while (changed && !source_is_eof_offset(self, pos)) {
      usize old_pos = pos;
      pos = source_scan_whitespace(self, pos);
      pos = source_scan_line_comment(self, pos);
      pos = source_scan_block_comment(self, pos);
      // check if error state is set
      if (Source.has_errors(self->doc)) {
         return pos;
      }

      changed = (pos != old_pos);
   }

   return pos;
}
static bool source_is_shebang(anvl_src self) {
   if (source_peek(self) != '#' || source_peek_offset(self, 1) != '!')
      return false;
   char d1 = source_peek_offset(self, 2);
   char d2 = source_peek_offset(self, 3);
   char d3 = source_peek_offset(self, 4);
   return (d1 == 'a' && d2 == 'm' && d3 == 'l') || (d1 == 'a' && d2 == 's' && d3 == 'l') ||
          (d1 == 'a' && d2 == 'm' && d3 == 'p');
}
static anvl_dialect source_parse_dialect(anvl_src self, anvl_dialect current) {
   usize skipped = source_skip_whitespace_and_comments(self);
   if (!doc_has_errors(self->doc)) {
      source_consume(self, skipped);
   }
   if (source_is_shebang(self)) {
      source_consume(self, 2);

      char first = source_peek(self);
      char second = source_peek_offset(self, 1);
      char third = source_peek_offset(self, 2);

      if (first == 'a' && second == 'm' && third == 'l') {
         source_consume(self, 3);
         return ANVL_DIALECT_AML;
      } else if (first == 'a' && second == 'm' && third == 'p') {
         source_consume(self, 3);
         return ANVL_DIALECT_AMP;
      } else if (first == 'a' && second == 's' && third == 'l') {
         source_consume(self, 3);
         return ANVL_DIALECT_ASL;
      }
   }
   return current;
}
static void source_set_position(anvl_src self, usize pos, usize line, usize col) {
   self->pos = pos;
   self->line = line;
   self->col = col;
}
static void source_reset(anvl_src self) {
   self->pos = 0;
   self->line = 1;
   self->col = 1;
}

const anvl_source_i Source = {
   .create = source_create,
   .dispose = source_dispose,
   //  .dialect = source_get_dialect,
   .has_errors = source_has_errors,
   //  .position = source_position,
   //  .line = source_line,
   //  .column = source_column,
   //  .is_eof = source_is_eof,
   //  .is_eof_offset = source_is_eof_offset,
   //  .peek = source_peek,
   //  .peek_offset = source_peek_offset,
   //  .match_length = source_scan_match,
   //  .match_operator = source_match_operator,
   //  .is_alpha = source_is_alpha,
   //  .is_digit = source_is_digit,
   //  .is_hex_digit = source_is_hex_digit,
   //  .is_identifier_start = source_is_identifier_start,
   //  .is_identifier_part = source_is_identifier_part,
   //  .consume = source_consume,
   //  .data = source_data,
   //  .length = source_length,
   //  .substring = source_substring,
   //  .skip_whitespace_and_comments = source_skip_whitespace_and_comments,
   //  .is_shebang = source_is_shebang,
   //  .parse_dialect = source_parse_dialect,
   //  .set_position = source_set_position,
   //  .reset = source_reset,
};