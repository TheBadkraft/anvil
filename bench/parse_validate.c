/* bench/parse_validate.c - parse and schema-validate cost, for per-commit regression bisects.
 *
 * Links against lib/release/libanvil.a (the shipped artifact). Uses only API present since
 * v0.10.0-rc (anvil_load_buffer, anvil_schema_load, anvil_schema_validate), so the same source
 * builds against any commit in e512b92..main.
 *
 * Workloads: floats (10k rows x 10 floats, e+3 form), ints (10k x 10 integers), mixed (a
 * FlyWire-style message x 10k: strings, ints, floats, bools, nested object, array), and
 * validate (schema validation of the floats document). The refs_* workloads (BR-2611-anvl-resolver-002) are documents
 * full of '$' references, acyclic and adversarial: a chain of 100k statements, a ladder of two references each, a hub
 * of 100k references to one object, a base inherited by 20k statements, a base whose one big field (array or object of 5k) is inherited by 5k statements, a chain of plain references, mixed rows that
 * each hold a reference, and a ring of 100k statements (a reference cycle: loaded clean before BR-2611-anvl-resolver-002,
 * refused since; timed either way, so the cost of finding it is in the number). Each is timed as the median of SAMPLES
 * samples; one sample is REPS back-to-back calls. Output: "name median_ms_per_call".
 * Use bench/compare.sh to interleave several builds, pin the CPU and compare medians. */
#define _POSIX_C_SOURCE 200809L
#include "anvil_flat.h"
#include "anvil_schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int ROWS = 10000; /* BENCH_ROWS overrides (small values for valgrind instruction counts) */
#define SAMPLES_DEFAULT 21
static int SAMPLES = SAMPLES_DEFAULT;
static int REPS = 3;

static double now_ms(void) {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int cmp_d(const void *a, const void *b) {
   double x = *(const double *)a, y = *(const double *)b;
   return (x > y) - (x < y);
}

static char *gen_numeric(int floats, size_t *len) {
   size_t cap = 64 + (size_t)ROWS * 10 * 24;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap, "#!aml\nrows := [\n");
   for (int r = 0; r < ROWS; r++) {
      p += (size_t)snprintf(b + p, cap - p, "{");
      for (int c = 0; c < 10; c++) {
         if (floats) {
            p += (size_t)snprintf(b + p, cap - p, " f%d := %d.%de+3;", c, r % 90 + 1, (r + c) % 100);
         } else {
            p += (size_t)snprintf(b + p, cap - p, " f%d := %d;", c, r * 10 + c);
         }
      }
      p += (size_t)snprintf(b + p, cap - p, " }%s\n", r + 1 < ROWS ? "," : "");
   }
   p += (size_t)snprintf(b + p, cap - p, "];\n");
   *len = p;
   return b;
}

static char *gen_mixed(size_t *len) {
   size_t cap = 256 + (size_t)ROWS * 400;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap, "#!aml\nmsgs := [\n");
   for (int r = 0; r < ROWS; r++) {
      p += (size_t)snprintf(
          b + p, cap - p,
          "{ id := %d; kind := \"update\"; user := user_%d; ok := %s; ts := %d.%03d; temp := -%d.5e-2;"
          " pos := { x := %d.25; y := %d.75; z := 0; }; tags := [ alpha, beta, \"gamma %d\" ];"
          " note := \"hello world, row %d\"; seq := %d; }%s\n",
          r, r % 977, (r & 1) ? "true" : "false", 1700000000 + r, r % 1000, r % 50, r % 300, r % 200, r,
          r, r * 7, r + 1 < ROWS ? "," : "");
   }
   p += (size_t)snprintf(b + p, cap - p, "];\n");
   *len = p;
   return b;
}

static char *gen_schema(size_t *len) {
   size_t cap = 4096;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap,
                               "#!aml\n@[schema]\nrows := { type := Array; items := { type := Object; fields := {\n");
   for (int c = 0; c < 10; c++) {
      p += (size_t)snprintf(b + p, cap - p, "f%d := { type := Numeric; required := true; };\n", c);
   }
   p += (size_t)snprintf(b + p, cap - p, "}; }; };\n");
   *len = p;
   return b;
}

static void report(const char *name, double *s) {
   qsort(s, SAMPLES, sizeof *s, cmp_d);
   printf("%s %.4f\n", name, s[SAMPLES / 2]);
}

static void bench_load(const char *name, const char *src, size_t len) {
   anvil_document w = anvil_load_buffer(src, len);
   if (anvil_has_errors(w)) {
      fprintf(stderr, "%s: generated document has errors\n", name);
      exit(1);
   }
   anvil_dispose(w);
   double s[64];
   for (int i = 0; i < SAMPLES; i++) {
      double t0 = now_ms();
      for (int r = 0; r < REPS; r++) {
         anvil_document d = anvil_load_buffer(src, len);
         anvil_dispose(d);
      }
      s[i] = (now_ms() - t0) / REPS;
   }
   report(name, s);
}

/* validate_small: a 4-field schema and one small document with an exponent float
 * (`year := 2000.5e+0`; the signed exponent is a number in every build since v0.10.0-rc), validated in a tight loop -- the shape of the wasm Schema.validate bench.
 * A fresh load is not part of the timing; only anvil_schema_validate is. */
#define SMALL_SCHEMA \
   "#!aml\n@[schema]\n" \
   "title := { type := String; required := true; };\n" \
   "year := { type := Numeric; required := true; min := 0; max := 3000; };\n" \
   "rating := { type := Numeric; required := true; };\n" \
   "ok := { type := Bool; required := true; };\n"
#define SMALL_DOC "#!aml\ntitle := \"Dune\";\nyear := 2000.5e+0;\nrating := 4.25e+0;\nok := true;\n"

/* Like bench_load, but the document may be refused (the ring): only the time of the load and dispose is measured. */
static void bench_load_any(const char *name, const char *src, size_t len) {
   anvil_dispose(anvil_load_buffer(src, len));
   double s[64];
   for (int i = 0; i < SAMPLES; i++) {
      double t0 = now_ms();
      for (int r = 0; r < REPS; r++) {
         anvil_dispose(anvil_load_buffer(src, len));
      }
      s[i] = (now_ms() - t0) / REPS;
   }
   report(name, s);
}

#define REF_N 100000
/* n statements s0..s(n-1), each an array holding a reference to the next (and to the one after when `ladder`);
 * the last holds a scalar, or refers to s0 when `ring`. */
static char *gen_ref_chain(size_t n, int ladder, int ring, size_t *len) {
   size_t cap = 64 + n * 56;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap, "#!aml\n");
   for (size_t i = 0; i < n; i++) {
      p += (size_t)snprintf(b + p, cap - p, "s%zu := [ ", i);
      int any = 0;
      if (i + 1 < n || ring) {
         p += (size_t)snprintf(b + p, cap - p, "$s%zu", (i + 1) % n);
         any = 1;
      }
      if (ladder && (i + 2 < n || ring)) {
         p += (size_t)snprintf(b + p, cap - p, "%s$s%zu", any ? ", " : "", (i + 2) % n);
         any = 1;
      }
      if (!any) {
         p += (size_t)snprintf(b + p, cap - p, "1");
      }
      p += (size_t)snprintf(b + p, cap - p, " ];\n");
   }
   *len = p;
   return b;
}

/* a target object and a hub object holding n references to it */
static char *gen_ref_hub(size_t n, size_t *len) {
   size_t cap = 128 + n * 24;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap, "#!aml\nt := { v := 1; };\nhub := {\n");
   for (size_t i = 0; i < n; i++) {
      p += (size_t)snprintf(b + p, cap - p, "f%zu := $t;\n", i);
   }
   p += (size_t)snprintf(b + p, cap - p, "};\n");
   *len = p;
   return b;
}

/* a base holding a reference, and n statements that inherit it */
static char *gen_ref_inherit(size_t n, size_t *len) {
   size_t cap = 160 + n * 40;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap, "#!aml\nt := { v := 1; };\nbase := { r := $t; w := 2; };\n");
   for (size_t i = 0; i < n; i++) {
      p += (size_t)snprintf(b + p, cap - p, "d%zu : base := { p := %zu; };\n", i, i);
   }
   *len = p;
   return b;
}

/* a base holding one big field (an array of m elements, or an object of m fields) inherited by m statements, and one
 * reference that turns the cycle check on: the cost must not be m times the size of the field */
static char *gen_ref_inherit_wide(size_t m, int nested, size_t *len) {
   size_t cap = 256 + m * 56;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap, "#!aml\nbase := { big := %s", nested ? "{ " : "[ ");
   for (size_t i = 0; i < m; i++) {
      p += (size_t)snprintf(b + p, cap - p, nested ? "f%zu := 1; " : "1, ", i);
   }
   p += (size_t)snprintf(b + p, cap - p, nested ? "}; };\n" : " ]; };\n");
   for (size_t i = 0; i < m; i++) {
      p += (size_t)snprintf(b + p, cap - p, "d%zu : base := { x := 1; };\n", i);
   }
   p += (size_t)snprintf(b + p, cap - p, "t := 1;\nr := $t;\n");
   *len = p;
   return b;
}

/* one big field holding m references to one value, inherited by m statements: the cost must be about the document's
 * size, not m times the number of references (BR-2611-anvl-resolver-002, F5) */
static char *gen_ref_inherit_refs(size_t m, size_t *len) {
   size_t cap = 256 + m * 48;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap, "#!aml\nt := 1;\nbase := { a := [ ");
   for (size_t i = 0; i < m; i++) {
      p += (size_t)snprintf(b + p, cap - p, "$t, ");
   }
   p += (size_t)snprintf(b + p, cap - p, "1 ]; };\n");
   for (size_t i = 0; i < m; i++) {
      p += (size_t)snprintf(b + p, cap - p, "d%zu : base := { x := 1; };\n", i);
   }
   *len = p;
   return b;
}

/* a plain chain a0 := $a1; ... (each resolves by chasing the whole chain: the resolver's own cost, not the cycle check's) */
static char *gen_ref_plain_chain(size_t n, size_t *len) {
   size_t cap = 64 + n * 32;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap, "#!aml\n");
   for (size_t i = 0; i + 1 < n; i++) {
      p += (size_t)snprintf(b + p, cap - p, "a%zu := $a%zu;\n", i, i + 1);
   }
   p += (size_t)snprintf(b + p, cap - p, "a%zu := { v := 1; };\n", n - 1);
   *len = p;
   return b;
}

/* the mixed document, each row also holding a reference to one shared object */
static char *gen_mixed_refs(size_t *len) {
   size_t cap = 256 + (size_t)ROWS * 420;
   char *b = malloc(cap);
   size_t p = (size_t)snprintf(b, cap, "#!aml\nshared := { region := eu; zone := 3; };\nmsgs := [\n");
   for (int r = 0; r < ROWS; r++) {
      p += (size_t)snprintf(
          b + p, cap - p,
          "{ id := %d; kind := \"update\"; user := user_%d; ok := %s; ts := %d.%03d; temp := -%d.5e-2;"
          " pos := { x := %d.25; y := %d.75; z := 0; }; tags := [ alpha, beta, \"gamma %d\" ];"
          " note := \"hello world, row %d\"; seq := %d; where := $shared; }%s\n",
          r, r % 977, (r & 1) ? "true" : "false", 1700000000 + r, r % 1000, r % 50, r % 300, r % 200, r,
          r, r * 7, r + 1 < ROWS ? "," : "");
   }
   p += (size_t)snprintf(b + p, cap - p, "];\n");
   *len = p;
   return b;
}

static void bench_references(void) {
   size_t len;
   char *t;
   t = gen_ref_chain(REF_N, 0, 0, &len);
   bench_load("refs_chain_100k", t, len);
   free(t);
   t = gen_ref_chain(REF_N, 1, 0, &len);
   bench_load("refs_ladder_100k", t, len);
   free(t);
   t = gen_ref_hub(REF_N, &len);
   bench_load("refs_hub_100k", t, len);
   free(t);
   t = gen_ref_inherit(20000, &len);
   bench_load("refs_inherit_20k", t, len);
   free(t);
   t = gen_ref_inherit_wide(5000, 0, &len);
   bench_load("refs_inherit_wide_array_5k", t, len);
   free(t);
   t = gen_ref_inherit_wide(5000, 1, &len);
   bench_load("refs_inherit_wide_object_5k", t, len);
   free(t);
   t = gen_ref_inherit_refs(5000, &len);
   bench_load("refs_inherit_refs_5k", t, len);
   free(t);
   t = gen_ref_plain_chain(3000, &len);
   bench_load("refs_plain_chain_3k", t, len);
   free(t);
   t = gen_mixed_refs(&len);
   bench_load("parse_mixed_refs", t, len);
   free(t);
   t = gen_ref_chain(REF_N, 0, 1, &len);
   bench_load_any("refs_ring_100k", t, len);
   free(t);
}

static void bench_validate_small(void) {
   anvil_document sdoc = anvil_load_buffer(SMALL_SCHEMA, sizeof SMALL_SCHEMA - 1);
   anvil_schema schema = anvil_schema_load(sdoc);
   anvil_document data = anvil_load_buffer(SMALL_DOC, sizeof SMALL_DOC - 1);
   if (!schema || !anvil_schema_validate(schema, data)) {
      fprintf(stderr, "validate_small: schema did not accept the document\n");
      exit(1);
   }
   enum { PER_SAMPLE = 3000 };
   double s[64];
   for (int i = 0; i < SAMPLES; i++) {
      double t0 = now_ms();
      for (int r = 0; r < PER_SAMPLE * REPS; r++) {
         anvil_schema_validate(schema, data);
      }
      s[i] = (now_ms() - t0) / REPS;
   }
   report("validate_small", s);
   anvil_dispose(data);
   anvil_schema_dispose(schema);
   anvil_dispose(sdoc);
}

int main(void) {
   if (getenv("BENCH_ROWS")) ROWS = atoi(getenv("BENCH_ROWS"));
   if (getenv("BENCH_SAMPLES")) { SAMPLES = atoi(getenv("BENCH_SAMPLES")); if (SAMPLES > 63) SAMPLES = 63; }
   if (getenv("BENCH_REPS")) REPS = atoi(getenv("BENCH_REPS"));
   size_t fl, il, ml, sl;
   char *floats = gen_numeric(1, &fl), *ints = gen_numeric(0, &il), *mixed = gen_mixed(&ml);
   char *schema_src = gen_schema(&sl);

   bench_load("parse_floats", floats, fl);
   bench_load("parse_ints", ints, il);
   bench_load("parse_mixed", mixed, ml);

   anvil_document sdoc = anvil_load_buffer(schema_src, sl);
   anvil_schema schema = anvil_schema_load(sdoc);
   anvil_document data = anvil_load_buffer(floats, fl);
   if (!schema || !anvil_schema_validate(schema, data)) {
      fprintf(stderr, "validate: schema did not accept the generated document\n");
      return 1;
   }
   double s[64];
   for (int i = 0; i < SAMPLES; i++) {
      double t0 = now_ms();
      for (int r = 0; r < REPS; r++) {
         anvil_schema_validate(schema, data);
      }
      s[i] = (now_ms() - t0) / REPS;
   }
   report("validate_floats", s);
   anvil_dispose(data);
   anvil_schema_dispose(schema);
   anvil_dispose(sdoc);
   bench_validate_small();
   if (!getenv("BENCH_NO_REFS")) {
      bench_references();
   }
   free(floats);
   free(ints);
   free(mixed);
   free(schema_src);
   return 0;
}
