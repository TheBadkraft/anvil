/*
 * Copyright (c) 2025-26 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * parser.h - Parser interface for Anvil
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * Created: 2025-12-14
 * File: include/internal/parser.h
 * ----------------------------------------------------------------------- *
 * Description:
 * Parser interface and structures for Anvil AML/ASL parsing
 * ----------------------------------------------------------------------- *
 */
#pragma once

#include "anvil.h"
#include "internal/module.h"
// -----------------------------------------------------------------
#include <stdbool.h>

/* ----------------------------------------------------------------- *
 * Parse instrumentation hook
 * ----------------------------------------------------------------- *
 * Optional, zero-cost-when-unset observer for a finished parse. The parser
 * depends only on this callback signature, not on any specific consumer
 * (a test framework, a logger, a profiler) — whoever calls
 * anvl_parser_set_hook() owns what happens with the metrics.
 * ----------------------------------------------------------------- */
typedef struct {
   usize bytes;      // Source.length() of what was parsed
   usize elapsed_ns; // Time.elapsed(start, end)
} anvl_parse_metrics_t;

typedef void (*anvl_parse_hook_fn)(const anvl_parse_metrics_t *metrics, void *userdata);

/**
 * @brief Register a hook to be called with metrics after every finished parse.
 * @param fn The hook function to call, or NULL to disable.
 * @param userdata Opaque pointer passed through to the hook unchanged.
 */
void anvl_parser_set_hook(anvl_parse_hook_fn fn, void *userdata);
/**
 * @brief Clear any registered parse hook.
 */
void anvl_parser_clear_hook(void);

/* ----------------------------------------------------------------- *
 * Internal Parser Functions
 * ----------------------------------------------------------------- */
anvl_result anvl_parse(anvl_source);
/**
 * @brief Parse a single, standalone value expression ("Value Fragment") from
 * `doc`'s source — no enclosing document/statement context, no VarRef
 * support at any nesting depth (there is no identifier map here to resolve
 * one against). `doc` must already be registered with a context that has an
 * arena (mirrors doc_parse_body's own precondition). A trailing ';' is
 * tolerated but never required; any other trailing content after the value
 * is a hard error.
 * @param doc The (unregistered-header, arena-ready) document whose source
 * holds the fragment text.
 * @param out_value Set to the parsed value on success, untouched on failure.
 * @param out_err_code Set to the failure's error code, or ANVL_ERR_NONE on
 * success.
 */
anvl_result anvl_parse_value_fragment(module_document doc, anvl_value *out_value,
                                      anvl_err_code *out_err_code);
/**
 * @brief Read an attribute's value text as a value (`attr->decoded`), through the scalar grammar.
 * @details Called for statement attributes as they are parsed and for module attributes at the
 * start of a document's body parse (the arena that owns value nodes does not exist before that).
 * A flag attribute (empty value) leaves `decoded` NULL. A value the scalar grammar does not accept
 * as a whole is exposed as a BARE value of its trimmed raw text; the trial parse leaves no trace:
 * source position, recorded errors and the parser's last-error code are all restored, so decoding
 * can never fail a load that scanning accepted. Fails only on allocation failure.
 * @param src The attribute's source, positioned anywhere (it is restored).
 */
bool anvl_attribute_decode(anvl_source src, anvl_attribute attr);

/**
 * True if an attribute value (the span [begin, end), already trimmed) starts with `[`, `{` or `(`
 * and is not closed within the span: the scan for an attribute's value stops at the first `,` or
 * `]`, so an array, object or tuple is cut short there. Shared by the statement-level and the
 * module-level attribute scanners, which report it as ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE.
 * A bracketed value that closes within the span (`{1}`, `(1 2)`) is a plain bare word, not this.
 */
bool anvl_attr_value_is_split_compound(const char *begin, const char *end);
void anvl_cleanup(void);
anvl_err_code anvl_get_error(void);

/**
 * @brief Records a parse error on `src`'s error state, computing line/column
 * from the source's current position. Shared with src/core/asl_parser.c
 * (WITH_ASL only) so AnvlScript's own signature-grammar errors go through
 * the exact same path as every other parser error, rather than a second,
 * separately-maintained copy.
 */
void parser_set_error(anvl_source src, anvl_err_code code);

/**
 * @brief Parses a bare identifier at the source's current position --
 * shared with src/core/asl_parser.c (WITH_ASL only) for parameter-name
 * parsing, so a keyword is rejected there exactly as it is everywhere
 * else, rather than a second, separately-maintained copy of this rule.
 */
bool parse_identifier(anvl_source src, anvl_slice *out_identifier);

/**
 * @brief Parses one scalar value (reserved literal, numeric, string, blob,
 * or bare) at the source's current position -- shared with
 * src/core/asl_parser.c (WITH_ASL only) for parameter default literals, so
 * a default reuses exactly the same literal forms every other scalar
 * value in this grammar already accepts, rather than a second, narrower
 * literal parser.
 */
bool parse_scalar_value(anvl_source src, anvl_value *out_value);