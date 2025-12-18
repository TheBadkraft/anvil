/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * source.c - Source buffer management
 * ------------------------------------------------------------------
 * Note: The anvl_source_i vtable and all interrogator functions have
 * been moved to context.c where the public API is centralized.
 * This file is kept as a stub for future source-specific utilities.
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-13
 * File: src/core/source.c
 * ------------------------------------------------------------------
 * Internal context helpers: simple pool-style creators and appenders.
 * These construct statements/values/fields/attributes directly without builders.
 */
#pragma once

#include "context.h"
#include <sigcore/memory.h>

// Ensure capacity helpers (grow by 2x, minimal 8)
static inline void ci_ensure_stmt_capacity(context ctx, usize need) {
   if (ctx->stmt_list.capacity >= need)
      return;
   usize newcap = ctx->stmt_list.capacity ? ctx->stmt_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   if (ctx->stmt_list.statements) {
      ctx->stmt_list.statements = Memory.realloc(ctx->stmt_list.statements, sizeof(statement) * newcap);
   } else {
      ctx->stmt_list.statements = Memory.alloc(sizeof(statement) * newcap, true);
      if (!ctx->stmt_list.statements) {
         // Allocation failed - log error
         anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate statement array", 0, 0, __FILE__);
         return;
      }
   }
   ctx->stmt_list.capacity = newcap;
}

static inline void ci_ensure_attr_capacity(context ctx, usize need) {
   if (ctx->attr_list.capacity >= need)
      return;
   usize newcap = ctx->attr_list.capacity ? ctx->attr_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   if (ctx->attr_list.attributes) {
      ctx->attr_list.attributes = Memory.realloc(ctx->attr_list.attributes, sizeof(attribute) * newcap);
   } else {
      ctx->attr_list.attributes = Memory.alloc(sizeof(attribute) * newcap, true);
   }
   ctx->attr_list.capacity = newcap;
}

static inline void ci_ensure_field_capacity(context ctx, usize need) {
   if (ctx->field_list.capacity >= need)
      return;
   usize newcap = ctx->field_list.capacity ? ctx->field_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   if (ctx->field_list.fields) {
      ctx->field_list.fields = Memory.realloc(ctx->field_list.fields, sizeof(field) * newcap);
   } else {
      ctx->field_list.fields = Memory.alloc(sizeof(field) * newcap, true);
   }
   ctx->field_list.capacity = newcap;
}

static inline void ci_ensure_value_capacity(context ctx, usize need) {
   if (ctx->value_list.capacity >= need)
      return;
   usize newcap = ctx->value_list.capacity ? ctx->value_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   if (ctx->value_list.values) {
      ctx->value_list.values = Memory.realloc(ctx->value_list.values, sizeof(value) * newcap);
   } else {
      ctx->value_list.values = Memory.alloc(sizeof(value) * newcap, true);
   }
   ctx->value_list.capacity = newcap;
}

// Constructors (zero-initialized)
static inline statement ci_new_statement(context ctx __attribute__((unused)), anvl_stmt_type type) {
   statement s = Memory.alloc(sizeof(*s), true);
   s->meta[STMT_META_TYPE] = (usize)type;
   return s;
}

static inline value ci_new_value(context ctx __attribute__((unused)), anvl_value_type type) {
   value v = Memory.alloc(sizeof(*v), true);
   v->type = type;
   return v;
}

static inline field ci_new_field(context ctx __attribute__((unused))) {
   field f = Memory.alloc(sizeof(*f), true);
   return f;
}

static inline attribute ci_new_attribute(context ctx __attribute__((unused))) {
   attribute a = Memory.alloc(sizeof(*a), true);
   return a;
}

// Append helpers to context pools
static inline void ci_add_statement(context ctx, statement s) {
   ci_ensure_stmt_capacity(ctx, ctx->stmt_list.count + 1);
   if (!ctx->stmt_list.statements) {
      // DEBUG: This shouldn't happen, but let's fail gracefully
      return;
   }
   ctx->stmt_list.statements[ctx->stmt_list.count++] = s;
}

static inline void ci_add_attribute(context ctx, attribute a) {
   ci_ensure_attr_capacity(ctx, ctx->attr_list.count + 1);
   ctx->attr_list.attributes[ctx->attr_list.count++] = a;
}

static inline void ci_add_field(context ctx, field f) {
   ci_ensure_field_capacity(ctx, ctx->field_list.count + 1);
   ctx->field_list.fields[ctx->field_list.count++] = f;
}

static inline void ci_add_value(context ctx, value v) {
   ci_ensure_value_capacity(ctx, ctx->value_list.count + 1);
   ctx->value_list.values[ctx->value_list.count++] = v;
}
