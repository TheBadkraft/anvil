/*
 * test_anon_block.c - Tests for anonymous top-level object blocks (ANVL_ANON_OBJECT)
 *
 * Implements FR-2603-anvil-001 test cases AB01-AB10.
 *
 * Coverage:
 *   AB01  basic parse: statement count, stmt type, field count
 *   AB02  field access by name
 *   AB03  braced dotted-path VarRef (${changelog.version})
 *   AB04  redeclaration error (anon block then assignment)
 *   AB05  duplicate block name error
 *   AB06  serializer round-trip: emits `ident {` not `ident := {`
 *   AB07  get_statement_by_name finds the block
 *   AB08  no-shebang (permissive) also accepts the syntax
 *   AB09  unbraced dotted-path VarRef ($changelog.version)
 *   AB10  unterminated braced VarRef error (${... no closing })
 */

#include "../src/core/context_internal.h"
#include "anvil.h"
#include "serializer.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Forward declarations
 * ======================================================================== */

static void test_AB01_basic_parse(void);
static void test_AB02_field_access_by_name(void);
static void test_AB03_braced_dotpath_varref(void);
static void test_AB04_redeclaration_error(void);
static void test_AB05_duplicate_block_error(void);
static void test_AB06_serializer_round_trip(void);
static void test_AB07_get_statement_by_name(void);
static void test_AB08_no_shebang(void);
static void test_AB09_unbraced_dotpath_varref(void);
static void test_AB10_unterminated_braced_varref(void);

/* ========================================================================
 * Helpers
 * ======================================================================== */

static context parse_ok(const char *src_str) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   if (!builder)
      return NULL;

   builder->set_source(builder, src_str, strlen(src_str));
   context ctx = builder->build(builder);
   if (!ctx) {
      builder->dispose(builder);
      return NULL;
   }

   if (!Context.parse(ctx)) {
      const anvl_error_state *err = Anvil.error_get();
      if (err)
         fprintf(stderr, "parse_ok: parse failed — %s (code %d)\n", err->message, err->code);
      builder->dispose(builder);
      Context.dispose(ctx);
      return NULL;
   }

   builder->dispose(builder);
   return ctx;
}

static bool parse_fails(const char *src_str, anvl_error_code *out_code) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   if (!builder)
      return false;

   builder->set_source(builder, src_str, strlen(src_str));
   context ctx = builder->build(builder);
   if (!ctx) {
      builder->dispose(builder);
      return false;
   }

   bool ok = Context.parse(ctx);
   if (ok) {
      builder->dispose(builder);
      Context.dispose(ctx);
      return false;
   }

   const anvl_error_state *err = Anvil.error_get();
   if (out_code && err)
      *out_code = err->code;

   builder->dispose(builder);
   Context.dispose(ctx);
   return true;
}

static void set_config(FILE **logger) {
   *logger = fopen("logs/test_anon_block.log", "w");
}

static void set_teardown(void) {
   Anvil.cleanup();
}

static void teardown(void) {
   Anvil.error_clear();
}

/* ========================================================================
 * AB01 — basic parse
 * ======================================================================== */

static void test_AB01_basic_parse(void) {
   const char *src =
       "#!aml\n"
       "changelog {\n"
       "    version := 1.0.0\n"
       "    date    := 2026-03-23\n"
       "}\n";

   context ctx = parse_ok(src);
   Assert.isNotNull(ctx, "AB01: parse should succeed");
   if (!ctx)
      return;

   usize count = Context.statement_count(ctx);
   writelnf("AB01: statement_count = %zu (expected 1)", count);
   Assert.isTrue(count == 1, "AB01: exactly one statement");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "AB01: get_statement(ctx, 0) != NULL");
   if (!stmt) {
      Context.dispose(ctx);
      teardown();
      return;
   }

   anvl_stmt_type stype = Statement.type(stmt);
   writelnf("AB01: stmt type = %d (expected ANVL_ANON_OBJECT=%d)", stype, ANVL_ANON_OBJECT);
   Assert.isTrue(stype == ANVL_ANON_OBJECT, "AB01: stmt type must be ANVL_ANON_OBJECT");

   usize fc = Context.field_count(ctx, stmt);
   writelnf("AB01: field_count = %zu (expected 2)", fc);
   Assert.isTrue(fc == 2, "AB01: anonymous block has 2 fields");

   Context.dispose(ctx);
   teardown();
}

/* ========================================================================
 * AB02 — field access by name
 * ======================================================================== */

static void test_AB02_field_access_by_name(void) {
   const char *src =
       "#!aml\n"
       "changelog {\n"
       "    version := 1.0.0\n"
       "    date    := 2026-03-23\n"
       "}\n";

   context ctx = parse_ok(src);
   Assert.isNotNull(ctx, "AB02: parse should succeed");
   if (!ctx)
      return;

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt, "AB02: get_statement(ctx, 0) != NULL");
   if (!stmt) {
      Context.dispose(ctx);
      teardown();
      return;
   }

   field f = Context.get_field_by_name(ctx, stmt, "version", 7);
   Assert.isNotNull(f, "AB02: get_field_by_name('version') != NULL");
   if (f) {
      char *val_str = malloc(f->val->data.scalar.len + 1);
      Source.substring(ctx->source, f->val->data.scalar.pos, f->val->data.scalar.len, val_str);
      writelnf("AB02: version value = '%s' (expected '1.0.0')", val_str ? val_str : "(null)");
      Assert.isTrue(val_str && strcmp(val_str, "1.0.0") == 0, "AB02: version value is '1.0.0'");
      free(val_str);
   }

   Context.dispose(ctx);
   teardown();
}

/* ========================================================================
 * AB03 — braced dotted-path VarRef (${changelog.version})
 * ======================================================================== */

static void test_AB03_braced_dotpath_varref(void) {
   const char *src =
       "#!aml\n"
       "changelog { version := 1.0.0 }\n"
       "display := ${changelog.version}\n";

   context ctx = parse_ok(src);
   Assert.isNotNull(ctx, "AB03: parse should succeed");
   if (!ctx)
      return;

   usize count = Context.statement_count(ctx);
   writelnf("AB03: statement_count = %zu (expected 2)", count);
   Assert.isTrue(count == 2, "AB03: two statements (block + assignment)");

   statement anon = Context.get_statement(ctx, 0);
   statement display = Context.get_statement(ctx, 1);
   Assert.isNotNull(anon, "AB03: first stmt (changelog block)");
   Assert.isNotNull(display, "AB03: second stmt (display)");

   if (anon)
      Assert.isTrue(Statement.type(anon) == ANVL_ANON_OBJECT, "AB03: first stmt is ANVL_ANON_OBJECT");

   if (display && display->value_meta) {
      Assert.isTrue(display->value_meta->type == ANVL_VALUE_VARREF,
                    "AB03: display value is VARREF");

      /* The stored identifier span should be "changelog.version" (no $ or {}) */
      char *path = malloc(display->value_meta->len + 1);
      Source.substring(ctx->source,
                       display->value_meta->pos,
                       display->value_meta->len, path);
      writelnf("AB03: varref path = '%s' (expected 'changelog.version')",
               path ? path : "(null)");
      Assert.isTrue(path && strcmp(path, "changelog.version") == 0,
                    "AB03: varref path is 'changelog.version'");
      free(path);
   } else {
      Assert.isTrue(false, "AB03: display->value_meta must not be NULL");
   }

   Context.dispose(ctx);
   teardown();
}

/* ========================================================================
 * AB04 — redeclaration error (anon block then assignment)
 * ======================================================================== */

static void test_AB04_redeclaration_error(void) {
   const char *src =
       "#!aml\n"
       "changelog { version := 1 }\n"
       "changelog := 2\n";

   anvl_error_code code = ANVL_ERR_NONE;
   bool failed = parse_fails(src, &code);

   writelnf("AB04: parse failed = %s, error code = %d (expected %d)", failed ? "true" : "false",
            code, ANVL_ERR_ANON_BLOCK_REDECLARATION);
   Assert.isTrue(failed, "AB04: parse must fail on redeclaration");
   Assert.isTrue(code == ANVL_ERR_ANON_BLOCK_REDECLARATION,
                 "AB04: error code must be ANVL_ERR_ANON_BLOCK_REDECLARATION");

   teardown();
}

/* ========================================================================
 * AB05 — duplicate anonymous block name error
 * ======================================================================== */

static void test_AB05_duplicate_block_error(void) {
   const char *src =
       "#!aml\n"
       "changelog { version := 1 }\n"
       "changelog { version := 2 }\n";

   anvl_error_code code = ANVL_ERR_NONE;
   bool failed = parse_fails(src, &code);

   writelnf("AB05: parse failed = %s, error code = %d (expected %d)", failed ? "true" : "false",
            code, ANVL_ERR_ANON_BLOCK_REDECLARATION);
   Assert.isTrue(failed, "AB05: parse must fail on duplicate block name");
   Assert.isTrue(code == ANVL_ERR_ANON_BLOCK_REDECLARATION,
                 "AB05: error code must be ANVL_ERR_ANON_BLOCK_REDECLARATION");

   teardown();
}

/* ========================================================================
 * AB06 — serializer round-trip: emits `ident {` not `ident := {`
 * ======================================================================== */

static void test_AB06_serializer_round_trip(void) {
   const char *src =
       "#!aml\n"
       "changelog {\n"
       "    version := 1.0.0\n"
       "    date    := 2026-03-23\n"
       "}\n";

   context ctx = parse_ok(src);
   Assert.isNotNull(ctx, "AB06: parse should succeed");
   if (!ctx)
      return;

   string_builder sb = Serializer.serialize(ctx, &ANVL_SERIALIZER_DEFAULT);
   Assert.isNotNull(sb, "AB06: serializer produced output");
   if (!sb) {
      Context.dispose(ctx);
      teardown();
      return;
   }

   char *output = (char *)StringBuilder.toString(sb);
   StringBuilder.dispose(sb);
   Assert.isNotNull(output, "AB06: serialized string is not NULL");

   if (output) {
      writelnf("AB06: serialized output:\n%s", output);

      /* Must contain `changelog {` (anon block form) */
      bool has_block_form = strstr(output, "changelog {") != NULL;
      Assert.isTrue(has_block_form, "AB06: output contains 'changelog {'");

      /* Must NOT contain `changelog :=` (assignment form) */
      bool has_assign_form = strstr(output, "changelog :=") != NULL;
      Assert.isFalse(has_assign_form, "AB06: output must NOT contain 'changelog :='");

      /* Round-trip: re-parse the emitted text */
      context ctx2 = parse_ok(output);
      Assert.isNotNull(ctx2, "AB06: re-parsed serialized output");
      if (ctx2) {
         usize count = Context.statement_count(ctx2);
         Assert.isTrue(count == 1, "AB06: re-parsed still has 1 statement");

         statement stmt2 = Context.get_statement(ctx2, 0);
         if (stmt2)
            Assert.isTrue(Statement.type(stmt2) == ANVL_ANON_OBJECT,
                          "AB06: re-parsed stmt type is still ANVL_ANON_OBJECT");

         Context.dispose(ctx2);
      }

      Allocator.dispose(output);
   }

   Context.dispose(ctx);
   teardown();
}

/* ========================================================================
 * AB07 — get_statement_by_name finds the block
 * ======================================================================== */

static void test_AB07_get_statement_by_name(void) {
   const char *src =
       "#!aml\n"
       "changelog {\n"
       "    version := 1.0.0\n"
       "    date    := 2026-03-23\n"
       "}\n";

   context ctx = parse_ok(src);
   Assert.isNotNull(ctx, "AB07: parse should succeed");
   if (!ctx)
      return;

   statement stmt = Context.get_statement_by_name(ctx, "changelog", 9);
   writelnf("AB07: get_statement_by_name('changelog') = %p (expected non-NULL)", (void *)stmt);
   Assert.isNotNull(stmt, "AB07: get_statement_by_name must find the anonymous block");

   if (stmt)
      Assert.isTrue(Statement.type(stmt) == ANVL_ANON_OBJECT, "AB07: found stmt is ANVL_ANON_OBJECT");

   Context.dispose(ctx);
   teardown();
}

/* ========================================================================
 * AB08 — no-shebang (permissive) also accepts the syntax
 * ======================================================================== */

static void test_AB08_no_shebang(void) {
   const char *src =
       "changelog {\n"
       "    version := 1.0.0\n"
       "}\n";

   context ctx = parse_ok(src);
   Assert.isNotNull(ctx, "AB08: parse without shebang should succeed");
   if (!ctx)
      return;

   usize count = Context.statement_count(ctx);
   writelnf("AB08: statement_count = %zu (expected 1)", count);
   Assert.isTrue(count == 1, "AB08: one statement parsed");

   statement stmt = Context.get_statement(ctx, 0);
   if (stmt)
      Assert.isTrue(Statement.type(stmt) == ANVL_ANON_OBJECT, "AB08: stmt type is ANVL_ANON_OBJECT");

   Context.dispose(ctx);
   teardown();
}

/* ========================================================================
 * AB09 — unbraced dotted-path VarRef ($changelog.version)
 *
 * Since '.' is an identifier_part, $changelog.version parses as a single
 * VARREF with path "changelog.version" — same semantics as the braced form.
 * ======================================================================== */

static void test_AB09_unbraced_dotpath_varref(void) {
   const char *src =
       "#!aml\n"
       "changelog { version := 1.0.0 }\n"
       "display := $changelog.version\n";

   context ctx = parse_ok(src);
   Assert.isNotNull(ctx, "AB09: parse should succeed");
   if (!ctx)
      return;

   statement display = Context.get_statement(ctx, 1);
   Assert.isNotNull(display, "AB09: second stmt (display)");

   if (display && display->value_meta) {
      Assert.isTrue(display->value_meta->type == ANVL_VALUE_VARREF,
                    "AB09: display value is VARREF");

      char *path = malloc(display->value_meta->len + 1);
      Source.substring(ctx->source,
                       display->value_meta->pos,
                       display->value_meta->len, path);
      writelnf("AB09: varref path = '%s' (expected 'changelog.version')",
               path ? path : "(null)");
      Assert.isTrue(path && strcmp(path, "changelog.version") == 0,
                    "AB09: unbraced varref path is 'changelog.version'");
      free(path);
   } else {
      Assert.isTrue(false, "AB09: display->value_meta must not be NULL");
   }

   Context.dispose(ctx);
   teardown();
}

/* ========================================================================
 * AB10 — unterminated braced VarRef: ${... no closing }
 * ======================================================================== */

static void test_AB10_unterminated_braced_varref(void) {
   const char *src =
       "#!aml\n"
       "display := ${changelog.version\n";

   anvl_error_code code = ANVL_ERR_NONE;
   bool failed = parse_fails(src, &code);

   writelnf("AB10: parse failed = %s, error code = %d (expected %d)",
            failed ? "true" : "false", code,
            ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF);
   Assert.isTrue(failed, "AB10: must fail on unterminated braced VarRef");
   Assert.isTrue(code == ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF,
                 "AB10: error code must be ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF");

   teardown();
}

/* ========================================================================
 * Registration
 * ======================================================================== */

static void _register(void) {
   testset("Anonymous Block (ANVL_ANON_OBJECT)", set_config, set_teardown);

   testcase("AB01 basic parse", test_AB01_basic_parse);
   testcase("AB02 field access by name", test_AB02_field_access_by_name);
   testcase("AB03 braced dotted-path VarRef", test_AB03_braced_dotpath_varref);
   testcase("AB04 redeclaration error", test_AB04_redeclaration_error);
   testcase("AB05 duplicate block name error", test_AB05_duplicate_block_error);
   testcase("AB06 serializer round-trip", test_AB06_serializer_round_trip);
   testcase("AB07 get_statement_by_name", test_AB07_get_statement_by_name);
   testcase("AB08 no-shebang permissive", test_AB08_no_shebang);
   testcase("AB09 unbraced dotted-path VarRef", test_AB09_unbraced_dotpath_varref);
   testcase("AB10 unterminated braced VarRef error", test_AB10_unterminated_braced_varref);
}

__attribute__((constructor)) static void register_test_anon_block(void) {
   Tests.enqueue(_register);
}
