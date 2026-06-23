/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * memory.c — Allocator + bump arena implementation
 * ------------------------------------------------------------------
 * Global Allocator: thin malloc/free/realloc wrappers.
 * Bump arena: chained fixed-size blocks, aligned to max_align_t.
 *             New blocks allocated on demand — no upfront size needed.
 * ------------------------------------------------------------------
 */

#include <sigma.memory/memory.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_ALIGN      sizeof(max_align_t)
#define ALIGN_UP(n)      (((n) + ARENA_ALIGN - 1) & ~(ARENA_ALIGN - 1))
#define DEFAULT_BLOCK_SZ (64u * 1024u)

/* ------------------------------------------------------------------ */
/* Global allocator                                                   */
/* ------------------------------------------------------------------ */
static void *mem_alloc(usize size) {
    if (!size) return NULL;
    void *p = malloc(size);
    if (p) memset(p, 0, size);
    return p;
}
static void  mem_dispose(void *ptr)             { free(ptr); }
static void *mem_realloc(void *ptr, usize size) {
    if (!size) { free(ptr); return NULL; }
    return realloc(ptr, size);
}

/* ------------------------------------------------------------------ */
/* Bump arena                                                         */
/* ------------------------------------------------------------------ */
static sc_arena_block_s *block_new(usize capacity) {
    sc_arena_block_s *b = malloc(sizeof(sc_arena_block_s) + capacity);
    if (!b) return NULL;
    b->next     = NULL;
    b->capacity = capacity;
    b->used     = 0;
    return b;
}

static void *bump_alloc(sc_bump_ctrl_s *self, usize size) {
    if (!self || !size) return NULL;
    usize aligned = ALIGN_UP(size);

    if (self->current &&
        (self->current->used + aligned) <= self->current->capacity) {
        void *p = (uint8_t *)(self->current + 1) + self->current->used;
        self->current->used += aligned;
        memset(p, 0, size);
        return p;
    }

    usize block_sz = self->default_block;
    if (aligned > block_sz) block_sz = aligned;

    sc_arena_block_s *b = block_new(block_sz);
    if (!b) return NULL;

    if (!self->head) { self->head = b; self->current = b; }
    else             { self->current->next = b; self->current = b; }

    void *p = (uint8_t *)(b + 1);
    b->used = aligned;
    memset(p, 0, size);
    return p;
}

static bump_allocator mem_create_bump(usize initial) {
    sc_bump_ctrl_s *c = malloc(sizeof(sc_bump_ctrl_s));
    if (!c) return NULL;
    memset(c, 0, sizeof(sc_bump_ctrl_s));
    c->base.policy   = POLICY_BUMP;
    c->alloc         = bump_alloc;
    c->default_block = initial ? initial : DEFAULT_BLOCK_SZ;
    c->head          = block_new(c->default_block);
    c->current       = c->head;
    if (!c->head) { free(c); return NULL; }
    return c;
}

static void mem_release(sc_ctrl_base_s *ctrl) {
    if (!ctrl || ctrl->policy != POLICY_BUMP) return;
    sc_bump_ctrl_s   *c = (sc_bump_ctrl_s *)ctrl;
    sc_arena_block_s *b = c->head;
    while (b) { sc_arena_block_s *n = b->next; free(b); b = n; }
    free(c);
}

/* ------------------------------------------------------------------ */
/* Singleton                                                          */
/* ------------------------------------------------------------------ */
const sc_allocator_i Allocator = {
    .alloc       = mem_alloc,
    .dispose     = mem_dispose,
    .realloc     = mem_realloc,
    .create_bump = mem_create_bump,
    .release     = mem_release,
};
