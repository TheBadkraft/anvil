/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * helpers.c - Test utility functions
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/utilities/helpers.c
 */

#include "helpers.h"
#include "anvil.h"
#include "internal/constants.h"
#include "internal/module.h"
// -----------------------------------------------------------------
#include <stdio.h>
#include <sigma/strings.h>

static char path_buffer[512];

const char *fixture_path(const char *name) {
   snprintf(path_buffer, sizeof(path_buffer), "../../test/fixtures/%s", name);
   return path_buffer;
}

anvl_result reset_context_spec_defaults(anvl_err_code *out_err_code) {
   anvl_ctx_spec defaults = {
      .docs_cap = ANVL_CTX_DEFAULT_DOC_CAP,
      .errs_cap = ANVL_CTX_DEFAULT_ERR_CAP,
      .map_cap = ANVL_CTX_DEFAULT_MAP_CAP,
      .strict_namespace = ANVL_CTX_DEFAULT_STRICT_NAMESPACE,
   };
   context_spec spec = &defaults;
   return resolve_context_spec(&spec, out_err_code);
}
