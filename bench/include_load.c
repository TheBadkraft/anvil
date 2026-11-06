/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * ---------------------------------------------------------------------- *
 * include_load.c - per-load regression bench for FILE loads with includes
 * (BR-2611-anvl-include-001). The other benches load buffers, which never
 * reach include processing. This one writes its own documents to a temp
 * directory (removed on exit) and prints "workload ms-per-load" lines, the
 * format bench/compare.sh reads. Lower is faster; best of 7 timed batches.
 *
 * Build: gcc -O2 -std=c2x -Iinclude -D_POSIX_C_SOURCE=200809L bench/include_load.c \
 *            lib/release/libanvil.a -o include_load
 * ********************************************************************** */
#define _XOPEN_SOURCE 700
#include "anvil_flat.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static char dir[256];
static int made;
static char made_names[400][64];

static double now(void) {
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void put(const char *name, const char *text, int append) {
   char p[400];
   snprintf(p, sizeof p, "%s/%s", dir, name);
   FILE *f = fopen(p, append ? "ab" : "wb");
   if (!f) {
      perror(p);
      exit(2);
   }
   fputs(text, f);
   fclose(f);
   if (!append) {
      snprintf(made_names[made++], sizeof made_names[0], "%s", name);
   }
}

static void cleanup(void) {
   char p[400];
   for (int i = 0; i < made; i++) {
      snprintf(p, sizeof p, "%s/%s", dir, made_names[i]);
      unlink(p);
   }
   rmdir(dir);
}

static void bench(const char *name, const char *file, int n) {
   char path[400];
   snprintf(path, sizeof path, "%s/%s", dir, file);
   for (int i = 0; i < 20; i++) {
      anvil_dispose(anvil_load(path));
   }
   double best = 1e18;
   for (int r = 0; r < 7; r++) {
      double t0 = now();
      for (int i = 0; i < n; i++) {
         anvil_document d = anvil_load(path);
         if (!d || anvil_has_errors(d)) {
            fprintf(stderr, "%s: load failed\n", name);
            exit(1);
         }
         anvil_dispose(d);
      }
      double ms = (now() - t0) / n;
      if (ms < best) {
         best = ms;
      }
   }
   printf("%-20s %.5f\n", name, best);
}

int main(void) {
   char t[] = "/tmp/anvl_incbench_XXXXXX";
   if (!mkdtemp(t)) {
      return 2;
   }
   snprintf(dir, sizeof dir, "%s", t);
   atexit(cleanup);
   char name[64], text[128];

   put("flat.anvl", "a := 1; b := \"x\";\n", 0);
   for (int n = 10; n <= 100; n *= 10) {
      snprintf(name, sizeof name, "inc%d.anvl", n);
      put(name, "", 0);
      for (int i = 1; i <= n; i++) {
         snprintf(text, sizeof text, "k%d := %d;\n", i, i);
         char child[64];
         snprintf(child, sizeof child, "m%d_%d.anvl", n, i);
         put(child, text, 0);
         snprintf(text, sizeof text, "include \"%s\";\n", child);
         put(name, text, 1);
      }
      put(name, "r := 1;\n", 1);
   }
   for (int i = 20; i >= 1; i--) {
      snprintf(name, sizeof name, "c%d.anvl", i);
      text[0] = 0;
      if (i < 20) {
         snprintf(text, sizeof text, "include \"c%d.anvl\";\n", i + 1);
      }
      put(name, text, 0);
      snprintf(text, sizeof text, "v%d := %d;\n", i, i);
      put(name, text, 1);
   }

   bench("load_no_include", "flat.anvl", 3000);
   bench("load_10_includes", "inc10.anvl", 1000);
   bench("load_100_includes", "inc100.anvl", 200);
   bench("load_chain_20_deep", "c1.anvl", 500);
   return 0;
}
