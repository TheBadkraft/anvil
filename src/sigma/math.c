/*
 * SigmaCore
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * ----------------------------------------------
 * File: math.c
 * Description: Math helper implementation for power-of-two capacity operations
 */

#include <sigma/math.h>
#include <limits.h>

static const usize POW_2_MAX = ((usize)1) << ((sizeof(usize) * CHAR_BIT) - 1);

static bool math_is_pow_2(usize n);
static usize math_ceil_pow_2(usize n);
static sc_math_status math_next_pow_2_checked(usize n, usize *out);
static usize math_next_pow_2(usize n);
static sc_math_status math_normalize_pow_2_min_checked(usize n, usize min, usize *out);
static usize math_normalize_pow_2_min(usize n, usize min);

const sc_math_i Math = {
   .is_pow_2 = math_is_pow_2,
   .next_pow_2_checked = math_next_pow_2_checked,
   .next_pow_2 = math_next_pow_2,
   .normalize_pow_2_min_checked = math_normalize_pow_2_min_checked,
   .normalize_pow_2_min = math_normalize_pow_2_min,
};

static bool math_is_pow_2(usize n) { return n != 0 && (n & (n - 1)) == 0; }

static usize math_ceil_pow_2(usize n) {
   if (n == 0) {
      return 1;
   }
   if (math_is_pow_2(n)) {
      return n;
   }

   usize p = 1;
   while (p < n) {
      if (p > (SIZE_MAX >> 1)) {
         return 0;
      }
      p <<= 1;
   }

   return p;
}

static sc_math_status math_next_pow_2_checked(usize n, usize *out) {
   if (!out) {
      return SC_MATH_ERR_INVALID_ARGUMENT;
   }

   if (n == 0) {
      *out = 1;
      return SC_MATH_OK;
   }

   if (n >= POW_2_MAX) {
      return SC_MATH_ERR_OVERFLOW;
   }

   if (math_is_pow_2(n)) {
      *out = n << 1;
      return SC_MATH_OK;
   }

   *out = math_ceil_pow_2(n);
   if (*out == 0) {
      return SC_MATH_ERR_OVERFLOW;
   }

   return SC_MATH_OK;
}

static usize math_next_pow_2(usize n) {
   usize out = 0;
   if (math_next_pow_2_checked(n, &out) != SC_MATH_OK) {
      return 0;
   }
   return out;
}

static sc_math_status math_normalize_pow_2_min_checked(usize n, usize min, usize *out) {
   if (!out) {
      return SC_MATH_ERR_INVALID_ARGUMENT;
   }

   usize base = (n < min) ? min : n;

   if (base == 0) {
      base = 1;
   }

   if (math_is_pow_2(base)) {
      *out = base;
      return SC_MATH_OK;
   }

   if (base > POW_2_MAX) {
      return SC_MATH_ERR_OVERFLOW;
   }

   *out = math_ceil_pow_2(base);
   if (*out == 0) {
      return SC_MATH_ERR_OVERFLOW;
   }

   return SC_MATH_OK;
}

static usize math_normalize_pow_2_min(usize n, usize min) {
   usize out = 0;
   if (math_normalize_pow_2_min_checked(n, min, &out) != SC_MATH_OK) {
      return 0;
   }

   return out;
}
