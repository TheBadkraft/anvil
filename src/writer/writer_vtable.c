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
 * writer_vtable.c - Writer vtable (mirrors anvil_writer.h one-for-one)   *
 * ********************************************************************** */

#include "anvil_writer.h"
#include "anvil_writer_vtable.h"

const anvil_writer_i Writer = {
   .create = anvil_writer_new,
   .dispose = anvil_writer_dispose,
   .get_error = anvil_writer_get_error,
   .error_message = anvil_writer_error_message,
   .attribute = anvil_writer_attribute,
   .attribute_string = anvil_writer_attribute_string,
   .include = anvil_writer_include,
   .statement = anvil_writer_statement,
   .null_value = anvil_writer_null,
   .bool_value = anvil_writer_bool,
   .numeric = anvil_writer_numeric,
   .int_value = anvil_writer_int,
   .double_value = anvil_writer_double,
   .string = anvil_writer_string,
   .bare = anvil_writer_bare,
   .blob = anvil_writer_blob,
   .varref = anvil_writer_varref,
   .begin_array = anvil_writer_begin_array,
   .end_array = anvil_writer_end_array,
   .begin_tuple = anvil_writer_begin_tuple,
   .end_tuple = anvil_writer_end_tuple,
   .begin_object = anvil_writer_begin_object,
   .end_object = anvil_writer_end_object,
   .finish = anvil_writer_finish,
   .data = anvil_writer_data,
};
