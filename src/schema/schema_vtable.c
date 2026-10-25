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
 * schema_vtable.c - the `Schema` vtable instance                          *
 * ********************************************************************** */
#include "anvil_schema_vtable.h"

const anvil_schema_i Schema = {
   .load = anvil_schema_load,
   .dispose = anvil_schema_dispose,
   .validate = anvil_schema_validate,
   .validate_value = anvil_schema_validate_value,
   .get_violation_count = anvil_schema_get_violation_count,
   .get_violation = anvil_schema_get_violation,
   .violation_get_category = anvil_schema_violation_get_category,
   .violation_get_field = anvil_schema_violation_get_field,
   .violation_get_message = anvil_schema_violation_get_message,
   .get_diagnostic_count = anvil_schema_get_diagnostic_count,
   .get_diagnostic = anvil_schema_get_diagnostic,
   .diagnostic_get_category = anvil_schema_diagnostic_get_category,
   .diagnostic_get_type_category = anvil_schema_diagnostic_get_type_category,
   .diagnostic_get_field = anvil_schema_diagnostic_get_field,
   .diagnostic_get_message = anvil_schema_diagnostic_get_message,
   .get_field_count = anvil_schema_get_field_count,
   .get_field = anvil_schema_get_field,
   .find_field = anvil_schema_find_field,
   .field_get_name = anvil_schema_field_get_name,
   .field_get_kind = anvil_schema_field_get_kind,
   .field_get_type_name = anvil_schema_field_get_type_name,
   .field_is_required = anvil_schema_field_is_required,
   .field_is_nullable = anvil_schema_field_is_nullable,
   .field_is_integer = anvil_schema_field_is_integer,
   .field_get_size = anvil_schema_field_get_size,
   .field_get_min_numeric = anvil_schema_field_get_min_numeric,
   .field_get_max_numeric = anvil_schema_field_get_max_numeric,
   .field_get_min_items = anvil_schema_field_get_min_items,
   .field_get_max_items = anvil_schema_field_get_max_items,
   .field_get_value_count = anvil_schema_field_get_value_count,
   .field_get_value = anvil_schema_field_get_value,
   .field_get_format = anvil_schema_field_get_format,
   .field_get_tag = anvil_schema_field_get_tag,
   .field_get_nested_count = anvil_schema_field_get_nested_count,
   .field_get_nested = anvil_schema_field_get_nested,
   .field_find_nested = anvil_schema_field_find_nested,
   .field_get_items = anvil_schema_field_get_items,
   .field_get_extra_count = anvil_schema_field_get_extra_count,
   .field_get_extra_name = anvil_schema_field_get_extra_name,
   .field_get_extra_value = anvil_schema_field_get_extra_value,
};
