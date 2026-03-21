/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2026-03-12
 * File: test/unit/test_utils.c
 * ------------------------------------------------------------------
 * Description:
 * Unit tests for utils.c — dialect_hint_from_ext and load_source.
 * ------------------------------------------------------------------
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "utilities/helpers.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <sigma.core/strings.h>
#include <stdlib.h>
#include <string.h>

static void set_config(FILE **logger) {
   *logger = fopen("logs/test_utils.log", "w");
}
static void set_teardown() {
}

// ============================================================================
// UT01–UT05: dialect_hint_from_ext
// ============================================================================

// UT01 — ".aml" extension hints AML dialect (primary test)
static void test_dialect_hint_anvl(void) {
   anvl_dialect d = dialect_hint_from_ext("config.aml");
   Assert.isTrue(d == ANVL_DIALECT_AML, ".aml should hint ANVL_DIALECT_AML");
}

// UT02 — ".anvl" extension: only ".aml" is the recognised AML extension;
//   ".anvl" falls back to ASL (same as unknown extensions)
static void test_dialect_hint_aml(void) {
   anvl_dialect d = dialect_hint_from_ext("config.anvl");
   Assert.isTrue(d == ANVL_DIALECT_ASL,
                 ".anvl (not .aml) returns the default fallback dialect");
}

// UT03 — non-.aml extension falls back to ASL (current internal default)
static void test_dialect_hint_asl(void) {
   anvl_dialect d = dialect_hint_from_ext("script.asl");
   // .asl (and all non-.aml extensions) currently returns ANVL_DIALECT_ASL
   Assert.isTrue(d == ANVL_DIALECT_ASL, ".asl should hint ANVL_DIALECT_ASL (default fallback)");
}

// UT04 — NULL path does not crash, returns a valid dialect
static void test_dialect_hint_null(void) {
   anvl_dialect d = dialect_hint_from_ext(NULL);
   Assert.isTrue(d == ANVL_DIALECT_AML || d == ANVL_DIALECT_AMP || d == ANVL_DIALECT_ASL,
                 "NULL filepath should return a valid dialect without crashing");
}

// UT05 — unknown extension also returns a valid dialect (no crash)
static void test_dialect_hint_unknown(void) {
   anvl_dialect d = dialect_hint_from_ext("data.txt");
   Assert.isTrue(d == ANVL_DIALECT_AML || d == ANVL_DIALECT_AMP || d == ANVL_DIALECT_ASL,
                 "Unknown extension should return a valid dialect");
}

// ============================================================================
// UT06–UT08: load_source
// ============================================================================

// UT06 — load_source returns true and sets output for a valid file
static void test_load_source_valid(void) {
   const char *filepath = get_source_path("assignments.anvl");
   const char *source = NULL;
   usize len = 0;
   bool ok = load_source(filepath, &source, &len);
   Assert.isTrue(ok, "load_source should return true for an existing file");
   Assert.isNotNull((void *)source, "source pointer should be set");
   Assert.isTrue(len > 0, "source length should be greater than zero");
   // load_source uses Allocator.alloc — dispose via Allocator
   Allocator.free((void *)source);
}

// UT07 — load_source returns false for a non-existent path
static void test_load_source_missing(void) {
   const char *source = NULL;
   usize len = 0;
   bool ok = load_source("/tmp/does_not_exist_anvil_test.anvl", &source, &len);
   Assert.isFalse(ok, "load_source should return false for a missing file");
}

// UT08 — loaded source length matches actual file content length
static void test_load_source_length(void) {
   const char *filepath = get_source_path("numbers.anvl");
   const char *source = NULL;
   usize len = 0;
   bool ok = load_source(filepath, &source, &len);
   Assert.isTrue(ok, "load_source should succeed for numbers.anvl");
   Assert.isTrue(len == strlen(source), "Reported length should match null-terminated string length");
   Allocator.free((void *)source);
}

// ============================================================================
// Registration
// ============================================================================

static void _register(void) {
   testset("Utils", set_config, set_teardown);

   testcase("UT01 dialect hint .aml  → AML", test_dialect_hint_anvl);
   testcase("UT02 dialect hint .anvl → AML", test_dialect_hint_aml);
   testcase("UT03 dialect hint .asl  → ASL default", test_dialect_hint_asl);
   testcase("UT04 dialect hint NULL no crash", test_dialect_hint_null);
   testcase("UT05 dialect hint unknown no crash", test_dialect_hint_unknown);
   testcase("UT06 load_source valid file", test_load_source_valid);
   testcase("UT07 load_source missing file", test_load_source_missing);
   testcase("UT08 load_source length accurate", test_load_source_length);
}
__attribute__((constructor)) static void register_test_utils(void) {
   Tests.enqueue(_register);
}
