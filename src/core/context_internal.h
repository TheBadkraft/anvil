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
// Old buffers are intentionally left in the resource scope — reclaimed together at Context.dispose.
// ctx->arena->alloc(ctx->arena, size) bumps the resource slab — O(1), never touches R7.
static inline void ci_ensure_stmt_capacity(context ctx, usize need) {
   if (ctx->stmt_list.capacity >= need)
      return;
   usize newcap = ctx->stmt_list.capacity ? ctx->stmt_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   statement *newbuf = ctx->arena->alloc(ctx->arena, sizeof(statement) * newcap);
   if (!newbuf) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate statement array", 0, 0, __FILE__);
      return;
   }
   if (ctx->stmt_list.statements)
      memcpy(newbuf, ctx->stmt_list.statements, sizeof(statement) * ctx->stmt_list.count);
   ctx->stmt_list.statements = newbuf;
   ctx->stmt_list.capacity = newcap;
}

static inline void ci_ensure_attr_capacity(context ctx, usize need) {
   if (ctx->attr_list.capacity >= need)
      return;
   usize newcap = ctx->attr_list.capacity ? ctx->attr_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   attribute *newbuf = ctx->arena->alloc(ctx->arena, sizeof(attribute) * newcap);
   if (!newbuf)
      return;
   if (ctx->attr_list.attributes)
      memcpy(newbuf, ctx->attr_list.attributes, sizeof(attribute) * ctx->attr_list.count);
   ctx->attr_list.attributes = newbuf;
   ctx->attr_list.capacity = newcap;
}

static inline void ci_ensure_field_capacity(context ctx, usize need) {
   if (ctx->field_list.capacity >= need)
      return;
   usize newcap = ctx->field_list.capacity ? ctx->field_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   field *newbuf = ctx->arena->alloc(ctx->arena, sizeof(field) * newcap);
   if (!newbuf)
      return;
   if (ctx->field_list.fields)
      memcpy(newbuf, ctx->field_list.fields, sizeof(field) * ctx->field_list.count);
   ctx->field_list.fields = newbuf;
   ctx->field_list.capacity = newcap;
}

static inline void ci_ensure_vars_capacity(context ctx, usize need) {
   if (ctx->vars_list.capacity >= need)
      return;
   usize newcap = ctx->vars_list.capacity ? ctx->vars_list.capacity * 2 : 8;
   while (newcap < need)
      newcap *= 2;
   struct anvl_vars_entry *newbuf = ctx->arena->alloc(ctx->arena, sizeof(struct anvl_vars_entry) * newcap);
   if (!newbuf) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate vars entry array", 0, 0, __FILE__);
      return;
   }
   if (ctx->vars_list.entries)
      memcpy(newbuf, ctx->vars_list.entries, sizeof(struct anvl_vars_entry) * ctx->vars_list.count);
   ctx->vars_list.entries = newbuf;
   ctx->vars_list.capacity = newcap;
}

static inline void ci_add_vars_entry(context ctx, struct anvl_vars_entry e) {
   ci_ensure_vars_capacity(ctx, ctx->vars_list.count + 1);
   if (!ctx->vars_list.entries)
      return;
   ctx->vars_list.entries[ctx->vars_list.count++] = e;
}

static inline void ci_ensure_import_capacity(context ctx, usize need) {
   if (ctx->import_list.capacity >= need)
      return;
   usize newcap = ctx->import_list.capacity ? ctx->import_list.capacity * 2 : 4;
   while (newcap < need)
      newcap *= 2;
   struct anvl_import_decl *newbuf = ctx->arena->alloc(ctx->arena, sizeof(struct anvl_import_decl) * newcap);
   if (!newbuf) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate import decl array", 0, 0, __FILE__);
      return;
   }
   if (ctx->import_list.decls)
      memcpy(newbuf, ctx->import_list.decls, sizeof(struct anvl_import_decl) * ctx->import_list.count);
   ctx->import_list.decls = newbuf;
   ctx->import_list.capacity = newcap;
}

static inline void ci_add_import_decl(context ctx, struct anvl_import_decl d) {
   ci_ensure_import_capacity(ctx, ctx->import_list.count + 1);
   if (!ctx->import_list.decls)
      return;
   ctx->import_list.decls[ctx->import_list.count++] = d;
}

static inline void ci_ensure_using_capacity(context ctx, usize need) {
   if (ctx->using_list.capacity >= need)
      return;
   usize newcap = ctx->using_list.capacity ? ctx->using_list.capacity * 2 : 4;
   while (newcap < need) newcap *= 2;
   struct anvl_using_decl *newbuf =
       ctx->arena->alloc(ctx->arena, sizeof(struct anvl_using_decl) * newcap);
   if (!newbuf) return;
   if (ctx->using_list.decls)
      memcpy(newbuf, ctx->using_list.decls,
             sizeof(struct anvl_using_decl) * ctx->using_list.count);
   ctx->using_list.decls = newbuf;
   ctx->using_list.capacity = newcap;
}

static inline void ci_add_using_decl(context ctx, struct anvl_using_decl d) {
   ci_ensure_using_capacity(ctx, ctx->using_list.count + 1);
   if (!ctx->using_list.decls) return;
   ctx->using_list.decls[ctx->using_list.count++] = d;
}

// Constructors — allocated from the context resource scope via ctx->arena->alloc(ctx->arena, size).
static inline statement ci_new_statement(context ctx, anvl_stmt_type type) {
   statement s = ctx->arena->alloc(ctx->arena, sizeof(*s));
   if (s) {
      memset(s, 0, sizeof(*s));
      s->meta[STMT_META_TYPE] = (usize)type;
   }
   return s;
}

static inline value ci_new_value(context ctx, anvl_value_type type) {
   value v = ctx->arena->alloc(ctx->arena, sizeof(*v));
   if (v) {
      memset(v, 0, sizeof(*v));
      v->type = type;
   }
   return v;
}

static inline field ci_new_field(context ctx) {
   field f = ctx->arena->alloc(ctx->arena, sizeof(struct anvl_field));
   if (f)
      memset(f, 0, sizeof(struct anvl_field));
   return f;
}

static inline attribute ci_new_attribute(context ctx) {
   attribute a = ctx->arena->alloc(ctx->arena, sizeof(struct anvl_attribute));
   if (a)
      memset(a, 0, sizeof(struct anvl_attribute));
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
 */
static inline struct anvl_base_meta *ci_new_base_meta(context ctx) {
   struct anvl_base_meta *bm = ctx->arena->alloc(ctx->arena, sizeof(struct anvl_base_meta));
   if (bm)
      memset(bm, 0, sizeof(struct anvl_base_meta));
   return bm;
}

/**
 * Allocate and return a value_meta structure for detailed value information.
 */
static inline struct anvl_value_meta *ci_new_value_meta(context ctx, anvl_value_type type) {
   struct anvl_value_meta *vm = ctx->arena->alloc(ctx->arena, sizeof(*vm));
   if (vm) {
      memset(vm, 0, sizeof(*vm));
      vm->type = type;
   }
   return vm;
}

/**
 * Allocate an element_meta array for collection types (arrays/tuples).
 */
static inline struct anvl_element_meta *ci_new_element_meta_array(context ctx, usize count) {
   struct anvl_element_meta *em = ctx->arena->alloc(ctx->arena, sizeof(struct anvl_element_meta) * count);
   if (em)
      memset(em, 0, sizeof(struct anvl_element_meta) * count);
   return em;
}

/**
 * Allocate an attr_meta array for statement-level attributes.
 */
static inline struct anvl_attr_meta *ci_new_attr_meta_array(context ctx, usize count) {
   struct anvl_attr_meta *am = ctx->arena->alloc(ctx->arena, sizeof(struct anvl_attr_meta) * count);
   if (am)
      memset(am, 0, sizeof(struct anvl_attr_meta) * count);
   return am;
}
