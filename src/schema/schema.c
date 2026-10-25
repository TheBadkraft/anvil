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
 * schema.c - AnvilSchema (the actual add-on layered on ANVL proper)      *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Built entirely on the public flat API (anvil_flat.h) and the public    *
 * type-registry API (anvil_type_registry.h) — no core parser/resolver    *
 * access. See docs/schema/README.md and notes/native-schema.md.          *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_schema.h"
#include "anvil_type_registry.h"
#include "internal/format.h"
#include "internal/numeric_bound.h"
#include "internal/text_alloc.h"
// ----------------
#include <sigma.core/allocator.h>
#include <sigma/list.h>
#include <sigma/map.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct anvil_schema_field_t {
   char *name; // allocated to its full length
   bool has_kind;
   anvil_type_kind kind;
   bool required;
   char *type_name;       // the `type :=` text as written ("types.Id32"); NULL if none
   list extras;           // struct schema_extra* - rule keys the schema didn't consume, in order
   char *format;          // a known lexical format the text must match; NULL if none
   char *tag;             // a blob must carry exactly this @tag; NULL if unconstrained
   bool integer;          // a numeric value must be whole (value-based: 2.0 and 1e+3 pass)
   bool integer_declared; // the rule said so itself, so it overrides the resolved type's
   bool nullable;          // an explicit `null` is a legal value (absent is governed by required)
   bool nullable_declared; // the rule said so itself, so it overrides the resolved type's
   bool has_size;
   long long size;
   bool has_min;
   anvil_numeric_t min;
   bool has_max;
   anvil_numeric_t max;
   list values; // list of individually-allocated char* labels; NULL if no values constraint
                // (own inline declaration or inherited from a resolved custom type)

   // Nested shape: a rule is an ordinary object, so it nests the same way a document does.
   bool has_fields;   // an Object rule declared `fields` (inline, or inherited from its type)
   list children;     // struct anvil_schema_field_t* - the nested rules, in order
   map child_by_name; // child->name -> child
   struct anvil_schema_field_t *items; // an Array rule's element rule (name is ""), or NULL
   bool has_min_items;
   long long min_items;
   bool has_max_items;
   long long max_items;
};

// A `type :=` field's text names either a bare native/enum kind ("Numeric", "enum") or a
// types.X custom type ("types.VIN") — anvil_type_resolve itself has no idea about the "types."
// namespace prefix (that's purely a source-level convention for *writing* a reference; the
// registry stores and looks up everything by bare name), so this is schema.c's own job before
// ever calling it.
static const char *strip_types_prefix(const char *text) {
   const char *prefix = "types.";
   size_t prefix_len = 6;
   if (strncmp(text, prefix, prefix_len) == 0) {
      return text + prefix_len;
   }
   return text;
}

// Mirrors anvil_types.c's own values-array reader — bare identifiers or quoted strings both
// read the same way through anvil_value_get_text regardless of which spelling was used.
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

// Copies one type_def's own value labels into a fresh list, for inheriting an enum type's
// values onto a field that didn't declare its own inline values.
static list copy_values_from_type(anvil_type_def def) {
   size_t count = anvil_type_def_get_value_count(def);
   if (count == 0) {
      return NULL;
   }
   list values = List.new(count, sizeof(char *));
   for (size_t i = 0; i < count; i++) {
      size_t len = anvil_type_def_get_value(def, i, NULL, 0);
      char *label = Allocator.alloc(len + 1);
      if (!label) {
         continue;
      }
      anvil_type_def_get_value(def, i, label, len + 1);
      List.append(values, label);
   }
   return values;
}

struct anvil_schema_violation_t {
   char *field; // allocated to its full length, never clipped to a fixed size
   anvil_schema_err_code category;
   char *message;
};

struct anvil_schema_diagnostic_t {
   anvil_schema_diag_code category;
   anvil_type_diag_code type_category; // only for ANVIL_SCHEMA_DIAG_TYPE_REGISTRY
   char *field;
   char *message;
};

struct anvil_schema_t {
   list fields;      // struct anvil_schema_field_t* — every declared field rule, in order
   map by_name;      // field->name -> struct anvil_schema_field_t* (presence/lookup)
   list violations;  // struct anvil_schema_violation_t* — from the most recent validate() call
   list diagnostics; // struct anvil_schema_diagnostic_t* — what anvil_schema_load noticed
};

// Records one load diagnostic. The message is formatted to its exact length: field names and
// type names have no fixed bound.
static void add_diag(struct anvil_schema_t *schema, anvil_schema_diag_code code,
                     anvil_type_diag_code type_code, const char *field, const char *fmt, ...) {
   struct anvil_schema_diagnostic_t *d = Allocator.alloc(sizeof(struct anvil_schema_diagnostic_t));
   if (!d) {
      return;
   }
   va_list ap, ap2;
   va_start(ap, fmt);
   va_copy(ap2, ap);
   int len = vsnprintf(NULL, 0, fmt, ap);
   va_end(ap);
   d->category = code;
   d->type_category = type_code;
   d->field = anvil_text_dup(field);
   d->message = len >= 0 ? Allocator.alloc((size_t)len + 1) : NULL;
   if (d->message) {
      vsnprintf(d->message, (size_t)len + 1, fmt, ap2);
   }
   va_end(ap2);
   if (!d->field || !d->message) {
      Allocator.dispose(d->field);
      Allocator.dispose(d->message);
      Allocator.dispose(d);
      return;
   }
   List.append(schema->diagnostics, d);
}

// Type definitions being expanded into nested rules right now, innermost first: a type whose
// `fields` reach itself (directly or through another type) would expand forever.
struct expand_frame {
   anvil_type_def def;
   const struct expand_frame *up;
};

// `prefix.name`, or just `name` at the top level. Paths are built to length: nesting has no bound.
static char *path_join(const char *prefix, const char *name) {
   if (!prefix || !*prefix) {
      return anvil_text_dup(name);
   }
   size_t plen = strlen(prefix), nlen = strlen(name);
   char *out = Allocator.alloc(plen + 1 + nlen + 1);
   if (out) {
      memcpy(out, prefix, plen);
      out[plen] = '.';
      memcpy(out + plen + 1, name, nlen + 1);
   }
   return out;
}

// `prefix[index]`.
static char *path_index(const char *prefix, size_t index) {
   int ilen = snprintf(NULL, 0, "%zu", index);
   size_t plen = strlen(prefix);
   char *out = Allocator.alloc(plen + (size_t)ilen + 3);
   if (out) {
      memcpy(out, prefix, plen);
      snprintf(out + plen, (size_t)ilen + 3, "[%zu]", index);
   }
   return out;
}

// A rule key the schema doesn't consume (a consumer's annotation, e.g. FlyWire's `pooled`). The
// value is copied into a schema-owned standalone value (a fragment document) so it outlives the
// schema's source document.
struct schema_extra {
   char *name;
   anvil_document frag; // owns `value`
   anvil_value value;   // NULL if the value couldn't be copied
};

// The source text of a value that anvil_parse_value_fragment will read back as the same value.
// Collections: their raw span (nested quoting intact). Scalars: a string's and a blob's content
// are delimiter-free in the public API, so they are re-wrapped from the exact bytes.
static char *value_source_text(anvil_value v) {
   anvil_value_type t = anvil_value_get_type(v);
   if (t != ANVIL_VALUE_STRING && t != ANVIL_VALUE_BLOB) {
      return anvil_text_alloc(v);
   }
   size_t blen = anvil_value_get_bytes(v, NULL, 0);
   size_t tlen = t == ANVIL_VALUE_BLOB ? anvil_value_get_blob_tag(v, NULL, 0) : 0;
   char *out = Allocator.alloc(blen + tlen + 4);
   if (!out) {
      return NULL;
   }
   size_t at = 0;
   if (tlen) {
      out[at++] = '@';
      anvil_value_get_blob_tag(v, out + at, tlen + 1);
      at += tlen;
   }
   out[at++] = t == ANVIL_VALUE_BLOB ? '`' : '"';
   anvil_value_get_bytes(v, out + at, blen);
   at += blen;
   out[at++] = t == ANVIL_VALUE_BLOB ? '`' : '"';
   out[at] = '\0';
   return out;
}

static void add_extra(struct anvil_schema_field_t *field, const char *name, anvil_value val) {
   struct schema_extra *e = Allocator.alloc(sizeof(struct schema_extra));
   if (!e) {
      return;
   }
   memset(e, 0, sizeof(*e));
   e->name = anvil_text_dup(name);
   char *text = val ? value_source_text(val) : NULL;
   if (text) {
      e->frag = anvil_parse_value_fragment(text, strlen(text));
      if (e->frag && !anvil_has_errors(e->frag)) {
         e->value = anvil_document_get_fragment_value(e->frag);
      }
      if (!e->value && e->frag) {
         anvil_dispose(e->frag);
         e->frag = NULL;
      }
   }
   Allocator.dispose(text);
   if (!e->name) {
      if (e->frag) {
         anvil_dispose(e->frag);
      }
      Allocator.dispose(e);
      return;
   }
   if (!field->extras) {
      field->extras = List.new(2, sizeof(struct schema_extra *));
   }
   List.append(field->extras, e);
}

static struct anvil_schema_field_t *new_field(char *name) {
   struct anvil_schema_field_t *f = Allocator.alloc(sizeof(struct anvil_schema_field_t));
   if (!f) {
      Allocator.dispose(name);
      return NULL;
   }
   memset(f, 0, sizeof(*f));
   f->kind = ANVIL_TYPE_UNKNOWN;
   f->name = name;
   return f;
}

static void dispose_field_values(list values);

static void field_dispose(struct anvil_schema_field_t *f) {
   if (!f) {
      return;
   }
   dispose_field_values(f->values);
   Allocator.dispose(f->tag);
   Allocator.dispose(f->format);
   Allocator.dispose(f->type_name);
   if (f->extras) {
      for (usize i = 0, n = List.size(f->extras); i < n; i++) {
         struct schema_extra *e = NULL;
         List.get(f->extras, i, (object *)&e);
         if (e) {
            if (e->frag) {
               anvil_dispose(e->frag);
            }
            Allocator.dispose(e->name);
         }
         Allocator.dispose(e);
      }
      List.dispose(f->extras);
   }
   if (f->children) {
      for (usize i = 0, n = List.size(f->children); i < n; i++) {
         struct anvil_schema_field_t *c = NULL;
         List.get(f->children, i, (object *)&c);
         field_dispose(c);
      }
      List.dispose(f->children);
   }
   if (f->child_by_name) {
      Map.dispose(f->child_by_name);
   }
   field_dispose(f->items);
   Allocator.dispose(f->name);
   Allocator.dispose(f);
}

static void build_rule(anvil_value obj_val, anvil_type_registry types,
                       struct anvil_schema_field_t *field, struct anvil_schema_t *schema,
                       const char *path, const struct expand_frame *stack);

// Builds the nested rules of a `fields := { name := { <rule> }; ... }` object into field.
static void build_children(anvil_value fields_obj, anvil_type_registry types,
                           struct anvil_schema_field_t *field, struct anvil_schema_t *schema,
                           const char *path, const struct expand_frame *stack) {
   field->has_fields = true;
   if (!field->children) {
      field->children = List.new(4, sizeof(struct anvil_schema_field_t *));
      field->child_by_name = Map.new(8);
   }
   size_t count = anvil_value_get_count(fields_obj);
   for (size_t i = 0; i < count; i++) {
      anvil_statement stmt = anvil_value_get_statement(fields_obj, i);
      char *name = anvil_name_alloc(stmt);
      if (!name) {
         continue;
      }
      anvil_value val = anvil_statement_get_value(stmt);
      char *child_path = path_join(path, name);
      if (!child_path) {
         Allocator.dispose(name);
         continue;
      }
      if (!val || anvil_value_get_type(val) != ANVIL_VALUE_OBJECT) {
         add_diag(schema, ANVIL_SCHEMA_DIAG_RULE_NOT_OBJECT, ANVIL_TYPE_DIAG_NONE, child_path,
                  "'%s' is not a field rule (expected a '{ ... }' object); skipped", child_path);
         Allocator.dispose(name);
         Allocator.dispose(child_path);
         continue;
      }
      struct anvil_schema_field_t *child = new_field(name);
      if (child) {
         build_rule(val, types, child, schema, child_path, stack);
         List.append(field->children, child);
         Map.set(field->child_by_name, child->name, strlen(child->name), (addr)child);
      }
      Allocator.dispose(child_path);
   }
}

// A field referencing an Object type that declares `fields` inherits that shape, unless it
// declared its own (inline replaces, never merges - same rule as every other constraint). The
// registry keeps the nested rules as source text and stays ignorant of the rule vocabulary, so
// the text is re-parsed here as a standalone value.
static void expand_type_fields(anvil_type_def def, anvil_type_registry types,
                               struct anvil_schema_field_t *field, struct anvil_schema_t *schema,
                               const char *path, const struct expand_frame *stack) {
   const char *text = anvil_type_def_fields_text(def);
   if (!text) {
      return;
   }
   for (const struct expand_frame *f = stack; f; f = f->up) {
      if (f->def == def) {
         add_diag(schema, ANVIL_SCHEMA_DIAG_SELF_REFERENCE, ANVIL_TYPE_DIAG_NONE, path,
                  "field '%s': its type's 'fields' refer back to the type itself; not expanded",
                  path);
         return;
      }
   }
   anvil_document frag = anvil_parse_value_fragment(text, strlen(text));
   anvil_value obj = frag ? anvil_document_get_fragment_value(frag) : NULL;
   if (obj && anvil_value_get_type(obj) == ANVIL_VALUE_OBJECT) {
      struct expand_frame frame = {def, stack};
      build_children(obj, types, field, schema, path, &frame);
   }
   if (frag) {
      anvil_dispose(frag);
   }
}

// A schema field's declared kind is transparent-through: a bare native primitive, a bare
// `enum`, or a types.X custom type all resolve to the same anvil_type_kind, so validation
// never needs to know which of the three produced it — see anvil_type_resolve. `path` is the
// rule's full path (`links[].target`), used only for diagnostics.
static void build_rule(anvil_value obj_val, anvil_type_registry types,
                       struct anvil_schema_field_t *field, struct anvil_schema_t *schema,
                       const char *path, const struct expand_frame *stack) {
   anvil_type_def resolved = NULL;

   // First pass: read exactly what the field itself declares, inline — never consulting the
   // resolved type yet. Constraint inheritance (below) only fills in what this pass left unset.
   size_t count = anvil_value_get_count(obj_val);
   for (size_t i = 0; i < count; i++) {
      anvil_statement fstmt = anvil_value_get_statement(obj_val, i);
      char *fname = anvil_name_alloc(fstmt);
      anvil_value fval = anvil_statement_get_value(fstmt);
      if (!fname) {
         continue;
      }

      if (strcmp(fname, "type") == 0) {
         char *text = anvil_text_alloc(fval);
         Allocator.dispose(field->type_name);
         field->type_name = text ? anvil_text_dup(text) : NULL;
         resolved = text ? anvil_type_resolve(types, strip_types_prefix(text)) : NULL;
         if (!resolved) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_UNKNOWN_TYPE, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': type '%s' is neither a native type nor a known types.X", path,
                     text ? text : "");
         }
         Allocator.dispose(text);
         if (resolved) {
            field->has_kind = true;
            field->kind = anvil_type_def_get_kind(resolved);
         }
         // An unrecognized type name leaves the field with no kind to check against, and is
         // reported as UNKNOWN_TYPE above; the schema still loads (the caller decides).
      } else if (strcmp(fname, "required") == 0) {
         char *text = anvil_text_alloc(fval);
         field->required = text && strcmp(text, "true") == 0;
         if (!text || (strcmp(text, "true") != 0 && strcmp(text, "false") != 0)) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'required' must be true or false", path);
         }
         Allocator.dispose(text);
      } else if (strcmp(fname, "format") == 0) {
         anvil_value_type ft = fval ? anvil_value_get_type(fval) : ANVIL_VALUE_NULL;
         char *name = (ft == ANVIL_VALUE_BARE || ft == ANVIL_VALUE_STRING) ? anvil_text_alloc(fval) : NULL;
         if (name && anvil_format_known(name)) {
            Allocator.dispose(field->format);
            field->format = name;
         } else {
            add_diag(schema, ANVIL_SCHEMA_DIAG_UNKNOWN_FORMAT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': unknown format '%s' (known: iso8601_date, iso8601_datetime, "
                     "uuid); treated as not declared", path, name ? name : "");
            Allocator.dispose(name);
         }
      } else if (strcmp(fname, "tag") == 0) {
         anvil_value_type tt = fval ? anvil_value_get_type(fval) : ANVIL_VALUE_NULL;
         if (tt == ANVIL_VALUE_BARE || tt == ANVIL_VALUE_STRING) {
            Allocator.dispose(field->tag);
            field->tag = anvil_text_alloc(fval);
         } else {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'tag' must be an identifier or string; treated as not declared",
                     path);
         }
      } else if (strcmp(fname, "integer") == 0) {
         char *text = anvil_text_alloc(fval);
         if (text && (strcmp(text, "true") == 0 || strcmp(text, "false") == 0)) {
            field->integer_declared = true;
            field->integer = (strcmp(text, "true") == 0);
         } else {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'integer' must be true or false; treated as false", path);
         }
         Allocator.dispose(text);
      } else if (strcmp(fname, "nullable") == 0) {
         char *text = anvil_text_alloc(fval);
         if (text && (strcmp(text, "true") == 0 || strcmp(text, "false") == 0)) {
            field->nullable_declared = true;
            field->nullable = (strcmp(text, "true") == 0);
         } else {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'nullable' must be true or false; treated as false", path);
         }
         Allocator.dispose(text);
      } else if (strcmp(fname, "size") == 0) {
         field->has_size = anvil_numeric_read_size(fval, &field->size);
         if (!field->has_size) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'size' is not a non-negative integer; treated as not declared",
                     path);
         }
      } else if (strcmp(fname, "min") == 0) {
         field->has_min = anvil_numeric_read_bound(fval, &field->min);
         if (!field->has_min) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'min' is not a valid number; treated as not declared", path);
         }
      } else if (strcmp(fname, "max") == 0) {
         field->has_max = anvil_numeric_read_bound(fval, &field->max);
         if (!field->has_max) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'max' is not a valid number; treated as not declared", path);
         }
      } else if (strcmp(fname, "values") == 0) {
         field->values = read_values_list(fval);
         if (!field->values) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'values' must be an array of labels", path);
         }
      } else if (strcmp(fname, "min_items") == 0) {
         field->has_min_items = anvil_numeric_read_size(fval, &field->min_items);
         if (!field->has_min_items) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'min_items' is not a non-negative integer; treated as not "
                     "declared", path);
         }
      } else if (strcmp(fname, "max_items") == 0) {
         field->has_max_items = anvil_numeric_read_size(fval, &field->max_items);
         if (!field->has_max_items) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'max_items' is not a non-negative integer; treated as not "
                     "declared", path);
         }
      } else if (strcmp(fname, "fields") == 0) {
         if (fval && anvil_value_get_type(fval) == ANVIL_VALUE_OBJECT) {
            build_children(fval, types, field, schema, path, stack);
         } else {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'fields' must be an object of field rules", path);
         }
      } else if (strcmp(fname, "items") == 0) {
         char *items_path = NULL;
         if (fval && anvil_value_get_type(fval) == ANVIL_VALUE_OBJECT) {
            size_t plen = strlen(path);
            items_path = Allocator.alloc(plen + 3);
            if (items_path) {
               memcpy(items_path, path, plen);
               memcpy(items_path + plen, "[]", 3);
               field_dispose(field->items); // a repeated `items` replaces the earlier one
               field->items = new_field(anvil_text_dup(""));
               if (field->items) {
                  build_rule(fval, types, field->items, schema, items_path, stack);
               }
            }
            Allocator.dispose(items_path);
         } else {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE, path,
                     "field '%s': 'items' must be a field rule object", path);
         }
      } else {
         add_extra(field, fname, fval); // not ours: passed on to the consumer, never diagnosed
      }
      Allocator.dispose(fname);
   }

   // Constraint inheritance: whatever the field didn't declare inline falls back to the
   // resolved type's own constraint, if any — the field's own inline declaration always wins,
   // never merged with the type's (see notes/native-schema.md's "inline field constraints"
   // decision).
   if (resolved) {
      if (!field->has_size) {
         field->has_size = anvil_type_def_get_size(resolved, &field->size);
      }
      if (!field->has_min) {
         field->has_min = anvil_type_def_get_min_numeric(resolved, &field->min);
      }
      if (!field->has_max) {
         field->has_max = anvil_type_def_get_max_numeric(resolved, &field->max);
      }
      if (!field->values) {
         field->values = copy_values_from_type(resolved);
      }
      if (!field->format) {
         const char *type_format = anvil_type_def_format(resolved);
         field->format = type_format ? anvil_text_dup(type_format) : NULL;
      }
      if (!field->tag) {
         const char *type_tag = anvil_type_def_tag(resolved);
         field->tag = type_tag ? anvil_text_dup(type_tag) : NULL;
      }
      if (!field->integer_declared) {
         bool declared = false;
         bool value = anvil_type_def_integer(resolved, &declared);
         if (declared) {
            field->integer = value;
         }
      }
      if (!field->nullable_declared) {
         bool declared = false;
         bool value = anvil_type_def_nullable(resolved, &declared);
         if (declared) {
            field->nullable = value;
         }
      }
      if (!field->has_fields) {
         expand_type_fields(resolved, types, field, schema, path, stack);
      }
   }

   // Effective (inline or inherited) bounds that nothing can satisfy.
   if (field->has_min && field->has_max && anvil_numeric_compare_bounds(field->min, field->max) > 0) {
      add_diag(schema, ANVIL_SCHEMA_DIAG_CONFLICTING_CONSTRAINTS, ANVIL_TYPE_DIAG_NONE, path,
               "field '%s': 'min' is greater than 'max'; no value can satisfy it", path);
   }
   if (field->has_min_items && field->has_max_items && field->min_items > field->max_items) {
      add_diag(schema, ANVIL_SCHEMA_DIAG_CONFLICTING_CONSTRAINTS, ANVIL_TYPE_DIAG_NONE, path,
               "field '%s': 'min_items' is greater than 'max_items'; no array can satisfy it",
               path);
   }
}

anvil_schema anvil_schema_load(anvil_document doc) {
   if (!doc || !anvil_document_find_attribute(doc, "schema")) {
      return NULL; // @[schema] marks a schema definition — declaring shape, never filling one
   }

   struct anvil_schema_t *schema = Allocator.alloc(sizeof(struct anvil_schema_t));
   if (!schema) {
      return NULL;
   }
   schema->fields = List.new(8, sizeof(struct anvil_schema_field_t *));
   schema->by_name = Map.new(8);
   schema->violations = NULL;
   schema->diagnostics = List.new(4, sizeof(struct anvil_schema_diagnostic_t *));

   // types. access comes purely from doc's own includes — @[schema] never implies it, matching
   // how any other document would resolve types.X. Only needed for the duration of the load
   // pass itself: each field's resolved *kind* is copied out below, so the registry doesn't
   // need to outlive this function.
   anvil_type_registry types = anvil_type_registry_load_from_includes(doc);

   // The registry's own diagnostics come first, so one list gives the full picture.
   size_t registry_diags = anvil_type_registry_get_diagnostic_count(types);
   for (size_t i = 0; i < registry_diags; i++) {
      anvil_type_diagnostic rd = anvil_type_registry_get_diagnostic(types, i);
      size_t name_len = anvil_type_diagnostic_get_name(rd, NULL, 0);
      size_t msg_len = anvil_type_diagnostic_get_message(rd, NULL, 0);
      char *name = Allocator.alloc(name_len + 1);
      char *msg = Allocator.alloc(msg_len + 1);
      if (name && msg) {
         anvil_type_diagnostic_get_name(rd, name, name_len + 1);
         anvil_type_diagnostic_get_message(rd, msg, msg_len + 1);
         add_diag(schema, ANVIL_SCHEMA_DIAG_TYPE_REGISTRY, anvil_type_diagnostic_get_category(rd),
                  name, "%s", msg);
      }
      Allocator.dispose(name);
      Allocator.dispose(msg);
   }

   anvil_statement_iterator it = anvil_document_get_statements(doc);
   if (it) {
      anvil_statement stmt = NULL;
      while (anvil_statement_iterator_next(it, &stmt)) {
         anvil_value val = anvil_statement_get_value(stmt);
         if (!val || anvil_value_get_type(val) != ANVIL_VALUE_OBJECT) {
            char *bad = anvil_name_alloc(stmt);
            if (bad) {
               add_diag(schema, ANVIL_SCHEMA_DIAG_RULE_NOT_OBJECT, ANVIL_TYPE_DIAG_NONE, bad,
                        "'%s' is not a field rule (expected a '{ ... }' object); skipped", bad);
               Allocator.dispose(bad);
            }
            continue; // not a field rule - skipped, and reported above
         }
         struct anvil_schema_field_t *field = new_field(anvil_name_alloc(stmt));
         if (!field || !field->name) {
            field_dispose(field);
            continue;
         }
         build_rule(val, types, field, schema, field->name, NULL);

         List.append(schema->fields, field);
         Map.set(schema->by_name, field->name, strlen(field->name), (addr)field);
      }
      anvil_statement_iterator_dispose(it);
   }

   anvil_type_registry_dispose(types);
   return (anvil_schema)schema;
}

static void dispose_violations(list violations) {
   if (!violations) {
      return;
   }
   usize n = List.size(violations);
   for (usize i = 0; i < n; i++) {
      struct anvil_schema_violation_t *v = NULL;
      List.get(violations, i, (object *)&v);
      if (v) {
         Allocator.dispose(v->field);
         Allocator.dispose(v->message);
      }
      Allocator.dispose(v);
   }
   List.dispose(violations);
}

static void dispose_field_values(list values) {
   if (!values) {
      return;
   }
   usize n = List.size(values);
   for (usize i = 0; i < n; i++) {
      char *label = NULL;
      List.get(values, i, (object *)&label);
      Allocator.dispose(label);
   }
   List.dispose(values);
}

void anvil_schema_dispose(anvil_schema schema) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   if (!s) {
      return;
   }
   dispose_violations(s->violations);
   if (s->diagnostics) {
      for (usize i = 0, n = List.size(s->diagnostics); i < n; i++) {
         struct anvil_schema_diagnostic_t *d = NULL;
         List.get(s->diagnostics, i, (object *)&d);
         if (d) {
            Allocator.dispose(d->field);
            Allocator.dispose(d->message);
         }
         Allocator.dispose(d);
      }
      List.dispose(s->diagnostics);
   }
   if (s->fields) {
      usize n = List.size(s->fields);
      for (usize i = 0; i < n; i++) {
         struct anvil_schema_field_t *f = NULL;
         List.get(s->fields, i, (object *)&f);
         field_dispose(f);
      }
      List.dispose(s->fields);
   }
   if (s->by_name) {
      Map.dispose(s->by_name);
   }
   Allocator.dispose(s);
}

// True if a data field's actual value kind satisfies a schema field's declared kind. A resolved
// enum (or String) accepts either a quoted STRING or a bare IDENTIFIER — schema fields may use
// either spelling for a label (FlyWire's own inline `values` convention uses quoted strings;
// anvil_types.c's own enum convention uses bare identifiers), and neither should read as a
// mismatch just because of quoting. ANVIL_TYPE_UNKNOWN (no declared kind) always matches — there
// is nothing to check.
static bool kind_matches(anvil_type_kind expected, anvil_value_type actual) {
   switch (expected) {
   case ANVIL_TYPE_NUMERIC: return actual == ANVIL_VALUE_NUMERIC;
   case ANVIL_TYPE_STRING:
      // "Textual content", whatever the spelling: a backtick blob is a string with a tag.
      return actual == ANVIL_VALUE_STRING || actual == ANVIL_VALUE_BARE || actual == ANVIL_VALUE_BLOB;
   case ANVIL_TYPE_ENUM:
      return actual == ANVIL_VALUE_STRING || actual == ANVIL_VALUE_BARE;
   case ANVIL_TYPE_BLOB:   return actual == ANVIL_VALUE_BLOB;
   case ANVIL_TYPE_BOOL:   return actual == ANVIL_VALUE_BOOL;
   case ANVIL_TYPE_OBJECT: return actual == ANVIL_VALUE_OBJECT;
   case ANVIL_TYPE_TUPLE:  return actual == ANVIL_VALUE_TUPLE;
   case ANVIL_TYPE_ARRAY:  return actual == ANVIL_VALUE_ARRAY;
   case ANVIL_TYPE_UNKNOWN:
   default:
      return true;
   }
}

static void add_violation(list violations, const char *field_name, anvil_schema_err_code cat,
                          const char *message) {
   struct anvil_schema_violation_t *v = Allocator.alloc(sizeof(struct anvil_schema_violation_t));
   if (!v) {
      return;
   }
   v->field = anvil_text_dup(field_name);
   v->message = anvil_text_dup(message);
   v->category = cat;
   if (!v->field || !v->message) {
      Allocator.dispose(v->field);
      Allocator.dispose(v->message);
      Allocator.dispose(v);
      return;
   }
   List.append(violations, v);
}

// The statements being validated, whichever shape they came from (a document's top level or an
// OBJECT value's members): every statement in order, plus a name -> first-statement map so the
// per-field pass is a lookup, not a scan. Names are held at full length.
struct data_view {
   list entries; // struct data_entry* in source order
   map first;    // name -> struct data_entry* of the first statement with that name
};

struct data_entry {
   char *name;
   anvil_statement stmt;
};

static void data_view_add(struct data_view *view, anvil_statement stmt) {
   struct data_entry *e = Allocator.alloc(sizeof(struct data_entry));
   if (!e) {
      return;
   }
   e->name = anvil_name_alloc(stmt);
   e->stmt = stmt;
   if (!e->name) {
      Allocator.dispose(e);
      return;
   }
   List.append(view->entries, e);
   addr prior = 0;
   if (!Map.get(view->first, e->name, strlen(e->name), &prior)) {
      Map.set(view->first, e->name, strlen(e->name), (addr)e);
   }
}

static void data_view_init(struct data_view *view) {
   view->entries = List.new(8, sizeof(struct data_entry *));
   view->first = Map.new(8);
}

static void data_view_dispose(struct data_view *view) {
   for (usize i = 0, n = List.size(view->entries); i < n; i++) {
      struct data_entry *e = NULL;
      List.get(view->entries, i, (object *)&e);
      if (e) {
         Allocator.dispose(e->name);
      }
      Allocator.dispose(e);
   }
   List.dispose(view->entries);
   Map.dispose(view->first);
}

// Option flags honour opts->size, so a caller compiled against an older, shorter struct reads
// every member past its end as the default.
#define OPT_FLAG(opts, member) \
   ((opts) && (opts)->size >= offsetof(anvil_schema_options, member) + sizeof((opts)->member) && \
    (opts)->member)

// Replaces the previous call's violations with a fresh, empty list.
static void reset_violations(struct anvil_schema_t *s) {
   dispose_violations(s->violations);
   s->violations = List.new(4, sizeof(struct anvil_schema_violation_t *));
}

// The two options validation reads, resolved once from the caller's (possibly shorter) struct.
struct vopts {
   bool skip_required;
   bool allow_unknown;
};

static void check_value(struct anvil_schema_t *s, const struct anvil_schema_field_t *rule,
                        const char *path, anvil_value data_val, const struct vopts *vo);

// Validates one level of members: every declared rule against the view, then every view statement
// the level doesn't declare. Violations are depth first in schema order: a rule's own violations,
// then its nested ones, before the next rule; unknown members of a level come after its rules.
static void validate_members(struct anvil_schema_t *s, list rules, map rules_by_name,
                             const struct data_view *view, const char *prefix,
                             const struct vopts *vo) {
   usize rule_count = rules ? List.size(rules) : 0;
   for (usize i = 0; i < rule_count; i++) {
      struct anvil_schema_field_t *rule = NULL;
      List.get(rules, i, (object *)&rule);
      if (!rule) {
         continue;
      }
      char *path = path_join(prefix, rule->name);
      if (!path) {
         continue;
      }

      addr found = 0;
      if (!Map.get(view->first, rule->name, strlen(rule->name), &found) || !found) {
         if (rule->required && !vo->skip_required) {
            add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,
                          "required field is missing");
         }
      } else {
         anvil_statement data_stmt = ((struct data_entry *)found)->stmt;
         check_value(s, rule, path, anvil_statement_get_value(data_stmt), vo);
      }
      Allocator.dispose(path);
   }

   // Every member not declared at this level is a violation of its own - a second pass over the
   // statements, independent of the pass above.
   if (!vo->allow_unknown) {
      for (usize i = 0, n = List.size(view->entries); i < n; i++) {
         struct data_entry *e = NULL;
         List.get(view->entries, i, (object *)&e);
         addr unused = 0;
         if (e && (!rules_by_name || !Map.get(rules_by_name, e->name, strlen(e->name), &unused))) {
            char *path = path_join(prefix, e->name);
            if (path) {
               add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD,
                             "field is not declared in the schema");
               Allocator.dispose(path);
            }
         }
      }
   }
}

// One present value against one rule - a declared field's value, or an array element against its
// array's `items` rule - then, if the rule has a shape, into it.
static void check_value(struct anvil_schema_t *s, const struct anvil_schema_field_t *rule,
                        const char *path, anvil_value data_val, const struct vopts *vo) {
   anvil_value_type actual = data_val ? anvil_value_get_type(data_val) : ANVIL_VALUE_NULL;

   // An explicit null: legal iff the rule says so, and then nothing else about the value is
   // evaluated. An untyped rule has no declared type for null to violate.
   if (actual == ANVIL_VALUE_NULL) {
      if (rule->nullable) {
         return;
      }
      if (rule->has_kind) {
         add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                       "field is not nullable");
         return;
      }
   }

   if (rule->has_kind && !kind_matches(rule->kind, actual)) {
      add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                    "field value does not match the declared type");
      return; // a value of the wrong kind can't be meaningfully checked against
              // size/min/max/values too — one violation, not a confusing pile of them
   }

   if (rule->has_size && data_val) {
      // A blob's size is its payload in bytes (the tag is not payload); everything else is the
      // length of its text.
      size_t len = actual == ANVIL_VALUE_BLOB ? anvil_value_get_bytes(data_val, NULL, 0)
                                              : anvil_value_get_text(data_val, NULL, 0);
      if ((long long)len > rule->size) {
         add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_SIZE,
                       "field value exceeds the declared size");
      }
   }
   if (rule->format && (actual == ANVIL_VALUE_STRING || actual == ANVIL_VALUE_BARE ||
                        actual == ANVIL_VALUE_BLOB)) {
      // The text to match: a string's content, or a blob's payload (get_bytes is not NUL-terminated).
      size_t blen = anvil_value_get_bytes(data_val, NULL, 0);
      char *text = Allocator.alloc(blen + 1);
      bool valid = false;
      if (text) {
         anvil_value_get_bytes(data_val, text, blen);
         text[blen] = '\0';
         valid = strlen(text) == blen && anvil_format_valid(rule->format, text);
      }
      Allocator.dispose(text);
      if (!valid) {
         add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_FORMAT,
                       "value does not match the declared format");
      }
   }
   if (rule->tag && actual == ANVIL_VALUE_BLOB) {
      size_t tlen = anvil_value_get_blob_tag(data_val, NULL, 0);
      char *tag = Allocator.alloc(tlen + 1);
      bool match = false;
      if (tag) {
         anvil_value_get_blob_tag(data_val, tag, tlen + 1);
         match = strcmp(tag, rule->tag) == 0;
      }
      Allocator.dispose(tag);
      if (!match) {
         add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_TAG,
                       "blob tag does not match the required tag");
      }
   }
   if (rule->integer && data_val && actual == ANVIL_VALUE_NUMERIC && !anvil_numeric_is_whole(data_val)) {
      add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                    "value must be an integer");
   }
   if ((rule->has_min || rule->has_max) && data_val && actual == ANVIL_VALUE_NUMERIC) {
      if ((rule->has_min && anvil_numeric_compare(data_val, rule->min) < 0) ||
          (rule->has_max && anvil_numeric_compare(data_val, rule->max) > 0)) {
         add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_RANGE,
                       "field value is outside the declared min/max range");
      }
   }
   if (rule->values && data_val) {
      char *text = anvil_text_alloc(data_val);
      bool member = false;
      usize value_count = List.size(rule->values);
      for (usize vi = 0; vi < value_count; vi++) {
         char *label = NULL;
         List.get(rule->values, vi, (object *)&label);
         if (label && text && strcmp(label, text) == 0) {
            member = true;
            break;
         }
      }
      Allocator.dispose(text);
      if (!member) {
         add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_VALUES,
                       "field value is not one of the declared values");
      }
   }

   // Shape: an object's declared `fields`, an array's `items` and item count.
   if (rule->has_fields && actual == ANVIL_VALUE_OBJECT) {
      struct data_view inner;
      data_view_init(&inner);
      size_t count = anvil_value_get_count(data_val);
      for (size_t i = 0; i < count; i++) {
         data_view_add(&inner, anvil_value_get_statement(data_val, i));
      }
      validate_members(s, rule->children, rule->child_by_name, &inner, path, vo);
      data_view_dispose(&inner);
   }
   if (actual == ANVIL_VALUE_ARRAY) {
      size_t count = anvil_value_get_count(data_val);
      if (rule->has_min_items && (long long)count < rule->min_items) {
         add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_ITEM_COUNT,
                       "array has fewer items than min_items");
      }
      if (rule->has_max_items && (long long)count > rule->max_items) {
         add_violation(s->violations, path, ANVIL_SCHEMA_ERR_VALIDATION_ITEM_COUNT,
                       "array has more items than max_items");
      }
      if (rule->items) {
         for (size_t i = 0; i < count; i++) {
            char *item_path = path_index(path, i);
            if (item_path) {
               check_value(s, rule->items, item_path, anvil_value_get_element(data_val, i), vo);
               Allocator.dispose(item_path);
            }
         }
      }
   }
}

// The one validation pass. Both the document form and the value form land here.
static bool validate_view(struct anvil_schema_t *s, const struct data_view *view,
                          const anvil_schema_options *opts) {
   reset_violations(s);
   struct vopts vo = {.skip_required = OPT_FLAG(opts, skip_required),
                      .allow_unknown = OPT_FLAG(opts, allow_unknown)};
   validate_members(s, s->fields, s->by_name, view, "", &vo);
   return List.size(s->violations) == 0;
}

bool anvil_schema_validate(anvil_schema schema, anvil_document data_doc) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   if (!s || !data_doc) {
      return false;
   }
   struct data_view view;
   data_view_init(&view);
   anvil_statement_iterator it = anvil_document_get_statements(data_doc);
   if (it) {
      anvil_statement stmt = NULL;
      while (anvil_statement_iterator_next(it, &stmt)) {
         data_view_add(&view, stmt);
      }
      anvil_statement_iterator_dispose(it);
   }
   bool ok = validate_view(s, &view, NULL);
   data_view_dispose(&view);
   return ok;
}

bool anvil_schema_validate_value(anvil_schema schema, anvil_value object_value,
                                 const anvil_schema_options *opts) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   if (!s || !object_value) {
      return false;
   }
   if (anvil_value_get_type(object_value) != ANVIL_VALUE_OBJECT) {
      reset_violations(s);
      add_violation(s->violations, "", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                    "value is not an object");
      return false;
   }
   struct data_view view;
   data_view_init(&view);
   size_t count = anvil_value_get_count(object_value);
   for (size_t i = 0; i < count; i++) {
      data_view_add(&view, anvil_value_get_statement(object_value, i));
   }
   bool ok = validate_view(s, &view, opts);
   data_view_dispose(&view);
   return ok;
}

size_t anvil_schema_get_violation_count(anvil_schema schema) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   if (!s || !s->violations) {
      return 0;
   }
   return List.size(s->violations);
}

anvil_schema_violation anvil_schema_get_violation(anvil_schema schema, size_t index) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   if (!s || !s->violations || index >= List.size(s->violations)) {
      return NULL;
   }
   struct anvil_schema_violation_t *v = NULL;
   List.get(s->violations, index, (object *)&v);
   return (anvil_schema_violation)v;
}

anvil_schema_err_code anvil_schema_violation_get_category(anvil_schema_violation v) {
   struct anvil_schema_violation_t *vv = (struct anvil_schema_violation_t *)v;
   if (!vv) {
      return ANVIL_SCHEMA_ERR_NONE;
   }
   return vv->category;
}

size_t anvil_schema_violation_get_field(anvil_schema_violation v, char *buf, size_t buflen) {
   struct anvil_schema_violation_t *vv = (struct anvil_schema_violation_t *)v;
   if (!vv) {
      return 0;
   }
   size_t len = strlen(vv->field);
   if (buf && buflen > 0) {
      size_t copy_len = len < buflen - 1 ? len : buflen - 1;
      memcpy(buf, vv->field, copy_len);
      buf[copy_len] = '\0';
   }
   return len;
}

size_t anvil_schema_violation_get_message(anvil_schema_violation v, char *buf, size_t buflen) {
   struct anvil_schema_violation_t *vv = (struct anvil_schema_violation_t *)v;
   if (!vv) {
      return 0;
   }
   size_t len = strlen(vv->message);
   if (buf && buflen > 0) {
      size_t copy_len = len < buflen - 1 ? len : buflen - 1;
      memcpy(buf, vv->message, copy_len);
      buf[copy_len] = '\0';
   }
   return len;
}

size_t anvil_schema_get_diagnostic_count(anvil_schema schema) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   return s && s->diagnostics ? (size_t)List.size(s->diagnostics) : 0;
}

anvil_schema_diagnostic anvil_schema_get_diagnostic(anvil_schema schema, size_t index) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   if (!s || !s->diagnostics || index >= (size_t)List.size(s->diagnostics)) {
      return NULL;
   }
   struct anvil_schema_diagnostic_t *d = NULL;
   List.get(s->diagnostics, index, (object *)&d);
   return (anvil_schema_diagnostic)d;
}

anvil_schema_diag_code anvil_schema_diagnostic_get_category(anvil_schema_diagnostic diag) {
   return diag ? diag->category : ANVIL_SCHEMA_DIAG_NONE;
}

anvil_type_diag_code anvil_schema_diagnostic_get_type_category(anvil_schema_diagnostic diag) {
   return diag ? diag->type_category : ANVIL_TYPE_DIAG_NONE;
}

// snprintf-style: always the full length, writes at most buflen - 1 characters plus the NUL.
static size_t copy_diag_text(const char *text, char *buf, size_t buflen) {
   size_t len = strlen(text);
   if (buf && buflen > 0) {
      size_t n = len < buflen - 1 ? len : buflen - 1;
      memcpy(buf, text, n);
      buf[n] = '\0';
   }
   return len;
}

size_t anvil_schema_diagnostic_get_field(anvil_schema_diagnostic diag, char *buf, size_t buflen) {
   return diag ? copy_diag_text(diag->field, buf, buflen) : 0;
}

size_t anvil_schema_diagnostic_get_message(anvil_schema_diagnostic diag, char *buf, size_t buflen) {
   return diag ? copy_diag_text(diag->message, buf, buflen) : 0;
}

/* ---------------------------------------------------------------------- *
 * Introspection (read-only)
 * ---------------------------------------------------------------------- */
static size_t copy_text(const char *text, char *buf, size_t buflen) {
   size_t len = strlen(text);
   if (buf && buflen > 0) {
      size_t n = len < buflen - 1 ? len : buflen - 1;
      memcpy(buf, text, n);
      buf[n] = '\0';
   }
   return len;
}

size_t anvil_schema_get_field_count(anvil_schema schema) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   return s && s->fields ? (size_t)List.size(s->fields) : 0;
}

anvil_schema_field anvil_schema_get_field(anvil_schema schema, size_t index) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   if (!s || !s->fields || index >= (size_t)List.size(s->fields)) {
      return NULL;
   }
   struct anvil_schema_field_t *f = NULL;
   List.get(s->fields, index, (object *)&f);
   return (anvil_schema_field)f;
}

anvil_schema_field anvil_schema_find_field(anvil_schema schema, const char *name) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   addr found = 0;
   if (!s || !name || !s->by_name || !Map.get(s->by_name, name, strlen(name), &found)) {
      return NULL;
   }
   return (anvil_schema_field)found;
}

size_t anvil_schema_field_get_name(anvil_schema_field field, char *buf, size_t buflen) {
   return field && field->name ? copy_text(field->name, buf, buflen) : 0;
}

anvil_type_kind anvil_schema_field_get_kind(anvil_schema_field field) {
   return field && field->has_kind ? field->kind : ANVIL_TYPE_UNKNOWN;
}

size_t anvil_schema_field_get_type_name(anvil_schema_field field, char *buf, size_t buflen) {
   return field && field->type_name ? copy_text(field->type_name, buf, buflen) : 0;
}

bool anvil_schema_field_is_required(anvil_schema_field field) { return field && field->required; }
bool anvil_schema_field_is_nullable(anvil_schema_field field) { return field && field->nullable; }
bool anvil_schema_field_is_integer(anvil_schema_field field) { return field && field->integer; }

bool anvil_schema_field_get_size(anvil_schema_field field, long long *out_size) {
   if (!field || !out_size || !field->has_size) {
      return false;
   }
   *out_size = field->size;
   return true;
}

bool anvil_schema_field_get_min_numeric(anvil_schema_field field, anvil_numeric_t *out_min) {
   if (!field || !out_min || !field->has_min) {
      return false;
   }
   *out_min = field->min;
   return true;
}

bool anvil_schema_field_get_max_numeric(anvil_schema_field field, anvil_numeric_t *out_max) {
   if (!field || !out_max || !field->has_max) {
      return false;
   }
   *out_max = field->max;
   return true;
}

bool anvil_schema_field_get_min_items(anvil_schema_field field, long long *out) {
   if (!field || !out || !field->has_min_items) {
      return false;
   }
   *out = field->min_items;
   return true;
}

bool anvil_schema_field_get_max_items(anvil_schema_field field, long long *out) {
   if (!field || !out || !field->has_max_items) {
      return false;
   }
   *out = field->max_items;
   return true;
}

size_t anvil_schema_field_get_value_count(anvil_schema_field field) {
   return field && field->values ? (size_t)List.size(field->values) : 0;
}

size_t anvil_schema_field_get_value(anvil_schema_field field, size_t index, char *buf,
                                    size_t buflen) {
   if (!field || !field->values || index >= (size_t)List.size(field->values)) {
      return 0;
   }
   char *label = NULL;
   List.get(field->values, index, (object *)&label);
   return label ? copy_text(label, buf, buflen) : 0;
}

size_t anvil_schema_field_get_format(anvil_schema_field field, char *buf, size_t buflen) {
   return field && field->format ? copy_text(field->format, buf, buflen) : 0;
}

size_t anvil_schema_field_get_tag(anvil_schema_field field, char *buf, size_t buflen) {
   return field && field->tag ? copy_text(field->tag, buf, buflen) : 0;
}

size_t anvil_schema_field_get_nested_count(anvil_schema_field field) {
   return field && field->children ? (size_t)List.size(field->children) : 0;
}

anvil_schema_field anvil_schema_field_get_nested(anvil_schema_field field, size_t index) {
   if (!field || !field->children || index >= (size_t)List.size(field->children)) {
      return NULL;
   }
   struct anvil_schema_field_t *c = NULL;
   List.get(field->children, index, (object *)&c);
   return (anvil_schema_field)c;
}

anvil_schema_field anvil_schema_field_find_nested(anvil_schema_field field, const char *name) {
   addr found = 0;
   if (!field || !name || !field->child_by_name ||
       !Map.get(field->child_by_name, name, strlen(name), &found)) {
      return NULL;
   }
   return (anvil_schema_field)found;
}

anvil_schema_field anvil_schema_field_get_items(anvil_schema_field field) {
   return field ? (anvil_schema_field)field->items : NULL;
}

size_t anvil_schema_field_get_extra_count(anvil_schema_field field) {
   return field && field->extras ? (size_t)List.size(field->extras) : 0;
}

static struct schema_extra *extra_at(anvil_schema_field field, size_t index) {
   if (!field || !field->extras || index >= (size_t)List.size(field->extras)) {
      return NULL;
   }
   struct schema_extra *e = NULL;
   List.get(field->extras, index, (object *)&e);
   return e;
}

size_t anvil_schema_field_get_extra_name(anvil_schema_field field, size_t index, char *buf,
                                         size_t buflen) {
   struct schema_extra *e = extra_at(field, index);
   return e ? copy_text(e->name, buf, buflen) : 0;
}

anvil_value anvil_schema_field_get_extra_value(anvil_schema_field field, size_t index) {
   struct schema_extra *e = extra_at(field, index);
   return e ? e->value : NULL;
}
