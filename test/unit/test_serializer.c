/*
 * test_serializer.c — TDD tests for anvil.serializer (port/serializer)
 *
 * Test contracts derived from AnvilWriter / WriterTests.cs (Anvil.Net reference).
 *
 * Test groups:
 *   ST01  Scalar statements: string, int, bool, bare id round-trip
 *   ST02  Inline object round-trip (≤ InlineThreshold fields)
 *   ST03  Block object round-trip (> InlineThreshold fields)
 *   ST04a Inline array round-trip
 *   ST04b Block array round-trip
 *   ST05  Tuple always inline
 *   ST06a Untagged blob round-trip
 *   ST06b Tagged blob round-trip
 *   ST07  Full document parse→serialize→parse: values unchanged
 *   ST08  AMP dialect: shebang emitted; objects cause serializer error
 *   ST12  Inheritance: derived:base syntax preserved in output
 *   ST_M1 Minify: single scalar, no spaces
 *   ST_M2 Minify: object collapses, no spaces
 *   ST_M3 Minify: array collapses, no spaces
 *   ST_M6 Minify: multi-statement, comma-separated
 */

#include "anvil.h"
#include "serializer.h"
#include "utilities/helpers.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */
static void test_st01_scalars_round_trip(void);
static void test_st02_inline_object_round_trip(void);
static void test_st03_block_object_round_trip(void);
static void test_st04a_inline_array_round_trip(void);
static void test_st04b_block_array_round_trip(void);
static void test_st05_tuple_always_inline(void);
static void test_st06a_untagged_blob_round_trip(void);
static void test_st06b_tagged_blob_round_trip(void);
static void test_st06c_multiline_blob_exact(void);
static void test_st06d_blob_special_chars_verbatim(void);
static void test_st06e_minify_blob_unchanged(void);
static void test_st06f_blob_serialization_idempotent(void);
static void test_st07_full_document_values_preserved(void);
static void test_st08_amp_dialect_shebang_and_guard(void);
static void test_st12_inheritance_syntax_preserved(void);
static void test_st_m1_minify_scalar(void);
static void test_st_m2_minify_object(void);
static void test_st_m3_minify_array(void);
static void test_st_m6_minify_multi_statement(void);

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static void set_config(FILE **logger) {
   *logger = fopen("logs/test_serializer.log", "w");
}
static void set_teardown(void) {
   Anvil.cleanup();
}
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

/* Serialize ctx, return heap string.  Caller must Allocator.free(). */
static char *serialize(context ctx, const anvl_serializer_options_t *opts) {
   string_builder sb = Serializer.serialize(ctx, opts);
   if (!sb)
      return NULL;
   char *text = (char *)StringBuilder.toString(sb);
   StringBuilder.dispose(sb);
   return text;
}

/* Parse → serialize → re-parse: returns the second context. Caller must Context.dispose(). */
static context round_trip(const char *src_str, const anvl_serializer_options_t *opts) {
   context ctx1 = build_context(src_str);
   if (!ctx1)
      return NULL;
   char *text = serialize(ctx1, opts);
   Context.dispose(ctx1);
   if (!text)
      return NULL;
   context ctx2 = build_context(text);
   Allocator.free(text);
   return ctx2;
}

/* Return raw source substring for statement value at stmt_idx. Caller must Allocator.free(). */
static char *stmt_value_str(context ctx, usize idx) {
   statement stmt = Context.get_statement(ctx, idx);
   if (!stmt || !stmt->value_meta)
      return NULL;
   return Source.substring(ctx->source, stmt->value_meta->pos, stmt->value_meta->len);
}

/* Return the number of fields in the object value of statement at stmt_idx. */
static usize stmt_object_field_count(context ctx, usize idx) {
   statement stmt = Context.get_statement(ctx, idx);
   if (!stmt || !stmt->value_meta)
      return 0;
   if (stmt->value_meta->type != ANVL_VALUE_OBJECT)
      return 0;
   return stmt->value_meta->data.object.field_count;
}

/* Return raw key string for field at field_start + offset. Caller must Allocator.free(). */
static char *field_key_str(context ctx, usize stmt_idx, usize field_offset) {
   statement stmt = Context.get_statement(ctx, stmt_idx);
   if (!stmt || !stmt->value_meta || stmt->value_meta->type != ANVL_VALUE_OBJECT)
      return NULL;
   usize fi = stmt->value_meta->data.object.field_start + field_offset;
   if (fi >= ctx->field_list.count)
      return NULL;
   field f = ctx->field_list.fields[fi];
   return Source.substring(ctx->source, f->key_pos, f->key_len);
}

/* Return raw value string for field at field_start + offset. Caller must Allocator.free(). */
static char *field_val_str(context ctx, usize stmt_idx, usize field_offset) {
   statement stmt = Context.get_statement(ctx, stmt_idx);
   if (!stmt || !stmt->value_meta || stmt->value_meta->type != ANVL_VALUE_OBJECT)
      return NULL;
   usize fi = stmt->value_meta->data.object.field_start + field_offset;
   if (fi >= ctx->field_list.count)
      return NULL;
   field f = ctx->field_list.fields[fi];
   if (!f->val || f->val->type != ANVL_VALUE_SCALAR)
      return NULL;
   return Source.substring(ctx->source, f->val->data.scalar.pos, f->val->data.scalar.len);
}

/* ================================================================
 * ST01 — Scalar statements round-trip
 * ================================================================ */
static void test_st01_scalars_round_trip(void) {
   const char *src =
       "#!aml\n"
       "name := \"Anvil.Engine\"\n"
       "count := 42\n"
       "debug := true\n"
       "material := stone\n";

   context ctx = round_trip(src, &ANVL_SERIALIZER_DEFAULT);
   Assert.isNotNull(ctx, "Round-trip context should exist");
   Assert.isTrue(Context.statement_count(ctx) == 4, "Should have 4 statements");

   char *v;

   v = stmt_value_str(ctx, 0);
   Assert.isNotNull(v, "name value present");
   Assert.isTrue(strcmp(v, "\"Anvil.Engine\"") == 0, "name := \"Anvil.Engine\"");
   Allocator.free(v);

   v = stmt_value_str(ctx, 1);
   Assert.isNotNull(v, "count value present");
   Assert.isTrue(strcmp(v, "42") == 0, "count := 42");
   Allocator.free(v);

   v = stmt_value_str(ctx, 2);
   Assert.isNotNull(v, "debug value present");
   Assert.isTrue(strcmp(v, "true") == 0, "debug := true");
   Allocator.free(v);

   v = stmt_value_str(ctx, 3);
   Assert.isNotNull(v, "material value present");
   Assert.isTrue(strcmp(v, "stone") == 0, "material := stone");
   Allocator.free(v);

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * ST02 — Inline object round-trip (≤ InlineThreshold = 2)
 * ================================================================ */
static void test_st02_inline_object_round_trip(void) {
   const char *src =
       "#!aml\n"
       "point := { x := 10, y := 20 }\n";

   context ctx = round_trip(src, &ANVL_SERIALIZER_DEFAULT);
   Assert.isNotNull(ctx, "Round-trip context should exist");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have 1 statement");
   Assert.isTrue(stmt_object_field_count(ctx, 0) == 2, "point has 2 fields");

   char *v;
   v = field_val_str(ctx, 0, 0); /* x */
   Assert.isNotNull(v, "x value present");
   Assert.isTrue(strcmp(v, "10") == 0, "x := 10");
   Allocator.free(v);

   v = field_val_str(ctx, 0, 1); /* y */
   Assert.isNotNull(v, "y value present");
   Assert.isTrue(strcmp(v, "20") == 0, "y := 20");
   Allocator.free(v);

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * ST03 — Block object round-trip (> InlineThreshold = 2)
 * ================================================================ */
static void test_st03_block_object_round_trip(void) {
   const char *src =
       "#!aml\n"
       "config := {\n"
       "    name := \"server\",\n"
       "    port := 8080,\n"
       "    debug := false\n"
       "}\n";

   context ctx = round_trip(src, &ANVL_SERIALIZER_DEFAULT);
   Assert.isNotNull(ctx, "Round-trip context should exist");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have 1 statement");
   Assert.isTrue(stmt_object_field_count(ctx, 0) == 3, "config has 3 fields");

   char *v;
   v = field_val_str(ctx, 0, 0); /* name */
   Assert.isNotNull(v, "name value present");
   Assert.isTrue(strcmp(v, "\"server\"") == 0, "name := \"server\"");
   Allocator.free(v);

   v = field_val_str(ctx, 0, 1); /* port */
   Assert.isNotNull(v, "port value present");
   Assert.isTrue(strcmp(v, "8080") == 0, "port := 8080");
   Allocator.free(v);

   v = field_val_str(ctx, 0, 2); /* debug */
   Assert.isNotNull(v, "debug value present");
   Assert.isTrue(strcmp(v, "false") == 0, "debug := false");
   Allocator.free(v);

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * ST04a — Inline array round-trip
 * ================================================================ */
static void test_st04a_inline_array_round_trip(void) {
   const char *src =
       "#!aml\n"
       "tags := [ alpha, beta ]\n";

   context ctx = round_trip(src, &ANVL_SERIALIZER_DEFAULT);
   Assert.isNotNull(ctx, "Round-trip context should exist");
   Assert.isTrue(Context.statement_count(ctx) == 1, "Should have 1 statement");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt->value_meta, "value_meta present");
   Assert.isTrue(stmt->value_meta->data.collection.element_count == 2, "2 elements");

   char *e0 = Source.substring(ctx->source,
                               stmt->value_meta->data.collection.elements[0].pos,
                               stmt->value_meta->data.collection.elements[0].len);
   char *e1 = Source.substring(ctx->source,
                               stmt->value_meta->data.collection.elements[1].pos,
                               stmt->value_meta->data.collection.elements[1].len);

   Assert.isTrue(strcmp(e0, "alpha") == 0, "element[0] == alpha");
   Assert.isTrue(strcmp(e1, "beta") == 0, "element[1] == beta");

   Allocator.free(e0);
   Allocator.free(e1);
   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * ST04b — Block array round-trip
 * ================================================================ */
static void test_st04b_block_array_round_trip(void) {
   const char *src =
       "#!aml\n"
       "ports := [\n"
       "    8080,\n"
       "    8443,\n"
       "    9000,\n"
       "]\n";

   context ctx = round_trip(src, &ANVL_SERIALIZER_DEFAULT);
   Assert.isNotNull(ctx, "Round-trip context should exist");

   statement stmt = Context.get_statement(ctx, 0);
   Assert.isNotNull(stmt->value_meta, "value_meta present");
   Assert.isTrue(stmt->value_meta->data.collection.element_count == 3, "3 elements");

   char *e;
   e = Source.substring(ctx->source,
                        stmt->value_meta->data.collection.elements[0].pos,
                        stmt->value_meta->data.collection.elements[0].len);
   Assert.isTrue(strcmp(e, "8080") == 0, "element[0] == 8080");
   Allocator.free(e);

   e = Source.substring(ctx->source,
                        stmt->value_meta->data.collection.elements[1].pos,
                        stmt->value_meta->data.collection.elements[1].len);
   Assert.isTrue(strcmp(e, "8443") == 0, "element[1] == 8443");
   Allocator.free(e);

   e = Source.substring(ctx->source,
                        stmt->value_meta->data.collection.elements[2].pos,
                        stmt->value_meta->data.collection.elements[2].len);
   Assert.isTrue(strcmp(e, "9000") == 0, "element[2] == 9000");
   Allocator.free(e);

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * ST05 — Tuple always inline
 * ================================================================ */
static void test_st05_tuple_always_inline(void) {
   const char *src =
       "#!aml\n"
       "pos := (10, 64, -200)\n";

   context ctx1 = build_context(src);
   Assert.isNotNull(ctx1, "ctx1 should exist");

   char *text = serialize(ctx1, &ANVL_SERIALIZER_DEFAULT);
   Context.dispose(ctx1);

   Assert.isNotNull(text, "serialized text exists");
   /* Tuple must be on a single line — check '(' and ')' are present */
   Assert.isTrue(strstr(text, "(") != NULL, "output has '('");
   Assert.isTrue(strstr(text, ")") != NULL, "output has ')'");

   /* Verify values round-trip */
   context ctx2 = build_context(text);
   Allocator.free(text);
   Assert.isNotNull(ctx2, "re-parsed context exists");

   statement stmt = Context.get_statement(ctx2, 0);
   Assert.isNotNull(stmt->value_meta, "value_meta present");
   Assert.isTrue(stmt->value_meta->data.collection.element_count == 3, "3 tuple elements");

   char *e;
   e = Source.substring(ctx2->source,
                        stmt->value_meta->data.collection.elements[0].pos,
                        stmt->value_meta->data.collection.elements[0].len);
   Assert.isTrue(strcmp(e, "10") == 0, "element[0] == 10");
   Allocator.free(e);

   e = Source.substring(ctx2->source,
                        stmt->value_meta->data.collection.elements[2].pos,
                        stmt->value_meta->data.collection.elements[2].len);
   Assert.isTrue(strcmp(e, "-200") == 0, "element[2] == -200");
   Allocator.free(e);

   Context.dispose(ctx2);
   teardown();
}

/* ================================================================
 * ST06a — Untagged blob round-trip
 * ================================================================ */
static void test_st06a_untagged_blob_round_trip(void) {
   const char *src =
       "#!aml\n"
       "raw := `hello`\n";

   context ctx1 = build_context(src);
   Assert.isNotNull(ctx1, "ctx1 should exist");

   char *text = serialize(ctx1, &ANVL_SERIALIZER_DEFAULT);
   Context.dispose(ctx1);

   Assert.isNotNull(text, "serialized text exists");
   Assert.isTrue(strstr(text, "`hello`") != NULL, "output contains `hello`");

   context ctx2 = build_context(text);
   Allocator.free(text);
   Assert.isNotNull(ctx2, "re-parsed context exists");
   Assert.isTrue(Context.statement_count(ctx2) == 1, "1 statement");

   Context.dispose(ctx2);
   teardown();
}

/* ================================================================
 * ST06b — Tagged blob round-trip
 * ================================================================ */
static void test_st06b_tagged_blob_round_trip(void) {
   const char *src =
       "#!aml\n"
       "doc := @md`# Title`\n";

   context ctx1 = build_context(src);
   Assert.isNotNull(ctx1, "ctx1 should exist");

   char *text = serialize(ctx1, &ANVL_SERIALIZER_DEFAULT);
   Context.dispose(ctx1);

   Assert.isNotNull(text, "serialized text exists");
   /* Verify the full blob span — tag, opening backtick, content, closing backtick */
   Assert.isTrue(strstr(text, "@md`# Title`") != NULL, "full blob span @md`# Title` preserved");

   context ctx2 = build_context(text);
   Allocator.free(text);
   Assert.isNotNull(ctx2, "re-parsed context exists");
   Assert.isTrue(Context.statement_count(ctx2) == 1, "1 statement");

   Context.dispose(ctx2);
   teardown();
}

/* ================================================================
 * ST06c — Multi-line blob: exact content preserved verbatim
 * ================================================================
 * Blob content spans multiple lines; every byte (including embedded
 * newlines and leading whitespace) must appear character-for-character
 * in the serialized output — the full @tag`...` span is emitted raw.
 * ================================================================ */
static void test_st06c_multiline_blob_exact(void) {
   const char *src =
       "#!aml\n"
       "data := `line1\n"
       "line2\n"
       "  indented\n"
       "`\n";

   context ctx1 = build_context(src);
   Assert.isNotNull(ctx1, "ctx1 should exist");

   char *text = serialize(ctx1, &ANVL_SERIALIZER_DEFAULT);
   Context.dispose(ctx1);

   Assert.isNotNull(text, "serialized text exists");
   /* Full span including both backtick delimiters must be verbatim */
   Assert.isTrue(strstr(text, "`line1\nline2\n  indented\n`") != NULL,
                 "multi-line blob content preserved verbatim");

   context ctx2 = build_context(text);
   Allocator.free(text);
   Assert.isNotNull(ctx2, "re-parsed context exists");
   Assert.isTrue(Context.statement_count(ctx2) == 1, "1 statement");

   Context.dispose(ctx2);
   teardown();
}

/* ================================================================
 * ST06d — Blob with AML-special chars preserved verbatim
 * ================================================================
 * Characters that carry syntactic meaning in AML (:=, {, }, [, ], ,)
 * must not be processed or mangled when they appear inside a blob.
 * ================================================================ */
static void test_st06d_blob_special_chars_verbatim(void) {
   const char *src =
       "#!aml\n"
       "raw := `name := {key:[1,2,3]}, debug := true`\n";

   context ctx1 = build_context(src);
   Assert.isNotNull(ctx1, "ctx1 should exist");

   char *text = serialize(ctx1, &ANVL_SERIALIZER_DEFAULT);
   Context.dispose(ctx1);

   Assert.isNotNull(text, "serialized text exists");
   Assert.isTrue(strstr(text, "`name := {key:[1,2,3]}, debug := true`") != NULL,
                 "AML-special chars inside blob preserved verbatim");

   context ctx2 = build_context(text);
   Allocator.free(text);
   Assert.isNotNull(ctx2, "re-parsed context exists");
   Assert.isTrue(Context.statement_count(ctx2) == 1, "1 statement");

   Context.dispose(ctx2);
   teardown();
}

/* ================================================================
 * ST06e — Minify: multi-line blob content NOT modified
 * ================================================================
 * In minify mode the serializer strips whitespace between statements
 * and uses ":=" instead of " := ".  The blob content itself must be
 * emitted verbatim — newlines inside the blob survive minification.
 * ================================================================ */
static void test_st06e_minify_blob_unchanged(void) {
   const char *src =
       "#!aml\n"
       "doc := @md`# Title\n"
       "\n"
       "Some **markdown**.\n"
       "`\n";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "ctx should exist");

   char *text = serialize(ctx, &ANVL_SERIALIZER_MINIFIED);
   Context.dispose(ctx);

   Assert.isNotNull(text, "minified text exists");
   /* Outer syntax is minified: ":=" not " := " */
   Assert.isTrue(strstr(text, "doc:=") != NULL, "minify: := has no spaces");
   /* Blob content (including internal newlines) is preserved verbatim */
   Assert.isTrue(strstr(text, "@md`# Title\n\nSome **markdown**.\n`") != NULL,
                 "minify: blob content including newlines unchanged");

   context ctx2 = build_context(text);
   Allocator.free(text);
   Assert.isNotNull(ctx2, "re-parsed context exists");
   Assert.isTrue(Context.statement_count(ctx2) == 1, "1 statement");

   Context.dispose(ctx2);
   teardown();
}

/* ================================================================
 * ST06f — Idempotency: serialize twice → byte-identical output
 * ================================================================
 * parse → serialize → text1
 * parse text1 → serialize → text2
 * text1 must equal text2 exactly.  This catches any path where the
 * serializer reconstructs blob delimiters or tags rather than
 * emitting the raw source span (reconstruction could drift on the
 * second pass if positions are mis-tracked).
 * ================================================================ */
static void test_st06f_blob_serialization_idempotent(void) {
   const char *src =
       "#!aml\n"
       "cert := @pem`-----BEGIN CERTIFICATE-----\n"
       "MIIDXTCCAkWgAwIBAgI=\n"
       "-----END CERTIFICATE-----\n"
       "`\n";

   /* First pass */
   context ctx1 = build_context(src);
   Assert.isNotNull(ctx1, "ctx1 should exist");
   char *text1 = serialize(ctx1, &ANVL_SERIALIZER_DEFAULT);
   Context.dispose(ctx1);
   Assert.isNotNull(text1, "text1 exists");

   /* Second pass */
   context ctx2 = build_context(text1);
   Assert.isNotNull(ctx2, "ctx2 should exist");
   char *text2 = serialize(ctx2, &ANVL_SERIALIZER_DEFAULT);
   Context.dispose(ctx2);
   Assert.isNotNull(text2, "text2 exists");

   /* Byte-identical */
   Assert.isTrue(strcmp(text1, text2) == 0, "serialize twice → identical output");

   Allocator.free(text1);
   Allocator.free(text2);
   teardown();
}

/* ================================================================
 * ST07 — Full document: parse→serialize→parse, values unchanged
 * ================================================================ */
static void test_st07_full_document_values_preserved(void) {
   const char *src =
       "#!aml\n"
       "name    := \"Anvil.Engine\"\n"
       "debug   := true\n"
       "count   := 42\n"
       "pi      := 3.14159\n"
       "flavor  := vanilla\n";

   context ctx = round_trip(src, &ANVL_SERIALIZER_DEFAULT);
   Assert.isNotNull(ctx, "Round-trip context should exist");
   Assert.isTrue(Context.statement_count(ctx) == 5, "5 statements");

   char *v;
   v = stmt_value_str(ctx, 0);
   Assert.isTrue(strcmp(v, "\"Anvil.Engine\"") == 0, "name");
   Allocator.free(v);
   v = stmt_value_str(ctx, 1);
   Assert.isTrue(strcmp(v, "true") == 0, "debug");
   Allocator.free(v);
   v = stmt_value_str(ctx, 2);
   Assert.isTrue(strcmp(v, "42") == 0, "count");
   Allocator.free(v);
   v = stmt_value_str(ctx, 3);
   Assert.isTrue(strcmp(v, "3.14159") == 0, "pi");
   Allocator.free(v);
   v = stmt_value_str(ctx, 4);
   Assert.isTrue(strcmp(v, "vanilla") == 0, "flavor");
   Allocator.free(v);

   Context.dispose(ctx);
   teardown();
}

/* ================================================================
 * ST08 — AMP dialect: shebang emitted; objects rejected
 * ================================================================ */
static void test_st08_amp_dialect_shebang_and_guard(void) {
   /* AMP scalar doc should produce #!amp shebang */
   const char *scalar_src =
       "#!amp\n"
       "status := ok\n"
       "code := 200\n";

   context ctx1 = build_context(scalar_src);
   Assert.isNotNull(ctx1, "AMP context should build");

   anvl_serializer_options_t amp_opts = ANVL_SERIALIZER_DEFAULT;
   amp_opts.dialect = ANVL_DIALECT_AMP;

   char *text = serialize(ctx1, &amp_opts);
   Context.dispose(ctx1);

   Assert.isNotNull(text, "AMP serialized text exists");
   Assert.isTrue(strncmp(text, "#!amp", 5) == 0, "AMP shebang emitted");
   Assert.isTrue(strstr(text, "status") != NULL, "status present");
   Assert.isTrue(strstr(text, "ok") != NULL, "ok present");
   Allocator.free(text);

   /* AML doc with object serialized as AMP should return NULL (error) */
   const char *obj_src = "#!aml\nconfig := { a := 1 }\n";
   context ctx2 = build_context(obj_src);
   Assert.isNotNull(ctx2, "AML object context should build");

   string_builder sb = Serializer.serialize(ctx2, &amp_opts);
   Assert.isNull(sb, "AMP serializer should return NULL for object values");
   Context.dispose(ctx2);
   Anvil.error_clear();

   teardown();
}

/* ================================================================
 * ST12 — Inheritance: derived:base syntax preserved in output
 * ================================================================ */
static void test_st12_inheritance_syntax_preserved(void) {
   const char *src =
       "#!aml\n"
       "Animal := { legs := 4, sound := generic }\n"
       "Dog:Animal := { sound := woof }\n";

   context ctx1 = build_context(src);
   Assert.isNotNull(ctx1, "ctx1 should exist");

   char *text = serialize(ctx1, &ANVL_SERIALIZER_DEFAULT);
   Context.dispose(ctx1);

   Assert.isNotNull(text, "serialized text exists");
   Assert.isTrue(strstr(text, "Animal") != NULL, "Animal statement present");
   /* Derived should have :Animal syntax */
   Assert.isTrue(strstr(text, ":Animal") != NULL || strstr(text, "Dog:Animal") != NULL,
                 "inheritance :base syntax preserved");

   context ctx2 = build_context(text);
   Allocator.free(text);
   Assert.isNotNull(ctx2, "re-parsed context exists");
   Assert.isTrue(Context.statement_count(ctx2) == 2, "2 statements");

   /* Verify Dog:Animal has base_meta */
   statement dog_stmt = Context.get_statement(ctx2, 1);
   Assert.isNotNull(dog_stmt->base_meta, "Dog statement has base_meta after round-trip");

   Context.dispose(ctx2);
   teardown();
}

/* ================================================================
 * ST_M1 — Minify: single scalar statement
 * ================================================================ */
static void test_st_m1_minify_scalar(void) {
   const char *src = "#!aml\na := 1";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   char *text = serialize(ctx, &ANVL_SERIALIZER_MINIFIED);
   Context.dispose(ctx);

   Assert.isNotNull(text, "minified text exists");
   Assert.isTrue(strcmp(text, "#!aml\na:=1") == 0, "minified output: #!aml\\na:=1");
   Allocator.free(text);
   teardown();
}

/* ================================================================
 * ST_M2 — Minify: object collapses to single line, no spaces
 * ================================================================ */
static void test_st_m2_minify_object(void) {
   const char *src = "#!aml\nconfig := { name := server, port := 8080 }";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   char *text = serialize(ctx, &ANVL_SERIALIZER_MINIFIED);
   Context.dispose(ctx);

   Assert.isNotNull(text, "minified text exists");
   Assert.isTrue(strcmp(text, "#!aml\nconfig:={name:=server,port:=8080}") == 0,
                 "minified object output");
   Allocator.free(text);
   teardown();
}

/* ================================================================
 * ST_M3 — Minify: array collapses to single line, no spaces
 * ================================================================ */
static void test_st_m3_minify_array(void) {
   const char *src = "#!aml\ntags := [ alpha, beta, gamma ]";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   char *text = serialize(ctx, &ANVL_SERIALIZER_MINIFIED);
   Context.dispose(ctx);

   Assert.isNotNull(text, "minified text exists");
   Assert.isTrue(strcmp(text, "#!aml\ntags:=[alpha,beta,gamma]") == 0,
                 "minified array output");
   Allocator.free(text);
   teardown();
}

/* ================================================================
 * ST_M6 — Minify: multi-statement, comma-separated on one line
 * ================================================================ */
static void test_st_m6_minify_multi_statement(void) {
   const char *src = "#!aml\nname := engine\ncount := 42\ndebug := true";

   context ctx = build_context(src);
   Assert.isNotNull(ctx, "context should exist");

   char *text = serialize(ctx, &ANVL_SERIALIZER_MINIFIED);
   Context.dispose(ctx);

   Assert.isNotNull(text, "minified text exists");
   Assert.isTrue(strcmp(text, "#!aml\nname:=engine,count:=42,debug:=true") == 0,
                 "minified multi-statement output");
   Allocator.free(text);
   teardown();
}

/* ================================================================
 * Registration
 * ================================================================ */
static void _register(void) {
   testset("Serializer Tests", set_config, set_teardown);

   testcase("ST01: Scalar statements round-trip", test_st01_scalars_round_trip);
   testcase("ST02: Inline object round-trip", test_st02_inline_object_round_trip);
   testcase("ST03: Block object round-trip", test_st03_block_object_round_trip);
   testcase("ST04a: Inline array round-trip", test_st04a_inline_array_round_trip);
   testcase("ST04b: Block array round-trip", test_st04b_block_array_round_trip);
   testcase("ST05: Tuple always inline", test_st05_tuple_always_inline);
   testcase("ST06a: Untagged blob round-trip", test_st06a_untagged_blob_round_trip);
   testcase("ST06b: Tagged blob round-trip", test_st06b_tagged_blob_round_trip);
   testcase("ST06c: Multi-line blob exact content", test_st06c_multiline_blob_exact);
   testcase("ST06d: Blob special chars verbatim", test_st06d_blob_special_chars_verbatim);
   testcase("ST06e: Minify blob content unchanged", test_st06e_minify_blob_unchanged);
   testcase("ST06f: Blob serialization idempotent", test_st06f_blob_serialization_idempotent);
   testcase("ST07: Full document values preserved", test_st07_full_document_values_preserved);
   testcase("ST08: AMP dialect shebang and guard", test_st08_amp_dialect_shebang_and_guard);
   testcase("ST12: Inheritance syntax preserved", test_st12_inheritance_syntax_preserved);
   testcase("ST_M1: Minify scalar", test_st_m1_minify_scalar);
   testcase("ST_M2: Minify object", test_st_m2_minify_object);
   testcase("ST_M3: Minify array", test_st_m3_minify_array);
   testcase("ST_M6: Minify multi-statement", test_st_m6_minify_multi_statement);
}
__attribute__((constructor)) static void register_test_serializer(void) {
   Tests.enqueue(_register);
}
