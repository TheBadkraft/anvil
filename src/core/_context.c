/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * context.c - Context implementation for Anvil
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * Created: 2025-12-16
 * File: src/core/context.c
 * ----------------------------------------------------------------------- *
 */

#include "context.h"
#include "context_internal.h"
#include "parser.h"
#include "utils.h"
#include <sigma/memory.h>
#include <sigma/strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static stmt_builder context_begin_statement(context self, anvl_stmt_type type);
static void context_end_statement(context self, stmt_builder bldr, anvl_stmt_type type);
static attr_builder context_begin_attribute(context self);
static void context_end_attribute(context self, attr_builder bldr);

// Statement interface functions
static void statement_identifier(statement self, source src, char *out_identifier);
static const char *statement_base(statement self, source src);
static anvl_stmt_type statement_type(statement self);
static anvl_value_type statement_value_type(statement self);
static usize statement_length(statement self);

// Forward declarations for builder methods
void stmt_builder_identifier(stmt_builder self, usize pos, usize len);
void stmt_builder_base(stmt_builder self, usize pos, usize len);
void stmt_builder_begin_attributes(stmt_builder self);
void stmt_builder_attribute_key(stmt_builder self, usize pos, usize len);
void stmt_builder_attribute_value(stmt_builder self, usize pos, usize len);
void stmt_builder_end_attributes(stmt_builder self);
value_builder stmt_builder_begin_value(stmt_builder self, anvl_value_type type);
void stmt_builder_end_value(stmt_builder self, value_builder vbldr);
void stmt_builder_value_scalar(stmt_builder self, usize pos, usize len, anvl_value_type type);

// Attribute builder methods
void attr_builder_key(attr_builder self, usize pos, usize len);
void attr_builder_value(attr_builder self, usize pos, usize len);
void attr_builder_add(attr_builder self);

// Value builder methods
void value_builder_begin_object(value_builder self);
void value_builder_field(value_builder self, usize key_pos, usize key_len, usize attrib_start, usize attrib_count);
void value_builder_field_value(value_builder self, value val);
void value_builder_end_object(value_builder self);
void value_builder_begin_array(value_builder self);
void value_builder_element(value_builder self, value val);
void value_builder_end_array(value_builder self);
void value_builder_scalar(value_builder self, usize pos, usize len, anvl_value_type type);
static value_builder context_begin_value(context self, anvl_value_type type);
static void context_end_value(context self, value_builder bldr);

// Value Builder method implementations
void value_builder_begin_object(value_builder self);
void value_builder_field(value_builder self, usize key_pos, usize key_len, usize attrib_start, usize attrib_count);
void value_builder_field_value(value_builder self, value val);
void value_builder_end_object(value_builder self);
void value_builder_begin_array(value_builder self);
void value_builder_element(value_builder self, value val);
void value_builder_end_array(value_builder self);
void value_builder_scalar(value_builder self, usize pos, usize len, anvl_value_type type);

static bool builder_load_file(ctx_builder self, const char *filepath) {
   // Load file content
   const char *data;
   usize len;
   bool loaded = load_source(filepath, &data, &len);
   if (!loaded)
      return false;

   // Create source from content
   source src = Source.create(data, len);
   free((void *)data); // Free the file buffer allocated by load_source (malloc'd in utils.c)
   if (src->dialect != ANVL_DIALECT_AML) {
      src->dialect = dialect_hint_from_ext(filepath);
   }
   if (src->dialect != ANVL_DIALECT_AML) {
      src->dialect = self->dialect; // set to default
   }

   // Let the builder overwrite existing source
   self->source = src;

   // Note: we don't free content here - Source.create takes ownership of the buffer
   return true;
}

ctx_builder context_get_builder(void) {
   ctx_builder b = Allocator.alloc(sizeof(struct anvl_ctx_builder_i));
   if (!b) {
      return NULL;
   }

   *b = CtxBuilder;  // copies function pointers + default dialect/source
   b->source = NULL; // ensure source is NULL initially

   return b;
}

static anvl_dialect context_dialect(context self) {
   if (!self || !self->source) {
      return ANVL_DIALECT_ASL; // default
   }
   return Source.dialect(self->source);
}

static usize context_statement_count(context self) {
   if (!self)
      return 0;
   return self->stmt_list.count;
}

static statement context_get_statement(context self, usize index) {
   if (!self || index >= self->stmt_list.count)
      return NULL;
   return self->stmt_list.statements[index];
}

static bool context_add_statement(context self, statement stmt) {
   if (!self || !stmt)
      return false;

   if (self->stmt_list.count >= self->stmt_list.capacity) {
      usize new_capacity = self->stmt_list.capacity == 0 ? 8 : self->stmt_list.capacity * 2;
      statement *new_statements = self->arena->alloc(self->arena, sizeof(statement) * new_capacity);
      if (!new_statements)
         return false;
      if (self->stmt_list.statements)
         memcpy(new_statements, self->stmt_list.statements, sizeof(statement) * self->stmt_list.count);
      self->stmt_list.statements = new_statements;
      self->stmt_list.capacity = new_capacity;
   }
   self->stmt_list.statements[self->stmt_list.count++] = stmt;
   return true;
}

static usize context_attribute_count(context self) {
   if (!self)
      return 0;
   return self->attr_list.count;
}

static attribute context_get_attribute(context self, usize index) {
   if (!self || index >= self->attr_list.count)
      return NULL;
   return self->attr_list.attributes[index];
}

static bool context_add_attribute(context self, attribute attr) {
   if (!self || !attr)
      return false;

   if (self->attr_list.count >= self->attr_list.capacity) {
      usize new_capacity = self->attr_list.capacity == 0 ? 8 : self->attr_list.capacity * 2;
      attribute *new_attributes = self->arena->alloc(self->arena, sizeof(attribute) * new_capacity);
      if (!new_attributes)
         return false;
      if (self->attr_list.attributes)
         memcpy(new_attributes, self->attr_list.attributes, sizeof(attribute) * self->attr_list.count);
      self->attr_list.attributes = new_attributes;
      self->attr_list.capacity = new_capacity;
   }
   self->attr_list.attributes[self->attr_list.count++] = attr;
   return true;
}

// dispose_value_recursive: values live in the context arena; kept as a no-op
// for any residual callers; reclaimed wholesale by Allocator.Arena.dispose(ctx->arena).
__attribute__((unused)) static void dispose_value_recursive(value v) {
   (void)v;
}

static void context_dispose(context self) {
   if (self) {
      // Drain any lazily-built name index maps before the arena is released.
      // Maps are malloc-backed (not arena) so they must be freed explicitly.
      // Source lives outside the arena (loaded independently), dispose it first.
      if (self->source)
         Source.dispose(self->source);
      // Release the parse resource scope: reclaims the entire parse tree in one shot.
      // Resource scopes are never in the R7 chain — no LIFO constraint, no BUG-001 risk.
      // Null immediately after release: slot is freed and may be recycled.
      if (self->arena) {
         Allocator.release((sc_ctrl_base_s *)self->arena);
         self->arena = NULL;
      }
      // The context struct itself was allocated from the reclaiming allocator.
      Allocator.dispose(self);
   }
}

static bool context_parse(context self) {
   if (!self || !self->source) {
      anvl_error_set(ANVL_ERR_INVALID_ARGUMENT, "Context or source is NULL", 0, 0, __FILE__);
      return false;
   }
   return anvl_parse(self);
}

static stmt_builder context_begin_statement(context self, anvl_stmt_type type) {
   (void)self;
   (void)type;
   return NULL; // Builders disabled - direct parser only
}

static void context_end_statement(context self, stmt_builder bldr, anvl_stmt_type type) {
   (void)self;
   (void)bldr;
   (void)type;
}

static attr_builder context_begin_attribute(context self) {
   (void)self;
   return NULL; // Builders disabled - direct parser only
}

static void context_end_attribute(context self, attr_builder bldr) {
   (void)self;
   (void)bldr;
}

// STUB: context_begin_value - Builders disabled (direct parser only)
static value_builder context_begin_value(context self, anvl_value_type type) {
   (void)self;
   (void)type;
   return NULL; // Builders disabled - direct parser only
}

// STUB: context_end_value - Builders disabled (direct parser only)
static void context_end_value(context self, value_builder bldr) {
   (void)self;
   (void)bldr;
}

/* ----------------------------------------------------------------- */
/* E3 — Query path primitives                                         */
/* ----------------------------------------------------------------- */

static statement context_get_statement_by_name(context self, const char *name, usize len) {
   if (!self || !name || len == 0)
      return NULL;

   /* Linear scan — O(n). Replace with hash index when sigma.collections available. */
   const char *raw = Source.data(self->source);
   for (usize i = 0; i < self->stmt_list.count; i++) {
      statement s = self->stmt_list.statements[i];
      if (s && s->meta[STMT_META_IDENT_LEN] == len && memcmp(raw + s->meta[STMT_META_IDENT_POS], name, len) == 0)
         return s;
   }
   return NULL;
}

static usize context_field_count(context self, statement stmt) {
   if (!self || !stmt || !stmt->value_meta)
      return 0;
   if (stmt->value_meta->type != ANVL_VALUE_OBJECT)
      return 0;
   return stmt->value_meta->data.object.field_count;
}

static field context_get_field(context self, statement stmt, usize index) {
   if (!self || !stmt || !stmt->value_meta)
      return NULL;
   if (stmt->value_meta->type != ANVL_VALUE_OBJECT)
      return NULL;
   usize start = stmt->value_meta->data.object.field_start;
   usize count = stmt->value_meta->data.object.field_count;
   if (index >= count)
      return NULL;
   return self->field_list.fields[start + index];
}

static field context_get_field_by_name(context self, statement stmt, const char *name, usize len) {
   if (!self || !stmt || !stmt->value_meta || !name || len == 0)
      return NULL;
   if (stmt->value_meta->type != ANVL_VALUE_OBJECT)
      return NULL;

   struct anvl_value_meta *vm = stmt->value_meta;

   /* Linear scan — O(n). Replace with hash index when sigma.collections available. */
   const char *raw = Source.data(self->source);
   usize start = vm->data.object.field_start;
   usize count = vm->data.object.field_count;
   for (usize i = 0; i < count; i++) {
      field f = self->field_list.fields[start + i];
      if (f && f->key_len == len && memcmp(raw + f->key_pos, name, len) == 0)
         return f;
   }
   return NULL;
}

static usize context_element_count(context self, statement stmt) {
   if (!self || !stmt || !stmt->value_meta)
      return 0;
   anvl_value_type t = stmt->value_meta->type;
   if (t != ANVL_VALUE_ARRAY && t != ANVL_VALUE_TUPLE)
      return 0;
   return stmt->value_meta->data.collection.element_count;
}

static struct anvl_element_meta *context_get_element(context self, statement stmt, usize index) {
   if (!self || !stmt || !stmt->value_meta)
      return NULL;
   anvl_value_type t = stmt->value_meta->type;
   if (t != ANVL_VALUE_ARRAY && t != ANVL_VALUE_TUPLE)
      return NULL;
   if (index >= stmt->value_meta->data.collection.element_count)
      return NULL;
   if (!stmt->value_meta->data.collection.elements)
      return NULL;
   return &stmt->value_meta->data.collection.elements[index];
}

static value context_get_element_value(context self, statement stmt, usize index) {
   struct anvl_element_meta *em = context_get_element(self, stmt, index);
   if (!em)
      return NULL;
   return em->child;
}

/* ================================================================
 * Value-level collection traversal (E3 extension)
 * Primitives for traversing arrays/tuples/objects nested in field values
 * ================================================================ */

static usize context_value_element_count(context self, value val) {
   if (!self || !val)
      return 0;
   if (val->type != ANVL_VALUE_ARRAY && val->type != ANVL_VALUE_TUPLE)
      return 0;
   return val->data.collection.element_count;
}

static struct anvl_element_meta *context_get_value_element(context self, value val, usize index) {
   if (!self || !val)
      return NULL;
   if (val->type != ANVL_VALUE_ARRAY && val->type != ANVL_VALUE_TUPLE)
      return NULL;
   if (index >= val->data.collection.element_count)
      return NULL;
   // For field values, element metadata is in _elem_types_temp
   struct anvl_element_meta *elements = (struct anvl_element_meta *)val->data.collection._elem_types_temp;
   if (!elements)
      return NULL;
   return &elements[index];
}

static value context_get_value_element_value(context self, value val, usize index) {
   struct anvl_element_meta *em = context_get_value_element(self, val, index);
   if (!em)
      return NULL;
   return em->child;
}

static usize context_value_field_count(context self, value val) {
   if (!self || !val)
      return 0;
   if (val->type != ANVL_VALUE_OBJECT)
      return 0;
   return val->data.object.field_count;
}

static field context_get_value_field(context self, value val, usize index) {
   if (!self || !val)
      return NULL;
   if (val->type != ANVL_VALUE_OBJECT)
      return NULL;
   if (index >= val->data.object.field_count)
      return NULL;
   // Access context field_list directly
   usize field_idx = val->data.object.field_start + index;
   if (field_idx >= self->field_list.count)
      return NULL;
   return self->field_list.fields[field_idx];
}

static field context_get_value_field_by_name(context self, value val, const char *name, usize len) {
   if (!self || !val || !name)
      return NULL;
   if (val->type != ANVL_VALUE_OBJECT)
      return NULL;

   // Linear scan (no lazy map caching in v0.6.0)
   usize start = val->data.object.field_start;
   usize count = val->data.object.field_count;

   const char *raw = Source.data(self->source);
   for (usize i = 0; i < count; i++) {
      field f = self->field_list.fields[start + i];
      if (f->key_len == len && strncmp(raw + f->key_pos, name, len) == 0)
         return f;
   }
   return NULL;
}
static usize context_stmt_attr_count(context self, statement stmt) {
   if (!self || !stmt)
      return 0;
   return stmt->meta[STMT_META_ATTR_IDX];
}

static struct anvl_attr_meta *context_get_stmt_attr(context self, statement stmt, usize index) {
   if (!self || !stmt || !stmt->attr_meta)
      return NULL;
   if (index >= stmt->meta[STMT_META_ATTR_IDX])
      return NULL;
   return &stmt->attr_meta[index];
}

static struct anvl_attr_meta *context_get_stmt_attr_by_name(context self, statement stmt,
                                                            const char *name, usize len) {
   if (!self || !stmt || !stmt->attr_meta || !name || len == 0)
      return NULL;
   const char *raw = Source.data(self->source);
   usize count = stmt->meta[STMT_META_ATTR_IDX];
   for (usize i = 0; i < count; i++) {
      struct anvl_attr_meta *a = &stmt->attr_meta[i];
      if (a->len == len && memcmp(raw + a->pos, name, len) == 0)
         return a;
   }
   return NULL;
}

static usize context_field_attr_count(context self, field f) {
   (void)self;
   if (!f)
      return 0;
   return f->attrib_count;
}

static attribute context_get_field_attr(context self, field f, usize index) {
   if (!self || !f)
      return NULL;
   if (index >= f->attrib_count)
      return NULL;
   return self->attr_list.attributes[f->attrib_start + index];
}

static attribute context_get_field_attr_by_name(context self, field f,
                                                const char *name, usize len) {
   if (!self || !f || !name || len == 0)
      return NULL;
   const char *raw = Source.data(self->source);
   for (usize i = 0; i < f->attrib_count; i++) {
      attribute a = self->attr_list.attributes[f->attrib_start + i];
      if (!a)
         continue;
      if (a->key_len == len && memcmp(raw + a->key_pos, name, len) == 0)
         return a;
   }
   return NULL;
}

const struct anvl_context_i Context = {
    .get_builder = context_get_builder,
    .dialect = context_dialect,
    .statement_count = context_statement_count,
    .get_statement = context_get_statement,
    .add_statement = context_add_statement,
    .attribute_count = context_attribute_count,
    .get_attribute = context_get_attribute,
    .add_attribute = context_add_attribute,
    .dispose = context_dispose,
    .parse = context_parse,
    .begin_statement = context_begin_statement,
    .end_statement = context_end_statement,
    .begin_value = context_begin_value,
    .end_value = context_end_value,
    .begin_attribute = context_begin_attribute,
    .end_attribute = context_end_attribute,
    .field_count = context_field_count,
    .get_field = context_get_field,
    .get_field_by_name = context_get_field_by_name,
    .element_count = context_element_count,
    .get_element = context_get_element,
    .get_element_value = context_get_element_value,
    .value_element_count = context_value_element_count,
    .get_value_element = context_get_value_element,
    .get_value_element_value = context_get_value_element_value,
    .value_field_count = context_value_field_count,
    .get_value_field = context_get_value_field,
    .get_value_field_by_name = context_get_value_field_by_name,
    .get_statement_by_name = context_get_statement_by_name,
    .stmt_attr_count = context_stmt_attr_count,
    .get_stmt_attr = context_get_stmt_attr,
    .get_stmt_attr_by_name = context_get_stmt_attr_by_name,
    .field_attr_count = context_field_attr_count,
    .get_field_attr = context_get_field_attr,
    .get_field_attr_by_name = context_get_field_attr_by_name,
};

const struct anvl_statement_i Statement = {
    .identifier = statement_identifier,
    .base = statement_base,
    .type = statement_type,
    .value_type = statement_value_type,
    .length = statement_length,
};

static void builder_set_dialect(ctx_builder self, anvl_dialect dialect) {
   self->dialect = dialect;
}
static void builder_set_source(ctx_builder self, const char *data) {
   // dispose existing source if any
   if (self->source) {
      Source.dispose(self->source);
   }
   // create new source
   self->source = Source.create(data, 0);
}
static context builder_build(ctx_builder self) {
   context ctx = Allocator.alloc(sizeof(struct anvl_context_t));
   if (ctx)
      memset(ctx, 0, sizeof(struct anvl_context_t));
   if (!ctx) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate context", 0, 0, __FILE__);
      return NULL;
   }
   ctx->source = self->source;
   if (!ctx->source) {
      // No source available - set error and fail gracefully
      if (!anvl_error_is_set()) {
         anvl_error_set(ANVL_ERR_BUILDER_NO_SOURCE,
                        anvl_error_code_message(ANVL_ERR_BUILDER_NO_SOURCE),
                        0, 0, NULL);
      }
      Allocator.dispose(ctx); // Clean up the context we just allocated
      return NULL;
   }

   // Clean up builder
   self->source = NULL; // Transfer ownership to context
   self->dispose(self); // Dispose builder after transferring source to context

   // Acquire a resource scope sized to the source: pure bump, no R7 coupling, no LIFO
   // constraint.  16× source length accounts for the parse tree structs AND the doubling
   // overhead in ci_ensure_*_capacity (old arrays are left in the bump by design and
   // reclaimed together at Context.dispose — worst case 2× the live set).
   // 64KB floor handles small/typical files comfortably.
   // context_dispose() MUST be called to release.
   usize src_len = Source.length(ctx->source);
   usize slab_sz = src_len * 16 < 64 * 1024 ? 64 * 1024 : src_len * 16;
   ctx->arena = Allocator.create_bump(slab_sz);
   if (!ctx->arena) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to acquire parse resource scope", 0, 0, __FILE__);
      Source.dispose(ctx->source);
      Allocator.dispose(ctx);
      return NULL;
   }

   return ctx;
}
static void builder_dispose(ctx_builder self) {
   if (self && self->source) {
      Source.dispose(self->source);
      self->source = NULL;
   }

   Allocator.dispose(self);
}

// Statement interface implementations
static void statement_identifier(statement self, source src, char *out_identifier) {
   if (!self || !src || !out_identifier || self->meta[STMT_META_IDENT_LEN] == 0) {
      if (out_identifier)
         out_identifier[0] = '\0';
      return;
   }
   Source.substring(src, self->meta[STMT_META_IDENT_POS], self->meta[STMT_META_IDENT_LEN], out_identifier);
}
static const char *statement_base(statement self, source src) {
   if (!self || !src)
      return NULL;

   // Get base metadata index from statement
   usize base_idx = self->meta[STMT_META_BASE_IDX];
   if (base_idx == 0)
      return NULL; // No base inheritance

   // TODO: Implement base_meta array in context to retrieve actual position/length
   // Strategy: Context needs to store array of all base_meta pointers (similar to attr_list)
   // Currently base_meta is self-contained in statement, but for retrieval we need:
   //   1. Array in context to store all base_meta pointers
   //   2. Index in statement to locate in that array
   // This would enable global queries about inheritance across all statements
   // For now, base_meta is accessed directly from statement via stmt->base_meta
   return NULL;
}
static anvl_stmt_type statement_type(statement self) {
   if (!self)
      return 0;
   return (anvl_stmt_type)self->meta[STMT_META_TYPE];
}
static anvl_value_type statement_value_type(statement self) {
   if (!self)
      return ANVL_VALUE_SCALAR;
   // Value type information is now accessed via value metadata at meta[STMT_META_VALUE_IDX]
   // For now, return scalar as default
   return ANVL_VALUE_SCALAR;
}
static usize statement_length(statement self) {
   if (!self)
      return 0;
   // Statement length is now stored in value_meta->len (total span from identifier to end of value)
   // If value_meta exists, use its length; otherwise return 0
   if (self->value_meta)
      return self->value_meta->len + self->meta[STMT_META_IDENT_LEN];
   return 0;
}

struct anvl_ctx_builder_i CtxBuilder = {
    .dialect = ANVL_DIALECT_ASL, // leave builder always set to ASL by default
    .source = NULL,
    .set_dialect = builder_set_dialect,
    .set_source = builder_set_source,
    .load_file = builder_load_file,
    .build = builder_build,
    .dispose = builder_dispose,
};