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
 * anvil_vtable.c - Public ABI implementation: vtable convenience layer   *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Every field points directly at its anvil_flat.c counterpart — verified *
 * by pointer identity in test/unit/test_anvil_vtable.c, not just         *
 * matching behavior. See notes/public-api.md.                           *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_vtable.h"

const anvil_i Anvil = {
   .load = anvil_load,
   .dispose = anvil_dispose,
   .has_errors = anvil_has_errors,
   .get_error = anvil_get_error,
   .get_version = anvil_get_version,
};

const anvil_statement_i Statement = {
   .get = anvil_statement_get,
   .get_value = anvil_statement_get_value,
   .get_name = anvil_statement_get_name,
};

const anvil_value_i Value = {
   .get_type = anvil_value_get_type,
   .get_text = anvil_value_get_text,
   .get_count = anvil_value_get_count,
   .get_element = anvil_value_get_element,
   .get_statement = anvil_value_get_statement,
};
