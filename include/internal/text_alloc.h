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
 * internal/text_alloc.h - full-length text reads for types and schema    *
 * ---------------------------------------------------------------------- *
 * The public getters return the FULL length (snprintf-style), so these    *
 * query it first and allocate exactly - never a fixed buffer that could   *
 * silently compare a truncated prefix. Release with Allocator.dispose.    *
 * ********************************************************************** */

#pragma once

#include "anvil_flat.h"
#include <sigma.core/allocator.h>
#include <string.h>

/// A fresh NUL-terminated copy of s, or NULL on allocation failure.
static inline char *anvil_text_dup(const char *s) {
   size_t len = strlen(s);
   char *copy = Allocator.alloc(len + 1);
   if (copy) {
      memcpy(copy, s, len + 1);
   }
   return copy;
}

/// A statement's full name, or NULL on allocation failure.
static inline char *anvil_name_alloc(anvil_statement stmt) {
   size_t len = anvil_statement_get_name(stmt, NULL, 0);
   char *name = Allocator.alloc(len + 1);
   if (name) {
      anvil_statement_get_name(stmt, name, len + 1);
   }
   return name;
}

/// A value's full text, or NULL on allocation failure.
static inline char *anvil_text_alloc(anvil_value val) {
   size_t len = anvil_value_get_text(val, NULL, 0);
   char *text = Allocator.alloc(len + 1);
   if (text) {
      anvil_value_get_text(val, text, len + 1);
   }
   return text;
}

/// A value's full text and its exact byte length. The text may contain NUL bytes (a string or blob
/// can); the buffer is allocated len + 1 and always NUL-terminated, but only `*out_len` is the
/// content. NULL on allocation failure.
static inline char *anvil_text_alloc_n(anvil_value val, size_t *out_len) {
   size_t len = anvil_value_get_text(val, NULL, 0);
   char *text = Allocator.alloc(len + 1);
   if (text) {
      anvil_value_get_text(val, text, len + 1);
      text[len] = '\0';
   }
   if (out_len) {
      *out_len = text ? len : 0;
   }
   return text;
}

/// The text of a value that NAMES something (a type, a format, a tag, a true/false flag): one that
/// is compared as a C string. If the text contains a NUL byte it returns NULL and sets *had_nul -
/// callers report the position as malformed - rather than a prefix that could match a real name.
/// Also NULL on allocation failure (with *had_nul false).
static inline char *anvil_ident_alloc(anvil_value val, bool *had_nul) {
   size_t len = 0;
   char *text = anvil_text_alloc_n(val, &len);
   *had_nul = false;
   if (text && memchr(text, '\0', len) != NULL) {
      *had_nul = true;
      Allocator.dispose(text);
      return NULL;
   }
   return text;
}

/// " (the value contains a NUL byte)" for a diagnostic when the flag is set, else "".
#define ANVIL_NUL_NOTE(flag) ((flag) ? " (the value contains a NUL byte)" : "")

/// A stored string that may contain NUL bytes: length-carried, one allocation. Enum labels are these;
/// every comparison is a length check then memcmp, never strcmp.
typedef struct anvil_label {
   size_t len;
   char text[]; // len bytes, then a terminator
} anvil_label;

static inline anvil_label *anvil_label_new(const char *text, size_t len) {
   anvil_label *l = Allocator.alloc(sizeof(anvil_label) + len + 1);
   if (l) {
      l->len = len;
      if (len) {
         memcpy(l->text, text, len);
      }
      l->text[len] = '\0';
   }
   return l;
}

/// A value's text as a label (exact length, NULs included).
static inline anvil_label *anvil_label_from_value(anvil_value val) {
   size_t len = anvil_value_get_text(val, NULL, 0);
   anvil_label *l = Allocator.alloc(sizeof(anvil_label) + len + 1);
   if (l) {
      l->len = len;
      anvil_value_get_text(val, l->text, len + 1);
      l->text[len] = '\0';
   }
   return l;
}

/// The snprintf-style getter body shared by every public label accessor: returns the FULL byte
/// length, and copies at most buflen - 1 bytes (NULs included) plus a terminator.
static inline size_t anvil_label_copy_out(const anvil_label *l, char *buf, size_t buflen) {
   if (!l) {
      return 0;
   }
   if (buf && buflen > 0) {
      size_t n = l->len < buflen - 1 ? l->len : buflen - 1;
      if (n) {
         memcpy(buf, l->text, n);
      }
      buf[n] = '\0';
   }
   return l->len;
}

/// True if the label is exactly `len` bytes equal to `text`.
static inline bool anvil_label_equals(const anvil_label *l, const char *text, size_t len) {
   return l && l->len == len && (len == 0 || memcmp(l->text, text, len) == 0);
}
