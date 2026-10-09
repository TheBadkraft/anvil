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
 * anvil_writer_vtable.h - Public ABI: vtable layer for the writer        *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Mirrors anvil_writer.h one-for-one, same convention as                 *
 * anvil_vtable.h: every field is the exact same function as its flat     *
 * counterpart (pointer identity, tested in test_writer.c), so a binding  *
 * that binds this one struct can never drift from the flat API. Includes *
 * only anvil_writer_types.h, never anvil_writer.h.                       *
 * ********************************************************************** */
#pragma once

#include "anvil_writer_types.h"

typedef struct anvil_writer_i {
   anvil_writer (*create)(anvil_writer_dialect dialect);
   void (*dispose)(anvil_writer w);
   anvil_writer_err_code (*get_error)(anvil_writer w);
   const char *(*error_message)(anvil_writer_err_code code);
   bool (*attribute)(anvil_writer w, const char *key, const char *value);
   bool (*attribute_string)(anvil_writer w, const char *key, const char *text, size_t length);
   bool (*include)(anvil_writer w, const char *path);
   bool (*statement)(anvil_writer w, const char *name, const char *base);
   bool (*null_value)(anvil_writer w);
   bool (*bool_value)(anvil_writer w, bool value);
   bool (*numeric)(anvil_writer w, const char *text);
   bool (*int_value)(anvil_writer w, int64_t value);
   bool (*double_value)(anvil_writer w, double value);
   bool (*string)(anvil_writer w, const char *text, size_t length);
   bool (*bare)(anvil_writer w, const char *text);
   bool (*blob)(anvil_writer w, const char *tag, const char *data, size_t length);
   bool (*varref)(anvil_writer w, const char *name);
   bool (*begin_array)(anvil_writer w);
   bool (*end_array)(anvil_writer w);
   bool (*begin_tuple)(anvil_writer w);
   bool (*end_tuple)(anvil_writer w);
   bool (*begin_object)(anvil_writer w);
   bool (*end_object)(anvil_writer w);
   bool (*finish)(anvil_writer w);
   const char *(*data)(anvil_writer w, size_t *length);
} anvil_writer_i;
extern const anvil_writer_i Writer;
