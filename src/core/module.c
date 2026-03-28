/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * src/core/module.c — Anvil module descriptor and lifecycle
 * ------------------------------------------------------------------
 * Registers the "anvil" module with sigma.core's module system.
 * Depend on sigma.memory so Allocator is ready before init() runs.
 *
 * init():
 *   • Creates a bump controller for scratch allocations.
 *   • Configures StringBuilder to route through that controller so
 *     serializer/round-trip output never touches the global SLB0.
 *
 * shutdown():
 *   • Restores StringBuilder to the global Allocator fallback.
 *   • Releases the bump controller and its backing slab.
 */

#include <sigma.core/allocator.h>
#include <sigma.core/module.h>
#include <sigma.core/strings.h>
#include <sigma.memory/memory.h>

/* ── Module-scoped bump controller ────────────────────────────────── */
static bump_allocator _anvil_bump = NULL;
static sc_alloc_use_t _anvil_alloc_use;

static void *_bump_alloc(usize size) {
   return _anvil_bump->alloc(_anvil_bump, size);
}

static void _bump_release(void *ptr) {
   (void)ptr; /* bump controllers reclaim on reset/release, not per-pointer */
}

static void *_bump_resize(void *ptr, usize size) {
   (void)ptr; /* bump cannot resize in place; allocate fresh */
   return _anvil_bump->alloc(_anvil_bump, size);
}

/* ── Lifecycle ─────────────────────────────────────────────────────── */
static int _anvil_init(void *ctx) {
   (void)ctx;

   _anvil_bump = Allocator.create_bump(1024 * 1024); /* 1 MiB scratch slab */
   if (!_anvil_bump)
      return 1;

   _anvil_alloc_use = (sc_alloc_use_t){
       .ctrl = (sc_ctrl_base_s *)_anvil_bump,
       .alloc = _bump_alloc,
       .release = _bump_release,
       .resize = _bump_resize,
       .frame_begin = NULL,
       .frame_end = NULL,
   };

   /* NOTE: StringBuilder.alloc_use() removed in FR-005/006.
    * StringBuilder now always uses global Allocator.
    * Bump controller remains available for future allocator-configurable features. */
   return 0;
}

static void _anvil_shutdown(void) {
   /* StringBuilder.alloc_use() removed in FR-005/006 — no restoration needed */
   /* Do NOT release _anvil_bump here: sigma.memory will drain all registered
      controllers on its own shutdown, which runs after every USER module. */
}

/* ── Module descriptor ─────────────────────────────────────────────── */
static const char *_anvil_deps[] = {"sigma.memory", NULL};

static const sigma_module_t _anvil_module = {
    .name = "anvil",
    .version = "0.1.0-alpha",
    .role = SIGMA_ROLE_USER,
    .alloc = SIGMA_ALLOC_DEFAULT,
    .deps = _anvil_deps,
    .init = _anvil_init,
    .shutdown = _anvil_shutdown,
};

__attribute__((constructor)) static void _register_anvil_module(void) {
   sigma_module_register(&_anvil_module);
}
