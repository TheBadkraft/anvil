/* ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2025-12-03                                                     *
 * File: include/internal/files.h                                          *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * This file contains file functions and definitions.                      *
 * ----------------------------------------------------------------------- *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 * *********************************************************************** */
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct anvl_files_i {
    bool (*load)(const char *path, const char **out_source, size_t *out_len);
    const char *(*filename)(const char *path);
} anvl_files_i;

extern const anvl_files_i Files;