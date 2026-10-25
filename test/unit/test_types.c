/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_types.c - Unit tests for Anvil's opt-in type registry (types.c)   *
 * ---------------------------------------------------------------------- *
 * First slice: load a @[types]-attributed document, find a definition by *
 * name, read its kind. See notes/native-schema.md.                       *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_type_registry.h"
#include "anvil_types.h"
#include "internal/numeric_bound.h"
#include "internal/source_registry.h"
#include "testbit.h"
// ----------------
#include "../utilities/helpers.h"
#include <string.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

/* ---------------------------------------------------------------------- *
 * TYP01 — a @[types] document's own definitions are loadable and
 * findable by name, each reporting the right kind
 * ---------------------------------------------------------------------- */
static void test_typ01_load_and_find(void) {
   anvil_document doc = anvil_load(fixture_path("types_basic.anvl"));
   TestBit.is_not_null(doc, "TYP01: document loaded");
   if (!doc) {
      return;
   }

   anvil_type_registry reg = anvil_type_registry_load(doc);
   TestBit.is_not_null(reg, "TYP01: registry loaded from a @[types] document");
   if (reg) {
      anvil_type_def vin = anvil_type_registry_find(reg, "VIN");
      TestBit.is_not_null(vin, "TYP01: 'VIN' found");
      if (vin) {
         TestBit.is_equal_int(ANVIL_TYPE_STRING, anvil_type_def_get_kind(vin),
                              "TYP01: 'VIN' is kind String");
      }

      anvil_type_def status = anvil_type_registry_find(reg, "AssetStatus");
      TestBit.is_not_null(status, "TYP01: 'AssetStatus' found");
      if (status) {
         TestBit.is_equal_int(ANVIL_TYPE_ENUM, anvil_type_def_get_kind(status),
                              "TYP01: 'AssetStatus' is kind Enum");
      }

      TestBit.is_null(anvil_type_registry_find(reg, "Nonexistent"),
                      "TYP01: a nonexistent name is not found");

      anvil_type_registry_dispose(reg);
   }
   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * TYP02 — a document without @[types] is rejected, not silently treated
 * as an empty registry
 * ---------------------------------------------------------------------- */
static void test_typ02_reject_non_types_document(void) {
   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "TYP02: document loaded");
   if (doc) {
      TestBit.is_null(anvil_type_registry_load(doc),
                      "TYP02: a document without @[types] is rejected");
      anvil_dispose(doc);
   }
}
/* ---------------------------------------------------------------------- *
 * TYP03 — NULL safety throughout, and dispose(NULL) is a no-op
 * ---------------------------------------------------------------------- */
static void test_typ03_null_safety(void) {
   TestBit.is_null(anvil_type_registry_load(NULL), "TYP03: load(NULL) is NULL");
   anvil_type_registry_dispose(NULL); // must not crash

   anvil_document doc = anvil_load(fixture_path("types_basic.anvl"));
   TestBit.is_not_null(doc, "TYP03: document loaded");
   if (doc) {
      anvil_type_registry reg = anvil_type_registry_load(doc);
      TestBit.is_not_null(reg, "TYP03: registry loaded");
      if (reg) {
         TestBit.is_null(anvil_type_registry_find(NULL, "VIN"),
                         "TYP03: find(NULL, ...) is NULL");
         TestBit.is_null(anvil_type_registry_find(reg, NULL),
                         "TYP03: find(reg, NULL) is NULL");
         anvil_type_registry_dispose(reg);
      }
      anvil_dispose(doc);
   }

   TestBit.is_equal_int(ANVIL_TYPE_UNKNOWN, anvil_type_def_get_kind(NULL),
                        "TYP03: get_kind(NULL) is ANVIL_TYPE_UNKNOWN");
}
/* ---------------------------------------------------------------------- *
 * TYP04 — constraint fields (size/min/max/values) are readable off a
 * type definition
 * ---------------------------------------------------------------------- */
static void test_typ04_constraints(void) {
   anvil_document doc = anvil_load(fixture_path("types_basic.anvl"));
   TestBit.is_not_null(doc, "TYP04: document loaded");
   if (!doc) {
      return;
   }

   anvil_type_registry reg = anvil_type_registry_load(doc);
   TestBit.is_not_null(reg, "TYP04: registry loaded");
   if (reg) {
      anvil_type_def vin = anvil_type_registry_find(reg, "VIN");
      TestBit.is_not_null(vin, "TYP04: 'VIN' found");
      if (vin) {
         long long size = 0;
         TestBit.is_true(anvil_type_def_get_size(vin, &size), "TYP04: 'VIN' declares a size");
         TestBit.is_equal_int(17, size, "TYP04: 'VIN' size is 17");

         long long unused = 0;
         TestBit.is_false(anvil_type_def_get_min(vin, &unused), "TYP04: 'VIN' has no min");
         TestBit.is_false(anvil_type_def_get_max(vin, &unused), "TYP04: 'VIN' has no max");
      }

      anvil_type_def year = anvil_type_registry_find(reg, "AssetYear");
      TestBit.is_not_null(year, "TYP04: 'AssetYear' found");
      if (year) {
         long long min = 0, max = 0;
         TestBit.is_true(anvil_type_def_get_min(year, &min), "TYP04: 'AssetYear' declares a min");
         TestBit.is_equal_int(1900, min, "TYP04: 'AssetYear' min is 1900");
         TestBit.is_true(anvil_type_def_get_max(year, &max), "TYP04: 'AssetYear' declares a max");
         TestBit.is_equal_int(2100, max, "TYP04: 'AssetYear' max is 2100");

         long long unused = 0;
         TestBit.is_false(anvil_type_def_get_size(year, &unused), "TYP04: 'AssetYear' has no size");
      }

      anvil_type_def status = anvil_type_registry_find(reg, "AssetStatus");
      TestBit.is_not_null(status, "TYP04: 'AssetStatus' found");
      if (status) {
         TestBit.is_equal_int(3, (long long)anvil_type_def_get_value_count(status),
                              "TYP04: 'AssetStatus' has three enum values");

         char label[32] = {0};
         anvil_type_def_get_value(status, 0, label, sizeof(label));
         TestBit.is_true(strcmp(label, "active") == 0, "TYP04: value 0 is 'active'");

         memset(label, 0, sizeof(label));
         anvil_type_def_get_value(status, 1, label, sizeof(label));
         TestBit.is_true(strcmp(label, "maintenance") == 0, "TYP04: value 1 is 'maintenance'");

         memset(label, 0, sizeof(label));
         anvil_type_def_get_value(status, 2, label, sizeof(label));
         TestBit.is_true(strcmp(label, "out_of_service") == 0, "TYP04: value 2 is 'out_of_service'");

         TestBit.is_equal_int(0, (long long)anvil_type_def_get_value(status, 3, label, sizeof(label)),
                              "TYP04: an out-of-bounds value index returns 0");
      }

      anvil_type_registry_dispose(reg);
   }
   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * TYP05 — constraint-accessor NULL safety
 * ---------------------------------------------------------------------- */
static void test_typ05_constraint_null_safety(void) {
   long long unused = 0;
   TestBit.is_false(anvil_type_def_get_size(NULL, &unused), "TYP05: get_size(NULL, ...) is false");
   TestBit.is_false(anvil_type_def_get_min(NULL, &unused), "TYP05: get_min(NULL, ...) is false");
   TestBit.is_false(anvil_type_def_get_max(NULL, &unused), "TYP05: get_max(NULL, ...) is false");
   TestBit.is_equal_int(0, (long long)anvil_type_def_get_value_count(NULL),
                        "TYP05: get_value_count(NULL) is 0");
   TestBit.is_equal_int(0, (long long)anvil_type_def_get_value(NULL, 0, NULL, 0),
                        "TYP05: get_value(NULL, ...) is 0");
}
/* ---------------------------------------------------------------------- *
 * TYP06 — types.X resolution: a document that never carries @[types]
 * itself resolves types from its own direct includes
 * ---------------------------------------------------------------------- */
static void test_typ06_load_from_includes(void) {
   anvil_document doc = anvil_load(fixture_path("types_consumer.anvl"));
   TestBit.is_not_null(doc, "TYP06: consumer document loaded");
   if (!doc) {
      return;
   }

   anvil_type_registry reg = anvil_type_registry_load_from_includes(doc);
   TestBit.is_not_null(reg, "TYP06: registry built from includes");
   if (reg) {
      anvil_type_def vin = anvil_type_registry_find(reg, "VIN");
      TestBit.is_not_null(vin, "TYP06: 'VIN' (from the includeed types file) is resolvable");
      if (vin) {
         TestBit.is_equal_int(ANVIL_TYPE_STRING, anvil_type_def_get_kind(vin),
                              "TYP06: 'VIN' is kind String");
      }

      anvil_type_def status = anvil_type_registry_find(reg, "AssetStatus");
      TestBit.is_not_null(status, "TYP06: 'AssetStatus' is also resolvable");

      anvil_type_registry_dispose(reg);
   }

   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * TYP07 — load_from_includes on a document with no @[types] includes at
 * all still yields a real, empty registry, not NULL
 * ---------------------------------------------------------------------- */
static void test_typ07_load_from_includes_empty(void) {
   TestBit.is_null(anvil_type_registry_load_from_includes(NULL),
                   "TYP07: load_from_includes(NULL) is NULL");

   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl")); // no includes at all
   TestBit.is_not_null(doc, "TYP07: document with no includes loaded");
   if (doc) {
      anvil_type_registry reg = anvil_type_registry_load_from_includes(doc);
      TestBit.is_not_null(reg, "TYP07: a document with no includes still yields a real registry");
      if (reg) {
         TestBit.is_null(anvil_type_registry_find(reg, "VIN"),
                         "TYP07: nothing is resolvable from an empty registry");
         anvil_type_registry_dispose(reg);
      }
      anvil_dispose(doc);
   }
}
/* ---------------------------------------------------------------------- *
 * TYP08 — anvil_type_resolve resolves the native/enum vocabulary with no
 * registry at all, and reports no constraints of their own
 * ---------------------------------------------------------------------- */
static void test_typ08_resolve_native(void) {
   anvil_type_def numeric = anvil_type_resolve(NULL, "Numeric");
   TestBit.is_not_null(numeric, "TYP08: 'Numeric' resolves with no registry");
   if (numeric) {
      TestBit.is_equal_int(ANVIL_TYPE_NUMERIC, anvil_type_def_get_kind(numeric),
                           "TYP08: 'Numeric' resolves to kind Numeric");
      long long unused = 0;
      TestBit.is_false(anvil_type_def_get_size(numeric, &unused),
                       "TYP08: native 'Numeric' declares no size of its own");
   }

   anvil_type_def str = anvil_type_resolve(NULL, "String");
   TestBit.is_not_null(str, "TYP08: 'String' resolves");
   if (str) {
      TestBit.is_equal_int(ANVIL_TYPE_STRING, anvil_type_def_get_kind(str),
                           "TYP08: 'String' resolves to kind String");
   }

   anvil_type_def en = anvil_type_resolve(NULL, "enum");
   TestBit.is_not_null(en, "TYP08: bare 'enum' resolves");
   if (en) {
      TestBit.is_equal_int(ANVIL_TYPE_ENUM, anvil_type_def_get_kind(en),
                           "TYP08: 'enum' resolves to kind Enum");
      TestBit.is_equal_int(0, (long long)anvil_type_def_get_value_count(en),
                           "TYP08: native 'enum' declares no values of its own");
   }

   TestBit.is_null(anvil_type_resolve(NULL, "NotARealType"),
                   "TYP08: an unrecognized name with no registry is NULL");
}
/* ---------------------------------------------------------------------- *
 * TYP09 — anvil_type_resolve falls through to a registry for a custom
 * type, and native names are checked first regardless
 * ---------------------------------------------------------------------- */
static void test_typ09_resolve_custom_falls_through(void) {
   anvil_document doc = anvil_load(fixture_path("types_basic.anvl"));
   TestBit.is_not_null(doc, "TYP09: document loaded");
   if (!doc) {
      return;
   }
   anvil_type_registry reg = anvil_type_registry_load(doc);
   TestBit.is_not_null(reg, "TYP09: registry loaded");
   if (reg) {
      anvil_type_def vin = anvil_type_resolve(reg, "VIN");
      TestBit.is_not_null(vin, "TYP09: 'VIN' resolves through the registry");
      if (vin) {
         TestBit.is_equal_int(ANVIL_TYPE_STRING, anvil_type_def_get_kind(vin),
                              "TYP09: 'VIN' resolves to kind String");
         long long size = 0;
         TestBit.is_true(anvil_type_def_get_size(vin, &size), "TYP09: 'VIN' declares a size");
         TestBit.is_equal_int(17, size, "TYP09: 'VIN' size is 17");
      }

      anvil_type_def numeric = anvil_type_resolve(reg, "Numeric");
      TestBit.is_not_null(numeric, "TYP09: native names still resolve with a registry present");
      TestBit.is_equal_int(ANVIL_TYPE_NUMERIC, anvil_type_def_get_kind(numeric),
                           "TYP09: 'Numeric' still resolves to kind Numeric, not a registry miss");

      TestBit.is_null(anvil_type_resolve(reg, "NotRegistered"),
                      "TYP09: a name that's neither native nor registered is NULL");

      anvil_type_registry_dispose(reg);
   }
   anvil_dispose(doc);
}
/* ---------------------------------------------------------------------- *
 * TYP10 — anvil_type_resolve NULL safety
 * ---------------------------------------------------------------------- */
static void test_typ10_resolve_null_safety(void) {
   TestBit.is_null(anvil_type_resolve(NULL, NULL), "TYP10: resolve(NULL, NULL) is NULL");

   anvil_document doc = anvil_load(fixture_path("types_basic.anvl"));
   if (doc) {
      anvil_type_registry reg = anvil_type_registry_load(doc);
      if (reg) {
         TestBit.is_null(anvil_type_resolve(reg, NULL), "TYP10: resolve(reg, NULL) is NULL");
         anvil_type_registry_dispose(reg);
      }
      anvil_dispose(doc);
   }
}


/* ---------------------------------------------------------------------- *
 * TYP11 (BR-2610-anvl-types-001) — numeric constraints are never truncated:
 * a fractional bound is not reported through the long long getters, a
 * malformed one is "not declared"
 * ---------------------------------------------------------------------- */
static void test_typ11_numeric_constraints_not_truncated(void) {
   const char *src =
      "#!aml\n@[types]\n"
      "Half := { type := Numeric; min := 0.5; max := 10; };\n"
      "Exp := { type := Numeric; max := 1e+3; };\n"
      "Bad := { type := Numeric; min := \"abc\"; max := 1e+999; size := 2.5; };\n";
   anvil_document doc = anvil_load_buffer(src, strlen(src));
   TestBit.is_not_null(doc, "TYP11: document loaded");
   anvil_type_registry reg = doc ? anvil_type_registry_load(doc) : NULL;
   TestBit.is_not_null(reg, "TYP11: registry loaded");
   if (reg) {
      long long v = -1;
      anvil_type_def half = anvil_type_registry_find(reg, "Half");
      TestBit.is_not_null(half, "TYP11: 'Half' found");
      if (half) {
         TestBit.is_false(anvil_type_def_get_min(half, &v),
                          "TYP11: fractional min is not reported as a truncated integer");
         TestBit.is_true(anvil_type_def_get_max(half, &v), "TYP11: integral max is reported");
         TestBit.is_equal_int(10, v, "TYP11: max is 10");
      }
      anvil_type_def ex = anvil_type_registry_find(reg, "Exp");
      if (ex) {
         TestBit.is_false(anvil_type_def_get_max(ex, &v),
                          "TYP11: exponent-form max is not reported as 1");
      }
      anvil_type_def bad = anvil_type_registry_find(reg, "Bad");
      TestBit.is_not_null(bad, "TYP11: 'Bad' found");
      if (bad) {
         TestBit.is_false(anvil_type_def_get_min(bad, &v), "TYP11: non-numeric min not declared");
         TestBit.is_false(anvil_type_def_get_max(bad, &v), "TYP11: overflowing max not declared");
         TestBit.is_false(anvil_type_def_get_size(bad, &v), "TYP11: fractional size not declared");
      }
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}


/* ---------------------------------------------------------------------- *
 * TYP12 (BR-2610-anvl-schema-001) — type names are stored and looked up in
 * full: two 80-char names sharing their first 63 chars stay distinct
 * ---------------------------------------------------------------------- */
static void test_typ12_long_type_names(void) {
   char a[81], b[81], prefix[64], src[512];
   memset(a, 'x', 63); memset(a + 63, 'A', 17); a[80] = '\0';
   memset(b, 'x', 63); memset(b + 63, 'B', 17); b[80] = '\0';
   memcpy(prefix, a, 63); prefix[63] = '\0';
   snprintf(src, sizeof(src),
            "#!aml\n@[types]\n%s := { type := String; size := 3; };\n%s := { type := Numeric; };\n",
            b, a);
   anvil_document doc = anvil_load_buffer(src, strlen(src));
   anvil_type_registry reg = doc ? anvil_type_registry_load(doc) : NULL;
   TestBit.is_not_null(reg, "TYP12: registry loaded");
   if (reg) {
      anvil_type_def da = anvil_type_registry_find(reg, a);
      anvil_type_def db = anvil_type_registry_find(reg, b);
      TestBit.is_not_null(da, "TYP12: first 80-char name found");
      TestBit.is_not_null(db, "TYP12: second 80-char name found");
      if (da && db) {
         TestBit.is_equal_int(ANVIL_TYPE_NUMERIC, anvil_type_def_get_kind(da),
                              "TYP12: first name keeps its own kind");
         TestBit.is_equal_int(ANVIL_TYPE_STRING, anvil_type_def_get_kind(db),
                              "TYP12: second name is not overwritten by the first");
      }
      TestBit.is_true(anvil_type_registry_find(reg, prefix) == NULL,
                      "TYP12: the shared 63-char prefix is not a type");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-types-001 — registry diagnostics
 * ---------------------------------------------------------------------- */

// Loads a one-document registry from buffer source.
static anvil_type_registry diag_registry(const char *src, anvil_document *out_doc) {
   *out_doc = anvil_load_buffer(src, strlen(src));
   return *out_doc ? anvil_type_registry_load(*out_doc) : NULL;
}

static size_t diag_text(size_t (*getter)(anvil_type_diagnostic, char *, size_t),
                        anvil_type_diagnostic d, char *out, size_t cap) {
   out[0] = '\0';
   return getter(d, out, cap);
}

static void test_typ13_name_collision_reported(void) {
   anvil_document consumer = anvil_load(fixture_path("types_dup_consumer.anvl"));
   TestBit.is_not_null(consumer, "TYP13: consumer loaded");
   anvil_type_registry reg = consumer ? anvil_type_registry_load_from_includes(consumer) : NULL;
   TestBit.is_not_null(reg, "TYP13: registry loaded");
   if (reg) {
      TestBit.is_equal_int(1, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP13: exactly one diagnostic");
      anvil_type_diagnostic d = anvil_type_registry_get_diagnostic(reg, 0);
      TestBit.is_not_null(d, "TYP13: diagnostic readable");
      if (d) {
         char out[512];
         TestBit.is_equal_int(ANVIL_TYPE_DIAG_NAME_COLLISION, anvil_type_diagnostic_get_category(d),
                              "TYP13: category is NAME_COLLISION");
         diag_text(anvil_type_diagnostic_get_name, d, out, sizeof(out));
         TestBit.is_true(strcmp(out, "VIN") == 0, "TYP13: names VIN");
         diag_text(anvil_type_diagnostic_get_message, d, out, sizeof(out));
         TestBit.is_true(strstr(out, "types_dup_a.anvl") && strstr(out, "types_dup_b.anvl"),
                         "TYP13: message names both include paths");
         diag_text(anvil_type_diagnostic_get_source, d, out, sizeof(out));
         TestBit.is_true(strstr(out, "types_dup_b.anvl") != NULL,
                         "TYP13: source is the later document");
      }
      // last-write-wins stays the behavior: the later include's VIN (size 20) is the one found.
      long long size = 0;
      anvil_type_def vin = anvil_type_resolve(reg, "VIN");
      TestBit.is_true(vin && anvil_type_def_get_size(vin, &size) && size == 20,
                      "TYP13: resolve returns the later include's definition");
      // Plate (only in the first include) is unaffected.
      TestBit.is_not_null(anvil_type_resolve(reg, "Plate"), "TYP13: non-colliding type resolves");
      anvil_type_registry_dispose(reg);
   }
   if (consumer) {
      anvil_dispose(consumer);
   }
}

static void test_typ14_unknown_base_type(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = diag_registry(
      "#!aml\n@[types]\nFoo := { type := Strng; };\nBar := { size := 3; };\n", &doc);
   TestBit.is_not_null(reg, "TYP14: registry loaded");
   if (reg) {
      TestBit.is_equal_int(2, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP14: one diagnostic per bad definition");
      char out[128];
      anvil_type_diagnostic d = anvil_type_registry_get_diagnostic(reg, 0);
      TestBit.is_equal_int(ANVIL_TYPE_DIAG_UNKNOWN_BASE_TYPE, anvil_type_diagnostic_get_category(d),
                           "TYP14: misspelled base is UNKNOWN_BASE_TYPE");
      diag_text(anvil_type_diagnostic_get_name, d, out, sizeof(out));
      TestBit.is_true(strcmp(out, "Foo") == 0, "TYP14: names Foo");
      d = anvil_type_registry_get_diagnostic(reg, 1);
      TestBit.is_equal_int(ANVIL_TYPE_DIAG_UNKNOWN_BASE_TYPE, anvil_type_diagnostic_get_category(d),
                           "TYP14: a missing `type :=` is UNKNOWN_BASE_TYPE too");
      diag_text(anvil_type_diagnostic_get_name, d, out, sizeof(out));
      TestBit.is_true(strcmp(out, "Bar") == 0, "TYP14: names Bar");
      TestBit.is_true(anvil_type_registry_find(reg, "Foo") == NULL,
                      "TYP14: an unknown-base definition is not registered");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_typ15_malformed_constraint(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = diag_registry(
      "#!aml\n@[types]\nFoo := { type := String; size := abc; };\n", &doc);
   TestBit.is_not_null(reg, "TYP15: registry loaded");
   if (reg) {
      TestBit.is_equal_int(1, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP15: one diagnostic");
      anvil_type_diagnostic d = anvil_type_registry_get_diagnostic(reg, 0);
      TestBit.is_equal_int(ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT,
                           anvil_type_diagnostic_get_category(d),
                           "TYP15: category is MALFORMED_CONSTRAINT");
      char out[128];
      diag_text(anvil_type_diagnostic_get_name, d, out, sizeof(out));
      TestBit.is_true(strcmp(out, "Foo") == 0, "TYP15: names Foo");
      diag_text(anvil_type_diagnostic_get_message, d, out, sizeof(out));
      TestBit.is_true(strstr(out, "size") != NULL, "TYP15: message names the constraint");
      long long size = -1;
      anvil_type_def foo = anvil_type_registry_find(reg, "Foo");
      TestBit.is_not_null(foo, "TYP15: the definition itself is still registered");
      TestBit.is_false(foo && anvil_type_def_get_size(foo, &size),
                       "TYP15: the malformed size is not a declared 0");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_typ16_malformed_constraint_variants(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = diag_registry(
      "#!aml\n@[types]\n"
      "A := { type := Numeric; min := \"x\"; max := 1e+999; };\n"
      "B := { type := String; size := 2.5; };\n", &doc);
   if (reg) {
      TestBit.is_equal_int(3, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP16: min, max and fractional size each reported once");
      for (size_t i = 0; i < anvil_type_registry_get_diagnostic_count(reg); i++) {
         TestBit.is_equal_int(ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT,
                              anvil_type_diagnostic_get_category(
                                 anvil_type_registry_get_diagnostic(reg, i)),
                              "TYP16: each is MALFORMED_CONSTRAINT");
      }
      anvil_type_registry_dispose(reg);
   } else {
      TestBit.is_not_null(reg, "TYP16: registry loaded");
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_typ17_malformed_enum_values(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = diag_registry(
      "#!aml\n@[types]\nE := { type := enum; values := red; };\n", &doc);
   TestBit.is_not_null(reg, "TYP17: registry loaded");
   if (reg) {
      TestBit.is_equal_int(1, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP17: one diagnostic");
      anvil_type_diagnostic d = anvil_type_registry_get_diagnostic(reg, 0);
      TestBit.is_equal_int(ANVIL_TYPE_DIAG_MALFORMED_ENUM_VALUES,
                           anvil_type_diagnostic_get_category(d),
                           "TYP17: category is MALFORMED_ENUM_VALUES");
      char out[128];
      diag_text(anvil_type_diagnostic_get_name, d, out, sizeof(out));
      TestBit.is_true(strcmp(out, "E") == 0, "TYP17: names E");
      anvil_type_def e = anvil_type_registry_find(reg, "E");
      TestBit.is_equal_int(0, (long long)(e ? anvil_type_def_get_value_count(e) : 99),
                           "TYP17: no labels are declared");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_typ18_clean_registry_has_no_diagnostics(void) {
   anvil_document doc = anvil_load(fixture_path("types_basic.anvl"));
   anvil_type_registry reg = doc ? anvil_type_registry_load(doc) : NULL;
   TestBit.is_not_null(reg, "TYP18: registry loaded");
   if (reg) {
      TestBit.is_equal_int(0, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP18: a clean registry reports 0 diagnostics");
      TestBit.is_true(anvil_type_registry_get_diagnostic(reg, 0) == NULL,
                      "TYP18: get_diagnostic is NULL out of bounds");
      anvil_type_registry_dispose(reg);
   }
   anvil_document consumer = anvil_load(fixture_path("types_clean_consumer.anvl"));
   anvil_type_registry reg2 = consumer ? anvil_type_registry_load_from_includes(consumer) : NULL;
   TestBit.is_not_null(reg2, "TYP18: include registry loaded");
   if (reg2) {
      TestBit.is_equal_int(0, (long long)anvil_type_registry_get_diagnostic_count(reg2),
                           "TYP18: clean includes report 0 diagnostics");
      anvil_type_registry_dispose(reg2);
   }
   if (consumer) {
      anvil_dispose(consumer);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_typ19_diagnostics_survive_document_disposal(void) {
   anvil_document consumer = anvil_load(fixture_path("types_dup_consumer.anvl"));
   anvil_type_registry reg = consumer ? anvil_type_registry_load_from_includes(consumer) : NULL;
   TestBit.is_not_null(reg, "TYP19: registry loaded");
   if (consumer) {
      anvil_dispose(consumer);
   }
   if (reg) {
      anvil_type_diagnostic d = anvil_type_registry_get_diagnostic(reg, 0);
      TestBit.is_not_null(d, "TYP19: diagnostic still present after the documents are disposed");
      if (d) {
         char out[512];
         diag_text(anvil_type_diagnostic_get_message, d, out, sizeof(out));
         TestBit.is_true(strstr(out, "types_dup_a.anvl") && strstr(out, "types_dup_b.anvl"),
                         "TYP19: message still names both paths");
         TestBit.is_true(anvil_type_diagnostic_get_message(d, NULL, 0) == strlen(out),
                         "TYP19: length query matches the full text");
      }
      anvil_type_registry_dispose(reg);
   }
}

static void test_typ20_diagnostic_null_safety(void) {
   char out[8] = "x";
   TestBit.is_equal_int(0, (long long)anvil_type_registry_get_diagnostic_count(NULL),
                        "TYP20: count(NULL) is 0");
   TestBit.is_true(anvil_type_registry_get_diagnostic(NULL, 0) == NULL, "TYP20: get(NULL) is NULL");
   TestBit.is_equal_int(ANVIL_TYPE_DIAG_NONE, anvil_type_diagnostic_get_category(NULL),
                        "TYP20: category(NULL) is NONE");
   TestBit.is_equal_int(0, (long long)anvil_type_diagnostic_get_name(NULL, out, sizeof(out)),
                        "TYP20: name(NULL) is 0");
   TestBit.is_equal_int(0, (long long)anvil_type_diagnostic_get_source(NULL, out, sizeof(out)),
                        "TYP20: source(NULL) is 0");
   TestBit.is_equal_int(0, (long long)anvil_type_diagnostic_get_message(NULL, out, sizeof(out)),
                        "TYP20: message(NULL) is 0");
}


static void test_typ21_document_get_path(void) {
   anvil_document doc = anvil_load(fixture_path("types_basic.anvl"));
   TestBit.is_not_null(doc, "TYP21: document loaded");
   if (doc) {
      size_t len = anvil_document_get_path(doc, NULL, 0);
      char out[512] = {0};
      TestBit.is_true(len > 0 && len < sizeof(out), "TYP21: path length is reported");
      TestBit.is_equal_int((long long)len, (long long)anvil_document_get_path(doc, out, sizeof(out)),
                           "TYP21: copy returns the same length");
      TestBit.is_true(strstr(out, "types_basic.anvl") != NULL, "TYP21: path names the file");
      char tiny[4];
      anvil_document_get_path(doc, tiny, sizeof(tiny));
      TestBit.is_true(strlen(tiny) == 3, "TYP21: a short buffer is truncated and terminated");
      anvil_dispose(doc);
   }
   TestBit.is_equal_int(0, (long long)anvil_document_get_path(NULL, NULL, 0),
                        "TYP21: NULL document returns 0");
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-types-002 — @[types] resolution follows nested includes
 * ---------------------------------------------------------------------- */
static anvil_type_registry include_registry(const char *fixture, anvil_document *out_doc) {
   *out_doc = anvil_load(fixture_path(fixture));
   return *out_doc ? anvil_type_registry_load_from_includes(*out_doc) : NULL;
}

static void test_typ22_transitive_include(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = include_registry("trans_consumer.anvl", &doc);
   TestBit.is_not_null(reg, "TYP22: registry loaded");
   if (reg) {
      TestBit.is_not_null(anvil_type_resolve(reg, "TicketId"), "TYP22: direct include's type resolves");
      anvil_type_def id = anvil_type_resolve(reg, "Id");
      TestBit.is_not_null(id, "TYP22: a type two includes deep resolves");
      long long min = 0;
      TestBit.is_true(id && anvil_type_def_get_min(id, &min) && min == 1,
                      "TYP22: it is the deep definition, constraints intact");
      TestBit.is_equal_int(0, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP22: no diagnostics");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

// The core treats an include cycle as a load error (ANVL_ERR_INCLUDE_CYCLIC) and never parses the
// bodies of a cyclic graph, so no definition inside one is reachable; what the walk must guarantee
// is that it terminates, doesn't crash, and invents no collision. (FR-2610-anvl-types-002 assumed
// the core resolved cycles - see the ticket's Resolution.)
static void test_typ23_include_cycle_terminates(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = include_registry("cyc_consumer.anvl", &doc);
   TestBit.is_not_null(doc, "TYP23: cyclic include graph loaded");
   TestBit.is_not_null(reg, "TYP23: registry loaded (the walk terminated)");
   if (reg) {
      TestBit.is_equal_int(0, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP23: a cycle produces no self-collision");
      TestBit.is_true(anvil_type_registry_find(reg, "Alpha") == NULL,
                      "TYP23: nothing from an unparsed cyclic graph is registered");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_typ24_non_types_document_is_traversed(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = include_registry("trans_via_mid_consumer.anvl", &doc);
   TestBit.is_not_null(reg, "TYP24: registry loaded");
   if (reg) {
      TestBit.is_not_null(anvil_type_resolve(reg, "Id"),
                          "TYP24: Id resolves through a non-@[types] document");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_typ25_closer_definition_overrides_deeper(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = include_registry("ovr_consumer.anvl", &doc);
   TestBit.is_not_null(reg, "TYP25: registry loaded");
   if (reg) {
      long long max = 0;
      anvil_type_def id = anvil_type_resolve(reg, "Id");
      TestBit.is_true(id && anvil_type_def_get_max(id, &max) && max == 5,
                      "TYP25: the including document's Id (max 5) wins over the deeper one");
      TestBit.is_equal_int(1, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP25: the override is reported once");
      anvil_type_diagnostic d = anvil_type_registry_get_diagnostic(reg, 0);
      TestBit.is_equal_int(ANVIL_TYPE_DIAG_NAME_COLLISION, anvil_type_diagnostic_get_category(d),
                           "TYP25: as a NAME_COLLISION");
      char out[512] = {0};
      anvil_type_diagnostic_get_message(d, out, sizeof(out));
      TestBit.is_true(strstr(out, "ovr_a.types.anvl") && strstr(out, "ovr_b.types.anvl"),
                      "TYP25: naming both documents");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_typ26_shared_include_visited_once(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = include_registry("dia_consumer.anvl", &doc);
   TestBit.is_not_null(reg, "TYP26: registry loaded");
   if (reg) {
      TestBit.is_not_null(anvil_type_resolve(reg, "Shared"), "TYP26: the shared type resolves");
      TestBit.is_not_null(anvil_type_resolve(reg, "X"), "TYP26: X resolves");
      TestBit.is_not_null(anvil_type_resolve(reg, "Y"), "TYP26: Y resolves");
      TestBit.is_equal_int(0, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP26: a diamond include is not a collision with itself");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}


static void test_typ27_object_type_fields(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = diag_registry(
      "#!aml\n@[types]\n"
      "Good := { type := Object; fields := { id := { type := Numeric; }; }; };\n"
      "Plain := { type := Object; };\n"
      "Bad := { type := Object; fields := 5; };\n", &doc);
   TestBit.is_not_null(reg, "TYP27: registry loaded");
   if (reg) {
      const char *text = anvil_type_def_fields_text(anvil_type_registry_find(reg, "Good"));
      TestBit.is_true(text != NULL && strstr(text, "id") != NULL,
                      "TYP27: a type's `fields` source text is kept");
      TestBit.is_true(anvil_type_def_fields_text(anvil_type_registry_find(reg, "Plain")) == NULL,
                      "TYP27: a type without `fields` has none");
      TestBit.is_true(anvil_type_def_fields_text(NULL) == NULL, "TYP27: NULL def has none");
      TestBit.is_equal_int(1, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP27: only the malformed `fields` is reported");
      anvil_type_diagnostic d = anvil_type_registry_get_diagnostic(reg, 0);
      TestBit.is_equal_int(ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT, anvil_type_diagnostic_get_category(d),
                           "TYP27: as MALFORMED_CONSTRAINT");
      TestBit.is_true(anvil_type_def_fields_text(anvil_type_registry_find(reg, "Bad")) == NULL,
                      "TYP27: and carries no fields");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}


static void test_typ28_nullable_declaration(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = diag_registry(
      "#!aml\n@[types]\n"
      "Yes := { type := Numeric; nullable := true; };\n"
      "No := { type := Numeric; nullable := false; };\n"
      "Unset := { type := Numeric; };\n"
      "Bad := { type := Numeric; nullable := maybe; };\n", &doc);
   TestBit.is_not_null(reg, "TYP28: registry loaded");
   if (reg) {
      bool declared = false;
      TestBit.is_true(anvil_type_def_nullable(anvil_type_registry_find(reg, "Yes"), &declared) &&
                         declared,
                      "TYP28: nullable := true is declared and true");
      TestBit.is_true(!anvil_type_def_nullable(anvil_type_registry_find(reg, "No"), &declared) &&
                         declared,
                      "TYP28: nullable := false is declared and false");
      TestBit.is_true(!anvil_type_def_nullable(anvil_type_registry_find(reg, "Unset"), &declared) &&
                         !declared,
                      "TYP28: an absent nullable is not declared");
      TestBit.is_true(!anvil_type_def_nullable(anvil_type_registry_find(reg, "Bad"), &declared) &&
                         !declared,
                      "TYP28: a malformed nullable is not declared");
      TestBit.is_true(!anvil_type_def_nullable(NULL, &declared) && !declared,
                      "TYP28: NULL def");
      TestBit.is_equal_int(1, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP28: only the malformed one is reported");
      TestBit.is_equal_int(ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT,
                           anvil_type_diagnostic_get_category(anvil_type_registry_get_diagnostic(reg, 0)),
                           "TYP28: as MALFORMED_CONSTRAINT");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}


static void test_typ29_numeric_bound_getters(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = diag_registry(
      "#!aml\n@[types]\n"
      "Frac := { type := Numeric; min := 0.25; max := 1e+3; };\n"
      "Whole := { type := Numeric; min := 1; max := 9; };\n"
      "None := { type := Numeric; };\n"
      "Bad := { type := Numeric; min := \"x\"; max := 1e+999; };\n", &doc);
   TestBit.is_not_null(reg, "TYP29: registry loaded");
   if (reg) {
      anvil_numeric_t n = {0};
      long long ll = -1;
      anvil_type_def frac = anvil_type_registry_find(reg, "Frac");
      TestBit.is_true(anvil_type_def_get_min_numeric(frac, &n) && !n.is_integral && n.as_double == 0.25,
                      "TYP29: min := 0.25 reads exactly through the numeric getter");
      TestBit.is_false(anvil_type_def_get_min(frac, &ll), "TYP29: and not through the long long one");
      TestBit.is_true(anvil_type_def_get_max_numeric(frac, &n) && n.as_double == 1000.0,
                      "TYP29: max := 1e+3 reads as 1000");
      TestBit.is_false(anvil_type_def_get_max(frac, &ll), "TYP29: exponent form is not truncated");

      anvil_type_def whole = anvil_type_registry_find(reg, "Whole");
      TestBit.is_true(anvil_type_def_get_min_numeric(whole, &n) && n.is_integral && n.as_int64 == 1,
                      "TYP29: min := 1 via the numeric getter");
      TestBit.is_true(anvil_type_def_get_min(whole, &ll) && ll == 1, "TYP29: and the long long one");
      TestBit.is_true(anvil_type_def_get_max_numeric(whole, &n) && n.as_int64 == 9, "TYP29: max");
      TestBit.is_true(anvil_type_def_get_max(whole, &ll) && ll == 9, "TYP29: max long long");

      anvil_type_def none = anvil_type_registry_find(reg, "None");
      TestBit.is_false(anvil_type_def_get_min_numeric(none, &n), "TYP29: undeclared min");
      TestBit.is_false(anvil_type_def_get_max_numeric(none, &n), "TYP29: undeclared max");
      anvil_type_def bad = anvil_type_registry_find(reg, "Bad");
      TestBit.is_false(anvil_type_def_get_min_numeric(bad, &n), "TYP29: malformed min is not declared");
      TestBit.is_false(anvil_type_def_get_max_numeric(bad, &n), "TYP29: overflowing max is not declared");
      TestBit.is_false(anvil_type_def_get_min_numeric(NULL, &n), "TYP29: NULL def (min)");
      TestBit.is_false(anvil_type_def_get_max_numeric(NULL, &n), "TYP29: NULL def (max)");
      TestBit.is_false(anvil_type_def_get_min_numeric(whole, NULL), "TYP29: NULL out (min)");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_typ30_integer_declaration(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = diag_registry(
      "#!aml\n@[types]\n"
      "Yes := { type := Numeric; integer := true; };\n"
      "No := { type := Numeric; integer := false; };\n"
      "Unset := { type := Numeric; };\n"
      "Bad := { type := Numeric; integer := maybe; };\n", &doc);
   TestBit.is_not_null(reg, "TYP30: registry loaded");
   if (reg) {
      bool declared = false;
      TestBit.is_true(anvil_type_def_integer(anvil_type_registry_find(reg, "Yes"), &declared) && declared,
                      "TYP30: integer := true");
      TestBit.is_true(!anvil_type_def_integer(anvil_type_registry_find(reg, "No"), &declared) && declared,
                      "TYP30: integer := false is declared and false");
      TestBit.is_true(!anvil_type_def_integer(anvil_type_registry_find(reg, "Unset"), &declared) && !declared,
                      "TYP30: absent");
      TestBit.is_true(!anvil_type_def_integer(anvil_type_registry_find(reg, "Bad"), &declared) && !declared,
                      "TYP30: malformed is not declared");
      TestBit.is_equal_int(1, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP30: and reported once");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}


static void test_typ31_blob_primitive(void) {
   anvil_document doc = NULL;
   anvil_type_registry reg = diag_registry(
      "#!aml\n@[types]\n"
      "Raw := { type := Blob; };\n"
      "Png := { type := Blob; tag := png; };\n"
      "Quoted := { type := Blob; tag := \"jpg\"; };\n", &doc);
   TestBit.is_not_null(reg, "TYP31: registry loaded");
   if (reg) {
      TestBit.is_equal_int(ANVIL_TYPE_BLOB, anvil_type_def_get_kind(anvil_type_registry_find(reg, "Raw")),
                           "TYP31: `type := Blob` in a @[types] document is ANVIL_TYPE_BLOB");
      anvil_type_def native = anvil_type_resolve(NULL, "Blob");
      TestBit.is_not_null(native, "TYP31: Blob is a native primitive, resolvable without a registry");
      TestBit.is_equal_int(ANVIL_TYPE_BLOB, anvil_type_def_get_kind(native), "TYP31: of kind BLOB");
      TestBit.is_true(anvil_type_def_tag(anvil_type_registry_find(reg, "Raw")) == NULL,
                      "TYP31: no tag declared");
      const char *tag = anvil_type_def_tag(anvil_type_registry_find(reg, "Png"));
      TestBit.is_true(tag && strcmp(tag, "png") == 0, "TYP31: tag := png (bare)");
      tag = anvil_type_def_tag(anvil_type_registry_find(reg, "Quoted"));
      TestBit.is_true(tag && strcmp(tag, "jpg") == 0, "TYP31: tag := \"jpg\" (quoted)");
      TestBit.is_true(anvil_type_def_tag(NULL) == NULL, "TYP31: NULL def");
      TestBit.is_equal_int(0, (long long)anvil_type_registry_get_diagnostic_count(reg),
                           "TYP31: no diagnostics");
      anvil_type_registry_dispose(reg);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("TYP01_load_and_find", NULL, test_typ01_load_and_find, th);
   TestBit.run_ex("TYP02_reject_non_types_document", NULL,
                  test_typ02_reject_non_types_document, th);
   TestBit.run_ex("TYP03_null_safety", NULL, test_typ03_null_safety, th);
   TestBit.run_ex("TYP04_constraints", NULL, test_typ04_constraints, th);
   TestBit.run_ex("TYP05_constraint_null_safety", NULL, test_typ05_constraint_null_safety, th);
   TestBit.run_ex("TYP06_load_from_includes", NULL, test_typ06_load_from_includes, th);
   TestBit.run_ex("TYP07_load_from_includes_empty", NULL, test_typ07_load_from_includes_empty, th);
   TestBit.run_ex("TYP08_resolve_native", NULL, test_typ08_resolve_native, th);
   TestBit.run_ex("TYP09_resolve_custom_falls_through", NULL,
                  test_typ09_resolve_custom_falls_through, th);
   TestBit.run_ex("TYP10_resolve_null_safety", NULL, test_typ10_resolve_null_safety, th);
   TestBit.run_ex("TYP11_numeric_constraints_not_truncated", NULL,
                  test_typ11_numeric_constraints_not_truncated, th);
   TestBit.run_ex("TYP12_long_type_names", NULL, test_typ12_long_type_names, th);
   TestBit.run_ex("TYP13_name_collision_reported", NULL,
                  test_typ13_name_collision_reported, th);
   TestBit.run_ex("TYP14_unknown_base_type", NULL,
                  test_typ14_unknown_base_type, th);
   TestBit.run_ex("TYP15_malformed_constraint", NULL,
                  test_typ15_malformed_constraint, th);
   TestBit.run_ex("TYP16_malformed_constraint_variants", NULL,
                  test_typ16_malformed_constraint_variants, th);
   TestBit.run_ex("TYP17_malformed_enum_values", NULL,
                  test_typ17_malformed_enum_values, th);
   TestBit.run_ex("TYP18_clean_registry_has_no_diagnostics", NULL,
                  test_typ18_clean_registry_has_no_diagnostics, th);
   TestBit.run_ex("TYP19_diagnostics_survive_document_disposal", NULL,
                  test_typ19_diagnostics_survive_document_disposal, th);
   TestBit.run_ex("TYP20_diagnostic_null_safety", NULL,
                  test_typ20_diagnostic_null_safety, th);
   TestBit.run_ex("TYP21_document_get_path", NULL, test_typ21_document_get_path, th);
   TestBit.run_ex("TYP22_TRANSITIVE_INCLUDE", NULL, test_typ22_transitive_include, th);
   TestBit.run_ex("TYP23_INCLUDE_CYCLE_TERMINATES", NULL, test_typ23_include_cycle_terminates, th);
   TestBit.run_ex("TYP24_NON_TYPES_DOCUMENT_IS_TRAVERSED", NULL, test_typ24_non_types_document_is_traversed, th);
   TestBit.run_ex("TYP25_CLOSER_DEFINITION_OVERRIDES_DEEPER", NULL, test_typ25_closer_definition_overrides_deeper, th);
   TestBit.run_ex("TYP26_SHARED_INCLUDE_VISITED_ONCE", NULL, test_typ26_shared_include_visited_once, th);

   TestBit.run_ex("TYP27_object_type_fields", NULL, test_typ27_object_type_fields, th);

   TestBit.run_ex("TYP28_nullable_declaration", NULL, test_typ28_nullable_declaration, th);

   TestBit.run_ex("TYP29_numeric_bound_getters", NULL, test_typ29_numeric_bound_getters, th);
   TestBit.run_ex("TYP30_integer_declaration", NULL, test_typ30_integer_declaration, th);

   TestBit.run_ex("TYP31_blob_primitive", NULL, test_typ31_blob_primitive, th);

   return TestBit.report();
}
