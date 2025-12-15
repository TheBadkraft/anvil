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
 * operators.c - Operator implementation for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: src/core/operators.c
 * ------------------------------------------------------------------
 */

#include "operators.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Operator Lookup Table                                             */
/* ------------------------------------------------------------------ */
const anvl_op_info_t ANVL_OPERATORS[] = {
    [ANVL_OP_ASSIGN] = {":=", 2},
    [ANVL_OP_EQUAL] = {"=", 1},
    [ANVL_OP_ROCKET] = {"=>", 2},
};

/* ------------------------------------------------------------------ */
/* Operator Utilities                                                */
/* ------------------------------------------------------------------ */
anvl_operator anvl_operator_from_symbol(const char *symbol, usize len) {
   for (usize i = 0; i < sizeof(ANVL_OPERATORS) / sizeof(ANVL_OPERATORS[0]); i++) {
      if (ANVL_OPERATORS[i].length == len &&
          strncmp(ANVL_OPERATORS[i].symbol, symbol, len) == 0) {
         return (anvl_operator)i;
      }
   }
   return ANVL_OP_INVALID;
}

const char *anvl_operator_symbol(anvl_operator op) {
   if (op >= sizeof(ANVL_OPERATORS) / sizeof(ANVL_OPERATORS[0])) {
      return NULL;
   }
   return ANVL_OPERATORS[op].symbol;
}

usize anvl_operator_length(anvl_operator op) {
   if (op >= sizeof(ANVL_OPERATORS) / sizeof(ANVL_OPERATORS[0])) {
      return 0;
   }
   return ANVL_OPERATORS[op].length;
}

bool anvl_is_operator(const char *symbol, usize len) {
   return anvl_operator_from_symbol(symbol, len) != ANVL_OP_INVALID;
}