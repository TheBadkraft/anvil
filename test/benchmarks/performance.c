/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * performance.c — Parser throughput sweep (all sample files)
 * ------------------------------------------------------------------
 * Runs a single-pass throughput measurement over every sample file
 * in test/samples/ and prints a summary table.  Each file is one
 * testcase; the bench asserts parse success and positive throughput.
 *
 * This file is the rtest-compatible successor to the original
 * standalone-main performance runner.  It covers the same set of
 * sample files but integrates with the rtest suite so it participates
 * in CI passes and failure reporting.
 *
 * For statistical (min/mean/max/σ) benchmarks see:
 *   test/benchmarks/bench_parse.c      — focused parse stats
 *   test/benchmarks/bench_resolver.c   — resolver cold/warm
 *   test/benchmarks/bench_serializer.c — writer throughput
 *   test/benchmarks/bench_asl.c        — ASL engine throughput
 * ------------------------------------------------------------------
 */
#include "anvil.h"
#include "benchmarks/bench.h"
#include "utils.h"
#include <sigma.memory/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Sample files to sweep (relative to project root)
 * ------------------------------------------------------------------ */
typedef struct {
   const char *path;
   const char *label;
} sample_entry_t;

static const sample_entry_t _samples[] = {
    {"test/samples/assignments.anvl", "assignments.anvl"},
    {"test/samples/objects.anvl", "objects.anvl"},
    {"test/samples/inherits.anvl", "inherits.anvl"},
    {"test/samples/numbers.anvl", "numbers.anvl"},
    {"test/samples/attributes.anvl", "attributes.anvl"},
    {"test/samples/arrays.anvl", "arrays.anvl"},
    {"test/samples/tuples.anvl", "tuples.anvl"},
    {"test/samples/modpack.anvl", "modpack.anvl"},
    {"test/samples/large_dataset.anvl", "large_dataset.anvl"},
    {"test/samples/complex_nested.anvl", "complex_nested.anvl"},
    {"test/samples/repetitive_data.anvl", "repetitive_data.anvl"},
    {"test/samples/deep_nesting.anvl", "deep_nesting.anvl"},
    {"test/samples/blob_heavy_json.anvl", "blob_heavy_json.anvl"},
    {"test/samples/blob_heavy_xml.anvl", "blob_heavy_xml.anvl"},
    {"test/samples/blob_heavy_csv.anvl", "blob_heavy_csv.anvl"},
    {"test/samples/blob_heavy_mixed.anvl", "blob_heavy_mixed.anvl"},
    {"test/samples/amp_envelope.anvl", "amp_envelope.anvl"},
};
#define SAMPLE_COUNT ((int)(sizeof(_samples) / sizeof(_samples[0])))

static void set_config(FILE **logger) {
   *logger = fopen("logs/performance.log", "w");
   printf("\n  %-28s  %8s  %10s  %6s  %10s\n",
          "File", "Size", "Parse time", "Stmts", "Throughput");
   printf("  %s\n",
          "─────────────────────────────────────────────────────────────────");
   fflush(stdout);
}

static void set_teardown(void) {
}

/* ------------------------------------------------------------------
 * Core: run BENCH_ITERS parse iterations for a single file, print
 * one summary line, and assert correctness.
 * ------------------------------------------------------------------ */
static void _run_file(int idx) {
   const char *path = _samples[idx].path;
   const char *label = _samples[idx].label;

   size_t sz = 0;
   const char *src = NULL;
   load_source(path, &src, &sz);
   if (!src) {
      printf("  %-28s  MISSING\n", label);
      fflush(stdout);
      fprintf(stderr, "FAIL: performance: sample file must exist: %s\n", path);
      return;
   }

   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, src, (usize)sz);
      context ctx = b->build(b);
      Context.parse(ctx);
      Context.dispose(ctx);
   }

   usize stmt_count = 0;
   for (int i = 0; i < BENCH_ITERS; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, src, (usize)sz);
      context ctx = b->build(b);
      uint64_t t0 = bench_ns();
      Context.parse(ctx);
      samples[i] = bench_ns() - t0;
      stmt_count = Context.statement_count(ctx);
      Context.dispose(ctx);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, sz);
   free((void *)src);

   char sz_buf[32], time_buf[32];
   if (sz < 1024)
      snprintf(sz_buf, sizeof sz_buf, "%zu B", sz);
   else if (sz < 1024 * 1024)
      snprintf(sz_buf, sizeof sz_buf, "%.1f KB", (double)sz / 1024.0);
   else
      snprintf(sz_buf, sizeof sz_buf, "%.1f MB", (double)sz / (1024.0 * 1024.0));
   bench_fmt_ns((uint64_t)r.mean_ns, time_buf, (int)sizeof time_buf);

   printf("  %-28s  %8s  %10s  %6zu  %8.2f MB/s\n",
          label, sz_buf, time_buf, (size_t)stmt_count, r.throughput_mbps);
   fflush(stdout);

   if (r.throughput_mbps <= 0)
      fprintf(stderr, "FAIL: performance: throughput must be positive: %s\n", label);
}

/* One testcase per sample file */
#define DECL_BENCH(idx) \
   static void bench_file_##idx(void) { _run_file(idx); }

DECL_BENCH(0)
DECL_BENCH(1)
DECL_BENCH(2)
DECL_BENCH(3)
    DECL_BENCH(4) DECL_BENCH(5) DECL_BENCH(6) DECL_BENCH(7)
        DECL_BENCH(8) DECL_BENCH(9) DECL_BENCH(10) DECL_BENCH(11)
            DECL_BENCH(12) DECL_BENCH(13) DECL_BENCH(14) DECL_BENCH(15)
                DECL_BENCH(16)
#undef DECL_BENCH

    /* ------------------------------------------------------------------
     * Public entry point
     * ------------------------------------------------------------------ */
    void run_performance(void) {
   FILE *logger = NULL;
   set_config(&logger);

   bench_file_0();
   bench_file_1();
   bench_file_2();
   bench_file_3();
   bench_file_4();
   bench_file_5();
   bench_file_6();
   bench_file_7();
   bench_file_8();
   bench_file_9();
   bench_file_10();
   bench_file_11();
   bench_file_12();
   bench_file_13();
   bench_file_14();
   bench_file_15();
   bench_file_16();

   set_teardown();
   if (logger)
      fclose(logger);
}
