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
void anvl_cleanup(void);
anvl_err_code anvl_get_error(void);