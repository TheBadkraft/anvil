/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * vars.h — anvil.vars: VarRef + interpolated string resolution
 * ------------------------------------------------------------------
 * Ported from: AnvilVarsState (Anvil.Net rc.2/rc.3)
 *
 * Processes a parsed context's vars block, resolves VarRef chains,
 * detects circular references, and materialises interpolated strings.
 *
 * Component: post-parse optional (anvil.vars.o)
 *
 * Design divergences from .Net reference (intentional):
 *   - Interpolation hole syntax: {ref} not {.ref}
 *   - No exceptions — errors set via anvl_error_set(), return NULL/false
 *   - Circular ref detection: Vars.build() returns NULL + sets error
 *   - Missing key: Vars.resolve() returns false + sets error (lazy)
 * ------------------------------------------------------------------
 */
#pragma once

#include "context.h"
#include "types.h"
#include <stdbool.h>
#include <stddef.h>

/* anvl_vars_entry is defined in types.h (included via context.h) */

/* ------------------------------------------------------------------ */
/* Resolved vars entry (built by Vars.build from context vars_list)  */
/* pos/len point to the final scalar/object/array/tuple/blob span     */
/* after following any VarRef chain.                                  */
/* ------------------------------------------------------------------ */
struct anvl_vars_resolved {
   usize key_pos;
   usize key_len;
   usize value_pos;
   usize value_len;
   anvl_value_type value_type;
};

/* ------------------------------------------------------------------ */
/* Vars state (returned by Vars.build)                               */
/* ------------------------------------------------------------------ */
typedef struct anvl_vars_state_t {
   struct anvl_vars_resolved *entries;
   usize count;
   /* back-reference to source for key comparisons */
   source src;
} anvl_vars_state_t;

/* ------------------------------------------------------------------ */
/* Vars interface vtable                                              */
/* ------------------------------------------------------------------ */
typedef struct anvl_vars_i {
   /* Build resolved vars state from a parsed context.
    * Detects circular VarRef chains eagerly.
    * Returns NULL + sets ANVL_ERR_VARS_CIRCULAR_REF on cycle. */
   anvl_vars_state_t *(*build)(context ctx);

   /* Resolve a key to its final value span.
    * key/key_len: name of the variable (not NUL-terminated required).
    * Writes resolved pos/len/type to out_* parameters.
    * Returns false + sets ANVL_ERR_VARS_KEY_NOT_FOUND if missing. */
   bool (*resolve)(anvl_vars_state_t *state,
                   const char *key, usize key_len,
                   usize *out_pos, usize *out_len,
                   anvl_value_type *out_type);

   /* Resolve a dotted path (e.g. "changelog.version") to a value span.
    * First component is looked up as a statement name; remaining components
    * traverse object fields.  Single-level paths fall back to Vars.resolve.
    * Returns false + sets error code on failure.
    * NOTE: multi-level (a.b.c) is WIP — only one dot supported. */
   bool (*resolve_path)(anvl_vars_state_t *state, context ctx,
                        const char *path, usize path_len,
                        usize *out_pos, usize *out_len,
                        anvl_value_type *out_type);

   /* Materialise an interpolated string value_meta to a heap string.
    * vm must have type ANVL_VALUE_INTERP_STRING.
    * Returns heap-allocated NUL-terminated string; caller must Allocator.dispose().
    * Returns NULL + sets ANVL_ERR_VARS_KEY_NOT_FOUND if a hole ref is missing. */
   char *(*materialise_interp)(anvl_vars_state_t *state, context ctx,
                               const struct anvl_value_meta *vm);

   /* Free vars state and all owned memory. */
   void (*dispose)(anvl_vars_state_t *state);
} anvl_vars_i;

extern const anvl_vars_i Vars;
