/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 *                                                                        *
 * This software is proprietary and confidential. Unauthorized copying,   *
 * distribution, modification, or use of this software, via any medium,   *
 * is strictly prohibited without express written permission from the     *
 * copyright holder.                                                      *
 *                                                                        *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * anvil_flat.c - Public ABI implementation: flat exported functions      *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * anvil_load runs the whole internal pipeline (header scan, import      *
 * loading, arena creation, body parse of every document in the import    *
 * graph, resolution) and records which *phase* first failed, if any, on  *
 * the returned handle — not a translated internal anvl_err_code, so      *
 * internal error-code churn never becomes a public ABI change. See      *
 * notes/public-api.md.                                                  *
 * ********************************************************************** */

#include "anvil.h"
#include "anvil_flat.h"
#include "internal/module.h"
#include "internal/parser.h"
#include "internal/source.h"
// ----------------
#include <sigma/allocator.h>
#include <sigma/farray.h>
#include <sigma/list.h>
#include <sigma/map.h>
#include <sigma/query.h>
#include <stddef.h>
#include <string.h>

struct anvil_document_t {
   module_context ctx;
   module_document root; // NULL once disposed, or if never successfully registered
   anvil_err_code error; // ANVIL_OK if the whole pipeline succeeded
   anvl_value fragment_value; // set only by anvil_parse_value_fragment; NULL otherwise
   anvil_error error_detail; // NULL until a phase fails; see make_error_detail
   bool owns_ctx; // true for every anvil_load*/parse_value_fragment handle (anvil_dispose tears
                  // down ctx); false for a handle synthesized by anvil_document_iterator_next —
                  // that handle shares its owning document's ctx, so anvil_dispose on it must
                  // only free the small handle itself, never the shared context.
};

struct anvil_error_t {
   anvil_err_code category;
   const char *message; // static message text; NULL if nothing was recorded internally
   usize line;
   usize column;
};

// Builds the diagnostic-detail object for a just-failed pipeline phase. Reads the single
// recorded internal error (anvl_error_set is first-error-wins, so there is never more than one)
// for its specific message/line/column, if the phase ever reached internal recording at all (an
// I/O failure, for instance, never does — doc_load_source's own failure has no source position
// to record). Always allocates, even when nothing internal was recorded, so every failure
// category consistently has a detail object to query (message/line/column simply read as
// empty/0 in that case) rather than some categories having one and others not.
static anvil_error make_error_detail(module_context ctx, anvil_err_code category) {
   struct anvil_error_t *detail = Allocator.alloc(sizeof(struct anvil_error_t));
   if (!detail) {
      return NULL;
   }
   detail->category = category;
   detail->message = NULL;
   detail->line = 0;
   detail->column = 0;

   if (ctx && ctx->errors && List.size(ctx->errors) > 0) {
      anvl_error recorded = anvl_error_get(ctx->errors, 0);
      if (recorded) {
         detail->message = recorded->message;
         detail->line = recorded->line;
         detail->column = recorded->column;
      }
   }
   return (anvil_error)detail;
}

// Shared by anvil_load/anvil_load_buffer - identical pipeline regardless of where the root
// document's source comes from. `register_label` is the symbolic name registered for the root
// document (the real filepath for a file load; a fixed placeholder for a buffer load, which
// has no real path of its own).
static anvil_document load_common(anvl_source_origin origin, const char *source, size_t length,
                                  const char *register_label) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   module_context ctx = NULL;
   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      return NULL; // truly foundational failure - nothing to hand back
   }

   module_document doc = NULL;
   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      return NULL;
   }

   struct anvil_document_t *handle = Allocator.alloc(sizeof(struct anvil_document_t));
   if (!handle) {
      doc_dispose(doc);
      mod_ctx_dispose(ctx);
      return NULL;
   }
   handle->ctx = ctx;
   handle->root = doc;
   handle->error = ANVIL_OK;
   handle->fragment_value = NULL; // load_common's documents are never fragments
   handle->error_detail = NULL;
   handle->owns_ctx = true;

   if (ANVL_RES_OK != doc_load_source(doc, origin, source, length, &err_code)) {
      // Never registered with ctx - mod_ctx_dispose won't reach it, so dispose it directly
      // here to avoid a leak, and clear the handle's reference to it.
      doc_dispose(doc);
      handle->root = NULL;
      handle->error = ANVIL_ERR_IO;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_IO);
      return handle;
   }

   if (ANVL_RES_OK != mod_ctx_register_doc(ctx, doc, register_label, &err_code)) {
      doc_dispose(doc);
      handle->root = NULL;
      handle->error = ANVIL_ERR_IO;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_IO);
      return handle;
   }

   if (ANVL_RES_OK != doc_scan_header(doc, &err_code)) {
      handle->error = ANVIL_ERR_HEADER;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_HEADER);
      return handle;
   }

   usize size_hint = 0;
   if (ANVL_RES_OK != mod_load_imports(ctx, doc, &size_hint, &err_code)) {
      // Also covers a nested import's own header-scan failure (mod_load_imports scans each
      // child's header as it recurses) - the root document's own header was fine, so
      // ANVIL_ERR_IMPORT ("something went wrong resolving the import graph") is the more
      // accurate category from this caller's perspective, not ANVIL_ERR_HEADER.
      handle->error = ANVIL_ERR_IMPORT;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_IMPORT);
      return handle;
   }

   usize capacity = mod_ctx_arena_size_hint(size_hint);
   if (ANVL_RES_OK != mod_ctx_create_arena(ctx, capacity, &err_code)) {
      handle->error = ANVIL_ERR_MEMORY;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_MEMORY);
      return handle;
   }

   usize doc_count = List.size(ctx->docs);
   for (usize i = 0; i < doc_count; i++) {
      module_document d = NULL;
      List.get(ctx->docs, i, (object *)&d);
      if (!d) {
         continue;
      }
      if (ANVL_RES_OK != doc_parse_body(d, &err_code)) {
         handle->error = ANVIL_ERR_SYNTAX;
         handle->error_detail = make_error_detail(ctx, ANVIL_ERR_SYNTAX);
         return handle;
      }
   }

   if (ANVL_RES_OK != mod_resolve_context(ctx, &err_code)) {
      handle->error = ANVIL_ERR_RESOLVE;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_RESOLVE);
      return handle;
   }

   return handle;
}

anvil_document anvil_load(const char *filepath) {
   return load_common(ANVL_SOURCE_FROM_FILE, filepath, 0, filepath);
}

anvil_document anvil_load_buffer(const char *source, size_t length) {
   return load_common(ANVL_SOURCE_FROM_BUFFER, source, length, "<buffer>");
}

anvil_document anvil_parse_value_fragment(const char *text, size_t length) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   module_context ctx = NULL;
   if (ANVL_RES_OK != mod_ctx_initialize(NULL, &ctx, &err_code) || !ctx) {
      return NULL; // truly foundational failure - nothing to hand back
   }

   module_document doc = NULL;
   if (ANVL_RES_OK != doc_initialize(&doc, &err_code) || !doc) {
      mod_ctx_dispose(ctx);
      return NULL;
   }

   struct anvil_document_t *handle = Allocator.alloc(sizeof(struct anvil_document_t));
   if (!handle) {
      doc_dispose(doc);
      mod_ctx_dispose(ctx);
      return NULL;
   }
   handle->ctx = ctx;
   handle->root = doc;
   handle->error = ANVIL_OK;
   handle->fragment_value = NULL;
   handle->error_detail = NULL;
   handle->owns_ctx = true;

   if (ANVL_RES_OK != doc_load_source(doc, ANVL_SOURCE_FROM_BUFFER, text, length, &err_code)) {
      doc_dispose(doc);
      handle->root = NULL;
      handle->error = ANVIL_ERR_IO;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_IO);
      return handle;
   }

   if (ANVL_RES_OK != mod_ctx_register_doc(ctx, doc, "<fragment>", &err_code)) {
      doc_dispose(doc);
      handle->root = NULL;
      handle->error = ANVIL_ERR_IO;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_IO);
      return handle;
   }

   // No header, no imports — a fragment is just the value text itself.
   usize capacity = mod_ctx_arena_size_hint(length);
   if (ANVL_RES_OK != mod_ctx_create_arena(ctx, capacity, &err_code)) {
      handle->error = ANVIL_ERR_MEMORY;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_MEMORY);
      return handle;
   }

   anvl_value value = NULL;
   if (ANVL_RES_OK != anvl_parse_value_fragment(doc, &value, &err_code)) {
      handle->error = ANVIL_ERR_SYNTAX;
      handle->error_detail = make_error_detail(ctx, ANVIL_ERR_SYNTAX);
      return handle;
   }

   handle->fragment_value = value;
   return handle;
}

void anvil_dispose(anvil_document doc) {
   if (!doc) {
      return;
   }
   if (!doc->owns_ctx) {
      // A handle synthesized by anvil_document_iterator_next — shares its owning document's
      // ctx, so disposing it here must only free this small handle, never the shared context
      // the caller's real (owning) document handle still needs. Harmless no-op beyond that.
      Allocator.dispose(doc);
      return;
   }
   mod_ctx_dispose(doc->ctx); // disposes every registered document too (root included, once registered)
   Allocator.dispose(doc->error_detail); // a dedicated wrapper struct (see make_error_detail), not
                                          // arena-owned or ctx-owned — disposed here explicitly
   Allocator.dispose(doc);
}

bool anvil_has_errors(anvil_document doc) {
   if (!doc) {
      return false;
   }
   return doc->error != ANVIL_OK;
}

anvil_err_code anvil_get_error(anvil_document doc) {
   if (!doc) {
      return ANVIL_ERR_INVALID_ARGUMENT;
   }
   return doc->error;
}

const char *anvil_get_version(void) {
   return Anvl.get_version();
}

anvil_statement anvil_statement_get(anvil_document doc, const char *name) {
   if (!doc || !doc->ctx || !doc->ctx->identifiers || !name) {
      return NULL;
   }
   addr val = 0;
   if (!Map.get(doc->ctx->identifiers, name, strlen(name), &val)) {
      return NULL;
   }
   return (anvil_statement)(anvl_statement)val;
}

struct anvil_statement_iterator_t {
   sc_queryable q;
};

anvil_statement_iterator anvil_document_get_statements(anvil_document doc) {
   if (!doc || !doc->root || !doc->root->body) {
      return NULL;
   }
   struct anvil_statement_iterator_t *it = Allocator.alloc(sizeof(struct anvil_statement_iterator_t));
   if (!it) {
      return NULL;
   }
   it->q = FArray.as_queryable(doc->root->body, sizeof(anvl_statement));
   return (anvil_statement_iterator)it;
}

bool anvil_statement_iterator_next(anvil_statement_iterator it, anvil_statement *out_stmt) {
   struct anvil_statement_iterator_t *i = (struct anvil_statement_iterator_t *)it;
   if (!i || !out_stmt) {
      return false;
   }
   const void *element = NULL;
   if (!Query.next(&i->q, &element, NULL)) {
      return false;
   }
   *out_stmt = (anvil_statement)(*(anvl_statement *)element);
   return true;
}

void anvil_statement_iterator_dispose(anvil_statement_iterator it) {
   Allocator.dispose(it);
}

struct anvil_document_iterator_t {
   module_context ctx;
   sc_queryable q;
};

anvil_document_iterator anvil_document_get_imports(anvil_document doc) {
   if (!doc || !doc->root || !doc->root->header) {
      return NULL;
   }
   struct anvil_document_iterator_t *it =
      Allocator.alloc(sizeof(struct anvil_document_iterator_t));
   if (!it) {
      return NULL;
   }
   it->ctx = doc->ctx;
   it->q = List.as_queryable(doc->root->header->imports);
   return (anvil_document_iterator)it;
}

// Each call allocates a fresh handle — the iterator itself never tracks or frees them (see
// anvil_document_iterator's own doc comment: each yielded handle is the caller's to dispose,
// same as any other anvil_document, safe because owns_ctx=false makes that a small, harmless
// free rather than tearing down the shared context).
bool anvil_document_iterator_next(anvil_document_iterator it, anvil_document *out_doc) {
   struct anvil_document_iterator_t *i = (struct anvil_document_iterator_t *)it;
   if (!i || !out_doc) {
      return false;
   }
   const void *element = NULL;
   while (Query.next(&i->q, &element, NULL)) {
      anvl_import imp = *(anvl_import *)element;
      if (!imp || !imp->resolved) {
         continue; // skip a broken/unresolved entry rather than yielding a bad handle
      }
      struct anvil_document_t *handle = Allocator.alloc(sizeof(struct anvil_document_t));
      if (!handle) {
         return false;
      }
      handle->ctx = i->ctx;
      handle->root = imp->resolved;
      handle->error = ANVIL_OK;
      handle->fragment_value = NULL;
      handle->error_detail = NULL;
      handle->owns_ctx = false; // shares i->ctx — anvil_dispose on this handle must not tear it down
      *out_doc = (anvil_document)handle;
      return true;
   }
   return false;
}

void anvil_document_iterator_dispose(anvil_document_iterator it) {
   Allocator.dispose(it); // frees only the scan cursor — never touches any handle it yielded
}

// Forward declaration — defined below, shared by every buffer-supplied accessor; needed here
// too for anvil_error_get_message.
static size_t copy_to_buffer(const char *src, size_t src_len, char *buf, size_t buflen);

anvil_error anvil_document_get_error(anvil_document doc) {
   if (!doc) {
      return NULL;
   }
   return doc->error_detail;
}

anvil_err_code anvil_error_get_category(anvil_error err) {
   struct anvil_error_t *e = (struct anvil_error_t *)err;
   if (!e) {
      return ANVIL_OK;
   }
   return e->category;
}

size_t anvil_error_get_message(anvil_error err, char *buf, size_t buflen) {
   struct anvil_error_t *e = (struct anvil_error_t *)err;
   if (!e || !e->message) {
      return copy_to_buffer("", 0, buf, buflen);
   }
   return copy_to_buffer(e->message, strlen(e->message), buf, buflen);
}

size_t anvil_error_get_line(anvil_error err) {
   struct anvil_error_t *e = (struct anvil_error_t *)err;
   return e ? (size_t)e->line : 0;
}

size_t anvil_error_get_column(anvil_error err) {
   struct anvil_error_t *e = (struct anvil_error_t *)err;
   return e ? (size_t)e->column : 0;
}

anvil_value anvil_statement_get_value(anvil_statement stmt) {
   anvl_statement s = (anvl_statement)stmt;
   if (!s) {
      return NULL;
   }
   return (anvil_value)s->value; // OBJECT_BLOCK synthesizes its own OBJECT value at parse
                                  // time (parser.c) — populated for both statement kinds now
}

// Copies up to buflen-1 bytes from [src, src+src_len) into buf, NUL-terminating - shared by
// every buffer-supplied accessor. Always returns src_len itself (the full, untruncated
// length), regardless of whether buf/buflen was big enough to hold it - matches snprintf's
// convention (call once with buf NULL/buflen 0 to size a buffer).
static size_t copy_to_buffer(const char *src, size_t src_len, char *buf, size_t buflen) {
   if (buf && buflen > 0) {
      size_t n = src_len < buflen - 1 ? src_len : buflen - 1;
      if (n > 0) {
         memcpy(buf, src, n);
      }
      buf[n] = '\0';
   }
   return src_len;
}

size_t anvil_statement_get_name(anvil_statement stmt, char *buf, size_t buflen) {
   anvl_statement s = (anvl_statement)stmt;
   if (!s) {
      return copy_to_buffer("", 0, buf, buflen);
   }
   return copy_to_buffer(s->name.start, (size_t)Source.slice_length(s->name), buf, buflen);
}

// Follows a resolved VarRef to its (already-flattened, by the resolver) final concrete
// value. Returns NULL for an unresolved VarRef (missing target, cycle) - callers translate
// that to ANVIL_VALUE_NULL / an empty/zero result, matching VarRef's overall "unresolved is
// not an error" policy.
static anvl_value deref_varref(anvl_value v) {
   if (v && v->type == ANVL_VALUE_VARREF) {
      return v->varref.resolved;
   }
   return v;
}

anvil_value_type anvil_value_get_type(anvil_value val) {
   anvl_value v = deref_varref((anvl_value)val);
   if (!v) {
      return ANVIL_VALUE_NULL;
   }
   switch (v->type) {
   case ANVL_VALUE_BOOL:
      return ANVIL_VALUE_BOOL;
   case ANVL_VALUE_NUMERIC:
      return ANVIL_VALUE_NUMERIC;
   case ANVL_VALUE_STRING:
      return ANVIL_VALUE_STRING;
   case ANVL_VALUE_BLOB:
      return ANVIL_VALUE_BLOB;
   case ANVL_VALUE_IDENTIFIER:
      return ANVIL_VALUE_IDENTIFIER;
   case ANVL_VALUE_ARRAY:
      return ANVIL_VALUE_ARRAY;
   case ANVL_VALUE_TUPLE:
      return ANVIL_VALUE_TUPLE;
   case ANVL_VALUE_OBJECT:
      return ANVIL_VALUE_OBJECT;
   case ANVL_VALUE_NULL:
   case ANVL_VALUE_NONE:
   case ANVL_VALUE_VARREF:
   default:
      return ANVIL_VALUE_NULL;
   }
}

// Resolves the minimal, deterministic escape set (\n \t \r \\ \") into out_buf (capacity
// out_cap), or just computes and returns the resolved length if out_buf is NULL - one
// function serves both the length-query and the actual-copy call, matching
// anvil_value_get_text's two-call buffer-sizing convention. An unrecognized escape (backslash
// followed by anything else) passes both characters through unchanged - never ambiguous,
// never silently drops data.
static size_t resolve_string_escapes(const char *raw, size_t raw_len, char *out_buf,
                                     size_t out_cap) {
   size_t out_len = 0;
   for (size_t i = 0; i < raw_len; i++) {
      char c = raw[i];
      char resolved = c;
      bool is_escape = false;
      if (c == '\\' && i + 1 < raw_len) {
         switch (raw[i + 1]) {
         case 'n':
            resolved = '\n';
            is_escape = true;
            break;
         case 't':
            resolved = '\t';
            is_escape = true;
            break;
         case 'r':
            resolved = '\r';
            is_escape = true;
            break;
         case '\\':
            resolved = '\\';
            is_escape = true;
            break;
         case '"':
            resolved = '"';
            is_escape = true;
            break;
         default:
            break; // unrecognized - fall through, keep '\' as its own output byte
         }
      }
      if (out_buf && out_len < out_cap) {
         out_buf[out_len] = resolved;
      }
      out_len++;
      if (is_escape) {
         i++; // consume the escaped character too
      }
   }
   return out_len;
}

size_t anvil_value_get_text(anvil_value val, char *buf, size_t buflen) {
   anvl_value v = deref_varref((anvl_value)val);
   if (!v) {
      return copy_to_buffer("", 0, buf, buflen);
   }
   usize raw_len = Source.slice_length(v->text);
   if (v->type != ANVL_VALUE_STRING) {
      return copy_to_buffer(v->text.start, (size_t)raw_len, buf, buflen);
   }
   size_t needed = resolve_string_escapes(v->text.start, (size_t)raw_len, NULL, 0);
   if (buf && buflen > 0) {
      resolve_string_escapes(v->text.start, (size_t)raw_len, buf, buflen - 1);
      size_t term_at = needed < buflen - 1 ? needed : buflen - 1;
      buf[term_at] = '\0';
   }
   return needed;
}

size_t anvil_value_get_count(anvil_value val) {
   anvl_value v = deref_varref((anvl_value)val);
   if (!v) {
      return 0;
   }
   if (v->type == ANVL_VALUE_ARRAY || v->type == ANVL_VALUE_TUPLE) {
      return (size_t)List.size(v->collection.items);
   }
   if (v->type == ANVL_VALUE_OBJECT) {
      return (size_t)List.size(v->object.statements);
   }
   return 0;
}

anvil_value anvil_value_get_element(anvil_value val, size_t index) {
   anvl_value v = deref_varref((anvl_value)val);
   if (!v || (v->type != ANVL_VALUE_ARRAY && v->type != ANVL_VALUE_TUPLE)) {
      return NULL;
   }
   if (index >= (size_t)List.size(v->collection.items)) {
      return NULL;
   }
   anvl_value elem = NULL;
   List.get(v->collection.items, index, (object *)&elem);
   return (anvil_value)elem;
}

anvil_statement anvil_value_get_statement(anvil_value val, size_t index) {
   anvl_value v = deref_varref((anvl_value)val);
   if (!v || v->type != ANVL_VALUE_OBJECT) {
      return NULL;
   }
   if (index >= (size_t)List.size(v->object.statements)) {
      return NULL;
   }
   anvl_statement stmt = NULL;
   List.get(v->object.statements, index, (object *)&stmt);
   return (anvil_statement)stmt;
}

anvil_value anvil_document_get_fragment_value(anvil_document doc) {
   if (!doc) {
      return NULL;
   }
   return (anvil_value)doc->fragment_value;
}

size_t anvil_document_get_attribute_count(anvil_document doc) {
   if (!doc || !doc->root || !doc->root->header) {
      return 0;
   }
   return (size_t)List.size(doc->root->header->attributes);
}

anvil_attribute anvil_document_get_attribute(anvil_document doc, size_t index) {
   if (!doc || !doc->root || !doc->root->header) {
      return NULL;
   }
   list attrs = doc->root->header->attributes;
   if (index >= (size_t)List.size(attrs)) {
      return NULL;
   }
   anvl_attribute attr = NULL;
   List.get(attrs, index, (object *)&attr);
   return (anvil_attribute)attr;
}

anvil_attribute anvil_document_find_attribute(anvil_document doc, const char *key) {
   if (!doc || !doc->root || !doc->root->header || !key) {
      return NULL;
   }
   list attrs = doc->root->header->attributes;
   usize count = List.size(attrs);
   for (usize i = 0; i < count; i++) {
      anvl_attribute attr = NULL;
      List.get(attrs, i, (object *)&attr);
      if (attr && Source.slice_equals(attr->key, key)) {
         return (anvil_attribute)attr;
      }
   }
   return NULL;
}

size_t anvil_statement_get_attribute_count(anvil_statement stmt) {
   anvl_statement s = (anvl_statement)stmt;
   if (!s) {
      return 0;
   }
   return (size_t)List.size(s->attributes);
}

anvil_attribute anvil_statement_get_attribute(anvil_statement stmt, size_t index) {
   anvl_statement s = (anvl_statement)stmt;
   if (!s || index >= (size_t)List.size(s->attributes)) {
      return NULL;
   }
   anvl_attribute attr = NULL;
   List.get(s->attributes, index, (object *)&attr);
   return (anvil_attribute)attr;
}

anvil_attribute anvil_statement_find_attribute(anvil_statement stmt, const char *key) {
   anvl_statement s = (anvl_statement)stmt;
   if (!s || !key) {
      return NULL;
   }
   usize count = List.size(s->attributes);
   for (usize i = 0; i < count; i++) {
      anvl_attribute attr = NULL;
      List.get(s->attributes, i, (object *)&attr);
      if (attr && Source.slice_equals(attr->key, key)) {
         return (anvil_attribute)attr;
      }
   }
   return NULL;
}

size_t anvil_attribute_get_key(anvil_attribute attr, char *buf, size_t buflen) {
   anvl_attribute a = (anvl_attribute)attr;
   if (!a) {
      return copy_to_buffer("", 0, buf, buflen);
   }
   return copy_to_buffer(a->key.start, (size_t)Source.slice_length(a->key), buf, buflen);
}

size_t anvil_attribute_get_value(anvil_attribute attr, char *buf, size_t buflen) {
   anvl_attribute a = (anvl_attribute)attr;
   if (!a) {
      return copy_to_buffer("", 0, buf, buflen);
   }
   return copy_to_buffer(a->value.start, (size_t)Source.slice_length(a->value), buf, buflen);
}
