/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-03
 * File: test/utilities/helpers.h
 * ------------------------------------------------------------------
 * Description:
 * Test helper functions for loading sample files.
 * ------------------------------------------------------------------
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "anvil.h"

/* Load test sample files by name */
const char *get_anvl_path(const char *name);
const char *get_source_path(const char *name);

/* Helper functions for stress tests */
char *generate_large_nested_structure(void);
char *generate_deep_nested_structure(void);
char *fetch_and_convert_real_data(void);

/* Shared parser test helpers */
context parse_source_ok(const char *source, anvl_dialect dialect);
context parse_source_with_err(const char *source, anvl_dialect dialect,
							  const anvl_error_state **err_state);
context parse_fixture_file_ok(const char *filename, anvl_dialect exp_dialect,
							  usize exp_pos, usize exp_line, usize exp_col);