/* ********************************************************************** *
 * builder-script - anvil-dump's builder mode: runs a builder script (the  *
 * format "anvil-builder-script 1" in conformance/FORMAT.md) through the   *
 * native builder API and prints the run ("anvil-builder-run 1"): every    *
 * failed call, every find result, the exact bytes of every emit and       *
 * write, and the final status.                                            *
 * ********************************************************************** */

#include "builder-script.h"
#include "script-common.h"
#include "anvil_builder.h"
#include "anvil_writer.h"
// ----------------
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUN_FORMAT_VERSION 1
#define SCRIPT_VERSION_LINE "anvil-builder-script 1"
#define MAX_ARGS 4
#define MAX_TOKENS (MAX_ARGS + 6)

typedef script_arg arg;

typedef struct {
   size_t line;
   char *text; // the line, verbatim, trailing blanks removed
   int verb;
   bool other;   // made on the second builder
   int let_slot; // handle slot bound to the result, or -1
   arg args[MAX_ARGS];
   int slot[MAX_ARGS]; // for a handle argument: its slot, or -1 for NULL
   int argc;
} step;

enum { V_DIALECT, V_BUILDER2, V_OPTION, V_GET_ERROR, V_ERROR_MESSAGE, V_ATTRIBUTE, V_ATTRIBUTE_STRING, V_INCLUDE,
       V_NULL, V_BOOL, V_NUMERIC, V_INT, V_DOUBLE, V_STRING, V_BARE, V_BLOB, V_VARREF, V_ARRAY, V_TUPLE, V_OBJECT,
       V_APPEND, V_ADD, V_SET_BASE, V_MEMBER_ATTRIBUTE, V_MEMBER_ATTRIBUTE_STRING, V_FIND, V_EMIT, V_WRITE, V_COUNT };

// Parameter kinds: P pointer or NULL; S bytes or NULL plus an optional length; I int64; B bool; D double; E dialect
// (AML, AMP or an integer); O option name and value; N node handle; M member handle; W writer dialect (AML, AMP, NULL).
// `ret` is the handle kind the call returns: 'N' a node, 'M' a member, 0 nothing.
static const struct {
   const char *name;
   const char *params;
   int min_args;
   char ret;
} verbs[V_COUNT] = {
   [V_DIALECT] = {"dialect", "E", 1, 0},
   [V_BUILDER2] = {"builder2", "E", 1, 0},
   [V_OPTION] = {"option", "OO", 2, 0},
   [V_GET_ERROR] = {"get_error", "", 0, 0},
   [V_ERROR_MESSAGE] = {"error_message", "I", 1, 0},
   [V_ATTRIBUTE] = {"attribute", "PP", 2, 0},
   [V_ATTRIBUTE_STRING] = {"attribute_string", "PSI", 2, 0},
   [V_INCLUDE] = {"include", "P", 1, 0},
   [V_NULL] = {"null", "", 0, 'N'},
   [V_BOOL] = {"bool", "B", 1, 'N'},
   [V_NUMERIC] = {"numeric", "P", 1, 'N'},
   [V_INT] = {"int", "I", 1, 'N'},
   [V_DOUBLE] = {"double", "D", 1, 'N'},
   [V_STRING] = {"string", "SI", 1, 'N'},
   [V_BARE] = {"bare", "P", 1, 'N'},
   [V_BLOB] = {"blob", "PSI", 2, 'N'},
   [V_VARREF] = {"varref", "P", 1, 'N'},
   [V_ARRAY] = {"array", "", 0, 'N'},
   [V_TUPLE] = {"tuple", "", 0, 'N'},
   [V_OBJECT] = {"object", "", 0, 'N'},
   [V_APPEND] = {"append", "NN", 2, 0},
   [V_ADD] = {"add", "NPN", 3, 'M'},
   [V_SET_BASE] = {"set_base", "MP", 2, 0},
   [V_MEMBER_ATTRIBUTE] = {"member_attribute", "MPP", 3, 0},
   [V_MEMBER_ATTRIBUTE_STRING] = {"member_attribute_string", "MPSI", 3, 0},
   [V_FIND] = {"find", "NP", 2, 'M'},
   [V_EMIT] = {"emit", "", 0, 0},
   [V_WRITE] = {"write", "W", 0, 0},
};

// The handle names bound so far, and the kind each holds (static: the kind of the latest `let`).
typedef struct {
   char **name;
   char *kind;
   size_t count, cap;
} handle_table;

static const char *g_path;

static int script_error(size_t line, const char *what) {
   fprintf(stderr, "anvil-dump: %s:%zu: %s\n", g_path, line, what);
   return 2;
}

static void free_step(step *st) {
   free(st->text);
   for (int a = 0; a < st->argc; a++) {
      free(st->args[a].bytes);
   }
}

static void free_steps(step *steps, size_t n) {
   for (size_t i = 0; i < n; i++) {
      free_step(&steps[i]);
   }
   free(steps);
}

static void free_handles(handle_table *h) {
   for (size_t i = 0; i < h->count; i++) {
      free(h->name[i]);
   }
   free(h->name);
   free(h->kind);
}

static bool name_char(char c, bool first) {
   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (!first && c >= '0' && c <= '9');
}

static bool valid_name(const char *s) {
   if (!*s || !name_char(*s, true)) {
      return false;
   }
   for (s++; *s; s++) {
      if (!name_char(*s, false)) {
         return false;
      }
   }
   return true;
}

static int find_handle(const handle_table *h, const char *name) {
   for (size_t i = 0; i < h->count; i++) {
      if (strcmp(h->name[i], name) == 0) {
         return (int)i;
      }
   }
   return -1;
}

// Binds `name` to a handle of `kind`; returns its slot, or -1 when out of memory.
static int bind_handle(handle_table *h, const char *name, char kind) {
   int slot = find_handle(h, name);
   if (slot < 0) {
      if (h->count == h->cap) {
         size_t cap = h->cap ? h->cap * 2 : 16;
         char **names = realloc(h->name, cap * sizeof *names);
         if (!names) {
            return -1;
         }
         h->name = names;
         char *kinds = realloc(h->kind, cap);
         if (!kinds) {
            return -1;
         }
         h->kind = kinds;
         h->cap = cap;
      }
      char *copy = strdup(name);
      if (!copy) {
         return -1;
      }
      slot = (int)h->count++;
      h->name[slot] = copy;
   }
   h->kind[slot] = kind;
   return slot;
}

static bool is_word(const arg *a, const char *w) { return !a->quoted && strcmp(a->bytes, w) == 0; }

// Checks one parsed step's arguments against its verb's parameters. Returns NULL, or a message.
static const char *check_step(step *st, const handle_table *handles) {
   const char *params = verbs[st->verb].params;
   int max = (int)strlen(params);
   if (st->argc < verbs[st->verb].min_args || st->argc > max) {
      return "wrong number of arguments";
   }
   for (int i = 0; i < st->argc; i++) {
      const arg *a = &st->args[i];
      int64_t iv;
      double dv;
      st->slot[i] = -1;
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
      case 'W':
         if (!is_word(a, "AML") && !is_word(a, "AMP") && !is_word(a, "NULL")) {
            return "expected AML, AMP or NULL";
         }
         break;
      case 'O':
         if (a->quoted) {
            return "expected a word";
         }
         break;
      case 'N':
      case 'M':
         if (a->quoted) {
            return "expected a handle name or NULL";
         }
         if (strcmp(a->bytes, "NULL") != 0) {
            int slot = valid_name(a->bytes) ? find_handle(handles, a->bytes) : -1;
            if (slot < 0) {
               return "unknown handle";
            }
            if (handles->kind[slot] != params[i]) {
               return "wrong kind of handle";
            }
            st->slot[i] = slot;
         }
         break;
      }
   }
   // dialect, builder2 and error_message take a C int
   if (st->verb == V_ERROR_MESSAGE || ((st->verb == V_DIALECT || st->verb == V_BUILDER2) && !is_word(&st->args[0], "AML") && !is_word(&st->args[0], "AMP"))) {
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

// Splits `s` into tokens. Returns NULL, or a message. *n is the count; on an error the tokens read are in `tok`.
static const char *tokenize(const char *s, arg *tok, int *n) {
   *n = 0;
   for (;;) {
      while (script_blank(*s)) {
         s++;
      }
      if (!*s) {
         return NULL;
      }
      if (*n == MAX_TOKENS) {
         return "too many arguments";
      }
      const char *msg = script_parse_arg(&s, &tok[*n]);
      if (msg) {
         return msg;
      }
      (*n)++;
   }
}

static int parse_script(const char *data, size_t size, step **out, size_t *count, handle_table *handles) {
   step *steps = NULL;
   size_t n = 0, cap = 0, lineno = 0;
   bool first = true, dialect_seen = false, builder2_seen = false;
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
      step st = {.line = lineno, .verb = -1, .let_slot = -1};
      arg tok[MAX_TOKENS] = {0};
      int ntok = 0, t = 0;
      const char *vstart = s;
      const char *msg = tokenize(s, tok, &ntok);
      const char *let_name = NULL;
      if (!msg && ntok > 0 && is_word(&tok[0], "let")) {
         if (ntok < 4 || !is_word(&tok[2], "=")) {
            msg = "expected let NAME = CALL";
         } else if (tok[1].quoted || !valid_name(tok[1].bytes) || strcmp(tok[1].bytes, "NULL") == 0 || strcmp(tok[1].bytes, "true") == 0 ||
                    strcmp(tok[1].bytes, "false") == 0 || strcmp(tok[1].bytes, "let") == 0 || strcmp(tok[1].bytes, "other") == 0) {
            msg = "bad handle name";
         } else {
            let_name = tok[1].bytes;
            t = 3;
         }
      }
      if (!msg && t < ntok && is_word(&tok[t], "other")) {
         st.other = true;
         t++;
      }
      if (!msg) {
         if (t >= ntok || tok[t].quoted) {
            msg = "unknown call";
         } else {
            for (int v = 0; v < V_COUNT; v++) {
               if (strcmp(verbs[v].name, tok[t].bytes) == 0) {
                  st.verb = v;
               }
            }
            msg = st.verb < 0 ? "unknown call" : NULL;
         }
      }
      if (!msg) {
         st.argc = ntok - t - 1;
         if (st.argc > MAX_ARGS) {
            msg = "too many arguments";
         } else {
            for (int a = 0; a < st.argc; a++) {
               st.args[a] = tok[t + 1 + a]; // ownership moves to the step
               tok[t + 1 + a].bytes = NULL;
            }
         }
      }
      if (!msg && let_name && !verbs[st.verb].ret) {
         msg = "this call returns nothing to bind";
      }
      if (!msg && st.other && (st.verb == V_DIALECT || st.verb == V_BUILDER2 || st.verb == V_OPTION || st.verb == V_ERROR_MESSAGE)) {
         msg = "other does not apply to this call";
      }
      if (!msg) {
         msg = check_step(&st, handles);
      }
      if (!msg) {
         if (!dialect_seen && st.verb != V_DIALECT && st.verb != V_OPTION) {
            msg = "the first call must be dialect";
         } else if (dialect_seen && st.verb == V_DIALECT) {
            msg = "dialect appears once";
         } else if (st.verb == V_BUILDER2 && builder2_seen) {
            msg = "builder2 appears once";
         } else if (st.other && !builder2_seen) {
            msg = "other needs builder2 first";
         }
      }
      if (!msg && let_name) {
         st.let_slot = bind_handle(handles, let_name, verbs[st.verb].ret);
         if (st.let_slot < 0) {
            msg = "out of memory";
         }
      }
      if (msg) {
         for (int i = 0; i < ntok; i++) {
            free(tok[i].bytes);
         }
         free_step(&st);
         free(line);
         free_steps(steps, n);
         return script_error(lineno, msg);
      }
      dialect_seen = dialect_seen || st.verb == V_DIALECT;
      builder2_seen = builder2_seen || st.verb == V_BUILDER2;
      for (int i = 0; i < ntok; i++) {
         free(tok[i].bytes); // the ones not moved into the step
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
static const char *ptr_of(const step *st, int i) { return st->args[i].quoted ? st->args[i].bytes : NULL; }

static size_t len_of(const step *st, int bytes_idx, int len_idx) {
   if (st->argc > len_idx) {
      return (size_t)strtoll(st->args[len_idx].bytes, NULL, 10);
   }
   return st->args[bytes_idx].quoted ? st->args[bytes_idx].len : 0;
}

static int save_emit(const char *dir, int k, const char *data, size_t n) {
   char path[4096];
   if (snprintf(path, sizeof path, "%s/emit-%d.anvl", dir, k) >= (int)sizeof path) {
      fprintf(stderr, "anvil-dump: emit directory path too long\n");
      return 2;
   }
   FILE *f = fopen(path, "wb");
   if (!f || (n && fwrite(data, 1, n, f) != n)) {
      fprintf(stderr, "anvil-dump: cannot write %s\n", path);
      if (f) {
         fclose(f);
      }
      return 2;
   }
   fclose(f);
   return 0;
}

static void print_failed(const step *st, int code) {
   printf("failed line=%zu code=%d name=%s call=", st->line, code, script_err_name(code));
   dump_put_quoted(st->text, strlen(st->text));
   const char *m = anvil_writer_error_message((anvil_writer_err_code)code);
   printf(" message=");
   dump_put_quoted(m, strlen(m));
   putchar('\n');
}

static int dialect_of(const char *word) {
   int64_t d = 0;
   if (strcmp(word, "AML") == 0) {
      return ANVIL_WRITER_AML;
   }
   if (strcmp(word, "AMP") == 0) {
      return ANVIL_WRITER_AMP;
   }
   script_parse_int64(word, &d);
   return (int)d;
}

int builder_script_run(const char *path, const char *emit_dir) {
   g_path = path;
   size_t size = 0;
   char *data = script_read_file(path, &size);
   if (!data) {
      fprintf(stderr, "anvil-dump: cannot read %s\n", path);
      return 2;
   }
   step *steps = NULL;
   size_t count = 0;
   handle_table handles = {0};
   int rc = parse_script(data, size, &steps, &count, &handles);
   free(data);
   if (rc != 0) {
      free_handles(&handles);
      return rc;
   }

   void **values = calloc(handles.count ? handles.count : 1, sizeof *values);
   anvil_builder builders[2] = {NULL, NULL};
   int dialects[2] = {ANVIL_WRITER_AML, ANVIL_WRITER_AML};
   bool cont = false;
   size_t ran = 0;
   int emits = 0;
   printf("anvil-builder-run %d\n", RUN_FORMAT_VERSION);
   for (size_t i = 0; i < count && values; i++) {
      const step *st = &steps[i];
      int bi = st->other ? 1 : 0;
      anvil_builder b = builders[bi];
      bool ok = true;
      void *result = NULL;
      int fail_code = -1; // set when the failure code is not the builder's
      ran++;
      switch (st->verb) {
      case V_DIALECT:
      case V_BUILDER2: {
         int d = dialect_of(st->args[0].bytes);
         int which = st->verb == V_DIALECT ? 0 : 1;
         builders[which] = anvil_builder_new((anvil_writer_dialect)d);
         dialects[which] = builders[which] ? d : ANVIL_WRITER_AML;
         ok = builders[which] != NULL;
         b = builders[which];
         break;
      }
      case V_OPTION: cont = strcmp(st->args[1].bytes, "continue") == 0; break;
      case V_GET_ERROR: {
         int code = (int)anvil_builder_get_error(b);
         printf("get_error code=%d name=%s\n", code, script_err_name(code));
         break;
      }
      case V_ERROR_MESSAGE: {
         int64_t code = 0;
         script_parse_int64(st->args[0].bytes, &code);
         printf("error_message code=%" PRId64 " text=", code);
         const char *m = anvil_writer_error_message((anvil_writer_err_code)code);
         dump_put_quoted(m, strlen(m));
         putchar('\n');
         break;
      }
      case V_ATTRIBUTE: ok = anvil_builder_attribute(b, ptr_of(st, 0), ptr_of(st, 1)); break;
      case V_ATTRIBUTE_STRING: ok = anvil_builder_attribute_string(b, ptr_of(st, 0), ptr_of(st, 1), len_of(st, 1, 2)); break;
      case V_INCLUDE: ok = anvil_builder_include(b, ptr_of(st, 0)); break;
      case V_NULL: result = anvil_builder_null(b); break;
      case V_BOOL: result = anvil_builder_bool(b, strcmp(st->args[0].bytes, "true") == 0); break;
      case V_NUMERIC: result = anvil_builder_numeric(b, ptr_of(st, 0)); break;
      case V_INT: {
         int64_t v = 0;
         script_parse_int64(st->args[0].bytes, &v);
         result = anvil_builder_int(b, v);
         break;
      }
      case V_DOUBLE: {
         double v = 0;
         script_parse_double(st->args[0].bytes, &v);
         result = anvil_builder_double(b, v);
         break;
      }
      case V_STRING: result = anvil_builder_string(b, ptr_of(st, 0), len_of(st, 0, 1)); break;
      case V_BARE: result = anvil_builder_bare(b, ptr_of(st, 0)); break;
      case V_BLOB: result = anvil_builder_blob(b, ptr_of(st, 0), ptr_of(st, 1), len_of(st, 1, 2)); break;
      case V_VARREF: result = anvil_builder_varref(b, ptr_of(st, 0)); break;
      case V_ARRAY: result = anvil_builder_array(b); break;
      case V_TUPLE: result = anvil_builder_tuple(b); break;
      case V_OBJECT: result = anvil_builder_object(b); break;
      case V_APPEND:
         ok = anvil_builder_append(b, st->slot[0] < 0 ? NULL : values[st->slot[0]], st->slot[1] < 0 ? NULL : values[st->slot[1]]);
         break;
      case V_ADD:
         result = anvil_builder_add(b, st->slot[0] < 0 ? NULL : values[st->slot[0]], ptr_of(st, 1), st->slot[2] < 0 ? NULL : values[st->slot[2]]);
         break;
      case V_SET_BASE: ok = anvil_builder_set_base(b, st->slot[0] < 0 ? NULL : values[st->slot[0]], ptr_of(st, 1)); break;
      case V_MEMBER_ATTRIBUTE:
         ok = anvil_builder_member_attribute(b, st->slot[0] < 0 ? NULL : values[st->slot[0]], ptr_of(st, 1), ptr_of(st, 2));
         break;
      case V_MEMBER_ATTRIBUTE_STRING:
         ok = anvil_builder_member_attribute_string(b, st->slot[0] < 0 ? NULL : values[st->slot[0]], ptr_of(st, 1), ptr_of(st, 2), len_of(st, 2, 3));
         break;
      case V_FIND: {
         result = anvil_builder_find(b, st->slot[0] < 0 ? NULL : values[st->slot[0]], ptr_of(st, 1));
         printf("find line=%zu result=%s\n", st->line, result ? "found" : "none");
         break;
      }
      case V_EMIT: {
         size_t n = 0;
         const char *text = anvil_builder_emit(b, &n);
         ok = text != NULL;
         if (ok) {
            script_print_data("emit", text, n);
            if (emit_dir && (rc = save_emit(emit_dir, ++emits, text, n)) != 0) {
               goto done;
            }
         }
         break;
      }
      case V_WRITE: {
         int wd = st->argc > 0 ? (is_word(&st->args[0], "AMP") ? ANVIL_WRITER_AMP : ANVIL_WRITER_AML) : dialects[bi];
         anvil_writer w = st->argc > 0 && is_word(&st->args[0], "NULL") ? NULL : anvil_writer_new((anvil_writer_dialect)wd);
         ok = anvil_builder_write(b, w);
         if (ok) {
            ok = anvil_writer_finish(w);
            if (!ok) {
               fail_code = (int)anvil_writer_get_error(w);
            } else {
               size_t n = 0;
               const char *text = anvil_writer_data(w, &n);
               script_print_data("write", text ? text : "", text ? n : 0);
               if (emit_dir && (rc = save_emit(emit_dir, ++emits, text ? text : "", text ? n : 0)) != 0) {
                  anvil_writer_dispose(w);
                  goto done;
               }
            }
         }
         anvil_writer_dispose(w);
         break;
      }
      }
      if (verbs[st->verb].ret && st->verb != V_FIND) {
         ok = result != NULL;
      }
      if (st->let_slot >= 0) {
         values[st->let_slot] = result;
      }
      if (!ok) {
         print_failed(st, fail_code >= 0 ? fail_code : (int)anvil_builder_get_error(b));
         if (!cont) {
            break;
         }
      }
   }

   printf("steps %zu\n", ran);
   {
      int code = (int)anvil_builder_get_error(builders[0]);
      if (code == 0) {
         puts("status ok");
      } else {
         printf("status error code=%d name=%s\n", code, script_err_name(code));
      }
   }
   rc = 0;
done:
   anvil_builder_dispose(builders[0]);
   anvil_builder_dispose(builders[1]);
   free(values);
   free_handles(&handles);
   free_steps(steps, count);
   return rc;
}
