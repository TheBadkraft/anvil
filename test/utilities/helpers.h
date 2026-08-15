/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * helpers.h - Test utility functions
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/utilities/helpers.h
 */
#pragma once

#include "internal/source.h"
#include "errors.h"
#include "types.h"

/* Path resolution */
const char *fixture_path(const char *name);

/* Context specification reset */
anvl_result reset_context_spec_defaults(anvl_err_code *out_err_code);

/* Source slice comparison against a C string */
bool slice_equals(anvl_source src, anvl_src_slice slice, const char *expected);

/* Source slice emptiness check */
bool slice_is_empty(anvl_src_slice slice);

/* Create and register a document with the given buffer. Returns the document and sets out_ctx. */
module_document setup_registered_doc(const char *buffer, module_context *out_ctx);
