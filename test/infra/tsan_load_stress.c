/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * tsan_load_stress.c - concurrent load/read/dispose stress, for TSan     *
 * ---------------------------------------------------------------------- *
 * N threads each load a document, read it and dispose it, repeatedly,    *
 * through the public flat API only. Built with -fsanitize=thread by      *
 * `make check-tsan`; any data race in anvil (or a global it calls) is    *
 * reported by the sanitizer. BR-2610-anvl-registry-001.                  *
 *                                                                        *
 *   tsan_load_stress different   every thread loads text no other thread *
 *                                loads (must always succeed)             *
 *   tsan_load_stress identical   every thread loads the SAME text, all   *
 *                                held alive at once, then read/disposed  *
 *   tsan_load_stress diamond     every thread loads a file whose         *
 *                                include graph contains a diamond        *
 *   tsan_load_stress cycle       every thread loads, from temp files,    *
 *                                a root that includes itself by its      *
 *                                absolute path, one through a symbolic   *
 *                                link and one through a hard link (each  *
 *                                must be include error 4206), and a      *
 *                                diamond spelled several ways (clean)    *
 *                                BR-2611-anvl-include-001                *
 * ********************************************************************** */
#include "anvil_flat.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREADS 8
#define ROUNDS 200

static atomic_int g_failures;
static const char *g_mode;
static pthread_barrier_t g_barrier;
static char g_dir[256];
static const char *const g_cycle_files[] = {"self.anvl", "lnk.anvl", "hard.anvl", "hard2.anvl", "ok.anvl",
                                            "k1.anvl", "k2.anvl", "leaf.anvl", NULL};

static void put_text(const char *name, const char *text) {
   char path[400];
   snprintf(path, sizeof path, "%s/%s", g_dir, name);
   FILE *f = fopen(path, "wb");
   if (f) {
      fputs(text, f);
      fclose(f);
   }
}

static void cycle_setup(void) {
   char t[] = "/tmp/anvl_tsan_XXXXXX", text[600], a[400], b[400];
   if (!mkdtemp(t)) {
      exit(2);
   }
   snprintf(g_dir, sizeof g_dir, "%s", t);
   snprintf(text, sizeof text, "include \"%s/self.anvl\";\nx := 1;\n", g_dir);
   put_text("self.anvl", text);
   put_text("lnk_target.anvl", "include \"lnk.anvl\";\nx := 1;\n");
   snprintf(a, sizeof a, "%s/lnk.anvl", g_dir);
   snprintf(b, sizeof b, "%s/lnk_target.anvl", g_dir);
   if (symlink(b, a) != 0) {
      exit(2);
   }
   put_text("hard.anvl", "include \"hard2.anvl\";\nx := 1;\n");
   snprintf(a, sizeof a, "%s/hard.anvl", g_dir);
   snprintf(b, sizeof b, "%s/hard2.anvl", g_dir);
   if (link(a, b) != 0) {
      exit(2);
   }
   put_text("ok.anvl", "include \"k1.anvl\";\ninclude \"./k2.anvl\";\nr := 1;\n");
   put_text("k1.anvl", "include \"leaf.anvl\";\nk1 := 1;\n");
   snprintf(text, sizeof text, "include \"%s/./leaf.anvl\";\nk2 := 1;\n", g_dir);
   put_text("k2.anvl", text);
   put_text("leaf.anvl", "leaf := 1;\n");
}

static void cycle_cleanup(void) {
   char path[400];
   snprintf(path, sizeof path, "%s/lnk_target.anvl", g_dir);
   unlink(path);
   for (const char *const *n = g_cycle_files; *n; n++) {
      snprintf(path, sizeof path, "%s/%s", g_dir, *n);
      unlink(path);
   }
   rmdir(g_dir);
}

static bool include_cycle_error(const char *name) {
   char path[400];
   snprintf(path, sizeof path, "%s/%s", g_dir, name);
   anvil_document d = anvil_load(path);
   bool ok = d && anvil_get_error(d) == ANVIL_ERR_INCLUDE;
   if (d) {
      anvil_dispose(d);
   }
   return ok;
}

static void fail(const char *what, int thread, int round) {
   atomic_fetch_add(&g_failures, 1);
   fprintf(stderr, "FAIL: %s (thread %d, round %d)\n", what, thread, round);
}

static bool read_back(anvil_document doc, const char *name, long long want) {
   anvil_statement st = anvil_document_find_statement(doc, name);
   if (!st) {
      return false;
   }
   anvil_numeric_t n = anvil_value_get_numeric(anvil_statement_get_value(st));
   return n.is_integral && n.as_int64 == want;
}

static void *worker(void *arg) {
   int id = (int)(long)arg;
   char text[128];
   for (int r = 0; r < ROUNDS; r++) {
      if (strcmp(g_mode, "different") == 0) {
         snprintf(text, sizeof(text), "#!aml\nt%d_r%d := %d;\nother := \"x%d\";\n", id, r, id * 1000 + r, id);
         anvil_document d = anvil_load_buffer(text, strlen(text));
         if (!d || anvil_has_errors(d)) {
            fail("different-text load failed", id, r);
         } else {
            char name[32];
            snprintf(name, sizeof(name), "t%d_r%d", id, r);
            if (!read_back(d, name, id * 1000 + r)) {
               fail("read back wrong value", id, r);
            }
         }
         if (d) {
            anvil_dispose(d);
         }
      } else if (strcmp(g_mode, "identical") == 0) {
         snprintf(text, sizeof(text), "#!aml\nshared_r%d := %d;\n", r, r);
         pthread_barrier_wait(&g_barrier); // all threads load at once, every one stays alive
         anvil_document d = anvil_load_buffer(text, strlen(text));
         pthread_barrier_wait(&g_barrier);
         if (!d || anvil_has_errors(d)) {
            fail("identical-text load failed while others are alive", id, r);
         } else {
            char name[32];
            snprintf(name, sizeof(name), "shared_r%d", r);
            if (!read_back(d, name, r)) {
               fail("read back wrong value", id, r);
            }
         }
         pthread_barrier_wait(&g_barrier);
         if (d) {
            anvil_dispose(d);
         }
      } else if (strcmp(g_mode, "asl") == 0) { // only run when built with WITH_ASL=1
         snprintf(text, sizeof(text),
                  "#!asl\nf%d_r%d (a, b = %d) => { var x = a + b * 2; return x; };\n", id, r, r);
         anvil_document d = anvil_load_buffer(text, strlen(text));
         if (!d || anvil_has_errors(d)) {
            fail("asl function load failed", id, r);
         }
         if (d) {
            anvil_dispose(d);
         }
      } else if (strcmp(g_mode, "cycle") == 0) {
         if (!include_cycle_error("self.anvl")) {
            fail("self-include by absolute path was not an include error", id, r);
         }
         if (!include_cycle_error("lnk_target.anvl")) {
            fail("self-include through a symbolic link was not an include error", id, r);
         }
         if (!include_cycle_error("hard.anvl")) {
            fail("include of a hard link to itself was not an include error", id, r);
         }
         char path[400];
         snprintf(path, sizeof path, "%s/ok.anvl", g_dir);
         anvil_document d = anvil_load(path);
         if (!d || anvil_has_errors(d)) {
            fail("differently spelled diamond was refused", id, r);
         }
         if (d) {
            anvil_dispose(d);
         }
      } else { // diamond
         anvil_document d = anvil_load("test/fixtures/reg_diamond_root.anvl");
         if (!d || anvil_has_errors(d)) {
            fail("diamond load failed", id, r);
         }
         if (d) {
            anvil_dispose(d);
         }
      }
   }
   return NULL;
}

int main(int argc, char **argv) {
   g_mode = argc > 1 ? argv[1] : "different";
   if (strcmp(g_mode, "different") && strcmp(g_mode, "identical") && strcmp(g_mode, "diamond") &&
       strcmp(g_mode, "cycle") && strcmp(g_mode, "asl")) {
      fprintf(stderr, "usage: %s different|identical|diamond|cycle|asl\n", argv[0]);
      return 2;
   }
   if (strcmp(g_mode, "cycle") == 0) {
      cycle_setup();
   }
   pthread_barrier_init(&g_barrier, NULL, THREADS);
   pthread_t threads[THREADS];
   for (long i = 0; i < THREADS; i++) {
      pthread_create(&threads[i], NULL, worker, (void *)i);
   }
   for (int i = 0; i < THREADS; i++) {
      pthread_join(threads[i], NULL);
   }
   if (strcmp(g_mode, "cycle") == 0) {
      cycle_cleanup();
   }
   int failures = atomic_load(&g_failures);
   printf("tsan_load_stress %s: %d threads x %d rounds, %d failures\n", g_mode, THREADS, ROUNDS,
          failures);
   return failures ? 1 : 0;
}
