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
 * File: time.c
 * Description: Time helper implementation for monotonic timestamp capture
 */

#include <sigma/time.h>
#include <time.h>

static usize time_now(void);

static usize time_now(void) {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (usize)ts.tv_sec * 1000000000ULL + (usize)ts.tv_nsec;
}
static usize time_elapsed(usize start, usize end) { return end - start; }

const sc_time_i Time = {
   .now = time_now,
   .elapsed = time_elapsed,
};