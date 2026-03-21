/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * import.c — anvil.import: import graph (DAG) resolution
 * ------------------------------------------------------------------
 * Implements the ImportGraph vtable:
 *   ImportGraph.build()   — DFS graph walk, cycle & alias dedup
 *   ImportGraph.dispose() — free graph and all owned memory
 *
 * Algorithm:
 *   For each import_decl in ctx->import_list:
 *     1. Materialise path string from source span.
 *     2. Derive alias (explicit or filename stem).
 *     3. Check alias_map → ANVL_ERR_IMPORT_DUPLICATE_ALIAS.
 *     4. Check canonical_map (diamond dedup) → register alias, skip.
 *     5. Check DFS load-stack → ANVL_ERR_IMPORT_CYCLIC.
 *     6. Call loader_cb → ANVL_ERR_IMPORT_FILE_NOT_FOUND.
 *     7. Parse loaded bytes into a new context.
 *     8. Push onto DFS stack, recurse for nested imports, pop.
 *     9. Append entry to graph.
 * ------------------------------------------------------------------
 */

#include "import.h"
#include "anvil.h"
#include "context.h"
#include "errors.h"
#include <sigma.memory/memory.h>
#include <sigma.core/strings.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal: DFS load-stack (cycle detection)                        */
/* ------------------------------------------------------------------ */
typedef struct {
   char **paths; /* non-owning pointers to canonical strings in entries */
   usize count;
   usize capacity;
} load_stack_t;

static bool load_stack_push(load_stack_t *s, char *canonical) {
   if (s->count >= s->capacity) {
      usize newcap = s->capacity ? s->capacity * 2 : 8;
      char **nb = Allocator.alloc(sizeof(char *) * newcap);
      if (!nb)
         return false;
      memset(nb, 0, sizeof(char *) * newcap);
      if (s->paths) {
         memcpy(nb, s->paths, sizeof(char *) * s->count);
         Allocator.free(s->paths);
      }
      s->paths = nb;
      s->capacity = newcap;
   }
   s->paths[s->count++] = canonical;
   return true;
}

static void load_stack_pop(load_stack_t *s) {
   if (s->count > 0)
      s->count--;
}

static bool load_stack_contains(const load_stack_t *s, const char *canonical) {
   for (usize i = 0; i < s->count; i++) {
      if (strcmp(s->paths[i], canonical) == 0)
         return true;
   }
   return false;
}

static void load_stack_free(load_stack_t *s) {
   if (s->paths)
      Allocator.free(s->paths);
   s->paths = NULL;
   s->count = s->capacity = 0;
}

/* ------------------------------------------------------------------ */
/* Internal: alias / canonical lookup maps (linear — imports are few) */
/* ------------------------------------------------------------------ */
typedef struct {
   char **keys; /* heap-owned strings (non-owning — points into entries) */
   usize *vals; /* index into graph->entries */
   usize count;
   usize capacity;
} str_map_t;

static bool str_map_insert(str_map_t *m, char *key, usize val) {
   if (m->count >= m->capacity) {
      usize newcap = m->capacity ? m->capacity * 2 : 8;
      char **nk = Allocator.alloc(sizeof(char *) * newcap);
      usize *nv = Allocator.alloc(sizeof(usize) * newcap);
      if (!nk || !nv) {
         if (nk)
            Allocator.free(nk);
         if (nv)
            Allocator.free(nv);
         return false;
      }
      memset(nk, 0, sizeof(char *) * newcap);
      memset(nv, 0, sizeof(usize) * newcap);
      if (m->keys) {
         memcpy(nk, m->keys, sizeof(char *) * m->count);
         memcpy(nv, m->vals, sizeof(usize) * m->count);
         Allocator.free(m->keys);
         Allocator.free(m->vals);
      }
      m->keys = nk;
      m->vals = nv;
      m->capacity = newcap;
   }
   m->keys[m->count] = key;
   m->vals[m->count] = val;
   m->count++;
   return true;
}

/* Returns USIZE_MAX if not found */
static usize str_map_find(const str_map_t *m, const char *key) {
   for (usize i = 0; i < m->count; i++) {
      if (strcmp(m->keys[i], key) == 0)
         return m->vals[i];
   }
   return (usize)-1;
}

static void str_map_free(str_map_t *m) {
   if (m->keys)
      Allocator.free(m->keys);
   if (m->vals)
      Allocator.free(m->vals);
   m->keys = NULL;
   m->vals = NULL;
   m->count = m->capacity = 0;
}

/* ------------------------------------------------------------------ */
/* Internal: graph capacity management                               */
/* ------------------------------------------------------------------ */
static bool graph_ensure_cap(anvl_import_graph_t *g, usize need) {
   if (g->capacity >= need)
      return true;
   usize newcap = g->capacity ? g->capacity * 2 : 4;
   while (newcap < need)
      newcap *= 2;
   anvl_import_entry_t *nb = Allocator.alloc(sizeof(anvl_import_entry_t) * newcap);
   if (!nb)
      return false;
   memset(nb, 0, sizeof(anvl_import_entry_t) * newcap);
   if (g->entries) {
      memcpy(nb, g->entries, sizeof(anvl_import_entry_t) * g->count);
      Allocator.free(g->entries);
   }
   g->entries = nb;
   g->capacity = newcap;
   return true;
}

/* ------------------------------------------------------------------ */
/* Internal: derive stem alias from a filename                       */
/* e.g. "some/path/my-blocks.aml" → "my_blocks"                     */
/* ------------------------------------------------------------------ */
static char *derive_stem_alias(const char *path, usize path_len) {
   /* Find last '/' to isolate filename */
   usize fname_start = 0;
   for (usize i = path_len; i > 0; i--) {
      if (path[i - 1] == '/') {
         fname_start = i;
         break;
      }
   }

   /* Find last '.' to strip extension */
   usize stem_end = path_len - fname_start;
   for (usize i = fname_start; i < path_len; i++) {
      if (path[i] == '.') {
         stem_end = i - fname_start;
         break;
      }
   }

   /* Build alias: lowercase, hyphens/spaces → underscores */
   string_builder sb = StringBuilder.new(64);
   if (!sb)
      return NULL;
   for (usize i = 0; i < stem_end; i++) {
      char c = path[fname_start + i];
      if (c == '-' || c == ' ')
         c = '_';
      else if (c >= 'A' && c <= 'Z')
         c = (char)(c + 32);
      StringBuilder.appendf(sb, "%c", c);
   }
   string result = StringBuilder.toString(sb);
   StringBuilder.dispose(sb);
   return result;
}

/* ------------------------------------------------------------------ */
/* Forward declaration for mutual recursion                          */
/* ------------------------------------------------------------------ */
typedef struct build_ctx_t build_ctx_t;
static bool load_import(build_ctx_t *bc, const char *path, usize path_len,
                        const char *alias);

/* ------------------------------------------------------------------ */
/* Internal build context (shared across recursive calls)            */
/* ------------------------------------------------------------------ */
struct build_ctx_t {
   anvl_import_graph_t *graph;
   load_stack_t stack;
   str_map_t alias_map;
   str_map_t canonical_map;
   anvl_import_loader_cb loader;
   void *userdata;
};

/* ------------------------------------------------------------------ */
/* Process one import declaration from a context                     */
/* ------------------------------------------------------------------ */
static bool process_decl(build_ctx_t *bc, context ctx,
                         const struct anvl_import_decl *decl) {
   const char *src_data = Source.data(ctx->source);

   /* Materialise path string */
   const char *raw_path = src_data + decl->path_pos;
   usize raw_path_len = decl->path_len;

   /* Materialise alias string (or derive stem) */
   char *alias = NULL;
   if (decl->alias_len > 0) {
      /* Explicit alias */
      string_builder sb = StringBuilder.new(64);
      if (!sb) {
         anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                        anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                        0, 0, __FILE__);
         return false;
      }
      StringBuilder.appendf(sb, "%.*s", (int)decl->alias_len,
                            src_data + decl->alias_pos);
      alias = StringBuilder.toString(sb);
      StringBuilder.dispose(sb);
   } else {
      alias = derive_stem_alias(raw_path, raw_path_len);
   }
   if (!alias) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return false;
   }

   bool ok = load_import(bc, raw_path, raw_path_len, alias);
   Allocator.free(alias);
   return ok;
}

/* ------------------------------------------------------------------ */
/* load_import — core graph-walk step                                */
/* ------------------------------------------------------------------ */
static bool load_import(build_ctx_t *bc, const char *path, usize path_len,
                        const char *alias) {
   /* Build NUL-terminated path for map lookups and loader call */
   string_builder sb = StringBuilder.new(64);
   if (!sb) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return false;
   }
   StringBuilder.appendf(sb, "%.*s", (int)path_len, path);
   char *canonical = StringBuilder.toString(sb);
   StringBuilder.dispose(sb);
   if (!canonical) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return false;
   }

   /* Diamond dedup: same canonical already loaded → register alias only */
   usize existing = str_map_find(&bc->canonical_map, canonical);
   if (existing != (usize)-1) {
      Allocator.free(canonical);
      /* If alias is already registered for this same entry, pure diamond — done */
      usize alias_existing = str_map_find(&bc->alias_map, alias);
      if (alias_existing == existing)
         return true;
      /* Alias already registered for a different entry → duplicate alias error */
      if (alias_existing != (usize)-1) {
         anvl_error_set(ANVL_ERR_IMPORT_DUPLICATE_ALIAS,
                        anvl_error_code_message(ANVL_ERR_IMPORT_DUPLICATE_ALIAS),
                        0, 0, __FILE__);
         return false;
      }
      /* Register the extra alias pointing to the existing entry */
      string_builder sb2 = StringBuilder.new(64);
      if (!sb2) {
         anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                        anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                        0, 0, __FILE__);
         return false;
      }
      StringBuilder.appendf(sb2, "%s", alias);
      char *alias_copy = StringBuilder.toString(sb2);
      StringBuilder.dispose(sb2);
      if (!alias_copy) {
         anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                        anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                        0, 0, __FILE__);
         return false;
      }
      if (!str_map_insert(&bc->alias_map, alias_copy, existing)) {
         Allocator.free(alias_copy);
         anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                        anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                        0, 0, __FILE__);
         return false;
      }
      return true;
   }

   /* Duplicate alias check (different canonical) */
   if (str_map_find(&bc->alias_map, alias) != (usize)-1) {
      anvl_error_set(ANVL_ERR_IMPORT_DUPLICATE_ALIAS,
                     anvl_error_code_message(ANVL_ERR_IMPORT_DUPLICATE_ALIAS),
                     0, 0, __FILE__);
      Allocator.free(canonical);
      return false;
   }

   /* Cycle detection */
   if (load_stack_contains(&bc->stack, canonical)) {
      anvl_error_set(ANVL_ERR_IMPORT_CYCLIC,
                     anvl_error_code_message(ANVL_ERR_IMPORT_CYCLIC),
                     0, 0, __FILE__);
      Allocator.free(canonical);
      return false;
   }

   /* Call binding loader */
   const char *src_bytes = NULL;
   usize src_len = 0;
   if (!bc->loader(canonical, &src_bytes, &src_len, bc->userdata)) {
      anvl_error_set(ANVL_ERR_IMPORT_FILE_NOT_FOUND,
                     anvl_error_code_message(ANVL_ERR_IMPORT_FILE_NOT_FOUND),
                     0, 0, __FILE__);
      Allocator.free(canonical);
      return false;
   }

   /* Parse the loaded source */
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, src_bytes, src_len);
   context child_ctx = builder->build(builder);
   builder->dispose(builder);
   if (!child_ctx) {
      Allocator.free((void *)src_bytes);
      Allocator.free(canonical);
      /* Error already set by builder */
      return false;
   }
   if (!Context.parse(child_ctx)) {
      Context.dispose(child_ctx);
      Allocator.free((void *)src_bytes);
      Allocator.free(canonical);
      /* Error already set by parser */
      return false;
   }

   /* Ensure graph capacity */
   usize entry_idx = bc->graph->count;
   if (!graph_ensure_cap(bc->graph, entry_idx + 1)) {
      Context.dispose(child_ctx);
      Allocator.free((void *)src_bytes);
      Allocator.free(canonical);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return false;
   }

   /* Build owned alias copy */
   string_builder sb3 = StringBuilder.new(64);
   if (!sb3) {
      Context.dispose(child_ctx);
      Allocator.free((void *)src_bytes);
      Allocator.free(canonical);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return false;
   }
   StringBuilder.appendf(sb3, "%s", alias);
   char *alias_owned = StringBuilder.toString(sb3);
   StringBuilder.dispose(sb3);
   if (!alias_owned) {
      Context.dispose(child_ctx);
      Allocator.free((void *)src_bytes);
      Allocator.free(canonical);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return false;
   }

   /* Register in maps (non-owning pointers to entry strings) */
   if (!str_map_insert(&bc->alias_map, alias_owned, entry_idx) ||
       !str_map_insert(&bc->canonical_map, canonical, entry_idx)) {
      Allocator.free(alias_owned);
      Context.dispose(child_ctx);
      Allocator.free((void *)src_bytes);
      Allocator.free(canonical);
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return false;
   }

   /* Add entry to graph */
   bc->graph->entries[entry_idx] = (anvl_import_entry_t){
       .canonical = canonical,
       .alias = alias_owned,
       .ctx = child_ctx,
       .src = child_ctx->source,
   };
   bc->graph->count++;

   /* Push onto DFS stack and recurse for nested imports */
   if (!load_stack_push(&bc->stack, canonical)) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return false;
   }
   for (usize i = 0; i < child_ctx->import_list.count; i++) {
      if (!process_decl(bc, child_ctx, &child_ctx->import_list.decls[i])) {
         load_stack_pop(&bc->stack);
         return false;
      }
   }
   load_stack_pop(&bc->stack);

   return true;
}

/* ------------------------------------------------------------------ */
/* import_graph_build                                                 */
/* ------------------------------------------------------------------ */
static anvl_import_graph_t *import_graph_build(context ctx,
                                               const char *owner_path,
                                               anvl_import_loader_cb loader,
                                               void *userdata) {
   if (!ctx || !loader)
      return NULL;

   anvl_import_graph_t *graph = Allocator.alloc(sizeof(anvl_import_graph_t));
   if (!graph) {
      anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                     anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                     0, 0, __FILE__);
      return NULL;
   }
   memset(graph, 0, sizeof(anvl_import_graph_t));

   /* Short-circuit: no imports */
   if (ctx->import_list.count == 0)
      return graph;

   build_ctx_t bc = {
       .graph = graph,
       .stack = {0},
       .alias_map = {0},
       .canonical_map = {0},
       .loader = loader,
       .userdata = userdata,
   };

   /* Seed the DFS stack with the root file to block self-import */
   if (owner_path) {
      string_builder sb = StringBuilder.new(64);
      if (!sb) {
         Allocator.free(graph);
         anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                        anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                        0, 0, __FILE__);
         return NULL;
      }
      StringBuilder.appendf(sb, "%s", owner_path);
      char *root_canon = StringBuilder.toString(sb);
      StringBuilder.dispose(sb);
      if (!root_canon || !load_stack_push(&bc.stack, root_canon)) {
         if (root_canon)
            Allocator.free(root_canon);
         Allocator.free(graph);
         anvl_error_set(ANVL_ERR_MEMORY_ALLOCATION_FAILED,
                        anvl_error_code_message(ANVL_ERR_MEMORY_ALLOCATION_FAILED),
                        0, 0, __FILE__);
         return NULL;
      }
      /* Pop the sentinel after the loop — store ptr for cleanup */
      /* Process root imports */
      bool ok = true;
      for (usize i = 0; i < ctx->import_list.count; i++) {
         if (!process_decl(&bc, ctx, &ctx->import_list.decls[i])) {
            ok = false;
            break;
         }
      }
      load_stack_pop(&bc.stack);
      Allocator.free(root_canon);
      if (!ok) {
         load_stack_free(&bc.stack);
         str_map_free(&bc.alias_map);
         str_map_free(&bc.canonical_map);
         ImportGraph.dispose(graph);
         return NULL;
      }
   } else {
      for (usize i = 0; i < ctx->import_list.count; i++) {
         if (!process_decl(&bc, ctx, &ctx->import_list.decls[i])) {
            load_stack_free(&bc.stack);
            str_map_free(&bc.alias_map);
            str_map_free(&bc.canonical_map);
            ImportGraph.dispose(graph);
            return NULL;
         }
      }
   }

   load_stack_free(&bc.stack);
   str_map_free(&bc.alias_map);
   str_map_free(&bc.canonical_map);
   return graph;
}

/* ------------------------------------------------------------------ */
/* import_graph_dispose                                               */
/* ------------------------------------------------------------------ */
static void import_graph_dispose(anvl_import_graph_t *graph) {
   if (!graph)
      return;
   for (usize i = 0; i < graph->count; i++) {
      anvl_import_entry_t *e = &graph->entries[i];
      if (e->canonical)
         Allocator.free(e->canonical);
      if (e->alias)
         Allocator.free(e->alias);
      if (e->ctx)
         Context.dispose(e->ctx);
      /* e->src is owned by e->ctx — already freed above */
   }
   if (graph->entries)
      Allocator.free(graph->entries);
   Allocator.free(graph);
}

/* ------------------------------------------------------------------ */
/* Public vtable                                                      */
/* ------------------------------------------------------------------ */
const anvl_import_graph_i ImportGraph = {
    .build = import_graph_build,
    .dispose = import_graph_dispose,
};
