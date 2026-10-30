/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * grammar.c - Internal: ANVL grammar rules shared by the writer/builder  *
 * ********************************************************************** */

#include "grammar.h"
#include "constants.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOB_TAG_MAX 31

static bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_ident_start(char c) { return is_alpha(c) || c == '_'; }
static bool is_ident_part(char c) { return is_alpha(c) || is_digit(c) || c == '_'; }
static bool is_bare_part(char c) {
   return is_ident_part(c) || c == '-' || c == ANVL_TOK_DOT || c == '/' || c == ':' || c == '$';
}

static bool valid_identifier(const char *s, size_t max_len) {
   if (!is_ident_start(*s)) {
      return false;
   }
   size_t n = 1;
   while (is_ident_part(s[n])) {
      n++;
   }
   return s[n] == '\0' && (max_len == 0 || n <= max_len);
}

static bool is_keyword(const char *s) {
   return strcmp(s, ANVL_KEYWORD_INCLUDE) == 0 || strcmp(s, ANVL_KEYWORD_IMPORT) == 0 ||
          strcmp(s, ANVL_KEYWORD_VARS) == 0 || strcmp(s, ANVL_KEYWORD_TRUE) == 0 ||
          strcmp(s, ANVL_KEYWORD_FALSE) == 0 || strcmp(s, ANVL_KEYWORD_NULL) == 0;
}

anvil_writer_err_code wg_check_name(const char *s) {
   if (!s) {
      return ANVIL_WRITER_ERR_INVALID_ARGUMENT;
   }
   if (!valid_identifier(s, 0)) {
      return ANVIL_WRITER_ERR_INVALID_IDENTIFIER;
   }
   return is_keyword(s) ? ANVIL_WRITER_ERR_RESERVED_WORD : ANVIL_WRITER_OK;
}

anvil_writer_err_code wg_check_attribute_key(const char *s) {
   if (!s) {
      return ANVIL_WRITER_ERR_INVALID_ARGUMENT;
   }
   if (!valid_identifier(s, 0)) {
      return ANVIL_WRITER_ERR_INVALID_IDENTIFIER;
   }
   // a keyword is no more an attribute name than a statement name (the reader's 4004)
   return is_keyword(s) ? ANVIL_WRITER_ERR_RESERVED_WORD : ANVIL_WRITER_OK;
}

// Attribute values are raw text ended by ',' or ']' outside double quotes (parse_attribute_list).
// A value is a scalar: one starting with '[', '(' or '{' is an array, tuple or object, and one starting
// with a backtick or '@' is a blob (attributes stay small); the reader
// refuses (2006) every one of them wherever a ',' or ']' falls.
anvil_writer_err_code wg_check_attribute_value(const char *v) {
   if (!v) {
      return ANVIL_WRITER_ERR_INVALID_ARGUMENT;
   }
   size_t n = strlen(v);
   if (n == 0 || v[0] == '$' || v[0] == '[' || v[0] == '(' || v[0] == '{' || v[0] == '`' || v[0] == '@' || v[0] == ' ' || v[0] == '\t' || v[n - 1] == ' ' || v[n - 1] == '\t') {
      return ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE;
   }
   bool in_string = false;
   for (size_t i = 0; i < n; i++) {
      char c = v[i];
      if (c == '\n' || c == '\r') {
         return ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE;
      }
      if (c == ANVL_TOK_QUOTE) {
         in_string = !in_string;
      } else if (!in_string && (c == ',' || c == ANVL_TOK_RBRACKET)) {
         return ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE;
      }
   }
   return in_string ? ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE : ANVIL_WRITER_OK;
}

anvil_writer_err_code wg_check_attribute_text(const char *text, size_t length) {
   if (!text && length > 0) {
      return ANVIL_WRITER_ERR_INVALID_ARGUMENT;
   }
   for (size_t i = 0; i < length; i++) {
      if (text[i] == ANVL_TOK_QUOTE || text[i] == '\n' || text[i] == '\r') {
         return ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE;
      }
   }
   return ANVIL_WRITER_OK;
}

anvil_writer_err_code wg_check_include_path(const char *path) {
   if (!path) {
      return ANVIL_WRITER_ERR_INVALID_ARGUMENT;
   }
   return *path == '\0' || strpbrk(path, "\"\n\r") ? ANVIL_WRITER_ERR_INVALID_INCLUDE_PATH
                                                   : ANVIL_WRITER_OK;
}

// -?digits(.digits)?([eE][+-]?digits)? - the whole string, nothing else.
static bool is_numeric_text(const char *s) {
   if (*s == '-') {
      s++;
   }
   if (!is_digit(*s)) {
      return false;
   }
   while (is_digit(*s)) {
      s++;
   }
   if (*s == '.') {
      s++;
      if (!is_digit(*s)) {
         return false;
      }
      while (is_digit(*s)) {
         s++;
      }
   }
   if (*s == 'e' || *s == 'E') {
      s++;
      if (*s == '+' || *s == '-') {
         s++;
      }
      if (!is_digit(*s)) {
         return false;
      }
      while (is_digit(*s)) {
         s++;
      }
   }
   return *s == '\0';
}

anvil_writer_err_code wg_check_numeric(const char *text) {
   if (!text) {
      return ANVIL_WRITER_ERR_INVALID_ARGUMENT;
   }
   return is_numeric_text(text) ? ANVIL_WRITER_OK : ANVIL_WRITER_ERR_INVALID_NUMERIC;
}

// Whether the reader would take a digit-led token as a number (or fail on it) rather than fall
// back to a bare literal. Deliberately looser than is_numeric_text: parse_numeric_literal errors
// on a malformed number (`1.`, `1.e3`, `1e`, `1e+`), so such a token can never be written as a
// bare literal and read back as one.
static bool reader_takes_as_number(const char *s) {
   if (!is_digit(*s)) {
      return false;
   }
   while (is_digit(*s)) {
      s++;
   }
   if (*s == '.') {
      s++;
      if (!is_digit(*s)) {
         // `1.` and `1.e3` are reader errors; `1.x` declines to a bare literal.
         return *s == '\0' || ((*s == 'e' || *s == 'E') &&
                               (is_digit(s[1]) || s[1] == '+' || s[1] == '-' || s[1] == '\0'));
      }
      while (is_digit(*s)) {
         s++;
      }
      if (*s == '.') {
         return false; // a second decimal point makes the reader decline the number
      }
   }
   if (*s == 'e' || *s == 'E') {
      const char *e = s + 1;
      if (*e == '+' || *e == '-') {
         if (!is_digit(e[1])) {
            return true; // malformed exponent: the reader errors rather than declining
         }
         e++;
      } else if (*e == '\0') {
         return true; // a trailing `e`: the reader errors
      }
      if (is_digit(*e)) {
         while (is_digit(*e)) {
            e++;
         }
         s = e;
      }
   }
   return *s == '\0'; // anything left over means the reader declines and reads it as bare
}

anvil_writer_err_code wg_check_bare(const char *text) {
   if (!text) {
      return ANVIL_WRITER_ERR_INVALID_ARGUMENT;
   }
   char first = text[0];
   bool valid_start = is_ident_start(first) || first == ANVL_TOK_DOT || first == '/' || is_digit(first);
   if (!valid_start) {
      return ANVIL_WRITER_ERR_INVALID_BARE;
   }
   for (const char *p = text + 1; *p; p++) {
      if (!is_bare_part(*p)) {
         return ANVIL_WRITER_ERR_INVALID_BARE;
      }
   }
   if (is_keyword(text)) {
      return ANVIL_WRITER_ERR_RESERVED_WORD;
   }
   // Read back as a different kind, or as a comment.
   if (reader_takes_as_number(text) || (first == '/' && (text[1] == '/' || text[1] == '*'))) {
      return ANVIL_WRITER_ERR_INVALID_BARE;
   }
   return ANVIL_WRITER_OK;
}

anvil_writer_err_code wg_check_blob(const char *tag, const char *data, size_t length) {
   if (!data && length > 0) {
      return ANVIL_WRITER_ERR_INVALID_ARGUMENT;
   }
   bool tagged = tag && *tag;
   if ((tagged && !valid_identifier(tag, BLOB_TAG_MAX)) ||
       (length > 0 && memchr(data, ANVL_TOK_BACKTICK, length))) {
      return ANVIL_WRITER_ERR_INVALID_BLOB;
   }
   return ANVIL_WRITER_OK;
}

anvil_writer_err_code wg_check_string(const char *text, size_t length) {
   return !text && length > 0 ? ANVIL_WRITER_ERR_INVALID_ARGUMENT : ANVIL_WRITER_OK;
}

bool wg_format_double(double value, char out[40]) {
   if (!isfinite(value)) {
      return false;
   }
   for (int precision = 15; precision <= 17; precision++) {
      snprintf(out, 40, "%.*g", precision, value);
      if (strtod(out, NULL) == value) {
         break;
      }
   }
   return true;
}

/* ----------------------------------------------------------------------- *
 * Name set: open addressing, FNV-1a, grown at 50% load
 * ----------------------------------------------------------------------- */
// True 64-bit FNV-1a on every target (same constants as src/sigma/map.c). It used to accumulate in
// a size_t, which silently truncated both constants on 32-bit targets (wasm32, 32-bit ARM, -m32):
// BR-2610-anvl-writer-001. Folded to size_t only where it indexes the table.
#define FNV1A_OFFSET UINT64_C(14695981039346656037)
#define FNV1A_PRIME UINT64_C(1099511628211)
_Static_assert(sizeof(uint64_t) == 8, "the name hash needs a 64-bit accumulator");

uint64_t wg_hash_name(const char *s) {
   uint64_t h = FNV1A_OFFSET;
   for (; *s; s++) {
      h ^= (unsigned char)*s;
      h *= FNV1A_PRIME;
   }
   return h;
}

// A slot index: the low bits of the hash. capacity is a power of two, so the mask is < capacity.
static size_t slot_for(const char *name, size_t capacity) {
   return (size_t)(wg_hash_name(name) & (uint64_t)(capacity - 1));
}

static bool nameset_grow(wg_nameset *set) {
   size_t capacity = set->capacity ? set->capacity * 2 : 64;
   char **slots = calloc(capacity, sizeof *slots);
   if (!slots) {
      return false;
   }
   for (size_t i = 0; i < set->capacity; i++) {
      if (set->slots[i]) {
         size_t at = slot_for(set->slots[i], capacity);
         while (slots[at]) {
            at = (at + 1) & (capacity - 1);
         }
         slots[at] = set->slots[i];
      }
   }
   free(set->slots);
   set->slots = slots;
   set->capacity = capacity;
   return true;
}

int wg_nameset_add(wg_nameset *set, const char *name) {
   if (set->count * 2 >= set->capacity && !nameset_grow(set)) {
      return -1;
   }
   size_t at = slot_for(name, set->capacity);
   while (set->slots[at]) {
      if (strcmp(set->slots[at], name) == 0) {
         return 0;
      }
      at = (at + 1) & (set->capacity - 1);
   }
   char *copy = strdup(name);
   if (!copy) {
      return -1;
   }
   set->slots[at] = copy;
   set->count++;
   return 1;
}

void wg_nameset_free(wg_nameset *set) {
   for (size_t i = 0; i < set->capacity; i++) {
      free(set->slots[i]);
   }
   free(set->slots);
   *set = (wg_nameset){0};
}
