/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * import.h — anvil.import: import graph (DAG) resolution
 * ------------------------------------------------------------------
 * Ported from: AnvilImportSet (Anvil.Net rc.3)
 *
 * Builds a directed acyclic graph (DAG) of imported contexts
 * starting from the root context's import_list.  Each node in the
 * graph corresponds to one distinct .aml/.anvl file (identified by
 * canonical path).
 *
 * Component: post-parse optional (anvil.import.o)
 *
 * Design notes:
 *   - Core owns the DAG structure and DFS cycle detection.
 *   - File I/O is delegated to a loader callback provided by the
 *     binding — core never opens file descriptors itself.
 *   - Diamond dedup: loading the same canonical path twice produces
 *     a single graph entry; the second reference only adds an alias.
 *   - Alias derivation: if no "as alias" clause is present the stem
 *     of the filename is used (e.g. "my-blocks.aml" → "my_blocks").
 * ------------------------------------------------------------------
 */
#pragma once

#include "context.h"
#include "types.h"
#include <stdbool.h>
#include <stddef.h>

/* anvl_import_decl is defined in types.h */

/* ------------------------------------------------------------------ */
/* Loader callback — provided by the binding                         */
/*                                                                    */
/* Called by ImportGraph.build() for each import path that must be   */
/* loaded.  The binding allocates *out_src and sets *out_len.         */
/* Ownership of *out_src is transferred to the graph; the graph will  */
/* call Allocator.dispose() on it when the graph is disposed.         */
/*                                                                    */
/* Return false to signal "file not found" or a load error; the       */
/* graph will set ANVL_ERR_IMPORT_FILE_NOT_FOUND and abort.           */
/* ------------------------------------------------------------------ */
typedef bool (*anvl_import_loader_cb)(const char *path,
                                      const char **out_src,
                                      usize *out_len,
                                      void *userdata);

/* ------------------------------------------------------------------ */
/* One resolved entry in the import graph                            */
/* ------------------------------------------------------------------ */
typedef struct anvl_import_entry_t {
   char *canonical; /* heap-owned canonical path (NUL-terminated)    */
   char *alias;     /* heap-owned alias identifier (NUL-terminated)  */
   context ctx;     /* parsed context for this import                */
   source src;      /* source for this import                        */
} anvl_import_entry_t;

/* ------------------------------------------------------------------ */
/* Import graph state (returned by ImportGraph.build)                */
/* ------------------------------------------------------------------ */
typedef struct anvl_import_graph_t {
   anvl_import_entry_t *entries; /* flat array of graph entries       */
   usize count;                  /* number of entries                 */
   usize capacity;               /* allocated capacity                */
} anvl_import_graph_t;

/* ------------------------------------------------------------------ */
/* ImportGraph interface vtable                                       */
/* ------------------------------------------------------------------ */
typedef struct anvl_import_graph_i {
   /* Build an import graph from a parsed root context.
    *
    * owner_path: canonical path of the root file (used for cycle
    *             detection — may be NULL for in-memory sources).
    * loader:     callback to load a file by canonical path.
    * userdata:   opaque pointer forwarded to every loader call.
    *
    * Returns NULL + sets an ANVL_ERR_IMPORT_* error on failure.
    * Returns a valid graph (possibly with count==0) on success.
    */
   anvl_import_graph_t *(*build)(context ctx,
                                 const char *owner_path,
                                 anvl_import_loader_cb loader,
                                 void *userdata);

   /* Free the graph and all owned memory (canonical, alias, ctx, src
    * for every entry).  Safe to call with NULL. */
   void (*dispose)(anvl_import_graph_t *graph);
} anvl_import_graph_i;

extern const anvl_import_graph_i ImportGraph;
