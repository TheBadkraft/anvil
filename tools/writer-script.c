/* ********************************************************************** *
 * writer-script - anvil-dump's writer mode: runs a writer script (the     *
 * format "anvil-writer-script 1" in conformance/FORMAT.md) through the    *
 * native writer API and prints the run ("anvil-writer-run 1"): every      *
 * failed call, and the exact output bytes.                                *
 * ********************************************************************** */

#include "writer-script.h"
#include "script-common.h"
#include "anvil_writer.h"
// ----------------
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUN_FORMAT_VERSION 1
#define SCRIPT_VERSION_LINE "anvil-writer-script 1"
#define MAX_ARGS 3

typedef script_arg arg;

typedef struct {
   size_t line;
   char *text; // the line, verbatim, trailing blanks removed
   int verb;
   arg args[MAX_ARGS];
   int argc;
} step;

enum { V_DIALECT, V_OPTION, V_GET_ERROR, V_ERROR_MESSAGE, V_ATTRIBUTE, V_ATTRIBUTE_STRING, V_INCLUDE, V_STATEMENT,
       V_NULL, V_BOOL, V_NUMERIC, V_INT, V_DOUBLE, V_STRING, V_BARE, V_BLOB, V_VARREF, V_BEGIN_ARRAY, V_END_ARRAY,
       V_BEGIN_TUPLE, V_END_TUPLE, V_BEGIN_OBJECT, V_END_OBJECT, V_FINISH, V_DATA, V_COUNT };

// Parameter kinds: P pointer or NULL; S bytes or NULL plus an optional length; I int64; B bool; D double; E dialect
// (AML, AMP or an integer); O option name and value.
static const struct {
   const char *name;
   const char *params;
   int min_args;
} verbs[V_COUNT] = {
   [V_DIALECT] = {"dialect", "E", 1},
   [V_OPTION] = {"option", "OO", 2},
   [V_GET_ERROR] = {"get_error", "", 0},
   [V_ERROR_MESSAGE] = {"error_message", "I", 1},
   [V_ATTRIBUTE] = {"attribute", "PP", 2},
   [V_ATTRIBUTE_STRING] = {"attribute_string", "PSI", 2},
   [V_INCLUDE] = {"include", "P", 1},
   [V_STATEMENT] = {"statement", "PP", 2},
   [V_NULL] = {"null", "", 0},
   [V_BOOL] = {"bool", "B", 1},
   [V_NUMERIC] = {"numeric", "P", 1},
   [V_INT] = {"int", "I", 1},
   [V_DOUBLE] = {"double", "D", 1},
   [V_STRING] = {"string", "SI", 1},
   [V_BARE] = {"bare", "P", 1},
   [V_BLOB] = {"blob", "PSI", 2},
   [V_VARREF] = {"varref", "P", 1},
   [V_BEGIN_ARRAY] = {"begin_array", "", 0},
   [V_END_ARRAY] = {"end_array", "", 0},
   [V_BEGIN_TUPLE] = {"begin_tuple", "", 0},
   [V_END_TUPLE] = {"end_tuple", "", 0},
   [V_BEGIN_OBJECT] = {"begin_object", "", 0},
   [V_END_OBJECT] = {"end_object", "", 0},
   [V_FINISH] = {"finish", "", 0},
   [V_DATA] = {"data", "", 0},
};

static const char *g_path;

static int script_error(size_t line, const char *what) {
   fprintf(stderr, "anvil-dump: %s:%zu: %s\n", g_path, line, what);
   return 2;
}

static void free_steps(step *steps, size_t n) {
   for (size_t i = 0; i < n; i++) {
      free(steps[i].text);
      for (int a = 0; a < steps[i].argc; a++) {
         free(steps[i].args[a].bytes);
      }
   }
   free(steps);
}

// Checks one parsed step against its verb's parameters. Returns NULL, or a message.
static const char *check_step(const step *st) {
   const char *params = verbs[st->verb].params;
   int max = (int)strlen(params);
   if (st->argc < verbs[st->verb].min_args || st->argc > max) {
      return "wrong number of arguments";
   }
   for (int i = 0; i < st->argc; i++) {
      const arg *a = &st->args[i];
      int64_t iv;
      double dv;
      switch (params[i]) {
      case 'P':
         if (a->quoted ? memchr(a->bytes, '\0', a->len) != NULL : strcmp(a->bytes, "NULL") != 0) {
            return a->quoted ? "NUL in a text argument" : "expected a quoted string or NULL";
         }
         break;
      case 'S':
         if (!a->quoted && strcmp(a->bytes, "NULL") != 0) {
            return "expected a quoted string or NULL";
         }
         break;
      case 'I':
         if (a->quoted || !script_parse_int64(a->bytes, &iv)) {
            return "expected a 64-bit integer";
         }
         if (i > 0 && params[i - 1] == 'S') {
            if (iv < 0 || (st->args[i - 1].quoted && (size_t)iv > st->args[i - 1].len)) {
               return "length out of range of the string";
            }
         }
         break;
      case 'B':
         if (a->quoted || (strcmp(a->bytes, "true") != 0 && strcmp(a->bytes, "false") != 0)) {
            return "expected true or false";
         }
         break;
      case 'D':
         if (a->quoted || !script_parse_double(a->bytes, &dv)) {
            return "expected a decimal, 0x + 16 hex digits, nan, inf or -inf";
         }
         break;
      case 'E':
         if (a->quoted || (strcmp(a->bytes, "AML") != 0 && strcmp(a->bytes, "AMP") != 0 && !script_parse_int64(a->bytes, &iv))) {
            return "expected AML, AMP or an integer";
         }
         break;
      case 'O':
         if (a->quoted) {
            return "expected a word";
         }
         break;
      }
   }
   // dialect and error_message take a C int
   if (st->verb == V_ERROR_MESSAGE || (st->verb == V_DIALECT && strcmp(st->args[0].bytes, "AML") != 0 && strcmp(st->args[0].bytes, "AMP") != 0)) {
      int64_t iv = 0;
      script_parse_int64(st->args[0].bytes, &iv);
      if (iv < INT32_MIN || iv > INT32_MAX) {
         return "expected a 32-bit integer";
      }
   }
   if (st->verb == V_OPTION) {
      if (strcmp(st->args[0].bytes, "on-error") != 0) {
         return "unknown option";
      }
      if (strcmp(st->args[1].bytes, "stop") != 0 && strcmp(st->args[1].bytes, "continue") != 0) {
         return "on-error is stop or continue";
      }
   }
   return NULL;
}

static int parse_script(const char *data, size_t size, step **out, size_t *count) {
   step *steps = NULL;
   size_t n = 0, cap = 0, lineno = 0;
   bool first = true, dialect_seen = false;
   const char *p = data, *end = data + size;
   while (p < end) {
      const char *nl = memchr(p, '\n', (size_t)(end - p));
      size_t llen = nl ? (size_t)(nl - p) : (size_t)(end - p);
      lineno++;
      char *line = malloc(llen + 1);
      if (!line) {
         free_steps(steps, n);
         return script_error(lineno, "out of memory");
      }
      memcpy(line, p, llen);
      line[llen] = '\0';
      p = nl ? nl + 1 : end;
      if (strlen(line) != llen) {
         free(line);
         free_steps(steps, n);
         return script_error(lineno, "NUL byte in a script line");
      }
      if (first) {
         first = false;
         if (strcmp(line, SCRIPT_VERSION_LINE) != 0) {
            free(line);
            free_steps(steps, n);
            return script_error(lineno, "first line must be \"" SCRIPT_VERSION_LINE "\"");
         }
         free(line);
         continue;
      }
      while (llen > 0 && script_blank(line[llen - 1])) {
         line[--llen] = '\0';
      }
      const char *s = line;
      while (script_blank(*s)) {
         s++;
      }
      if (*s == '\0' || *s == '#') {
         free(line);
         continue;
      }
      step st = {.line = lineno, .verb = -1};
      const char *vstart = s;
      while (*s && !script_blank(*s)) {
         s++;
      }
      size_t vlen = (size_t)(s - vstart);
      for (int v = 0; v < V_COUNT; v++) {
         if (strlen(verbs[v].name) == vlen && memcmp(verbs[v].name, vstart, vlen) == 0) {
            st.verb = v;
         }
      }
      const char *msg = st.verb < 0 ? "unknown call" : NULL;
      while (!msg) {
         while (script_blank(*s)) {
            s++;
         }
         if (!*s) {
            break;
         }
         if (st.argc == MAX_ARGS) {
            msg = "too many arguments";
            break;
         }
         msg = script_parse_arg(&s, &st.args[st.argc]);
         if (!msg) {
            st.argc++;
         }
      }
      if (!msg) {
         msg = check_step(&st);
      }
      if (!msg) {
         if (!dialect_seen && st.verb != V_DIALECT && st.verb != V_OPTION) {
            msg = "the first call must be dialect";
         } else if (dialect_seen && st.verb == V_DIALECT) {
            msg = "dialect appears once";
         }
         dialect_seen = dialect_seen || st.verb == V_DIALECT;
      }
      if (msg) {
         for (int a = 0; a < st.argc; a++) {
            free(st.args[a].bytes);
         }
         free(line);
         free_steps(steps, n);
         return script_error(lineno, msg);
      }
      memmove(line, vstart, strlen(vstart) + 1); // the call text: leading blanks gone (trailing ones were cut above)
      st.text = line;
      if (n == cap) {
         cap = cap ? cap * 2 : 64;
         step *grown = realloc(steps, cap * sizeof *grown);
         if (!grown) {
            free_steps(steps, n);
            return script_error(lineno, "out of memory");
         }
         steps = grown;
      }
      steps[n++] = st;
   }
   if (first) {
      free_steps(steps, n);
      return script_error(1, "empty script");
   }
   if (!dialect_seen) {
      free_steps(steps, n);
      return script_error(lineno, "no calls (the first call must be dialect)");
   }
   *out = steps;
   *count = n;
   return 0;
}

// The argument as a C pointer: NULL for the word NULL, else the string.
static const char *ptr_of(const step *st, int i) {
   return st->args[i].quoted ? st->args[i].bytes : NULL;
}

static size_t len_of(const step *st, int bytes_idx, int len_idx) {
   if (st->argc > len_idx) {
      return (size_t)strtoll(st->args[len_idx].bytes, NULL, 10);
   }
   return st->args[bytes_idx].quoted ? st->args[bytes_idx].len : 0;
}

int writer_script_run(const char *path, bool output_only) {
   g_path = path;
   size_t size = 0;
   char *data = script_read_file(path, &size);
   if (!data) {
      fprintf(stderr, "anvil-dump: cannot read %s\n", path);
      return 2;
   }
   step *steps = NULL;
   size_t count = 0;
   int rc = parse_script(data, size, &steps, &count);
   free(data);
   if (rc != 0) {
      return rc;
   }

   anvil_writer w = NULL;
   bool cont = false;
   size_t ran = 0;
   if (!output_only) {
      printf("anvil-writer-run %d\n", RUN_FORMAT_VERSION);
   }
   for (size_t i = 0; i < count; i++) {
      const step *st = &steps[i];
      bool ok = true;
      ran++;
      switch (st->verb) {
      case V_DIALECT: {
         int64_t d = 0;
         if (strcmp(st->args[0].bytes, "AML") == 0) {
            d = ANVIL_WRITER_AML;
         } else if (strcmp(st->args[0].bytes, "AMP") == 0) {
            d = ANVIL_WRITER_AMP;
         } else {
            script_parse_int64(st->args[0].bytes, &d);
         }
         w = anvil_writer_new((anvil_writer_dialect)d);
         ok = w != NULL;
         break;
      }
      case V_OPTION:
         cont = strcmp(st->args[1].bytes, "continue") == 0;
         break;
      case V_GET_ERROR: {
         int code = (int)anvil_writer_get_error(w);
         if (!output_only) {
            printf("get_error code=%d name=%s\n", code, script_err_name(code));
         }
         break;
      }
      case V_ERROR_MESSAGE: {
         int64_t code = 0;
         script_parse_int64(st->args[0].bytes, &code);
         if (!output_only) {
            printf("error_message code=%" PRId64 " text=", code);
            const char *m = anvil_writer_error_message((anvil_writer_err_code)code);
            dump_put_quoted(m, strlen(m));
            putchar('\n');
         }
         break;
      }
      case V_ATTRIBUTE: ok = anvil_writer_attribute(w, ptr_of(st, 0), ptr_of(st, 1)); break;
      case V_ATTRIBUTE_STRING:
         ok = anvil_writer_attribute_string(w, ptr_of(st, 0), ptr_of(st, 1), len_of(st, 1, 2));
         break;
      case V_INCLUDE: ok = anvil_writer_include(w, ptr_of(st, 0)); break;
      case V_STATEMENT: ok = anvil_writer_statement(w, ptr_of(st, 0), ptr_of(st, 1)); break;
      case V_NULL: ok = anvil_writer_null(w); break;
      case V_BOOL: ok = anvil_writer_bool(w, strcmp(st->args[0].bytes, "true") == 0); break;
      case V_NUMERIC: ok = anvil_writer_numeric(w, ptr_of(st, 0)); break;
      case V_INT: {
         int64_t v = 0;
         script_parse_int64(st->args[0].bytes, &v);
         ok = anvil_writer_int(w, v);
         break;
      }
      case V_DOUBLE: {
         double v = 0;
         script_parse_double(st->args[0].bytes, &v);
         ok = anvil_writer_double(w, v);
         break;
      }
      case V_STRING: ok = anvil_writer_string(w, ptr_of(st, 0), len_of(st, 0, 1)); break;
      case V_BARE: ok = anvil_writer_bare(w, ptr_of(st, 0)); break;
      case V_BLOB: ok = anvil_writer_blob(w, ptr_of(st, 0), ptr_of(st, 1), len_of(st, 1, 2)); break;
      case V_VARREF: ok = anvil_writer_varref(w, ptr_of(st, 0)); break;
      case V_BEGIN_ARRAY: ok = anvil_writer_begin_array(w); break;
      case V_END_ARRAY: ok = anvil_writer_end_array(w); break;
      case V_BEGIN_TUPLE: ok = anvil_writer_begin_tuple(w); break;
      case V_END_TUPLE: ok = anvil_writer_end_tuple(w); break;
      case V_BEGIN_OBJECT: ok = anvil_writer_begin_object(w); break;
      case V_END_OBJECT: ok = anvil_writer_end_object(w); break;
      case V_FINISH: ok = anvil_writer_finish(w); break;
      case V_DATA: {
         size_t n = 0;
         const char *d = anvil_writer_data(w, &n);
         if (!output_only) {
            if (d) {
               script_print_data("data", d, n);
            } else {
               puts("data none");
            }
         }
         break;
      }
      }
      if (!ok) {
         int code = (int)anvil_writer_get_error(w);
         if (!output_only) {
            printf("failed line=%zu code=%d name=%s call=", st->line, code, script_err_name(code));
            dump_put_quoted(st->text, strlen(st->text));
            const char *m = anvil_writer_error_message((anvil_writer_err_code)code);
            printf(" message=");
            dump_put_quoted(m, strlen(m));
            putchar('\n');
         }
         if (!cont) {
            break;
         }
      }
   }

   size_t outlen = 0;
   const char *out = anvil_writer_data(w, &outlen);
   int status = 0;
   if (output_only) {
      if (out) {
         fwrite(out, 1, outlen, stdout);
      } else {
         status = 1;
      }
   } else {
      int code = (int)anvil_writer_get_error(w);
      printf("steps %zu\n", ran);
      if (code == 0) {
         puts("status ok");
      } else {
         printf("status error code=%d name=%s\n", code, script_err_name(code));
      }
      if (out) {
         script_print_data("output", out, outlen);
      } else {
         puts("output none");
      }
   }
   anvil_writer_dispose(w);
   free_steps(steps, count);
   return status;
}
