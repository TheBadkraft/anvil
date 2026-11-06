/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium,    *
 * is strictly prohibited without express written permission from the      *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * errors.h - Error handling for Anvil                                     *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-13                                                     *
 * File: include/errors.h                                                  *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * Error codes and state management for Anvil parser and operations        *
 * *********************************************************************** */
#pragma once

#include "anvil_error_codes.h"
#include "types.h"
// -----------------------------------------------------------------
#include <sigma/types.h>
#include <sigma/list.h>
#include <stdbool.h>


/* -----------------------------------------------------------------
 * Error State Structure
 * ----------------------------------------------------------------- */
typedef struct anvl_err_state_t {
   anvl_err_code code;
   const char *message;
   usize line;
   usize column;
   const char *file; // owned copy of the path of the document the error is in; NULL when unknown
   char *detail; // owned: set when the message is built for this one error (e.g. it names an
                 // identifier); `message` then points at it. NULL for a table message.
} anvl_err_state_t;

/* ----------------------------------------------------------------- */
/* Error State Management                                            */
/* ----------------------------------------------------------------- */
anvl_result anvl_error_set(list, anvl_err_code, usize, usize, const char *, anvl_err_code *);
/// Like anvl_error_set, but with a message built for this error (copied), used instead of the
/// code's table message. First-error-wins applies the same way.
anvl_result anvl_error_set_message(list, anvl_err_code, usize, usize, const char *, const char *,
                                   anvl_err_code *);
/// Frees an error and the message it owns.
void anvl_error_dispose(anvl_error);
void anvl_error_clear(list);
bool anvl_error_is_set(list);
anvl_error anvl_error_get(list, usize);

/* ----------------------------------------------------------------- */
/* Error Code Utilities                                              */
/* ----------------------------------------------------------------- */
const char *anvl_error_code_message(anvl_err_code);
const char *anvl_error_code_name(anvl_err_code);