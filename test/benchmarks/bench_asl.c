/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * bench_asl.c — ASL script engine benchmarks
 * ------------------------------------------------------------------
 * Mirrors .NET ScriptEngineBenchmarks.cs (SE01–SE08).
 * Function bodies correspond to the named functions in
 * test/samples/scripts.asl (arith, fib, sumLoop, classify, collatz).
 *
 * AB01  Cold_Parse      — Asl.parse() only (build AST); no exec.
 *                         Measures the cold AST construction cost.
 * AB02  Exec_Arith      — Execute  7 * 3 + 7 - 3  (no params,
 *                         4 ops, no branches, result = 25).
 * AB03  Exec_Classify   — Execute if/else chain: classify score 85
 *                         → "B" (4 branches, string return).
 * AB04  Exec_SumLoop    — Execute for-loop: sum 0..99 → 4950
 *                         (100 iterations, integer accumulation).
 * AB05  Exec_FibIter    — Execute iterative fibonacci to n=20
 *                         → 6765 (20 loop iterations, 4 vars).
 * AB06  Exec_NestedLoop — Execute 10×10 grid accumulation → 2025
 *                         (nested for-loops, 100 mul+add ops).
 * AB07  Exec_Collatz    — Execute collatz(27) → 111 steps.
 *                         Unpredictable branch + variable-length loop;
 *                         corresponds to scripts.asl collatz function.
 *
 * All benchmarks use zero-parameter function bodies (no param_spans
 * needed) to isolate execution cost from argument dispatch.
 * Each testcase: BENCH_WARMUP discarded + BENCH_ITERS measured.
 * Correctness assertion validates result after the timing loop.
 * ------------------------------------------------------------------
 */
#include "anvil.h"
#include "asl.h"
#include "benchmarks/bench.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Function body sources — no parameters, each body is self-contained.
 * body_start = 0 (the opening '{'), body_length = strlen(src).
 * ------------------------------------------------------------------ */

/* AB01 / AB02 — arithmetic: 7*3+7-3 = 25 */
static const char _arith_src[] = "{ return 7 * 3 + 7 - 3; }";

/* AB03 — classify: if/else ladder, score = 85 → "B" */
static const char _classify_src[] =
    "{ score = 85; "
    "if (score >= 90) { return \"A\"; } "
    "if (score >= 80) { return \"B\"; } "
    "if (score >= 70) { return \"C\"; } "
    "return \"F\"; }";

/* AB04 — sum loop: Σ i for i in [0, 99] → 4950 */
static const char _sum_src[] =
    "{ sum = 0; "
    "for (i = 0; i < 100; i++) { sum = sum + i; } "
    "return sum; }";

/* AB05 — iterative fib(20) → 6765 */
static const char _fib_src[] =
    "{ a = 0; b = 1; "
    "for (i = 0; i < 20; i++) { t = a + b; a = b; b = t; } "
    "return a; }";

/* AB06 — nested loop: Σ_{i,j ∈ [0,9]} i*j = 45*45 = 2025 */
static const char _nested_src[] =
    "{ total = 0; "
    "for (i = 0; i < 10; i++) { "
    "  for (j = 0; j < 10; j++) { "
    "    total = total + i * j; "
    "  } "
    "} "
    "return total; }";

/* AB07 — collatz(27): unpredictable branch, variable-length loop → 111 steps */
static const char _collatz_src[] =
    "{ n = 27; steps = 0; "
    "for (; n != 1;) { "
    "  if (n % 2 == 0) { n = n / 2; } "
    "  else { n = n * 3 + 1; } "
    "  steps = steps + 1; "
    "} "
    "return steps; }";

/* ------------------------------------------------------------------
 * Helper — build a zero-parameter function meta from a body string.
 * ------------------------------------------------------------------ */
static inline asl_function_meta_t _make_meta(const char *src) {
   return (asl_function_meta_t){
       .name_start = 0,
       .name_length = 1,
       .param_count = 0,
       .param_spans = NULL,
       .body_start = 0,
       .body_length = (int)strlen(src),
   };
}

/* ------------------------------------------------------------------
 * Helper — execute a zero-param body; returns the result value.
 * Caller must call Asl.value_free(&result) after use.
 * ------------------------------------------------------------------ */
static inline bool _exec(const asl_function_meta_t *meta, const char *src,
                         asl_value_t *out) {
   return Asl.exec(meta, src, NULL, 0, out, NULL, 0, NULL, NULL, NULL, NULL);
}

static void set_config(FILE **logger) {
   *logger = fopen("logs/bench_asl.log", "w");

   printf("\n  %-42s  %9s  %9s  %9s  %7s\n",
          "Benchmark", "min", "mean", "max", "σ");
   printf("  %s\n",
          "────────────────────────────────────────────────────────────────────────────");
   fflush(stdout);
}

static void set_teardown(void) {
}

/* ------------------------------------------------------------------
 * AB01 — Cold AST parse: Asl.parse() + Asl.node_free()
 *         Does NOT call exec; isolates parser overhead.
 * ------------------------------------------------------------------ */
static void bench_asl_cold_parse(void) {
   asl_function_meta_t meta = _make_meta(_arith_src);
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      asl_node_t *root = Asl.parse(&meta, _arith_src);
      Asl.node_free(root);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      asl_node_t *root = Asl.parse(&meta, _arith_src);
      samples[i] = bench_ns() - t0;
      Asl.node_free(root);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("AB01 cold AST parse (arith body)", &r);

   asl_node_t *root = Asl.parse(&meta, _arith_src);
   if (!root) {
      fprintf(stderr, "FAIL: AB01: parse must return non-NULL AST\n");
      return;
   }
   if (root->kind != ASL_NODE_BLOCK)
      fprintf(stderr, "FAIL: AB01: root must be BLOCK node\n");
   Asl.node_free(root);
}

/* ------------------------------------------------------------------
 * AB02 — Exec arithmetic: 7*3+7-3 → 25  (4 ops, no branches)
 * ------------------------------------------------------------------ */
static void bench_asl_exec_arith(void) {
   asl_function_meta_t meta = _make_meta(_arith_src);
   asl_value_t result = {0};
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      _exec(&meta, _arith_src, &result);
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      _exec(&meta, _arith_src, &result);
      samples[i] = bench_ns() - t0;
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("AB02 exec arithmetic: 7*3+7-3 → 25", &r);

   bool ok = _exec(&meta, _arith_src, &result);
   if (!ok)
      fprintf(stderr, "FAIL: AB02: exec must succeed\n");
   if (result.kind != ASL_INT)
      fprintf(stderr, "FAIL: AB02: result must be INT\n");
   if (result.long_value != 25LL)
      fprintf(stderr, "FAIL: AB02: 7*3+7-3 must equal 25\n");
   Asl.value_free(&result);
}

/* ------------------------------------------------------------------
 * AB03 — Exec classify: 4-branch if/else, score=85 → "B"
 * ------------------------------------------------------------------ */
static void bench_asl_exec_classify(void) {
   asl_function_meta_t meta = _make_meta(_classify_src);
   asl_value_t result = {0};
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      _exec(&meta, _classify_src, &result);
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      _exec(&meta, _classify_src, &result);
      samples[i] = bench_ns() - t0;
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("AB03 exec classify 85 → \"B\" (4-branch)", &r);

   bool ok = _exec(&meta, _classify_src, &result);
   if (!ok)
      fprintf(stderr, "FAIL: AB03: exec must succeed\n");
   if (result.kind != ASL_STRING)
      fprintf(stderr, "FAIL: AB03: result must be STRING\n");
   if (!(result.string_value != NULL && result.string_value[0] == 'B'))
      fprintf(stderr, "FAIL: AB03: classify(85) must return \"B\"\n");
   Asl.value_free(&result);
}

/* ------------------------------------------------------------------
 * AB04 — Exec sum loop: for i in [0,99] sum+=i → 4950
 *         100 iterations, integer accumulation in a for loop.
 * ------------------------------------------------------------------ */
static void bench_asl_exec_sum_loop(void) {
   asl_function_meta_t meta = _make_meta(_sum_src);
   asl_value_t result = {0};
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      _exec(&meta, _sum_src, &result);
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      _exec(&meta, _sum_src, &result);
      samples[i] = bench_ns() - t0;
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("AB04 exec sum loop 0..99 → 4950", &r);

   bool ok = _exec(&meta, _sum_src, &result);
   if (!ok)
      fprintf(stderr, "FAIL: AB04: exec must succeed\n");
   if (result.kind != ASL_INT)
      fprintf(stderr, "FAIL: AB04: result must be INT\n");
   if (result.long_value != 4950LL)
      fprintf(stderr, "FAIL: AB04: sum 0..99 must equal 4950\n");
   Asl.value_free(&result);
}

/* ------------------------------------------------------------------
 * AB05 — Exec iterative fib(20) → 6765
 *         20 loop iterations, 4 live variables.
 * ------------------------------------------------------------------ */
static void bench_asl_exec_fib_iter(void) {
   asl_function_meta_t meta = _make_meta(_fib_src);
   asl_value_t result = {0};
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      _exec(&meta, _fib_src, &result);
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      _exec(&meta, _fib_src, &result);
      samples[i] = bench_ns() - t0;
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("AB05 exec iterative fib(20) → 6765", &r);

   bool ok = _exec(&meta, _fib_src, &result);
   if (!ok)
      fprintf(stderr, "FAIL: AB05: exec must succeed\n");
   if (result.kind != ASL_INT)
      fprintf(stderr, "FAIL: AB05: result must be INT\n");
   if (result.long_value != 6765LL)
      fprintf(stderr, "FAIL: AB05: fib(20) must equal 6765\n");
   Asl.value_free(&result);
}

/* ------------------------------------------------------------------
 * AB06 — Exec nested loop: 10×10 → Σ i*j = 2025
 *         Stresses nested for-loop dispatch + 100 multiply + add ops.
 * ------------------------------------------------------------------ */
static void bench_asl_exec_nested_loop(void) {
   asl_function_meta_t meta = _make_meta(_nested_src);
   asl_value_t result = {0};
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      _exec(&meta, _nested_src, &result);
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      _exec(&meta, _nested_src, &result);
      samples[i] = bench_ns() - t0;
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("AB06 exec 10×10 nested loop → 2025", &r);

   bool ok = _exec(&meta, _nested_src, &result);
   if (!ok)
      fprintf(stderr, "FAIL: AB06: exec must succeed\n");
   if (result.kind != ASL_INT)
      fprintf(stderr, "FAIL: AB06: result must be INT\n");
   if (result.long_value != 2025LL)
      fprintf(stderr, "FAIL: AB06: 10×10 nested sum must equal 2025\n");
   Asl.value_free(&result);
}

/* ------------------------------------------------------------------
 * AB07 — Exec collatz(27): unpredictable branch + variable-length
 *         loop (111 steps to reach 1).  Stresses the for(;;) dispatch
 *         path with a loop count the engine cannot predict.
 *         Corresponds to scripts.asl collatz function.
 * ------------------------------------------------------------------ */
static void bench_asl_exec_collatz(void) {
   asl_function_meta_t meta = _make_meta(_collatz_src);
   asl_value_t result = {0};
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      _exec(&meta, _collatz_src, &result);
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      _exec(&meta, _collatz_src, &result);
      samples[i] = bench_ns() - t0;
      Asl.value_free(&result);
      result = (asl_value_t){0};
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("AB07 exec collatz(27) → 111 steps", &r);

   bool ok = _exec(&meta, _collatz_src, &result);
   if (!ok)
      fprintf(stderr, "FAIL: AB07: exec must succeed\n");
   if (result.kind != ASL_INT)
      fprintf(stderr, "FAIL: AB07: result must be INT\n");
   if (result.long_value != 111LL)
      fprintf(stderr, "FAIL: AB07: collatz(27) must equal 111\n");
   Asl.value_free(&result);
}

/* ------------------------------------------------------------------
 * Public entry point
 * ------------------------------------------------------------------ */
void run_bench_asl(void) {
   FILE *logger = NULL;
   set_config(&logger);

   bench_asl_cold_parse();
   bench_asl_exec_arith();
   bench_asl_exec_classify();
   bench_asl_exec_sum_loop();
   bench_asl_exec_fib_iter();
   bench_asl_exec_nested_loop();
   bench_asl_exec_collatz();

   set_teardown();
   if (logger)
      fclose(logger);
}
