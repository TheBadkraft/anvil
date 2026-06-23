/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * sigma.memory/memory.h — Concrete controller structs
 * ------------------------------------------------------------------
 * sigma.core/allocator.h forward-declares sc_ctrl_base_s, sc_bump_ctrl_s,
 * sc_allocator_i, and the Allocator extern.
 * This header provides the actual struct bodies and the arena block type.
 * ------------------------------------------------------------------
 */
#pragma once

#include <sigma.core/allocator.h>
#include <sigma.core/types.h>

/* ------------------------------------------------------------------ */
/* Common controller header — first member of every controller type   */
/* Allows (sc_ctrl_base_s*) ↔ (sc_bump_ctrl_s*) casts                */
/* ------------------------------------------------------------------ */
typedef struct sc_ctrl_base_s {
    sc_alloc_policy policy;
} sc_ctrl_base_s;

/* ------------------------------------------------------------------ */
/* Arena block — internal chain link                                  */
/* ------------------------------------------------------------------ */
typedef struct sc_arena_block_s {
    struct sc_arena_block_s *next;
    usize                    capacity;
    usize                    used;
    /* data follows in-memory: malloc(sizeof(sc_arena_block_s) + capacity) */
} sc_arena_block_s;

/* ------------------------------------------------------------------ */
/* Bump controller                                                    */
/* Accessed as: ctx->arena->alloc(ctx->arena, size)                   */
/* ------------------------------------------------------------------ */
typedef struct sc_bump_ctrl_s {
    sc_ctrl_base_s    base;           /* MUST be first — cast compatibility */
    void *(*alloc)(struct sc_bump_ctrl_s *self, usize size);
    sc_arena_block_s *head;
    sc_arena_block_s *current;
    usize             default_block;
} sc_bump_ctrl_s;
