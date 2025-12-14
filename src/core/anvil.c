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

const anvl_i Anvil = {
    .load = anvil_load,
    .read = anvil_read,
    .dispose = anvil_dispose,
    .cleanup = anvil_cleanup,
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
      Memory.dispose(self);
   }
}

const struct anvl_context_i Context = {
    .get_builder = context_get_builder,
    .dialect = context_dialect,
    .statement_count = context_statement_count,
    .get_statement = context_get_statement,
    .add_statement = context_add_statement,
    .dispose = context_dispose,
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
      // create a dummy source with builder's dialect
      ctx->source = Source.create("", 0);
      ctx->source->dialect = self->dialect;

      // this creates a silent fail situation ... we need to set a global err flag
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