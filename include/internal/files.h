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

#include "errors.h"
#include "types.h"
// -------------------------
#include <stdbool.h>
#include <stddef.h>

typedef struct anvl_files_i {
   /**
    * @brief Load a file into memory.
    * @param[in] path The path to the file.
    * @param[out] out_source Pointer to the loaded source.
    * @param[out] out_len Pointer to the length of the loaded source.
    * @param[out] out_err_code Pointer to the error code if loading fails.
    * @return Anvl result indicating success or failure.
    */
   anvl_result (*load)(const char *, const char **, usize *, anvl_err_code *);
   /**
    * @brief Get the filename from a path.
    * @param[in] path The path to the file.
    * @return The filename.
    */
   const char *(*filename)(const char *);
   /**
    * @brief Get the dialect hint from a file path.
    * @param[in] path The path to the file.
    * @return The dialect hint.
    */
   anvl_dialect (*dialect_hint)(const char *);
   /**
    * @brief Get the length of a file.
    * @param[in] path The path to the file.
    * @return The length of the file.
    */
   usize (*length)(const char *);
} anvl_files_i;

extern const anvl_files_i Files;