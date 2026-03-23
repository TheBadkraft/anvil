/*
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * serializer.c — anvil.serializer implementation
 * ------------------------------------------------------------------
 * Ported from: AnvilWriter.cs (Anvil.Net v0.2.1)
 *
 * Converts a parsed context back to AML/AMP/ASL text, preserving
 * all structural and value semantics.
 *
 * Component: post-parse optional (anvil.serializer.o)
 * ------------------------------------------------------------------
 */

#include "serializer.h"
#include "anvil.h"

#include <sigma.memory/memory.h>
#include <sigma.core/strings.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Default option singletons                                          */
/* ------------------------------------------------------------------ */
const anvl_serializer_options_t ANVL_SERIALIZER_DEFAULT = {
    .dialect = ANVL_DIALECT_AML,
    .include_shebang = true,
    .minify = false,
    .indent_width = 4,
    .align_assignments = false,
    .preserve_vars = true,
    .inline_threshold = 2,
};

const anvl_serializer_options_t ANVL_SERIALIZER_MINIFIED = {
    .dialect = ANVL_DIALECT_AML,
    .include_shebang = true,
    .minify = true,
    .indent_width = 4,
    .align_assignments = false,
    .preserve_vars = false,
    .inline_threshold = 2,
};

/* ------------------------------------------------------------------ */
/* IndentWriter — tracks indent level, line-start state, minify mode  */
/* ------------------------------------------------------------------ */
typedef struct {
   string_builder sb;
   int level;
   int indent_width;
   bool minify;
   bool at_line_start;
   const char *assign; /* " := " pretty  |  ":="  minify */
} iw_t;
typedef iw_t *iw;

static void iw_init(iw_t *w, string_builder sb, int width, bool minify) {
   w->sb = sb;
   w->level = 0;
   w->indent_width = width;
   w->minify = minify;
   w->at_line_start = true;
   w->assign = minify ? ":=" : " := ";
}

/* Emit indent prefix if at start of line, then text (no newline). */
static void iw_write(iw w, const char *text) {
   if (!w->minify && w->at_line_start && w->level > 0) {
      int spaces = w->level * w->indent_width;
      for (int i = 0; i < spaces; i++)
         StringBuilder.append(w->sb, " ");
   }
   w->at_line_start = false;
   StringBuilder.append(w->sb, (string)text);
}

/* Append text without indent prefix. */
static void iw_append(iw w, const char *text) {
   w->at_line_start = false;
   StringBuilder.append(w->sb, (string)text);
}

/* Append a fixed-length substring from source text. */
static void iw_appendn(iw w, const char *text, usize len) {
   w->at_line_start = false;
   StringBuilder.appendf(w->sb, "%.*s", (int)len, text);
}

/* Emit indent + text + newline (always emits newline even in minify for shebang). */
static void iw_line(iw w, const char *text) {
   if (!w->minify && w->level > 0) {
      int spaces = w->level * w->indent_width;
      for (int i = 0; i < spaces; i++)
         StringBuilder.append(w->sb, " ");
   }
   StringBuilder.append(w->sb, (string)text);
   StringBuilder.appendl(w->sb, NULL); /* newline */
   w->at_line_start = true;
}

/* Emit a newline ending the current line. No-op in minify (stmts are comma-separated). */
static void iw_blank(iw w) {
   if (w->minify)
      return;
   StringBuilder.appendl(w->sb, NULL);
   w->at_line_start = true;
}

/* Ensure we are at the start of a fresh line (used after shebang in minify). */
static void iw_ensure_newline(iw w) {
   if (!w->minify && !w->at_line_start) {
      StringBuilder.appendl(w->sb, NULL);
      w->at_line_start = true;
   }
}

static void iw_push(iw w) {
   if (!w->minify)
      w->level++;
}
static void iw_pop(iw w) {
   if (!w->minify && w->level > 0)
      w->level--;
}

/* ------------------------------------------------------------------ */
/* Blob detection                                                      */
/* AML blobs are stored as ARRAY with elements[0].type == BLOB.       */
/* AMP blobs are stored directly as ANVL_VALUE_BLOB (scalar-encoded). */
/* ------------------------------------------------------------------ */
static bool is_blob_meta(const struct anvl_value_meta *vm) {
   if (vm->type == ANVL_VALUE_BLOB)
      return true; /* AMP dialect blob */
   if (vm->type == ANVL_VALUE_ARRAY &&
       vm->data.collection.element_count > 0 &&
       vm->data.collection.elements != NULL &&
       vm->data.collection.elements[0].type == ANVL_VALUE_BLOB)
      return true; /* AML/ASL blob collection */
   return false;
}

/* ------------------------------------------------------------------ */
/* Forward declarations for write functions                           */
/* ------------------------------------------------------------------ */
static void write_value_meta(context ctx, iw w,
                             const struct anvl_value_meta *vm,
                             const anvl_serializer_options_t *opts,
                             bool inner_inline);
static void write_field_value(context ctx, iw w,
                              value val,
                              const anvl_serializer_options_t *opts,
                              bool inner_inline);
static void write_object_fields(context ctx, iw w,
                                usize field_start, usize field_count,
                                const anvl_serializer_options_t *opts,
                                bool force_inline);
static void write_array_elems(context ctx, iw w,
                              const struct anvl_value_meta *vm,
                              const anvl_serializer_options_t *opts,
                              bool is_tuple);

/* ------------------------------------------------------------------ */
/* write_blob_raw — emit pre-existing blob span verbatim              */
/* ------------------------------------------------------------------ */
static void write_blob_raw(context ctx, iw w, const struct anvl_value_meta *vm) {
   const char *src_data = Source.data(ctx->source);
   iw_appendn(w, src_data + vm->pos, vm->len);
}

/* ------------------------------------------------------------------ */
/* write_scalar_raw — emit scalar span verbatim                       */
/* ------------------------------------------------------------------ */
static void write_scalar_raw(context ctx, iw w, usize pos, usize len) {
   const char *src_data = Source.data(ctx->source);
   iw_appendn(w, src_data + pos, len);
}

/* ------------------------------------------------------------------ */
/* write_array_elems — emit array or tuple elements                   */
/* ------------------------------------------------------------------ */
static void write_array_elems(context ctx, iw w,
                              const struct anvl_value_meta *vm,
                              const anvl_serializer_options_t *opts,
                              bool is_tuple) {
   const char *src_data = Source.data(ctx->source);
   char open = is_tuple ? '(' : '[';
   char close = is_tuple ? ')' : ']';
   usize count = vm->data.collection.element_count;

   bool do_inline = opts->minify || is_tuple ||
                    (usize)opts->inline_threshold >= count;

   if (do_inline) {
      const char *open_str = opts->minify ? (is_tuple ? "(" : "[")
                                          : (is_tuple ? "( " : "[ ");
      const char *close_str = opts->minify ? (is_tuple ? ")" : "]")
                                           : (is_tuple ? " )" : " ]");
      const char *sep = opts->minify ? "," : ", ";

      iw_append(w, open_str);
      for (usize i = 0; i < count; i++) {
         if (i > 0)
            iw_append(w, sep);
         struct anvl_element_meta *em = &vm->data.collection.elements[i];
         iw_appendn(w, src_data + em->pos, em->len);
      }
      iw_append(w, close_str);
   } else {
      /* Block format */
      char open_s[2] = {open, '\0'};
      char close_s[2] = {close, '\0'};
      iw_append(w, open_s);
      iw_blank(w);
      iw_push(w);
      for (usize i = 0; i < count; i++) {
         struct anvl_element_meta *em = &vm->data.collection.elements[i];
         iw_write(w, ""); /* emit indent */
         iw_appendn(w, src_data + em->pos, em->len);
         iw_append(w, ",");
         iw_blank(w);
      }
      iw_pop(w);
      iw_write(w, close_s);
      iw_blank(w);
      return;
   }
}

/* ------------------------------------------------------------------ */
/* write_object_fields — emit object { key := val, ... }             */
/* ------------------------------------------------------------------ */
static void write_object_fields(context ctx, iw w,
                                usize field_start, usize field_count,
                                const anvl_serializer_options_t *opts,
                                bool force_inline) {
   /* AMP guard */
   if (opts->dialect == ANVL_DIALECT_AMP) {
      anvl_error_set(ANVL_ERR_SERIALIZER_AMP_OBJECT_NOT_ALLOWED,
                     "Serializer: AMP dialect does not support Object values",
                     0, 0, __FILE__);
      return;
   }

   bool do_inline = opts->minify || force_inline ||
                    (usize)opts->inline_threshold >= field_count;

   if (do_inline) {
      const char *open = opts->minify ? "{" : "{ ";
      const char *close = opts->minify ? "}" : " }";
      const char *sep = opts->minify ? "," : ", ";

      iw_append(w, open);
      for (usize i = 0; i < field_count; i++) {
         if (i > 0)
            iw_append(w, sep);
         field f = ctx->field_list.fields[field_start + i];
         write_scalar_raw(ctx, w, f->key_pos, f->key_len);
         iw_append(w, w->assign);
         write_field_value(ctx, w, f->val, opts, /*inner_inline=*/true);
      }
      iw_append(w, close);
   } else {
      iw_append(w, "{");
      iw_blank(w);
      iw_push(w);
      for (usize i = 0; i < field_count; i++) {
         bool is_last = (i == field_count - 1);
         field f = ctx->field_list.fields[field_start + i];
         iw_write(w, ""); /* emit indent */
         write_scalar_raw(ctx, w, f->key_pos, f->key_len);
         iw_append(w, w->assign);
         /* inner_inline=true: write_field_value won't emit newline;
          * we add comma (for non-last) then newline ourselves. */
         write_field_value(ctx, w, f->val, opts, /*inner_inline=*/true);
         if (!is_last)
            iw_append(w, ",");
         iw_blank(w);
      }
      iw_pop(w);
      iw_write(w, "}");
      iw_blank(w);
      return;
   }
}

/* ------------------------------------------------------------------ */
/* write_field_value — emit a value from an anvl_value*               */
/*   (field values don't have value_meta; fall back on raw pos/len)   */
/* ------------------------------------------------------------------ */
static void write_field_value(context ctx, iw w,
                              value val,
                              const anvl_serializer_options_t *opts,
                              bool inner_inline) {
   if (!val)
      return;

   switch (val->type) {
   case ANVL_VALUE_SCALAR:
      write_scalar_raw(ctx, w, val->data.scalar.pos, val->data.scalar.len);
      if (!inner_inline)
         iw_blank(w);
      break;

   case ANVL_VALUE_OBJECT:
      write_object_fields(ctx, w,
                          val->data.object.field_start,
                          val->data.object.field_count,
                          opts, /*force_inline=*/inner_inline);
      if (!inner_inline && !opts->minify) {
         /* block already emitted trailing blank; nothing to do */
      }
      break;

   case ANVL_VALUE_ARRAY:
   case ANVL_VALUE_TUPLE:
      /* Nested collection field values lack element_meta; emit verbatim span.
       * element_start/count are set but pos/len per element are in _elem_types_temp
       * which is freed after parse_statement for top-level values only.
       * For nested field collections, emit the raw source span. */
      {
         /* We don't have a clean pos/len for nested collection values.
          * As a fallback, emit element count placeholder — this is a
          * known limitation until nested value tracking is implemented. */
         (void)inner_inline;
         /* Best effort: emit "[]" or "()" empty — rare in practice */
         iw_append(w, val->type == ANVL_VALUE_TUPLE ? "()" : "[]");
         if (!inner_inline)
            iw_blank(w);
      }
      break;

   case ANVL_VALUE_BLOB:
      /* AMP scalar blob — use encoded pos/len */
      {
         uint8_t tag_len = blob_tag_length(val->data.scalar.len);
         usize content_len = blob_content_length(val->data.scalar.len);
         usize content_pos = val->data.scalar.pos;
         const char *src = Source.data(ctx->source);
         if (tag_len > 0) {
            iw_append(w, "@");
            iw_appendn(w, src + content_pos - 1 - tag_len, tag_len);
         }
         iw_append(w, "`");
         iw_appendn(w, src + content_pos, content_len);
         iw_append(w, "`");
         if (!inner_inline)
            iw_blank(w);
      }
      break;
   }
}

/* ------------------------------------------------------------------ */
/* write_value_meta — emit top-level statement value from value_meta  */
/* ------------------------------------------------------------------ */
static void write_value_meta(context ctx, iw w,
                             const struct anvl_value_meta *vm,
                             const anvl_serializer_options_t *opts,
                             bool inner_inline) {
   if (!vm)
      return;

   /* Blob detection first (AML blob is stored as ARRAY internally) */
   if (is_blob_meta(vm)) {
      write_blob_raw(ctx, w, vm);
      if (!inner_inline)
         iw_blank(w);
      return;
   }

   switch (vm->type) {
   case ANVL_VALUE_SCALAR:
      write_scalar_raw(ctx, w, vm->pos, vm->len);
      if (!inner_inline)
         iw_blank(w);
      break;

   case ANVL_VALUE_OBJECT:
      write_object_fields(ctx, w,
                          vm->data.object.field_start,
                          vm->data.object.field_count,
                          opts, /*force_inline=*/inner_inline);
      if (!inner_inline && !opts->minify) {
         /* block already emitted trailing blank */
      }
      break;

   case ANVL_VALUE_ARRAY:
      write_array_elems(ctx, w, vm, opts, /*is_tuple=*/false);
      if (!inner_inline && !opts->minify) {
         /* block already handles newlines */
      } else if (!inner_inline) {
         iw_blank(w);
      }
      break;

   case ANVL_VALUE_TUPLE:
      write_array_elems(ctx, w, vm, opts, /*is_tuple=*/true);
      if (!inner_inline)
         iw_blank(w);
      break;

   case ANVL_VALUE_BLOB:
      /* AMP scalar blob */
      write_blob_raw(ctx, w, vm);
      if (!inner_inline)
         iw_blank(w);
      break;
   }
}

/* ------------------------------------------------------------------ */
/* write_statement                                                     */
/* ------------------------------------------------------------------ */
static bool write_statement(context ctx, iw w, statement stmt,
                            const anvl_serializer_options_t *opts) {
   /* AMP guard: check value type before writing anything */
   if (opts->dialect == ANVL_DIALECT_AMP && stmt->value_meta) {
      anvl_value_type vt = stmt->value_meta->type;
      if (vt == ANVL_VALUE_OBJECT ||
          (vt == ANVL_VALUE_ARRAY && !is_blob_meta(stmt->value_meta)) ||
          vt == ANVL_VALUE_TUPLE) {
         anvl_error_set(ANVL_ERR_SERIALIZER_AMP_OBJECT_NOT_ALLOWED,
                        "Serializer: AMP dialect does not support Object/Array/Tuple values",
                        0, 0, __FILE__);
         return false;
      }
   }

   const char *src_data = Source.data(ctx->source);

   bool is_anon = ((anvl_stmt_type)stmt->meta[STMT_META_TYPE] == ANVL_ANON_OBJECT);

   /* Identifier */
   usize ident_pos = stmt->meta[STMT_META_IDENT_POS];
   usize ident_len = stmt->meta[STMT_META_IDENT_LEN];
   iw_write(w, ""); /* emit indent if needed */
   iw_appendn(w, src_data + ident_pos, ident_len);

   if (is_anon) {
      /* Anonymous block: emit `ident { ... }` (no := operator) */
      iw_append(w, " ");
      write_value_meta(ctx, w, stmt->value_meta, opts, /*inner_inline=*/false);
      return true;
   }

   /* Inheritance :base */
   if (stmt->base_meta && stmt->meta[STMT_META_BASE_IDX]) {
      iw_append(w, ":");
      iw_appendn(w, src_data + stmt->base_meta->pos, stmt->base_meta->len);
   }

   /* Assignment operator */
   iw_append(w, w->assign);

   /* Value */
   write_value_meta(ctx, w, stmt->value_meta, opts, /*inner_inline=*/false);
   return true;
}

/* ------------------------------------------------------------------ */
/* write_root                                                          */
/* ------------------------------------------------------------------ */
static bool write_root(context ctx, iw w,
                       const anvl_serializer_options_t *opts) {
   /* Shebang */
   if (opts->include_shebang) {
      const char *shebang = NULL;
      switch (opts->dialect) {
      case ANVL_DIALECT_AML:
         shebang = "#!aml";
         break;
      case ANVL_DIALECT_ASL:
         shebang = "#!asl";
         break;
      case ANVL_DIALECT_AMP:
         shebang = "#!amp";
         break;
      case ANVL_DIALECT_AURORA:
         shebang = NULL;
         break;
      }
      if (shebang)
         iw_line(w, shebang); /* always real newline even in minify */
   }

   /* Statements */
   usize count = (usize)Context.statement_count(ctx);
   bool first = true;

   for (usize i = 0; i < count; i++) {
      statement stmt = Context.get_statement(ctx, i);
      if (!stmt)
         continue;

      if (opts->minify) {
         if (!first)
            iw_append(w, ",");
         if (first)
            iw_ensure_newline(w);
         first = false;
      } else {
         iw_blank(w);
         (void)first;
         first = false;
      }

      if (!write_statement(ctx, w, stmt, opts))
         return false;
   }

   return true;
}

/* ------------------------------------------------------------------ */
/* Vtable implementations                                             */
/* ------------------------------------------------------------------ */

static string_builder serializer_serialize(context ctx,
                                           const anvl_serializer_options_t *opts) {
   if (!ctx)
      return NULL;
   const anvl_serializer_options_t *o = opts ? opts : &ANVL_SERIALIZER_DEFAULT;

   string_builder sb = StringBuilder.new(256);
   if (!sb)
      return NULL;

   iw_t w;
   iw_init(&w, sb, o->indent_width, o->minify);

   if (!write_root(ctx, &w, o)) {
      StringBuilder.dispose(sb);
      return NULL;
   }

   return sb;
}

static bool serializer_to_stream(context ctx, FILE *out,
                                 const anvl_serializer_options_t *opts) {
   if (!ctx || !out)
      return false;

   string_builder sb = serializer_serialize(ctx, opts);
   if (!sb)
      return false;

   StringBuilder.toStream(sb, out);
   StringBuilder.dispose(sb);
   return true;
}

/* ------------------------------------------------------------------ */
/* Vtable                                                             */
/* ------------------------------------------------------------------ */

const anvl_serializer_i Serializer = {
    .serialize = serializer_serialize,
    .to_stream = serializer_to_stream,
};
