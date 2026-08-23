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
 * File: time.h
 * Description: Time helper interface for monotonic timestamp capture
 */
#pragma once

#include <sigma/types.h>

typedef struct sc_time_i {
   /**
    * @brief Return the current monotonic time in nanoseconds.
    * @details Backed by a monotonic clock, immune to wall-clock adjustments
    * (NTP sync, DST, manual changes). The value has no meaningful epoch and
    * is only valid for computing elapsed durations between two calls on the
    * same machine, e.g. `usize elapsed = Time.now() - start;`.
    * @return Current monotonic time in nanoseconds.
    */
   usize (*now)(void);

   /**
    * @brief Compute the elapsed time between two monotonic timestamps.
    * @param start The start timestamp.
    * @param end The end timestamp.
    * @return Elapsed time in nanoseconds.
    */
   usize (*elapsed)(usize, usize);
} sc_time_i;

extern const sc_time_i Time;
