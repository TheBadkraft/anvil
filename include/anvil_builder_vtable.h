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
 * anvil_builder_vtable.h - Public ABI: vtable layer for the builder      *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Mirrors anvil_builder.h one-for-one, same convention as                *
 * anvil_vtable.h and anvil_writer_vtable.h: every field is the exact     *
 * same function as its flat counterpart (pointer identity, tested in     *
 * test_builder.c). Includes only the types headers, never anvil_builder.h.*
 * ********************************************************************** */
#pragma once

#include "anvil_builder_types.h"
#include "anvil_writer_types.h"

typedef struct anvil_builder_i {
   anvil_builder (*create)(anvil_writer_dialect dialect);
   void (*dispose)(anvil_builder b);
   anvil_writer_err_code (*get_error)(anvil_builder b);
   bool (*attribute)(anvil_builder b, const char *key, const char *value);
   bool (*attribute_string)(anvil_builder b, const char *key, const char *text, size_t length);
   bool (*include)(anvil_builder b, const char *path);
   anvil_node (*null_value)(anvil_builder b);
   anvil_node (*bool_value)(anvil_builder b, bool value);
   anvil_node (*numeric)(anvil_builder b, const char *text);
   anvil_node (*int_value)(anvil_builder b, int64_t value);
   anvil_node (*double_value)(anvil_builder b, double value);
   anvil_node (*string)(anvil_builder b, const char *text, size_t length);
   anvil_node (*bare)(anvil_builder b, const char *text);
   anvil_node (*blob)(anvil_builder b, const char *tag, const char *data, size_t length);
   anvil_node (*varref)(anvil_builder b, const char *name);
   anvil_node (*array)(anvil_builder b);
   anvil_node (*tuple)(anvil_builder b);
   anvil_node (*object)(anvil_builder b);
   bool (*append)(anvil_builder b, anvil_node collection, anvil_node element);
   anvil_member (*add)(anvil_builder b, anvil_node object, const char *name, anvil_node value);
   bool (*set_base)(anvil_builder b, anvil_member member, const char *base);
   bool (*member_attribute)(anvil_builder b, anvil_member member, const char *key, const char *value);
   bool (*member_attribute_string)(anvil_builder b, anvil_member member, const char *key,
                                   const char *text, size_t length);
   anvil_member (*find)(anvil_builder b, anvil_node object, const char *name);
   bool (*write)(anvil_builder b, anvil_writer w);
   const char *(*emit)(anvil_builder b, size_t *length);
} anvil_builder_i;
extern const anvil_builder_i Builder;
