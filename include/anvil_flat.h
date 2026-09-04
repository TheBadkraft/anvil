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
 * anvil_flat.h - Public ABI: flat exported functions for Anvil Native    *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * The guaranteed-universal Anvil Native ABI surface — every function     *
 * here is an independently-callable, individually-documented extern      *
 * symbol, reachable via ordinary FFI from any language (JNI, ctypes,     *
 * N-API, extern "C" linking, P/Invoke) with no extra machinery. A        *
 * vtable convenience layer will mirror this surface later, one entry     *
 * per function here, but this header is never conditional on that layer *
 * existing. See notes/public-api.md for the full design.                *
 * ********************************************************************** */
#pragma once

#include "anvil_types.h"

/**
 * @brief Load, scan, import, parse, and resolve a document from a file path.
 * @param filepath Path to the root .anvl/.aml/.amp source file.
 * @return A document handle on success. Also returns a valid, non-NULL
 * handle when a later pipeline phase fails (bad path, syntax error, import
 * or resolve failure) — check anvil_has_errors()/anvil_get_error() to know
 * whether the document actually loaded cleanly. Returns NULL only when the
 * handle itself could not be allocated.
 */
anvil_document anvil_load(const char *filepath);

/**
 * @brief Release a document and everything it owns (every imported
 * document, the shared arena, all recorded errors).
 * @param doc The document to dispose. Safe to call with NULL (no-op).
 */
void anvil_dispose(anvil_document doc);

/**
 * @brief Whether any error was recorded while loading this document.
 * @param doc The document to check. NULL returns false.
 */
bool anvil_has_errors(anvil_document doc);

/**
 * @brief The category of the first error recorded while loading this
 * document, if any.
 * @param doc The document to check. NULL returns ANVIL_ERR_INVALID_ARGUMENT.
 * @return ANVIL_OK if anvil_has_errors(doc) is false.
 */
anvil_err_code anvil_get_error(anvil_document doc);

/**
 * @brief The Anvil Native library version string.
 */
const char *anvil_get_version(void);
