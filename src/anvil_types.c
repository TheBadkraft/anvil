/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 *                                                                        *
 * This software is proprietary and confidential. Unauthorized copying,   *
 * distribution, modification, or use of this software, via any medium,   *
 * is strictly prohibited without express written permission from the     *
 * copyright holder.                                                      *
 *                                                                        *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * anvil_types.c - Anvil's opt-in type registry                           *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Part of ANVL proper — opt-in, not an add-on: usable without schema.c,  *
 * which is the actual add-on layered on top of this (schema.c depends on *
 * this file, never the reverse; AnvilScript will depend on it directly   *
 * too). Lives directly under src/, a sibling of core/ and sigma/, not    *
 * nested under src/schema/ (that directory is reserved for schema.c      *
 * itself). Built entirely on the public flat API (anvil_flat.h) — no     *
 * core parser/resolver access. See notes/native-schema.md.               *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_type_registry.h"
// ----------------
#include <sigma/allocator.h>
#include <sigma/list.h>
#include <sigma/map.h>
#include <stdlib.h>
#include <string.h>

#define ANVIL_TYPE_NAME_MAX 65

struct anvil_type_def_t {
   char name[ANVIL_TYPE_NAME_MAX];
   anvil_type_kind kind;
   bool has_size;
   long long size;
   bool has_min;
   long long min;
   bool has_max;
   long long max;
   list values; // list of individually-allocated char* labels — only meaningful for
                // ANVIL_TYPE_ENUM; NULL if `values` wasn't declared
};

struct anvil_type_registry_t {
   map by_name;   // def->name -> struct anvil_type_def_t* (lookup)
   list all_defs; // struct anvil_type_def_t* — every def owned by this registry, for disposal
};

// Maps a `type :=` field's raw text to a recognized kind — the six native primitives
// (capitalized, per notes/native-schema.md's "Decided — native primitive set") plus the
// engine-understood `enum` kind. Anything else (including a Blob-only or unrecognized name)
// reports ANVIL_TYPE_UNKNOWN — Blob's own type identity is still an open design question.
static anvil_type_kind parse_type_kind(const char *text) {
   if (strcmp(text, "Numeric") == 0) return ANVIL_TYPE_NUMERIC;
   if (strcmp(text, "String") == 0) return ANVIL_TYPE_STRING;
   if (strcmp(text, "Bool") == 0) return ANVIL_TYPE_BOOL;
   if (strcmp(text, "Object") == 0) return ANVIL_TYPE_OBJECT;
   if (strcmp(text, "Tuple") == 0) return ANVIL_TYPE_TUPLE;
   if (strcmp(text, "Array") == 0) return ANVIL_TYPE_ARRAY;
   if (strcmp(text, "enum") == 0) return ANVIL_TYPE_ENUM;
   return ANVIL_TYPE_UNKNOWN;
}

// Reads a numeric field's raw text ("17", "1900") and converts it — anvil_value_get_text is
// the only way to read a scalar's content through the public flat API; there's no numeric
// accessor, so every consumer of a numeric field (this module included) parses it itself, same
// as anvil.node's own JS binding does with strtod.
static long long read_numeric(anvil_value val) {
   char text[32] = {0};
   anvil_value_get_text(val, text, sizeof(text));
   return strtoll(text, NULL, 10);
}

// Reads an enum type's `values := [ ... ]` array — bare, unquoted identifiers, per
// notes/native-schema.md's enum design — into a list of individually-allocated label copies.
// Returns NULL if val isn't an ARRAY (also correct for a non-enum type, which has no `values`
// field to reach this at all).
static list read_values_list(anvil_value val) {
   if (!val || anvil_value_get_type(val) != ANVIL_VALUE_ARRAY) {
      return NULL;
   }
   size_t count = anvil_value_get_count(val);
   list values = List.new(count > 0 ? count : 1, sizeof(char *));
   for (size_t i = 0; i < count; i++) {
      anvil_value elem = anvil_value_get_element(val, i);
      size_t len = anvil_value_get_text(elem, NULL, 0);
      char *label = Allocator.alloc(len + 1);
      if (!label) {
         continue;
      }
      anvil_value_get_text(elem, label, len + 1);
      List.append(values, label);
   }
   return values;
}

// Reads one type-definition statement's own OBJECT value in a single pass — its `type :=` kind
// plus whatever constraints it declares (`size`/`min`/`max`/`values`). Walks the statement's
// OBJECT value by index (the public flat API has no name-based nested-field lookup — see
// notes/native-schema.md) since that's the only way to reach any of these fields at all.
static void read_type_fields(anvil_value obj_val, struct anvil_type_def_t *def) {
   def->kind = ANVIL_TYPE_UNKNOWN;
   def->has_size = def->has_min = def->has_max = false;
   def->size = def->min = def->max = 0;
   def->values = NULL;

   size_t count = anvil_value_get_count(obj_val);
   for (size_t i = 0; i < count; i++) {
      anvil_statement field = anvil_value_get_statement(obj_val, i);
      char fname[16] = {0};
      anvil_statement_get_name(field, fname, sizeof(fname));
      anvil_value fval = anvil_statement_get_value(field);

      if (strcmp(fname, "type") == 0) {
         char text[32] = {0};
         anvil_value_get_text(fval, text, sizeof(text));
         def->kind = parse_type_kind(text);
      } else if (strcmp(fname, "size") == 0) {
         def->has_size = true;
         def->size = read_numeric(fval);
      } else if (strcmp(fname, "min") == 0) {
         def->has_min = true;
         def->min = read_numeric(fval);
      } else if (strcmp(fname, "max") == 0) {
         def->has_max = true;
         def->max = read_numeric(fval);
      } else if (strcmp(fname, "values") == 0) {
         def->values = read_values_list(fval);
      }
   }
}

// Walks one document's own top-level statements into reg, registering each well-formed type
// definition — shared by anvil_type_registry_load (doc itself is the definitions file) and
// anvil_type_registry_load_from_imports (called once per @[types]-carrying direct import, so
// several documents' definitions can land in the same combined registry).
static void collect_type_defs(anvil_document doc, struct anvil_type_registry_t *reg) {
   anvil_statement_iterator it = anvil_document_get_statements(doc);
   if (!it) {
      return;
   }
   anvil_statement stmt = NULL;
   while (anvil_statement_iterator_next(it, &stmt)) {
      anvil_value val = anvil_statement_get_value(stmt);
      if (!val || anvil_value_get_type(val) != ANVIL_VALUE_OBJECT) {
         continue; // not a well-formed type definition — skip (first-slice scope, not yet
                   // reported as a validation error; see anvil_type_registry_load's doc comment)
      }
      struct anvil_type_def_t *def = Allocator.alloc(sizeof(struct anvil_type_def_t));
      if (!def) {
         continue;
      }
      read_type_fields(val, def);
      if (def->kind == ANVIL_TYPE_UNKNOWN) {
         Allocator.dispose(def); // no recognized `type :=` — skip, same reasoning as above
         continue;
      }
      anvil_statement_get_name(stmt, def->name, sizeof(def->name));
      Map.set(reg->by_name, def->name, strlen(def->name), (addr)def); // last-write-wins on
                                                                       // a name collision
      List.append(reg->all_defs, def);
   }
   anvil_statement_iterator_dispose(it);
}

static struct anvil_type_registry_t *new_empty_registry(void) {
   struct anvil_type_registry_t *reg = Allocator.alloc(sizeof(struct anvil_type_registry_t));
   if (!reg) {
      return NULL;
   }
   reg->by_name = Map.new(8);
   reg->all_defs = List.new(8, sizeof(struct anvil_type_def_t *));
   return reg;
}

anvil_type_registry anvil_type_registry_load(anvil_document doc) {
   if (!doc || !anvil_document_find_attribute(doc, "types")) {
      return NULL; // @[types] has exactly one meaning — see notes/native-schema.md
   }
   struct anvil_type_registry_t *reg = new_empty_registry();
   if (!reg) {
      return NULL;
   }
   collect_type_defs(doc, reg);
   return (anvil_type_registry)reg;
}

anvil_type_registry anvil_type_registry_load_from_imports(anvil_document doc) {
   if (!doc) {
      return NULL;
   }
   struct anvil_type_registry_t *reg = new_empty_registry();
   if (!reg) {
      return NULL;
   }

   anvil_document_iterator it = anvil_document_get_imports(doc);
   if (it) {
      anvil_document imported = NULL;
      while (anvil_document_iterator_next(it, &imported)) {
         if (anvil_document_find_attribute(imported, "types")) {
            collect_type_defs(imported, reg);
         }
         anvil_dispose(imported); // caller's own responsibility — safe, shares doc's context
                                   // (see anvil_document_iterator's own doc comment)
      }
      anvil_document_iterator_dispose(it);
   }

   return (anvil_type_registry)reg;
}

void anvil_type_registry_dispose(anvil_type_registry reg) {
   struct anvil_type_registry_t *r = (struct anvil_type_registry_t *)reg;
   if (!r) {
      return;
   }
   if (r->all_defs) {
      usize n = List.size(r->all_defs);
      for (usize i = 0; i < n; i++) {
         struct anvil_type_def_t *def = NULL;
         List.get(r->all_defs, i, (object *)&def);
         if (def && def->values) {
            usize value_count = List.size(def->values);
            for (usize j = 0; j < value_count; j++) {
               char *label = NULL;
               List.get(def->values, j, (object *)&label);
               Allocator.dispose(label);
            }
            List.dispose(def->values);
         }
         Allocator.dispose(def);
      }
      List.dispose(r->all_defs);
   }
   if (r->by_name) {
      Map.dispose(r->by_name);
   }
   Allocator.dispose(r);
}

anvil_type_def anvil_type_registry_find(anvil_type_registry reg, const char *name) {
   struct anvil_type_registry_t *r = (struct anvil_type_registry_t *)reg;
   if (!r || !name) {
      return NULL;
   }
   addr val = 0;
   if (!Map.get(r->by_name, name, strlen(name), &val)) {
      return NULL;
   }
   return (anvil_type_def)val;
}

anvil_type_kind anvil_type_def_get_kind(anvil_type_def def) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d) {
      return ANVIL_TYPE_UNKNOWN;
   }
   return d->kind;
}

bool anvil_type_def_get_size(anvil_type_def def, long long *out_size) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !out_size || !d->has_size) {
      return false;
   }
   *out_size = d->size;
   return true;
}

bool anvil_type_def_get_min(anvil_type_def def, long long *out_min) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !out_min || !d->has_min) {
      return false;
   }
   *out_min = d->min;
   return true;
}

bool anvil_type_def_get_max(anvil_type_def def, long long *out_max) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !out_max || !d->has_max) {
      return false;
   }
   *out_max = d->max;
   return true;
}

size_t anvil_type_def_get_value_count(anvil_type_def def) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !d->values) {
      return 0;
   }
   return List.size(d->values);
}

size_t anvil_type_def_get_value(anvil_type_def def, size_t index, char *buf, size_t buflen) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !d->values || index >= List.size(d->values)) {
      return 0;
   }
   char *label = NULL;
   List.get(d->values, index, (object *)&label);
   if (!label) {
      return 0;
   }
   size_t len = strlen(label);
   if (buf && buflen > 0) {
      size_t copy_len = len < buflen - 1 ? len : buflen - 1;
      memcpy(buf, label, copy_len);
      buf[copy_len] = '\0';
   }
   return len;
}
