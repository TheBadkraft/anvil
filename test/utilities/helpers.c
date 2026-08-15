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
#include "internal/source.h"
#include "internal/source_registry.h"
#include "testbit.h"
// -----------------------------------------------------------------
#include <stdio.h>
#include <sigma/list.h>
#include <sigma/strings.h>
#include <string.h>

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
   };
   context_spec spec = &defaults;
   return resolve_context_spec(&spec, out_err_code);
}

bool slice_equals(anvl_source src, anvl_src_slice slice, const char *expected) {
   if (!src || !src->buffer.bucket || !expected) {
      return false;
   }
   usize expected_len = strlen(expected);
   if (slice.length != expected_len) {
      return false;
   }
   const char *data = (const char *)src->buffer.bucket;
   return memcmp(data + slice.start, expected, expected_len) == 0;
}

bool slice_is_empty(anvl_src_slice slice) { return slice.start == 0 && slice.length == 0; }

module_document setup_registered_doc(const char *buffer, module_context *out_ctx) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   module_context ctx = NULL;
   module_document doc = NULL;

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      return NULL;
   }

   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      return NULL;
   }

   if (ANVL_RES_OK != Source.from_buffer(&doc->source, buffer, strlen(buffer), &err_code) ||
       ANVL_RES_OK != mod_ctx_register_doc(ctx, doc, "test.anvl", &err_code)) {
      doc_dispose(doc);
      mod_ctx_dispose(ctx);
      return NULL;
   }

   *out_ctx = ctx;
   return doc;
}
