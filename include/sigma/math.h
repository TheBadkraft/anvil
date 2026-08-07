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
 * File: math.h
 * Description: Math helper interface for power-of-two capacity operations
 */
#pragma once

#include <sigma/types.h>

typedef enum sc_math_status_e {
   SC_MATH_OK = 0,
   SC_MATH_ERR_INVALID_ARGUMENT = -1,
   SC_MATH_ERR_OVERFLOW = -2,
} sc_math_status;

typedef struct sc_math_i {
   /**
    * @brief Return true when n is a non-zero power of two.
    */
   bool (*is_pow_2)(usize n);

   /**
    * @brief Return the strict next power-of-two after n.
    * For powers of two, returns the following power (e.g., 8 -> 16).
    * @param n Input value.
    * @param[out] out Receives the next power-of-two value.
    * @return SC_MATH_OK on success, SC_MATH_ERR_OVERFLOW when advancing
    * would overflow usize, SC_MATH_ERR_INVALID_ARGUMENT when out is NULL.
    */
   sc_math_status (*next_pow_2_checked)(usize n, usize *out);

   /**
    * @brief Return the strict next power-of-two after n.
    * For powers of two, returns the following power (e.g., 8 -> 16).
    * @return 0 when advancing would overflow usize.
    */
   usize (*next_pow_2)(usize n);

   /**
    * @brief Apply a minimum floor, then round up to power-of-two.
    * @param n Input value.
    * @param min Minimum floor to apply prior to rounding.
    * @param[out] out Receives the normalized power-of-two value.
    * @return SC_MATH_OK on success, SC_MATH_ERR_OVERFLOW when rounding would
    * overflow usize, SC_MATH_ERR_INVALID_ARGUMENT when out is NULL.
    */
   sc_math_status (*normalize_pow_2_min_checked)(usize n, usize min, usize *out);

   /**
    * @brief Apply a minimum floor, then round up to power-of-two.
    * @return 0 when rounding would overflow usize.
    */
   usize (*normalize_pow_2_min)(usize n, usize min);
} sc_math_i;

extern const sc_math_i Math;
