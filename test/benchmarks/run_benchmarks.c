/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * run_benchmarks.c — Single entry point for the full benchmark suite
 * ------------------------------------------------------------------
 * Drives all benchmark suites in sequence:
 *   1. bench_parse      — parser throughput (PB01–PB07)
 *   2. bench_resolver   — inheritance resolver cold/warm (RB01–RB06)
 *   3. bench_serializer — writer throughput (SB01–SB05)
 *   4. bench_asl        — ASL script engine (AB01–AB07)
 *   5. performance      — full sample sweep (17 files)
 *
 * Build:  ./bench --build
 * Run:    ./bench
 * ------------------------------------------------------------------
 */

void run_bench_parse(void);
void run_bench_resolver(void);
void run_bench_serializer(void);
void run_bench_asl(void);
void run_performance(void);

int main(void) {
   run_bench_parse();
   run_bench_resolver();
   run_bench_serializer();
   run_bench_asl();
   run_performance();
   return 0;
}
