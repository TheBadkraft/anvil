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
 * anvil_types_vtable.c - the `TypeRegistry` vtable instance              *
 * ********************************************************************** */
#include "anvil_type_registry_vtable.h"

const anvil_type_registry_i TypeRegistry = {
   .load = anvil_type_registry_load,
   .load_from_includes = anvil_type_registry_load_from_includes,
   .dispose = anvil_type_registry_dispose,
   .find = anvil_type_registry_find,
   .resolve = anvil_type_resolve,
   .def_get_kind = anvil_type_def_get_kind,
   .def_get_size = anvil_type_def_get_size,
   .def_get_min = anvil_type_def_get_min,
   .def_get_min_numeric = anvil_type_def_get_min_numeric,
   .def_get_max = anvil_type_def_get_max,
   .def_get_max_numeric = anvil_type_def_get_max_numeric,
   .def_get_format = anvil_type_def_get_format,
   .def_get_value_count = anvil_type_def_get_value_count,
   .def_get_value = anvil_type_def_get_value,
   .get_diagnostic_count = anvil_type_registry_get_diagnostic_count,
   .get_diagnostic = anvil_type_registry_get_diagnostic,
   .diagnostic_get_category = anvil_type_diagnostic_get_category,
   .diagnostic_get_name = anvil_type_diagnostic_get_name,
   .diagnostic_get_source = anvil_type_diagnostic_get_source,
   .diagnostic_get_message = anvil_type_diagnostic_get_message,
};
