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
 * writer.c - Streaming ANVL writer                                       *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Implements anvil_writer.h. Links no reader code: the only thing shared *
 * with the parser is include/constants.h (token characters and the       *
 * reserved-word list), so a keyword added there is rejected here too.    *
 * The character classes below (identifier, bare-literal, numeric) are    *
 * restated from src/core/source.c and parser.c because the reader isn't  *
 * linked; test/unit/test_writer_roundtrip.c is what catches them drifting*
 * apart.                                                                 *
 * ********************************************************************** */

#include "anvil_writer.h"
#include "constants.h"
#include "grammar.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INDENT_WIDTH 3

typedef enum { FRAME_ARRAY, FRAME_TUPLE, FRAME_OBJECT } frame_kind;

typedef struct {
   frame_kind kind;
   size_t count;    // elements (array/tuple) or completed statements (object)
   bool multiline;  // an object element was written, so later elements go on their own lines
   bool owns_stmt;  // this collection is a statement's value, so closing it emits ';'
} frame;

typedef enum { PHASE_HEADER, PHASE_BODY, PHASE_FINISHED } phase;

struct anvil_writer_t {
   anvil_writer_dialect dialect;
   anvil_writer_err_code err;
   phase phase;
   char *buf;
   size_t len;
   size_t cap;
   frame *stack;
   size_t depth;
   size_t stack_cap;
   bool header_dirty; // a module attribute or include has been written
   bool stmt_open;    // a statement name is written and its value is pending
   bool stmt_base;    // that statement declared an inheritance base
   bool attrs_open;   // that statement's `@[` group is written but not yet closed
   wg_nameset names;  // top-level statement names so far
};

/* ----------------------------------------------------------------------- *
 * Errors
 * ----------------------------------------------------------------------- */
static bool fail(anvil_writer w, anvil_writer_err_code code) {
   if (w->err == ANVIL_WRITER_OK) {
      w->err = code;
   }
   return false;
}

const char *anvil_writer_error_message(anvil_writer_err_code code) {
   switch (code) {
   case ANVIL_WRITER_OK:
      return "no error";
   case ANVIL_WRITER_ERR_INVALID_ARGUMENT:
      return "invalid argument";
   case ANVIL_WRITER_ERR_MEMORY:
      return "memory allocation failed";
   case ANVIL_WRITER_ERR_STATE:
      return "call is not legal at this point in the document";
   case ANVIL_WRITER_ERR_DIALECT:
      return "construct is forbidden in this dialect";
   case ANVIL_WRITER_ERR_INVALID_IDENTIFIER:
      return "invalid identifier";
   case ANVIL_WRITER_ERR_RESERVED_WORD:
      return "reserved word";
   case ANVIL_WRITER_ERR_INVALID_NUMERIC:
      return "invalid numeric";
   case ANVIL_WRITER_ERR_INVALID_BARE:
      return "text cannot be written as a bare literal";
   case ANVIL_WRITER_ERR_INVALID_BLOB:
      return "invalid blob tag or content";
   case ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE:
      return "invalid attribute value";
   case ANVIL_WRITER_ERR_INVALID_INCLUDE_PATH:
      return "invalid include path";
   case ANVIL_WRITER_ERR_EMPTY_COLLECTION:
      return "empty collection (arrays and objects need 1 element, tuples 2)";
   case ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT:
      return "a statement with a base must have an object value";
   case ANVIL_WRITER_ERR_UNFINISHED:
      return "document has an open statement, array, tuple or object";
   case ANVIL_WRITER_ERR_DUPLICATE_NAME:
      return "top-level name declared twice";
   case ANVIL_WRITER_ERR_DEPTH_EXCEEDED:
      return "nesting is too deep";
   }
   return "unknown error";
}

anvil_writer_err_code anvil_writer_get_error(anvil_writer w) {
   return w ? w->err : ANVIL_WRITER_ERR_INVALID_ARGUMENT;
}

// Common entry check: a usable handle, no earlier error, not yet finished.
static bool ready(anvil_writer w) {
   if (!w || w->err != ANVIL_WRITER_OK) {
      return false;
   }
   if (w->phase == PHASE_FINISHED) {
      return fail(w, ANVIL_WRITER_ERR_STATE);
   }
   return true;
}

/* ----------------------------------------------------------------------- *
 * Output buffer
 * ----------------------------------------------------------------------- */
static bool put(anvil_writer w, const char *s, size_t n) {
   if (w->len + n + 1 > w->cap) {
      size_t cap = w->cap ? w->cap : 256;
      while (w->len + n + 1 > cap) {
         cap *= 2;
      }
      char *grown = realloc(w->buf, cap);
      if (!grown) {
         return fail(w, ANVIL_WRITER_ERR_MEMORY);
      }
      w->buf = grown;
      w->cap = cap;
   }
   memcpy(w->buf + w->len, s, n);
   w->len += n;
   w->buf[w->len] = '\0';
   return true;
}

static bool puts_(anvil_writer w, const char *s) { return put(w, s, strlen(s)); }

static bool put_char(anvil_writer w, char c) { return put(w, &c, 1); }

static bool put_indent(anvil_writer w, size_t levels) {
   for (size_t i = 0; i < levels * INDENT_WIDTH; i++) {
      if (!put_char(w, ' ')) {
         return false;
      }
   }
   return true;
}

static bool check(anvil_writer w, anvil_writer_err_code code) {
   return code == ANVIL_WRITER_OK ? true : fail(w, code);
}

/* ----------------------------------------------------------------------- *
 * Lifecycle
 * ----------------------------------------------------------------------- */
anvil_writer anvil_writer_new(anvil_writer_dialect dialect) {
   if (dialect != ANVIL_WRITER_AML && dialect != ANVIL_WRITER_AMP) {
      return NULL;
   }
   anvil_writer w = calloc(1, sizeof *w);
   if (!w) {
      return NULL;
   }
   w->dialect = dialect;
   w->phase = PHASE_HEADER;
   if (!puts_(w, dialect == ANVIL_WRITER_AML ? ANVL_TOK_SHEBANG_PREFIX "aml\n\n"
                                              : ANVL_TOK_SHEBANG_PREFIX "amp\n\n")) {
      anvil_writer_dispose(w);
      return NULL;
   }
   return w;
}

void anvil_writer_dispose(anvil_writer w) {
   if (!w) {
      return;
   }
   free(w->buf);
   free(w->stack);
   wg_nameset_free(&w->names);
   free(w);
}

/* ----------------------------------------------------------------------- *
 * Header
 * ----------------------------------------------------------------------- */
static bool attribute_emit(anvil_writer w, const char *key, const char *value, bool quoted,
                           size_t value_len) {
   if (w->stmt_open) {
      if (!(w->attrs_open ? puts_(w, ", ") : puts_(w, " @["))) {
         return false;
      }
      w->attrs_open = true;
   } else if (!puts_(w, "@[")) {
      return false;
   }
   if (!puts_(w, key)) {
      return false;
   }
   if (value) {
      if (!put_char(w, '=') || (quoted && !put_char(w, ANVL_TOK_QUOTE)) ||
          !put(w, value, value_len) || (quoted && !put_char(w, ANVL_TOK_QUOTE))) {
         return false;
      }
   }
   if (!w->stmt_open) {
      w->header_dirty = true;
      return puts_(w, "]\n");
   }
   return true;
}

// Decides which kind of attribute this call is, or fails with the right error.
static bool attribute_context(anvil_writer w) {
   if (!ready(w)) {
      return false;
   }
   if (w->dialect == ANVIL_WRITER_AMP) {
      return fail(w, ANVIL_WRITER_ERR_DIALECT);
   }
   bool in_statement = w->stmt_open;
   bool in_header = w->phase == PHASE_HEADER && w->depth == 0;
   return in_statement || in_header ? true : fail(w, ANVIL_WRITER_ERR_STATE);
}

bool anvil_writer_attribute(anvil_writer w, const char *key, const char *value) {
   if (!attribute_context(w)) {
      return false;
   }
   if (!check(w, wg_check_attribute_key(key)) || (value && !check(w, wg_check_attribute_value(value)))) {
      return false;
   }
   return attribute_emit(w, key, value, false, value ? strlen(value) : 0);
}

bool anvil_writer_attribute_string(anvil_writer w, const char *key, const char *text,
                                   size_t length) {
   if (!attribute_context(w)) {
      return false;
   }
   if (!check(w, wg_check_attribute_key(key)) || !check(w, wg_check_attribute_text(text, length))) {
      return false;
   }
   return attribute_emit(w, key, text ? text : "", true, length);
}

bool anvil_writer_include(anvil_writer w, const char *path) {
   if (!ready(w)) {
      return false;
   }
   if (w->dialect == ANVIL_WRITER_AMP) {
      return fail(w, ANVIL_WRITER_ERR_DIALECT);
   }
   if (w->phase != PHASE_HEADER || w->depth != 0 || w->stmt_open) {
      return fail(w, ANVIL_WRITER_ERR_STATE);
   }
   if (!check(w, wg_check_include_path(path))) {
      return false;
   }
   w->header_dirty = true;
   return puts_(w, ANVL_KEYWORD_INCLUDE " \"") && puts_(w, path) && puts_(w, "\";\n");
}

/* ----------------------------------------------------------------------- *
 * Statements
 * ----------------------------------------------------------------------- */
bool anvil_writer_statement(anvil_writer w, const char *name, const char *base) {
   if (!ready(w)) {
      return false;
   }
   if (base && w->dialect == ANVIL_WRITER_AMP) {
      return fail(w, ANVIL_WRITER_ERR_DIALECT);
   }
   bool in_object_or_top = w->depth == 0 || w->stack[w->depth - 1].kind == FRAME_OBJECT;
   if (w->stmt_open || !in_object_or_top) {
      return fail(w, ANVIL_WRITER_ERR_STATE);
   }
   if (!check(w, wg_check_name(name)) || (base && !check(w, wg_check_name(base)))) {
      return false;
   }
   if (w->depth == 0) {
      int added = wg_nameset_add(&w->names, name);
      if (added < 0) {
         return fail(w, ANVIL_WRITER_ERR_MEMORY);
      }
      if (added == 0) {
         return fail(w, ANVIL_WRITER_ERR_DUPLICATE_NAME);
      }
   }
   if (w->phase == PHASE_HEADER) {
      if (w->header_dirty && !put_char(w, '\n')) {
         return false;
      }
      w->phase = PHASE_BODY;
   }
   if (!put_indent(w, w->depth) || !puts_(w, name)) {
      return false;
   }
   if (base && (!puts_(w, " : ") || !puts_(w, base))) {
      return false;
   }
   w->stmt_open = true;
   w->stmt_base = base != NULL;
   w->attrs_open = false;
   return true;
}

/* ----------------------------------------------------------------------- *
 * Values
 * ----------------------------------------------------------------------- */
static bool in_collection(anvil_writer w) {
   return w->depth > 0 && w->stack[w->depth - 1].kind != FRAME_OBJECT;
}

// Everything that must happen before a value's own text is written: where it goes (an
// array/tuple element, or the open statement's value), the dialect and base rules, and the
// separator/`:=` that precedes it. `*owns_stmt` reports whether the value completes a statement.
static bool value_prelude(anvil_writer w, bool is_object, bool is_collection, bool *owns_stmt) {
   if (!ready(w)) {
      return false;
   }
   if (in_collection(w)) {
      frame *top = &w->stack[w->depth - 1];
      if (w->dialect == ANVIL_WRITER_AMP && is_collection) {
         return fail(w, ANVIL_WRITER_ERR_DIALECT);
      }
      if (top->count > 0 && !put_char(w, ',')) {
         return false;
      }
      if (top->multiline || is_object) {
         top->multiline = true;
         if (!put_char(w, '\n') || !put_indent(w, w->depth)) {
            return false;
         }
      } else if (top->count > 0 && !put_char(w, ' ')) {
         return false;
      }
      *owns_stmt = false;
      return true;
   }
   if (!w->stmt_open) {
      return fail(w, ANVIL_WRITER_ERR_STATE);
   }
   if (is_object && w->dialect == ANVIL_WRITER_AMP) {
      return fail(w, ANVIL_WRITER_ERR_DIALECT);
   }
   if (w->stmt_base && !is_object) {
      return fail(w, ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT);
   }
   if ((w->attrs_open && !put_char(w, ANVL_TOK_RBRACKET)) || !puts_(w, " " ANVL_TOK_ASSIGN " ")) {
      return false;
   }
   w->stmt_open = false;
   w->attrs_open = false;
   *owns_stmt = true;
   return true;
}

// Bookkeeping after a value is fully written.
static bool value_complete(anvil_writer w, bool owns_stmt) {
   frame *top = w->depth > 0 ? &w->stack[w->depth - 1] : NULL;
   if (owns_stmt) {
      if (!puts_(w, ";\n")) {
         return false;
      }
      if (top) {
         top->count++;
      }
   } else if (top) {
      top->count++;
   }
   return true;
}

static bool write_scalar(anvil_writer w, const char *text, size_t n) {
   bool owns = false;
   return value_prelude(w, false, false, &owns) && put(w, text, n) && value_complete(w, owns);
}

bool anvil_writer_null(anvil_writer w) {
   return w && write_scalar(w, ANVL_KEYWORD_NULL, ANVL_KEYWORD_NULL_LEN);
}

bool anvil_writer_bool(anvil_writer w, bool value) {
   return w && (value ? write_scalar(w, ANVL_KEYWORD_TRUE, ANVL_KEYWORD_TRUE_LEN)
                      : write_scalar(w, ANVL_KEYWORD_FALSE, ANVL_KEYWORD_FALSE_LEN));
}

bool anvil_writer_numeric(anvil_writer w, const char *text) {
   if (!w || w->err != ANVIL_WRITER_OK) {
      return false;
   }
   if (!check(w, wg_check_numeric(text))) {
      return false;
   }
   return write_scalar(w, text, strlen(text));
}

bool anvil_writer_int(anvil_writer w, int64_t value) {
   char text[32];
   snprintf(text, sizeof text, "%lld", (long long)value);
   return anvil_writer_numeric(w, text);
}

bool anvil_writer_double(anvil_writer w, double value) {
   if (!w || w->err != ANVIL_WRITER_OK) {
      return false;
   }
   char text[40];
   if (!wg_format_double(value, text)) {
      return fail(w, ANVIL_WRITER_ERR_INVALID_NUMERIC);
   }
   return anvil_writer_numeric(w, text);
}

bool anvil_writer_string(anvil_writer w, const char *text, size_t length) {
   if (!w || w->err != ANVIL_WRITER_OK) {
      return false;
   }
   if (!check(w, wg_check_string(text, length))) {
      return false;
   }
   bool owns = false;
   if (!value_prelude(w, false, false, &owns) || !put_char(w, ANVL_TOK_QUOTE)) {
      return false;
   }
   size_t run = 0; // start of the pending run of bytes that need no escape
   for (size_t i = 0; i <= length; i++) {
      const char *escape = NULL;
      if (i < length) {
         switch (text[i]) {
         case ANVL_TOK_QUOTE:
            escape = "\\\"";
            break;
         case ANVL_TOK_ESCAPE:
            escape = "\\\\";
            break;
         case ANVL_TOK_NEWLINE:
            escape = "\\n";
            break;
         case ANVL_TOK_TAB:
            escape = "\\t";
            break;
         case ANVL_TOK_RETURN:
            escape = "\\r";
            break;
         default:
            break;
         }
      }
      if (i == length || escape) {
         if (i > run && !put(w, text + run, i - run)) {
            return false;
         }
         if (escape && !puts_(w, escape)) {
            return false;
         }
         run = i + 1;
      }
   }
   return put_char(w, ANVL_TOK_QUOTE) && value_complete(w, owns);
}

bool anvil_writer_bare(anvil_writer w, const char *text) {
   if (!w || w->err != ANVIL_WRITER_OK) {
      return false;
   }
   if (!check(w, wg_check_bare(text))) {
      return false;
   }
   return write_scalar(w, text, strlen(text));
}

bool anvil_writer_blob(anvil_writer w, const char *tag, const char *data, size_t length) {
   if (!w || w->err != ANVIL_WRITER_OK) {
      return false;
   }
   if (!check(w, wg_check_blob(tag, data, length))) {
      return false;
   }
   bool tagged = tag && *tag;
   bool owns = false;
   if (!value_prelude(w, false, false, &owns)) {
      return false;
   }
   if (tagged && (!put_char(w, ANVL_TOK_ATTRIB) || !puts_(w, tag))) {
      return false;
   }
   return put_char(w, ANVL_TOK_BACKTICK) && put(w, data, length) &&
          put_char(w, ANVL_TOK_BACKTICK) && value_complete(w, owns);
}

bool anvil_writer_varref(anvil_writer w, const char *name) {
   if (!w || w->err != ANVIL_WRITER_OK) {
      return false;
   }
   if (w->dialect == ANVIL_WRITER_AMP) {
      return fail(w, ANVIL_WRITER_ERR_DIALECT);
   }
   if (!check(w, wg_check_name(name))) {
      return false;
   }
   bool owns = false;
   return value_prelude(w, false, false, &owns) && put_char(w, '$') && puts_(w, name) &&
          value_complete(w, owns);
}

/* ----------------------------------------------------------------------- *
 * Collections
 * ----------------------------------------------------------------------- */
static bool begin_collection(anvil_writer w, frame_kind kind) {
   bool is_object = kind == FRAME_OBJECT;
   bool owns = false;
   if (!value_prelude(w, is_object, true, &owns)) {
      return false;
   }
   if (w->depth == w->stack_cap) {
      size_t cap = w->stack_cap ? w->stack_cap * 2 : 8;
      frame *grown = realloc(w->stack, cap * sizeof *grown);
      if (!grown) {
         return fail(w, ANVIL_WRITER_ERR_MEMORY);
      }
      w->stack = grown;
      w->stack_cap = cap;
   }
   const char *open = kind == FRAME_ARRAY ? "[" : kind == FRAME_TUPLE ? "(" : "{\n";
   if (!puts_(w, open)) {
      return false;
   }
   w->stack[w->depth++] = (frame){.kind = kind, .owns_stmt = owns};
   return true;
}

static bool end_collection(anvil_writer w, frame_kind kind) {
   if (!ready(w)) {
      return false;
   }
   if (w->depth == 0 || w->stack[w->depth - 1].kind != kind || w->stmt_open) {
      return fail(w, ANVIL_WRITER_ERR_STATE);
   }
   frame done = w->stack[w->depth - 1];
   if (done.count < (kind == FRAME_TUPLE ? 2u : 1u)) {
      return fail(w, ANVIL_WRITER_ERR_EMPTY_COLLECTION);
   }
   w->depth--;
   if (kind == FRAME_OBJECT) {
      if (!put_indent(w, w->depth) || !put_char(w, ANVL_TOK_RBRACE)) {
         return false;
      }
   } else {
      if (done.multiline && (!put_char(w, '\n') || !put_indent(w, w->depth))) {
         return false;
      }
      if (!put_char(w, kind == FRAME_ARRAY ? ANVL_TOK_RBRACKET : ANVL_TOK_RPAREN)) {
         return false;
      }
   }
   return value_complete(w, done.owns_stmt);
}

bool anvil_writer_begin_array(anvil_writer w) { return w && begin_collection(w, FRAME_ARRAY); }
bool anvil_writer_end_array(anvil_writer w) { return w && end_collection(w, FRAME_ARRAY); }
bool anvil_writer_begin_tuple(anvil_writer w) { return w && begin_collection(w, FRAME_TUPLE); }
bool anvil_writer_end_tuple(anvil_writer w) { return w && end_collection(w, FRAME_TUPLE); }
bool anvil_writer_begin_object(anvil_writer w) { return w && begin_collection(w, FRAME_OBJECT); }
bool anvil_writer_end_object(anvil_writer w) { return w && end_collection(w, FRAME_OBJECT); }

/* ----------------------------------------------------------------------- *
 * Finishing
 * ----------------------------------------------------------------------- */
bool anvil_writer_finish(anvil_writer w) {
   if (!w || w->err != ANVIL_WRITER_OK) {
      return false;
   }
   if (w->phase == PHASE_FINISHED) {
      return true;
   }
   if (w->depth > 0 || w->stmt_open) {
      return fail(w, ANVIL_WRITER_ERR_UNFINISHED);
   }
   w->phase = PHASE_FINISHED;
   return true;
}

const char *anvil_writer_data(anvil_writer w, size_t *length) {
   if (!w || w->err != ANVIL_WRITER_OK || w->phase != PHASE_FINISHED) {
      return NULL;
   }
   if (length) {
      *length = w->len;
   }
   return w->buf;
}
