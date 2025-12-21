/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * source.h - Internal source utilities
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-03
 * File: include/internal/source.h
 * ------------------------------------------------------------------
 * Description:
 * This header defines internal utilities for source loading and
 * management within the Anvil library.
 * ------------------------------------------------------------------
 */
#pragma once

#include <stddef.h>

typedef struct anvil_source_t *anvil_source;
struct anvil_source_t
{
    const char *name; // source name (for error reporting)
    const char *data; // source data (null-terminated string)
    size_t length;    // length of source data
    size_t position;  // current position in source data
    size_t line;      // current line number
    size_t column;    // current column number
};

struct source_interface
{
    anvil_source (*init)(const char **data, size_t length, const char *name);
    void (*dispose)(anvil_source src);
} source;
extern struct source_interface source;