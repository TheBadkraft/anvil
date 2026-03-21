/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * bench_serializer.c — Serializer (writer) benchmarks
 * ------------------------------------------------------------------
 * Mirrors .NET WriterBenchmarks.cs (WB01–WB05).
 *
 * SB01  Write_Pretty_Small     — objects.anvl, default (pretty) options
 * SB02  Write_Minified_Small   — objects.anvl, minified options
 * SB03  Write_Pretty_Large     — modpack.anvl, default (pretty) options
 * SB04  Write_Minified_Large   — modpack.anvl, minified options
 * SB05  RoundTrip_Medium       — parse objects.anvl → serialize →
 *                                re-parse; full round-trip cost.
 *
 * Pre-parsed contexts are held in set-scoped globals.
 * Each iteration times only Serializer.serialize() (read path) or
 * the full round-trip (SB05); the string_builder result is disposed
 * after each iteration outside the timed region.
 * ------------------------------------------------------------------
 */
#include "anvil.h"
#include "benchmarks/bench.h"
#include "serializer.h"
#include "utils.h"
#include <sigma.memory/memory.h>
#include <sigma.core/strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Fixtures
 * ------------------------------------------------------------------ */
static context _ctx_small = NULL; /* objects.anvl  */
static context _ctx_large = NULL; /* modpack.anvl  */
static const char *_src_medium = NULL;
static size_t _sz_medium = 0;

static inline context _build_ctx(const char *src, size_t sz) {
   anvl_ctx_builder_i *b = Context.get_builder();
   b->set_source(b, src, (usize)sz);
   context ctx = b->build(b);
   Context.parse(ctx);
   return ctx;
}

static void set_config(FILE **logger) {
   *logger = fopen("logs/bench_serializer.log", "w");

   const char *src_small = NULL;
   load_source("test/samples/objects.anvl", &src_small, &_sz_medium);
   _src_medium = src_small; /* keep for SB05 round-trip */

   const char *src_large = NULL;
   size_t sz_large;
   load_source("test/samples/modpack.anvl", &src_large, &sz_large);

   _ctx_small = _build_ctx(src_small, _sz_medium);
   _ctx_large = _build_ctx(src_large, sz_large);

   /* Large source no longer needed once context is parsed */
   free((void *)src_large);

   printf("\n  %-42s  %9s  %9s  %9s  %7s\n",
          "Benchmark", "min", "mean", "max", "σ");
   printf("  %s\n",
          "────────────────────────────────────────────────────────────────────────────");
   fflush(stdout);
}

static void set_teardown(void) {
   // Dispose in LIFO order (last created first) — Arena.create pushes R7;
   // Arena.dispose must see the arena as current R7 to pop cleanly.
   Context.dispose(_ctx_large);
   Context.dispose(_ctx_small);
   free((void *)_src_medium);
   _ctx_small = _ctx_large = NULL;
   _src_medium = NULL;
}

/* ------------------------------------------------------------------
 * SB01 — Serialize small document, pretty (default) options
 * ------------------------------------------------------------------ */
static void bench_write_pretty_small(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      string_builder sb = Serializer.serialize(_ctx_small, &ANVL_SERIALIZER_DEFAULT);
      StringBuilder.dispose(sb);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      string_builder sb = Serializer.serialize(_ctx_small, &ANVL_SERIALIZER_DEFAULT);
      samples[i] = bench_ns() - t0;
      StringBuilder.dispose(sb);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("SB01 write pretty small (objects.anvl)", &r);

   string_builder sb = Serializer.serialize(_ctx_small, &ANVL_SERIALIZER_DEFAULT);
   if (!sb)
      fprintf(stderr, "FAIL: SB01: serialized result must not be NULL\n");
   if (sb && StringBuilder.length(sb) == 0)
      fprintf(stderr, "FAIL: SB01: serialized output must be non-empty\n");
   StringBuilder.dispose(sb);
}

/* ------------------------------------------------------------------
 * SB02 — Serialize small document, minified
 * ------------------------------------------------------------------ */
static void bench_write_minified_small(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      string_builder sb = Serializer.serialize(_ctx_small, &ANVL_SERIALIZER_MINIFIED);
      StringBuilder.dispose(sb);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      string_builder sb = Serializer.serialize(_ctx_small, &ANVL_SERIALIZER_MINIFIED);
      samples[i] = bench_ns() - t0;
      StringBuilder.dispose(sb);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("SB02 write minified small", &r);

   string_builder sb = Serializer.serialize(_ctx_small, &ANVL_SERIALIZER_MINIFIED);
   if (!sb)
      fprintf(stderr, "FAIL: SB02: minified result must not be NULL\n");
   StringBuilder.dispose(sb);
}

/* ------------------------------------------------------------------
 * SB03 — Serialize large document, pretty
 *         modpack.anvl: ~311 lines, full feature set
 * ------------------------------------------------------------------ */
static void bench_write_pretty_large(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      string_builder sb = Serializer.serialize(_ctx_large, &ANVL_SERIALIZER_DEFAULT);
      StringBuilder.dispose(sb);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      string_builder sb = Serializer.serialize(_ctx_large, &ANVL_SERIALIZER_DEFAULT);
      samples[i] = bench_ns() - t0;
      StringBuilder.dispose(sb);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("SB03 write pretty large (modpack.anvl)", &r);

   string_builder sb = Serializer.serialize(_ctx_large, &ANVL_SERIALIZER_DEFAULT);
   if (!sb)
      fprintf(stderr, "FAIL: SB03: large pretty result must not be NULL\n");
   if (sb && StringBuilder.length(sb) == 0)
      fprintf(stderr, "FAIL: SB03: large pretty output non-empty\n");
   StringBuilder.dispose(sb);
}

/* ------------------------------------------------------------------
 * SB04 — Serialize large document, minified
 * ------------------------------------------------------------------ */
static void bench_write_minified_large(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      string_builder sb = Serializer.serialize(_ctx_large, &ANVL_SERIALIZER_MINIFIED);
      StringBuilder.dispose(sb);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      string_builder sb = Serializer.serialize(_ctx_large, &ANVL_SERIALIZER_MINIFIED);
      samples[i] = bench_ns() - t0;
      StringBuilder.dispose(sb);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("SB04 write minified large", &r);

   string_builder sb = Serializer.serialize(_ctx_large, &ANVL_SERIALIZER_MINIFIED);
   if (!sb)
      fprintf(stderr, "FAIL: SB04: large minified result must not be NULL\n");
   StringBuilder.dispose(sb);
}

/* ------------------------------------------------------------------
 * SB05 — Round-trip: parse → serialize → re-parse (objects.anvl)
 *         Measures total parse + serialize + re-parse cost in one shot.
 * ------------------------------------------------------------------ */
static void bench_round_trip_medium(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, _src_medium, (usize)_sz_medium);
      context ctx1 = b->build(b);
      if (!ctx1) {
         fprintf(stderr, "FAIL: SB05: warmup build ctx1 failed\n");
         continue;
      }
      Context.parse(ctx1);
      string_builder sb = Serializer.serialize(ctx1, &ANVL_SERIALIZER_DEFAULT);
      if (!sb) {
         fprintf(stderr, "FAIL: SB05: warmup serialize failed\n");
         Context.dispose(ctx1);
         continue;
      }
      anvl_ctx_builder_i *b2 = Context.get_builder();
      /* set_source copies the data — must happen before dispose(sb) releases its arena */
      b2->set_source(b2, StringBuilder.toString(sb), StringBuilder.length(sb));
      StringBuilder.dispose(sb);
      context ctx2 = b2->build(b2);
      if (!ctx2) {
         fprintf(stderr, "FAIL: SB05: warmup build ctx2 failed\n");
         Context.dispose(ctx1);
         continue;
      }
      Context.parse(ctx2);
      Context.dispose(ctx2);
      Context.dispose(ctx1);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();

      anvl_ctx_builder_i *b = Context.get_builder();
      b->set_source(b, _src_medium, (usize)_sz_medium);
      context ctx1 = b->build(b);
      if (!ctx1) {
         fprintf(stderr, "FAIL: SB05: iteration %d build ctx1 failed\n", i);
         samples[i] = 0;
         continue;
      }
      Context.parse(ctx1);
      string_builder sb = Serializer.serialize(ctx1, &ANVL_SERIALIZER_DEFAULT);
      if (!sb) {
         fprintf(stderr, "FAIL: SB05: iteration %d serialize failed\n", i);
         Context.dispose(ctx1);
         samples[i] = 0;
         continue;
      }
      anvl_ctx_builder_i *b2 = Context.get_builder();
      /* set_source copies the data — must happen before dispose(sb) releases its arena */
      b2->set_source(b2, StringBuilder.toString(sb), StringBuilder.length(sb));
      context ctx2 = b2->build(b2);
      if (!ctx2) {
         fprintf(stderr, "FAIL: SB05: iteration %d build ctx2 failed\n", i);
         StringBuilder.dispose(sb);
         Context.dispose(ctx1);
         samples[i] = 0;
         continue;
      }
      Context.parse(ctx2);

      samples[i] = bench_ns() - t0;

      Context.dispose(ctx2);
      StringBuilder.dispose(sb);
      Context.dispose(ctx1);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("SB05 round-trip parse→serialize→parse", &r);

   /* correctness: round-trip must produce non-NULL output and re-parse */
   string_builder sb = Serializer.serialize(_ctx_small, &ANVL_SERIALIZER_DEFAULT);
   if (!sb) {
      fprintf(stderr, "FAIL: SB05: serialized output must not be NULL\n");
      return;
   }
   /* set_source copies before dispose(sb) releases its arena */
   anvl_ctx_builder_i *b = Context.get_builder();
   b->set_source(b, StringBuilder.toString(sb), StringBuilder.length(sb));
   StringBuilder.dispose(sb);
   context ctx2 = b->build(b);
   if (!ctx2) {
      fprintf(stderr, "FAIL: SB05: re-parsed context must not be NULL\n");
      return;
   }
   Context.parse(ctx2);
   Context.dispose(ctx2);
}

/* ------------------------------------------------------------------
 * Public entry point
 * ------------------------------------------------------------------ */
void run_bench_serializer(void) {
   FILE *logger = NULL;
   set_config(&logger);

   bench_write_pretty_small();
   bench_write_minified_small();
   bench_write_pretty_large();
   bench_write_minified_large();
   bench_round_trip_medium();

   set_teardown();
   if (logger)
      fclose(logger);
}
