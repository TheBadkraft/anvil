/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * source.c - Source interface implementation
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-13
 * File: src/core/source.c
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for functions used in source_create
static usize source_skip_whitespace_and_comments(source self);
static usize source_consume(source self, usize n);
static char *source_substring(source self, usize start, usize len);

// --- Substring extraction ---
static char *source_substring(source self, usize start, usize len) {
   if (start >= (usize)(self->buffer.end - self->buffer.bucket) || len == 0) {
      return NULL;
   }

   usize available = (usize)(self->buffer.end - self->buffer.bucket) - start;
   usize actual_len = len < available ? len : available;

   char *result = Memory.alloc(actual_len + 1, false);
   memcpy(result, self->buffer.bucket + start, actual_len);
   result[actual_len] = '\0';
   return result;
}
static bool source_is_shebang(source self);
static usize source_match_length(source self, const char *s, usize slen);
static void source_reset(source self);

// --- Helper functions for whitespace and comments ---
static usize source_scan_whitespace_length(source self, usize offset);
static usize source_scan_line_comment_length(source self, usize offset);
static usize source_scan_block_comment_length(source self, usize offset);

/* Source interface implementations */
static source source_create(const char *data, usize len) {
   source src = Memory.alloc(sizeof(struct anvl_source_t), true);

   // Always allocate our own buffer - source owns its data
   if (data && len > 0) {
      char *buffer = Memory.alloc(len + 1, false); // +1 for null terminator
      memcpy(buffer, data, len);
      buffer[len] = '\0'; // null terminate
      src->buffer.bucket = buffer;
      src->buffer.end = buffer + len;
   } else {
      // Empty source
      src->buffer.bucket = NULL;
      src->buffer.end = NULL;
   }

   // Set default dialect initially
   src->dialect = ANVL_DIALECT_ASL;
   src->stride = sizeof(char);
   src->pos = 0;
   src->line = 1;
   src->col = 1;

   // Use interrogators to detect actual dialect from shebang (after whitespace/comments)
   if (data && len > 0) {
      usize skip_len = source_skip_whitespace_and_comments(src);
      source_consume(src, skip_len);
      if (source_is_shebang(src)) {
         if (source_match_length(src, "#!aml", 5) == 5) {
            src->dialect = ANVL_DIALECT_AML;
         } else if (source_match_length(src, "#!asl", 5) == 5) {
            src->dialect = ANVL_DIALECT_ASL;
         }
      }
      // do not reset position after shebang ... prep. why scan the preamble again?
      // in fact, be kind and skip ws/comments
      skip_len = source_skip_whitespace_and_comments(src);
      if (skip_len > 0) {
         source_consume(src, skip_len);
      }
   }

   return src;
}

static void source_dispose(source self) {
   if (self->buffer.bucket) {
      Memory.dispose(self->buffer.bucket);
   }
   Memory.dispose(self);
}

static anvl_dialect source_get_dialect(source self) {
   return self->dialect;
}

// --- Position & EOF ---
static usize source_position(source self) {
   return self->pos;
}

static usize source_line(source self) {
   return self->line;
}

static usize source_column(source self) {
   return self->col;
}

static bool source_is_eof(source self) {
   return self->pos >= (usize)(self->buffer.end - self->buffer.bucket);
}

static bool source_is_eof_offset(source self, usize offset) {
   return self->pos + offset >= (usize)(self->buffer.end - self->buffer.bucket);
}

// --- Character peek ---
static char source_peek_offset(source self, usize offset) {
   usize idx = self->pos + offset;
   usize len = (usize)(self->buffer.end - self->buffer.bucket);
   return idx < len ? ((char *)self->buffer.bucket)[idx] : '\0';
}

static char source_peek(source self) {
   return source_peek_offset(self, 0);
}

// --- String matching ---
static usize source_match_length(source self, const char *s, usize slen) {
   if (!s || slen == 0)
      return 0;
   for (usize i = 0; i < slen; i++) {
      if (source_peek_offset(self, i) != s[i])
         return 0;
   }
   return self->pos + slen <= (usize)(self->buffer.end - self->buffer.bucket) ? slen : 0;
}

static usize source_match_operator(source self, const char *op, usize oplen) {
   return source_match_length(self, op, oplen);
}

// --- Character classification ---
static bool source_is_alpha(char c) {
   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool source_is_digit(char c) {
   return c >= '0' && c <= '9';
}

static bool source_is_hex_digit(char c) {
   return source_is_digit(c) ||
          (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool source_is_identifier_start(char c) {
   return source_is_alpha(c);
}

static bool source_is_identifier_part(char c) {
   return source_is_alpha(c) || source_is_digit(c) || c == '.';
}

// --- Operator & Symbol classification ---
static anvl_operator source_is_operator(source self) {
   // Try operators in order of length (longest first)
   const char *pos = self->buffer.bucket + self->pos;
   usize remaining = (usize)(self->buffer.end - self->buffer.bucket) - self->pos;

   // Check for 2-character operators first
   if (remaining >= 2) {
      if (anvl_is_operator(pos, 2)) {
         return anvl_operator_from_symbol(pos, 2);
      }
   }

   // Check for 1-character operators
   if (remaining >= 1) {
      if (anvl_is_operator(pos, 1)) {
         return anvl_operator_from_symbol(pos, 1);
      }
   }

   return ANVL_OP_INVALID;
}

static anvl_symbol source_is_symbol(source self) {
   // Try symbols in order of length (longest first)
   const char *pos = self->buffer.bucket + self->pos;
   usize remaining = (usize)(self->buffer.end - self->buffer.bucket) - self->pos;

   // Check for 2-character symbols first
   if (remaining >= 2) {
      if (anvl_is_symbol(pos, 2)) {
         return anvl_symbol_from_symbol(pos, 2);
      }
   }

   // Check for 1-character symbols
   if (remaining >= 1) {
      if (anvl_is_symbol(pos, 1)) {
         return anvl_symbol_from_symbol(pos, 1);
      }
   }

   return ANVL_SYM_INVALID;
}

// --- Consume ---
static usize source_consume(source self, usize n) {
   // why are we doing this with a for loop??? for line & col adjustment?
   // just make sure our pos + n doesn't exceed the buffer end and then
   // do a single add to pos. but how do we adjust line & col then?
   // we have to scan the consumed characters for newlines anyway...
   for (usize i = 0; i < n; i++) {
      if (source_is_eof(self))
         break;
      char c = ((char *)self->buffer.bucket)[self->pos++];
      if (c == '\n') {
         self->line++;
         self->col = 1;
      } else {
         self->col++;
      }
   }
   return self->pos; // return new position
}

// --- Helper functions for whitespace and comments ---
static usize source_scan_whitespace_length(source self, usize offset) {
   usize len = 0;
   while (!source_is_eof_offset(self, offset + len)) {
      char c = source_peek_offset(self, offset + len);
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
         len++;
      } else {
         break;
      }
   }
   return len;
}

static usize source_scan_line_comment_length(source self, usize offset) {
   usize current_pos = self->pos;
   self->pos += offset;
   usize match_len = source_match_length(self, "//", 2);
   self->pos = current_pos;
   if (match_len == 0)
      return 0;
   usize len = 2;
   while (!source_is_eof_offset(self, offset + len)) {
      char c = source_peek_offset(self, offset + len);
      if (c == '\n')
         break;
      len++;
   }
   return len;
}

static usize source_scan_block_comment_length(source self, usize offset) {
   usize current_pos = self->pos;
   self->pos += offset;
   usize match_len = source_match_length(self, "/*", 2);
   self->pos = current_pos;
   if (match_len == 0)
      return 0;
   usize len = 2;
   usize depth = 1;
   while (depth > 0 && !source_is_eof_offset(self, offset + len)) {
      usize current_pos_inner = self->pos;
      self->pos += offset + len;
      usize close_match = source_match_length(self, "*/", 2);
      usize open_match = source_match_length(self, "/*", 2);
      self->pos = current_pos_inner;

      if (close_match != 0) {
         len += 2;
         depth--;
      } else if (open_match != 0) {
         len += 2;
         depth++;
      } else {
         len++;
      }
   }
   return depth == 0 ? len : 0; // incomplete block comment → not skipped
}

// --- Whitespace & comments ---
static usize source_skip_whitespace_and_comments(source self) {
   while (true) {
      usize ws = source_scan_whitespace_length(self, 0);
      usize line_comment = source_scan_line_comment_length(self, ws);
      usize block_comment = source_scan_block_comment_length(self, ws + line_comment);
      if (ws + line_comment + block_comment == 0)
         break;
      source_consume(self, ws + line_comment + block_comment);
   }
   return self->pos;
}

// --- Shebang ---
static bool source_is_shebang(source self) {
   return source_match_length(self, "#!asl", 5) == 5 ||
          source_match_length(self, "#!aml", 5) == 5;
}

// --- Dialect parsing ---
static anvl_dialect source_parse_dialect(source self, anvl_dialect hint) {
   source_skip_whitespace_and_comments(self);

   if (source_is_shebang(self)) {
      if (source_match_length(self, "#!aml", 5) == 5) {
         source_consume(self, 5);
         return ANVL_DIALECT_AML;
      } else if (source_match_length(self, "#!asl", 5) == 5) {
         source_consume(self, 5);
         return ANVL_DIALECT_ASL;
      }
   }
   return hint;
}

// --- Position management ---
static void source_set_position(source self, usize pos, usize line, usize col) {
   self->pos = pos;
   self->line = line;
   self->col = col;
}

static void source_reset(source self) {
   self->pos = 0;
   self->line = 1;
   self->col = 1;
}

const anvl_source_i Source = {
    .create = source_create,
    .dispose = source_dispose,
    .dialect = source_get_dialect,
    .position = source_position,
    .line = source_line,
    .column = source_column,
    .is_eof = source_is_eof,
    .is_eof_offset = source_is_eof_offset,
    .peek = source_peek,
    .peek_offset = source_peek_offset,
    .match_length = source_match_length,
    .match_operator = source_match_operator,
    .is_alpha = source_is_alpha,
    .is_digit = source_is_digit,
    .is_hex_digit = source_is_hex_digit,
    .is_identifier_start = source_is_identifier_start,
    .is_identifier_part = source_is_identifier_part,
    .is_operator = source_is_operator,
    .is_symbol = source_is_symbol,
    .consume = source_consume,
    .substring = source_substring,
    .skip_whitespace_and_comments = source_skip_whitespace_and_comments,
    .is_shebang = source_is_shebang,
    .parse_dialect = source_parse_dialect,
    .set_position = source_set_position,
    .reset = source_reset};
