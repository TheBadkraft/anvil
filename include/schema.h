/*
 * schema.h — Public API for Anvil schema validation (v0.4.5-alpha)
 *
 * Provides:
 *   - anvl_schema_type_kind_t  — Object / Enum / Flags discriminator
 *   - anvl_field_rule_t        — Single declared field within an Object type
 *   - anvl_schema_type_t       — One named type from a schema document
 *   - anvl_schema_ruleset_t    — Complete set of types from a schema document
 *   - anvl_schema_error_t      — Single validation error record
 *   - anvl_schema_result_t     — Validation result (errors collected, not fail-fast)
 *   - anvl_schema_i            — Vtable: Schema.resolve / Schema.validate / free
 *
 * Usage:
 *   anvl_schema_ruleset_t *rules = Schema.resolve(schema_ctx);
 *   if (!rules) { ... anvl_error_get() ... }
 *   anvl_schema_result_t *result = Schema.validate(rules, data_ctx, NULL);
 *   if (!result->is_valid) { ... result->errors[i] ... }
 *   Schema.result_free(result);
 *   Schema.ruleset_free(rules);
 */

#pragma once

#include "context.h"
#include "types.h"
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Schema Type Kind                                                   */
/* ------------------------------------------------------------------ */

typedef enum {
   ANVL_SCHEMA_OBJECT = 0, /* structured type with named fields             */
   ANVL_SCHEMA_ENUM   = 1, /* closed set of scalar identifier values        */
   ANVL_SCHEMA_FLAGS  = 2, /* composable set of flag identifier values      */
} anvl_schema_type_kind_t;

/* ------------------------------------------------------------------ */
/* Field Rule (one declared field within an Object schema type)       */
/* ------------------------------------------------------------------ */

typedef struct {
   char           *name;          /* heap-allocated, owned by schema_type  */
   anvl_value_type expected_type; /* SCALAR, OBJECT, ARRAY, TUPLE, BLOB, … */
} anvl_field_rule_t;

/* ------------------------------------------------------------------ */
/* Schema Type (one named type declaration from a schema document)    */
/* ------------------------------------------------------------------ */

typedef struct {
   char                    *name;        /* heap-allocated type name            */
   anvl_schema_type_kind_t  kind;        /* OBJECT, ENUM, or FLAGS              */
   /* Object types: fields array */
   anvl_field_rule_t       *fields;      /* NULL for Enum/Flags                 */
   int                      field_count;
   /* Enum/Flags types: values array (allowed identifier strings) */
   char                   **values;      /* NULL for Object                     */
   int                      value_count;
} anvl_schema_type_t;

/* ------------------------------------------------------------------ */
/* Schema Ruleset (all types from one schema document)                */
/* ------------------------------------------------------------------ */

typedef struct {
   anvl_schema_type_t **types;    /* flat array; NULL entries are hash holes  */
   int                  count;    /* number of live types                     */
   int                  capacity; /* allocated slots (power of 2)             */
} anvl_schema_ruleset_t;

/* ------------------------------------------------------------------ */
/* Schema Validation Error                                            */
/* ------------------------------------------------------------------ */

typedef struct {
   int         code;       /* ANVL_ERR_SCHEMA_VALIDATION_* code              */
   char       *message;    /* heap-allocated, owned by schema_result         */
   const char *file_path;  /* optional, NOT owned (points to caller string)  */
} anvl_schema_error_t;

/* ------------------------------------------------------------------ */
/* Schema Validation Result                                           */
/* ------------------------------------------------------------------ */

typedef struct {
   bool                  is_valid;
   anvl_schema_error_t  *errors;         /* heap-allocated array               */
   int                   error_count;
   int                   error_capacity; /* internal: allocated slots          */
} anvl_schema_result_t;

/* ------------------------------------------------------------------ */
/* Schema Vtable                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
   /*
    * resolve — Walk a schema document context and extract type rules.
    *   schema_ctx  — must have been parsed; must carry @[schema] module attr.
    *   Returns NULL on error (anvl_error_get() has the code/message).
    *   Caller must call ruleset_free() when done.
    */
   anvl_schema_ruleset_t *(*resolve)(context schema_ctx);

   /*
    * validate — Walk a data document context and check against rules.
    *   rules     — produced by resolve().
    *   data_ctx  — parsed data document.
    *   file_path — optional; attached to each error for diagnostics. May be NULL.
    *   Returns a heap-allocated result; caller must call result_free().
    *   Never returns NULL — returns an is_valid=true result on success.
    */
   anvl_schema_result_t *(*validate)(anvl_schema_ruleset_t *rules,
                                     context               data_ctx,
                                     const char            *file_path);

   /* look up a type by name; returns NULL if not found */
   anvl_schema_type_t *(*get_type)(anvl_schema_ruleset_t *rules, const char *name);

   /* free all memory owned by a ruleset produced by resolve() */
   void (*ruleset_free)(anvl_schema_ruleset_t *rules);

   /* free all memory owned by a result produced by validate() */
   void (*result_free)(anvl_schema_result_t *result);

} anvl_schema_i;

extern const anvl_schema_i Schema;
