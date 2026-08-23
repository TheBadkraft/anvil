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
void anvl_cleanup(void);
anvl_err_code anvl_get_error(void);