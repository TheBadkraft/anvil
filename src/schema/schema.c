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
// ----------------
#include <sigma/allocator.h>
#include <sigma/list.h>
#include <sigma/map.h>
#include <string.h>

#define ANVIL_SCHEMA_NAME_MAX 65
#define ANVIL_SCHEMA_MSG_MAX 128

struct anvil_schema_field_t {
   char name[ANVIL_SCHEMA_NAME_MAX];
   bool has_kind;
   anvil_type_kind kind;
   bool required;
};

struct anvil_schema_violation_t {
   char field[ANVIL_SCHEMA_NAME_MAX];
   anvil_schema_err_code category;
   char message[ANVIL_SCHEMA_MSG_MAX];
};

struct anvil_schema_t {
   list fields;      // struct anvil_schema_field_t* — every declared field rule, in order
   map by_name;      // field->name -> struct anvil_schema_field_t* (presence/lookup)
   list violations;  // struct anvil_schema_violation_t* — from the most recent validate() call
};

// A schema field's declared kind is transparent-through: a bare native primitive, a bare
// `enum`, or a types.X custom type all resolve to the same anvil_type_kind, so validation
// never needs to know which of the three produced it — see anvil_type_resolve.
static void read_field_rule(anvil_value obj_val, anvil_type_registry types,
                            struct anvil_schema_field_t *field) {
   field->has_kind = false;
   field->kind = ANVIL_TYPE_UNKNOWN;
   field->required = false;

   size_t count = anvil_value_get_count(obj_val);
   for (size_t i = 0; i < count; i++) {
      anvil_statement fstmt = anvil_value_get_statement(obj_val, i);
      char fname[16] = {0};
      anvil_statement_get_name(fstmt, fname, sizeof(fname));
      anvil_value fval = anvil_statement_get_value(fstmt);

      if (strcmp(fname, "type") == 0) {
         char text[64] = {0};
         anvil_value_get_text(fval, text, sizeof(text));
         anvil_type_def def = anvil_type_resolve(types, text);
         if (def) {
            field->has_kind = true;
            field->kind = anvil_type_def_get_kind(def);
         }
         // An unrecognized type name is silently left with no kind to check against — same
         // deferred-validation-error reasoning as anvil_types.c's own malformed-declaration
         // handling; no error-reporting design exists yet for a malformed *schema* field
         // itself (as opposed to a data document failing to satisfy a well-formed one).
      } else if (strcmp(fname, "required") == 0) {
         char text[8] = {0};
         anvil_value_get_text(fval, text, sizeof(text));
         field->required = (strcmp(text, "true") == 0);
      }
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

   // types. access comes purely from doc's own imports — @[schema] never implies it, matching
   // how any other document would resolve types.X. Only needed for the duration of the load
   // pass itself: each field's resolved *kind* is copied out below, so the registry doesn't
   // need to outlive this function.
   anvil_type_registry types = anvil_type_registry_load_from_imports(doc);

   anvil_statement_iterator it = anvil_document_get_statements(doc);
   if (it) {
      anvil_statement stmt = NULL;
      while (anvil_statement_iterator_next(it, &stmt)) {
         anvil_value val = anvil_statement_get_value(stmt);
         if (!val || anvil_value_get_type(val) != ANVIL_VALUE_OBJECT) {
            continue; // not a well-formed field rule — skip, same reasoning as anvil_types.c
         }
         struct anvil_schema_field_t *field =
            Allocator.alloc(sizeof(struct anvil_schema_field_t));
         if (!field) {
            continue;
         }
         anvil_statement_get_name(stmt, field->name, sizeof(field->name));
         read_field_rule(val, types, field);

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
      Allocator.dispose(v);
   }
   List.dispose(violations);
}

void anvil_schema_dispose(anvil_schema schema) {
   struct anvil_schema_t *s = (struct anvil_schema_t *)schema;
   if (!s) {
      return;
   }
   dispose_violations(s->violations);
   if (s->fields) {
      usize n = List.size(s->fields);
      for (usize i = 0; i < n; i++) {
         struct anvil_schema_field_t *f = NULL;
         List.get(s->fields, i, (object *)&f);
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
      return actual == ANVIL_VALUE_STRING || actual == ANVIL_VALUE_IDENTIFIER;
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
   size_t flen = strlen(field_name);
   if (flen >= sizeof(v->field)) {
      flen = sizeof(v->field) - 1;
   }
   memcpy(v->field, field_name, flen);
   v->field[flen] = '\0';
   v->category = cat;
   size_t mlen = strlen(message);
   if (mlen >= sizeof(v->message)) {
      mlen = sizeof(v->message) - 1;
   }
   memcpy(v->message, message, mlen);
   v->message[mlen] = '\0';
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

      anvil_statement data_stmt = anvil_statement_get(data_doc, field->name);
      if (!data_stmt) {
         if (field->required) {
            add_violation(s->violations, field->name, ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,
                          "required field is missing");
         }
         continue;
      }

      if (field->has_kind) {
         anvil_value data_val = anvil_statement_get_value(data_stmt);
         anvil_value_type actual = data_val ? anvil_value_get_type(data_val) : ANVIL_VALUE_NULL;
         if (!kind_matches(field->kind, actual)) {
            add_violation(s->violations, field->name, ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                          "field value does not match the declared type");
         }
      }
   }

   // Every data field not declared in the schema is a violation of its own — checked as a
   // second pass over the data document's own statements, independent of the pass above.
   anvil_statement_iterator dit = anvil_document_get_statements(data_doc);
   if (dit) {
      anvil_statement dstmt = NULL;
      while (anvil_statement_iterator_next(dit, &dstmt)) {
         char dname[ANVIL_SCHEMA_NAME_MAX] = {0};
         anvil_statement_get_name(dstmt, dname, sizeof(dname));
         addr unused = 0;
         if (!Map.get(s->by_name, dname, strlen(dname), &unused)) {
            add_violation(s->violations, dname, ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD,
                          "field is not declared in the schema");
         }
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
