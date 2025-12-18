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
 * context.c - Context implementation for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-16
 * File: src/core/context.c
 * ------------------------------------------------------------------
 */

#include "context.h"
#include "parser.h"
#include "utils.h"
#include <sigcore/memory.h>
#include <sigcore/strings.h>
#include <sigtest/sigtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static stmt_builder context_begin_statement(context self, anvl_stmt_type type);
static void context_end_statement(context self, stmt_builder bldr, anvl_stmt_type type);
static attr_builder context_begin_attribute(context self);
static void context_end_attribute(context self, attr_builder bldr);

// Statement interface functions
static const char *statement_identifier(statement self, source src);
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
      usize new_capacity = self->stmt_list.capacity == 0 ? 1 : self->stmt_list.capacity * 2;
      statement *new_statements = Memory.alloc(sizeof(statement) * new_capacity, false);
      if (!new_statements) {
         return false;
      }
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

static void value_dispose(value val) {
   if (!val)
      return;
   if (val->type == ANVL_VALUE_OBJECT && val->data.object.fields) {
      for (usize j = 0; j < val->data.object.field_count; j++) {
         field f = val->data.object.fields[j];
         if (f) {
            if (f->attributes) {
               Memory.dispose(f->attributes);
            }
            if (f->val) {
               value_dispose(f->val);
            }
            Memory.dispose(f);
         }
      }
      Memory.dispose(val->data.object.fields);
   } else if ((val->type == ANVL_VALUE_ARRAY || val->type == ANVL_VALUE_TUPLE) && val->data.collection.elements) {
      for (usize j = 0; j < val->data.collection.element_count; j++) {
         value_dispose(val->data.collection.elements[j]);
      }
      Memory.dispose(val->data.collection.elements);
   }
   Memory.dispose(val);
}

static void context_dispose(context self) {
   if (self) {
      if (self->source) {
         Source.dispose(self->source);
      }
      // Dispose statements and their owned data
      for (usize i = 0; i < self->stmt_list.count; i++) {
         statement stmt = self->stmt_list.statements[i];
         if (stmt) {
            // Dispose statement attributes
            if (stmt->attributes) {
               Memory.dispose(stmt->attributes);
            }
            // Dispose statement value recursively
            if (stmt->val) {
               value_dispose(stmt->val);
            }
            Memory.dispose(stmt);
         }
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
      // Note: value_list and field_list are not used in nested ownership model
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

static stmt_builder context_begin_statement(context self, anvl_stmt_type type) {
   (void)type; // unused for now
   if (!self) {
      anvl_error_set(ANVL_ERR_INVALID_ARGUMENT, "Context is NULL", 0, 0, __FILE__);
      return NULL;
   }

   // Allocate statement
   statement stmt = Memory.alloc(sizeof(struct anvl_statement), true);
   if (!stmt) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate statement", 0, 0, __FILE__);
      return NULL;
   }

   // Allocate builder
   stmt_builder bldr = Memory.alloc(sizeof(struct anvl_stmt_builder_t), true);
   if (!bldr) {
      Memory.dispose(stmt);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate statement builder", 0, 0, __FILE__);
      return NULL;
   }

   // Initialize builder
   bldr->stmt = stmt;
   bldr->ctx = self;
   // Initialize stack
   bldr->value_stack.depth = 0;

   bldr->identifier = stmt_builder_identifier;
   bldr->base = stmt_builder_base;
   bldr->begin_attributes = stmt_builder_begin_attributes;
   bldr->attribute_key = stmt_builder_attribute_key;
   bldr->attribute_value = stmt_builder_attribute_value;
   bldr->end_attributes = stmt_builder_end_attributes;
   bldr->begin_value = stmt_builder_begin_value;
   bldr->end_value = stmt_builder_end_value;
   bldr->value_scalar = stmt_builder_value_scalar;

   return bldr;
}

static void context_end_statement(context self, stmt_builder bldr, anvl_stmt_type type) {
   if (!self || !bldr) {
      anvl_error_set(ANVL_ERR_INVALID_ARGUMENT, "Context or builder is NULL", 0, 0, __FILE__);
      return;
   }

   // Set the statement type
   bldr->stmt->meta[STMT_META_TYPE] = type;

   // Add to context
   if (!context_add_statement(self, bldr->stmt)) {
      // Error already set
      Memory.dispose(bldr->stmt);
   }

   // Free builder
   Memory.dispose(bldr);
}

static attr_builder context_begin_attribute(context self) {
   if (!self) {
      anvl_error_set(ANVL_ERR_INVALID_ARGUMENT, "Context is NULL", 0, 0, __FILE__);
      return NULL;
   }

   // Allocate builder
   attr_builder bldr = Memory.alloc(sizeof(struct anvl_attr_builder_t), true);
   if (!bldr) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate attribute builder", 0, 0, __FILE__);
      return NULL;
   }

   // Initialize builder
   bldr->ctx = self;
   bldr->key_pos = 0;
   bldr->key_len = 0;
   bldr->value_pos = 0;
   bldr->value_len = 0;
   bldr->has_value = false;

   bldr->key = attr_builder_key;
   bldr->value = attr_builder_value;
   bldr->add = attr_builder_add;

   return bldr;
}

static void context_end_attribute(context self, attr_builder bldr) {
   if (!self || !bldr) {
      anvl_error_set(ANVL_ERR_INVALID_ARGUMENT, "Context or builder is NULL", 0, 0, __FILE__);
      return;
   }

   // Free builder
   Memory.dispose(bldr);
}

static value_builder context_begin_value(context self, anvl_value_type type) {
   if (!self) {
      anvl_error_set(ANVL_ERR_INVALID_ARGUMENT, "Context is NULL", 0, 0, __FILE__);
      return NULL;
   }

   // Allocate value
   value val = Memory.alloc(sizeof(struct anvl_value), true);
   if (!val) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate value", 0, 0, __FILE__);
      return NULL;
   }
   val->type = type;

   // Allocate builder
   value_builder bldr = Memory.alloc(sizeof(struct anvl_value_builder_t), true);
   if (!bldr) {
      Memory.dispose(val);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate value builder", 0, 0, __FILE__);
      return NULL;
   }

   // Initialize builder
   bldr->val = val;
   bldr->ctx = self;
   bldr->value_stack.depth = 0;

   bldr->begin_object = value_builder_begin_object;
   bldr->field = value_builder_field;
   bldr->field_value = value_builder_field_value;
   bldr->end_object = value_builder_end_object;
   bldr->begin_array = value_builder_begin_array;
   bldr->element = value_builder_element;
   bldr->end_array = value_builder_end_array;
   bldr->scalar = value_builder_scalar;

   return bldr;
}

static void context_end_value(context self, value_builder bldr) {
   if (!self || !bldr) {
      anvl_error_set(ANVL_ERR_INVALID_ARGUMENT, "Context or builder is NULL", 0, 0, __FILE__);
      return;
   }

   // Add value to context's value_list
   if (self->value_list.count >= self->value_list.capacity) {
      usize new_capacity = self->value_list.capacity == 0 ? 8 : self->value_list.capacity * 2;
      value *new_values = Memory.alloc(sizeof(value) * new_capacity, false);
      if (!new_values) {
         anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to reallocate value list", 0, 0, __FILE__);
         Memory.dispose(bldr->val);
         Memory.dispose(bldr);
         return;
      }
      if (self->value_list.values) {
         memcpy(new_values, self->value_list.values, sizeof(value) * self->value_list.count);
         Memory.dispose(self->value_list.values);
      }
      self->value_list.values = new_values;
      self->value_list.capacity = new_capacity;
   }
   self->value_list.values[self->value_list.count++] = bldr->val;

   // Free builder
   Memory.dispose(bldr);
}

// Builder method implementations
void stmt_builder_identifier(stmt_builder self, usize pos, usize len) {
   self->stmt->meta[STMT_META_IDENT_POS] = pos;
   self->stmt->meta[STMT_META_IDENT_LEN] = len;
}

void stmt_builder_base(stmt_builder self, usize pos, usize len) {
   self->stmt->base[0] = pos;
   self->stmt->base[1] = len;
}

void stmt_builder_begin_attributes(stmt_builder self) {
   self->stmt->attrib_count = 0;
   self->stmt->attributes = NULL;
}

void stmt_builder_attribute_key(stmt_builder self, usize pos, usize len) {
   (void)self;
   (void)pos;
   (void)len; // For now, assume attributes are built by parser and set directly
}

void stmt_builder_attribute_value(stmt_builder self, usize pos, usize len) {
   (void)self;
   (void)pos;
   (void)len; // Similar
}

void stmt_builder_end_attributes(stmt_builder self) {
   (void)self; // Finalize
}

value_builder stmt_builder_begin_value(stmt_builder self, anvl_value_type type) {
   self->stmt->meta[STMT_META_VALUE_TYPE] = type;

   // Allocate the statement's value
   self->stmt->val = Memory.alloc(sizeof(struct anvl_value), true);
   if (!self->stmt->val) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate statement value", 0, 0, __FILE__);
      return NULL;
   }
   self->stmt->val->type = type;

   // Allocate builder
   value_builder bldr = Memory.alloc(sizeof(struct anvl_value_builder_t), true);
   if (!bldr) {
      Memory.dispose(self->stmt->val);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate value builder", 0, 0, __FILE__);
      return NULL;
   }

   // Initialize builder - val points to statement's value
   bldr->val = self->stmt->val;
   bldr->ctx = self->ctx;
   bldr->value_stack.depth = 0;
   bldr->temp.fields = NULL;
   bldr->temp.field_count = 0;
   bldr->temp.field_capacity = 0;
   bldr->temp.values = NULL;
   bldr->temp.value_count = 0;
   bldr->temp.value_capacity = 0;

   bldr->begin_object = value_builder_begin_object;
   bldr->field = value_builder_field;
   bldr->field_value = value_builder_field_value;
   bldr->end_object = value_builder_end_object;
   bldr->begin_array = value_builder_begin_array;
   bldr->element = value_builder_element;
   bldr->end_array = value_builder_end_array;
   bldr->scalar = value_builder_scalar;

   return bldr;
}
void stmt_builder_end_value(stmt_builder self, value_builder vbldr) {
   // Copy temp data to statement's value union
   anvl_value_type type = (anvl_value_type)self->stmt->meta[STMT_META_VALUE_TYPE];
   if (type == ANVL_VALUE_OBJECT) {
      self->stmt->val->data.object.field_count = vbldr->temp.field_count;
      self->stmt->val->data.object.fields = vbldr->temp.fields;
   } else if (type == ANVL_VALUE_ARRAY || type == ANVL_VALUE_TUPLE) {
      self->stmt->val->data.collection.element_count = vbldr->temp.value_count;
      self->stmt->val->data.collection.elements = vbldr->temp.values;
   }
   // Dispose the builder
   Memory.dispose(vbldr);
}

void stmt_builder_value_scalar(stmt_builder self, usize pos, usize len, anvl_value_type type) {
   if (self->value_stack.depth == 0) {
      // Top level scalar - allocate value if not already allocated
      if (!self->stmt->val) {
         self->stmt->val = Memory.alloc(sizeof(struct anvl_value), true);
         if (!self->stmt->val) {
            anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate statement value", 0, 0, __FILE__);
            return;
         }
         self->stmt->val->type = type;
      }
      self->stmt->val->data.scalar.pos = pos;
      self->stmt->val->data.scalar.len = len;
   }
   // Set the meta type
   self->stmt->meta[STMT_META_VALUE_TYPE] = type;
}

// Attribute Builder method implementations
void attr_builder_key(attr_builder self, usize pos, usize len) {
   self->key_pos = pos;
   self->key_len = len;
}

void attr_builder_value(attr_builder self, usize pos, usize len) {
   self->value_pos = pos;
   self->value_len = len;
   self->has_value = true;
}

void attr_builder_add(attr_builder self) {
   // Create attribute
   attribute attr = Memory.alloc(sizeof(struct anvl_attribute), true);
   if (!attr) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate attribute", 0, 0, __FILE__);
      return;
   }

   attr->key_pos = self->key_pos;
   attr->key_len = self->key_len;
   if (self->has_value) {
      attr->value_pos = self->value_pos;
      attr->value_len = self->value_len;
   } else {
      attr->value_pos = 0;
      attr->value_len = 0;
   }

   // Add to context
   if (!context_add_attribute(self->ctx, attr)) {
      Memory.dispose(attr);
      return;
   }

   // Reset for next attribute
   self->key_pos = 0;
   self->key_len = 0;
   self->value_pos = 0;
   self->value_len = 0;
   self->has_value = false;
}

// Value Builder method implementations
void value_builder_begin_object(value_builder self) {
   self->value_stack.depth++;
   // Initialize temp fields if top level
   if (self->value_stack.depth == 1) {
      self->temp.fields = NULL;
      self->temp.field_count = 0;
      self->temp.field_capacity = 0;
   }
}

void value_builder_field(value_builder self, usize key_pos, usize key_len, usize attrib_start, usize attrib_count) {
   // Ensure capacity
   if (self->temp.field_count >= self->temp.field_capacity) {
      self->temp.field_capacity = self->temp.field_capacity == 0 ? 4 : self->temp.field_capacity * 2;
      self->temp.fields = Memory.realloc(self->temp.fields, sizeof(field) * self->temp.field_capacity);
   }
   // Allocate field
   field f = Memory.alloc(sizeof(struct anvl_field), true);
   f->key_pos = key_pos;
   f->key_len = key_len;
   f->attrib_count = attrib_count;
   if (attrib_count > 0) {
      f->attributes = Memory.alloc(sizeof(attribute) * attrib_count, true);
      for (usize i = 0; i < attrib_count; i++) {
         f->attributes[i] = Context.get_attribute(self->ctx, attrib_start + i);
      }
   } else {
      f->attributes = NULL;
   }
   f->val = NULL; // Will be set in field_value
   self->temp.fields[self->temp.field_count++] = f;
}

void value_builder_field_value(value_builder self, value val) {
   if (self->temp.field_count > 0) {
      field f = self->temp.fields[self->temp.field_count - 1];
      f->val = val;
   }
}

void value_builder_end_object(value_builder self) {
   self->value_stack.depth--;
   if (self->value_stack.depth == 0) {
      // Set the value data for nested object
      self->val->data.object.field_count = self->temp.field_count;
      self->val->data.object.fields = self->temp.fields;
   }
}

void value_builder_begin_array(value_builder self) {
   self->value_stack.depth++;
   // Initialize temp values if top level
   if (self->value_stack.depth == 1) {
      self->temp.values = NULL;
      self->temp.value_count = 0;
      self->temp.value_capacity = 0;
   }
}

void value_builder_element(value_builder self, value val) {
   // Ensure capacity
   if (self->temp.value_count >= self->temp.value_capacity) {
      self->temp.value_capacity = self->temp.value_capacity == 0 ? 4 : self->temp.value_capacity * 2;
      self->temp.values = Memory.realloc(self->temp.values, sizeof(value) * self->temp.value_capacity);
   }
   self->temp.values[self->temp.value_count++] = val;
}

void value_builder_end_array(value_builder self) {
   self->value_stack.depth--;
   if (self->value_stack.depth == 0) {
      // Set the value data for nested array/tuple
      self->val->data.collection.element_count = self->temp.value_count;
      self->val->data.collection.elements = self->temp.values;
   }
}

void value_builder_scalar(value_builder self, usize pos, usize len, anvl_value_type type) {
   (void)type; // Not used for scalars
   if (self->value_stack.depth == 0) {
      // Top level scalar
      self->val->data.scalar.pos = pos;
      self->val->data.scalar.len = len;
   }
}

value_builder create_temp_value_builder(context ctx) {
   value val = Memory.alloc(sizeof(struct anvl_value), true);
   if (!val) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate temp value", 0, 0, __FILE__);
      return NULL;
   }
   val->type = ANVL_VALUE_SCALAR; // default
   value_builder bldr = Memory.alloc(sizeof(struct anvl_value_builder_t), true);
   if (!bldr) {
      Memory.dispose(val);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate temp value builder", 0, 0, __FILE__);
      return NULL;
   }
   bldr->val = val;
   bldr->ctx = ctx;
   bldr->value_stack.depth = 0;
   bldr->temp.fields = NULL;
   bldr->temp.field_count = 0;
   bldr->temp.field_capacity = 0;
   bldr->temp.values = NULL;
   bldr->temp.value_count = 0;
   bldr->temp.value_capacity = 0;
   bldr->begin_object = value_builder_begin_object;
   bldr->field = value_builder_field;
   bldr->field_value = value_builder_field_value;
   bldr->end_object = value_builder_end_object;
   bldr->begin_array = value_builder_begin_array;
   bldr->element = value_builder_element;
   bldr->end_array = value_builder_end_array;
   bldr->scalar = value_builder_scalar;
   return bldr;
}

void dispose_temp_value_builder(value_builder bldr) {
   if (bldr) {
      if (bldr->temp.fields) {
         for (usize i = 0; i < bldr->temp.field_count; i++) {
            field f = bldr->temp.fields[i];
            if (f) {
               if (f->attributes)
                  Memory.dispose(f->attributes);
               // f->val is owned by parent
               Memory.dispose(f);
            }
         }
         Memory.dispose(bldr->temp.fields);
      }
      if (bldr->temp.values) {
         // values owned by parent
         Memory.dispose(bldr->temp.values);
      }
      // val is owned by parent
      Memory.dispose(bldr);
   }
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
};

const struct anvl_statement_i Statement = {
    .identifier = statement_identifier,
    .base = statement_base,
    .type = statement_type,
    .value_type = statement_value_type,
    .length = statement_length,
};

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

// Statement interface implementations
static const char *statement_identifier(statement self, source src) {
   if (!self || !src || self->meta[STMT_META_IDENT_LEN] == 0)
      return NULL;
   return Source.substring(src, self->meta[STMT_META_IDENT_POS], self->meta[STMT_META_IDENT_LEN]);
}

static const char *statement_base(statement self, source src) {
   if (!self || !src || self->base[1] == 0)
      return NULL;
   return Source.substring(src, self->base[0], self->base[1]);
}

static anvl_stmt_type statement_type(statement self) {
   if (!self)
      return 0;
   return (anvl_stmt_type)self->meta[STMT_META_TYPE];
}

static anvl_value_type statement_value_type(statement self) {
   if (!self)
      return ANVL_VALUE_SCALAR;
   return (anvl_value_type)self->meta[STMT_META_VALUE_TYPE];
}

static usize statement_length(statement self) {
   if (!self)
      return 0;
   return self->meta[STMT_META_LEN];
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

/* === Source interface implementations (moved from source.c) === */
static bool source_is_shebang(source self);
static usize source_scan_match(source self, const char *s, usize slen);
static void source_reset(source self);
static usize source_skip_whitespace_and_comments(source);
static anvl_dialect source_parse_dialect(source, anvl_dialect);
static usize source_consume(source, usize);
static char *source_substring(source, usize, usize);
static usize source_scan_whitespace(source self, usize offset);
static usize source_scan_line_comment(source self, usize offset);
static usize source_scan_block_comment(source self, usize offset);

static source source_create(const char *data, usize len) {
   if (String.length((string)data) == 0 || len == 0)
      return NULL;

   source src = Memory.alloc(sizeof(struct anvl_source_t), true);

   if (data && len > 0) {
      char *buffer = Memory.alloc(len + 1, false);
      memcpy(buffer, data, len);
      buffer[len] = '\0';
      src->buffer.bucket = buffer;
      src->buffer.end = buffer + len;
   } else {
      src->buffer.bucket = NULL;
      src->buffer.end = NULL;
   }

   src->dialect = ANVL_DIALECT_ASL;
   src->stride = sizeof(char);
   src->pos = 0;
   src->line = 1;
   src->col = 1;

   if (data && len > 0) {
      source_skip_whitespace_and_comments(src);
      if (source_is_shebang(src)) {
         src->dialect = source_parse_dialect(src, src->dialect);
         source_skip_whitespace_and_comments(src);
         if (source_is_shebang(src)) {
            anvl_error_set(ANVL_ERR_PARSER_MULTIPLE_SHEBANG, "Multiple shebangs not allowed", src->line, src->col, NULL);
            Memory.dispose(src);
            return NULL;
         }
      }
      source_skip_whitespace_and_comments(src);
   }

   return src;
}

static void source_dispose(source self) {
   if (self->buffer.bucket) {
      Memory.dispose(self->buffer.bucket);
   }
   Memory.dispose(self);
}

static anvl_dialect source_get_dialect(source self) {
   return self->dialect;
}

static usize source_position(source self) {
   return self->pos;
}
static usize source_line(source self) {
   return self->line;
}
static usize source_column(source self) {
   return self->col;
}

static bool source_is_eof(source self) {
   return self->pos >= (usize)(self->buffer.end - self->buffer.bucket);
}
static bool source_is_eof_offset(source self, usize offset) {
   return self->pos + offset >= (usize)(self->buffer.end - self->buffer.bucket);
}

static char source_peek_offset(source self, usize offset) {
   usize idx = self->pos + offset;
   usize len = (usize)(self->buffer.end - self->buffer.bucket);
   return idx < len ? ((char *)self->buffer.bucket)[idx] : '\0';
}
static char source_peek(source self) {
   return source_peek_offset(self, 0);
}

static usize source_scan_match(source self, const char *s, usize slen) {
   if (!s || slen == 0)
      return 0;
   for (usize i = 0; i < slen; i++) {
      if (source_peek_offset(self, i) != s[i])
         return 0;
   }
   return self->pos + slen <= (usize)(self->buffer.end - self->buffer.bucket) ? slen : 0;
}
static usize source_match_operator(source self, const char *op, usize oplen) {
   return source_scan_match(self, op, oplen);
}

static bool source_is_alpha(char c) {
   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static bool source_is_digit(char c) {
   return c >= '0' && c <= '9';
}
static bool source_is_hex_digit(char c) {
   return source_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static bool source_is_identifier_start(char c) {
   return source_is_alpha(c);
}
static bool source_is_identifier_part(char c) {
   return source_is_alpha(c) || source_is_digit(c);
}

static usize source_consume(source self, usize count) {
   for (usize i = 0; i < count; i++) {
      if (source_is_eof(self))
         return i;

      char c = source_peek(self);
      self->pos++;

      if (c == '\n') {
         self->line++;
         self->col = 1;
      } else {
         self->col++;
      }
   }
   return count;
}

static const char *source_data(source self) {
   return self->buffer.bucket;
}

static usize source_length(source self) {
   if (!self || !self->buffer.bucket)
      return 0;
   return (usize)(self->buffer.end - self->buffer.bucket);
}

static char *source_substring(source self, usize start, usize len) {
   if (start >= (usize)(self->buffer.end - self->buffer.bucket) || len == 0) {
      return NULL;
   }

   usize available = (usize)(self->buffer.end - self->buffer.bucket) - start;
   usize actual_len = len < available ? len : available;

   char *result = Memory.alloc(actual_len + 1, false);
   memcpy(result, self->buffer.bucket + start, actual_len);
   result[actual_len] = '\0';
   return result;
}

static usize source_scan_whitespace(source self, usize offset) {
   usize pos = offset;
   char c = source_peek_offset(self, pos);
   while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      pos++;
      c = source_peek_offset(self, pos);
   }
   return pos;
}

static usize source_scan_line_comment(source self, usize offset) {
   if (source_peek_offset(self, offset) == '/' && source_peek_offset(self, offset + 1) == '/') {
      usize pos = offset + 2;
      char c = source_peek_offset(self, pos);
      while (c != '\n' && c != '\0') {
         pos++;
         c = source_peek_offset(self, pos);
      }
      if (c == '\n') {
         pos++;
      }
      return pos;
   }
   return offset;
}

static usize source_scan_block_comment(source self, usize offset) {
   if (source_peek_offset(self, offset) == '/' && source_peek_offset(self, offset + 1) == '*') {
      usize pos = offset + 2;
      char c = source_peek_offset(self, pos);
      while (!(c == '*' && source_peek_offset(self, pos + 1) == '/') && c != '\0') {
         pos++;
         c = source_peek_offset(self, pos);
      }
      if (c != '\0') {
         pos += 2;
      }
      return pos;
   }
   return offset;
}

static usize source_skip_whitespace_and_comments(source self) {
   usize pos = 0;
   bool changed = true;

   while (changed && !source_is_eof_offset(self, pos)) {
      usize old_pos = pos;
      pos = source_scan_whitespace(self, pos);
      pos = source_scan_line_comment(self, pos);
      pos = source_scan_block_comment(self, pos);
      changed = (pos != old_pos);
   }

   self->pos += pos;
   return self->pos;
}

static bool source_is_shebang(source self) {
   if (source_peek(self) == '#' && source_peek_offset(self, 1) == '!') {
      return true;
   }
   return false;
}

static anvl_dialect source_parse_dialect(source self, anvl_dialect current) {
   if (source_is_shebang(self)) {
      source_consume(self, 2);

      char first = source_peek(self);
      char second = source_peek_offset(self, 1);
      char third = source_peek_offset(self, 2);

      if (first == 'a' && second == 'm' && third == 'l') {
         source_consume(self, 3);
         return ANVL_DIALECT_AML;
      } else if (first == 'a' && second == 's' && third == 'l') {
         source_consume(self, 3);
         return ANVL_DIALECT_ASL;
      }
   }
   return current;
}

static void source_set_position(source self, usize pos, usize line, usize col) {
   self->pos = pos;
   self->line = line;
   self->col = col;
}

static void source_reset(source self) {
   self->pos = 0;
   self->line = 1;
   self->col = 1;
}

const anvl_source_i Source = {
    .create = source_create,
    .dispose = source_dispose,
    .dialect = source_get_dialect,
    .position = source_position,
    .line = source_line,
    .column = source_column,
    .is_eof = source_is_eof,
    .is_eof_offset = source_is_eof_offset,
    .peek = source_peek,
    .peek_offset = source_peek_offset,
    .match_length = source_scan_match,
    .match_operator = source_match_operator,
    .is_alpha = source_is_alpha,
    .is_digit = source_is_digit,
    .is_hex_digit = source_is_hex_digit,
    .is_identifier_start = source_is_identifier_start,
    .is_identifier_part = source_is_identifier_part,
    .consume = source_consume,
    .data = source_data,
    .length = source_length,
    .substring = source_substring,
    .skip_whitespace_and_comments = source_skip_whitespace_and_comments,
    .is_shebang = source_is_shebang,
    .parse_dialect = source_parse_dialect,
    .set_position = source_set_position,
    .reset = source_reset,
};