/*
 * schema.c — Anvil schema resolver and validator (v0.4.5-alpha)
 *
 * Resolver (Schema.resolve):
 *   Walk a schema document context that carries @[schema] module attribute.
 *   Classify each top-level statement as Object, Enum, or Flags, extracting
 *   field rules or allowed values.  Returns an anvl_schema_ruleset_t or NULL
 *   on error (anvl_error_set is called with the relevant 460x code).
 *
 * Validator (Schema.validate):
 *   Walk a data document.  For every top-level statement whose base
 *   identifier resolves to a known schema type, check:
 *     - Object: all declared fields must be present, each with the correct type.
 *     - Enum/Flags: no field-level check in v1.
 *   All errors in a document are collected before returning (no fail-fast).
 */

#include "schema.h"
#include "anvil.h"
#include "context.h"
#include "errors.h"
#include "types.h"
#include <sigma.memory/memory.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* §0  Span helpers                                                   */
/* ------------------------------------------------------------------ */

static bool span_eq(const char *raw, usize pos, usize len, const char *cmp) {
   usize clen = strlen(cmp);
   return len == clen && memcmp(raw + pos, cmp, len) == 0;
}

static char *span_dup(const char *raw, usize pos, usize len) {
   char *s = Allocator.alloc(len + 1);
   if (!s)
      return NULL;
   memcpy(s, raw + pos, len);
   s[len] = '\0';
   return s;
}

/* ------------------------------------------------------------------ */
/* §1  anvl_schema_type_t helpers                                     */
/* ------------------------------------------------------------------ */

static anvl_schema_type_t *stype_alloc(void) {
   anvl_schema_type_t *t = Allocator.alloc(sizeof(anvl_schema_type_t));
   if (!t)
      return NULL;
   t->name        = NULL;
   t->kind        = ANVL_SCHEMA_OBJECT;
   t->fields      = NULL;
   t->field_count = 0;
   t->values      = NULL;
   t->value_count = 0;
   return t;
}

static void stype_free(anvl_schema_type_t *t) {
   if (!t)
      return;
   Allocator.free(t->name);
   if (t->fields) {
      for (int i = 0; i < t->field_count; i++)
         Allocator.free(t->fields[i].name);
      Allocator.free(t->fields);
   }
   if (t->values) {
      for (int i = 0; i < t->value_count; i++)
         Allocator.free(t->values[i]);
      Allocator.free(t->values);
   }
   Allocator.free(t);
}

/* ------------------------------------------------------------------ */
/* §2  anvl_schema_ruleset_t helpers                                  */
/* ------------------------------------------------------------------ */

#define RULESET_INIT_CAP 8

static anvl_schema_ruleset_t *ruleset_alloc(void) {
   anvl_schema_ruleset_t *r = Allocator.alloc(sizeof(anvl_schema_ruleset_t));
   if (!r)
      return NULL;
   r->types = Allocator.alloc(sizeof(anvl_schema_type_t *) * RULESET_INIT_CAP);
   if (!r->types) {
      Allocator.free(r);
      return NULL;
   }
   memset(r->types, 0, sizeof(anvl_schema_type_t *) * RULESET_INIT_CAP);
   r->count    = 0;
   r->capacity = RULESET_INIT_CAP;
   return r;
}

static bool ruleset_push(anvl_schema_ruleset_t *r, anvl_schema_type_t *t) {
   if (r->count >= r->capacity) {
      int new_cap = r->capacity * 2;
      anvl_schema_type_t **a =
          Allocator.alloc(sizeof(anvl_schema_type_t *) * (usize)new_cap);
      if (!a)
         return false;
      memcpy(a, r->types, sizeof(anvl_schema_type_t *) * (usize)r->capacity);
      memset(a + r->capacity, 0,
             sizeof(anvl_schema_type_t *) * (usize)(new_cap - r->capacity));
      Allocator.free(r->types);
      r->types    = a;
      r->capacity = new_cap;
   }
   r->types[r->count++] = t;
   return true;
}

static void ruleset_free_impl(anvl_schema_ruleset_t *r) {
   if (!r)
      return;
   for (int i = 0; i < r->count; i++)
      stype_free(r->types[i]);
   Allocator.free(r->types);
   Allocator.free(r);
}

/* ------------------------------------------------------------------ */
/* §3  anvl_schema_result_t helpers                                   */
/* ------------------------------------------------------------------ */

#define RESULT_INIT_CAP 4

static anvl_schema_result_t *result_alloc(void) {
   anvl_schema_result_t *res = Allocator.alloc(sizeof(anvl_schema_result_t));
   if (!res)
      return NULL;
   res->errors =
       Allocator.alloc(sizeof(anvl_schema_error_t) * RESULT_INIT_CAP);
   if (!res->errors) {
      Allocator.free(res);
      return NULL;
   }
   res->is_valid       = true;
   res->error_count    = 0;
   res->error_capacity = RESULT_INIT_CAP;
   return res;
}

static bool result_push(anvl_schema_result_t *res, int code,
                         const char *msg, const char *file_path) {
   if (res->error_count >= res->error_capacity) {
      int new_cap = res->error_capacity * 2;
      anvl_schema_error_t *a =
          Allocator.alloc(sizeof(anvl_schema_error_t) * (usize)new_cap);
      if (!a)
         return false;
      memcpy(a, res->errors,
             sizeof(anvl_schema_error_t) * (usize)res->error_count);
      Allocator.free(res->errors);
      res->errors         = a;
      res->error_capacity = new_cap;
   }
   anvl_schema_error_t *e = &res->errors[res->error_count++];
   usize mlen    = strlen(msg);
   e->message    = Allocator.alloc(mlen + 1);
   if (e->message)
      memcpy(e->message, msg, mlen + 1);
   e->code      = code;
   e->file_path = file_path;
   res->is_valid = false;
   return true;
}

static void result_free_impl(anvl_schema_result_t *res) {
   if (!res)
      return;
   for (int i = 0; i < res->error_count; i++)
      Allocator.free(res->errors[i].message);
   Allocator.free(res->errors);
   Allocator.free(res);
}

/* ------------------------------------------------------------------ */
/* §4  Module attribute check                                         */
/* ------------------------------------------------------------------ */

static bool has_schema_attr(context ctx) {
   const char *raw = Source.data(ctx->source);
   usize n         = Context.attribute_count(ctx);
   for (usize i = 0; i < n; i++) {
      attribute a = Context.get_attribute(ctx, i);
      if (span_eq(raw, a->key_pos, a->key_len, "schema"))
         return true;
   }
   return false;
}

/* ------------------------------------------------------------------ */
/* §5  Schema.resolve                                                 */
/* ------------------------------------------------------------------ */

static anvl_schema_ruleset_t *schema_resolve(context schema_ctx) {
   const char *raw = Source.data(schema_ctx->source);

   if (!has_schema_attr(schema_ctx)) {
      anvl_error_set(ANVL_ERR_SCHEMA_ATTR_MISSING,
                     anvl_error_code_message(ANVL_ERR_SCHEMA_ATTR_MISSING),
                     0, 0, NULL);
      return NULL;
   }

   anvl_schema_ruleset_t *ruleset = ruleset_alloc();
   if (!ruleset)
      return NULL;

   usize stmt_count = Context.statement_count(schema_ctx);
   for (usize i = 0; i < stmt_count; i++) {
      statement stmt = Context.get_statement(schema_ctx, i);
      if (!stmt)
         continue;

      /* Skip anonymous statements */
      usize ident_len = stmt->meta[STMT_META_IDENT_LEN];
      if (ident_len == 0)
         continue;

      /* Base identifier */
      const char *base_ptr = NULL;
      usize       base_len = 0;
      if (stmt->base_meta) {
         base_ptr = raw + stmt->base_meta->pos;
         base_len = stmt->base_meta->len;
      }

      bool is_enum  = base_ptr && base_len == 4
                      && memcmp(base_ptr, "enum",  4) == 0;
      bool is_flags = base_ptr && base_len == 5
                      && memcmp(base_ptr, "flags", 5) == 0;

      /* Unknown non-virtual base → error */
      if (base_ptr && !is_enum && !is_flags) {
         char msg[512];
         char *type_name =
             span_dup(raw, stmt->meta[STMT_META_IDENT_POS], ident_len);
         char *base_name =
             span_dup(raw, stmt->base_meta->pos, base_len);
         snprintf(msg, sizeof(msg),
                  "Unknown schema base '%s' in type '%s'.",
                  base_name ? base_name : "?",
                  type_name ? type_name : "?");
         Allocator.free(type_name);
         Allocator.free(base_name);
         ruleset_free_impl(ruleset);
         anvl_error_set(ANVL_ERR_SCHEMA_BASE_UNKNOWN, msg, 0, 0, NULL);
         return NULL;
      }

      anvl_schema_type_t *t = stype_alloc();
      if (!t)
         break;
      t->name = span_dup(raw, stmt->meta[STMT_META_IDENT_POS], ident_len);

      if (is_enum || is_flags) {
         /* ── Enum / Flags ─────────────────────────────────────── */
         t->kind = is_enum ? ANVL_SCHEMA_ENUM : ANVL_SCHEMA_FLAGS;
         if (stmt->value_meta
             && stmt->value_meta->type == ANVL_VALUE_TUPLE) {
            usize ec = stmt->value_meta->data.collection.element_count;
            if (ec > 0) {
               t->values = Allocator.alloc(sizeof(char *) * ec);
               if (t->values) {
                  t->value_count = (int)ec;
                  for (usize k = 0; k < ec; k++) {
                     struct anvl_element_meta *em =
                         &stmt->value_meta->data.collection.elements[k];
                     t->values[k] = span_dup(raw, em->pos, em->len);
                  }
               }
            }
         }
      } else {
         /* ── Object ───────────────────────────────────────────── */
         t->kind = ANVL_SCHEMA_OBJECT;
         if (stmt->value_meta
             && stmt->value_meta->type == ANVL_VALUE_OBJECT) {
            usize fc = stmt->value_meta->data.object.field_count;
            usize fs = stmt->value_meta->data.object.field_start;
            if (fc > 0) {
               t->fields = Allocator.alloc(sizeof(anvl_field_rule_t) * fc);
               if (t->fields) {
                  t->field_count = (int)fc;
                  for (usize k = 0; k < fc; k++) {
                     field f = schema_ctx->field_list.fields[fs + k];
                     t->fields[k].name =
                         span_dup(raw, f->key_pos, f->key_len);
                     t->fields[k].expected_type =
                         f->val ? f->val->type : ANVL_VALUE_SCALAR;
                  }
               }
            }
         }
      }

      ruleset_push(ruleset, t);
   }

   return ruleset;
}

/* ------------------------------------------------------------------ */
/* §6  Schema.get_type                                                */
/* ------------------------------------------------------------------ */

static anvl_schema_type_t *schema_get_type(anvl_schema_ruleset_t *rules,
                                            const char            *name) {
   if (!rules || !name)
      return NULL;
   for (int i = 0; i < rules->count; i++) {
      if (rules->types[i] && strcmp(rules->types[i]->name, name) == 0)
         return rules->types[i];
   }
   return NULL;
}

/* ------------------------------------------------------------------ */
/* §7  Schema.validate helpers                                        */
/* ------------------------------------------------------------------ */

/* Scan a data statement's own object fields for a field with the given name.
 * Returns the field's value (struct anvl_value *), or NULL if not found.
 * Only covers the statement's directly declared fields (no inherited merge). */
static value find_own_field(context ctx, statement stmt,
                             const char *name, usize name_len) {
   if (!stmt->value_meta || stmt->value_meta->type != ANVL_VALUE_OBJECT)
      return NULL;
   const char *raw = Source.data(ctx->source);
   usize fs        = stmt->value_meta->data.object.field_start;
   /* Scan from field_start to the end of the field_list.  The parser
      allocates nested object fields before their parent field in the flat
      list (depth-first ordering), so the range [field_start, field_start +
      field_count) may begin with inner-object fields rather than the
      statement's own direct fields.  Scanning to field_list.count (bounded
      below by field_start) ensures we reach every direct field regardless of
      nesting depth while still ignoring fields that belong to earlier
      statements (which occupy indices below field_start). */
   for (usize k = fs; k < ctx->field_list.count; k++) {
      field f = ctx->field_list.fields[k];
      if (f->key_len == name_len
          && memcmp(raw + f->key_pos, name, name_len) == 0)
         return f->val;
   }
   return NULL;
}

static void validate_object(context data_ctx, statement stmt,
                             anvl_schema_type_t *schema_type,
                             const char         *file_path,
                             anvl_schema_result_t *result) {
   const char *raw      = Source.data(data_ctx->source);
   usize ident_len      = stmt->meta[STMT_META_IDENT_LEN];
   char *stmt_name =
       span_dup(raw, stmt->meta[STMT_META_IDENT_POS], ident_len);

   for (int r = 0; r < schema_type->field_count; r++) {
      const char *fname = schema_type->fields[r].name;
      usize fname_len   = strlen(fname);
      value found       = find_own_field(data_ctx, stmt, fname, fname_len);

      if (!found) {
         char msg[512];
         snprintf(msg, sizeof(msg),
                  "Required field '%s' is missing in '%s' (type '%s').",
                  fname,
                  stmt_name ? stmt_name : "?",
                  schema_type->name);
         result_push(result, ANVL_ERR_SCHEMA_VALIDATION_REQUIRED,
                     msg, file_path);
         continue;
      }

      if (found->type != schema_type->fields[r].expected_type) {
         char msg[512];
         snprintf(msg, sizeof(msg),
                  "Field '%s' in '%s' expected type %d but got %d.",
                  fname,
                  stmt_name ? stmt_name : "?",
                  (int)schema_type->fields[r].expected_type,
                  (int)found->type);
         result_push(result, ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH,
                     msg, file_path);
      }
   }

   Allocator.free(stmt_name);
}

/* ------------------------------------------------------------------ */
/* §8  Schema.validate                                                */
/* ------------------------------------------------------------------ */

static anvl_schema_result_t *schema_validate(anvl_schema_ruleset_t *rules,
                                              context               data_ctx,
                                              const char            *file_path) {
   anvl_schema_result_t *result = result_alloc();
   if (!result)
      return NULL;

   const char *raw      = Source.data(data_ctx->source);
   usize stmt_count     = Context.statement_count(data_ctx);

   for (usize i = 0; i < stmt_count; i++) {
      statement stmt = Context.get_statement(data_ctx, i);
      if (!stmt || !stmt->base_meta)
         continue;

      usize base_len = stmt->base_meta->len;
      if (base_len == 0)
         continue;

      char *base_name = span_dup(raw, stmt->base_meta->pos, base_len);
      if (!base_name)
         continue;
      anvl_schema_type_t *schema_type = schema_get_type(rules, base_name);
      Allocator.free(base_name);

      if (!schema_type)
         continue; /* unrecognised base → pass through */

      if (schema_type->kind == ANVL_SCHEMA_OBJECT)
         validate_object(data_ctx, stmt, schema_type, file_path, result);
      /* Enum / Flags: no field-level validation in v1 */
   }

   return result;
}

/* ------------------------------------------------------------------ */
/* §9  Vtable                                                         */
/* ------------------------------------------------------------------ */

const anvl_schema_i Schema = {
    .resolve      = schema_resolve,
    .validate     = schema_validate,
    .get_type     = schema_get_type,
    .ruleset_free = ruleset_free_impl,
    .result_free  = result_free_impl,
};
