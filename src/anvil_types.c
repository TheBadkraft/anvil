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
 * anvil_types.c - Anvil's opt-in type registry                           *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Part of ANVL proper — opt-in, not an add-on: usable without schema.c,  *
 * which is the actual add-on layered on top of this (schema.c depends on *
 * this file, never the reverse; AnvilScript will depend on it directly   *
 * too). Lives directly under src/, a sibling of core/ and sigma/, not    *
 * nested under src/schema/ (that directory is reserved for schema.c      *
 * itself). Built entirely on the public flat API (anvil_flat.h) — no     *
 * core parser/resolver access. See notes/native-schema.md.               *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_type_registry.h"
#include "internal/format.h"
#include "internal/numeric_bound.h"
#include "internal/text_alloc.h"
// ----------------
#include <sigma.core/allocator.h>
#include <sigma/list.h>
#include <sigma/map.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct anvil_type_def_t {
   char *name; // allocated to its full length for a registry-owned def; the native defs below
               // point at string literals and are never freed
   anvil_type_kind kind;
   bool has_size;
   long long size;
   bool has_min;
   anvil_numeric_t min;
   bool has_max;
   anvil_numeric_t max;
   list values; // list of individually-allocated char* labels — only meaningful for
                // ANVIL_TYPE_ENUM; NULL if `values` wasn't declared
   char *source; // path of the document that defined it (for collision reports); NULL for natives
   char *format; // a known lexical format (internal/format.h); NULL if none
   char *tag; // a Blob type's required `tag := ...`; NULL if none
   bool integer_declared; // `integer := true|false` was present and well-formed
   bool integer;
   bool nullable_declared; // `nullable := true|false` was present and well-formed
   bool nullable;
   size_t fields_len; // exact byte length of fields_text (it may contain NUL bytes)
   char *fields_text; // source text of `fields := { ... }` (copied), NULL if none - see
                      // anvil_type_def_fields_text
};

struct anvil_type_diagnostic_t {
   anvil_type_diag_code category;
   char *name;    // the type the diagnostic is about
   char *source;  // the document it was found in
   char *message; // human-readable; all three allocated to their full length
};

struct anvil_type_registry_t {
   map by_name;   // def->name -> struct anvil_type_def_t* (lookup)
   list all_defs; // struct anvil_type_def_t* — every def owned by this registry, for disposal
   list diagnostics; // struct anvil_type_diagnostic_t* — what loading noticed, in order found
};

// Maps a `type :=` field's raw text to a recognized kind — the six native primitives
// (capitalized, per notes/native-schema.md's "Decided — native primitive set") plus the
// engine-understood `enum` kind. Anything else (including a Blob-only or unrecognized name)
// reports ANVIL_TYPE_UNKNOWN — Blob's own type identity is still an open design question.
static anvil_type_kind parse_type_kind(const char *text) {
   if (strcmp(text, "Numeric") == 0) return ANVIL_TYPE_NUMERIC;
   if (strcmp(text, "String") == 0) return ANVIL_TYPE_STRING;
   if (strcmp(text, "Bool") == 0) return ANVIL_TYPE_BOOL;
   if (strcmp(text, "Object") == 0) return ANVIL_TYPE_OBJECT;
   if (strcmp(text, "Tuple") == 0) return ANVIL_TYPE_TUPLE;
   if (strcmp(text, "Array") == 0) return ANVIL_TYPE_ARRAY;
   if (strcmp(text, "enum") == 0) return ANVIL_TYPE_ENUM;
   if (strcmp(text, "Blob") == 0) return ANVIL_TYPE_BLOB;
   return ANVIL_TYPE_UNKNOWN;
}

// Static, always-available definitions for the six native primitives + enum — never allocated,
// never disposed, no size/min/max/values of their own (a bare `Numeric` carries no baked-in
// constraint). anvil_type_resolve hands out pointers to these directly; their lifetime is the
// process's own, independent of any registry. Not const (despite never being mutated) purely to
// avoid a const-correctness mismatch against the public, non-const anvil_type_def pointer type
// — nothing outside this file can reach them except through read-only accessors anyway.
static struct anvil_type_def_t NATIVE_NUMERIC = { .name = "Numeric", .kind = ANVIL_TYPE_NUMERIC };
static struct anvil_type_def_t NATIVE_STRING  = { .name = "String",  .kind = ANVIL_TYPE_STRING };
static struct anvil_type_def_t NATIVE_BOOL    = { .name = "Bool",    .kind = ANVIL_TYPE_BOOL };
static struct anvil_type_def_t NATIVE_OBJECT  = { .name = "Object",  .kind = ANVIL_TYPE_OBJECT };
static struct anvil_type_def_t NATIVE_TUPLE   = { .name = "Tuple",   .kind = ANVIL_TYPE_TUPLE };
static struct anvil_type_def_t NATIVE_ARRAY   = { .name = "Array",   .kind = ANVIL_TYPE_ARRAY };
static struct anvil_type_def_t NATIVE_ENUM    = { .name = "enum",    .kind = ANVIL_TYPE_ENUM };
static struct anvil_type_def_t NATIVE_BLOB    = { .name = "Blob",    .kind = ANVIL_TYPE_BLOB };

// Maps a recognized native/enum kind to its static definition — the other half of
// parse_type_kind, shared so anvil_type_resolve doesn't duplicate the kind-to-instance mapping.
static struct anvil_type_def_t *native_def_for_kind(anvil_type_kind kind) {
   switch (kind) {
   case ANVIL_TYPE_NUMERIC: return &NATIVE_NUMERIC;
   case ANVIL_TYPE_STRING:  return &NATIVE_STRING;
   case ANVIL_TYPE_BOOL:    return &NATIVE_BOOL;
   case ANVIL_TYPE_OBJECT:  return &NATIVE_OBJECT;
   case ANVIL_TYPE_TUPLE:   return &NATIVE_TUPLE;
   case ANVIL_TYPE_ARRAY:   return &NATIVE_ARRAY;
   case ANVIL_TYPE_ENUM:    return &NATIVE_ENUM;
   case ANVIL_TYPE_BLOB:    return &NATIVE_BLOB;
   case ANVIL_TYPE_UNKNOWN: default: return NULL;
   }
}

// An int64 against a double, exactly - converting the int64 to double would round above 2^53.
static int compare_int_double(int64_t i, double d) {
   if (d >= 9223372036854775808.0) return -1;
   if (d < -9223372036854775808.0) return 1;
   int64_t t = (int64_t)d; // truncates toward zero; step down to the floor for negatives
   if ((double)t > d) t--;
   if (i < t) return -1;
   if (i > t) return 1;
   return d > (double)t ? -1 : 0; // i == floor(d): equal only if d has no fractional part
}

static int compare_numeric(anvil_numeric_t a, anvil_numeric_t b) {
   if (a.is_integral && b.is_integral) {
      return a.as_int64 < b.as_int64 ? -1 : a.as_int64 > b.as_int64;
   }
   if (a.is_integral) return compare_int_double(a.as_int64, b.as_double);
   if (b.is_integral) return -compare_int_double(b.as_int64, a.as_double);
   return a.as_double < b.as_double ? -1 : a.as_double > b.as_double;
}

int anvil_numeric_compare_bounds(anvil_numeric_t a, anvil_numeric_t b) {
   return compare_numeric(a, b);
}

bool anvil_numeric_read_bound(anvil_value val, anvil_numeric_t *out) {
   anvil_numeric_t n = anvil_value_get_numeric(val); // overflowed for NULL/non-numeric too
   if (n.overflowed) {
      return false;
   }
   *out = n;
   return true;
}

bool anvil_numeric_read_size(anvil_value val, long long *out) {
   anvil_numeric_t n;
   if (!anvil_numeric_read_bound(val, &n) || !n.is_integral || n.as_int64 < 0) {
      return false;
   }
   *out = n.as_int64;
   return true;
}

int anvil_numeric_compare(anvil_value data, anvil_numeric_t bound) {
   anvil_numeric_t n = anvil_value_get_numeric(data);
   if (n.overflowed) {
      char lead[2] = {0};
      anvil_value_get_text(data, lead, sizeof(lead));
      return lead[0] == '-' ? -1 : 1;
   }
   return compare_numeric(n, bound);
}

// Reads an enum type's `values := [ ... ]` array — bare, unquoted identifiers, per
// notes/native-schema.md's enum design — into a list of individually-allocated label copies.
// Returns NULL if val isn't an ARRAY (also correct for a non-enum type, which has no `values`
// field to reach this at all).
static list read_values_list(anvil_value val) {
   if (!val || anvil_value_get_type(val) != ANVIL_VALUE_ARRAY) {
      return NULL;
   }
   size_t count = anvil_value_get_count(val);
   list values = List.new(count > 0 ? count : 1, sizeof(anvil_label *));
   for (size_t i = 0; i < count; i++) {
      anvil_label *label = anvil_label_from_value(anvil_value_get_element(val, i));
      if (!label) {
         continue;
      }
      List.append(values, label); // length-carried: a label may contain NUL bytes
   }
   return values;
}

// Reads one type-definition statement's own OBJECT value in a single pass — its `type :=` kind
// plus whatever constraints it declares (`size`/`min`/`max`/`values`). Walks the statement's
// OBJECT value by index (the public flat API has no name-based nested-field lookup — see
// notes/native-schema.md) since that's the only way to reach any of these fields at all.
// What read_type_fields saw that the definition itself can't carry: reported by the caller as
// diagnostics once it knows the definition's name.
struct type_issues {
   bool type_declared; // a `type :=` field was present
   char *type_text;    // its text (allocated), for the report; NULL if absent
   bool bad_size, bad_min, bad_max, bad_values, bad_fields, bad_nullable, bad_integer, bad_tag;
   // Which identifier-like values (a name or a true/false flag) contained a NUL byte: reported as
   // malformed, never truncated into a match.
   bool nul_type, nul_format, nul_tag, nul_nullable, nul_integer;
   char *bad_format_name; // an unknown `format := name`, for the report (allocated)
};

static void read_type_fields(anvil_value obj_val, struct anvil_type_def_t *def,
                             struct type_issues *issues) {
   *issues = (struct type_issues){0};
   def->source = NULL;
   def->fields_text = NULL;
   def->fields_len = 0;
   def->nullable_declared = def->nullable = false;
   def->integer_declared = def->integer = false;
   def->tag = NULL;
   def->format = NULL;
   def->kind = ANVIL_TYPE_UNKNOWN;
   def->has_size = def->has_min = def->has_max = false;
   def->size = 0;
   def->min = def->max = (anvil_numeric_t){0};
   def->values = NULL;
   def->name = NULL;

   size_t count = anvil_value_get_count(obj_val);
   for (size_t i = 0; i < count; i++) {
      anvil_statement field = anvil_value_get_statement(obj_val, i);
      char *fname = anvil_name_alloc(field);
      anvil_value fval = anvil_statement_get_value(field);
      if (!fname) {
         continue;
      }

      if (strcmp(fname, "type") == 0) {
         bool nul = false;
         char *text = anvil_ident_alloc(fval, &nul);
         issues->type_declared = true;
         issues->nul_type = issues->nul_type || nul;
         if (text) {
            def->kind = parse_type_kind(text);
            Allocator.dispose(issues->type_text);
            issues->type_text = text; // caller disposes
         }
      } else if (strcmp(fname, "size") == 0) {
         def->has_size = anvil_numeric_read_size(fval, &def->size);
         issues->bad_size = !def->has_size;
      } else if (strcmp(fname, "min") == 0) {
         def->has_min = anvil_numeric_read_bound(fval, &def->min);
         issues->bad_min = !def->has_min;
      } else if (strcmp(fname, "max") == 0) {
         def->has_max = anvil_numeric_read_bound(fval, &def->max);
         issues->bad_max = !def->has_max;
      } else if (strcmp(fname, "values") == 0) {
         def->values = read_values_list(fval);
         issues->bad_values = (def->values == NULL);
      } else if (strcmp(fname, "format") == 0) {
         anvil_value_type ft = fval ? anvil_value_get_type(fval) : ANVIL_VALUE_NULL;
         bool nul = false;
         char *name = (ft == ANVIL_VALUE_BARE || ft == ANVIL_VALUE_STRING) ? anvil_ident_alloc(fval, &nul) : NULL;
         issues->nul_format = issues->nul_format || nul;
         if (name && anvil_format_known(name)) {
            Allocator.dispose(def->format);
            def->format = name;
         } else {
            Allocator.dispose(issues->bad_format_name);
            issues->bad_format_name = name ? name : anvil_text_dup("");
         }
      } else if (strcmp(fname, "tag") == 0) {
         anvil_value_type tt = fval ? anvil_value_get_type(fval) : ANVIL_VALUE_NULL;
         bool nul = false;
         char *tag = (tt == ANVIL_VALUE_BARE || tt == ANVIL_VALUE_STRING) ? anvil_ident_alloc(fval, &nul) : NULL;
         if (tag) {
            Allocator.dispose(def->tag);
            def->tag = tag;
         } else {
            issues->bad_tag = true;
            issues->nul_tag = issues->nul_tag || nul;
         }
      } else if (strcmp(fname, "integer") == 0) {
         bool nul = false;
         char *text = anvil_ident_alloc(fval, &nul);
         if (text && (strcmp(text, "true") == 0 || strcmp(text, "false") == 0)) {
            def->integer_declared = true;
            def->integer = (strcmp(text, "true") == 0);
         } else {
            issues->bad_integer = true;
            issues->nul_integer = issues->nul_integer || nul;
         }
         Allocator.dispose(text);
      } else if (strcmp(fname, "nullable") == 0) {
         bool nul = false;
         char *text = anvil_ident_alloc(fval, &nul);
         if (text && (strcmp(text, "true") == 0 || strcmp(text, "false") == 0)) {
            def->nullable_declared = true;
            def->nullable = (strcmp(text, "true") == 0);
         } else {
            issues->bad_nullable = true;
            issues->nul_nullable = issues->nul_nullable || nul;
         }
         Allocator.dispose(text);
      } else if (strcmp(fname, "fields") == 0) {
         if (fval && anvil_value_get_type(fval) == ANVIL_VALUE_OBJECT) {
            def->fields_text = anvil_text_alloc_n(fval, &def->fields_len); // may contain NULs
         } else {
            issues->bad_fields = true;
         }
      }
      Allocator.dispose(fname);
   }
}

// Frees a registry-owned def's enum labels (the def itself and its name are the caller's).
static void dispose_def_values(struct anvil_type_def_t *def) {
   if (!def->values) {
      return;
   }
   usize value_count = List.size(def->values);
   for (usize j = 0; j < value_count; j++) {
      char *label = NULL;
      List.get(def->values, j, (object *)&label);
      Allocator.dispose(label);
   }
   List.dispose(def->values);
   def->values = NULL;
}

// Records one diagnostic on reg. The message is formatted to its exact length - names and
// paths have no fixed bound.
static void add_diag(struct anvil_type_registry_t *reg, anvil_type_diag_code code,
                     const char *name, const char *source, const char *fmt, ...) {
   struct anvil_type_diagnostic_t *d = Allocator.alloc(sizeof(struct anvil_type_diagnostic_t));
   if (!d) {
      return;
   }
   va_list ap, ap2;
   va_start(ap, fmt);
   va_copy(ap2, ap);
   int len = vsnprintf(NULL, 0, fmt, ap);
   va_end(ap);
   d->category = code;
   d->name = anvil_text_dup(name);
   d->source = anvil_text_dup(source);
   d->message = len >= 0 ? Allocator.alloc((size_t)len + 1) : NULL;
   if (d->message) {
      vsnprintf(d->message, (size_t)len + 1, fmt, ap2);
   }
   va_end(ap2);
   if (!d->name || !d->source || !d->message) {
      Allocator.dispose(d->name);
      Allocator.dispose(d->source);
      Allocator.dispose(d->message);
      Allocator.dispose(d);
      return;
   }
   List.append(reg->diagnostics, d);
}

// Walks one document's own top-level statements into reg, registering each well-formed type
// definition — shared by anvil_type_registry_load (doc itself is the definitions file) and
// anvil_type_registry_load_from_includes (called once per @[types]-carrying direct include, so
// several documents' definitions can land in the same combined registry). Anything wrong with
// a definition is reported on reg's diagnostics; the registry itself stays best-effort.
static void collect_type_defs(anvil_document doc, struct anvil_type_registry_t *reg) {
   anvil_statement_iterator it = anvil_document_get_statements(doc);
   if (!it) {
      return;
   }
   size_t path_len = anvil_document_get_path(doc, NULL, 0);
   char *path = Allocator.alloc(path_len + 1);
   if (path) {
      anvil_document_get_path(doc, path, path_len + 1);
   }
   const char *src = path ? path : "";

   anvil_statement stmt = NULL;
   while (anvil_statement_iterator_next(it, &stmt)) {
      anvil_value val = anvil_statement_get_value(stmt);
      if (!val || anvil_value_get_type(val) != ANVIL_VALUE_OBJECT) {
         continue; // not a type-definition shape at all (a bare value) - no category fits it
      }
      struct anvil_type_def_t *def = Allocator.alloc(sizeof(struct anvil_type_def_t));
      if (!def) {
         continue;
      }
      struct type_issues issues;
      read_type_fields(val, def, &issues);
      def->name = anvil_name_alloc(stmt);
      def->source = anvil_text_dup(src);
      if (!def->name || !def->source) {
         goto discard;
      }

      if (def->kind == ANVIL_TYPE_UNKNOWN) {
         if (issues.type_declared) {
            add_diag(reg, ANVIL_TYPE_DIAG_UNKNOWN_BASE_TYPE, def->name, src,
                     "type '%s' has an unrecognized base type 'type := %s'%s", def->name,
                     issues.type_text ? issues.type_text : "", ANVIL_NUL_NOTE(issues.nul_type));
         } else {
            add_diag(reg, ANVIL_TYPE_DIAG_UNKNOWN_BASE_TYPE, def->name, src,
                     "type '%s' declares no 'type :=' base type", def->name);
         }
         goto discard; // not registered
      }

      if (issues.bad_size) {
         add_diag(reg, ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT, def->name, src,
                  "type '%s': 'size' is not a non-negative integer; treated as not declared",
                  def->name);
      }
      if (issues.bad_min) {
         add_diag(reg, ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT, def->name, src,
                  "type '%s': 'min' is not a valid number; treated as not declared", def->name);
      }
      if (issues.bad_max) {
         add_diag(reg, ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT, def->name, src,
                  "type '%s': 'max' is not a valid number; treated as not declared", def->name);
      }
      if (issues.bad_format_name) {
         add_diag(reg, ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT, def->name, src,
                  "type '%s': unknown format '%s' (known: iso8601_date, iso8601_datetime, uuid)%s; "
                  "treated as not declared", def->name, issues.bad_format_name,
                  ANVIL_NUL_NOTE(issues.nul_format));
      }
      if (issues.bad_tag) {
         add_diag(reg, ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT, def->name, src,
                  "type '%s': 'tag' must be an identifier or string%s; treated as not declared",
                  def->name, ANVIL_NUL_NOTE(issues.nul_tag));
      }
      if (issues.bad_integer) {
         add_diag(reg, ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT, def->name, src,
                  "type '%s': 'integer' must be true or false%s; treated as not declared",
                  def->name, ANVIL_NUL_NOTE(issues.nul_integer));
      }
      if (issues.bad_nullable) {
         add_diag(reg, ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT, def->name, src,
                  "type '%s': 'nullable' must be true or false%s; treated as not declared",
                  def->name, ANVIL_NUL_NOTE(issues.nul_nullable));
      }
      if (issues.bad_fields) {
         add_diag(reg, ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT, def->name, src,
                  "type '%s': 'fields' must be an object of field rules; treated as not declared",
                  def->name);
      }
      if (issues.bad_values) {
         add_diag(reg, ANVIL_TYPE_DIAG_MALFORMED_ENUM_VALUES, def->name, src,
                  "type '%s': 'values' must be an array of labels", def->name);
      }

      addr prior = 0;
      if (Map.get(reg->by_name, def->name, strlen(def->name), &prior) && prior) {
         const char *earlier = ((struct anvil_type_def_t *)prior)->source;
         add_diag(reg, ANVIL_TYPE_DIAG_NAME_COLLISION, def->name, src,
                  "type '%s' is defined in both '%s' and '%s'; the one in '%s' is used",
                  def->name, earlier ? earlier : "", src, src);
      }
      Map.set(reg->by_name, def->name, strlen(def->name), (addr)def); // last-write-wins on
                                                                       // a name collision
      List.append(reg->all_defs, def);
      Allocator.dispose(issues.type_text);
      Allocator.dispose(issues.bad_format_name);
      continue;

   discard:
      Allocator.dispose(issues.type_text);
      Allocator.dispose(issues.bad_format_name);
      Allocator.dispose(def->format);
      dispose_def_values(def);
      Allocator.dispose(def->name);
      Allocator.dispose(def->source);
      Allocator.dispose(def->fields_text);
      Allocator.dispose(def->tag);
      Allocator.dispose(def);
   }
   Allocator.dispose(path);
   anvil_statement_iterator_dispose(it);
}

static struct anvil_type_registry_t *new_empty_registry(void) {
   struct anvil_type_registry_t *reg = Allocator.alloc(sizeof(struct anvil_type_registry_t));
   if (!reg) {
      return NULL;
   }
   reg->by_name = Map.new(8);
   reg->all_defs = List.new(8, sizeof(struct anvil_type_def_t *));
   reg->diagnostics = List.new(4, sizeof(struct anvil_type_diagnostic_t *));
   return reg;
}

// A document's path as a fresh, full-length string (NULL on allocation failure).
static char *doc_path_alloc(anvil_document doc) {
   size_t len = anvil_document_get_path(doc, NULL, 0);
   char *path = Allocator.alloc(len + 1);
   if (path) {
      anvil_document_get_path(doc, path, len + 1);
   }
   return path;
}

// Takes ownership of path (kept in `paths` for the Map's key lifetime).
static void mark_visited(map visited, list paths, char *path) {
   List.append(paths, path);
   Map.set(visited, path, strlen(path), (addr)1);
}

// Depth-first, in include order, deeper documents first: each include's own includes are
// merged before its own definitions, so a definition closer to the consumer overrides a deeper
// one (and collect_type_defs reports that as a NAME_COLLISION). Every document is visited at
// most once. A document without @[types] is traversed - its includes may be types files - but
// contributes no definitions.
static void walk_includes(anvil_document doc, struct anvil_type_registry_t *reg, map visited,
                          list visited_paths) {
   anvil_document_iterator it = anvil_document_get_includes(doc);
   if (!it) {
      return;
   }
   anvil_document included = NULL;
   while (anvil_document_iterator_next(it, &included)) {
      char *path = doc_path_alloc(included);
      addr seen = 0;
      if (path && !Map.get(visited, path, strlen(path), &seen)) {
         mark_visited(visited, visited_paths, path);
         walk_includes(included, reg, visited, visited_paths);
         if (anvil_document_find_attribute(included, "types")) {
            collect_type_defs(included, reg);
         }
      } else {
         Allocator.dispose(path);
      }
      anvil_dispose(included); // caller's own responsibility - safe, shares doc's context
                               // (see anvil_document_iterator's own doc comment)
   }
   anvil_document_iterator_dispose(it);
}

anvil_type_registry anvil_type_registry_load(anvil_document doc) {
   if (!doc || !anvil_document_find_attribute(doc, "types")) {
      return NULL; // @[types] has exactly one meaning — see notes/native-schema.md
   }
   struct anvil_type_registry_t *reg = new_empty_registry();
   if (!reg) {
      return NULL;
   }
   collect_type_defs(doc, reg);
   return (anvil_type_registry)reg;
}

anvil_type_registry anvil_type_registry_load_from_includes(anvil_document doc) {
   if (!doc) {
      return NULL;
   }
   struct anvil_type_registry_t *reg = new_empty_registry();
   if (!reg) {
      return NULL;
   }

   // Visited set keyed by document path: a diamond include contributes once, and a cycle (the
   // core reports one as a load error, but the walk must not depend on that) cannot loop.
   map visited = Map.new(8);
   list visited_paths = List.new(8, sizeof(char *)); // owns the key strings
   char *root_path = doc_path_alloc(doc);
   if (root_path) {
      mark_visited(visited, visited_paths, root_path);
   }
   walk_includes(doc, reg, visited, visited_paths);
   for (usize i = 0, n = List.size(visited_paths); i < n; i++) {
      char *p = NULL;
      List.get(visited_paths, i, (object *)&p);
      Allocator.dispose(p);
   }
   List.dispose(visited_paths);
   Map.dispose(visited);

   return (anvil_type_registry)reg;
}

void anvil_type_registry_dispose(anvil_type_registry reg) {
   struct anvil_type_registry_t *r = (struct anvil_type_registry_t *)reg;
   if (!r) {
      return;
   }
   if (r->all_defs) {
      usize n = List.size(r->all_defs);
      for (usize i = 0; i < n; i++) {
         struct anvil_type_def_t *def = NULL;
         List.get(r->all_defs, i, (object *)&def);
         if (def) {
            dispose_def_values(def);
            Allocator.dispose(def->name);
            Allocator.dispose(def->source);
            Allocator.dispose(def->fields_text);
            Allocator.dispose(def->tag);
            Allocator.dispose(def->format);
         }
         Allocator.dispose(def);
      }
      List.dispose(r->all_defs);
   }
   if (r->by_name) {
      Map.dispose(r->by_name);
   }
   if (r->diagnostics) {
      usize n = List.size(r->diagnostics);
      for (usize i = 0; i < n; i++) {
         struct anvil_type_diagnostic_t *d = NULL;
         List.get(r->diagnostics, i, (object *)&d);
         if (d) {
            Allocator.dispose(d->name);
            Allocator.dispose(d->source);
            Allocator.dispose(d->message);
         }
         Allocator.dispose(d);
      }
      List.dispose(r->diagnostics);
   }
   Allocator.dispose(r);
}

anvil_type_def anvil_type_registry_find(anvil_type_registry reg, const char *name) {
   struct anvil_type_registry_t *r = (struct anvil_type_registry_t *)reg;
   if (!r || !name) {
      return NULL;
   }
   addr val = 0;
   if (!Map.get(r->by_name, name, strlen(name), &val)) {
      return NULL;
   }
   return (anvil_type_def)val;
}

anvil_type_def anvil_type_resolve(anvil_type_registry reg, const char *name) {
   if (!name) {
      return NULL;
   }
   struct anvil_type_def_t *native = native_def_for_kind(parse_type_kind(name));
   if (native) {
      return (anvil_type_def)native;
   }
   if (!reg) {
      return NULL;
   }
   return anvil_type_registry_find(reg, name);
}

anvil_type_kind anvil_type_def_get_kind(anvil_type_def def) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d) {
      return ANVIL_TYPE_UNKNOWN;
   }
   return d->kind;
}

bool anvil_type_def_get_size(anvil_type_def def, long long *out_size) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !out_size || !d->has_size) {
      return false;
   }
   *out_size = d->size;
   return true;
}

bool anvil_type_def_get_min(anvil_type_def def, long long *out_min) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !out_min || !d->has_min || !d->min.is_integral) {
      return false; // a fractional bound is never truncated to fit a long long
   }
   *out_min = d->min.as_int64;
   return true;
}

bool anvil_type_def_get_max(anvil_type_def def, long long *out_max) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !out_max || !d->has_max || !d->max.is_integral) {
      return false; // a fractional bound is never truncated to fit a long long
   }
   *out_max = d->max.as_int64;
   return true;
}

size_t anvil_type_def_get_value_count(anvil_type_def def) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !d->values) {
      return 0;
   }
   return List.size(d->values);
}

size_t anvil_type_def_get_value(anvil_type_def def, size_t index, char *buf, size_t buflen) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !d->values || index >= List.size(d->values)) {
      return 0;
   }
   anvil_label *label = NULL;
   List.get(d->values, index, (object *)&label);
   return anvil_label_copy_out(label, buf, buflen); // exact length: a label may contain NULs
}

const struct anvil_label *anvil_type_def_value_label(anvil_type_def def, size_t index) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !d->values || index >= List.size(d->values)) {
      return NULL;
   }
   anvil_label *label = NULL;
   List.get(d->values, index, (object *)&label);
   return label;
}

size_t anvil_type_def_fields_length(anvil_type_def def) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   return d && d->fields_text ? d->fields_len : 0;
}

size_t anvil_type_registry_get_diagnostic_count(anvil_type_registry reg) {
   struct anvil_type_registry_t *r = (struct anvil_type_registry_t *)reg;
   return r && r->diagnostics ? (size_t)List.size(r->diagnostics) : 0;
}

anvil_type_diagnostic anvil_type_registry_get_diagnostic(anvil_type_registry reg, size_t index) {
   struct anvil_type_registry_t *r = (struct anvil_type_registry_t *)reg;
   if (!r || !r->diagnostics || index >= (size_t)List.size(r->diagnostics)) {
      return NULL;
   }
   struct anvil_type_diagnostic_t *d = NULL;
   List.get(r->diagnostics, index, (object *)&d);
   return (anvil_type_diagnostic)d;
}

anvil_type_diag_code anvil_type_diagnostic_get_category(anvil_type_diagnostic diag) {
   return diag ? diag->category : ANVIL_TYPE_DIAG_NONE;
}

// snprintf-style: always the full length, writes at most buflen - 1 characters plus the NUL.
static size_t copy_diag_text(const char *text, char *buf, size_t buflen) {
   size_t len = strlen(text);
   if (buf && buflen > 0) {
      size_t n = len < buflen - 1 ? len : buflen - 1;
      memcpy(buf, text, n);
      buf[n] = '\0';
   }
   return len;
}

size_t anvil_type_diagnostic_get_name(anvil_type_diagnostic diag, char *buf, size_t buflen) {
   return diag ? copy_diag_text(diag->name, buf, buflen) : 0;
}

size_t anvil_type_diagnostic_get_source(anvil_type_diagnostic diag, char *buf, size_t buflen) {
   return diag ? copy_diag_text(diag->source, buf, buflen) : 0;
}

size_t anvil_type_diagnostic_get_message(anvil_type_diagnostic diag, char *buf, size_t buflen) {
   return diag ? copy_diag_text(diag->message, buf, buflen) : 0;
}

const char *anvil_type_def_fields_text(anvil_type_def def) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   return d ? d->fields_text : NULL;
}

bool anvil_type_def_nullable(anvil_type_def def, bool *out_declared) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (out_declared) {
      *out_declared = d && d->nullable_declared;
   }
   return d && d->nullable_declared && d->nullable;
}

bool anvil_type_def_get_min_numeric(anvil_type_def def, anvil_numeric_t *out_min) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !out_min || !d->has_min) {
      return false;
   }
   *out_min = d->min;
   return true;
}

bool anvil_type_def_get_max_numeric(anvil_type_def def, anvil_numeric_t *out_max) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !out_max || !d->has_max) {
      return false;
   }
   *out_max = d->max;
   return true;
}

bool anvil_type_def_integer(anvil_type_def def, bool *out_declared) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (out_declared) {
      *out_declared = d && d->integer_declared;
   }
   return d && d->integer_declared && d->integer;
}

bool anvil_numeric_is_whole(anvil_value data) {
   anvil_numeric_t n = anvil_value_get_numeric(data);
   if (n.is_integral) {
      // A digits-only literal is whole, even past int64 (overflowed): it has no fraction.
      return true;
   }
   if (n.overflowed) {
      // A decimal/exponent literal beyond double range (1e400) has no finite value to be whole, and
      // an infinity is not an integer (BR-2610-anvl-schema-004). Also what a non-number reads as.
      return false;
   }
   double d = n.as_double;
   if (d >= 9223372036854775808.0 || d <= -9223372036854775808.0) {
      return true; // at or beyond 2^63 a double has no fractional bits left
   }
   return (double)(int64_t)d == d;
}

const char *anvil_type_def_tag(anvil_type_def def) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   return d ? d->tag : NULL;
}

/* ---------------------------------------------------------------------- *
 * Lexical formats (internal/format.h): a closed set, hand-written.
 * ---------------------------------------------------------------------- */
static bool fmt_digits(const char *s, size_t n) {
   for (size_t i = 0; i < n; i++) {
      if (s[i] < '0' || s[i] > '9') {
         return false;
      }
   }
   return true;
}

static int fmt_num(const char *s, size_t n) {
   int v = 0;
   for (size_t i = 0; i < n; i++) {
      v = v * 10 + (s[i] - '0');
   }
   return v;
}

// YYYY-MM-DD at the start of s (exactly 10 characters), with a real calendar check.
static bool fmt_date10(const char *s) {
   if (!fmt_digits(s, 4) || s[4] != '-' || !fmt_digits(s + 5, 2) || s[7] != '-' ||
       !fmt_digits(s + 8, 2)) {
      return false;
   }
   int year = fmt_num(s, 4), month = fmt_num(s + 5, 2), day = fmt_num(s + 8, 2);
   if (month < 1 || month > 12 || day < 1) {
      return false;
   }
   static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
   int dim = days[month - 1];
   bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
   if (month == 2 && leap) {
      dim = 29;
   }
   return day <= dim;
}

static bool fmt_date(const char *s) {
   return strlen(s) == 10 && fmt_date10(s);
}

// HH:MM:SS[.fraction](Z|+HH:MM|-HH:MM): the offset is required.
static bool fmt_time_offset(const char *s) {
   if (!fmt_digits(s, 2) || s[2] != ':' || !fmt_digits(s + 3, 2) || s[5] != ':' ||
       !fmt_digits(s + 6, 2)) {
      return false;
   }
   if (fmt_num(s, 2) > 23 || fmt_num(s + 3, 2) > 59 || fmt_num(s + 6, 2) > 60) { // 60: leap second
      return false;
   }
   s += 8;
   if (*s == '.') {
      s++;
      size_t n = 0;
      while (s[n] >= '0' && s[n] <= '9') {
         n++;
      }
      if (n == 0) {
         return false;
      }
      s += n;
   }
   if (s[0] == 'Z') {
      return s[1] == '\0';
   }
   if ((s[0] != '+' && s[0] != '-') || !fmt_digits(s + 1, 2) || s[3] != ':' ||
       !fmt_digits(s + 4, 2) || s[6] != '\0') {
      return false;
   }
   return fmt_num(s + 1, 2) <= 23 && fmt_num(s + 4, 2) <= 59;
}

static bool fmt_datetime(const char *s) {
   return strlen(s) > 11 && fmt_date10(s) && s[10] == 'T' && fmt_time_offset(s + 11);
}

static bool fmt_uuid(const char *s) {
   if (strlen(s) != 36) {
      return false;
   }
   for (size_t i = 0; i < 36; i++) {
      if (i == 8 || i == 13 || i == 18 || i == 23) {
         if (s[i] != '-') {
            return false;
         }
      } else if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f') ||
                   (s[i] >= 'A' && s[i] <= 'F'))) {
         return false;
      }
   }
   return true;
}

bool anvil_format_known(const char *name) {
   return name && (strcmp(name, "iso8601_date") == 0 || strcmp(name, "iso8601_datetime") == 0 ||
                   strcmp(name, "uuid") == 0);
}

bool anvil_format_valid(const char *name, const char *text) {
   if (!name || !text) {
      return false;
   }
   if (strcmp(name, "iso8601_date") == 0) return fmt_date(text);
   if (strcmp(name, "iso8601_datetime") == 0) return fmt_datetime(text);
   if (strcmp(name, "uuid") == 0) return fmt_uuid(text);
   return false;
}

size_t anvil_type_def_get_format(anvil_type_def def, char *buf, size_t buflen) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   if (!d || !d->format) {
      return 0;
   }
   return copy_diag_text(d->format, buf, buflen);
}

const char *anvil_type_def_format(anvil_type_def def) {
   struct anvil_type_def_t *d = (struct anvil_type_def_t *)def;
   return d ? d->format : NULL;
}
