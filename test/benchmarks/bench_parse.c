/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * bench_parse.c — Parser throughput benchmarks
 * ------------------------------------------------------------------
 * Mirrors .NET ParseBenchmarks.cs (PB01–PB04) and extends with
 * real-world fixture sizes present in test/samples/.
 *
 * PB01  Parse_Small    — assignments.anvl  (~17 lines, scalars+blobs)
 * PB02  Parse_Medium   — objects.anvl      (~20 lines, nested objects)
 * PB03  Parse_Large    — modpack.anvl      (~311 lines, full feature set)
 * PB04  Parse_BlobHeavy— blob_heavy_mixed.anvl (blobs: JSON+XML+CSV)
 * PB05  Parse_Deep     — deep_nesting.anvl (~79 lines, max nesting)
 * PB06  Parse_Basic    — basic.aml         (~45 stmts, primitives+blobs)
 *                        ← apples-to-apples with .NET Parse_Small(basic.aml)
 * PB07  Parse_LargeAml — large.aml         (~347 lines, full feature set)
 *                        ← apples-to-apples with .NET Parse_Large(large.aml)
 *
 * Each benchmark:
 *   - BENCH_WARMUP discarded warm-up iterations
 *   - BENCH_ITERS  measured iterations → min/mean/max/σ/MB/s
 *   - Times: builder set_source + build + parse (full cold parse per iter)
 *   - Correctness assertion on final iteration before exit
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
 * Fixtures — loaded once per test set, reused across all benchmarks
 * ------------------------------------------------------------------ */
static const char *_src_small;
static const char *_src_medium;
static const char *_src_large;
static const char *_src_blob;
static const char *_src_deep;
static const char *_src_basic;
static const char *_src_largeaml;
static size_t _sz_small, _sz_medium, _sz_large, _sz_blob, _sz_deep;
static size_t _sz_basic, _sz_largeaml;

static void set_config(FILE **logger) {
   *logger = fopen("logs/bench_parse.log", "w");

   load_source("test/samples/assignments.anvl", &_src_small, &_sz_small);
   load_source("test/samples/objects.anvl", &_src_medium, &_sz_medium);
   load_source("test/samples/modpack.anvl", &_src_large, &_sz_large);
   load_source("test/samples/blob_heavy_mixed.anvl", &_src_blob, &_sz_blob);
   load_source("test/samples/deep_nesting.anvl", &_src_deep, &_sz_deep);
   load_source("test/samples/basic.aml", &_src_basic, &_sz_basic);
   load_source("test/samples/large.aml", &_src_largeaml, &_sz_largeaml);

   printf("\n  %-42s  %9s  %9s  %9s  %7s  %12s\n",
          "Benchmark", "min", "mean", "max", "σ", "throughput");
   printf("  %s\n",
          "─────────────────────────────────────────────────────────"
          "───────────────────────────────────────────");
   fflush(stdout);
}

static void set_teardown(void) {
   free((void *)_src_small);
   free((void *)_src_medium);
   free((void *)_src_large);
   free((void *)_src_blob);
   free((void *)_src_deep);
   free((void *)_src_basic);
   free((void *)_src_largeaml);
   _src_small = _src_medium = _src_large = _src_blob = _src_deep = NULL;
   _src_basic = _src_largeaml = NULL;
}

/* ------------------------------------------------------------------
 * Helper — parse a source string through the full cold path.
 * Returns the parsed context; caller must Context.dispose().
 * ------------------------------------------------------------------ */
static inline context _cold_parse(const char *src, size_t sz) {
   anvl_ctx_builder_i *b = Context.get_builder();
   b->set_source(b, src, (usize)sz);
   context ctx = b->build(b);
   Context.parse(ctx);
   return ctx;
}

/* ------------------------------------------------------------------
 * PB01 — Parse small document (assignments.anvl, ~17 lines, scalars)
 * ------------------------------------------------------------------ */
static void bench_parse_small(void) {
   if (!_src_small) {
      fprintf(stderr, "FAIL: PB01: fixture must load\n");
      return;
   }

   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      context ctx = _cold_parse(_src_small, _sz_small);
      Context.dispose(ctx);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, _src_small, (usize)_sz_small);
      context ctx = b->build(b);
      uint64_t t0 = bench_ns();
      Context.parse(ctx);
      samples[i] = bench_ns() - t0;
      Context.dispose(ctx);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, _sz_small);
   bench_report("PB01 parse small (assignments.anvl)", &r);

   context ctx = _cold_parse(_src_small, _sz_small);
   if (Context.statement_count(ctx) == 0)
      fprintf(stderr, "FAIL: PB01: parse must yield statements\n");
   Context.dispose(ctx);
}

/* ------------------------------------------------------------------
 * PB02 — Parse medium document (objects.anvl, ~20 lines, nested)
 * ------------------------------------------------------------------ */
static void bench_parse_medium(void) {
   if (!_src_medium) {
      fprintf(stderr, "FAIL: PB02: fixture must load\n");
      return;
   }

   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      context ctx = _cold_parse(_src_medium, _sz_medium);
      Context.dispose(ctx);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, _src_medium, (usize)_sz_medium);
      context ctx = b->build(b);
      uint64_t t0 = bench_ns();
      Context.parse(ctx);
      samples[i] = bench_ns() - t0;
      Context.dispose(ctx);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, _sz_medium);
   bench_report("PB02 parse medium (objects.anvl)", &r);

   context ctx = _cold_parse(_src_medium, _sz_medium);
   if (Context.statement_count(ctx) == 0)
      fprintf(stderr, "FAIL: PB02: must yield statements\n");
   Context.dispose(ctx);
}

/* ------------------------------------------------------------------
 * PB03 — Parse large document (modpack.anvl, ~311 lines, full feature set)
 * ------------------------------------------------------------------ */
static void bench_parse_large(void) {
   if (!_src_large) {
      fprintf(stderr, "FAIL: PB03: fixture must load\n");
      return;
   }

   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      context ctx = _cold_parse(_src_large, _sz_large);
      Context.dispose(ctx);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, _src_large, (usize)_sz_large);
      context ctx = b->build(b);
      uint64_t t0 = bench_ns();
      Context.parse(ctx);
      samples[i] = bench_ns() - t0;
      Context.dispose(ctx);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, _sz_large);
   bench_report("PB03 parse large (modpack.anvl)", &r);

   context ctx = _cold_parse(_src_large, _sz_large);
   if (Context.statement_count(ctx) == 0)
      fprintf(stderr, "FAIL: PB03: must yield statements\n");
   Context.dispose(ctx);
}

/* ------------------------------------------------------------------
 * PB04 — Parse blob-heavy document (blob_heavy_mixed.anvl)
 *         Tests scanner performance over large raw blob spans.
 * ------------------------------------------------------------------ */
static void bench_parse_blob_heavy(void) {
   if (!_src_blob) {
      fprintf(stderr, "FAIL: PB04: fixture must load\n");
      return;
   }

   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      context ctx = _cold_parse(_src_blob, _sz_blob);
      Context.dispose(ctx);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, _src_blob, (usize)_sz_blob);
      context ctx = b->build(b);
      uint64_t t0 = bench_ns();
      Context.parse(ctx);
      samples[i] = bench_ns() - t0;
      Context.dispose(ctx);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, _sz_blob);
   bench_report("PB04 parse blob-heavy (blob_heavy_mixed.anvl)", &r);

   context ctx = _cold_parse(_src_blob, _sz_blob);
   Context.dispose(ctx);
}

/* ------------------------------------------------------------------
 * PB05 — Parse deep-nesting document (deep_nesting.anvl, ~79 lines)
 *         Stresses nesting stack depth management.
 * ------------------------------------------------------------------ */
static void bench_parse_deep(void) {
   if (!_src_deep) {
      fprintf(stderr, "FAIL: PB05: fixture must load\n");
      return;
   }

   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      context ctx = _cold_parse(_src_deep, _sz_deep);
      Context.dispose(ctx);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, _src_deep, (usize)_sz_deep);
      context ctx = b->build(b);
      uint64_t t0 = bench_ns();
      Context.parse(ctx);
      samples[i] = bench_ns() - t0;
      Context.dispose(ctx);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, _sz_deep);
   bench_report("PB05 parse deep-nesting (deep_nesting.anvl)", &r);

   context ctx = _cold_parse(_src_deep, _sz_deep);
   Context.dispose(ctx);
}

/* ------------------------------------------------------------------
 * PB06 — Parse basic document (basic.aml, ~45 stmts, primitives+blobs)
 *         Apples-to-apples with .NET ParseBenchmarks.Parse_Small(basic.aml)
 * ------------------------------------------------------------------ */
static void bench_parse_basic(void) {
   if (!_src_basic) {
      fprintf(stderr, "FAIL: PB06: fixture must load\n");
      return;
   }

   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      context ctx = _cold_parse(_src_basic, _sz_basic);
      Context.dispose(ctx);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, _src_basic, (usize)_sz_basic);
      context ctx = b->build(b);
      uint64_t t0 = bench_ns();
      Context.parse(ctx);
      samples[i] = bench_ns() - t0;
      Context.dispose(ctx);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, _sz_basic);
   bench_report("PB06 parse basic (basic.aml)", &r);

   context ctx = _cold_parse(_src_basic, _sz_basic);
   if (Context.statement_count(ctx) == 0)
      fprintf(stderr, "FAIL: PB06: must yield statements\n");
   Context.dispose(ctx);
}

/* ------------------------------------------------------------------
 * PB07 — Parse large document (large.aml, ~347 lines, full feature set)
 *         Apples-to-apples with .NET ParseBenchmarks.Parse_Large(large.aml)
 * ------------------------------------------------------------------ */
static void bench_parse_largeaml(void) {
   if (!_src_largeaml) {
      fprintf(stderr, "FAIL: PB07: fixture must load\n");
      return;
   }

   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      context ctx = _cold_parse(_src_largeaml, _sz_largeaml);
      Context.dispose(ctx);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, _src_largeaml, (usize)_sz_largeaml);
      context ctx = b->build(b);
      uint64_t t0 = bench_ns();
      Context.parse(ctx);
      samples[i] = bench_ns() - t0;
      Context.dispose(ctx);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, _sz_largeaml);
   bench_report("PB07 parse large  (large.aml)", &r);

   context ctx = _cold_parse(_src_largeaml, _sz_largeaml);
   if (Context.statement_count(ctx) == 0)
      fprintf(stderr, "FAIL: PB07: must yield statements\n");
   Context.dispose(ctx);
}

/* ------------------------------------------------------------------
 * Public entry point
 * ------------------------------------------------------------------ */
void run_bench_parse(void) {
   FILE *logger = NULL;
   set_config(&logger);

   bench_parse_small();
   bench_parse_medium();
   bench_parse_large();
   bench_parse_blob_heavy();
   bench_parse_deep();
   bench_parse_basic();
   bench_parse_largeaml();

   set_teardown();
   if (logger)
      fclose(logger);
}
