/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * serializer.h — Public API for anvil.serializer (post-parse optional)
 * ------------------------------------------------------------------
 * Ported from: AnvilWriter.cs + AnvilWriterOptions.cs (Anvil.Net v0.2.1)
 *
 * The serializer takes a parsed context and emits AML/AMP/ASL text.
 * It is a post-parse optional component: the core parser hands back a
 * context and is done; everything else (resolver, serializer, schema,
 * deserializer) operates on that context independently.
 *
 * Package artifact: anvil.serializer.o  (cpkg anvil.serializer)
 * ------------------------------------------------------------------
 */
#pragma once

#include "constants.h"
#include "context.h"
#include <sigma.core/strings.h>
#include <stdbool.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Serializer options (port of AnvilWriterOptions)                   */
/* ------------------------------------------------------------------ */
typedef struct {
   anvl_dialect dialect;   /* AML/AMP/ASL — controls shebang          */
   bool include_shebang;   /* emit #!aml / #!amp / etc.  default true  */
   bool minify;            /* single-line, no whitespace  default false */
   int indent_width;       /* spaces per indent level     default 4     */
   bool align_assignments; /* pad := to align             default false */
   bool preserve_vars;     /* emit vars{} block + .ref    default true  */
   int inline_threshold;   /* fields/elems before block   default 2     */
} anvl_serializer_options_t;

/* ------------------------------------------------------------------ */
/* Default singleton options (read-only)                              */
/* ------------------------------------------------------------------ */
extern const anvl_serializer_options_t ANVL_SERIALIZER_DEFAULT;
extern const anvl_serializer_options_t ANVL_SERIALIZER_MINIFIED;

/* ------------------------------------------------------------------ */
/* Serializer interface (vtable)                                      */
/* ------------------------------------------------------------------ */
typedef struct anvl_serializer_i {
   /**
    * Serialize context to a string_builder.
    * Caller must dispose the result via StringBuilder.dispose().
    * Returns NULL on error (e.g. AMP dialect with object values).
    */
   string_builder (*serialize)(context ctx, const anvl_serializer_options_t *opts);

   /**
    * Serialize context to a FILE stream.
    * Returns false on error.
    */
   bool (*to_stream)(context ctx, FILE *out, const anvl_serializer_options_t *opts);
} anvl_serializer_i;

extern const anvl_serializer_i Serializer;
