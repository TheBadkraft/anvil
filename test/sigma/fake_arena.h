/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * fake_arena.h - Test-only bump-arena stand-in for sc_alloc_use_t.
 * ----------------------------------------------------------------------- *
 * sc_alloc_use_t's alloc/release/resize function pointers carry no
 * userdata/context parameter, so a test double has to track its calls via
 * file-static state. This backs every "allocation" with malloc, remembers
 * every pointer it hands out so fake_arena_reset() can bulk-free them all
 * at once (simulating an arena's single bulk-release and keeping valgrind
 * clean), and counts alloc()/release() calls so a test can assert an
 * arena-backed instance routes through the override and never calls
 * release() on a buffer it's supposed to orphan instead of freeing.
 *
 * Included directly (header-only, file-static) by exactly one test_*.c
 * translation unit at a time — no linkage concerns across test binaries.
 * Call fake_arena_reset() before first use in a test (and/or as that
 * test's teardown) to get a clean slate.
 * ----------------------------------------------------------------------- *
 */
#pragma once

#include <sigma/allocator.h>
#include <sigma/types.h>
#include <stdlib.h>
#include <string.h>

#define FAKE_ARENA_MAX_BLOCKS 256

static void *fake_arena_blocks[FAKE_ARENA_MAX_BLOCKS];
static int fake_arena_block_count = 0;
static int fake_arena_alloc_calls = 0;
static int fake_arena_release_calls = 0;

static void *fake_arena_alloc(usize size) {
   fake_arena_alloc_calls++;
   void *p = malloc(size);
   if (p) {
      memset(p, 0, size);
      if (fake_arena_block_count < FAKE_ARENA_MAX_BLOCKS) {
         fake_arena_blocks[fake_arena_block_count++] = p;
      }
   }
   return p;
}

/* A real bump/arena allocator has no way to free one specific prior
 * allocation. This exists only so tests can prove an arena-backed
 * instance never calls it on an orphaned grow/dispose path. */
static void fake_arena_release(void *ptr) {
   (void)ptr;
   fake_arena_release_calls++;
}

/* Simulates the arena's single bulk-release: frees every block handed out
 * since the last reset, then zeroes the counters for the next test. */
static void fake_arena_reset(void) {
   for (int i = 0; i < fake_arena_block_count; i++) {
      free(fake_arena_blocks[i]);
   }
   fake_arena_block_count = 0;
   fake_arena_alloc_calls = 0;
   fake_arena_release_calls = 0;
}

static sc_alloc_use_t fake_arena_use = {
   .ctrl = NULL,
   .alloc = fake_arena_alloc,
   .release = fake_arena_release,
   .resize = NULL,
   .frame_begin = NULL,
   .frame_end = NULL,
};
