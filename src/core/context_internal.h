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
#include <sigma.memory/memory.h>
#include <string.h>

// Ensure capacity helpers (grow by 2x, minimal 8)
static inline void ci_ensure_stmt_capacity(context ctx, usize need) {
   if (ctx->stmt_list.capacity >= need)
      return;
   usize newcap = ctx->stmt_list.capacity ? ctx->stmt_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   statement *newbuf = Allocator.alloc(sizeof(statement) * newcap);
   if (!newbuf) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate statement array", 0, 0, __FILE__);
      return;
   }
   memset(newbuf, 0, sizeof(statement) * newcap);
   if (ctx->stmt_list.statements) {
      memcpy(newbuf, ctx->stmt_list.statements, sizeof(statement) * ctx->stmt_list.count);
      Allocator.dispose(ctx->stmt_list.statements);
   }
   ctx->stmt_list.statements = newbuf;
   ctx->stmt_list.capacity = newcap;
}

static inline void ci_ensure_attr_capacity(context ctx, usize need) {
   if (ctx->attr_list.capacity >= need)
      return;
   usize newcap = ctx->attr_list.capacity ? ctx->attr_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   attribute *newbuf = Allocator.alloc(sizeof(attribute) * newcap);
   if (!newbuf)
      return;
   memset(newbuf, 0, sizeof(attribute) * newcap);
   if (ctx->attr_list.attributes) {
      memcpy(newbuf, ctx->attr_list.attributes, sizeof(attribute) * ctx->attr_list.count);
      Allocator.dispose(ctx->attr_list.attributes);
   }
   ctx->attr_list.attributes = newbuf;
   ctx->attr_list.capacity = newcap;
}

static inline void ci_ensure_field_capacity(context ctx, usize need) {
   if (ctx->field_list.capacity >= need)
      return;
   usize newcap = ctx->field_list.capacity ? ctx->field_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   field *newbuf = Allocator.alloc(sizeof(field) * newcap);
   if (!newbuf)
      return;
   memset(newbuf, 0, sizeof(field) * newcap);
   if (ctx->field_list.fields) {
      memcpy(newbuf, ctx->field_list.fields, sizeof(field) * ctx->field_list.count);
      Allocator.dispose(ctx->field_list.fields);
   }
   ctx->field_list.fields = newbuf;
   ctx->field_list.capacity = newcap;
}

// Constructors (zero-initialized)
static inline statement ci_new_statement(context ctx __attribute__((unused)), anvl_stmt_type type) {
   statement s = Allocator.alloc(sizeof(*s));
   if (s)
      memset(s, 0, sizeof(*s));
   s->meta[STMT_META_TYPE] = (usize)type;
   return s;
}

static inline value ci_new_value(context ctx __attribute__((unused)), anvl_value_type type) {
   value v = Allocator.alloc(sizeof(*v));
   if (v)
      memset(v, 0, sizeof(*v));
   v->type = type;
   return v;
}

static inline field ci_new_field(context ctx __attribute__((unused))) {
   field f = Allocator.alloc(sizeof(*f));
   if (f)
      memset(f, 0, sizeof(*f));
   return f;
}

static inline attribute ci_new_attribute(context ctx __attribute__((unused))) {
   attribute a = Allocator.alloc(sizeof(*a));
   if (a)
      memset(a, 0, sizeof(*a));
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

static inline attribute ci_get_attribute(context ctx, usize index) {
   if (index >= ctx->attr_list.count)
      return NULL;
   return ctx->attr_list.attributes[index];
}

static inline void ci_add_field(context ctx, field f) {
   ci_ensure_field_capacity(ctx, ctx->field_list.count + 1);
   ctx->field_list.fields[ctx->field_list.count++] = f;
}

/* ========================================================================
 * Meta-buffer allocation helpers for self-contained statement metadata
 * ======================================================================== */

/**
 * Allocate and return a base_meta structure for inheritance metadata.
 * Returns the allocated base_meta pointer (caller's responsibility to store).
 */
static inline struct anvl_base_meta *ci_new_base_meta(void) {
   struct anvl_base_meta *bm = Allocator.alloc(sizeof(*bm));
   if (bm)
      memset(bm, 0, sizeof(*bm));
   return bm;
}

/**
 * Allocate and return a value_meta structure for detailed value information.
 * Returns the allocated value_meta pointer (caller's responsibility to store).
 */
static inline struct anvl_value_meta *ci_new_value_meta(anvl_value_type type) {
   struct anvl_value_meta *vm = Allocator.alloc(sizeof(*vm));
   if (vm)
      memset(vm, 0, sizeof(*vm));
   if (vm) {
      vm->type = type;
   }
   return vm;
}

/**
 * Allocate an element_meta array for collection types (arrays/tuples).
 * Returns the allocated array pointer (caller responsible for storing).
 */
static inline struct anvl_element_meta *ci_new_element_meta_array(usize count) {
   struct anvl_element_meta *em = Allocator.alloc(sizeof(struct anvl_element_meta) * count);
   if (em)
      memset(em, 0, sizeof(struct anvl_element_meta) * count);
   return em;
}

/**
 * Allocate an attr_meta array for statement-level attributes.
 * Returns the allocated array pointer (caller responsible for storing).
 */
static inline struct anvl_attr_meta *ci_new_attr_meta_array(usize count) {
   struct anvl_attr_meta *am = Allocator.alloc(sizeof(struct anvl_attr_meta) * count);
   if (am)
      memset(am, 0, sizeof(struct anvl_attr_meta) * count);
   return am;
}
