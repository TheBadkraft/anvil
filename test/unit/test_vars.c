/*
 * test_vars.c — TDD tests for port/vars (anvil.vars)
 *
 * Contracts derived from VarsTests.cs (Anvil.Net reference).
 *
 * Test groups:
 *   V01–V07  Parser-level: vars block structure, keyword guard
 *   V08–V15  Runtime resolution: VarRef -> vars entry lookup
 *   V16–V20  Interpolation materialisation: $"…{ref}…"
 *
 * Design divergences from .Net reference (intentional):
 *   - Interpolation holes use {ref} not {.ref}
 *   - No exceptions — errors set via anvl_error_set(), returned NULL
 *   - Circular ref detection: Vars.build() returns NULL + sets error
 *   - Missing ref: Vars.resolve() / Vars.materialise_interp() returns NULL + sets error
 */

#include "anvil.h"
#include "vars.h"
#include <sigma.memory/memory.h>
#include "testbit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bridge legacy Assert.* calls to TestBit assertions. */
static struct {
   void (*isTrue)(bool value, const char *message);
   void (*isFalse)(bool value, const char *message);
   void (*isNotNull)(const void *value, const char *message);
   void (*isNull)(const void *value, const char *message);
} Assert;

static void init_assert_bridge(void) {
   Assert.isTrue = TestBit.is_true;
   Assert.isFalse = TestBit.is_false;
   Assert.isNotNull = TestBit.is_not_null;
   Assert.isNull = TestBit.is_null;
}

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */
static void test_v01_empty_vars_block_parses_ok(void);
static void test_v02_single_entry_stored(void);
static void test_v03_multiple_entries_stored(void);
static void test_v04_vars_after_statement_is_error(void);
static void test_v05_second_vars_block_is_error(void);
static void test_v06_duplicate_key_is_error(void);
static void test_v07_vars_as_statement_identifier_is_error(void);
static void test_v08_scalar_varref_resolves(void);
static void test_v09_varref_in_object_field_resolves(void);
static void test_v10_varref_to_object_forwards(void);
static void test_v11_varref_to_array_forwards(void);
static void test_v12_varref_in_derived_object_resolves(void);
static void test_v13_varref_chain_resolves_to_scalar(void);
static void test_v14_missing_varref_error_at_resolve(void);
static void test_v15_circular_varref_error_at_build(void);
static void test_v16_single_ref_interpolation(void);
static void test_v17_multi_ref_interpolation(void);
static void test_v18_literal_only_interpolation(void);
static void test_v19_escaped_braces_materialise(void);
static void test_v20_missing_ref_in_interpolation(void);
static void test_v21_braced_dotted_varref_parses(void);
static void test_v22_unbraced_dotted_varref_parses(void);
static void test_v23_unterminated_braced_varref_error(void);

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */
static void teardown(void) {
   Anvil.error_clear();
}

static context build_context(const char *src_str) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, src_str, strlen(src_str));
   context ctx = builder->build(builder);
   if (!ctx)
      return NULL;
   bool ok = Context.parse(ctx);
   if (!ok) {
      Context.dispose(ctx);
      return NULL;
   }
   return ctx;
}

static bool parse_fails(const char *src_str, anvl_error_code *out_code) {
   anvl_ctx_builder_i *builder = Context.get_builder();
   builder->set_source(builder, src_str, strlen(src_str));
   context ctx = builder->build(builder);
   if (!ctx)
      return false;

   bool ok = Context.parse(ctx);
   if (ok) {
      Context.dispose(ctx);
      return false;
   }

   if (out_code) {
      const anvl_error_state *err = Anvil.error_get();
      *out_code = err ? err->code : ANVL_ERR_NONE;
   }

   Context.dispose(ctx);
   return true;
}

/* Resolve a top-level VarRef statement's value to a source string.
 * Returns heap string (caller frees with free()) or NULL on error. */
static char *resolve_stmt_varref(context ctx, anvl_vars_state_t *vs, usize stmt_idx) {
   statement stmt = Context.get_statement(ctx, stmt_idx);
   if (!stmt || !stmt->value_meta)
      return NULL;
   if (stmt->value_meta->type != ANVL_VALUE_VARREF)
      return NULL;
   usize rpos, rlen;
   anvl_value_type rtype;
   if (!Vars.resolve(vs, Source.data(ctx->source) + stmt->value_meta->pos,
                     stmt->value_meta->len, &rpos, &rlen, &rtype))
      return NULL;
   char *result = malloc(rlen + 1);
   Source.substring(ctx->source, rpos, rlen, result);
   return result;
}

/* ================================================================
 * V01 — Empty vars block parses without error
 * ================================================================ */
static void test_v01_empty_vars_block_parses_ok(void) {
   const char *src =
       "#!aml\n"
       "vars { }\n"
       "name := test\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should parse with empty vars block");
   Assert.isTrue(Context.statement_count(ctx) == 1, "1 statement (not vars block)");

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V02 — Single entry stored in vars
 * ================================================================ */
static void test_v02_single_entry_stored(void) {
   const char *src =
       "#!aml\n"
       "vars { atlas := terrain.png }\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");
   Assert.isTrue(ctx->vars_list.count == 1, "vars list has 1 entry");

   const struct anvl_vars_entry *e = &ctx->vars_list.entries[0];
   char *key = malloc(e->key_len + 1);
   Source.substring(ctx->source, e->key_pos, e->key_len, key);
   Assert.isTrue(strcmp(key, "atlas") == 0, "key is 'atlas'");
   free(key);

   char *val = malloc(e->value_len + 1);
   Source.substring(ctx->source, e->value_pos, e->value_len, val);
   Assert.isTrue(strcmp(val, "terrain.png") == 0, "value is 'terrain.png'");
   free(val);

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V03 — Multiple entries all stored
 * ================================================================ */
static void test_v03_multiple_entries_stored(void) {
   const char *src =
       "#!aml\n"
       "vars {\n"
       "    atlas   := terrain.png\n"
       "    version := 2.0\n"
       "    scale   := 1.5\n"
       "}\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");
   Assert.isTrue(ctx->vars_list.count == 3, "vars list has 3 entries");

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V04 — vars block after a statement is a parse error
 * ================================================================ */
static void test_v04_vars_after_statement_is_error(void) {
   const char *src =
       "#!aml\n"
       "name := test\n"
       "vars { atlas := terrain.png }\n";

   context ctx = build_context(src);
   Assert.isNull(ctx, "parse should fail when vars follows a statement");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_VARS_NOT_FIRST,
                 "error code should be ANVL_ERR_VARS_NOT_FIRST");

   teardown();
}

/* ================================================================
 * V05 — Second vars block is a parse error
 * ================================================================ */
static void test_v05_second_vars_block_is_error(void) {
   const char *src =
       "#!aml\n"
       "vars { a := 1 }\n"
       "vars { b := 2 }\n";

   context ctx = build_context(src);
   Assert.isNull(ctx, "parse should fail on second vars block");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED,
                 "error code should be ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED");

   teardown();
}

/* ================================================================
 * V06 — Duplicate key inside vars is a parse error
 * ================================================================ */
static void test_v06_duplicate_key_is_error(void) {
   const char *src =
       "#!aml\n"
       "vars { atlas := a, atlas := b }\n";

   context ctx = build_context(src);
   Assert.isNull(ctx, "parse should fail on duplicate vars key");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_VARS_DUPLICATE_KEY,
                 "error code should be ANVL_ERR_VARS_DUPLICATE_KEY");

   teardown();
}

/* ================================================================
 * V07 — 'vars' as a statement identifier is a parse error
 * ================================================================ */
static void test_v07_vars_as_statement_identifier_is_error(void) {
   const char *src =
       "#!aml\n"
       "vars := \"not allowed\"\n";

   context ctx = build_context(src);
   Assert.isNull(ctx, "parse should fail when 'vars' used as identifier");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD,
                 "error code should be ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD");

   teardown();
}

/* ================================================================
 * V08 — Scalar VarRef resolves to correct value
 * ================================================================ */
static void test_v08_scalar_varref_resolves(void) {
   const char *src =
       "#!aml\n"
       "vars { atlas := terrain.png }\n"
       "texture := $atlas\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");
   Assert.isTrue(Context.statement_count(ctx) == 1, "1 statement");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isTrue(stmt->value_meta->type == ANVL_VALUE_VARREF, "value type is VARREF");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build");

   char *resolved = resolve_stmt_varref(ctx, vs, 0);
   Assert.isNotNull(resolved, "resolved value exists");
   Assert.isTrue(strcmp(resolved, "terrain.png") == 0, "resolved to terrain.png");
   free(resolved);

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V09 — VarRef inside an object field resolves correctly
 * ================================================================ */
static void test_v09_varref_in_object_field_resolves(void) {
   const char *src =
       "#!aml\n"
       "vars { tex := stone.png }\n"
       "stone := { texture := $tex }\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build");

   /* Navigate: statement 0 (stone) -> field "texture" -> value (VARREF) */
   statement stmt = Context.get_statement(ctx, 0);
   Assert.isTrue(stmt->value_meta->type == ANVL_VALUE_OBJECT, "stone is an object");
   usize fstart = stmt->value_meta->data.object.field_start;
   field f = ctx->field_list.fields[fstart]; /* first field: texture */
   Assert.isTrue(f->val->type == ANVL_VALUE_VARREF, "field value is VARREF");

   /* Resolve the varref */
   usize rpos, rlen;
   anvl_value_type rtype;
   bool ok = Vars.resolve(vs, Source.data(ctx->source) + f->val->data.scalar.pos,
                          f->val->data.scalar.len, &rpos, &rlen, &rtype);
   Assert.isTrue(ok, "resolve succeeded");
   char *resolved = malloc(rlen + 1);
   Source.substring(ctx->source, rpos, rlen, resolved);
   Assert.isTrue(strcmp(resolved, "stone.png") == 0, "resolved to stone.png");
   free(resolved);

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V10 — VarRef pointing to an object forwards the full value type
 * ================================================================ */
static void test_v10_varref_to_object_forwards(void) {
   const char *src =
       "#!aml\n"
       "vars { defaults := { hardness := 1.0, solid := true } }\n"
       "stone := $defaults\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build");

   usize rpos, rlen;
   anvl_value_type rtype;
   bool ok = Vars.resolve(vs, "defaults", 8, &rpos, &rlen, &rtype);
   Assert.isTrue(ok, "resolve succeeded");
   Assert.isTrue(rtype == ANVL_VALUE_OBJECT, "resolved type is OBJECT");

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V11 — VarRef pointing to an array forwards the full value type
 * ================================================================ */
static void test_v11_varref_to_array_forwards(void) {
   const char *src =
       "#!aml\n"
       "vars { tags := [block, stone, solid] }\n"
       "stone_tags := $tags\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build");

   usize rpos, rlen;
   anvl_value_type rtype;
   bool ok = Vars.resolve(vs, "tags", 4, &rpos, &rlen, &rtype);
   Assert.isTrue(ok, "resolve succeeded");
   Assert.isTrue(rtype == ANVL_VALUE_ARRAY, "resolved type is ARRAY");

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V12 — VarRef inside an inherited object field resolves correctly
 * ================================================================ */
static void test_v12_varref_in_derived_object_resolves(void) {
   const char *src =
       "#!aml\n"
       "vars { default_tex := missing.png }\n"
       "base_block := { hardness := 1.0, texture := $default_tex }\n"
       "stone : base_block := { hardness := 3.0 }\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build");

   /* Find the 'texture' field in base_block (stmt[0]) */
   statement base = Context.get_statement(ctx, 0);
   usize fstart = base->value_meta->data.object.field_start;
   usize fcount = base->value_meta->data.object.field_count;
   field tex_field = NULL;
   for (usize i = 0; i < fcount; i++) {
      field f = ctx->field_list.fields[fstart + i];
      char *k = malloc(f->key_len + 1);
      Source.substring(ctx->source, f->key_pos, f->key_len, k);
      bool match = strcmp(k, "texture") == 0;
      free(k);
      if (match) {
         tex_field = f;
         break;
      }
   }
   Assert.isNotNull(tex_field, "texture field found in base_block");
   Assert.isTrue(tex_field->val->type == ANVL_VALUE_VARREF, "texture is a VARREF");

   usize rpos, rlen;
   anvl_value_type rtype;
   Vars.resolve(vs, Source.data(ctx->source) + tex_field->val->data.scalar.pos,
                tex_field->val->data.scalar.len, &rpos, &rlen, &rtype);
   char *resolved = malloc(rlen + 1);
   Source.substring(ctx->source, rpos, rlen, resolved);
   Assert.isTrue(strcmp(resolved, "missing.png") == 0, "resolved to missing.png");
   free(resolved);

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V13 — VarRef chain (a := $b, b := scalar) resolves to scalar
 * ================================================================ */
static void test_v13_varref_chain_resolves_to_scalar(void) {
   const char *src =
       "#!aml\n"
       "vars {\n"
       "    base_tex := terrain.png\n"
       "    atlas    := $base_tex\n"
       "}\n"
       "stone := { texture := $atlas }\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build (chain is not circular)");

   usize rpos, rlen;
   anvl_value_type rtype;
   bool ok = Vars.resolve(vs, "atlas", 5, &rpos, &rlen, &rtype);
   Assert.isTrue(ok, "atlas resolves");
   char *val = malloc(rlen + 1);
   Source.substring(ctx->source, rpos, rlen, val);
   Assert.isTrue(strcmp(val, "terrain.png") == 0, "atlas chain -> terrain.png");
   free(val);

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V14 — Missing VarRef: error set at first resolve call
 * ================================================================ */
static void test_v14_missing_varref_error_at_resolve(void) {
   const char *src =
       "#!aml\n"
       "stone := { texture := $nonexistent }\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context parses (missing ref not caught at parse time)");

   anvl_vars_state_t *vs = Vars.build(ctx);
   /* Vars state builds fine — no vars block, no circular ref */

   usize rpos, rlen;
   anvl_value_type rtype;
   bool ok = Vars.resolve(vs, "nonexistent", 11, &rpos, &rlen, &rtype);
   Assert.isFalse(ok, "resolve returns false for missing key");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_VARS_KEY_NOT_FOUND,
                 "error is ANVL_ERR_VARS_KEY_NOT_FOUND");

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V15 — Circular VarRef: error at Vars.build() time
 * ================================================================ */
static void test_v15_circular_varref_error_at_build(void) {
   const char *src =
       "#!aml\n"
       "vars {\n"
       "    a := $b\n"
       "    b := $a\n"
       "}\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context parses (cycle not detectable at parse time)");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNull(vs, "Vars.build returns NULL on circular ref");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_VARS_CIRCULAR_REF,
                 "error is ANVL_ERR_VARS_CIRCULAR_REF");

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V16 — Single-ref interpolation materialises correctly
 * ================================================================ */
static void test_v16_single_ref_interpolation(void) {
   const char *src =
       "#!aml\n"
       "vars { name := Lattice }\n"
       "greeting := $\"Hello, {name}!\"\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isTrue(stmt->value_meta->type == ANVL_VALUE_INTERP_STRING,
                 "value type is INTERP_STRING");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build");

   char *result = Vars.materialise_interp(vs, ctx, stmt->value_meta);
   Assert.isNotNull(result, "materialised result exists");
   Assert.isTrue(strcmp(result, "Hello, Lattice!") == 0,
                 "materialised: Hello, Lattice!");
   Allocator.dispose(result);

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V17 — Multi-ref interpolation materialises correctly
 * ================================================================ */
static void test_v17_multi_ref_interpolation(void) {
   const char *src =
       "#!aml\n"
       "vars { major := 1, minor := 0 }\n"
       "version := $\"v{major}.{minor}\"\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build");

   statement stmt = Context.get_statement(ctx, 0);
   char *result = Vars.materialise_interp(vs, ctx, stmt->value_meta);
   Assert.isNotNull(result, "materialised result exists");
   Assert.isTrue(strcmp(result, "v1.0") == 0, "materialised: v1.0");
   Allocator.dispose(result);

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V18 — Literal-only interpolation (no refs) materialises correctly
 * ================================================================ */
static void test_v18_literal_only_interpolation(void) {
   const char *src =
       "#!aml\n"
       "vars { }\n"
       "msg := $\"no refs here\"\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isTrue(stmt->value_meta->type == ANVL_VALUE_INTERP_STRING,
                 "value type is INTERP_STRING");

   char *result = Vars.materialise_interp(vs, ctx, stmt->value_meta);
   Assert.isNotNull(result, "materialised result exists");
   Assert.isTrue(strcmp(result, "no refs here") == 0, "materialised: no refs here");
   Allocator.dispose(result);

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V19 — {{ and }} escape braces to single brace in output
 * ================================================================ */
static void test_v19_escaped_braces_materialise(void) {
   const char *src =
       "#!aml\n"
       "vars { }\n"
       "msg := $\"open {{ and close }}\"\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   anvl_vars_state_t *vs = Vars.build(ctx);
   Assert.isNotNull(vs, "vars state should build");

   statement stmt = Context.get_statement(ctx, 0);
   char *result = Vars.materialise_interp(vs, ctx, stmt->value_meta);
   Assert.isNotNull(result, "materialised result exists");
   Assert.isTrue(strcmp(result, "open { and close }") == 0,
                 "materialised: open { and close }");
   Allocator.dispose(result);

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V20 — Missing ref inside interpolation: error at materialise
 * ================================================================ */
static void test_v20_missing_ref_in_interpolation(void) {
   const char *src =
       "#!aml\n"
       "msg := $\"Hello, {nobody}!\"\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context parses (missing ref not caught at parse time)");

   anvl_vars_state_t *vs = Vars.build(ctx);

   statement stmt = Context.get_statement(ctx, 0);
   char *result = Vars.materialise_interp(vs, ctx, stmt->value_meta);
   Assert.isNull(result, "materialise returns NULL for missing ref");
   Assert.isTrue(Anvil.error_get()->code == ANVL_ERR_VARS_KEY_NOT_FOUND,
                 "error is ANVL_ERR_VARS_KEY_NOT_FOUND");

   Vars.dispose(vs);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V21 — Braced dotted varref parses and stores full path
 * ================================================================ */
static void test_v21_braced_dotted_varref_parses(void) {
   const char *src =
       "#!aml\n"
       "changelog { version := 1.0.0 }\n"
       "display := ${changelog.version}\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context parses with braced dotted varref");
   Assert.isTrue(Context.statement_count(ctx) == 2, "2 statements parsed");

   statement display = Context.get_statement(ctx, 1);
   Assert.isNotNull(display, "display statement exists");
   Assert.isTrue(display && display->value_meta &&
                     display->value_meta->type == ANVL_VALUE_VARREF,
                 "display value is VARREF");

   if (display && display->value_meta) {
      char *path = malloc(display->value_meta->len + 1);
      Source.substring(ctx->source,
                       display->value_meta->pos,
                       display->value_meta->len,
                       path);
      Assert.isTrue(path && strcmp(path, "changelog.version") == 0,
                    "stored varref path is changelog.version");
      free(path);
   }

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V22 — Unbraced dotted varref parses and stores full path
 * ================================================================ */
static void test_v22_unbraced_dotted_varref_parses(void) {
   const char *src =
       "#!aml\n"
       "changelog { version := 1.0.0 }\n"
       "display := $changelog.version\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context parses with unbraced dotted varref");
   Assert.isTrue(Context.statement_count(ctx) == 2, "2 statements parsed");

   statement display = Context.get_statement(ctx, 1);
   Assert.isNotNull(display, "display statement exists");
   Assert.isTrue(display && display->value_meta &&
                     display->value_meta->type == ANVL_VALUE_VARREF,
                 "display value is VARREF");

   if (display && display->value_meta) {
      char *path = malloc(display->value_meta->len + 1);
      Source.substring(ctx->source,
                       display->value_meta->pos,
                       display->value_meta->len,
                       path);
      Assert.isTrue(path && strcmp(path, "changelog.version") == 0,
                    "stored varref path is changelog.version");
      free(path);
   }

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * V23 — Unterminated braced varref returns dedicated error
 * ================================================================ */
static void test_v23_unterminated_braced_varref_error(void) {
   const char *src =
       "#!aml\n"
       "display := ${changelog.version\n";

   anvl_error_code code = ANVL_ERR_NONE;
   bool failed = parse_fails(src, &code);

   Assert.isTrue(failed, "parse fails for unterminated braced varref");
   Assert.isTrue(code == ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF,
                 "error is ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF");

   teardown();
}

/* ================================================================
 * Registration
 * ================================================================ */
int main(void) {
   init_assert_bridge();

   TestBit.run_ex("V01_empty_vars_block_parses_ok",      NULL, test_v01_empty_vars_block_parses_ok,      NULL);
   TestBit.run_ex("V02_single_entry_stored",             NULL, test_v02_single_entry_stored,             NULL);
   TestBit.run_ex("V03_multiple_entries_stored",         NULL, test_v03_multiple_entries_stored,         NULL);
   TestBit.run_ex("V04_vars_after_statement_is_error",   NULL, test_v04_vars_after_statement_is_error,   NULL);
   TestBit.run_ex("V05_second_vars_block_is_error",      NULL, test_v05_second_vars_block_is_error,      NULL);
   TestBit.run_ex("V06_duplicate_key_is_error",          NULL, test_v06_duplicate_key_is_error,          NULL);
   TestBit.run_ex("V07_vars_as_identifier_is_error",     NULL, test_v07_vars_as_statement_identifier_is_error, NULL);
   TestBit.run_ex("V08_scalar_varref_resolves",          NULL, test_v08_scalar_varref_resolves,          NULL);
   TestBit.run_ex("V09_varref_in_object_field_resolves", NULL, test_v09_varref_in_object_field_resolves, NULL);
   TestBit.run_ex("V10_varref_to_object_forwards",       NULL, test_v10_varref_to_object_forwards,       NULL);
   TestBit.run_ex("V11_varref_to_array_forwards",        NULL, test_v11_varref_to_array_forwards,        NULL);
   TestBit.run_ex("V12_varref_in_derived_object_resolves", NULL, test_v12_varref_in_derived_object_resolves, NULL);
   TestBit.run_ex("V13_varref_chain_resolves_to_scalar", NULL, test_v13_varref_chain_resolves_to_scalar, NULL);
   TestBit.run_ex("V14_missing_varref_error_at_resolve", NULL, test_v14_missing_varref_error_at_resolve, NULL);
   TestBit.run_ex("V15_circular_varref_error_at_build",  NULL, test_v15_circular_varref_error_at_build,  NULL);
   TestBit.run_ex("V16_single_ref_interpolation",        NULL, test_v16_single_ref_interpolation,        NULL);
   TestBit.run_ex("V17_multi_ref_interpolation",         NULL, test_v17_multi_ref_interpolation,         NULL);
   TestBit.run_ex("V18_literal_only_interpolation",      NULL, test_v18_literal_only_interpolation,      NULL);
   TestBit.run_ex("V19_escaped_braces_materialise",      NULL, test_v19_escaped_braces_materialise,      NULL);
   TestBit.run_ex("V20_missing_ref_in_interpolation",    NULL, test_v20_missing_ref_in_interpolation,    NULL);
   TestBit.run_ex("V21_braced_dotted_varref_parses",     NULL, test_v21_braced_dotted_varref_parses,     NULL);
   TestBit.run_ex("V22_unbraced_dotted_varref_parses",   NULL, test_v22_unbraced_dotted_varref_parses,   NULL);
   TestBit.run_ex("V23_unterminated_braced_varref_error", NULL, test_v23_unterminated_braced_varref_error, NULL);
   return TestBit.report();
}
