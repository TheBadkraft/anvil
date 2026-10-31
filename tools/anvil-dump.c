/* ********************************************************************** *
 * anvil-dump - prints a canonical, deterministic description of what      *
 * Anvil Native parsed. The output format is the contract in               *
 * conformance/FORMAT.md (version 1).                                      *
 *                                                                         *
 *   anvil-dump [--fragment] <file | ->                                    *
 *   anvil-dump --writer-script <script> | --writer-output <script>        *
 *                                                                         *
 * A file is loaded with anvil_load (so includes resolve, and the file     *
 * extension is a dialect hint); "-" reads stdin and loads it as a buffer. *
 * --fragment parses the input as a standalone value expression.           *
 * --writer-script runs a writer script through the native writer and      *
 * prints the run; --writer-output prints only the output bytes            *
 * (tools/writer-script.c; FORMAT.md, "The writer script").                *
 * --builder-script runs a builder script and prints the run               *
 * (tools/builder-script.c; FORMAT.md, "The builder script").              *
 * Exit status: 0 for a dump (even of an input that fails to parse), 2 for  *
 * a usage or I/O error of the tool itself.                                *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "errors.h"
#include "internal/flat_internal.h"
#include "internal/module.h"
#include "internal/source.h"
#include "script-common.h"
#include "builder-script.h"
#include "writer-script.h"
#include <sigma/farray.h>
#include <sigma/list.h>
#include <sigma/query.h>
// ----------------
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DUMP_FORMAT_VERSION 1

/* ---------------------------------------------------------------------- *
 * Primitive writers
 * ---------------------------------------------------------------------- */

// A quoted string: ASCII only; \\ \" \n \r \t, and \xHH (lowercase) for every other byte below 0x20,
// 0x7f and everything from 0x80 up. Length-delimited, so NUL bytes are ordinary.
void dump_put_quoted(const char *s, size_t n) {
   static const char hex[] = "0123456789abcdef";
   putchar('"');
   for (size_t i = 0; i < n; i++) {
      unsigned char c = (unsigned char)s[i];
      switch (c) {
      case '\\': fputs("\\\\", stdout); break;
      case '"': fputs("\\\"", stdout); break;
      case '\n': fputs("\\n", stdout); break;
      case '\r': fputs("\\r", stdout); break;
      case '\t': fputs("\\t", stdout); break;
      default:
         if (c < 0x20 || c >= 0x7f) {
            putchar('\\');
            putchar('x');
            putchar(hex[c >> 4]);
            putchar(hex[c & 15]);
         } else {
            putchar(c);
         }
      }
   }
   putchar('"');
}

static void put_hex(const char *s, size_t n) {
   static const char hex[] = "0123456789abcdef";
   for (size_t i = 0; i < n; i++) {
      unsigned char c = (unsigned char)s[i];
      putchar(hex[c >> 4]);
      putchar(hex[c & 15]);
   }
}

static void put_indent(int depth) {
   for (int i = 0; i < depth * 2; i++) {
      putchar(' ');
   }
}

static size_t slice_len(anvl_slice s) {
   return (s.start && s.end && s.end >= s.start) ? (size_t)(s.end - s.start) : 0;
}

// key=<start>-<end> (half-open byte offsets into the slice's own source buffer), or key=none when the
// slice has no position at all.
static void put_span(const char *key, anvl_slice s) {
   if (!s.start || !s.end || !s.data) {
      printf(" %s=none", key);
      return;
   }
   printf(" %s=%zu-%zu", key, (size_t)(s.start - s.data), (size_t)(s.end - s.data));
}

static void put_text(const char *key, anvl_slice s) {
   printf(" %s=", key);
   dump_put_quoted(s.start, slice_len(s));
}

/* ---------------------------------------------------------------------- *
 * Names
 * ---------------------------------------------------------------------- */

static const char *dialect_name(anvl_dialect d) {
   switch (d) {
   case ANVL_DIALECT_AML: return "AML";
   case ANVL_DIALECT_ASL: return "ASL";
   case ANVL_DIALECT_AMP: return "AMP";
   default: return "ERROR";
   }
}

static const char *value_type_name(anvl_value_type t) {
   switch (t) {
   case ANVL_VALUE_NONE: return "NONE";
   case ANVL_VALUE_NULL: return "NULL";
   case ANVL_VALUE_BOOL: return "BOOL";
   case ANVL_VALUE_NUMERIC: return "NUMERIC";
   case ANVL_VALUE_STRING: return "STRING";
   case ANVL_VALUE_BLOB: return "BLOB";
   case ANVL_VALUE_ARRAY: return "ARRAY";
   case ANVL_VALUE_TUPLE: return "TUPLE";
   case ANVL_VALUE_OBJECT: return "OBJECT";
   case ANVL_VALUE_BARE: return "BARE";
   case ANVL_VALUE_VARREF: return "VARREF";
   case ANVL_VALUE_FUNCTION: return "FUNCTION";
   }
   return "UNKNOWN";
}

static const char *public_type_name(anvil_value_type t) {
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

static const char *stmt_kind_name(anvl_stmt_kind k) {
   switch (k) {
   case ANVL_STMT_ASSIGN: return "ASSIGN";
   case ANVL_STMT_OBJECT_BLOCK: return "OBJECT_BLOCK";
   case ANVL_STMT_VARS: return "VARS";
   case ANVL_STMT_IMPORT: return "IMPORT";
   case ANVL_STMT_FUNCTION: return "FUNCTION";
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

/* ---------------------------------------------------------------------- *
 * Nodes
 * ---------------------------------------------------------------------- */

typedef struct {
   module_document *docs;
   size_t count;
   size_t cap;
} doc_set;
static const doc_set *g_docs; // the documents being dumped, to name the owner of a resolved reference

static void dump_value(anvl_value v, int depth);
static void dump_statement(anvl_statement s, int depth);

static void dump_numeric(anvl_value v) {
   anvil_numeric_t n = anvil_value_get_numeric((anvil_value)v);
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

static void dump_string_decoded(anvl_value v) {
   size_t need = anvil_value_get_text((anvil_value)v, NULL, 0);
   char *buf = malloc(need + 1);
   if (!buf) {
      return;
   }
   anvil_value_get_text((anvil_value)v, buf, need + 1);
   fputs(" decoded=", stdout);
   dump_put_quoted(buf, need);
   free(buf);
}

static void dump_value(anvl_value v, int depth) {
   put_indent(depth);
   if (!v) {
      puts("value type=NONE");
      return;
   }
   printf("value type=%s", value_type_name(v->type));
   put_span("span", v->text);
   put_text("text", v->text);
   switch (v->type) {
   case ANVL_VALUE_BOOL:
      printf(" bool=%s", (slice_len(v->text) == 4 && memcmp(v->text.start, "true", 4) == 0) ? "true" : "false");
      break;
   case ANVL_VALUE_NUMERIC:
      dump_numeric(v);
      break;
   case ANVL_VALUE_STRING:
      dump_string_decoded(v);
      break;
   case ANVL_VALUE_BLOB:
      fputs(" tag=", stdout);
      dump_put_quoted(v->blob.tag.start, slice_len(v->blob.tag));
      put_span("tag-span", v->blob.tag);
      fputs(" bytes=", stdout);
      put_hex(v->text.start, slice_len(v->text));
      break;
   case ANVL_VALUE_ARRAY:
   case ANVL_VALUE_TUPLE:
      printf(" count=%zu", (size_t)List.size(v->collection.items));
      break;
   case ANVL_VALUE_OBJECT:
      printf(" count=%zu", (size_t)List.size(v->object.statements));
      break;
   case ANVL_VALUE_VARREF:
      put_text("target", v->varref.target);
      printf(" public=%s resolved=%d", public_type_name(anvil_value_get_type((anvil_value)v)),
             v->varref.resolved ? 1 : 0);
      if (v->varref.resolved) {
         // Only a reference to the target, which is dumped where it is declared: expanding it here
         // would make a chain of references print exponentially many lines.
         const anvl_slice t = v->varref.resolved->text;
         long owner = -1;
         for (size_t i = 0; g_docs && i < g_docs->count; i++) {
            if (g_docs->docs[i]->source && (const char *)g_docs->docs[i]->source->buffer.bucket == t.data) {
               owner = (long)i;
            }
         }
         printf(" resolved-doc=%ld", owner);
         put_span("resolved-span", t);
      }
      break;
   default:
      break;
   }
   putchar('\n');

   if (v->type == ANVL_VALUE_ARRAY || v->type == ANVL_VALUE_TUPLE) {
      usize n = List.size(v->collection.items);
      for (usize i = 0; i < n; i++) {
         anvl_value e = NULL;
         List.get(v->collection.items, i, (object *)&e);
         dump_value(e, depth + 1);
      }
   } else if (v->type == ANVL_VALUE_OBJECT) {
      usize n = List.size(v->object.statements);
      for (usize i = 0; i < n; i++) {
         anvl_statement s = NULL;
         List.get(v->object.statements, i, (object *)&s);
         dump_statement(s, depth + 1);
      }
   }
}

static void dump_attribute(anvl_attribute a, int depth) {
   put_indent(depth);
   fputs("attr key=", stdout);
   dump_put_quoted(a->key.start, slice_len(a->key));
   put_span("key-span", a->key);
   put_text("raw", a->value);
   if (slice_len(a->value) == 0) {
      fputs(" raw-span=none flag=1\n", stdout);
   } else {
      put_span("raw-span", a->value);
      fputs(" flag=0\n", stdout);
   }
   if (a->decoded) {
      dump_value(a->decoded, depth + 1);
   }
}

static void dump_statement(anvl_statement s, int depth) {
   put_indent(depth);
   if (!s) {
      puts("stmt kind=NONE");
      return;
   }
   usize nattrs = List.size(s->attributes);
   printf("stmt kind=%s", stmt_kind_name(s->kind));
   // s->span is never filled in by the parser, so a statement has no span of its own to dump.
   put_text("name", s->name);
   put_span("name-span", s->name);
   put_text("base", s->base);
   put_span("base-span", s->base);
   printf(" attrs=%zu\n", (size_t)nattrs);
   for (usize i = 0; i < nattrs; i++) {
      anvl_attribute a = NULL;
      List.get(s->attributes, i, (object *)&a);
      if (a) {
         dump_attribute(a, depth + 1);
      }
   }
   if (s->value) {
      dump_value(s->value, depth + 1);
   }
}

/* ---------------------------------------------------------------------- *
 * Documents (the root, then each document its includes resolved to)
 * ---------------------------------------------------------------------- */

static long doc_index(const doc_set *set, module_document d) {
   for (size_t i = 0; i < set->count; i++) {
      if (set->docs[i] == d) {
         return (long)i;
      }
   }
   return -1;
}

// Pre-order walk of the include graph; a document reached twice (a diamond) keeps its first index.
static void collect_docs(doc_set *set, module_document d) {
   if (!d || doc_index(set, d) >= 0) {
      return;
   }
   if (set->count == set->cap) {
      set->cap = set->cap ? set->cap * 2 : 8;
      set->docs = realloc(set->docs, set->cap * sizeof *set->docs);
      if (!set->docs) {
         exit(2);
      }
   }
   set->docs[set->count++] = d;
   if (d->header && d->header->includes) {
      usize n = List.size(d->header->includes);
      for (usize i = 0; i < n; i++) {
         anvl_include inc = NULL;
         List.get(d->header->includes, i, (object *)&inc);
         if (inc) {
            collect_docs(set, inc->resolved);
         }
      }
   }
}

// A document's path as the dump shows it: "<root>" for the root, otherwise relative to the root's
// directory when it lies under it (so the dump does not depend on where the corpus is checked out).
static void put_doc_path(module_document d, module_document root) {
   if (d == root) {
      fputs("\"<root>\"", stdout);
      return;
   }
   const char *p = d->filepath ? d->filepath : "";
   const char *rp = root->filepath ? root->filepath : "";
   const char *slash = strrchr(rp, '/');
   size_t dirlen = slash ? (size_t)(slash - rp) + 1 : 0;
   if (dirlen && strncmp(p, rp, dirlen) == 0) {
      p += dirlen;
   }
   dump_put_quoted(p, strlen(p));
}

static void dump_document(const doc_set *set, size_t index) {
   module_document d = set->docs[index];
   module_document root = set->docs[0];
   printf("doc %zu path=", index);
   put_doc_path(d, root);
   printf(" dialect=%s bytes=%zu\n", dialect_name(d->source->dialect), (size_t)d->source->length);

   if (d->header && d->header->attributes) {
      usize n = List.size(d->header->attributes);
      for (usize i = 0; i < n; i++) {
         anvl_attribute a = NULL;
         List.get(d->header->attributes, i, (object *)&a);
         if (a) {
            dump_attribute(a, 1);
         }
      }
   }
   if (d->header && d->header->includes) {
      usize n = List.size(d->header->includes);
      for (usize i = 0; i < n; i++) {
         anvl_include inc = NULL;
         List.get(d->header->includes, i, (object *)&inc);
         if (!inc) {
            continue;
         }
         put_indent(1);
         fputs("include", stdout);
         put_span("decl-span", inc->decl);
         put_text("decl", inc->decl);
         put_span("path-span", inc->path);
         put_text("path", inc->path);
         if (inc->resolved) {
            printf(" resolved=%ld\n", doc_index(set, inc->resolved));
         } else {
            fputs(" resolved=none\n", stdout);
         }
      }
   }
   if (d->body) {
      sc_queryable q = FArray.as_queryable(d->body, sizeof(anvl_statement));
      const void *element = NULL;
      while (Query.next(&q, &element, NULL)) {
         dump_statement(*(anvl_statement *)element, 1);
      }
   }
}

/* ---------------------------------------------------------------------- *
 * Errors
 * ---------------------------------------------------------------------- */

static void dump_errors(anvil_document doc, module_context ctx) {
   anvil_err_code cat = anvil_get_error(doc);
   usize n = ctx && ctx->errors ? List.size(ctx->errors) : 0;
   for (usize i = 0; i < n; i++) {
      anvl_error e = anvl_error_get(ctx->errors, i);
      if (!e) {
         continue;
      }
      printf("error code=%d name=%s category=%d category-name=%s line=%zu column=%zu message=",
             (int)e->code, anvl_error_code_name(e->code), (int)cat, category_name(cat),
             (size_t)e->line, (size_t)e->column);
      const char *m = e->message ? e->message : "";
      dump_put_quoted(m, strlen(m));
      putchar('\n');
   }
   if (n == 0) {
      // the load failed before anything was recorded internally (an unreadable file, say)
      anvil_error pub = anvil_document_get_error(doc);
      printf("error code=0 name=ANVL_ERR_NONE category=%d category-name=%s line=%zu column=%zu message=",
             (int)cat, category_name(cat), anvil_error_get_line(pub), anvil_error_get_column(pub));
      size_t need = anvil_error_get_message(pub, NULL, 0);
      char *buf = malloc(need + 1);
      if (buf) {
         anvil_error_get_message(pub, buf, need + 1);
         dump_put_quoted(buf, need);
         free(buf);
      } else {
         dump_put_quoted("", 0);
      }
      putchar('\n');
   }
}

/* ---------------------------------------------------------------------- *
 * Driver
 * ---------------------------------------------------------------------- */

static char *read_all(FILE *f, size_t *len) {
   size_t cap = 4096, n = 0;
   char *buf = malloc(cap);
   if (!buf) {
      return NULL;
   }
   size_t got;
   while ((got = fread(buf + n, 1, cap - n, f)) > 0) {
      n += got;
      if (n == cap) {
         cap *= 2;
         char *nb = realloc(buf, cap);
         if (!nb) {
            free(buf);
            return NULL;
         }
         buf = nb;
      }
   }
   *len = n;
   return buf;
}

// Dumps one input. Returns 0, or 2 when the input cannot even be read for a fragment (a tool error).
static int dump_one(const char *path, bool fragment) {
   bool from_stdin = strcmp(path, "-") == 0;
   size_t len = 0;
   char *bytes = NULL;
   FILE *f = from_stdin ? stdin : fopen(path, "rb");
   if (f) {
      bytes = read_all(f, &len);
      if (!from_stdin) {
         fclose(f);
      }
      if (!bytes) {
         return 2;
      }
   } else if (fragment) {
      fprintf(stderr, "anvil-dump: cannot read %s\n", path);
      return 2;
   }

   anvil_document doc;
   if (fragment) {
      doc = anvil_parse_value_fragment(bytes, len);
   } else if (from_stdin) {
      doc = anvil_load_buffer(bytes, len);
   } else {
      doc = anvil_load(path); // an unreadable file is a native I/O error, which the dump reports
   }
   if (!doc) {
      fputs("anvil-dump: native returned no handle\n", stderr);
      free(bytes);
      return 2;
   }

   module_context ctx = anvil_internal_document_context(doc);
   module_document root = anvil_internal_document_root(doc);
   bool failed = anvil_has_errors(doc);

   printf("anvil-dump %d\n", DUMP_FORMAT_VERSION);
   printf("mode %s\n", fragment ? "fragment" : "document");
   printf("input-bytes %zu\n", len);
   printf("status %s\n", failed ? "error" : "ok");
   if (!fragment && root && root->source) {
      printf("dialect %s\n", dialect_name(root->source->dialect));
   }
   if (failed) {
      dump_errors(doc, ctx);
   } else if (fragment) {
      dump_value(anvil_internal_document_fragment(doc), 0);
   } else {
      doc_set set = {0};
      collect_docs(&set, root);
      g_docs = &set;
      for (size_t i = 0; i < set.count; i++) {
         dump_document(&set, i);
      }
      g_docs = NULL;
      free(set.docs);
   }

   anvil_dispose(doc);
   free(bytes);
   return 0;
}

static bool has_suffix(const char *s, const char *suffix) {
   size_t n = strlen(s), m = strlen(suffix);
   return n >= m && strcmp(s + n - m, suffix) == 0;
}

int main(int argc, char **argv) {
   bool fragment = false, batch = false, writer_script = false, writer_output = false, builder_script = false;
   const char *path = NULL, *emit_dir = NULL;
   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--fragment") == 0) {
         fragment = true;
      } else if (strcmp(argv[i], "--writer-script") == 0) {
         writer_script = true;
      } else if (strcmp(argv[i], "--writer-output") == 0) {
         writer_output = true;
      } else if (strcmp(argv[i], "--builder-script") == 0) {
         builder_script = true;
      } else if (strcmp(argv[i], "--emit-dir") == 0 && i + 1 < argc && !emit_dir) {
         emit_dir = argv[++i];
      } else if (strcmp(argv[i], "--batch") == 0) {
         batch = true;
      } else if (!path) {
         path = argv[i];
      } else {
         path = NULL;
         break;
      }
   }
   if (batch) {
      // Many inputs in one process (for the fuzz harness): paths one per line on stdin; each dump is
      // preceded by a line "@@ <path>", and a ".frag" path is dumped as a value fragment.
      if (path || fragment) {
         fputs("usage: anvil-dump --batch  (paths on stdin)\n", stderr);
         return 2;
      }
      char *line = NULL;
      size_t cap = 0;
      ssize_t got;
      int rc = 0;
      while ((got = getline(&line, &cap, stdin)) > 0) {
         if (line[got - 1] == '\n') {
            line[--got] = '\0';
         }
         if (got == 0) {
            continue;
         }
         printf("@@ %s\n", line);
         if (dump_one(line, has_suffix(line, ".frag")) != 0) {
            rc = 2;
         }
      }
      free(line);
      return rc;
   }
   if (!path || ((writer_script || writer_output || builder_script) && (fragment || (writer_script && writer_output) || (builder_script && (writer_script || writer_output)))) ||
       (emit_dir && !builder_script)) {
      fputs("usage: anvil-dump [--fragment] <file | ->\n       anvil-dump --batch  (paths on stdin)\n"
            "       anvil-dump --writer-script <script> | --writer-output <script>\n"
            "       anvil-dump --builder-script <script> [--emit-dir <dir>]\n", stderr);
      return 2;
   }
   if (builder_script) {
      return builder_script_run(path, emit_dir);
   }
   if (writer_script || writer_output) {
      return writer_script_run(path, writer_output);
   }
   return dump_one(path, fragment);
}
