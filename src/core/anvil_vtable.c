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
   .load_buffer = anvil_load_buffer,
   .dispose = anvil_dispose,
   .has_errors = anvil_has_errors,
   .get_error = anvil_get_error,
   .get_version = anvil_get_version,
   .parse_value_fragment = anvil_parse_value_fragment,
};

const anvil_statement_i Statement = {
   .get = anvil_statement_get,
   .get_value = anvil_statement_get_value,
   .get_name = anvil_statement_get_name,
   .get_attribute_count = anvil_statement_get_attribute_count,
   .get_attribute = anvil_statement_get_attribute,
   .find_attribute = anvil_statement_find_attribute,
};

const anvil_document_i Document = {
   .get_attribute_count = anvil_document_get_attribute_count,
   .get_attribute = anvil_document_get_attribute,
   .find_attribute = anvil_document_find_attribute,
   .get_fragment_value = anvil_document_get_fragment_value,
   .get_statements = anvil_document_get_statements,
   .get_imports = anvil_document_get_imports,
   .get_error = anvil_document_get_error,
};

const anvil_statement_iterator_i StatementIterator = {
   .next = anvil_statement_iterator_next,
   .dispose = anvil_statement_iterator_dispose,
};

const anvil_document_iterator_i DocumentIterator = {
   .next = anvil_document_iterator_next,
   .dispose = anvil_document_iterator_dispose,
};

const anvil_attribute_i Attribute = {
   .get_key = anvil_attribute_get_key,
   .get_value = anvil_attribute_get_value,
};

const anvil_error_i Error = {
   .get_category = anvil_error_get_category,
   .get_message = anvil_error_get_message,
   .get_line = anvil_error_get_line,
   .get_column = anvil_error_get_column,
};

const anvil_value_i Value = {
   .get_type = anvil_value_get_type,
   .get_text = anvil_value_get_text,
   .get_count = anvil_value_get_count,
   .get_element = anvil_value_get_element,
   .get_statement = anvil_value_get_statement,
};
