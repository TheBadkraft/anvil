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
 * anvil_schema_vtable.h - the `Schema` vtable (AnvilSchema)
 * ---------------------------------------------------------------------- *
 * Mirrors anvil_schema.h one-for-one, same convention as
 * anvil_vtable.h and anvil_writer_vtable.h: every field is the exact same
 * function as its flat counterpart (pointer identity, tested), so a binding
 * that binds this one struct can never drift from the flat API. Includes
 * only anvil_schema.h. Member name = the flat name without its `anvil_schema_` prefix.
 * Members are in the flat header's declaration order; a function added to
 * the flat header later is APPENDED here, never inserted, so a binding's
 * mirrored layout stays valid. The completeness test fails if one is missed.
 * ********************************************************************** */
#pragma once

#include "anvil_schema.h"

// See anvil_flat.h's own comment: the library compiles with -fvisibility=hidden, so this
// pragma keeps the vtable global below exported. It must also be listed in libanvil.exports.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

typedef struct anvil_schema_i {
   anvil_schema (*load)(anvil_document doc);
   void (*dispose)(anvil_schema schema);
   bool (*validate)(anvil_schema schema, anvil_document data_doc);
   bool (*validate_value)(anvil_schema schema, anvil_value object_value, const anvil_schema_options *opts);
   size_t (*get_violation_count)(anvil_schema schema);
   anvil_schema_violation (*get_violation)(anvil_schema schema, size_t index);
   anvil_schema_err_code (*violation_get_category)(anvil_schema_violation v);
   size_t (*violation_get_field)(anvil_schema_violation v, char *buf, size_t buflen);
   size_t (*violation_get_message)(anvil_schema_violation v, char *buf, size_t buflen);
   size_t (*get_diagnostic_count)(anvil_schema schema);
   anvil_schema_diagnostic (*get_diagnostic)(anvil_schema schema, size_t index);
   anvil_schema_diag_code (*diagnostic_get_category)(anvil_schema_diagnostic diag);
   anvil_type_diag_code (*diagnostic_get_type_category)(anvil_schema_diagnostic diag);
   size_t (*diagnostic_get_field)(anvil_schema_diagnostic diag, char *buf, size_t buflen);
   size_t (*diagnostic_get_message)(anvil_schema_diagnostic diag, char *buf, size_t buflen);
   size_t (*get_field_count)(anvil_schema schema);
   anvil_schema_field (*get_field)(anvil_schema schema, size_t index);
   anvil_schema_field (*find_field)(anvil_schema schema, const char *name);
   size_t (*field_get_name)(anvil_schema_field field, char *buf, size_t buflen);
   anvil_type_kind (*field_get_kind)(anvil_schema_field field);
   size_t (*field_get_type_name)(anvil_schema_field field, char *buf, size_t buflen);
   bool (*field_is_required)(anvil_schema_field field);
   bool (*field_is_nullable)(anvil_schema_field field);
   bool (*field_is_integer)(anvil_schema_field field);
   bool (*field_get_size)(anvil_schema_field field, long long *out_size);
   bool (*field_get_min_numeric)(anvil_schema_field field, anvil_numeric_t *out_min);
   bool (*field_get_max_numeric)(anvil_schema_field field, anvil_numeric_t *out_max);
   bool (*field_get_min_items)(anvil_schema_field field, long long *out_min_items);
   bool (*field_get_max_items)(anvil_schema_field field, long long *out_max_items);
   size_t (*field_get_value_count)(anvil_schema_field field);
   size_t (*field_get_value)(anvil_schema_field field, size_t index, char *buf, size_t buflen);
   size_t (*field_get_format)(anvil_schema_field field, char *buf, size_t buflen);
   size_t (*field_get_tag)(anvil_schema_field field, char *buf, size_t buflen);
   size_t (*field_get_nested_count)(anvil_schema_field field);
   anvil_schema_field (*field_get_nested)(anvil_schema_field field, size_t index);
   anvil_schema_field (*field_find_nested)(anvil_schema_field field, const char *name);
   anvil_schema_field (*field_get_items)(anvil_schema_field field);
   size_t (*field_get_extra_count)(anvil_schema_field field);
   size_t (*field_get_extra_name)(anvil_schema_field field, size_t index, char *buf, size_t buflen);
   anvil_value (*field_get_extra_value)(anvil_schema_field field, size_t index);
} anvil_schema_i;
extern const anvil_schema_i Schema;

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif
