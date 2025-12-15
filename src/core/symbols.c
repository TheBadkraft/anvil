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
 * symbols.c - Symbol implementation for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: src/core/symbols.c
 * ------------------------------------------------------------------
 */

#include "symbols.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Symbol Lookup Table                                               */
/* ------------------------------------------------------------------ */
const anvl_sym_info_t ANVL_SYMBOLS[] = {
    [ANVL_SYM_AT] = {"@", 1},
    [ANVL_SYM_BACKTICK] = {"`", 1},
    [ANVL_SYM_COLON] = {":", 1},
    [ANVL_SYM_COMMA] = {",", 1},
    [ANVL_SYM_DOT_DOT] = {"..", 2},
    [ANVL_SYM_L_BRACE] = {"{", 1},
    [ANVL_SYM_R_BRACE] = {"}", 1},
    [ANVL_SYM_L_BRACKET] = {"[", 1},
    [ANVL_SYM_R_BRACKET] = {"]", 1},
    [ANVL_SYM_L_PAREN] = {"(", 1},
    [ANVL_SYM_R_PAREN] = {")", 1},
    [ANVL_SYM_NEWLINE] = {"\n", 1},
    [ANVL_SYM_QUOTE] = {"\"", 1},
    [ANVL_SYM_S_QUOTE] = {"'", 1},
};

/* ------------------------------------------------------------------ */
/* Symbol Utilities                                                  */
/* ------------------------------------------------------------------ */
anvl_symbol anvl_symbol_from_symbol(const char *symbol, usize len) {
   for (usize i = 0; i < sizeof(ANVL_SYMBOLS) / sizeof(ANVL_SYMBOLS[0]); i++) {
      if (ANVL_SYMBOLS[i].length == len &&
          strncmp(ANVL_SYMBOLS[i].symbol, symbol, len) == 0) {
         return (anvl_symbol)i;
      }
   }
   return ANVL_SYM_INVALID;
}

const char *anvl_symbol_symbol(anvl_symbol sym) {
   if (sym >= sizeof(ANVL_SYMBOLS) / sizeof(ANVL_SYMBOLS[0])) {
      return NULL;
   }
   return ANVL_SYMBOLS[sym].symbol;
}

usize anvl_symbol_length(anvl_symbol sym) {
   if (sym >= sizeof(ANVL_SYMBOLS) / sizeof(ANVL_SYMBOLS[0])) {
      return 0;
   }
   return ANVL_SYMBOLS[sym].length;
}

bool anvl_is_symbol(const char *symbol, usize len) {
   return anvl_symbol_from_symbol(symbol, len) != ANVL_SYM_INVALID;
}