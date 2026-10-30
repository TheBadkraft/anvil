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
 * anvil_type_registry_vtable.h - the `TypeRegistry` vtable (AnvilTypes)
 * ---------------------------------------------------------------------- *
 * Mirrors anvil_type_registry.h one-for-one, same convention as
 * anvil_vtable.h and anvil_writer_vtable.h: every field is the exact same
 * function as its flat counterpart (pointer identity, tested), so a binding
 * that binds this one struct can never drift from the flat API. Includes
 * only anvil_type_registry.h. Member name = the flat name without its `anvil_type_registry_` prefix, or else without `anvil_type_` (`def_get_kind`, `diagnostic_get_name`, `resolve`).
 * Members are in the flat header's declaration order; a function added to
 * the flat header later is APPENDED here, never inserted, so a binding's
 * mirrored layout stays valid. The completeness test fails if one is missed.
 * ********************************************************************** */
#pragma once

#include "anvil_type_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// See anvil_flat.h's own comment: the library compiles with -fvisibility=hidden, so this
// pragma keeps the vtable global below exported. It must also be listed in libanvil.exports.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

typedef struct anvil_type_registry_i {
   anvil_type_registry (*load)(anvil_document doc);
   anvil_type_registry (*load_from_includes)(anvil_document doc);
   void (*dispose)(anvil_type_registry reg);
   anvil_type_def (*find)(anvil_type_registry reg, const char *name);
   anvil_type_def (*resolve)(anvil_type_registry reg, const char *name);
   anvil_type_kind (*def_get_kind)(anvil_type_def def);
   bool (*def_get_size)(anvil_type_def def, long long *out_size);
   bool (*def_get_min)(anvil_type_def def, long long *out_min);
   bool (*def_get_min_numeric)(anvil_type_def def, anvil_numeric_t *out_min);
   bool (*def_get_max)(anvil_type_def def, long long *out_max);
   bool (*def_get_max_numeric)(anvil_type_def def, anvil_numeric_t *out_max);
   size_t (*def_get_format)(anvil_type_def def, char *buf, size_t buflen);
   size_t (*def_get_value_count)(anvil_type_def def);
   size_t (*def_get_value)(anvil_type_def def, size_t index, char *buf, size_t buflen);
   size_t (*get_diagnostic_count)(anvil_type_registry reg);
   anvil_type_diagnostic (*get_diagnostic)(anvil_type_registry reg, size_t index);
   anvil_type_diag_code (*diagnostic_get_category)(anvil_type_diagnostic diag);
   size_t (*diagnostic_get_name)(anvil_type_diagnostic diag, char *buf, size_t buflen);
   size_t (*diagnostic_get_source)(anvil_type_diagnostic diag, char *buf, size_t buflen);
   size_t (*diagnostic_get_message)(anvil_type_diagnostic diag, char *buf, size_t buflen);
} anvil_type_registry_i;
extern const anvil_type_registry_i TypeRegistry;

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif
