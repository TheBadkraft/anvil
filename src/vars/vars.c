/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * vars.c — anvil.vars: VarRef + interpolated string resolution
 * ------------------------------------------------------------------
 * Implements the Vars vtable:
 *   Vars.build()              — resolve VarRef chains, detect cycles
 *   Vars.resolve()            — look up a key in the resolved state
 *   Vars.materialise_interp() — expand $"…{ref}…" to a heap string
 *   Vars.dispose()            — free vars state
 * ------------------------------------------------------------------
 */

#include "vars.h"
#include "context.h"
#include "errors.h"
#include <sigma.core/strings.h>
#include <sigma.memory/memory.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* vars_build                                                         */
/* ------------------------------------------------------------------ */

/* Forward ref to resolve used within the build loop */
static bool vars_resolve(anvl_vars_state_t *state, const char *key, usize key_len,
                         usize *out_pos, usize *out_len, anvl_value_type *out_type);

static anvl_vars_state_t *vars_build(context ctx) {
   if (!ctx)
      return NULL;

   anvl_vars_state_t *state = Allocator.alloc(sizeof(anvl_vars_state_t));
   if (!state) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return NULL;
   }
   memset(state, 0, sizeof(anvl_vars_state_t));
   state->src = ctx->source;

   usize n = ctx->vars_list.count;
   if (n == 0)
      return state; /* empty vars list — valid empty state */

   state->entries = Allocator.alloc(sizeof(struct anvl_vars_resolved) * n);
   if (!state->entries) {
      Allocator.free(state);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return NULL;
   }
   memset(state->entries, 0, sizeof(struct anvl_vars_resolved) * n);
   state->count = n;

   /* Copy raw entries into resolved table (initial pass) */
   const char *src_data = Source.data(ctx->source);
   for (usize i = 0; i < n; i++) {
      state->entries[i].key_pos = ctx->vars_list.entries[i].key_pos;
      state->entries[i].key_len = ctx->vars_list.entries[i].key_len;
      state->entries[i].value_pos = ctx->vars_list.entries[i].value_pos;
      state->entries[i].value_len = ctx->vars_list.entries[i].value_len;
      state->entries[i].value_type = ctx->vars_list.entries[i].value_type;
   }

   /*
    * Resolve VarRef chains with cycle detection.
    * For each entry i that is a VARREF, follow the chain until we reach a
    * non-VARREF terminal or detect a cycle.
    *
    * done[i]:
    *   0 = unvisited
    *   1 = in the current DFS path (detecting cycle)
    *   2 = fully resolved
    */
   uint8_t *done = Allocator.alloc(n);
   if (!done) {
      Allocator.free(state->entries);
      Allocator.free(state);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return NULL;
   }
   memset(done, 0, n);

   /* Stack to track current DFS path (maximum chain = n entries) */
   usize *path = Allocator.alloc(sizeof(usize) * (n + 1));
   if (!path) {
      Allocator.free(done);
      Allocator.free(state->entries);
      Allocator.free(state);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return NULL;
   }

   for (usize i = 0; i < n; i++) {
      if (done[i] == 2)
         continue; /* already resolved */
      if (state->entries[i].value_type != ANVL_VALUE_VARREF) {
         done[i] = 2; /* terminal — nothing to resolve */
         continue;
      }

      /* DFS: follow the VarRef chain from entry i */
      usize path_len = 0;
      usize cur = i;

      while (true) {
         if (done[cur] == 2) {
            /* already resolved: short-circuit. Update all path entries. */
            for (usize k = 0; k < path_len; k++) {
               usize pk = path[k];
               state->entries[pk].value_pos = state->entries[cur].value_pos;
               state->entries[pk].value_len = state->entries[cur].value_len;
               state->entries[pk].value_type = state->entries[cur].value_type;
               done[pk] = 2;
            }
            break;
         }

         if (done[cur] == 1) {
            /* cur is already on the current path → cycle detected */
            Allocator.free(path);
            Allocator.free(done);
            Allocator.free(state->entries);
            Allocator.free(state);
            anvl_error_set(ANVL_ERR_VARS_CIRCULAR_REF,
                           anvl_error_code_message(ANVL_ERR_VARS_CIRCULAR_REF),
                           0, 0, __FILE__);
            return NULL;
         }

         if (state->entries[cur].value_type != ANVL_VALUE_VARREF) {
            /* Terminal: update all path entries and mark done */
            for (usize k = 0; k < path_len; k++) {
               usize pk = path[k];
               state->entries[pk].value_pos = state->entries[cur].value_pos;
               state->entries[pk].value_len = state->entries[cur].value_len;
               state->entries[pk].value_type = state->entries[cur].value_type;
               done[pk] = 2;
            }
            done[cur] = 2;
            break;
         }

         /* cur is a VARREF — find where it points */
         const char *ref_key = src_data + state->entries[cur].value_pos;
         usize ref_len = state->entries[cur].value_len;

         usize next = n; /* sentinel: not found */
         for (usize j = 0; j < n; j++) {
            if (ctx->vars_list.entries[j].key_len == ref_len &&
                strncmp(src_data + ctx->vars_list.entries[j].key_pos, ref_key, ref_len) == 0) {
               next = j;
               break;
            }
         }

         if (next == n) {
            /*
             * Missing key: the VarRef points to an undefined entry.
             * Leave these entries unresolved (still VARREF type).
             * Vars.resolve() will report ANVL_ERR_VARS_KEY_NOT_FOUND at runtime.
             */
            for (usize k = 0; k < path_len; k++)
               done[path[k]] = 2; /* mark as "done" in the sense of "no more work" */
            done[cur] = 2;
            break;
         }

         /* Mark cur as "in path" and push onto path stack */
         done[cur] = 1;
         path[path_len++] = cur;
         cur = next;
      }
   }

   Allocator.free(path);
   Allocator.free(done);
   return state;
}

/* ------------------------------------------------------------------ */
/* vars_resolve                                                       */
/* ------------------------------------------------------------------ */
static bool vars_resolve(anvl_vars_state_t *state, const char *key, usize key_len,
                         usize *out_pos, usize *out_len, anvl_value_type *out_type) {
   if (!state || !key || key_len == 0) {
      anvl_error_set(ANVL_ERR_VARS_KEY_NOT_FOUND,
                     anvl_error_code_message(ANVL_ERR_VARS_KEY_NOT_FOUND),
                     0, 0, __FILE__);
      return false;
   }

   const char *src_data = Source.data(state->src);

   for (usize i = 0; i < state->count; i++) {
      struct anvl_vars_resolved *e = &state->entries[i];
      if (e->key_len == key_len &&
          strncmp(src_data + e->key_pos, key, key_len) == 0) {
         /* Found — but still a VARREF means the chain had a missing link */
         if (e->value_type == ANVL_VALUE_VARREF) {
            anvl_error_set(ANVL_ERR_VARS_KEY_NOT_FOUND,
                           anvl_error_code_message(ANVL_ERR_VARS_KEY_NOT_FOUND),
                           0, 0, __FILE__);
            return false;
         }
         *out_pos = e->value_pos;
         *out_len = e->value_len;
         *out_type = e->value_type;
         return true;
      }
   }

   /* Key not found in vars table at all */
   anvl_error_set(ANVL_ERR_VARS_KEY_NOT_FOUND,
                  anvl_error_code_message(ANVL_ERR_VARS_KEY_NOT_FOUND),
                  0, 0, __FILE__);
   return false;
}

/* ------------------------------------------------------------------ */
/* vars_materialise_interp                                            */
/* ------------------------------------------------------------------ */
static char *vars_materialise_interp(anvl_vars_state_t *state, context ctx,
                                     const struct anvl_value_meta *vm) {
   if (!vm || vm->type != ANVL_VALUE_INTERP_STRING)
      return NULL;

   const char *src_data = Source.data(ctx->source);
   string_builder sb = StringBuilder.new(64);
   if (!sb) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return NULL;
   }

   for (usize i = 0; i < vm->data.interp_string.segment_count; i++) {
      const struct anvl_interp_segment *seg = &vm->data.interp_string.segments[i];
      if (!seg->is_ref) {
         /* Literal span: copy directly from source */
         StringBuilder.appendf(sb, "%.*s", (int)seg->len, src_data + seg->pos);
      } else {
         /* Reference hole: resolve the identifier and copy its value */
         usize rpos, rlen;
         anvl_value_type rtype;
         if (!vars_resolve(state, src_data + seg->pos, seg->len, &rpos, &rlen, &rtype)) {
            StringBuilder.dispose(sb);
            return NULL; /* error already set by vars_resolve */
         }
         StringBuilder.appendf(sb, "%.*s", (int)rlen, src_data + rpos);
      }
   }

   string result = StringBuilder.toString(sb);
   StringBuilder.dispose(sb);
   return result;
}

/* ------------------------------------------------------------------ */
/* vars_resolve_path                                                  */
/*                                                                    */
/* Resolves a dotted VarRef path against statement fields.           */
/* "stmt.field" → looks up stmt by name, then field within it.       */
/* Plain (no dot) paths delegate to vars_resolve.                    */
/*                                                                    */
/* WIP: only single-level dot ("a.b") is supported.  Deeper paths   */
/* ("a.b.c") are not yet implemented.                                */
/* ------------------------------------------------------------------ */
static bool vars_resolve_path(anvl_vars_state_t *state, context ctx,
                              const char *path, usize path_len,
                              usize *out_pos, usize *out_len,
                              anvl_value_type *out_type) {
   if (!path || path_len == 0) {
      anvl_error_set(ANVL_ERR_VARS_KEY_NOT_FOUND,
                     anvl_error_code_message(ANVL_ERR_VARS_KEY_NOT_FOUND),
                     0, 0, __FILE__);
      return false;
   }

   /* Find first dot separator */
   const char *dot = NULL;
   for (usize i = 0; i < path_len; i++) {
      if (path[i] == '.') {
         dot = path + i;
         break;
      }
   }

   /* No dot — delegate to flat vars_resolve */
   if (!dot)
      return vars_resolve(state, path, path_len, out_pos, out_len, out_type);

   usize head_len = (usize)(dot - path);
   const char *tail = dot + 1;
   usize tail_len = path_len - head_len - 1;

   /* Look up the leading name as a statement */
   statement stmt = Context.get_statement_by_name(ctx, path, head_len);
   if (!stmt) {
      /* Not a statement — try flat vars_resolve with the full key */
      return vars_resolve(state, path, path_len, out_pos, out_len, out_type);
   }

   /* Find named field within that statement */
   field f = Context.get_field_by_name(ctx, stmt, tail, tail_len);
   if (!f || !f->val) {
      anvl_error_set(ANVL_ERR_VARS_KEY_NOT_FOUND,
                     anvl_error_code_message(ANVL_ERR_VARS_KEY_NOT_FOUND),
                     0, 0, __FILE__);
      return false;
   }

   /* TODO: if f->val->type == ANVL_VALUE_OBJECT, support deeper traversal */
   *out_pos = f->val->data.scalar.pos;
   *out_len = f->val->data.scalar.len;
   *out_type = f->val->type;
   return true;
}

/* ------------------------------------------------------------------ */
/* vars_dispose                                                       */
/* ------------------------------------------------------------------ */
static void vars_dispose(anvl_vars_state_t *state) {
   if (!state)
      return;
   if (state->entries)
      Allocator.free(state->entries);
   Allocator.free(state);
}

/* ------------------------------------------------------------------ */
/* Public vtable                                                      */
/* ------------------------------------------------------------------ */
const anvl_vars_i Vars = {
    .build = vars_build,
    .resolve = vars_resolve,
    .resolve_path = vars_resolve_path,
    .materialise_interp = vars_materialise_interp,
    .dispose = vars_dispose,
};
