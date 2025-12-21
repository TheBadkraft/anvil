/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-03
 * File: include/utils.h
 * ------------------------------------------------------------------
 * Description:
 * This file contains utility functions and definitions.
 * ------------------------------------------------------------------
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */
#pragma once

#include "constants.h"
#include <stdbool.h>
#include <stddef.h>

/* Load source files */
bool load_source(const char *path, const char **out_source, size_t *out_len);
/* Get dialect hint from source extension */
anvl_dialect dialect_hint_from_ext(const char *filepath);