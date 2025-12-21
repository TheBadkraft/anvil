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
 * operators.h - Operator definitions for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: include/operators.h
 * ------------------------------------------------------------------
 * Description:
 * This file contains operator definitions and utilities for Anvil
 * ------------------------------------------------------------------
 */
#pragma once

#include <sigcore/types.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Operators                                                          */
/* ------------------------------------------------------------------ */
typedef enum {
   ANVL_OP_ASSIGN, // :=
   ANVL_OP_EQUAL,  // =
   ANVL_OP_ROCKET, // =>
   ANVL_OP_INVALID // Not an operator
} anvl_operator;

/* ------------------------------------------------------------------ */
/* Operator Info Structure                                           */
/* ------------------------------------------------------------------ */
typedef struct {
   const char *symbol;
   usize length;
} anvl_op_info_t;

/* ------------------------------------------------------------------ */
/* Operator Lookup Table                                             */
/* ------------------------------------------------------------------ */
extern const anvl_op_info_t ANVL_OPERATORS[];

/* ------------------------------------------------------------------ */
/* Operator Utilities                                                */
/* ------------------------------------------------------------------ */
anvl_operator anvl_operator_from_symbol(const char *symbol, usize len);
const char *anvl_operator_symbol(anvl_operator op);
usize anvl_operator_length(anvl_operator op);
bool anvl_is_operator(const char *symbol, usize len);