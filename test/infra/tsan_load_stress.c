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
 * ********************************************************************** */
#include "anvil_flat.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREADS 8
#define ROUNDS 200

static atomic_int g_failures;
static const char *g_mode;
static pthread_barrier_t g_barrier;

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
   if (strcmp(g_mode, "different") && strcmp(g_mode, "identical") && strcmp(g_mode, "diamond")) {
      fprintf(stderr, "usage: %s different|identical|diamond\n", argv[0]);
      return 2;
   }
   pthread_barrier_init(&g_barrier, NULL, THREADS);
   pthread_t threads[THREADS];
   for (long i = 0; i < THREADS; i++) {
      pthread_create(&threads[i], NULL, worker, (void *)i);
   }
   for (int i = 0; i < THREADS; i++) {
      pthread_join(threads[i], NULL);
   }
   int failures = atomic_load(&g_failures);
   printf("tsan_load_stress %s: %d threads x %d rounds, %d failures\n", g_mode, THREADS, ROUNDS,
          failures);
   return failures ? 1 : 0;
}
