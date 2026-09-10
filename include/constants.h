/* *********************************************************************** *
 * Copyright (c) 2025 Quantum Override. All rights reserved.               *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium,    *
 * is strictly prohibited without express written permission from the      *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * constants.h - Constants for Anvil                                       *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2025-12-14                                                     *
 * File: include/constants.h                                               *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * This file contains constants for Anvil                                  *
 * ----------------------------------------------------------------------- */
#pragma once

#include "std.h"
// -------------------------
#include <sigma/types.h>

/* ----------------------------------------------------------------------- *
 * Versioning                                                              *
 * Update this by script for each release                                  *
 * ----------------------------------------------------------------------- */
#define ANVL_VERSION_MAJOR 0
#define ANVL_VERSION_MINOR 8
#define ANVL_VERSION_PATCH 0
#define ANVL_VERSION_TAG "rc"
#define ANVL_VERSION_STR "0.8.0-rc"

/* ----------------------------------------------------------------------- *
 * Constants                                                               *
 * ----------------------------------------------------------------------- */
#define ANVL_SHEBANG_LEN 5
#define ANVL_EXT_LEN 4
#define ANVL_SHEBANG_AML "#!aml"
#define ANVL_SHEBANG_AMP "#!amp"
#define ANVL_SHEBANG_ASL "#!asl"
#define ANVL_EXT_AML ".aml"
#define ANVL_EXT_AMP ".amp"
#define ANVL_EXT_ASL ".asl"
#define ANVL_EXT_ANVL ".anvl"

/* ----------------------------------------------------------------------- *
 * Grammar tokens — the fixed vocabulary of AML/AMP body syntax. Only
 * tokens worth a shared name: multi-character operators (not obvious at a
 * glance, and their length would otherwise be a second, easily-desynced
 * magic number), and single characters that carry more than one grammar
 * role (so one canonical name is shared across every place that role
 * shows up, instead of several unrelated-looking literals that happen to
 * be identical). See notes/document-body-parse.md.
 * ----------------------------------------------------------------------- */
#define ANVL_TOK_SHEBANG_PREFIX "#!"
#define ANVL_TOK_SHEBANG_PREFIX_LEN (sizeof(ANVL_TOK_SHEBANG_PREFIX) - 1)
#define ANVL_TOK_ASSIGN ":="
#define ANVL_TOK_ASSIGN_LEN (sizeof(ANVL_TOK_ASSIGN) - 1)
#define ANVL_TOK_STMT_TERMINATOR ';'
#define ANVL_TOK_QUOTE '"'
#define ANVL_TOK_BACKTICK '`'
#define ANVL_TOK_ATTRIB '@'
// Left/right delimiters for structured values (object, array/attributes, tuple, blob)
#define ANVL_TOK_LBRACE '{'
#define ANVL_TOK_RBRACE '}'
#define ANVL_TOK_LBRACKET '['
#define ANVL_TOK_RBRACKET ']'
#define ANVL_TOK_LPAREN '('
#define ANVL_TOK_RPAREN ')'
// Escape sequence and characters
#define ANVL_TOK_ESCAPE '\\'
#define ANVL_TOK_NEWLINE '\n'
#define ANVL_TOK_RETURN '\r'
#define ANVL_TOK_TAB '\t'

// Decimal separator in a numeric literal (parse_numeric_literal) *and* the
// namespace/member separator in a dotted bare-literal path (`foo.bar.baz`,
// not yet implemented) — one name for both, since it's genuinely one token
// playing two roles, not two coincidentally-identical characters.
#define ANVL_TOK_DOT '.'

/* ----------------------------------------------------------------------- *
 * Reserved keywords — see notes/document-body-parse.md. Two groups, by
 * role, not one flat list: RESERVED words are illegal both as an
 * identifier and as a bare value; VALUE words are illegal as an
 * identifier but classify a value when seen in value position (`true`/
 * `false` -> ANVL_VALUE_BOOL, `null` -> ANVL_VALUE_NULL). `vars`/`using`
 * are reserved ahead of their actual feature (AnvlScript, not yet
 * implemented) so a name chosen today doesn't silently break later.
 * ----------------------------------------------------------------------- */
#define ANVL_KEYWORD_IMPORT "import"
#define ANVL_KEYWORD_IMPORT_LEN (sizeof(ANVL_KEYWORD_IMPORT) - 1)
#define ANVL_KEYWORD_VARS "vars"
#define ANVL_KEYWORD_VARS_LEN (sizeof(ANVL_KEYWORD_VARS) - 1)
#define ANVL_KEYWORD_USING "using"
#define ANVL_KEYWORD_USING_LEN (sizeof(ANVL_KEYWORD_USING) - 1)

#define ANVL_KEYWORD_TRUE "true"
#define ANVL_KEYWORD_TRUE_LEN (sizeof(ANVL_KEYWORD_TRUE) - 1)
#define ANVL_KEYWORD_FALSE "false"
#define ANVL_KEYWORD_FALSE_LEN (sizeof(ANVL_KEYWORD_FALSE) - 1)
#define ANVL_KEYWORD_NULL "null"
#define ANVL_KEYWORD_NULL_LEN (sizeof(ANVL_KEYWORD_NULL) - 1)

/* ----------------------------------------------------------------------- *
 * Arena sizing — see notes/document-body-parse.md "Initial sizing heuristic"
 * capacity = max(sum(Source.length() across ctx->docs) * ANVL_ARENA_SIZE_MULTIPLIER,
 *                ANVL_ARENA_MIN_SIZE)
 * ----------------------------------------------------------------------- */
// Working multiplier on summed source length to estimate body-parse arena capacity.
// A minimal statement (`name := 42;`, 11 bytes) produces ~170 bytes of arena nodes —
// over 15x its own length — so raw byte count alone badly undercounts. Chosen for
// being cheap, not precise; replace with a measured value once real parsing exists
// to instrument against.
#define ANVL_ARENA_SIZE_MULTIPLIER 2
// Floor for the initial arena block. Allocator.create_bump(initial) uses a non-zero
// `initial` verbatim — it does not floor to its own default — so a small document's
// multiplied estimate could still land under a sensible minimum without this.
// Currently matches sigma's own bump-allocator default (DEFAULT_BLOCK_SZ,
// src/sigma/memory.c) but is Anvil's own named setting, not a re-export of it.
#define ANVL_ARENA_MIN_SIZE (64u * 1024u)