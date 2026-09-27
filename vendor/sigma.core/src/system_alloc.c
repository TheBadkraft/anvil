/*
 * SigmaCore
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------
 * File: system_alloc.c
 * Description: sigma.system.alloc — zero-dependency, zero-setup sc_allocator_i
 *              provider (FR-2609-sigmem-001 / SR-2609-sigma-collections-001).
 *
 *              malloc-backed alloc/dispose/realloc, plus a chained-block bump
 *              arena for create_bump. No sigma.core Module/Application
 *              bootstrap requirement — usable from the very first call, for
 *              consumers (Anvil, or any bare sigma.collections consumer) that
 *              don't want sigma.memory's SLB0/mmap controller model.
 *
 *              Ported from anvil/src/sigma/memory.c, the reference
 *              implementation this package brings into the sigma.* ecosystem
 *              proper. include/system_alloc.h provides the matching full
 *              struct bodies — see that header for why a consumer must
 *              include it (not sigma.memory's memory.h) when linking this
 *              object.
 *
 *              create_reclaim / acquire / create_custom are explicitly
 *              implemented to return NULL, and register_ctrl as an explicit
 *              no-op, rather than left as unset (NULL) function pointers —
 *              a caller that tries them gets a clean NULL/no-op instead of a
 *              crash through an empty vtable slot. is_ready() explicitly
 *              returns false for the same reason: this provider never
 *              reaches the "fully wired" state the real sigma.memory
 *              Allocator represents (TC-SR-004).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "system_alloc.h"

#define ARENA_ALIGN sizeof(max_align_t)
#define ALIGN_UP(n) (((n) + ARENA_ALIGN - 1) & ~(ARENA_ALIGN - 1))
#define DEFAULT_BLOCK_SZ (64u * 1024u)

// ── Global allocator ─────────────────────────────────────────────────────
static void *mem_alloc(usize size) {
    if (!size) return NULL;
    void *p = malloc(size);
    if (p) memset(p, 0, size);
    return p;
}

static void mem_dispose(void *ptr) {
    free(ptr);
}

static void *mem_realloc(void *ptr, usize size) {
    if (!size) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

// ── Bump arena ────────────────────────────────────────────────────────────
static sc_arena_block_s *block_new(usize capacity) {
    sc_arena_block_s *b = malloc(sizeof(sc_arena_block_s) + capacity);
    if (!b) return NULL;
    b->next = NULL;
    b->capacity = capacity;
    b->used = 0;
    return b;
}

static void *bump_alloc(struct sc_bump_ctrl_s *self, usize size) {
    if (!self || !size) return NULL;
    usize aligned = ALIGN_UP(size);

    if (self->current && (self->current->used + aligned) <= self->current->capacity) {
        void *p = (uint8_t *)(self->current + 1) + self->current->used;
        self->current->used += aligned;
        memset(p, 0, size);
        return p;
    }

    usize block_sz = self->default_block;
    if (aligned > block_sz) block_sz = aligned;

    sc_arena_block_s *b = block_new(block_sz);
    if (!b) return NULL;

    if (!self->head) {
        self->head = b;
        self->current = b;
    } else {
        self->current->next = b;
        self->current = b;
    }

    void *p = (uint8_t *)(b + 1);
    b->used = aligned;
    memset(p, 0, size);
    return p;
}

static bump_allocator mem_create_bump(usize initial) {
    struct sc_bump_ctrl_s *c = malloc(sizeof(struct sc_bump_ctrl_s));
    if (!c) return NULL;
    memset(c, 0, sizeof(struct sc_bump_ctrl_s));
    c->base.policy = POLICY_BUMP;
    c->alloc = bump_alloc;
    c->default_block = initial ? initial : DEFAULT_BLOCK_SZ;
    c->head = block_new(c->default_block);
    c->current = c->head;
    if (!c->head) {
        free(c);
        return NULL;
    }
    return c;
}

static void mem_release(sc_ctrl_base_s *ctrl) {
    if (!ctrl || ctrl->policy != POLICY_BUMP) return;
    struct sc_bump_ctrl_s *c = (struct sc_bump_ctrl_s *)ctrl;
    sc_arena_block_s *b = c->head;
    while (b) {
        sc_arena_block_s *n = b->next;
        free(b);
        b = n;
    }
    free(c);
}

// ── Explicit "unsupported" stubs — never left as bare NULL fn ptrs ────────
static reclaim_allocator mem_create_reclaim(usize size) {
    (void)size;
    return NULL;
}

static slab mem_acquire(usize size) {
    (void)size;
    return NULL;
}

static sc_ctrl_base_s *mem_create_custom(usize size, ctrl_factory_fn factory) {
    (void)size;
    (void)factory;
    return NULL;
}

static void mem_register_ctrl(sc_ctrl_base_s *ctrl) {
    (void)ctrl;
    // No-op: this provider has no registry to enroll an externally-built
    // controller into.
}

static bool mem_is_ready(void) {
    return false;  // No Module/Application bootstrap contract to satisfy.
}

// ── Singleton ─────────────────────────────────────────────────────────────
const sc_allocator_i Allocator = {
    .acquire = mem_acquire,
    .release = mem_release,
    .create_bump = mem_create_bump,
    .create_reclaim = mem_create_reclaim,
    .alloc = mem_alloc,
    .dispose = mem_dispose,
    .realloc = mem_realloc,
    .create_custom = mem_create_custom,
    .register_ctrl = mem_register_ctrl,
    .is_ready = mem_is_ready,
};
