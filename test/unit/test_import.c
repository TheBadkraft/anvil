/*
 * test_import.c — TDD tests for port/import-graph (anvil.import)
 *
 * Tests the import declaration parser and the ImportGraph DAG builder.
 *
 * Test groups:
 *   I01–I04  Parser-level: import syntax, placement guards
 *   I05–I10  Graph builder: single import, aliases, dedup, errors
 */

#include "anvil.h"
#include "import.h"
#include "utilities/helpers.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */
static void test_i01_no_imports_parses_ok(void);
static void test_i02_single_import_stored(void);
static void test_i03_import_with_explicit_alias_stored(void);
static void test_i04_import_after_statement_is_error(void);
static void test_i05_graph_build_no_imports_returns_empty(void);
static void test_i06_graph_build_single_import_no_alias(void);
static void test_i07_graph_build_single_import_explicit_alias(void);
static void test_i08_graph_build_duplicate_alias_is_error(void);
static void test_i09_graph_build_cyclic_import_is_error(void);
static void test_i10_graph_build_diamond_dedup(void);

/* ------------------------------------------------------------------ */
/* Test setup / teardown                                              */
/* ------------------------------------------------------------------ */
static void set_config(FILE **logger) {
   *logger = fopen("logs/test_import.log", "w");
}
static void set_teardown(void) {
   Anvil.cleanup();
}
static void teardown(void) {
   Anvil.error_clear();
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */
static context build_context(const char *src_str) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, src_str, strlen(src_str));
   context ctx = builder->build(builder);
   builder->dispose(builder);
   if (!ctx)
      return NULL;
   bool ok = Context.parse(ctx);
   if (!ok) {
      Context.dispose(ctx);
      return NULL;
   }
   return ctx;
}

/* ------------------------------------------------------------------ */
/* Mock loader infrastructure                                        */
/* ------------------------------------------------------------------ */
typedef struct {
   const char *path;   /* the path this entry answers to */
   const char *source; /* static source string to return */
   usize source_len;
} mock_entry_t;

typedef struct {
   const mock_entry_t *entries;
   usize count;
} mock_loader_t;

static bool mock_load(const char *path, const char **out_src, usize *out_len,
                      void *userdata) {
   const mock_loader_t *m = (const mock_loader_t *)userdata;
   for (usize i = 0; i < m->count; i++) {
      if (strcmp(m->entries[i].path, path) == 0) {
         /* Copy source to heap (ImportGraph.dispose will Allocator.dispose it) */
         usize len = m->entries[i].source_len;
         char *buf = Allocator.alloc(len + 1);
         if (!buf)
            return false;
         memcpy(buf, m->entries[i].source, len);
         buf[len] = '\0';
         *out_src = buf;
         *out_len = len;
         return true;
      }
   }
   return false; /* not found */
}

/* ================================================================
 * I01 — Source with no imports parses cleanly
 * ================================================================ */
static void test_i01_no_imports_parses_ok(void) {
   const char *src =
       "#!aml\n"
       "name := world\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context parses ok");
   Assert.isTrue(ctx->import_list.count == 0, "no import declarations");

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * I02 — Single import declaration is stored in context
 * ================================================================ */
static void test_i02_single_import_stored(void) {
   const char *src =
       "#!aml\n"
       "import \"blocks/terrain.aml\"\n"
       "name := world\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context parses ok");
   Assert.isTrue(ctx->import_list.count == 1, "one import declaration");

   const struct anvl_import_decl *d = &ctx->import_list.decls[0];
   char *path = Source.substring(ctx->source, d->path_pos, d->path_len);
   Assert.isTrue(strcmp(path, "blocks/terrain.aml") == 0, "path is 'blocks/terrain.aml'");
   Allocator.dispose(path);

   Assert.isTrue(d->alias_len == 0, "no explicit alias");

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * I03 — Import with explicit "as alias" is stored
 * ================================================================ */
static void test_i03_import_with_explicit_alias_stored(void) {
   const char *src =
       "#!aml\n"
       "import \"blocks/terrain.aml\" as terrain\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context parses ok");
   Assert.isTrue(ctx->import_list.count == 1, "one import declaration");

   const struct anvl_import_decl *d = &ctx->import_list.decls[0];
   char *alias = Source.substring(ctx->source, d->alias_pos, d->alias_len);
   Assert.isTrue(strcmp(alias, "terrain") == 0, "alias is 'terrain'");
   Allocator.dispose(alias);

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * I04 — Import appearing after a statement is a parse error
 * ================================================================ */
static void test_i04_import_after_statement_is_error(void) {
   const char *src =
       "#!aml\n"
       "name := world\n"
       "import \"blocks/terrain.aml\"\n";

   context ctx = build_context(src);
   Assert.isNull(ctx, "parse should fail");
   Assert.isTrue(Anvil.error_is_set(), "error is set");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_IMPORT_NOT_FIRST,
                 "error is ANVL_ERR_IMPORT_NOT_FIRST");

   teardown();
}

/* ================================================================
 * I05 — BuildGraph with no imports returns empty (non-NULL) graph
 * ================================================================ */
static void test_i05_graph_build_no_imports_returns_empty(void) {
   const char *src =
       "#!aml\n"
       "name := world\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context parses ok");

   static const mock_loader_t loader = {NULL, 0};
   anvl_import_graph_t *graph =
       ImportGraph.build(ctx, NULL, mock_load, (void *)&loader);

   Assert.isNotNull(graph, "graph is not NULL for empty import list");
   Assert.isTrue(graph->count == 0, "graph has 0 entries");

   ImportGraph.dispose(graph);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * I06 — Single import, stem alias auto-derived
 * ================================================================ */
static void test_i06_graph_build_single_import_no_alias(void) {
   const char *src =
       "#!aml\n"
       "import \"blocks/my-terrain.aml\"\n";

   const char *child_src = "#!aml\n";
   static mock_entry_t entries[] = {
       {"blocks/my-terrain.aml", "#!aml\n", 6},
   };
   static const mock_loader_t loader = {entries, 1};

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "root context parses ok");

   anvl_import_graph_t *graph =
       ImportGraph.build(ctx, NULL, mock_load, (void *)&loader);

   Assert.isNotNull(graph, "graph built");
   Assert.isTrue(graph->count == 1, "graph has 1 entry");
   Assert.isTrue(strcmp(graph->entries[0].canonical, "blocks/my-terrain.aml") == 0,
                 "canonical path correct");
   Assert.isTrue(strcmp(graph->entries[0].alias, "my_terrain") == 0,
                 "stem alias derived: 'my_terrain'");

   (void)child_src;
   ImportGraph.dispose(graph);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * I07 — Single import with explicit alias
 * ================================================================ */
static void test_i07_graph_build_single_import_explicit_alias(void) {
   const char *src =
       "#!aml\n"
       "import \"blocks/terrain.aml\" as t\n";

   static mock_entry_t entries[] = {
       {"blocks/terrain.aml", "#!aml\n", 6},
   };
   static const mock_loader_t loader = {entries, 1};

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "root context parses ok");

   anvl_import_graph_t *graph =
       ImportGraph.build(ctx, NULL, mock_load, (void *)&loader);

   Assert.isNotNull(graph, "graph built");
   Assert.isTrue(graph->count == 1, "graph has 1 entry");
   Assert.isTrue(strcmp(graph->entries[0].alias, "t") == 0,
                 "explicit alias 't' preserved");

   ImportGraph.dispose(graph);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * I08 — Two imports with the same alias → duplicate alias error
 * ================================================================ */
static void test_i08_graph_build_duplicate_alias_is_error(void) {
   const char *src =
       "#!aml\n"
       "import \"a.aml\" as shared\n"
       "import \"b.aml\" as shared\n";

   static mock_entry_t entries[] = {
       {"a.aml", "#!aml\n", 6},
       {"b.aml", "#!aml\n", 6},
   };
   static const mock_loader_t loader = {entries, 2};

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "root context parses ok (duplicate caught at graph build)");

   anvl_import_graph_t *graph =
       ImportGraph.build(ctx, NULL, mock_load, (void *)&loader);

   Assert.isNull(graph, "graph build fails on duplicate alias");
   Assert.isTrue(Anvil.error_is_set(), "error is set");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_IMPORT_DUPLICATE_ALIAS,
                 "error is ANVL_ERR_IMPORT_DUPLICATE_ALIAS");

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * I09 — Cyclic import: A imports B imports A → cycle error
 * ================================================================ */
static void test_i09_graph_build_cyclic_import_is_error(void) {
   /* a.aml imports b.aml; b.aml imports a.aml */
   const char *src_a =
       "#!aml\n"
       "import \"b.aml\"\n";
   const char *src_b =
       "#!aml\n"
       "import \"a.aml\"\n";

   static mock_entry_t entries[] = {
       {"b.aml", "#!aml\nimport \"a.aml\"\n", 20},
   };
   static const mock_loader_t loader = {entries, 1};

   /* Root is treated as "a.aml" via owner_path */
   context ctx = build_context(src_a);
   Assert.isNotNull(ctx, "root context parses ok");

   anvl_import_graph_t *graph =
       ImportGraph.build(ctx, "a.aml", mock_load, (void *)&loader);

   Assert.isNull(graph, "graph build fails on cycle");
   Assert.isTrue(Anvil.error_is_set(), "error is set");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_IMPORT_CYCLIC,
                 "error is ANVL_ERR_IMPORT_CYCLIC");

   (void)src_b;
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * I10 — Diamond dedup: A imports B and C, both import D → D once
 * ================================================================ */
static void test_i10_graph_build_diamond_dedup(void) {
   /* Root imports b.aml and c.aml; b.aml imports d.aml; c.aml imports d.aml */
   const char *src_root =
       "#!aml\n"
       "import \"b.aml\"\n"
       "import \"c.aml\"\n";

   static mock_entry_t entries[] = {
       {"b.aml", "#!aml\nimport \"d.aml\"\n", 20},
       {"c.aml", "#!aml\nimport \"d.aml\"\n", 20},
       {"d.aml", "#!aml\n", 6},
   };
   static const mock_loader_t loader = {entries, 3};

   context ctx = build_context(src_root);
   Assert.isNotNull(ctx, "root context parses ok");

   anvl_import_graph_t *graph =
       ImportGraph.build(ctx, NULL, mock_load, (void *)&loader);

   Assert.isNotNull(graph, "graph built");
   /* Expect entries: b, d, c — d is NOT duplicated */
   Assert.isTrue(graph->count == 3, "graph has exactly 3 entries (b, d, c)");

   /* Verify d.aml appears exactly once */
   usize d_count = 0;
   for (usize i = 0; i < graph->count; i++) {
      if (strcmp(graph->entries[i].canonical, "d.aml") == 0)
         d_count++;
   }
   Assert.isTrue(d_count == 1, "d.aml appears exactly once (diamond dedup)");

   ImportGraph.dispose(graph);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * Registration
 * ================================================================ */
__attribute__((constructor)) static void register_test_import(void) {
   testset("Import Tests", set_config, set_teardown);

   testcase("I01: No imports parses ok", test_i01_no_imports_parses_ok);
   testcase("I02: Single import stored", test_i02_single_import_stored);
   testcase("I03: Import with explicit alias stored", test_i03_import_with_explicit_alias_stored);
   testcase("I04: Import after statement is error", test_i04_import_after_statement_is_error);
   testcase("I05: Graph build no imports returns empty", test_i05_graph_build_no_imports_returns_empty);
   testcase("I06: Graph build single import stem alias", test_i06_graph_build_single_import_no_alias);
   testcase("I07: Graph build single import explicit alias", test_i07_graph_build_single_import_explicit_alias);
   testcase("I08: Graph build duplicate alias is error", test_i08_graph_build_duplicate_alias_is_error);
   testcase("I09: Graph build cyclic import is error", test_i09_graph_build_cyclic_import_is_error);
   testcase("I10: Graph build diamond dedup", test_i10_graph_build_diamond_dedup);
}
