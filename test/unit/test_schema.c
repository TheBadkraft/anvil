/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_schema.c - Unit tests for AnvilSchema (schema.c)                  *
 * ---------------------------------------------------------------------- *
 * First slice: @[schema] loading, required-field presence, and          *
 * type-kind validation, collecting every violation. See                 *
 * notes/native-schema.md.                                                *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_schema.h"
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
 * SCH01 — a clean data document validates with zero violations
 * ---------------------------------------------------------------------- */
static void test_sch01_valid_document(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_basic.anvl"));
   TestBit.is_not_null(schema_doc, "SCH01: schema document loaded");
   if (!schema_doc) {
      return;
   }
   anvil_schema schema = anvil_schema_load(schema_doc);
   TestBit.is_not_null(schema, "SCH01: schema loaded");
   if (schema) {
      const char *data = "#!aml\nasset_id := 1;\nname := \"widget\";\n";
      anvil_document data_doc = anvil_load_buffer(data, strlen(data));
      TestBit.is_not_null(data_doc, "SCH01: data document loaded");
      if (data_doc) {
         TestBit.is_true(anvil_schema_validate(schema, data_doc), "SCH01: valid data passes");
         TestBit.is_equal_int(0, (long long)anvil_schema_get_violation_count(schema),
                              "SCH01: zero violations");
         anvil_dispose(data_doc);
      }
      anvil_schema_dispose(schema);
   }
   anvil_dispose(schema_doc);
}
/* ---------------------------------------------------------------------- *
 * SCH02 — a document without @[schema] is rejected
 * ---------------------------------------------------------------------- */
static void test_sch02_reject_non_schema_document(void) {
   anvil_document doc = anvil_load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "SCH02: document loaded");
   if (doc) {
      TestBit.is_null(anvil_schema_load(doc), "SCH02: a document without @[schema] is rejected");
      anvil_dispose(doc);
   }
}
/* ---------------------------------------------------------------------- *
 * SCH03 — a missing required field is a violation
 * ---------------------------------------------------------------------- */
static void test_sch03_required_field_missing(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_basic.anvl"));
   TestBit.is_not_null(schema_doc, "SCH03: schema document loaded");
   if (!schema_doc) {
      return;
   }
   anvil_schema schema = anvil_schema_load(schema_doc);
   if (schema) {
      const char *data = "#!aml\nname := \"widget\";\n"; // asset_id (required) missing
      anvil_document data_doc = anvil_load_buffer(data, strlen(data));
      if (data_doc) {
         TestBit.is_false(anvil_schema_validate(schema, data_doc),
                          "SCH03: missing required field fails validation");
         TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                              "SCH03: one violation");
         anvil_schema_violation v = anvil_schema_get_violation(schema, 0);
         TestBit.is_not_null(v, "SCH03: violation retrieved");
         if (v) {
            TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,
                                 anvil_schema_violation_get_category(v),
                                 "SCH03: category is VALIDATION_REQUIRED");
            char field[32] = {0};
            anvil_schema_violation_get_field(v, field, sizeof(field));
            TestBit.is_true(strcmp(field, "asset_id") == 0, "SCH03: violation names 'asset_id'");
         }
         anvil_dispose(data_doc);
      }
      anvil_schema_dispose(schema);
   }
   anvil_dispose(schema_doc);
}
/* ---------------------------------------------------------------------- *
 * SCH04 — a type mismatch is a violation
 * ---------------------------------------------------------------------- */
static void test_sch04_type_mismatch(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_basic.anvl"));
   TestBit.is_not_null(schema_doc, "SCH04: schema document loaded");
   if (!schema_doc) {
      return;
   }
   anvil_schema schema = anvil_schema_load(schema_doc);
   if (schema) {
      // 'name' declares type := String; sending a numeric value is a mismatch
      const char *data = "#!aml\nasset_id := 1;\nname := 42;\n";
      anvil_document data_doc = anvil_load_buffer(data, strlen(data));
      if (data_doc) {
         TestBit.is_false(anvil_schema_validate(schema, data_doc),
                          "SCH04: type mismatch fails validation");
         TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                              "SCH04: one violation");
         anvil_schema_violation v = anvil_schema_get_violation(schema, 0);
         if (v) {
            TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                                 anvil_schema_violation_get_category(v),
                                 "SCH04: category is VALIDATION_TYPE_MISMATCH");
            char field[32] = {0};
            anvil_schema_violation_get_field(v, field, sizeof(field));
            TestBit.is_true(strcmp(field, "name") == 0, "SCH04: violation names 'name'");
         }
         anvil_dispose(data_doc);
      }
      anvil_schema_dispose(schema);
   }
   anvil_dispose(schema_doc);
}
/* ---------------------------------------------------------------------- *
 * SCH05 — a field not declared in the schema is a violation
 * ---------------------------------------------------------------------- */
static void test_sch05_unknown_field(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_basic.anvl"));
   TestBit.is_not_null(schema_doc, "SCH05: schema document loaded");
   if (!schema_doc) {
      return;
   }
   anvil_schema schema = anvil_schema_load(schema_doc);
   if (schema) {
      const char *data = "#!aml\nasset_id := 1;\nname := \"widget\";\nextra := true;\n";
      anvil_document data_doc = anvil_load_buffer(data, strlen(data));
      if (data_doc) {
         TestBit.is_false(anvil_schema_validate(schema, data_doc),
                          "SCH05: an undeclared field fails validation");
         TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                              "SCH05: one violation");
         anvil_schema_violation v = anvil_schema_get_violation(schema, 0);
         if (v) {
            TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD,
                                 anvil_schema_violation_get_category(v),
                                 "SCH05: category is VALIDATION_UNKNOWN_FIELD");
            char field[32] = {0};
            anvil_schema_violation_get_field(v, field, sizeof(field));
            TestBit.is_true(strcmp(field, "extra") == 0, "SCH05: violation names 'extra'");
         }
         anvil_dispose(data_doc);
      }
      anvil_schema_dispose(schema);
   }
   anvil_dispose(schema_doc);
}
/* ---------------------------------------------------------------------- *
 * SCH06 — every violation is collected in one pass, not fail-fast
 * ---------------------------------------------------------------------- */
static void test_sch06_collects_all_violations(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_basic.anvl"));
   TestBit.is_not_null(schema_doc, "SCH06: schema document loaded");
   if (!schema_doc) {
      return;
   }
   anvil_schema schema = anvil_schema_load(schema_doc);
   if (schema) {
      // asset_id (required) missing, name has the wrong type, extra is undeclared — three
      // distinct problems in one document.
      const char *data = "#!aml\nname := 42;\nextra := true;\n";
      anvil_document data_doc = anvil_load_buffer(data, strlen(data));
      if (data_doc) {
         TestBit.is_false(anvil_schema_validate(schema, data_doc), "SCH06: validation fails");
         TestBit.is_equal_int(3, (long long)anvil_schema_get_violation_count(schema),
                              "SCH06: all three violations collected in one pass");
         anvil_dispose(data_doc);
      }
      anvil_schema_dispose(schema);
   }
   anvil_dispose(schema_doc);
}
/* ---------------------------------------------------------------------- *
 * SCH07 — NULL safety, and dispose(NULL) is a no-op
 * ---------------------------------------------------------------------- */
static void test_sch07_null_safety(void) {
   TestBit.is_null(anvil_schema_load(NULL), "SCH07: load(NULL) is NULL");
   anvil_schema_dispose(NULL); // must not crash

   TestBit.is_false(anvil_schema_validate(NULL, NULL), "SCH07: validate(NULL, NULL) is false");
   TestBit.is_equal_int(0, (long long)anvil_schema_get_violation_count(NULL),
                        "SCH07: get_violation_count(NULL) is 0");
   TestBit.is_null(anvil_schema_get_violation(NULL, 0), "SCH07: get_violation(NULL, ...) is NULL");
   TestBit.is_equal_int(ANVIL_SCHEMA_ERR_NONE, anvil_schema_violation_get_category(NULL),
                        "SCH07: violation_get_category(NULL) is ANVIL_SCHEMA_ERR_NONE");
   TestBit.is_equal_int(0, (long long)anvil_schema_violation_get_field(NULL, NULL, 0),
                        "SCH07: violation_get_field(NULL, ...) is 0");
   TestBit.is_equal_int(0, (long long)anvil_schema_violation_get_message(NULL, NULL, 0),
                        "SCH07: violation_get_message(NULL, ...) is 0");

   anvil_document schema_doc = anvil_load(fixture_path("schema_basic.anvl"));
   if (schema_doc) {
      anvil_schema schema = anvil_schema_load(schema_doc);
      if (schema) {
         TestBit.is_false(anvil_schema_validate(schema, NULL),
                          "SCH07: validate(schema, NULL) is false");
         anvil_schema_dispose(schema);
      }
      anvil_dispose(schema_doc);
   }
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("SCH01_valid_document", NULL, test_sch01_valid_document, th);
   TestBit.run_ex("SCH02_reject_non_schema_document", NULL,
                  test_sch02_reject_non_schema_document, th);
   TestBit.run_ex("SCH03_required_field_missing", NULL, test_sch03_required_field_missing, th);
   TestBit.run_ex("SCH04_type_mismatch", NULL, test_sch04_type_mismatch, th);
   TestBit.run_ex("SCH05_unknown_field", NULL, test_sch05_unknown_field, th);
   TestBit.run_ex("SCH06_collects_all_violations", NULL, test_sch06_collects_all_violations, th);
   TestBit.run_ex("SCH07_null_safety", NULL, test_sch07_null_safety, th);

   return TestBit.report();
}
