/*
 * test_resolver.c — TDD tests for the inheritance resolver (port/resolver)
 *
 * Test contracts derived from ResolverTests.cs (Anvil.Net.Api reference).
 *
 * Test groups:
 *   RS01  Fast-path: document with no inheritance → build_state returns NULL
 *   RS02  Single-level: Derived:Base inherits un-overridden fields
 *   RS03  Single-level: Derived overrides own field (derived wins)
 *   RS04  Single-level: Base statement unchanged after resolve
 *   RS05  Three-level chain: Gamma:Beta:Alpha — full field resolution
 *   RS06  Three-level: Beta midpoint correct
 *   RS07  Forward reference: derived declared before base — still works
 *   RS08  Cycle A:B, B:A → ANVL_ERR_RESOLVER_CYCLE_DETECTED
 *   RS09  warm_all + idempotent double warm
 *   RS10  Missing base → NULL from get_merged_fields + RESOLVER_MISSING_BASE
 */

#include "anvil.h"
#include "resolver.h"
#include "utilities/helpers.h"
#include <sigma.memory/memory.h>
#include <sigtest/sigtest.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Test function forward declarations                                 */
/* ------------------------------------------------------------------ */
static void test_no_inheritance_fast_path(void);
static void test_single_level_inherits_unoverridden(void);
static void test_single_level_derived_override(void);
static void test_single_level_base_unchanged(void);
static void test_three_level_full_resolve(void);
static void test_three_level_beta_correct(void);
static void test_forward_reference(void);
static void test_cycle_detected(void);
static void test_warm_all_idempotent(void);
static void test_missing_base_deferred_error(void);

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static void set_config(FILE **logger) {
   *logger = fopen("logs/test_resolver.log", "w");
}
static void set_teardown(void) {
   Anvil.cleanup();
}
static void teardown(void) {
   Anvil.error_clear();
}

static context build_context(const char *src_str) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, src_str, strlen(src_str));
   context ctx = builder->build(builder);
   if (!ctx)
      return NULL;
   bool ok = Context.parse(ctx);
   if (!ok) {
      Context.dispose(ctx);
      return NULL;
   }
   return ctx;
}

/* Return source string from context (convenience) */
static inline source ctx_src(context ctx) { return ctx->source; }

/* Get identifier from statement using raw meta */
static const char *stmt_ident(statement stmt, source src) {
   return Statement.identifier(stmt, src);
}

/* Find merged field by key name; returns NULL when not found */
static field find_field(const anvl_field_list_t *list, source src, const char *key) {
   if (!list)
      return NULL;
   usize klen = strlen(key);
   for (usize i = 0; i < list->count; i++) {
      field f = list->fields[i];
      if (!f)
         continue;
      if (f->key_len == klen && memcmp(Source.data(src) + f->key_pos, key, klen) == 0)
         return f;
   }
   return NULL;
}

/* Read scalar value as long (decimal only, no negatives for small helpers) */
static long field_as_long(field f, source src) {
   if (!f || !f->val || f->val->type != ANVL_VALUE_SCALAR)
      return -1;
   const char *s = Source.data(src) + f->val->data.scalar.pos;
   usize len = f->val->data.scalar.len;
   long result = 0;
   int sign = 1;
   usize i = 0;
   if (i < len && s[i] == '-') {
      sign = -1;
      i++;
   }
   for (; i < len; i++) {
      if (s[i] < '0' || s[i] > '9')
         break;
      result = result * 10 + (s[i] - '0');
   }
   return sign * result;
}

/* ------------------------------------------------------------------ */
/* Test sample documents                                              */
/* ------------------------------------------------------------------ */

static const char SINGLE_LEVEL[] =
    "#!aml\n"
    "Base := { a := \"hello\", b := 42, c := 7 }\n"
    "Derived:Base := { b := 99 }\n";

static const char THREE_LEVEL[] =
    "#!aml\n"
    "Alpha := { x := 1, y := 2, z := 3 }\n"
    "Beta:Alpha := { y := 20 }\n"
    "Gamma:Beta := { z := 300 }\n";

static const char CYCLE[] =
    "#!aml\n"
    "A:B := { valA := 1 }\n"
    "B:A := { valB := 2 }\n";

static const char MISSING_BASE[] =
    "#!aml\n"
    "X:Ghost := { x := 10 }\n";

static const char NO_INHERITANCE[] =
    "#!aml\n"
    "Standalone := { p := 1, q := 2 }\n";

static const char FORWARD_REF[] =
    "#!aml\n"
    "Child:Parent := { own := 1 }\n"
    "Parent := { shared := 42 }\n";

/* ================================================================
 * RS01 — Fast-path: no inheritance → build_state returns NULL
 * ================================================================ */
static void test_no_inheritance_fast_path(void) {
   context ctx = build_context(NO_INHERITANCE);
   Assert.isNotNull(ctx, "Context should be created");

   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNull(state, "Fast-path: build_state should return NULL when no inheritance");
   Assert.isFalse(anvl_error_is_set(), "No error should be set for fast-path");

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * RS02 — Single-level: Derived:Base inherits un-overridden fields
 * ================================================================ */
static void test_single_level_inherits_unoverridden(void) {
   context ctx = build_context(SINGLE_LEVEL);
   Assert.isNotNull(ctx, "Context should build");

   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNotNull(state, "State should be built for doc with inheritance");

   /* Find Derived statement index */
   usize derived_idx = (usize)-1;
   for (usize i = 0; i < (usize)Context.statement_count(ctx); i++) {
      const char *id = stmt_ident(Context.get_statement(ctx, i), ctx_src(ctx));
      if (id && strcmp(id, "Derived") == 0) {
         derived_idx = i;
         break;
      }
   }
   Assert.isTrue(derived_idx != (usize)-1, "Should find Derived statement");

   const anvl_field_list_t *merged = anvl_node_state_get_merged_fields(state, derived_idx);
   Assert.isNotNull((void *)merged, "Merged fields should not be NULL");

   field fa = find_field(merged, ctx_src(ctx), "a");
   field fc = find_field(merged, ctx_src(ctx), "c");
   Assert.isNotNull(fa, "Field 'a' should be inherited from Base");
   Assert.isNotNull(fc, "Field 'c' should be inherited from Base");
   Assert.isTrue(fc->val != NULL && field_as_long(fc, ctx_src(ctx)) == 7L,
                 "Field 'c' value should be 7 (inherited)");

   anvl_node_state_dispose(state);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * RS03 — Single-level: Derived overrides own field
 * ================================================================ */
static void test_single_level_derived_override(void) {
   context ctx = build_context(SINGLE_LEVEL);
   Assert.isNotNull(ctx, "Context should build");

   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNotNull(state, "State should be built");

   usize derived_idx = (usize)-1;
   for (usize i = 0; i < (usize)Context.statement_count(ctx); i++) {
      const char *id = stmt_ident(Context.get_statement(ctx, i), ctx_src(ctx));
      if (id && strcmp(id, "Derived") == 0) {
         derived_idx = i;
         break;
      }
   }
   Assert.isTrue(derived_idx != (usize)-1, "Should find Derived");

   const anvl_field_list_t *merged = anvl_node_state_get_merged_fields(state, derived_idx);
   Assert.isNotNull((void *)merged, "Merged fields should not be NULL");

   field fb = find_field(merged, ctx_src(ctx), "b");
   Assert.isNotNull(fb, "Field 'b' should be present");
   Assert.isTrue(field_as_long(fb, ctx_src(ctx)) == 99L,
                 "Derived 'b' should be 99 (override)");

   anvl_node_state_dispose(state);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * RS04 — Single-level: Base unchanged
 * ================================================================ */
static void test_single_level_base_unchanged(void) {
   context ctx = build_context(SINGLE_LEVEL);
   Assert.isNotNull(ctx, "Context should build");

   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNotNull(state, "State should be built");

   usize base_idx = (usize)-1;
   for (usize i = 0; i < (usize)Context.statement_count(ctx); i++) {
      const char *id = stmt_ident(Context.get_statement(ctx, i), ctx_src(ctx));
      if (id && strcmp(id, "Base") == 0) {
         base_idx = i;
         break;
      }
   }
   Assert.isTrue(base_idx != (usize)-1, "Should find Base");

   const anvl_field_list_t *merged = anvl_node_state_get_merged_fields(state, base_idx);
   Assert.isNotNull((void *)merged, "Base merged fields should not be NULL");

   field fb = find_field(merged, ctx_src(ctx), "b");
   Assert.isNotNull(fb, "Base should have field 'b'");
   Assert.isTrue(field_as_long(fb, ctx_src(ctx)) == 42L,
                 "Base 'b' should still be 42");

   anvl_node_state_dispose(state);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * RS05 — Three-level: Gamma fully resolved
 * ================================================================ */
static void test_three_level_full_resolve(void) {
   context ctx = build_context(THREE_LEVEL);
   Assert.isNotNull(ctx, "Context should build");

   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNotNull(state, "State should be built");

   usize gamma_idx = (usize)-1;
   for (usize i = 0; i < (usize)Context.statement_count(ctx); i++) {
      const char *id = stmt_ident(Context.get_statement(ctx, i), ctx_src(ctx));
      if (id && strcmp(id, "Gamma") == 0) {
         gamma_idx = i;
         break;
      }
   }
   Assert.isTrue(gamma_idx != (usize)-1, "Should find Gamma");

   const anvl_field_list_t *merged = anvl_node_state_get_merged_fields(state, gamma_idx);
   Assert.isNotNull((void *)merged, "Gamma merged fields should not be NULL");

   field fx = find_field(merged, ctx_src(ctx), "x");
   field fy = find_field(merged, ctx_src(ctx), "y");
   field fz = find_field(merged, ctx_src(ctx), "z");
   Assert.isNotNull(fx, "Gamma should inherit 'x' from Alpha");
   Assert.isNotNull(fy, "Gamma should inherit 'y' from Beta");
   Assert.isNotNull(fz, "Gamma should have own 'z'");

   Assert.isTrue(field_as_long(fx, ctx_src(ctx)) == 1L, "Gamma x = 1");
   Assert.isTrue(field_as_long(fy, ctx_src(ctx)) == 20L, "Gamma y = 20 (Beta override)");
   Assert.isTrue(field_as_long(fz, ctx_src(ctx)) == 300L, "Gamma z = 300 (own)");

   anvl_node_state_dispose(state);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * RS06 — Three-level: Beta midpoint correct
 * ================================================================ */
static void test_three_level_beta_correct(void) {
   context ctx = build_context(THREE_LEVEL);
   Assert.isNotNull(ctx, "Context should build");

   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNotNull(state, "State should be built");

   usize beta_idx = (usize)-1;
   for (usize i = 0; i < (usize)Context.statement_count(ctx); i++) {
      const char *id = stmt_ident(Context.get_statement(ctx, i), ctx_src(ctx));
      if (id && strcmp(id, "Beta") == 0) {
         beta_idx = i;
         break;
      }
   }
   Assert.isTrue(beta_idx != (usize)-1, "Should find Beta");

   const anvl_field_list_t *merged = anvl_node_state_get_merged_fields(state, beta_idx);
   Assert.isNotNull((void *)merged, "Beta merged fields should not be NULL");

   Assert.isTrue(field_as_long(find_field(merged, ctx_src(ctx), "x"), ctx_src(ctx)) == 1L, "Beta x=1");
   Assert.isTrue(field_as_long(find_field(merged, ctx_src(ctx), "y"), ctx_src(ctx)) == 20L, "Beta y=20");
   Assert.isTrue(field_as_long(find_field(merged, ctx_src(ctx), "z"), ctx_src(ctx)) == 3L, "Beta z=3");

   anvl_node_state_dispose(state);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * RS07 — Forward reference (Child declared before Parent)
 * ================================================================ */
static void test_forward_reference(void) {
   context ctx = build_context(FORWARD_REF);
   Assert.isNotNull(ctx, "Context should build");

   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNotNull(state, "State should be built for forward ref doc");

   usize child_idx = (usize)-1;
   for (usize i = 0; i < (usize)Context.statement_count(ctx); i++) {
      const char *id = stmt_ident(Context.get_statement(ctx, i), ctx_src(ctx));
      if (id && strcmp(id, "Child") == 0) {
         child_idx = i;
         break;
      }
   }
   Assert.isTrue(child_idx != (usize)-1, "Should find Child");

   const anvl_field_list_t *merged = anvl_node_state_get_merged_fields(state, child_idx);
   Assert.isNotNull((void *)merged, "Forward-ref merge should succeed");

   field fs = find_field(merged, ctx_src(ctx), "shared");
   Assert.isNotNull(fs, "Child should inherit 'shared' from Parent");
   Assert.isTrue(field_as_long(fs, ctx_src(ctx)) == 42L, "shared = 42");

   anvl_node_state_dispose(state);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * RS08 — Cycle A:B, B:A → ANVL_ERR_RESOLVER_CYCLE_DETECTED
 * ================================================================ */
static void test_cycle_detected(void) {
   context ctx = build_context(CYCLE);
   Assert.isNotNull(ctx, "Context should build (cycle not detected at parse time)");

   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNull(state, "build_state should return NULL on cycle");
   Assert.isTrue(anvl_error_is_set(), "Error should be set");
   const anvl_error_state *err = Anvil.error_get();
   Assert.isTrue(err->code == ANVL_ERR_RESOLVER_CYCLE_DETECTED,
                 "Error code should be RESOLVER_CYCLE_DETECTED");

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * RS09 — warm_all + idempotent double warm
 * ================================================================ */
static void test_warm_all_idempotent(void) {
   context ctx = build_context(SINGLE_LEVEL);
   Assert.isNotNull(ctx, "Context should build");

   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNotNull(state, "State should be built");

   bool ok1 = anvl_node_state_warm_all(state);
   Assert.isTrue(ok1, "First warm_all should succeed");

   bool ok2 = anvl_node_state_warm_all(state);
   Assert.isTrue(ok2, "Second warm_all should be idempotent");

   /* Verify cached result still correct after double warm */
   usize derived_idx = (usize)-1;
   for (usize i = 0; i < (usize)Context.statement_count(ctx); i++) {
      const char *id = stmt_ident(Context.get_statement(ctx, i), ctx_src(ctx));
      if (id && strcmp(id, "Derived") == 0) {
         derived_idx = i;
         break;
      }
   }
   const anvl_field_list_t *merged = anvl_node_state_get_merged_fields(state, derived_idx);
   Assert.isNotNull((void *)merged, "Merged fields still valid after double warm");
   field fb = find_field(merged, ctx_src(ctx), "b");
   Assert.isTrue(field_as_long(fb, ctx_src(ctx)) == 99L, "Cached result correct after double warm");

   anvl_node_state_dispose(state);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * RS10 — Missing base → ANVL_ERR_RESOLVER_MISSING_BASE on access
 * ================================================================ */
static void test_missing_base_deferred_error(void) {
   context ctx = build_context(MISSING_BASE);
   Assert.isNotNull(ctx, "Context should build (missing base not detected at parse time)");

   /* build_state should succeed — missing base is a deferred error */
   anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx_src(ctx));
   Assert.isNotNull(state, "build_state should succeed for missing-base doc");
   Assert.isFalse(anvl_error_is_set(), "No error yet at build time");

   /* First field access should trigger RESOLVER_MISSING_BASE */
   usize x_idx = 0; /* X is the only statement */
   const anvl_field_list_t *merged = anvl_node_state_get_merged_fields(state, x_idx);
   Assert.isNull((void *)merged, "get_merged_fields should return NULL for missing base");
   Assert.isTrue(anvl_error_is_set(), "Error should be set after missing-base access");
   const anvl_error_state *err = Anvil.error_get();
   Assert.isTrue(err->code == ANVL_ERR_RESOLVER_MISSING_BASE,
                 "Error code should be RESOLVER_MISSING_BASE");

   anvl_node_state_dispose(state);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * Registration
 * ================================================================ */
__attribute__((constructor)) static void register_test_resolver(void) {
   testset("Resolver Tests", set_config, set_teardown);

   testcase("No inheritance fast-path", test_no_inheritance_fast_path);
   testcase("Single-level: inherits unoverridden", test_single_level_inherits_unoverridden);
   testcase("Single-level: derived overrides field", test_single_level_derived_override);
   testcase("Single-level: base unchanged", test_single_level_base_unchanged);
   testcase("Three-level: Gamma full resolve", test_three_level_full_resolve);
   testcase("Three-level: Beta midpoint", test_three_level_beta_correct);
   testcase("Forward reference", test_forward_reference);
   testcase("Cycle detected", test_cycle_detected);
   testcase("warm_all idempotent", test_warm_all_idempotent);
   testcase("Missing base deferred error", test_missing_base_deferred_error);
}
