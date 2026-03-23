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
 * symbols.h - Symbol definitions for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: include/symbols.h
 * ------------------------------------------------------------------
 * Description:
 * This file contains symbol definitions and utilities for Anvil
 * ------------------------------------------------------------------
 */
#pragma once

#include <sigma.core/types.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Symbols                                                            */
/* ------------------------------------------------------------------ */
typedef enum {
   ANVL_SYM_AT,        // @
   ANVL_SYM_BACKTICK,  // `
   ANVL_SYM_COLON,     // :
   ANVL_SYM_COMMA,     // ,
   ANVL_SYM_DOT_DOT,   // ..
   ANVL_SYM_L_BRACE,   // {
   ANVL_SYM_R_BRACE,   // }
   ANVL_SYM_L_BRACKET, // [
   ANVL_SYM_R_BRACKET, // ]
   ANVL_SYM_L_PAREN,   // (
   ANVL_SYM_R_PAREN,   // )
   ANVL_SYM_NEWLINE,   // \n
   ANVL_SYM_QUOTE,     // "
   ANVL_SYM_S_QUOTE,   // '
   ANVL_SYM_INVALID    // Not a symbol
} anvl_symbol;

/* ------------------------------------------------------------------ */
/* Symbol Info Structure                                             */
/* ------------------------------------------------------------------ */
typedef struct {
   const char *symbol;
   usize length;
} anvl_sym_info_t;

/* ------------------------------------------------------------------ */
/* Symbol Lookup Table                                               */
/* ------------------------------------------------------------------ */
extern const anvl_sym_info_t ANVL_SYMBOLS[];

/* ------------------------------------------------------------------ */
/* Symbol Utilities                                                  */
/* ------------------------------------------------------------------ */
anvl_symbol anvl_symbol_from_symbol(const char *symbol, usize len);
const char *anvl_symbol_symbol(anvl_symbol sym);
usize anvl_symbol_length(anvl_symbol sym);
bool anvl_is_symbol(const char *symbol, usize len);