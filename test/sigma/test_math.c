/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_math.c - Infrastructure tests: Math vtable
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/infra/test_math.c
 */
#include "sigma/math.h"
#include "testbit.h"

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IMATH01 - is_pow_2 classification                                 */
/* ----------------------------------------------------------------- */
static void test_imath01_is_pow_2(void) {
   TestBit.is_true(!Math.is_pow_2(0), "IMATH01: 0 is not a power of two");
   TestBit.is_true(Math.is_pow_2(1), "IMATH01: 1 is a power of two");
   TestBit.is_true(Math.is_pow_2(2), "IMATH01: 2 is a power of two");
   TestBit.is_true(!Math.is_pow_2(3), "IMATH01: 3 is not a power of two");
   TestBit.is_true(Math.is_pow_2(4), "IMATH01: 4 is a power of two");
   TestBit.is_true(!Math.is_pow_2(5), "IMATH01: 5 is not a power of two");
   TestBit.is_true(Math.is_pow_2(8), "IMATH01: 8 is a power of two");
}

/* ----------------------------------------------------------------- */
/* IMATH02 - next_pow_2 returns strict-next power-of-two             */
/* ----------------------------------------------------------------- */
static void test_imath02_next_pow_2(void) {
   usize out = 0;

   TestBit.is_equal_int((long long)SC_MATH_OK, (long long)Math.next_pow_2_checked(0, &out),
                        "IMATH02: checked next_pow_2(0) returns OK");
   TestBit.is_equal_int(1, (long long)out, "IMATH02: checked next_pow_2(0) == 1");

   TestBit.is_equal_int((long long)SC_MATH_OK, (long long)Math.next_pow_2_checked(8, &out),
                        "IMATH02: checked next_pow_2(8) returns OK");
   TestBit.is_equal_int(16, (long long)out, "IMATH02: checked next_pow_2(8) == 16");

   TestBit.is_equal_int(1, (long long)Math.next_pow_2(0), "IMATH02: next_pow_2(0) == 1");
   TestBit.is_equal_int(2, (long long)Math.next_pow_2(1), "IMATH02: next_pow_2(1) == 2");
   TestBit.is_equal_int(4, (long long)Math.next_pow_2(2), "IMATH02: next_pow_2(2) == 4");
   TestBit.is_equal_int(4, (long long)Math.next_pow_2(3), "IMATH02: next_pow_2(3) == 4");
   TestBit.is_equal_int(8, (long long)Math.next_pow_2(5), "IMATH02: next_pow_2(5) == 8");
   TestBit.is_equal_int(16, (long long)Math.next_pow_2(8), "IMATH02: next_pow_2(8) == 16");
   TestBit.is_equal_int(16, (long long)Math.next_pow_2(15), "IMATH02: next_pow_2(15) == 16");
}

/* ----------------------------------------------------------------- */
/* IMATH03 - normalize_pow_2_min applies floor and rounding          */
/* ----------------------------------------------------------------- */
static void test_imath03_normalize_pow_2_min(void) {
   usize out = 0;

   TestBit.is_equal_int((long long)SC_MATH_OK,
                        (long long)Math.normalize_pow_2_min_checked(16, 8, &out),
                        "IMATH03: checked normalize(16,8) returns OK");
   TestBit.is_equal_int(16, (long long)out, "IMATH03: checked normalize(16,8) == 16");

   TestBit.is_equal_int(8, (long long)Math.normalize_pow_2_min(0, 8),
                        "IMATH03: normalize(0,8) == 8");
   TestBit.is_equal_int(8, (long long)Math.normalize_pow_2_min(5, 8),
                        "IMATH03: normalize(5,8) == 8");
   TestBit.is_equal_int(16, (long long)Math.normalize_pow_2_min(9, 8),
                        "IMATH03: normalize(9,8) == 16");
   TestBit.is_equal_int(16, (long long)Math.normalize_pow_2_min(16, 8),
                        "IMATH03: normalize(16,8) == 16");
}

/* ----------------------------------------------------------------- */
/* IMATH04 - overflow path returns 0                                 */
/* ----------------------------------------------------------------- */
static void test_imath04_overflow_returns_zero(void) {
   usize highest_pow_2 = ((usize)1) << ((sizeof(usize) * 8) - 1);
   usize out = 0;

   TestBit.is_equal_int((long long)SC_MATH_ERR_OVERFLOW,
                        (long long)Math.next_pow_2_checked(highest_pow_2, &out),
                        "IMATH04: checked next_pow_2 reports overflow at POW_2_MAX");
   TestBit.is_true(Math.next_pow_2(highest_pow_2) == 0,
                   "IMATH04: convenience next_pow_2 returns 0 on overflow");
}

/* ----------------------------------------------------------------- */
/* IMATH05 - checked APIs reject invalid arguments                    */
/* ----------------------------------------------------------------- */
static void test_imath05_invalid_args(void) {
   TestBit.is_equal_int((long long)SC_MATH_ERR_INVALID_ARGUMENT,
                        (long long)Math.next_pow_2_checked(8, NULL),
                        "IMATH05: next_pow_2_checked rejects NULL out");

   TestBit.is_equal_int((long long)SC_MATH_ERR_INVALID_ARGUMENT,
                        (long long)Math.normalize_pow_2_min_checked(8, 8, NULL),
                        "IMATH05: normalize_pow_2_min_checked rejects NULL out");
}

int main(void) {
   TestBit.run_ex("IMATH01_is_pow_2", NULL, test_imath01_is_pow_2, td);
   TestBit.run_ex("IMATH02_next_pow_2", NULL, test_imath02_next_pow_2, td);
   TestBit.run_ex("IMATH03_normalize_pow_2_min", NULL, test_imath03_normalize_pow_2_min, td);
   TestBit.run_ex("IMATH04_overflow_returns_zero", NULL, test_imath04_overflow_returns_zero, td);
   TestBit.run_ex("IMATH05_invalid_args", NULL, test_imath05_invalid_args, td);

   return TestBit.report();
}
