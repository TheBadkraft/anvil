/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * bench_resolver.c — Inheritance-resolver benchmarks
 * ------------------------------------------------------------------
 * Mirrors .NET MergeBenchmarks.cs (MB01–MB06).
 *
 * RB01  Cold_SingleLevel   — build_state + get_merged (1-level chain),
 *                            no prior cache; measures topological sort
 *                            + first merge computation for RubyGem:BaseItem.
 * RB02  Warm_SingleLevel   — pre-built state, get_merged cache hit.
 * RB03  Cold_DeepChain     — build_state + get_merged for a 3-level
 *                            chain (Creeper:MobBase:EntityBase); triggers
 *                            ancestor chain resolution on cold path.
 * RB04  Warm_DeepChain     — same 3-level path, cache already warm.
 * RB05  WarmAll_Eager      — anvl_node_state_warm_all() on the full
 *                            merge.aml document (10 stmts, 4 inheritance
 *                            trees); measures bulk eager warming cost.
 * RB06  Siblings_Warm      — 3 sibling derived types (IronOreDeep,
 *                            GoldOreDeep, DiamondOreDeep) reading the
 *                            same inherited field on a pre-warmed state.
 *
 * All benchmarks use test/samples/merge.aml — the same file as
 * .NET MergeBenchmarks for apples-to-apples comparison.
 *
 * Cold benchmarks: each iteration rebuilds the resolver state from
 *   the pre-parsed context so the merge cache is always cold.
 * Warm benchmarks: state is built + primed in set_config; iterations
 *   only call get_merged_fields() (cache-hit path).
 *
 * merge.aml statement indices:
 *   0  BaseItem       (8 fields, no parent)
 *   1  RubyGem        :BaseItem  (1 override — low density)
 *   2  AncientRelic   :BaseItem  (7 overrides — high density)
 *   3  EntityBase     (8 fields, no parent)
 *   4  MobBase        :EntityBase (3 fields)
 *   5  Creeper        :MobBase   (4 fields; 3-level chain)
 *   6  BaseOre        (5 fields, no parent)
 *   7  IronOreDeep    :BaseOre   (4 fields)
 *   8  GoldOreDeep    :BaseOre   (4 fields)
 *   9  DiamondOreDeep :BaseOre   (4 fields)
 * ------------------------------------------------------------------
 */
#include "anvil.h"
#include "benchmarks/bench.h"
#include "resolver.h"
#include "utils.h"
#include <sigma.memory/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 * merge.aml fixture — loaded once from disk, reused across all benchmarks
 * ------------------------------------------------------------------ */
static const char *_merge_src = NULL;
static size_t _merge_sz = 0;

/* Statement indices within merge.aml (see header comment) */
#define IDX_RUBY_GEM 1      /* RubyGem:BaseItem        1-override     */
#define IDX_ANCIENT_RELIC 2 /* AncientRelic:BaseItem   7-overrides    */
#define IDX_CREEPER 5       /* Creeper:MobBase:EntityBase 3-level     */
#define IDX_IRON_ORE 7      /* IronOreDeep:BaseOre                     */
#define IDX_GOLD_ORE 8      /* GoldOreDeep:BaseOre                     */
#define IDX_DIAMOND_ORE 9   /* DiamondOreDeep:BaseOre                  */

/* ------------------------------------------------------------------
 * Pre-parsed context (built once, reused across all bench iterations)
 * ------------------------------------------------------------------ */
static context _ctx_merge = NULL;

/* Pre-built warm state (primed for all derived nodes) */
static anvl_node_state_t *_state_warm = NULL;

/* ------------------------------------------------------------------
 * Helper — build+parse a context from the merge.aml source string.
 * ------------------------------------------------------------------ */
static inline context _build_ctx(const char *src, size_t sz) {
   anvl_ctx_builder_i *b = Context.get_builder();
   b->set_source(b, src, sz);
   context ctx = b->build(b);
   Context.parse(ctx);
   return ctx;
}

static void set_config(FILE **logger) {
   *logger = fopen("logs/bench_resolver.log", "w");

   load_source("test/samples/merge.aml", &_merge_src, &_merge_sz);
   _ctx_merge = _build_ctx(_merge_src, _merge_sz);

   /* Build warm state and prime all derived nodes for cache-hit benchmarks */
   _state_warm = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
   anvl_node_state_warm_all(_state_warm);

   printf("\n  %-42s  %9s  %9s  %9s  %7s\n",
          "Benchmark", "min", "mean", "max", "σ");
   printf("  %s\n",
          "────────────────────────────────────────────────────────────────────────────");
   fflush(stdout);
}

static void set_teardown(void) {
   anvl_node_state_dispose(_state_warm);
   Context.dispose(_ctx_merge);
   free((void *)_merge_src);
   _state_warm = NULL;
   _ctx_merge = NULL;
   _merge_src = NULL;
}

/* ------------------------------------------------------------------
 * RB01 — Cold single-level: build_state + get_merged for RubyGem
 *         (idx=IDX_RUBY_GEM, 1 override on 8-field base).
 *         Measures allocation, topological sort, and first merge
 *         computation.  Apples-to-apples with .NET Cold_LowOverride.
 * ------------------------------------------------------------------ */
static void bench_resolver_cold_single(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      anvl_node_state_t *s = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
      anvl_node_state_get_merged_fields(s, IDX_RUBY_GEM);
      anvl_node_state_dispose(s);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      anvl_node_state_t *s = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
      anvl_node_state_get_merged_fields(s, IDX_RUBY_GEM);
      samples[i] = bench_ns() - t0;
      anvl_node_state_dispose(s);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("RB01 cold 1-level build+merge (RubyGem:BaseItem)", &r);

   anvl_node_state_t *s = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
   const anvl_field_list_t *fl = anvl_node_state_get_merged_fields(s, IDX_RUBY_GEM);
   if (!fl) {
      fprintf(stderr, "FAIL: RB01: merged fields must not be NULL\n");
      anvl_node_state_dispose(s);
      return;
   }
   if (fl->count < 8)
      fprintf(stderr, "FAIL: RB01: RubyGem must have >= 8 merged fields\n");
   anvl_node_state_dispose(s);
}

/* ------------------------------------------------------------------
 * RB02 — Warm single-level: get_merged_fields cache hit for RubyGem
 *         (state pre-warmed in set_config via warm_all).
 * ------------------------------------------------------------------ */
static void bench_resolver_warm_single(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++)
      anvl_node_state_get_merged_fields(_state_warm, IDX_RUBY_GEM);

   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      anvl_node_state_get_merged_fields(_state_warm, IDX_RUBY_GEM);
      samples[i] = bench_ns() - t0;
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("RB02 warm 1-level cache hit  (RubyGem:BaseItem)", &r);

   const anvl_field_list_t *fl = anvl_node_state_get_merged_fields(_state_warm, IDX_RUBY_GEM);
   if (!fl)
      fprintf(stderr, "FAIL: RB02: warm merged fields must not be NULL\n");
}

/* ------------------------------------------------------------------
 * RB03 — Cold 3-level chain: build_state + get_merged for Creeper
 *         (idx=IDX_CREEPER).  Triggers 3 chained merge computations:
 *         EntityBase, MobBase, Creeper.  Apples-to-apples with .NET
 *         Cold_3LevelChain.
 * ------------------------------------------------------------------ */
static void bench_resolver_cold_deep3(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      anvl_node_state_t *s = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
      anvl_node_state_get_merged_fields(s, IDX_CREEPER);
      anvl_node_state_dispose(s);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      anvl_node_state_t *s = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
      anvl_node_state_get_merged_fields(s, IDX_CREEPER);
      samples[i] = bench_ns() - t0;
      anvl_node_state_dispose(s);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("RB03 cold 3-level build+merge (Creeper:MobBase:EntityBase)", &r);

   anvl_node_state_t *s = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
   const anvl_field_list_t *fl = anvl_node_state_get_merged_fields(s, IDX_CREEPER);
   if (!fl) {
      fprintf(stderr, "FAIL: RB03: 3-level merged fields must not be NULL\n");
      anvl_node_state_dispose(s);
      return;
   }
   if (fl->count < 8)
      fprintf(stderr, "FAIL: RB03: Creeper must have >= 8 ancestor+own fields\n");
   anvl_node_state_dispose(s);
}

/* ------------------------------------------------------------------
 * RB04 — Warm 3-level chain: cache hit for Creeper (state pre-warmed).
 * ------------------------------------------------------------------ */
static void bench_resolver_warm_deep3(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++)
      anvl_node_state_get_merged_fields(_state_warm, IDX_CREEPER);

   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      anvl_node_state_get_merged_fields(_state_warm, IDX_CREEPER);
      samples[i] = bench_ns() - t0;
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("RB04 warm 3-level cache hit  (Creeper:MobBase:EntityBase)", &r);

   const anvl_field_list_t *fl = anvl_node_state_get_merged_fields(_state_warm, IDX_CREEPER);
   if (!fl)
      fprintf(stderr, "FAIL: RB04: warm 3-level merged fields must not be NULL\n");
}

/* ------------------------------------------------------------------
 * RB05 — Eager warm_all: build state + pre-compute all merges for
 *         the full merge.aml document (10 stmts across 4 inheritance
 *         trees).  Measures bulk eager resolution cost.
 *         Apples-to-apples with .NET WarmAll_Eager.
 * ------------------------------------------------------------------ */
static void bench_resolver_warm_all(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      anvl_node_state_t *s = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
      anvl_node_state_warm_all(s);
      anvl_node_state_dispose(s);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      anvl_node_state_t *s = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
      anvl_node_state_warm_all(s);
      samples[i] = bench_ns() - t0;
      anvl_node_state_dispose(s);
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("RB05 eager warm_all (merge.aml 10 stmts)", &r);

   anvl_node_state_t *s = anvl_resolver_build_state(_ctx_merge, _ctx_merge->source);
   bool ok = anvl_node_state_warm_all(s);
   if (!ok)
      fprintf(stderr, "FAIL: RB05: warm_all must succeed\n");
   anvl_node_state_dispose(s);
}

/* ------------------------------------------------------------------
 * RB06 — Siblings warm: IronOreDeep/GoldOreDeep/DiamondOreDeep
 *         reading merged fields on a pre-warmed state.  Measures
 *         3 independent cache hits (all share BaseOre as parent).
 *         Apples-to-apples with .NET Siblings_Warm.
 * ------------------------------------------------------------------ */
static void bench_resolver_siblings_warm(void) {
   uint64_t samples[BENCH_ITERS];

   for (int i = 0; i < BENCH_WARMUP; i++) {
      anvl_node_state_get_merged_fields(_state_warm, IDX_IRON_ORE);
      anvl_node_state_get_merged_fields(_state_warm, IDX_GOLD_ORE);
      anvl_node_state_get_merged_fields(_state_warm, IDX_DIAMOND_ORE);
   }
   for (int i = 0; i < BENCH_ITERS; i++) {
      uint64_t t0 = bench_ns();
      anvl_node_state_get_merged_fields(_state_warm, IDX_IRON_ORE);
      anvl_node_state_get_merged_fields(_state_warm, IDX_GOLD_ORE);
      anvl_node_state_get_merged_fields(_state_warm, IDX_DIAMOND_ORE);
      samples[i] = bench_ns() - t0;
   }

   bench_result_t r = bench_compute(samples, BENCH_ITERS, 0);
   bench_report("RB06 3-sibling warm reads (Iron/Gold/DiamondOre)", &r);

   const anvl_field_list_t *a = anvl_node_state_get_merged_fields(_state_warm, IDX_IRON_ORE);
   const anvl_field_list_t *b = anvl_node_state_get_merged_fields(_state_warm, IDX_GOLD_ORE);
   const anvl_field_list_t *c = anvl_node_state_get_merged_fields(_state_warm, IDX_DIAMOND_ORE);
   if (!a)
      fprintf(stderr, "FAIL: RB06: IronOreDeep merged fields\n");
   if (!b)
      fprintf(stderr, "FAIL: RB06: GoldOreDeep merged fields\n");
   if (!c)
      fprintf(stderr, "FAIL: RB06: DiamondOreDeep merged fields\n");
}

/* ------------------------------------------------------------------
 * Public entry point
 * ------------------------------------------------------------------ */
void run_bench_resolver(void) {
   FILE *logger = NULL;
   set_config(&logger);

   bench_resolver_cold_single();
   bench_resolver_warm_single();
   bench_resolver_cold_deep3();
   bench_resolver_warm_deep3();
   bench_resolver_warm_all();
   bench_resolver_siblings_warm();

   set_teardown();
   if (logger)
      fclose(logger);
}
