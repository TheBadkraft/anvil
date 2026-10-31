/* script-common.c - the lexical rules shared by anvil-dump's writer and builder script modes (conformance/FORMAT.md,
 * "Lexical rules" of "The writer script": quoted strings, NULL, integers, doubles). */

#include "script-common.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const err_names[] = {
   "ANVIL_WRITER_OK",
   "ANVIL_WRITER_ERR_INVALID_ARGUMENT",
   "ANVIL_WRITER_ERR_MEMORY",
   "ANVIL_WRITER_ERR_STATE",
   "ANVIL_WRITER_ERR_DIALECT",
   "ANVIL_WRITER_ERR_INVALID_IDENTIFIER",
   "ANVIL_WRITER_ERR_RESERVED_WORD",
   "ANVIL_WRITER_ERR_INVALID_NUMERIC",
   "ANVIL_WRITER_ERR_INVALID_BARE",
   "ANVIL_WRITER_ERR_INVALID_BLOB",
   "ANVIL_WRITER_ERR_INVALID_ATTRIBUTE_VALUE",
   "ANVIL_WRITER_ERR_INVALID_INCLUDE_PATH",
   "ANVIL_WRITER_ERR_EMPTY_COLLECTION",
   "ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT",
   "ANVIL_WRITER_ERR_UNFINISHED",
   "ANVIL_WRITER_ERR_DUPLICATE_NAME",
   "ANVIL_WRITER_ERR_DEPTH_EXCEEDED",
   "ANVIL_WRITER_ERR_DUPLICATE_FIELD",
   "ANVIL_WRITER_ERR_DUPLICATE_ATTRIBUTE_KEY",
   "ANVIL_WRITER_ERR_INHERITANCE_CYCLE",
   "ANVIL_WRITER_ERR_MISSING_BASE",
};
#define ERR_NAME_COUNT (sizeof err_names / sizeof err_names[0])

const char *script_err_name(int code) {
   return code >= 0 && (size_t)code < ERR_NAME_COUNT ? err_names[code] : "ANVIL_WRITER_ERR_UNKNOWN";
}

bool script_blank(char c) { return c == ' ' || c == '\t'; }

int script_hexval(int c) {
   return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

// Parses one argument at *p (a quoted string or a word); advances *p. Returns NULL, or an error message.
const char *script_parse_arg(const char **p, script_arg *out) {
   const char *s = *p;
   if (*s == '"') {
      s++;
      size_t cap = strlen(s) + 1, n = 0;
      char *buf = malloc(cap);
      if (!buf) {
         return "out of memory";
      }
      for (;;) {
         unsigned char c = (unsigned char)*s;
         if (c == '\0') {
            free(buf);
            return "unterminated string";
         }
         s++;
         if (c == '"') {
            break;
         }
         if (c < 0x20) {
            free(buf);
            return "control byte in a string (write it as \\xHH)";
         }
         if (c == '\\') {
            char e = *s++;
            int h1, h2;
            switch (e) {
            case '\\': c = '\\'; break;
            case '"': c = '"'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'x':
               h1 = script_hexval((unsigned char)s[0]);
               h2 = h1 < 0 ? -1 : script_hexval((unsigned char)s[1]);
               if (h2 < 0) {
                  free(buf);
                  return "bad \\x escape";
               }
               c = (unsigned char)(h1 * 16 + h2);
               s += 2;
               break;
            default:
               free(buf);
               return "unknown escape";
            }
         }
         buf[n++] = (char)c;
      }
      buf[n] = '\0';
      if (*s && !script_blank(*s)) {
         free(buf);
         return "text after a closing quote";
      }
      *out = (script_arg){true, buf, n};
   } else {
      const char *start = s;
      while (*s && !script_blank(*s)) {
         if (*s == '"') {
            return "quote inside a word";
         }
         s++;
      }
      size_t n = (size_t)(s - start);
      char *buf = malloc(n + 1);
      if (!buf) {
         return "out of memory";
      }
      memcpy(buf, start, n);
      buf[n] = '\0';
      *out = (script_arg){false, buf, n};
   }
   *p = s;
   return NULL;
}

bool script_parse_int64(const char *s, int64_t *out) {
   if (*s != '-' && (*s < '0' || *s > '9')) {
      return false;
   }
   if (*s == '-' && (s[1] < '0' || s[1] > '9')) {
      return false;
   }
   errno = 0;
   char *end;
   long long v = strtoll(s, &end, 10);
   if (errno || *end) {
      return false;
   }
   *out = v;
   return true;
}

bool script_parse_double(const char *s, double *out) {
   if (strcmp(s, "nan") == 0) {
      *out = __builtin_nan("");
      return true;
   }
   if (strcmp(s, "inf") == 0 || strcmp(s, "-inf") == 0) {
      *out = *s == '-' ? -__builtin_inf() : __builtin_inf();
      return true;
   }
   if (s[0] == '0' && s[1] == 'x') {
      if (strlen(s) != 18) {
         return false;
      }
      uint64_t bits = 0;
      for (int i = 2; i < 18; i++) {
         int h = script_hexval((unsigned char)s[i]);
         if (h < 0) {
            return false;
         }
         bits = bits << 4 | (uint64_t)h;
      }
      memcpy(out, &bits, sizeof bits);
      return true;
   }
   for (const char *c = s; *c; c++) {
      if (!((*c >= '0' && *c <= '9') || *c == '.' || *c == 'e' || *c == 'E' || *c == '+' || *c == '-')) {
         return false;
      }
   }
   if (!*s) {
      return false;
   }
   char *end;
   *out = strtod(s, &end);
   return *end == '\0';
}

char *script_read_file(const char *path, size_t *len) {
   FILE *f = fopen(path, "rb");
   if (!f) {
      return NULL;
   }
   size_t cap = 4096, n = 0;
   char *buf = malloc(cap);
   size_t got;
   while (buf && (got = fread(buf + n, 1, cap - n, f)) > 0) {
      n += got;
      if (n == cap) {
         char *nb = realloc(buf, cap *= 2);
         if (!nb) {
            free(buf);
            buf = NULL;
         } else {
            buf = nb;
         }
      }
   }
   fclose(f);
   *len = n;
   return buf;
}

void script_print_data(const char *tag, const char *data, size_t n) {
   printf("%s bytes=%zu text=", tag, n);
   dump_put_quoted(data, n);
   putchar('\n');
}

