/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * anvil.c - Implementation of Anvil public API
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-13
 * File: src/core/anvil.c
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include "context.h"
#include "utils.h"
#include <sigma.memory/memory.h>
#include <string.h>

/* Root structure */
struct anvl_root_t {
   int dummy;
};

/* Detect dialect from source content (shebang) */
/* Stub implementations for Iteration 1 */

static root anvil_load(const char *filepath) {
   (void)filepath;
   root r = Allocator.alloc(sizeof(struct anvl_root_t)); // minimal stub
   if (r)
      memset(r, 0, sizeof(struct anvl_root_t));
   return r;
}

static root anvil_read(const char *source, usize len) {
   (void)source;
   (void)len;
   root r = Allocator.alloc(sizeof(struct anvl_root_t)); // minimal
   if (r)
      memset(r, 0, sizeof(struct anvl_root_t));
   return r;
}

static void anvil_cleanup(void) {
   // currently no global resources to clean up
   // make sure no residual builder state lingers
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->dialect = ANVL_DIALECT_ASL;
   if (builder->source) {
      Source.dispose(builder->source);
      builder->source = NULL;
   }
}

static void anvil_dispose(root r) {
   Allocator.free(r);
}

static bool anvil_error_is_set(void) {
   return anvl_error_is_set();
}

static const anvl_error_state *anvil_error_get(void) {
   return anvl_error_get();
}

static void anvil_error_clear(void) {
   anvl_error_clear();
}

const anvl_i Anvil = {
    .load = anvil_load,
    .read = anvil_read,
    .dispose = anvil_dispose,
    .cleanup = anvil_cleanup,
    .error_is_set = anvil_error_is_set,
    .error_get = anvil_error_get,
    .error_clear = anvil_error_clear,
};