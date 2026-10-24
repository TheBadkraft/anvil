/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 *                                                                        *
 * This software is proprietary and confidential. Unauthorized copying,   *
 * distribution, modification, or use of this software, via any medium,   *
 * is strictly prohibited without express written permission from the     *
 * copyright holder.                                                      *
 *                                                                        *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * asl_parser.h - AnvlScript function declaration signature (WITH_ASL)    *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: include/internal/asl_parser.h                                    *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * The ANVL-owned half of an AnvlScript function declaration -- the       *
 * signature, `ident [@[...]] (params) => { body };`. Everything inside   *
 * '{ ... }' is AnvlScript's own grammar and is never parsed here; see    *
 * notes/anvilscript-design.md ("ANVL/AnvlScript boundary"). Compiled     *
 * unconditionally (src/core/asl_parser.c sits in CORE_SRCS, not a        *
 * separately-gated source list), but its one declaration below is       *
 * guarded by WITH_ASL so an ANVL-only build has no symbol, no grammar,   *
 * and no awareness of AnvlScript's function-declaration syntax at all -- *
 * parse_statement's own call site is guarded the same way.               *
 * ********************************************************************** */
#pragma once

#include "internal/module.h"

#if WITH_ASL
/**
 * @brief One parameter in a function declaration's parameter list: a name,
 * plus either an optional default literal (`greeting = "Hello"`) or a
 * trailing `[]` parameter-array marker (`args []`) -- never both; the
 * grammar treats them as alternatives, not combinable. `default_value` is
 * NULL for a bare `identifier` parameter with no default -- it is never a
 * sentinel/placeholder value, genuinely absent. `is_vararg` is only ever
 * true on the *last* parameter in the list (enforced at parse time,
 * ANVL_ERR_ASL_VARARG_NOT_LAST otherwise) -- the caller packs every
 * argument from that position onward into one AML array for it.
 *
 * `function.params` (anvl_value_t, include/internal/module.h) is a `list`
 * of pointers to this struct, heap-allocated the same way
 * anvl_statement_t.attributes' own anvl_attribute items are -- not
 * arena-owned, so each one is disposed individually (src/core/module.c).
 * `default_value`, in contrast, *is* arena-owned (parsed the same way any
 * other scalar value is, via Source.new_node) and needs no separate
 * disposal of its own -- the existing value-tree pass already reaches it.
 */
typedef struct anvl_asl_param_t {
   anvl_slice name;
   anvl_value default_value;
   bool is_vararg;
} anvl_asl_param_t;
typedef anvl_asl_param_t *anvl_asl_param;

/**
 * @brief Parses an AnvlScript function declaration's ANVL-owned signature,
 * from the opening '(' through the trailing ';'. Called from parse_statement
 * once the identifier and any optional '@[...]' attributes are already
 * captured and the next unconsumed token is '('.
 *
 * The body's own AnvlScript grammar (statements, expressions) is never
 * parsed here -- only the raw '{ ... }' interior is captured as a source
 * slice, for the ScriptEngine to compile later. See
 * notes/anvilscript-design.md Open Question #1.
 *
 * @param src The source being parsed. '(' must be the next unconsumed
 * token; this function consumes it.
 * @param out_stmt The in-progress statement node (already allocated, with
 * `name`/`attributes` already set) to populate as ANVL_STMT_FUNCTION.
 * @return true on success. false with the source's error state already
 * set otherwise (dialect mismatch, or a malformed signature).
 */
bool asl_parse_function_decl(anvl_source src, anvl_statement *out_stmt);
#endif
