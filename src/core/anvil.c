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
#include "utils.h"
#include <sigcore/memory.h>
#include <stdlib.h>
#include <string.h>

/* Root structure */
struct anvl_root_t {
   int dummy;
};

/* Detect dialect from source content (shebang) */
/* Stub implementations for Iteration 1 */

static root anvil_load(const char *filepath) {
   (void)filepath;
   return Memory.alloc(sizeof(struct anvl_root_t), true); // minimal stub
}

static root anvil_read(const char *source, usize len) {
   (void)source;
   (void)len;
   return Memory.alloc(sizeof(struct anvl_root_t), true); // minimal
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
   Memory.dispose(r);
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

static bool builder_load_file(struct anvl_ctx_builder_i *self, const char *filepath) {
   // Load file content
   const char *data;
   usize len;
   bool loaded = load_source(filepath, &data, &len);
   if (!loaded)
      return false;

   // Create source from content
   source src = Source.create(data, len);
   free((void *)data); // Free the file buffer allocated by load_source
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

static anvl_ctx_builder_i *context_get_builder(void) {
   return (anvl_ctx_builder_i *)&CtxBuilder; // return the global builder interface
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

   // Add to statement list
   if (self->stmt_list.count >= self->stmt_list.capacity) {
      usize new_capacity = self->stmt_list.capacity == 0 ? 8 : self->stmt_list.capacity * 2;
      statement *new_statements = Memory.alloc(sizeof(statement) * new_capacity, false);
      if (self->stmt_list.statements) {
         memcpy(new_statements, self->stmt_list.statements, sizeof(statement) * self->stmt_list.count);
         Memory.dispose(self->stmt_list.statements);
      }
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

   // Add to attribute list
   if (self->attr_list.count >= self->attr_list.capacity) {
      usize new_capacity = self->attr_list.capacity == 0 ? 8 : self->attr_list.capacity * 2;
      attribute *new_attributes = Memory.alloc(sizeof(attribute) * new_capacity, false);
      if (self->attr_list.attributes) {
         memcpy(new_attributes, self->attr_list.attributes, sizeof(attribute) * self->attr_list.count);
         Memory.dispose(self->attr_list.attributes);
      }
      self->attr_list.attributes = new_attributes;
      self->attr_list.capacity = new_capacity;
   }
   self->attr_list.attributes[self->attr_list.count++] = attr;
   return true;
}

static usize context_value_count(context self) {
   if (!self)
      return 0;
   return self->value_list.count;
}

static value context_get_value(context self, usize index) {
   if (!self || index >= self->value_list.count)
      return NULL;
   return self->value_list.values[index];
}

static bool context_add_value(context self, value val) {
   if (!self || !val)
      return false;

   // Add to value list
   if (self->value_list.count >= self->value_list.capacity) {
      usize new_capacity = self->value_list.capacity == 0 ? 8 : self->value_list.capacity * 2;
      value *new_values = Memory.alloc(sizeof(value) * new_capacity, false);
      if (self->value_list.values) {
         memcpy(new_values, self->value_list.values, sizeof(value) * self->value_list.count);
         Memory.dispose(self->value_list.values);
      }
      self->value_list.values = new_values;
      self->value_list.capacity = new_capacity;
   }
   self->value_list.values[self->value_list.count++] = val;
   return true;
}

static usize context_field_count(context self) {
   if (!self)
      return 0;
   return self->field_list.count;
}

static field context_get_field(context self, usize index) {
   if (!self || index >= self->field_list.count)
      return NULL;
   return self->field_list.fields[index];
}

static bool context_add_field(context self, field fld) {
   if (!self || !fld)
      return false;

   // Add to field list
   if (self->field_list.count >= self->field_list.capacity) {
      usize new_capacity = self->field_list.capacity == 0 ? 8 : self->field_list.capacity * 2;
      field *new_fields = Memory.alloc(sizeof(field) * new_capacity, false);
      if (self->field_list.fields) {
         memcpy(new_fields, self->field_list.fields, sizeof(field) * self->field_list.count);
         Memory.dispose(self->field_list.fields);
      }
      self->field_list.fields = new_fields;
      self->field_list.capacity = new_capacity;
   }
   self->field_list.fields[self->field_list.count++] = fld;
   return true;
}

static void context_dispose(context self) {
   if (self) {
      if (self->source) {
         Source.dispose(self->source);
      }
      // Dispose statements
      for (usize i = 0; i < self->stmt_list.count; i++) {
         Memory.dispose(self->stmt_list.statements[i]);
      }
      if (self->stmt_list.statements) {
         Memory.dispose(self->stmt_list.statements);
      }
      // Dispose attributes
      for (usize i = 0; i < self->attr_list.count; i++) {
         Memory.dispose(self->attr_list.attributes[i]);
      }
      if (self->attr_list.attributes) {
         Memory.dispose(self->attr_list.attributes);
      }
      // Dispose values
      for (usize i = 0; i < self->value_list.count; i++) {
         Memory.dispose(self->value_list.values[i]);
      }
      if (self->value_list.values) {
         Memory.dispose(self->value_list.values);
      }
      // Dispose fields
      for (usize i = 0; i < self->field_list.count; i++) {
         Memory.dispose(self->field_list.fields[i]);
      }
      if (self->field_list.fields) {
         Memory.dispose(self->field_list.fields);
      }
      Memory.dispose(self);
   }
}

static bool context_parse(context self) {
   if (!self || !self->source) {
      anvl_error_set(ANVL_ERR_INVALID_ARGUMENT, "Context or source is NULL", 0, 0, __FILE__);
      return false;
   }
   return anvl_parse(self);
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
    .value_count = context_value_count,
    .get_value = context_get_value,
    .add_value = context_add_value,
    .field_count = context_field_count,
    .get_field = context_get_field,
    .add_field = context_add_field,
    .dispose = context_dispose,
    .parse = context_parse,
};

/* Builder stubs */

static void builder_set_dialect(struct anvl_ctx_builder_i *self, anvl_dialect dialect) {
   self->dialect = dialect;
}

static void builder_set_source(struct anvl_ctx_builder_i *self, const char *data, usize len) {
   // dispose existing source if any
   if (self->source) {
      Source.dispose(self->source);
   }
   // create new source
   self->source = Source.create(data, len);
}

static context builder_build(struct anvl_ctx_builder_i *self) {
   context ctx = Memory.alloc(sizeof(struct anvl_context_t), true);
   ctx->source = self->source;
   if (!ctx->source) {
      // No source available - set error and fail gracefully
      anvl_error_set(ANVL_ERR_BUILDER_NO_SOURCE,
                     anvl_error_code_message(ANVL_ERR_BUILDER_NO_SOURCE),
                     0, 0, NULL);
      Memory.dispose(ctx); // Clean up the context we just allocated
      return NULL;
   }
   // Transfer ownership - clear builder's reference
   self->source = NULL;
   self->dialect = ANVL_DIALECT_ASL; // reset to default

   return ctx;
}

static void builder_dispose(struct anvl_ctx_builder_i *self) {
   if (self->source) {
      Memory.dispose(self->source);
   }
   self->source = NULL;
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