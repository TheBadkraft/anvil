/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * debug.h - Manual lifecycle helpers for atomized test troubleshooting
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/utilities/debug.h
 * ---------------------------------------------------------------------- *
 * Manual stub/dispose operations that mirror the module/context disposal
 * contracts without invoking the APIs under test.  Keeping these outside the
 * code-under-test isolates failures to a known cleanup site during red-stage
 * TDD and lets larger function groups be exercised with explicit teardown.
 */
#pragma once

#include "anvil.h"
#include "types.h"
#include "errors.h"
#include "internal/module.h"

/* ********************************************************************** *
 * Debug interface for manual lifecycle helpers.  These functions are     *
 * used in unit tests to create and dispose of module documents, errors,  *
 * and contexts without invoking the APIs under test.  This allows for    *
 * explicit teardown and isolation of failures during testing.            *
 * ********************************************************************** */
typedef struct anvl_debug_i {
   /* Allocate a zeroed stub document (context/filepath/source/op all NULL). */
   module_document (*stub_doc)(anvl_source, struct anvl_mod_doc_i *);
   /* Dispose a stub document and any owned source/filepath. */
   void (*dispose_doc)(module_document);
   /* Dispose every document in the list, then the list itself. */
   void (*clear_docs)(list);
   /* Allocate a zeroed stub error carrying the given code. */
   anvl_error (*stub_err)(anvl_err_code);
   /* Dispose a stub error. */
   void (*dispose_err)(anvl_error);
   /* Dispose every error in the list, then the list itself. */
   void (*clear_errs)(list);
   /* Conservative module teardown when mod_dispose() is not under test. */
   void (*dispose_mod)(AnvlMod);
   /* Conservative context teardown when mod_ctx_dispose() is not under test. */
   void (*dispose_ctx)(module_context);

   anvl_error err;
} anvl_debug_i;
extern const anvl_debug_i Debug;