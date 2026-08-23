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

#include "internal/parser.h"
#include "internal/source.h"
#include "errors.h"
#include "types.h"

/* Path resolution */
const char *fixture_path(const char *name);

/* Context specification reset */
anvl_result reset_context_spec_defaults(anvl_err_code *);

/* Source slice comparison against a C string */
bool slice_equals(anvl_slice slice, const char *);

/* Create and register a document with the given buffer. Returns the document and sets out_ctx. */
module_document setup_registered_doc(const char *, module_context *);

/* Load a fixture file and register it as a document. Returns the document and sets out_ctx. */
module_document setup_registered_file(const char *, module_context *);

/* Loads an AMP buffer through header-scan + import-loading, ready for doc_parse_body. */
module_document setup_amp_doc(const char *, module_context *);

/* Formats parse metrics as "N bytes in X ms (Y MB/s)" and hands it to TestBit.log(),
 * which attributes the line to whichever test is currently running. Matches
 * anvl_parse_hook_fn's signature — register directly via anvl_parser_set_hook().
 * The userdata parameter is unused (TestBit already knows the current test). */
void report_throughput(const anvl_parse_metrics_t *metrics, void *userdata);