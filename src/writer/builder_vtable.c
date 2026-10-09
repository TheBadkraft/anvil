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
 * builder_vtable.c - Builder vtable (mirrors anvil_builder.h one-for-one) *
 * ********************************************************************** */

#include "anvil_builder.h"
#include "anvil_builder_vtable.h"

const anvil_builder_i Builder = {
   .create = anvil_builder_new,
   .dispose = anvil_builder_dispose,
   .get_error = anvil_builder_get_error,
   .attribute = anvil_builder_attribute,
   .attribute_string = anvil_builder_attribute_string,
   .include = anvil_builder_include,
   .null_value = anvil_builder_null,
   .bool_value = anvil_builder_bool,
   .numeric = anvil_builder_numeric,
   .int_value = anvil_builder_int,
   .double_value = anvil_builder_double,
   .string = anvil_builder_string,
   .bare = anvil_builder_bare,
   .blob = anvil_builder_blob,
   .varref = anvil_builder_varref,
   .array = anvil_builder_array,
   .tuple = anvil_builder_tuple,
   .object = anvil_builder_object,
   .append = anvil_builder_append,
   .add = anvil_builder_add,
   .set_base = anvil_builder_set_base,
   .member_attribute = anvil_builder_member_attribute,
   .member_attribute_string = anvil_builder_member_attribute_string,
   .find = anvil_builder_find,
   .write = anvil_builder_write,
   .emit = anvil_builder_emit,
};
