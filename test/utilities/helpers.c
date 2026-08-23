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
#include "internal/files.h"
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

bool slice_equals(anvl_slice slice, const char *expected) {
   if (!slice.data || !expected) {
      return false;
   }
   usize expected_len = strlen(expected);
   if (Source.slice_length(slice) != expected_len) {
      return false;
   }
   // can we use non-allocated buffer here?
   char data[256];
   Source.substring(slice, data);

   return memcmp(data, expected, expected_len) == 0;
}

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

module_document setup_registered_file(const char *fixture_name, module_context *out_ctx) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   module_context ctx = NULL;
   module_document doc = NULL;
   const char *path = fixture_path(fixture_name);

   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      return NULL;
   }

   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      return NULL;
   }

   if (ANVL_RES_OK != doc_load_source(doc, ANVL_SOURCE_FROM_FILE, path, 0, &err_code) ||
       ANVL_RES_OK != mod_ctx_register_doc(ctx, doc, path, &err_code)) {
      doc_dispose(doc);
      mod_ctx_dispose(ctx);
      return NULL;
   }

   *out_ctx = ctx;
   return doc;
}

void report_throughput(const anvl_parse_metrics_t *metrics, void *userdata) {
   (void)userdata; // unused: TestBit.log() already attributes the line to the current test
   if (!metrics) {
      return;
   }
   double sec = (double)metrics->elapsed_ns / 1e9;
   double mb_per_sec = sec > 0 ? ((double)metrics->bytes / 1e6) / sec : 0.0;

   char buf[128];
   snprintf(buf, sizeof(buf), "%zu bytes in %.3f ms (%.2f MB/s)", metrics->bytes, sec * 1000.0,
            mb_per_sec);
   TestBit.log(buf);
}

module_document setup_amp_doc(const char *buffer, module_context *out_ctx) {
   module_document doc = setup_registered_doc(buffer, out_ctx);
   if (!doc) {
      return NULL;
   }
   anvl_err_code err_code = ANVL_ERR_NONE;
   if (ANVL_RES_OK != doc_scan_header(doc, &err_code)) {
      return doc;
   }

   usize size_hint = 0;
   (void)mod_load_imports(*out_ctx, doc, &size_hint, &err_code);
   usize capacity = mod_ctx_arena_size_hint(size_hint);
   mod_ctx_create_arena(*out_ctx, capacity, &err_code);

   return doc;
}