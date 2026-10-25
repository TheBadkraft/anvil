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
#include "internal/numeric_bound.h"
#include "internal/text_alloc.h"
// ----------------
#include <sigma.core/allocator.h>
#include <sigma/list.h>
#include <sigma/map.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct anvil_schema_field_t {
   char *name; // allocated to its full length
   bool has_kind;
   anvil_type_kind kind;
   bool required;
   bool has_size;
   long long size;
   bool has_min;
   anvil_numeric_t min;
   bool has_max;
   anvil_numeric_t max;
   list values; // list of individually-allocated char* labels; NULL if no values constraint
                // (own inline declaration or inherited from a resolved custom type)
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

// A schema field's declared kind is transparent-through: a bare native primitive, a bare
// `enum`, or a types.X custom type all resolve to the same anvil_type_kind, so validation
// never needs to know which of the three produced it — see anvil_type_resolve.
static void read_field_rule(anvil_value obj_val, anvil_type_registry types,
                            struct anvil_schema_field_t *field, struct anvil_schema_t *schema) {
   field->has_kind = false;
   field->kind = ANVIL_TYPE_UNKNOWN;
   field->required = false;
   field->has_size = false;
   field->has_min = false;
   field->has_max = false;
   field->values = NULL;

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
         resolved = text ? anvil_type_resolve(types, strip_types_prefix(text)) : NULL;
         if (!resolved) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_UNKNOWN_TYPE, ANVIL_TYPE_DIAG_NONE, field->name,
                     "field '%s': type '%s' is neither a native type nor a known types.X",
                     field->name, text ? text : "");
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
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE,
                     field->name, "field '%s': 'required' must be true or false", field->name);
         }
         Allocator.dispose(text);
      } else if (strcmp(fname, "size") == 0) {
         field->has_size = anvil_numeric_read_size(fval, &field->size);
         if (!field->has_size) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE,
                     field->name,
                     "field '%s': 'size' is not a non-negative integer; treated as not declared",
                     field->name);
         }
      } else if (strcmp(fname, "min") == 0) {
         field->has_min = anvil_numeric_read_bound(fval, &field->min);
         if (!field->has_min) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE,
                     field->name, "field '%s': 'min' is not a valid number; treated as not declared",
                     field->name);
         }
      } else if (strcmp(fname, "max") == 0) {
         field->has_max = anvil_numeric_read_bound(fval, &field->max);
         if (!field->has_max) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE,
                     field->name, "field '%s': 'max' is not a valid number; treated as not declared",
                     field->name);
         }
      } else if (strcmp(fname, "values") == 0) {
         field->values = read_values_list(fval);
         if (!field->values) {
            add_diag(schema, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, ANVIL_TYPE_DIAG_NONE,
                     field->name, "field '%s': 'values' must be an array of labels", field->name);
         }
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
         field->has_min = anvil_type_def_numeric_min(resolved, &field->min);
      }
      if (!field->has_max) {
         field->has_max = anvil_type_def_numeric_max(resolved, &field->max);
      }
      if (!field->values) {
         field->values = copy_values_from_type(resolved);
      }
   }

   // Effective (inline or inherited) bounds that nothing can satisfy.
   if (field->has_min && field->has_max && anvil_numeric_compare_bounds(field->min, field->max) > 0) {
      add_diag(schema, ANVIL_SCHEMA_DIAG_CONFLICTING_CONSTRAINTS, ANVIL_TYPE_DIAG_NONE,
               field->name, "field '%s': 'min' is greater than 'max'; no value can satisfy it",
               field->name);
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
         struct anvil_schema_field_t *field =
            Allocator.alloc(sizeof(struct anvil_schema_field_t));
         if (!field) {
            continue;
         }
         field->name = anvil_name_alloc(stmt);
         if (!field->name) {
            Allocator.dispose(field);
            continue;
         }
         read_field_rule(val, types, field, schema);

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
         if (f) {
            dispose_field_values(f->values);
         }
         if (f) {
            Allocator.dispose(f->name);
         }
         Allocator.dispose(f);
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
   case ANVIL_TYPE_ENUM:
      return actual == ANVIL_VALUE_STRING || actual == ANVIL_VALUE_BARE;
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

bool anvil_schema_validate(anvil_schema schema, anvil_document data_doc) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   if (!s || !data_doc) {
      return false;
   }

   dispose_violations(s->violations);
   s->violations = List.new(4, sizeof(struct anvil_schema_violation_t *));

   usize field_count = List.size(s->fields);
   for (usize i = 0; i < field_count; i++) {
      struct anvil_schema_field_t *field = NULL;
      List.get(s->fields, i, (object *)&field);
      if (!field) {
         continue;
      }

      anvil_statement data_stmt = anvil_document_find_statement(data_doc, field->name);
      if (!data_stmt) {
         if (field->required) {
            add_violation(s->violations, field->name, ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,
                          "required field is missing");
         }
         continue;
      }

      anvil_value data_val = anvil_statement_get_value(data_stmt);
      anvil_value_type actual = data_val ? anvil_value_get_type(data_val) : ANVIL_VALUE_NULL;

      if (field->has_kind && !kind_matches(field->kind, actual)) {
         add_violation(s->violations, field->name, ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                       "field value does not match the declared type");
         continue; // a value of the wrong kind can't be meaningfully checked against
                   // size/min/max/values too — one violation, not a confusing pile of them
      }

      if (field->has_size && data_val) {
         size_t len = anvil_value_get_text(data_val, NULL, 0);
         if ((long long)len > field->size) {
            add_violation(s->violations, field->name, ANVIL_SCHEMA_ERR_VALIDATION_SIZE,
                          "field value exceeds the declared size");
         }
      }
      if ((field->has_min || field->has_max) && data_val && actual == ANVIL_VALUE_NUMERIC) {
         if ((field->has_min && anvil_numeric_compare(data_val, field->min) < 0) ||
             (field->has_max && anvil_numeric_compare(data_val, field->max) > 0)) {
            add_violation(s->violations, field->name, ANVIL_SCHEMA_ERR_VALIDATION_RANGE,
                          "field value is outside the declared min/max range");
         }
      }
      if (field->values && data_val) {
         char *text = anvil_text_alloc(data_val);
         bool member = false;
         usize value_count = List.size(field->values);
         for (usize vi = 0; vi < value_count; vi++) {
            char *label = NULL;
            List.get(field->values, vi, (object *)&label);
            if (label && text && strcmp(label, text) == 0) {
               member = true;
               break;
            }
         }
         Allocator.dispose(text);
         if (!member) {
            add_violation(s->violations, field->name, ANVIL_SCHEMA_ERR_VALIDATION_VALUES,
                          "field value is not one of the declared values");
         }
      }
   }

   // Every data field not declared in the schema is a violation of its own — checked as a
   // second pass over the data document's own statements, independent of the pass above.
   anvil_statement_iterator dit = anvil_document_get_statements(data_doc);
   if (dit) {
      anvil_statement dstmt = NULL;
      while (anvil_statement_iterator_next(dit, &dstmt)) {
         char *dname = anvil_name_alloc(dstmt);
         if (!dname) {
            continue;
         }
         addr unused = 0;
         if (!Map.get(s->by_name, dname, strlen(dname), &unused)) {
            add_violation(s->violations, dname, ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD,
                          "field is not declared in the schema");
         }
         Allocator.dispose(dname);
      }
      anvil_statement_iterator_dispose(dit);
   }

   return List.size(s->violations) == 0;
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
