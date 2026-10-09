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
 * builder.c - Document builder                                           *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Implements anvil_builder.h. A tree of nodes and members, validated at  *
 * each call with the same rules (grammar.c) the streaming writer uses,   *
 * and emitted by driving that writer - so the builder adds no emission   *
 * logic of its own. Every node and member is also recorded in a flat     *
 * registry, so disposal never recurses however deep the tree is.         *
 * ********************************************************************** */

#include "anvil_builder.h"
#include "anvil_writer.h"
#include "grammar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
   NODE_NULL,
   NODE_BOOL,
   NODE_NUMERIC,
   NODE_STRING,
   NODE_BARE,
   NODE_BLOB,
   NODE_VARREF,
   NODE_ARRAY,
   NODE_TUPLE,
   NODE_OBJECT,
} node_kind;

typedef struct {
   char *key;
   char *value; // NULL for a flag attribute
   size_t value_len;
   bool quoted;
} attr;

typedef struct {
   attr *items;
   size_t count;
   size_t cap;
} attr_list;

struct anvil_node_t {
   anvil_builder builder;
   node_kind kind;
   bool attached;
   struct anvil_node_t *parent; // the array/tuple/object holding this node; NULL at document level
   bool flag;                   // NODE_BOOL
   char *text;                  // scalar text, blob content, or varref name
   size_t text_len;
   char *tag;                   // NODE_BLOB
   struct anvil_node_t **elements; // NODE_ARRAY / NODE_TUPLE
   struct anvil_member_t **members; // NODE_OBJECT
   size_t count;
   size_t cap;
};

struct anvil_member_t {
   char *name;
   char *base;
   attr_list attrs;
   struct anvil_node_t *value;
};

// A module-header entry: an attribute, or (is_include) an include whose path is item.key.
typedef struct {
   attr item;
   bool is_include;
} header_item;

struct anvil_builder_t {
   anvil_writer_dialect dialect;
   anvil_writer_err_code err;
   header_item *header;
   size_t header_count;
   size_t header_cap;
   struct anvil_member_t **members; // document-level statements, in order
   size_t member_count;
   size_t member_cap;
   wg_nameset names; // document-level statement names
   struct anvil_node_t **nodes; // every node, for disposal
   size_t node_count;
   size_t node_cap;
   struct anvil_member_t **all_members; // every member, for disposal
   size_t all_member_count;
   size_t all_member_cap;
   anvil_writer emitted;
};

/* ----------------------------------------------------------------------- *
 * Helpers
 * ----------------------------------------------------------------------- */
// Records the outcome of a call; returns whether it succeeded.
static bool outcome(anvil_builder b, anvil_writer_err_code code) {
   b->err = code;
   return code == ANVIL_WRITER_OK;
}

static char *copy_bytes(const char *s, size_t n) {
   char *out = malloc(n + 1);
   if (out) {
      if (n) {
         memcpy(out, s, n);
      }
      out[n] = '\0';
   }
   return out;
}

static char *copy_string(const char *s) { return copy_bytes(s, strlen(s)); }

// Grows a pointer array to hold one more element.
static bool reserve(void **array, size_t count, size_t *cap, size_t element_size) {
   if (count < *cap) {
      return true;
   }
   size_t grown = *cap ? *cap * 2 : 8;
   void *bigger = realloc(*array, grown * element_size);
   if (!bigger) {
      return false;
   }
   *array = bigger;
   *cap = grown;
   return true;
}

static void free_attr(attr *a) {
   free(a->key);
   free(a->value);
}

static bool add_attr(attr_list *list, const char *key, const char *value, size_t value_len,
                     bool quoted) {
   if (!reserve((void **)&list->items, list->count, &list->cap, sizeof *list->items)) {
      return false;
   }
   attr a = {.key = copy_string(key), .value_len = value_len, .quoted = quoted};
   if (value) {
      a.value = copy_bytes(value, value_len);
   }
   if (!a.key || (value && !a.value)) {
      free_attr(&a);
      return false;
   }
   list->items[list->count++] = a;
   return true;
}

/* ----------------------------------------------------------------------- *
 * Lifecycle
 * ----------------------------------------------------------------------- */
anvil_builder anvil_builder_new(anvil_writer_dialect dialect) {
   if (dialect != ANVIL_WRITER_AML && dialect != ANVIL_WRITER_AMP) {
      return NULL;
   }
   anvil_builder b = calloc(1, sizeof *b);
   if (b) {
      b->dialect = dialect;
   }
   return b;
}

void anvil_builder_dispose(anvil_builder b) {
   if (!b) {
      return;
   }
   for (size_t i = 0; i < b->node_count; i++) {
      struct anvil_node_t *n = b->nodes[i];
      free(n->text);
      free(n->tag);
      free(n->elements);
      free(n->members);
      free(n);
   }
   for (size_t i = 0; i < b->all_member_count; i++) {
      struct anvil_member_t *m = b->all_members[i];
      for (size_t a = 0; a < m->attrs.count; a++) {
         free_attr(&m->attrs.items[a]);
      }
      free(m->attrs.items);
      free(m->name);
      free(m->base);
      free(m);
   }
   for (size_t i = 0; i < b->header_count; i++) {
      free_attr(&b->header[i].item);
   }
   free(b->header);
   free(b->nodes);
   free(b->all_members);
   free(b->members);
   wg_nameset_free(&b->names);
   anvil_writer_dispose(b->emitted);
   free(b);
}

anvil_writer_err_code anvil_builder_get_error(anvil_builder b) {
   return b ? b->err : ANVIL_WRITER_ERR_INVALID_ARGUMENT;
}

/* ----------------------------------------------------------------------- *
 * Header
 * ----------------------------------------------------------------------- */
static bool add_header(anvil_builder b, bool is_include, const char *key, const char *value,
                       size_t value_len, bool quoted) {
   if (!reserve((void **)&b->header, b->header_count, &b->header_cap, sizeof *b->header)) {
      return outcome(b, ANVIL_WRITER_ERR_MEMORY);
   }
   attr a = {.key = copy_string(key), .value_len = value_len, .quoted = quoted};
   if (value) {
      a.value = copy_bytes(value, value_len);
   }
   if (!a.key || (value && !a.value)) {
      free_attr(&a);
      return outcome(b, ANVIL_WRITER_ERR_MEMORY);
   }
   b->header[b->header_count++] = (header_item){.item = a, .is_include = is_include};
   return outcome(b, ANVIL_WRITER_OK);
}

bool anvil_builder_attribute(anvil_builder b, const char *key, const char *value) {
   if (!b) {
      return false;
   }
   if (b->dialect == ANVIL_WRITER_AMP) {
      return outcome(b, ANVIL_WRITER_ERR_DIALECT);
   }
   anvil_writer_err_code code = wg_check_attribute_key(key);
   if (code == ANVIL_WRITER_OK && value) {
      code = wg_check_attribute_value(value);
   }
   if (code != ANVIL_WRITER_OK) {
      return outcome(b, code);
   }
   return add_header(b, false, key, value, value ? strlen(value) : 0, false);
}

bool anvil_builder_attribute_string(anvil_builder b, const char *key, const char *text, size_t length) {
   if (!b) {
      return false;
   }
   if (b->dialect == ANVIL_WRITER_AMP) {
      return outcome(b, ANVIL_WRITER_ERR_DIALECT);
   }
   anvil_writer_err_code code = wg_check_attribute_key(key);
   if (code == ANVIL_WRITER_OK) {
      code = wg_check_attribute_text(text, length);
   }
   if (code != ANVIL_WRITER_OK) {
      return outcome(b, code);
   }
   return add_header(b, false, key, text ? text : "", length, true);
}

bool anvil_builder_include(anvil_builder b, const char *path) {
   if (!b) {
      return false;
   }
   if (b->dialect == ANVIL_WRITER_AMP) {
      return outcome(b, ANVIL_WRITER_ERR_DIALECT);
   }
   anvil_writer_err_code code = wg_check_include_path(path);
   if (code != ANVIL_WRITER_OK) {
      return outcome(b, code);
   }
   return add_header(b, true, path, NULL, 0, false);
}

/* ----------------------------------------------------------------------- *
 * Values
 * ----------------------------------------------------------------------- */
// Allocates a detached node and records it for disposal. NULL (with the error set) on failure.
static anvil_node new_node(anvil_builder b, node_kind kind) {
   if (!reserve((void **)&b->nodes, b->node_count, &b->node_cap, sizeof *b->nodes)) {
      outcome(b, ANVIL_WRITER_ERR_MEMORY);
      return NULL;
   }
   anvil_node n = calloc(1, sizeof *n);
   if (!n) {
      outcome(b, ANVIL_WRITER_ERR_MEMORY);
      return NULL;
   }
   n->builder = b;
   n->kind = kind;
   b->nodes[b->node_count++] = n;
   outcome(b, ANVIL_WRITER_OK);
   return n;
}

// A scalar node whose text is `text` (copied). `code` is the already-computed validation result.
static anvil_node scalar_node(anvil_builder b, node_kind kind, anvil_writer_err_code code,
                              const char *text, size_t length) {
   if (!b) {
      return NULL;
   }
   if (code != ANVIL_WRITER_OK) {
      outcome(b, code);
      return NULL;
   }
   anvil_node n = new_node(b, kind);
   if (!n) {
      return NULL;
   }
   if (text) {
      n->text = copy_bytes(text, length);
      if (!n->text) {
         outcome(b, ANVIL_WRITER_ERR_MEMORY);
         return NULL; // the empty node stays in the registry and is freed with the builder
      }
      n->text_len = length;
   }
   return n;
}

anvil_node anvil_builder_null(anvil_builder b) {
   return scalar_node(b, NODE_NULL, ANVIL_WRITER_OK, NULL, 0);
}

anvil_node anvil_builder_bool(anvil_builder b, bool value) {
   anvil_node n = scalar_node(b, NODE_BOOL, ANVIL_WRITER_OK, NULL, 0);
   if (n) {
      n->flag = value;
   }
   return n;
}

anvil_node anvil_builder_numeric(anvil_builder b, const char *text) {
   return scalar_node(b, NODE_NUMERIC, wg_check_numeric(text), text, text ? strlen(text) : 0);
}

anvil_node anvil_builder_int(anvil_builder b, int64_t value) {
   char text[32];
   snprintf(text, sizeof text, "%lld", (long long)value);
   return anvil_builder_numeric(b, text);
}

anvil_node anvil_builder_double(anvil_builder b, double value) {
   char text[40];
   if (!b) {
      return NULL;
   }
   if (!wg_format_double(value, text)) {
      outcome(b, ANVIL_WRITER_ERR_INVALID_NUMERIC);
      return NULL;
   }
   return anvil_builder_numeric(b, text);
}

anvil_node anvil_builder_string(anvil_builder b, const char *text, size_t length) {
   return scalar_node(b, NODE_STRING, wg_check_string(text, length), text ? text : "", length);
}

anvil_node anvil_builder_bare(anvil_builder b, const char *text) {
   return scalar_node(b, NODE_BARE, wg_check_bare(text), text, text ? strlen(text) : 0);
}

anvil_node anvil_builder_blob(anvil_builder b, const char *tag, const char *data, size_t length) {
   anvil_node n = scalar_node(b, NODE_BLOB, wg_check_blob(tag, data, length), data ? data : "", length);
   if (n && tag && *tag) {
      n->tag = copy_string(tag);
      if (!n->tag) {
         outcome(b, ANVIL_WRITER_ERR_MEMORY);
         return NULL;
      }
   }
   return n;
}

anvil_node anvil_builder_varref(anvil_builder b, const char *name) {
   if (b && b->dialect == ANVIL_WRITER_AMP) {
      outcome(b, ANVIL_WRITER_ERR_DIALECT);
      return NULL;
   }
   return scalar_node(b, NODE_VARREF, wg_check_name(name), name, name ? strlen(name) : 0);
}

anvil_node anvil_builder_array(anvil_builder b) { return b ? new_node(b, NODE_ARRAY) : NULL; }
anvil_node anvil_builder_tuple(anvil_builder b) { return b ? new_node(b, NODE_TUPLE) : NULL; }

anvil_node anvil_builder_object(anvil_builder b) {
   if (!b) {
      return NULL;
   }
   if (b->dialect == ANVIL_WRITER_AMP) {
      outcome(b, ANVIL_WRITER_ERR_DIALECT);
      return NULL;
   }
   return new_node(b, NODE_OBJECT);
}

/* ----------------------------------------------------------------------- *
 * Structure
 * ----------------------------------------------------------------------- */
static bool is_collection(const struct anvil_node_t *n) {
   return n->kind == NODE_ARRAY || n->kind == NODE_TUPLE;
}

// Whether `node` is `candidate` or sits inside it, walking up through the containers.
static bool contained_in(const struct anvil_node_t *node, const struct anvil_node_t *candidate) {
   for (; node; node = node->parent) {
      if (node == candidate) {
         return true;
      }
   }
   return false;
}

// Common argument checks for attaching `value` under `container` (NULL = the document).
static anvil_writer_err_code attach_check(anvil_builder b, const struct anvil_node_t *container,
                                          const struct anvil_node_t *value) {
   if (!value || value->builder != b || (container && container->builder != b)) {
      return ANVIL_WRITER_ERR_INVALID_ARGUMENT;
   }
   if (value->attached || contained_in(container, value)) {
      return ANVIL_WRITER_ERR_STATE;
   }
   return ANVIL_WRITER_OK;
}

bool anvil_builder_append(anvil_builder b, anvil_node collection, anvil_node element) {
   if (!b) {
      return false;
   }
   if (!collection || collection->builder != b) {
      return outcome(b, ANVIL_WRITER_ERR_INVALID_ARGUMENT);
   }
   if (!is_collection(collection)) {
      return outcome(b, ANVIL_WRITER_ERR_STATE);
   }
   anvil_writer_err_code code = attach_check(b, collection, element);
   if (code == ANVIL_WRITER_OK && b->dialect == ANVIL_WRITER_AMP &&
       (is_collection(element) || element->kind == NODE_OBJECT)) {
      code = ANVIL_WRITER_ERR_DIALECT;
   }
   if (code != ANVIL_WRITER_OK) {
      return outcome(b, code);
   }
   if (!reserve((void **)&collection->elements, collection->count, &collection->cap,
                sizeof *collection->elements)) {
      return outcome(b, ANVIL_WRITER_ERR_MEMORY);
   }
   collection->elements[collection->count++] = element;
   element->attached = true;
   element->parent = collection;
   return outcome(b, ANVIL_WRITER_OK);
}

anvil_member anvil_builder_add(anvil_builder b, anvil_node object, const char *name, anvil_node value) {
   if (!b) {
      return NULL;
   }
   if (object && (object->builder != b || object->kind != NODE_OBJECT)) {
      outcome(b, object->builder != b ? ANVIL_WRITER_ERR_INVALID_ARGUMENT : ANVIL_WRITER_ERR_STATE);
      return NULL;
   }
   anvil_writer_err_code code = wg_check_name(name);
   if (code == ANVIL_WRITER_OK) {
      code = attach_check(b, object, value);
   }
   if (code != ANVIL_WRITER_OK) {
      outcome(b, code);
      return NULL;
   }
   struct anvil_member_t ***list = object ? &object->members : &b->members;
   size_t *count = object ? &object->count : &b->member_count;
   size_t *cap = object ? &object->cap : &b->member_cap;
   if (!reserve((void **)list, *count, cap, sizeof **list) ||
       !reserve((void **)&b->all_members, b->all_member_count, &b->all_member_cap,
                sizeof *b->all_members)) {
      outcome(b, ANVIL_WRITER_ERR_MEMORY);
      return NULL;
   }
   struct anvil_member_t *m = calloc(1, sizeof *m);
   if (!m || !(m->name = copy_string(name))) {
      free(m);
      outcome(b, ANVIL_WRITER_ERR_MEMORY);
      return NULL;
   }
   // Top-level names are unique (the reader's resolver requires it); nested names may repeat.
   if (!object) {
      int added = wg_nameset_add(&b->names, name);
      if (added <= 0) {
         free(m->name);
         free(m);
         outcome(b, added < 0 ? ANVIL_WRITER_ERR_MEMORY : ANVIL_WRITER_ERR_DUPLICATE_NAME);
         return NULL;
      }
   }
   m->value = value;
   value->attached = true;
   value->parent = object;
   (*list)[(*count)++] = m;
   b->all_members[b->all_member_count++] = m;
   outcome(b, ANVIL_WRITER_OK);
   return m;
}

static bool member_ready(anvil_builder b, anvil_member m) {
   if (!b) {
      return false;
   }
   if (!m) {
      return outcome(b, ANVIL_WRITER_ERR_INVALID_ARGUMENT);
   }
   if (b->dialect == ANVIL_WRITER_AMP) {
      return outcome(b, ANVIL_WRITER_ERR_DIALECT);
   }
   return true;
}

bool anvil_builder_set_base(anvil_builder b, anvil_member m, const char *base) {
   if (!member_ready(b, m)) {
      return false;
   }
   anvil_writer_err_code code = wg_check_name(base);
   if (code == ANVIL_WRITER_OK && m->value->kind != NODE_OBJECT) {
      code = ANVIL_WRITER_ERR_INHERITANCE_REQUIRES_OBJECT;
   }
   if (code != ANVIL_WRITER_OK) {
      return outcome(b, code);
   }
   char *copy = copy_string(base);
   if (!copy) {
      return outcome(b, ANVIL_WRITER_ERR_MEMORY);
   }
   free(m->base);
   m->base = copy;
   return outcome(b, ANVIL_WRITER_OK);
}

bool anvil_builder_member_attribute(anvil_builder b, anvil_member m, const char *key, const char *value) {
   if (!member_ready(b, m)) {
      return false;
   }
   anvil_writer_err_code code = wg_check_attribute_key(key);
   if (code == ANVIL_WRITER_OK && value) {
      code = wg_check_attribute_value(value);
   }
   if (code != ANVIL_WRITER_OK) {
      return outcome(b, code);
   }
   return outcome(b, add_attr(&m->attrs, key, value, value ? strlen(value) : 0, false)
                         ? ANVIL_WRITER_OK
                         : ANVIL_WRITER_ERR_MEMORY);
}

bool anvil_builder_member_attribute_string(anvil_builder b, anvil_member m, const char *key,
                                           const char *text, size_t length) {
   if (!member_ready(b, m)) {
      return false;
   }
   anvil_writer_err_code code = wg_check_attribute_key(key);
   if (code == ANVIL_WRITER_OK) {
      code = wg_check_attribute_text(text, length);
   }
   if (code != ANVIL_WRITER_OK) {
      return outcome(b, code);
   }
   return outcome(b, add_attr(&m->attrs, key, text ? text : "", length, true) ? ANVIL_WRITER_OK
                                                                              : ANVIL_WRITER_ERR_MEMORY);
}

anvil_member anvil_builder_find(anvil_builder b, anvil_node object, const char *name) {
   if (!b || !name || (object && (object->builder != b || object->kind != NODE_OBJECT))) {
      return NULL;
   }
   struct anvil_member_t **members = object ? object->members : b->members;
   size_t count = object ? object->count : b->member_count;
   for (size_t i = 0; i < count; i++) {
      if (strcmp(members[i]->name, name) == 0) {
         return members[i];
      }
   }
   return NULL;
}

/* ----------------------------------------------------------------------- *
 * Output
 * ----------------------------------------------------------------------- */
static bool emit_node(anvil_writer w, const struct anvil_node_t *n);

static bool emit_member(anvil_writer w, const struct anvil_member_t *m) {
   if (!anvil_writer_statement(w, m->name, m->base)) {
      return false;
   }
   for (size_t i = 0; i < m->attrs.count; i++) {
      const attr *a = &m->attrs.items[i];
      bool ok = a->quoted ? anvil_writer_attribute_string(w, a->key, a->value, a->value_len)
                          : anvil_writer_attribute(w, a->key, a->value);
      if (!ok) {
         return false;
      }
   }
   return emit_node(w, m->value);
}

static bool emit_node(anvil_writer w, const struct anvil_node_t *n) {
   switch (n->kind) {
   case NODE_NULL:
      return anvil_writer_null(w);
   case NODE_BOOL:
      return anvil_writer_bool(w, n->flag);
   case NODE_NUMERIC:
      return anvil_writer_numeric(w, n->text);
   case NODE_STRING:
      return anvil_writer_string(w, n->text, n->text_len);
   case NODE_BARE:
      return anvil_writer_bare(w, n->text);
   case NODE_BLOB:
      return anvil_writer_blob(w, n->tag, n->text, n->text_len);
   case NODE_VARREF:
      return anvil_writer_varref(w, n->text);
   case NODE_ARRAY:
   case NODE_TUPLE:
   case NODE_OBJECT:
      break;
   }
   bool is_object = n->kind == NODE_OBJECT;
   bool ok = is_object ? anvil_writer_begin_object(w)
                       : n->kind == NODE_ARRAY ? anvil_writer_begin_array(w) : anvil_writer_begin_tuple(w);
   for (size_t i = 0; ok && i < n->count; i++) {
      ok = is_object ? emit_member(w, n->members[i]) : emit_node(w, n->elements[i]);
   }
   if (!ok) {
      return false;
   }
   return is_object ? anvil_writer_end_object(w)
                    : n->kind == NODE_ARRAY ? anvil_writer_end_array(w) : anvil_writer_end_tuple(w);
}

typedef struct {
   const struct anvil_node_t *node;
   size_t depth; // containers above this node
} walk_item;

// Whether any array/tuple/object reachable from the document nests deeper than the emit limit.
// Walks with an explicit stack: a tree this deep must not be walked by recursion. The result
// is an error code: OK, DEPTH_EXCEEDED, or MEMORY.
static anvil_writer_err_code check_depth(anvil_builder b) {
   walk_item *stack = NULL;
   size_t count = 0, cap = 0;
   anvil_writer_err_code result = ANVIL_WRITER_OK;
   for (size_t i = 0; result == ANVIL_WRITER_OK && i < b->member_count; i++) {
      if (!reserve((void **)&stack, count, &cap, sizeof *stack)) {
         result = ANVIL_WRITER_ERR_MEMORY;
         break;
      }
      stack[count++] = (walk_item){b->members[i]->value, 0};
   }
   while (result == ANVIL_WRITER_OK && count > 0) {
      walk_item item = stack[--count];
      const struct anvil_node_t *n = item.node;
      if (n->kind != NODE_ARRAY && n->kind != NODE_TUPLE && n->kind != NODE_OBJECT) {
         continue;
      }
      if (item.depth + 1 > ANVIL_BUILDER_MAX_DEPTH) {
         result = ANVIL_WRITER_ERR_DEPTH_EXCEEDED;
         break;
      }
      for (size_t i = 0; i < n->count; i++) {
         if (!reserve((void **)&stack, count, &cap, sizeof *stack)) {
            result = ANVIL_WRITER_ERR_MEMORY;
            break;
         }
         const struct anvil_node_t *child = n->kind == NODE_OBJECT ? n->members[i]->value : n->elements[i];
         stack[count++] = (walk_item){child, item.depth + 1};
      }
   }
   free(stack);
   return result;
}

bool anvil_builder_write(anvil_builder b, anvil_writer w) {
   if (!b) {
      return false;
   }
   if (!w) {
      return outcome(b, ANVIL_WRITER_ERR_INVALID_ARGUMENT);
   }
   anvil_writer_err_code depth = check_depth(b);
   if (depth != ANVIL_WRITER_OK) {
      return outcome(b, depth);
   }
   bool ok = true;
   for (size_t i = 0; ok && i < b->header_count; i++) {
      const header_item *h = &b->header[i];
      if (h->is_include) {
         ok = anvil_writer_include(w, h->item.key);
      } else if (h->item.quoted) {
         ok = anvil_writer_attribute_string(w, h->item.key, h->item.value, h->item.value_len);
      } else {
         ok = anvil_writer_attribute(w, h->item.key, h->item.value);
      }
   }
   for (size_t i = 0; ok && i < b->member_count; i++) {
      ok = emit_member(w, b->members[i]);
   }
   return outcome(b, ok ? ANVIL_WRITER_OK : anvil_writer_get_error(w));
}

const char *anvil_builder_emit(anvil_builder b, size_t *length) {
   if (!b) {
      return NULL;
   }
   anvil_writer_dispose(b->emitted);
   b->emitted = anvil_writer_new(b->dialect);
   if (!b->emitted) {
      outcome(b, ANVIL_WRITER_ERR_MEMORY);
      return NULL;
   }
   if (!anvil_builder_write(b, b->emitted) || !anvil_writer_finish(b->emitted)) {
      if (b->err == ANVIL_WRITER_OK) {
         outcome(b, anvil_writer_get_error(b->emitted));
      }
      return NULL;
   }
   return anvil_writer_data(b->emitted, length);
}
