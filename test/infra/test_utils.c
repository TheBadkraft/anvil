/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_utils.c - Infrastructure tests: dialect_hint_from_ext
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/infra/test_utils.c
 */
#include "utils.h"
#include "testbit.h"

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IU01 — .aml extension → AML dialect                               */
/* ----------------------------------------------------------------- */
static void test_iu01_aml_extension(void) {
    anvl_dialect d = dialect_hint_from_ext("config.aml");
    TestBit.is_equal_int(ANVL_DIALECT_AML, (long long)d, "IU01: .aml → AML dialect");
}

/* ----------------------------------------------------------------- */
/* IU02 — unknown extension → ASL dialect                            */
/* ----------------------------------------------------------------- */
static void test_iu02_unknown_extension(void) {
    anvl_dialect d = dialect_hint_from_ext("config.json");
    TestBit.is_equal_int(ANVL_DIALECT_ASL, (long long)d, "IU02: unknown ext → ASL dialect");
}

/* ----------------------------------------------------------------- */
/* IU03 — no extension → ASL dialect                                 */
/* ----------------------------------------------------------------- */
static void test_iu03_no_extension(void) {
    anvl_dialect d = dialect_hint_from_ext("config");
    TestBit.is_equal_int(ANVL_DIALECT_ASL, (long long)d, "IU03: no ext → ASL dialect");
}

/* ----------------------------------------------------------------- */
/* IU04 — null path → ASL dialect                                    */
/* ----------------------------------------------------------------- */
static void test_iu04_null_path(void) {
    anvl_dialect d = dialect_hint_from_ext(NULL);
    TestBit.is_equal_int(ANVL_DIALECT_ASL, (long long)d, "IU04: null → ASL dialect");
}

/* ----------------------------------------------------------------- */
/* IU05 — .amp extension → AMP dialect                               */
/* ----------------------------------------------------------------- */
static void test_iu05_amp_extension(void) {
    anvl_dialect d = dialect_hint_from_ext("payload.amp");
    TestBit.is_equal_int(ANVL_DIALECT_AMP, (long long)d, "IU05: .amp → AMP dialect");
}

int main(void) {
    TestBit.run_ex("IU01_aml_extension",     NULL, test_iu01_aml_extension,     td);
    TestBit.run_ex("IU02_unknown_extension", NULL, test_iu02_unknown_extension, td);
    TestBit.run_ex("IU03_no_extension",      NULL, test_iu03_no_extension,      td);
    TestBit.run_ex("IU04_null_path",         NULL, test_iu04_null_path,         td);
    TestBit.run_ex("IU05_amp_extension",     NULL, test_iu05_amp_extension,     td);

    return TestBit.report();
}