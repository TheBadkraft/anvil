/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * bench.h — Lightweight benchmark timing harness for anvil benchmarks
 * ------------------------------------------------------------------
 * Usage:
 *   1. Call bench_ns() before and after the hot region; store the
 *      delta in a samples[] array.
 *   2. Call bench_compute(samples, n, data_bytes) to get statistics.
 *   3. Call bench_report(label, &result) to print a result line.
 *
 * Tuning:
 *   Define BENCH_WARMUP / BENCH_ITERS before including to override.
 *
 * Dependencies: <stdint.h>, <stdio.h>, <time.h> only.
 *   Uses __builtin_sqrt to avoid a libm (-lm) link dependency.
 * ------------------------------------------------------------------
 */
/* Feature-test macro must precede ALL system headers */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <time.h>

/* ------------------------------------------------------------------
 * Tuning knobs
 * ------------------------------------------------------------------ */
#ifndef BENCH_WARMUP
#define BENCH_WARMUP 5 /* discarded warm-up iterations           */
#endif
#ifndef BENCH_ITERS
#define BENCH_ITERS 100 /* measured iterations (stats window)     */
#endif

/* ------------------------------------------------------------------
 * Result structure
 * ------------------------------------------------------------------ */
typedef struct {
   uint64_t min_ns;        /* fastest observed iteration             */
   uint64_t max_ns;        /* slowest observed iteration             */
   double mean_ns;         /* arithmetic mean over BENCH_ITERS       */
   double stddev_ns;       /* population std-deviation               */
   double throughput_mbps; /* MB/s if data_bytes > 0, else 0         */
} bench_result_t;

/* ------------------------------------------------------------------
 * Current wall-clock time in nanoseconds (CLOCK_MONOTONIC).
 * ------------------------------------------------------------------ */
static inline uint64_t bench_ns(void) {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ------------------------------------------------------------------
 * Compute statistics from a raw sample array.
 *
 * data_bytes — bytes processed per iteration for throughput calc;
 *              pass 0 to skip throughput (throughput_mbps will be 0).
 * ------------------------------------------------------------------ */
static inline bench_result_t bench_compute(const uint64_t *samples, int n,
                                           size_t data_bytes) {
   uint64_t sum = 0, mn = UINT64_MAX, mx = 0;
   for (int i = 0; i < n; i++) {
      if (samples[i] < mn)
         mn = samples[i];
      if (samples[i] > mx)
         mx = samples[i];
      sum += samples[i];
   }
   double mean = (double)sum / (double)n;
   double var = 0.0;
   for (int i = 0; i < n; i++) {
      double d = (double)samples[i] - mean;
      var += d * d;
   }
   bench_result_t r;
   r.min_ns = mn;
   r.max_ns = mx;
   r.mean_ns = mean;
   r.stddev_ns = __builtin_sqrt(var / (double)n);
   r.throughput_mbps = (data_bytes > 0 && mean > 0.0)
                           ? (double)data_bytes / (1024.0 * 1024.0) / (mean / 1.0e9)
                           : 0.0;
   return r;
}

/* ------------------------------------------------------------------
 * Format nanoseconds into a human-readable string.
 * ------------------------------------------------------------------ */
static inline void bench_fmt_ns(uint64_t ns, char *buf, int bufsz) {
   if (ns < 1000ULL)
      snprintf(buf, (size_t)bufsz, "%llu ns", (unsigned long long)ns);
   else if (ns < 1000000ULL)
      snprintf(buf, (size_t)bufsz, "%.2f µs", (double)ns / 1.0e3);
   else if (ns < 1000000000ULL)
      snprintf(buf, (size_t)bufsz, "%.2f ms", (double)ns / 1.0e6);
   else
      snprintf(buf, (size_t)bufsz, "%.3f s", (double)ns / 1.0e9);
}

/* ------------------------------------------------------------------
 * Print one formatted benchmark result line to stdout.
 *
 * Format (with throughput):
 *   label  min=Xns  mean=Xns  max=Xns  σ=X µs  X.XX MB/s
 * Format (without throughput):
 *   label  min=Xns  mean=Xns  max=Xns  σ=X µs
 * ------------------------------------------------------------------ */
static inline void bench_report(const char *label, const bench_result_t *r) {
   char mn[32], mx[32], av[32];
   bench_fmt_ns(r->min_ns, mn, sizeof mn);
   bench_fmt_ns(r->max_ns, mx, sizeof mx);
   bench_fmt_ns((uint64_t)r->mean_ns, av, sizeof av);
   if (r->throughput_mbps > 0.0)
      printf("  %-42s  min=%-9s  mean=%-9s  max=%-9s  σ=%5.1f µs  %7.2f MB/s\n",
             label, mn, av, mx, r->stddev_ns / 1.0e3, r->throughput_mbps);
   else
      printf("  %-42s  min=%-9s  mean=%-9s  max=%-9s  σ=%5.1f µs\n",
             label, mn, av, mx, r->stddev_ns / 1.0e3);
   fflush(stdout);
}
