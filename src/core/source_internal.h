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
 * source.c - Source buffer management
 * ------------------------------------------------------------------
 * Note: The anvl_source_i vtable and all interrogator functions have
 * been moved to context.c where the public API is centralized.
 * This file is kept as a stub for future source-specific utilities.
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-13
 * File: src/core/source.c
 * ------------------------------------------------------------------
 * Internal source buffer management for Anvil. Fast-path helpers for
 * scanning source content directly on the `struct anvl_source_t`.
 */
#pragma once

#include "context.h"

// Return raw data pointer (byte buffer)
static inline const char *si_data(source s) {
   return (const char *)s->buffer.bucket;
}

// Total length of the source buffer (in bytes)
static inline usize si_length(source s) {
   const char *begin = (const char *)s->buffer.bucket;
   const char *end = (const char *)s->buffer.end;
   if (!begin || !end)
      return 0;
   usize bytes = (usize)(end - begin);
   return s->stride ? (bytes / s->stride) : bytes;
}

// Current absolute position
static inline usize si_position(source s) { return s->pos; }

// Line and column (1-based)
static inline usize si_line(source s) { return s->line; }
static inline usize si_column(source s) { return s->col; }

// EOF checks
static inline bool si_is_eof(source s) { return si_position(s) >= si_length(s); }
static inline bool si_is_eof_offset(source s, usize off) {
   return (si_position(s) + off) >= si_length(s);
}

// Peek helpers
static inline char si_peek(source s) {
   if (si_is_eof(s))
      return '\0';
   return si_data(s)[si_position(s)];
}

static inline char si_peek_offset(source s, usize off) {
   if (si_is_eof_offset(s, off))
      return '\0';
   return si_data(s)[si_position(s) + off];
}

// Advance and maintain line/column
static inline usize si_consume(source s, usize n) {
   const char *buf = si_data(s);
   usize len = si_length(s);
   usize pos = si_position(s);
   if (n == 0 || pos >= len)
      return 0;
   usize to = pos + n;
   if (to > len)
      to = len;
   for (usize i = pos; i < to; i++) {
      char c = buf[i];
      if (c == '\n') {
         s->line += 1;
         s->col = 1;
      } else {
         s->col += 1;
      }
   }
   s->pos = to;
   return to - pos;
}

// Literal and operator matches (exact length)
static inline usize si_match_length(source s, const char *lit, usize n) {
   if (!lit || n == 0)
      return 0;
   if (si_is_eof_offset(s, n - 1))
      return 0;
   const char *buf = si_data(s) + si_position(s);
   for (usize i = 0; i < n; i++) {
      if (buf[i] != lit[i])
         return 0;
   }
   return n;
}

static inline usize si_match_operator(source s, const char *op, usize n) {
   return si_match_length(s, op, n);
}

// Whitespace/comments skipping: delegate to existing implementation for now
// (We can replace with a fully inlined scanner later once grammar is locked.)
static inline usize si_skip_whitespace_and_comments(source s) {
   usize n = Source.skip_whitespace_and_comments(s);
   si_consume(s, n);
   return n;
}

// Position management helpers
static inline void si_set_position(source s, usize pos, usize line, usize col) {
   s->pos = pos;
   s->line = line;
   s->col = col;
}
