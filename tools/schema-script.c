/* ********************************************************************** *
 * schema-script - anvil-dump's schema mode: runs a schema script (the     *
 * format "anvil-schema-script 1" in conformance/FORMAT.md) through the    *
 * native type registry and schema APIs and prints the run                 *
 * ("anvil-schema-run 1"): the registry and schema diagnostics, the type   *
 * and rule accessors, and every validation with its violations.           *
 *                                                                         *
 * The script carries its own files. They are written to a private        *
 * temporary directory that is the working directory for the run and is   *
 * removed before returning, so documents load by relative path and no     *
 * path of the directory ever reaches the output.                          *
 * ********************************************************************** */

#include "schema-script.h"
#include "script-common.h"
#include "anvil_flat.h"
#include "anvil_schema.h"
#include "anvil_type_registry.h"
// ----------------
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define RUN_FORMAT_VERSION 1
#define SCRIPT_VERSION_LINE "anvil-schema-script 1"
#define MAX_TOKENS 16
#define MAX_FILE_NAME 200

typedef script_arg arg;

enum { V_FILE, V_REGISTRY, V_TYPE, V_SCHEMA, V_VALIDATE, V_VALIDATE_VALUE, V_VIOLATIONS, V_FIELD, V_COUNT };

static const char *const verb_names[V_COUNT] = {"file", "registry", "type", "schema", "validate", "validate_value", "violations", "field"};

typedef struct {
   size_t line;
   int verb;
   arg *args; // owned
   int argc;
   int slot[2];   // [0] the slot a binding step binds; [1] the slot of the registry or schema a step uses (-1 for NULL)
   char *content; // a file step: its bytes (owned)
   size_t content_len;
} step;

// The names bound so far and the kind each holds: 'R' registry, 'S' schema.
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
   for (int a = 0; a < st->argc; a++) {
      free(st->args[a].bytes);
   }
   free(st->args);
   free(st->content);
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

// Binds a new name of `kind`; returns its slot, or -1 when out of memory.
static int bind_handle(handle_table *h, const char *name, char kind) {
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
   int slot = (int)h->count++;
   h->name[slot] = copy;
   h->kind[slot] = kind;
   return slot;
}

static bool is_word(const arg *a, const char *w) { return !a->quoted && strcmp(a->bytes, w) == 0; }

// A file name: components of [A-Za-z0-9_.-] separated by '/', no empty, '.' or '..' component, no leading '/'.
static bool valid_file_name(const char *s, size_t len) {
   if (len == 0 || len > MAX_FILE_NAME) {
      return false;
   }
   size_t start = 0;
   for (size_t i = 0; i <= len; i++) {
      if (i == len || s[i] == '/') {
         size_t clen = i - start;
         if (clen == 0 || (clen == 1 && s[start] == '.') || (clen == 2 && s[start] == '.' && s[start + 1] == '.')) {
            return false;
         }
         start = i + 1;
      } else {
         char c = s[i];
         if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) {
            return false;
         }
      }
   }
   return true;
}

// Two file names conflict when they are equal or one is a directory prefix of the other.
static bool names_conflict(const char *a, const char *b) {
   size_t la = strlen(a), lb = strlen(b);
   if (la == lb) {
      return strcmp(a, b) == 0;
   }
   const char *s = la < lb ? a : b, *l = la < lb ? b : a;
   size_t ls = la < lb ? la : lb;
   return strncmp(s, l, ls) == 0 && l[ls] == '/';
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

static const char *need_quoted(const arg *a, bool allow_null, bool no_nul) {
   if (!a->quoted) {
      if (allow_null && strcmp(a->bytes, "NULL") == 0) {
         return NULL;
      }
      return allow_null ? "expected a quoted string or NULL" : "expected a quoted string";
   }
   if (no_nul && memchr(a->bytes, '\0', a->len)) {
      return "NUL in a text argument";
   }
   return NULL;
}

// A handle argument of kind `kind` (or the word NULL): sets *slot (-1 for NULL).
static const char *need_handle(const arg *a, char kind, const handle_table *h, int *slot) {
   if (a->quoted) {
      return "expected a handle name or NULL";
   }
   if (strcmp(a->bytes, "NULL") == 0) {
      *slot = -1;
      return NULL;
   }
   int s = valid_name(a->bytes) ? find_handle(h, a->bytes) : -1;
   if (s < 0) {
      return "unknown handle";
   }
   if (h->kind[s] != kind) {
      return "wrong kind of handle";
   }
   *slot = s;
   return NULL;
}

// Checks one parsed step. For a binding step, binds the name. Returns NULL, or a message.
static const char *check_step(step *st, handle_table *handles) {
   const char *m;
   int n = st->argc;
   arg *a = st->args;
   st->slot[0] = st->slot[1] = -1;
   switch (st->verb) {
   case V_FILE:
      if (n < 1 || n > 2) {
         return "wrong number of arguments";
      }
      if ((m = need_quoted(&a[0], false, true))) {
         return m;
      }
      if (!valid_file_name(a[0].bytes, a[0].len)) {
         return "bad file name";
      }
      if (n == 2 && (m = need_quoted(&a[1], false, false))) {
         return m;
      }
      return NULL;
   case V_REGISTRY:
   case V_SCHEMA: {
      bool reg = st->verb == V_REGISTRY;
      if (n != 3) {
         return "wrong number of arguments";
      }
      if (a[0].quoted || !valid_name(a[0].bytes) || strcmp(a[0].bytes, "NULL") == 0) {
         return "bad handle name";
      }
      if (find_handle(handles, a[0].bytes) >= 0) {
         return "name already bound";
      }
      if (reg ? (!is_word(&a[1], "load") && !is_word(&a[1], "includes")) : !is_word(&a[1], "load")) {
         return reg ? "expected load or includes" : "expected load";
      }
      if ((m = need_quoted(&a[2], false, true))) {
         return m;
      }
      st->slot[0] = bind_handle(handles, a[0].bytes, reg ? 'R' : 'S');
      return st->slot[0] < 0 ? "out of memory" : NULL;
   }
   case V_TYPE:
      if (n != 3) {
         return "wrong number of arguments";
      }
      if (!is_word(&a[0], "find") && !is_word(&a[0], "resolve")) {
         return "expected find or resolve";
      }
      if ((m = need_handle(&a[1], 'R', handles, &st->slot[1]))) {
         return m;
      }
      return need_quoted(&a[2], false, true);
   case V_VALIDATE:
      if (n != 2) {
         return "wrong number of arguments";
      }
      if ((m = need_handle(&a[0], 'S', handles, &st->slot[1]))) {
         return m;
      }
      return need_quoted(&a[1], false, true);
   case V_VALIDATE_VALUE: {
      if (n < 2) {
         return "wrong number of arguments";
      }
      if ((m = need_handle(&a[0], 'S', handles, &st->slot[1])) || (m = need_quoted(&a[1], true, false))) {
         return m;
      }
      bool skip = false, allow = false, no_opts = false;
      for (int i = 2; i < n; i++) {
         if (is_word(&a[i], "skip_required") && !skip) {
            skip = true;
         } else if (is_word(&a[i], "allow_unknown") && !allow) {
            allow = true;
         } else if (is_word(&a[i], "no_opts") && !no_opts) {
            no_opts = true;
         } else {
            return "bad option";
         }
      }
      if (no_opts && (skip || allow)) {
         return "no_opts takes no other option";
      }
      return NULL;
   }
   case V_VIOLATIONS:
      if (n != 1) {
         return "wrong number of arguments";
      }
      return need_handle(&a[0], 'S', handles, &st->slot[1]);
   case V_FIELD:
      if (n < 2) {
         return "wrong number of arguments";
      }
      if ((m = need_handle(&a[0], 'S', handles, &st->slot[1]))) {
         return m;
      }
      for (int i = 1; i < n; i++) {
         if ((m = need_quoted(&a[i], false, true))) {
            return m;
         }
      }
      return NULL;
   }
   return "unknown call";
}

static int parse_script(const char *data, size_t size, step **out, size_t *count, handle_table *handles) {
   step *steps = NULL;
   size_t n = 0, cap = 0, lineno = 0;
   bool first = true;
   bool in_block = false; // the last step is a file step in block form
   const char *p = data, *end = data + size;
   while (p < end) {
      const char *nl = memchr(p, '\n', (size_t)(end - p));
      size_t llen = nl ? (size_t)(nl - p) : (size_t)(end - p);
      lineno++;
      const char *lp = p;
      p = nl ? nl + 1 : end;
      if (memchr(lp, '\0', llen)) {
         free_steps(steps, n);
         return script_error(lineno, "NUL byte in a script line");
      }
      if (first) {
         first = false;
         if (llen != strlen(SCRIPT_VERSION_LINE) || memcmp(lp, SCRIPT_VERSION_LINE, llen) != 0) {
            free_steps(steps, n);
            return script_error(lineno, "first line must be \"" SCRIPT_VERSION_LINE "\"");
         }
         continue;
      }
      if (llen > 0 && lp[0] == '|') {
         if (!in_block) {
            free_steps(steps, n);
            return script_error(lineno, "content line outside a file block");
         }
         step *f = &steps[n - 1];
         char *grown = realloc(f->content, f->content_len + llen + 1);
         if (!grown) {
            free_steps(steps, n);
            return script_error(lineno, "out of memory");
         }
         f->content = grown;
         memcpy(f->content + f->content_len, lp + 1, llen - 1);
         f->content_len += llen - 1;
         f->content[f->content_len++] = '\n';
         continue;
      }
      in_block = false;
      char *line = malloc(llen + 1);
      if (!line) {
         free_steps(steps, n);
         return script_error(lineno, "out of memory");
      }
      memcpy(line, lp, llen);
      line[llen] = '\0';
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
      arg tok[MAX_TOKENS] = {0};
      int ntok = 0;
      const char *msg = tokenize(s, tok, &ntok);
      if (!msg) {
         if (ntok == 0 || tok[0].quoted) {
            msg = "unknown call";
         } else {
            for (int v = 0; v < V_COUNT; v++) {
               if (strcmp(verb_names[v], tok[0].bytes) == 0) {
                  st.verb = v;
               }
            }
            msg = st.verb < 0 ? "unknown call" : NULL;
         }
      }
      if (!msg) {
         st.argc = ntok - 1;
         st.args = calloc((size_t)(st.argc ? st.argc : 1), sizeof(arg));
         if (!st.args) {
            msg = "out of memory";
         } else {
            for (int a = 0; a < st.argc; a++) {
               st.args[a] = tok[1 + a]; // ownership moves to the step
               tok[1 + a].bytes = NULL;
            }
            msg = check_step(&st, handles);
         }
      }
      if (!msg && st.verb == V_FILE) {
         for (size_t i = 0; i < n; i++) {
            if (steps[i].verb == V_FILE && names_conflict(steps[i].args[0].bytes, st.args[0].bytes)) {
               msg = strcmp(steps[i].args[0].bytes, st.args[0].bytes) == 0 ? "file declared twice" : "file name conflicts with another file";
               break;
            }
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
      for (int i = 0; i < ntok; i++) {
         free(tok[i].bytes); // the ones not moved into the step
      }
      free(line);
      if (st.verb == V_FILE) {
         if (st.argc == 2) {
            st.content = malloc(st.args[1].len + 1);
            if (!st.content) {
               free_step(&st);
               free_steps(steps, n);
               return script_error(lineno, "out of memory");
            }
            memcpy(st.content, st.args[1].bytes, st.args[1].len);
            st.content_len = st.args[1].len;
         } else {
            in_block = true;
         }
      }
      if (n == cap) {
         cap = cap ? cap * 2 : 64;
         step *grown = realloc(steps, cap * sizeof *grown);
         if (!grown) {
            free_step(&st);
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
   *out = steps;
   *count = n;
   return 0;
}

/* ---------------------------------------------------------------------- *
 * The private directory
 * ---------------------------------------------------------------------- */

typedef struct {
   char root[1024];
   char **files; // paths made, in order
   char **dirs;  // directories made, in order (the root first)
   size_t nfiles, ndirs;
} tmp_tree;

static bool push_path(char ***list, size_t *n, const char *path) {
   char **grown = realloc(*list, (*n + 1) * sizeof **list);
   if (!grown) {
      return false;
   }
   *list = grown;
   char *copy = strdup(path);
   if (!copy) {
      return false;
   }
   (*list)[(*n)++] = copy;
   return true;
}

static void tree_remove(tmp_tree *t) {
   if (chdir("/") != 0) {
      /* the removal below uses absolute paths */
   }
   for (size_t i = 0; i < t->nfiles; i++) {
      unlink(t->files[i]);
      free(t->files[i]);
   }
   for (size_t i = t->ndirs; i-- > 0;) {
      rmdir(t->dirs[i]);
      free(t->dirs[i]);
   }
   free(t->files);
   free(t->dirs);
   t->files = t->dirs = NULL;
   t->nfiles = t->ndirs = 0;
}

// Makes the directory tree and writes every `file` step; returns false (with a message on stderr) on failure.
static bool tree_make(tmp_tree *t, const step *steps, size_t count) {
   const char *base = getenv("TMPDIR");
   if (!base || !*base) {
      base = "/tmp";
   }
   if (snprintf(t->root, sizeof t->root, "%s/anvil-schema-script-XXXXXX", base) >= (int)sizeof t->root || !mkdtemp(t->root)) {
      fprintf(stderr, "anvil-dump: cannot create a temporary directory under %s\n", base);
      return false;
   }
   if (!push_path(&t->dirs, &t->ndirs, t->root)) {
      return false;
   }
   for (size_t i = 0; i < count; i++) {
      if (steps[i].verb != V_FILE) {
         continue;
      }
      char path[2048];
      int len = snprintf(path, sizeof path, "%s/%s", t->root, steps[i].args[0].bytes);
      if (len < 0 || (size_t)len >= sizeof path) {
         fprintf(stderr, "anvil-dump: file name too long\n");
         return false;
      }
      for (char *c = path + strlen(t->root) + 1; *c; c++) {
         if (*c == '/') {
            *c = '\0';
            struct stat sb;
            if (stat(path, &sb) != 0) {
               if (mkdir(path, 0700) != 0 || !push_path(&t->dirs, &t->ndirs, path)) {
                  fprintf(stderr, "anvil-dump: cannot create %s\n", path);
                  return false;
               }
            }
            *c = '/';
         }
      }
      FILE *f = fopen(path, "wb");
      if (!f) {
         fprintf(stderr, "anvil-dump: cannot write %s\n", path);
         return false;
      }
      push_path(&t->files, &t->nfiles, path);
      size_t k = steps[i].content_len;
      if (k && fwrite(steps[i].content, 1, k, f) != k) {
         fclose(f);
         fprintf(stderr, "anvil-dump: cannot write %s\n", path);
         return false;
      }
      fclose(f);
   }
   return true;
}

/* ---------------------------------------------------------------------- *
 * Printing
 * ---------------------------------------------------------------------- */

static void put_indent(int depth) {
   for (int i = 0; i < depth * 2; i++) {
      putchar(' ');
   }
}

// Text getters with the two-call convention; the result is malloc'ed with room for a NUL, *len is its exact length.
#define DEF_TEXT(fname, htype, getter)        \
   static char *fname(htype h, size_t *len) { \
      size_t n = getter(h, NULL, 0);          \
      char *b = malloc(n + 1);                \
      if (!b) {                               \
         *len = 0;                            \
         return NULL;                         \
      }                                       \
      getter(h, b, n + 1);                    \
      *len = n;                               \
      return b;                               \
   }

DEF_TEXT(td_name, anvil_type_diagnostic, anvil_type_diagnostic_get_name)
DEF_TEXT(td_source, anvil_type_diagnostic, anvil_type_diagnostic_get_source)
DEF_TEXT(td_message, anvil_type_diagnostic, anvil_type_diagnostic_get_message)
DEF_TEXT(sd_field, anvil_schema_diagnostic, anvil_schema_diagnostic_get_field)
DEF_TEXT(sd_message, anvil_schema_diagnostic, anvil_schema_diagnostic_get_message)
DEF_TEXT(sv_field, anvil_schema_violation, anvil_schema_violation_get_field)
DEF_TEXT(sv_message, anvil_schema_violation, anvil_schema_violation_get_message)
DEF_TEXT(sf_name, anvil_schema_field, anvil_schema_field_get_name)
DEF_TEXT(sf_type, anvil_schema_field, anvil_schema_field_get_type_name)
DEF_TEXT(sf_format, anvil_schema_field, anvil_schema_field_get_format)
DEF_TEXT(sf_tag, anvil_schema_field, anvil_schema_field_get_tag)
DEF_TEXT(td_format, anvil_type_def, anvil_type_def_get_format)
DEF_TEXT(val_text, anvil_value, anvil_value_get_text)
DEF_TEXT(val_tag, anvil_value, anvil_value_get_blob_tag)
DEF_TEXT(stmt_name, anvil_statement, anvil_statement_get_name)

// Prints ` key=<quoted text>`.
static void put_q(const char *key, const char *text, size_t len) {
   printf(" %s=", key);
   dump_put_quoted(text ? text : "", text ? len : 0);
}

// Prints ` key=<quoted>` for an optional text, or ` key=none` when its length is 0.
static void put_opt_q(const char *key, const char *text, size_t len) {
   if (len == 0) {
      printf(" %s=none", key);
   } else {
      put_q(key, text, len);
   }
}

static void put_ll(const char *key, bool declared, long long v) {
   if (declared) {
      printf(" %s=%lld", key, v);
   } else {
      printf(" %s=none", key);
   }
}

static void put_numeric(const char *key, bool declared, anvil_numeric_t n) {
   if (!declared) {
      printf(" %s=none", key);
   } else if (n.overflowed) {
      printf(" %s=ovf:%s", key, n.is_integral ? "integer" : "float");
   } else if (n.is_integral) {
      printf(" %s=i64:%" PRId64, key, n.as_int64);
   } else {
      uint64_t bits;
      memcpy(&bits, &n.as_double, sizeof bits);
      printf(" %s=f64:0x%016" PRIx64, key, bits);
   }
}

static const char *type_kind_name(anvil_type_kind k) {
   switch (k) {
   case ANVIL_TYPE_UNKNOWN: return "ANVIL_TYPE_UNKNOWN";
   case ANVIL_TYPE_NUMERIC: return "ANVIL_TYPE_NUMERIC";
   case ANVIL_TYPE_STRING: return "ANVIL_TYPE_STRING";
   case ANVIL_TYPE_BOOL: return "ANVIL_TYPE_BOOL";
   case ANVIL_TYPE_OBJECT: return "ANVIL_TYPE_OBJECT";
   case ANVIL_TYPE_TUPLE: return "ANVIL_TYPE_TUPLE";
   case ANVIL_TYPE_ARRAY: return "ANVIL_TYPE_ARRAY";
   case ANVIL_TYPE_ENUM: return "ANVIL_TYPE_ENUM";
   case ANVIL_TYPE_BLOB: return "ANVIL_TYPE_BLOB";
   }
   return "ANVIL_TYPE_UNNAMED";
}

static const char *type_diag_name(anvil_type_diag_code c) {
   switch (c) {
   case ANVIL_TYPE_DIAG_NONE: return "ANVIL_TYPE_DIAG_NONE";
   case ANVIL_TYPE_DIAG_NAME_COLLISION: return "ANVIL_TYPE_DIAG_NAME_COLLISION";
   case ANVIL_TYPE_DIAG_UNKNOWN_BASE_TYPE: return "ANVIL_TYPE_DIAG_UNKNOWN_BASE_TYPE";
   case ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT: return "ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT";
   case ANVIL_TYPE_DIAG_MALFORMED_ENUM_VALUES: return "ANVIL_TYPE_DIAG_MALFORMED_ENUM_VALUES";
   }
   return "ANVIL_TYPE_DIAG_UNNAMED";
}

static const char *schema_diag_name(anvil_schema_diag_code c) {
   switch (c) {
   case ANVIL_SCHEMA_DIAG_NONE: return "ANVIL_SCHEMA_DIAG_NONE";
   case ANVIL_SCHEMA_DIAG_UNKNOWN_TYPE: return "ANVIL_SCHEMA_DIAG_UNKNOWN_TYPE";
   case ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT: return "ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT";
   case ANVIL_SCHEMA_DIAG_RULE_NOT_OBJECT: return "ANVIL_SCHEMA_DIAG_RULE_NOT_OBJECT";
   case ANVIL_SCHEMA_DIAG_UNKNOWN_FORMAT: return "ANVIL_SCHEMA_DIAG_UNKNOWN_FORMAT";
   case ANVIL_SCHEMA_DIAG_SELF_REFERENCE: return "ANVIL_SCHEMA_DIAG_SELF_REFERENCE";
   case ANVIL_SCHEMA_DIAG_CONFLICTING_CONSTRAINTS: return "ANVIL_SCHEMA_DIAG_CONFLICTING_CONSTRAINTS";
   case ANVIL_SCHEMA_DIAG_TYPE_REGISTRY: return "ANVIL_SCHEMA_DIAG_TYPE_REGISTRY";
   }
   return "ANVIL_SCHEMA_DIAG_UNNAMED";
}

static const char *violation_name(anvil_schema_err_code c) {
   switch (c) {
   case ANVIL_SCHEMA_ERR_NONE: return "ANVIL_SCHEMA_ERR_NONE";
   case ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED: return "ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED";
   case ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH: return "ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH";
   case ANVIL_SCHEMA_ERR_VALIDATION_SIZE: return "ANVIL_SCHEMA_ERR_VALIDATION_SIZE";
   case ANVIL_SCHEMA_ERR_VALIDATION_RANGE: return "ANVIL_SCHEMA_ERR_VALIDATION_RANGE";
   case ANVIL_SCHEMA_ERR_VALIDATION_VALUES: return "ANVIL_SCHEMA_ERR_VALIDATION_VALUES";
   case ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD: return "ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD";
   case ANVIL_SCHEMA_ERR_VALIDATION_ITEM_COUNT: return "ANVIL_SCHEMA_ERR_VALIDATION_ITEM_COUNT";
   case ANVIL_SCHEMA_ERR_VALIDATION_TAG: return "ANVIL_SCHEMA_ERR_VALIDATION_TAG";
   case ANVIL_SCHEMA_ERR_VALIDATION_FORMAT: return "ANVIL_SCHEMA_ERR_VALIDATION_FORMAT";
   case ANVIL_SCHEMA_ERR_VALIDATION_UNRESOLVED_TYPE: return "ANVIL_SCHEMA_ERR_VALIDATION_UNRESOLVED_TYPE";
   }
   return "ANVIL_SCHEMA_ERR_UNNAMED";
}

static const char *value_type_name(anvil_value_type t) {
   switch (t) {
   case ANVIL_VALUE_NULL: return "NULL";
   case ANVIL_VALUE_BOOL: return "BOOL";
   case ANVIL_VALUE_NUMERIC: return "NUMERIC";
   case ANVIL_VALUE_STRING: return "STRING";
   case ANVIL_VALUE_BLOB: return "BLOB";
   case ANVIL_VALUE_BARE: return "BARE";
   case ANVIL_VALUE_ARRAY: return "ARRAY";
   case ANVIL_VALUE_TUPLE: return "TUPLE";
   case ANVIL_VALUE_OBJECT: return "OBJECT";
   }
   return "UNKNOWN";
}

static const char *category_name(anvil_err_code c) {
   switch (c) {
   case ANVIL_OK: return "ANVIL_OK";
   case ANVIL_ERR_IO: return "ANVIL_ERR_IO";
   case ANVIL_ERR_HEADER: return "ANVIL_ERR_HEADER";
   case ANVIL_ERR_INCLUDE: return "ANVIL_ERR_INCLUDE";
   case ANVIL_ERR_SYNTAX: return "ANVIL_ERR_SYNTAX";
   case ANVIL_ERR_RESOLVE: return "ANVIL_ERR_RESOLVE";
   case ANVIL_ERR_MEMORY: return "ANVIL_ERR_MEMORY";
   case ANVIL_ERR_INVALID_ARGUMENT: return "ANVIL_ERR_INVALID_ARGUMENT";
   }
   return "ANVIL_ERR_UNKNOWN";
}

// How a document loaded: ok, null, or the category of its error.
static const char *doc_state(anvil_document d) {
   if (!d) {
      return "null";
   }
   return anvil_has_errors(d) ? category_name(anvil_get_error(d)) : "ok";
}

static void put_value_tree(anvil_value v, int depth) {
   put_indent(depth);
   if (!v) {
      puts("value type=none");
      return;
   }
   anvil_value_type t = anvil_value_get_type(v);
   printf("value type=%s", value_type_name(t));
   size_t len = 0;
   switch (t) {
   case ANVIL_VALUE_BLOB: {
      char *tag = val_tag(v, &len);
      put_q("tag", tag, len);
      free(tag);
      size_t n = anvil_value_get_bytes(v, NULL, 0);
      char *b = malloc(n + 1);
      if (b) {
         anvil_value_get_bytes(v, b, n);
         put_q("text", b, n);
         free(b);
      }
      putchar('\n');
      return;
   }
   case ANVIL_VALUE_ARRAY:
   case ANVIL_VALUE_TUPLE:
   case ANVIL_VALUE_OBJECT: {
      size_t count = anvil_value_get_count(v);
      printf(" count=%zu\n", count);
      for (size_t i = 0; i < count; i++) {
         if (t == ANVIL_VALUE_OBJECT) {
            anvil_statement s = anvil_value_get_statement(v, i);
            char *nm = stmt_name(s, &len);
            put_indent(depth + 1);
            fputs("stmt", stdout);
            put_q("name", nm, len);
            putchar('\n');
            free(nm);
            put_value_tree(anvil_statement_get_value(s), depth + 2);
         } else {
            put_value_tree(anvil_value_get_element(v, i), depth + 1);
         }
      }
      return;
   }
   default:
      break;
   }
   char *text = val_text(v, &len);
   put_q("text", text, len);
   free(text);
   if (t == ANVIL_VALUE_NUMERIC) {
      anvil_numeric_t n = anvil_value_get_numeric(v);
      printf(" kind=%s overflow=%d", n.is_integral ? "integer" : "float", n.overflowed ? 1 : 0);
      if (!n.overflowed) {
         if (n.is_integral) {
            printf(" i64=%" PRId64, n.as_int64);
         } else {
            uint64_t bits;
            memcpy(&bits, &n.as_double, sizeof bits);
            printf(" f64=0x%016" PRIx64, bits);
         }
      }
   }
   putchar('\n');
}

static void put_rule(anvil_schema_field f, const char *role, int depth) {
   put_indent(depth);
   fputs(role, stdout);
   size_t len;
   char *s = sf_name(f, &len);
   put_q("name", s, len);
   free(s);
   printf(" kind=%s", type_kind_name(anvil_schema_field_get_kind(f)));
   s = sf_type(f, &len);
   put_opt_q("type", s, len);
   free(s);
   printf(" required=%d nullable=%d integer=%d", anvil_schema_field_is_required(f) ? 1 : 0, anvil_schema_field_is_nullable(f) ? 1 : 0,
          anvil_schema_field_is_integer(f) ? 1 : 0);
   long long ll = 0;
   anvil_numeric_t num = {0};
   bool has = anvil_schema_field_get_size(f, &ll);
   put_ll("size", has, ll);
   has = anvil_schema_field_get_min_numeric(f, &num);
   put_numeric("min", has, num);
   has = anvil_schema_field_get_max_numeric(f, &num);
   put_numeric("max", has, num);
   has = anvil_schema_field_get_min_items(f, &ll);
   put_ll("min_items", has, ll);
   has = anvil_schema_field_get_max_items(f, &ll);
   put_ll("max_items", has, ll);
   s = sf_format(f, &len);
   put_opt_q("format", s, len);
   free(s);
   s = sf_tag(f, &len);
   put_opt_q("tag", s, len);
   free(s);
   size_t values = anvil_schema_field_get_value_count(f);
   size_t nested = anvil_schema_field_get_nested_count(f);
   anvil_schema_field items = anvil_schema_field_get_items(f);
   size_t extras = anvil_schema_field_get_extra_count(f);
   printf(" values=%zu nested=%zu items=%d extras=%zu\n", values, nested, items ? 1 : 0, extras);
   for (size_t i = 0; i < values; i++) {
      size_t n = anvil_schema_field_get_value(f, i, NULL, 0);
      char *b = malloc(n + 1);
      if (b) {
         anvil_schema_field_get_value(f, i, b, n + 1);
         put_indent(depth + 1);
         printf("label index=%zu", i);
         put_q("text", b, n);
         putchar('\n');
         free(b);
      }
   }
   for (size_t i = 0; i < extras; i++) {
      size_t n = anvil_schema_field_get_extra_name(f, i, NULL, 0);
      char *b = malloc(n + 1);
      if (b) {
         anvil_schema_field_get_extra_name(f, i, b, n + 1);
         put_indent(depth + 1);
         printf("extra index=%zu", i);
         put_q("name", b, n);
         putchar('\n');
         free(b);
      }
      put_value_tree(anvil_schema_field_get_extra_value(f, i), depth + 2);
   }
   for (size_t i = 0; i < nested; i++) {
      put_rule(anvil_schema_field_get_nested(f, i), "nested", depth + 1);
   }
   if (items) {
      put_rule(items, "items", depth + 1);
   }
}

static void put_violations(anvil_schema schema) {
   size_t count = anvil_schema_get_violation_count(schema);
   for (size_t i = 0; i < count; i++) {
      anvil_schema_violation v = anvil_schema_get_violation(schema, i);
      anvil_schema_err_code code = anvil_schema_violation_get_category(v);
      size_t len;
      char *f = sv_field(v, &len);
      printf("  violation index=%zu code=%d name=%s", i, (int)code, violation_name(code));
      put_q("field", f, len);
      free(f);
      char *m = sv_message(v, &len);
      put_q("message", m, len);
      free(m);
      putchar('\n');
   }
}

static void put_type_def(anvil_type_def def) {
   long long ll = 0;
   anvil_numeric_t num = {0};
   printf(" result=found kind=%s", type_kind_name(anvil_type_def_get_kind(def)));
   bool has = anvil_type_def_get_size(def, &ll);
   put_ll("size", has, ll);
   has = anvil_type_def_get_min(def, &ll);
   put_ll("min", has, ll);
   has = anvil_type_def_get_max(def, &ll);
   put_ll("max", has, ll);
   has = anvil_type_def_get_min_numeric(def, &num);
   put_numeric("min_numeric", has, num);
   has = anvil_type_def_get_max_numeric(def, &num);
   put_numeric("max_numeric", has, num);
   size_t len;
   char *fmt = td_format(def, &len);
   put_opt_q("format", fmt, len);
   free(fmt);
   size_t values = anvil_type_def_get_value_count(def);
   printf(" values=%zu\n", values);
   for (size_t i = 0; i < values; i++) {
      size_t n = anvil_type_def_get_value(def, i, NULL, 0);
      char *b = malloc(n + 1);
      if (b) {
         anvil_type_def_get_value(def, i, b, n + 1);
         printf("  label index=%zu", i);
         put_q("text", b, n);
         putchar('\n');
         free(b);
      }
   }
}

/* ---------------------------------------------------------------------- *
 * The run
 * ---------------------------------------------------------------------- */

static const char *handle_label(const handle_table *h, int slot) { return slot < 0 ? "NULL" : h->name[slot]; }

static void run_steps(const step *steps, size_t count, const handle_table *handles, void **values) {
   printf("anvil-schema-run %d\n", RUN_FORMAT_VERSION);
   for (size_t i = 0; i < count; i++) {
      const step *st = &steps[i];
      const arg *a = st->args;
      size_t len;
      switch (st->verb) {
      case V_FILE: break;
      case V_REGISTRY: {
         bool includes = strcmp(a[1].bytes, "includes") == 0;
         anvil_document doc = anvil_load(a[2].bytes);
         const char *state = doc_state(doc);
         anvil_type_registry reg = includes ? anvil_type_registry_load_from_includes(doc) : anvil_type_registry_load(doc);
         anvil_dispose(doc);
         values[st->slot[0]] = reg;
         size_t nd = anvil_type_registry_get_diagnostic_count(reg);
         printf("registry line=%zu name=%s how=%s", st->line, a[0].bytes, a[1].bytes);
         put_q("file", a[2].bytes, a[2].len);
         printf(" doc=%s result=%s diagnostics=%zu\n", state, reg ? "ok" : "null", nd);
         for (size_t d = 0; d < nd; d++) {
            anvil_type_diagnostic diag = anvil_type_registry_get_diagnostic(reg, d);
            anvil_type_diag_code code = anvil_type_diagnostic_get_category(diag);
            printf("  diag index=%zu code=%d name=%s", d, (int)code, type_diag_name(code));
            char *s = td_name(diag, &len);
            put_q("type", s, len);
            free(s);
            s = td_source(diag, &len);
            put_q("source", s, len);
            free(s);
            s = td_message(diag, &len);
            put_q("message", s, len);
            free(s);
            putchar('\n');
         }
         break;
      }
      case V_TYPE: {
         bool resolve = strcmp(a[0].bytes, "resolve") == 0;
         anvil_type_registry reg = st->slot[1] < 0 ? NULL : values[st->slot[1]];
         anvil_type_def def = resolve ? anvil_type_resolve(reg, a[2].bytes) : anvil_type_registry_find(reg, a[2].bytes);
         printf("type line=%zu how=%s registry=%s", st->line, a[0].bytes, handle_label(handles, st->slot[1]));
         put_q("name", a[2].bytes, a[2].len);
         if (!def) {
            puts(" result=none");
         } else {
            put_type_def(def);
         }
         break;
      }
      case V_SCHEMA: {
         anvil_document doc = anvil_load(a[2].bytes);
         const char *state = doc_state(doc);
         anvil_schema schema = anvil_schema_load(doc);
         anvil_dispose(doc);
         values[st->slot[0]] = schema;
         size_t nd = anvil_schema_get_diagnostic_count(schema);
         size_t nf = anvil_schema_get_field_count(schema);
         printf("schema line=%zu name=%s", st->line, a[0].bytes);
         put_q("file", a[2].bytes, a[2].len);
         printf(" doc=%s result=%s diagnostics=%zu fields=%zu\n", state, schema ? "ok" : "null", nd, nf);
         for (size_t d = 0; d < nd; d++) {
            anvil_schema_diagnostic diag = anvil_schema_get_diagnostic(schema, d);
            anvil_schema_diag_code code = anvil_schema_diagnostic_get_category(diag);
            anvil_type_diag_code tcode = anvil_schema_diagnostic_get_type_category(diag);
            printf("  diag index=%zu code=%d name=%s type_code=%d type_name=%s", d, (int)code, schema_diag_name(code), (int)tcode,
                   type_diag_name(tcode));
            char *s = sd_field(diag, &len);
            put_q("field", s, len);
            free(s);
            s = sd_message(diag, &len);
            put_q("message", s, len);
            free(s);
            putchar('\n');
         }
         for (size_t f = 0; f < nf; f++) {
            put_rule(anvil_schema_get_field(schema, f), "field", 1);
         }
         break;
      }
      case V_VALIDATE: {
         anvil_schema schema = st->slot[1] < 0 ? NULL : values[st->slot[1]];
         anvil_document doc = anvil_load(a[1].bytes);
         const char *state = doc_state(doc);
         bool ok = anvil_schema_validate(schema, doc);
         anvil_dispose(doc);
         printf("validate line=%zu schema=%s", st->line, handle_label(handles, st->slot[1]));
         put_q("file", a[1].bytes, a[1].len);
         printf(" doc=%s result=%s count=%zu\n", state, ok ? "pass" : "fail", anvil_schema_get_violation_count(schema));
         put_violations(schema);
         break;
      }
      case V_VALIDATE_VALUE: {
         anvil_schema schema = st->slot[1] < 0 ? NULL : values[st->slot[1]];
         anvil_document doc = NULL;
         anvil_value value = NULL;
         const char *state = "none";
         if (a[1].quoted) {
            doc = anvil_parse_value_fragment(a[1].bytes, a[1].len);
            state = doc_state(doc);
            value = doc ? anvil_document_get_fragment_value(doc) : NULL;
         }
         bool skip = false, allow = false, no_opts = false;
         for (int k = 2; k < st->argc; k++) {
            skip = skip || strcmp(a[k].bytes, "skip_required") == 0;
            allow = allow || strcmp(a[k].bytes, "allow_unknown") == 0;
            no_opts = no_opts || strcmp(a[k].bytes, "no_opts") == 0;
         }
         anvil_schema_options opts = {.size = sizeof opts, .skip_required = skip, .allow_unknown = allow};
         bool ok = anvil_schema_validate_value(schema, value, no_opts ? NULL : &opts);
         anvil_dispose(doc);
         printf("validate_value line=%zu schema=%s doc=%s opts=", st->line, handle_label(handles, st->slot[1]), state);
         if (no_opts) {
            fputs("null", stdout);
         } else if (skip && allow) {
            fputs("skip_required+allow_unknown", stdout);
         } else if (skip || allow) {
            fputs(skip ? "skip_required" : "allow_unknown", stdout);
         } else {
            fputs("default", stdout);
         }
         printf(" result=%s count=%zu\n", ok ? "pass" : "fail", anvil_schema_get_violation_count(schema));
         put_violations(schema);
         break;
      }
      case V_VIOLATIONS: {
         anvil_schema schema = st->slot[1] < 0 ? NULL : values[st->slot[1]];
         printf("violations line=%zu schema=%s count=%zu\n", st->line, handle_label(handles, st->slot[1]),
                anvil_schema_get_violation_count(schema));
         put_violations(schema);
         break;
      }
      case V_FIELD: {
         anvil_schema schema = st->slot[1] < 0 ? NULL : values[st->slot[1]];
         anvil_schema_field f = anvil_schema_find_field(schema, a[1].bytes);
         for (int k = 2; k < st->argc && f; k++) {
            f = anvil_schema_field_find_nested(f, a[k].bytes);
         }
         // the path: the names joined by '.', as one quoted string
         size_t total = 0;
         for (int k = 1; k < st->argc; k++) {
            total += a[k].len + 1;
         }
         char *joined = malloc(total + 1);
         size_t at = 0;
         for (int k = 1; joined && k < st->argc; k++) {
            if (k > 1) {
               joined[at++] = '.';
            }
            memcpy(joined + at, a[k].bytes, a[k].len);
            at += a[k].len;
         }
         printf("field line=%zu schema=%s path=", st->line, handle_label(handles, st->slot[1]));
         dump_put_quoted(joined ? joined : "", joined ? at : 0);
         free(joined);
         printf(" depth=%d result=%s\n", st->argc - 1, f ? "found" : "none");
         if (f) {
            put_rule(f, st->argc == 2 ? "field" : "nested", 1);
         }
         break;
      }
      }
   }
   printf("steps %zu\nstatus ok\n", count);
}

int schema_script_run(const char *path) {
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
   tmp_tree tree = {{0}, NULL, NULL, 0, 0};
   void **values = calloc(handles.count ? handles.count : 1, sizeof *values);
   if (!values) {
      fprintf(stderr, "anvil-dump: out of memory\n");
      rc = 2;
   } else if (!tree_make(&tree, steps, count) || chdir(tree.root) != 0) {
      rc = 2;
   } else {
      fflush(stdout);
      run_steps(steps, count, &handles, values);
      fflush(stdout);
   }
   for (size_t i = 0; values && i < handles.count; i++) {
      if (handles.kind[i] == 'R') {
         anvil_type_registry_dispose(values[i]);
      } else {
         anvil_schema_dispose(values[i]);
      }
   }
   tree_remove(&tree);
   free(values);
   free_handles(&handles);
   free_steps(steps, count);
   return rc;
}
