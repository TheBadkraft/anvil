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
 * itself resolves types from its own direct imports
 * ---------------------------------------------------------------------- */
static void test_typ06_load_from_imports(void) {
   anvil_document doc = anvil_load(fixture_path("types_consumer.anvl"));
   TestBit.is_not_null(doc, "TYP06: consumer document loaded");
   if (!doc) {
      return;
   }

   anvil_type_registry reg = anvil_type_registry_load_from_imports(doc);
   TestBit.is_not_null(reg, "TYP06: registry built from imports");
   if (reg) {
      anvil_type_def vin = anvil_type_registry_find(reg, "VIN");
      TestBit.is_not_null(vin, "TYP06: 'VIN' (from the imported types file) is resolvable");
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
 * TYP07 — load_from_imports on a document with no @[types] imports at
 * all still yields a real, empty registry, not NULL
 * ---------------------------------------------------------------------- */
static void test_typ07_load_from_imports_empty(void) {
   TestBit.is_null(anvil_type_registry_load_from_imports(NULL),
                   "TYP07: load_from_imports(NULL) is NULL");

   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl")); // no imports at all
   TestBit.is_not_null(doc, "TYP07: document with no imports loaded");
   if (doc) {
      anvil_type_registry reg = anvil_type_registry_load_from_imports(doc);
      TestBit.is_not_null(reg, "TYP07: a document with no imports still yields a real registry");
      if (reg) {
         TestBit.is_null(anvil_type_registry_find(reg, "VIN"),
                         "TYP07: nothing is resolvable from an empty registry");
         anvil_type_registry_dispose(reg);
      }
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
   TestBit.run_ex("TYP06_load_from_imports", NULL, test_typ06_load_from_imports, th);
   TestBit.run_ex("TYP07_load_from_imports_empty", NULL, test_typ07_load_from_imports_empty, th);

   return TestBit.report();
}
