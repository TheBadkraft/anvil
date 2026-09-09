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

// Finds a violation by field name among all of them, so multi-violation tests don't depend on
// collection order.
static anvil_schema_violation find_violation(anvil_schema schema, const char *field_name) {
   size_t count = anvil_schema_get_violation_count(schema);
   for (size_t i = 0; i < count; i++) {
      anvil_schema_violation v = anvil_schema_get_violation(schema, i);
      char field[32] = {0};
      anvil_schema_violation_get_field(v, field, sizeof(field));
      if (strcmp(field, field_name) == 0) {
         return v;
      }
   }
   return NULL;
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
 * SCH08 — a String field's declared `size` is a maximum-length bound
 * ---------------------------------------------------------------------- */
static void test_sch08_size_constraint(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_basic.anvl"));
   anvil_schema schema = schema_doc ? anvil_schema_load(schema_doc) : NULL;
   TestBit.is_not_null(schema, "SCH08: schema loaded");
   if (schema) {
      // 'name' declares size := 20 — six characters is well within bounds.
      const char *ok = "#!aml\nasset_id := 1;\nname := \"widget\";\n";
      anvil_document ok_doc = anvil_load_buffer(ok, strlen(ok));
      if (ok_doc) {
         TestBit.is_true(anvil_schema_validate(schema, ok_doc), "SCH08: within size passes");
         anvil_dispose(ok_doc);
      }

      const char *too_long = "#!aml\nasset_id := 1;\nname := \"this name is definitely over twenty characters\";\n";
      anvil_document long_doc = anvil_load_buffer(too_long, strlen(too_long));
      if (long_doc) {
         TestBit.is_false(anvil_schema_validate(schema, long_doc), "SCH08: over size fails");
         anvil_schema_violation v = find_violation(schema, "name");
         TestBit.is_not_null(v, "SCH08: violation names 'name'");
         TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_SIZE,
                               anvil_schema_violation_get_category(v),
                               "SCH08: category is VALIDATION_SIZE");
         anvil_dispose(long_doc);
      }
      anvil_schema_dispose(schema);
   }
   if (schema_doc) {
      anvil_dispose(schema_doc);
   }
}
/* ---------------------------------------------------------------------- *
 * SCH09 — a Numeric field's declared `min`/`max` bound its legal range
 * ---------------------------------------------------------------------- */
static void test_sch09_min_max_constraint(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_basic.anvl"));
   anvil_schema schema = schema_doc ? anvil_schema_load(schema_doc) : NULL;
   TestBit.is_not_null(schema, "SCH09: schema loaded");
   if (schema) {
      // 'year' declares min := 1900; max := 2100;
      const char *ok = "#!aml\nasset_id := 1;\nyear := 2020;\n";
      anvil_document ok_doc = anvil_load_buffer(ok, strlen(ok));
      if (ok_doc) {
         TestBit.is_true(anvil_schema_validate(schema, ok_doc), "SCH09: within range passes");
         anvil_dispose(ok_doc);
      }

      const char *too_high = "#!aml\nasset_id := 1;\nyear := 2200;\n";
      anvil_document high_doc = anvil_load_buffer(too_high, strlen(too_high));
      if (high_doc) {
         TestBit.is_false(anvil_schema_validate(schema, high_doc), "SCH09: above max fails");
         anvil_schema_violation v = find_violation(schema, "year");
         TestBit.is_not_null(v, "SCH09: violation names 'year'");
         TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_RANGE,
                               anvil_schema_violation_get_category(v),
                               "SCH09: category is VALIDATION_RANGE (above max)");
         anvil_dispose(high_doc);
      }

      const char *too_low = "#!aml\nasset_id := 1;\nyear := 1899;\n";
      anvil_document low_doc = anvil_load_buffer(too_low, strlen(too_low));
      if (low_doc) {
         TestBit.is_false(anvil_schema_validate(schema, low_doc), "SCH09: below min fails");
         anvil_schema_violation lv = find_violation(schema, "year");
         TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_RANGE,
                               anvil_schema_violation_get_category(lv),
                               "SCH09: category is VALIDATION_RANGE (below min)");
         anvil_dispose(low_doc);
      }
      anvil_schema_dispose(schema);
   }
   if (schema_doc) {
      anvil_dispose(schema_doc);
   }
}
/* ---------------------------------------------------------------------- *
 * SCH10 — an enum field's declared `values` are a membership constraint
 * ---------------------------------------------------------------------- */
static void test_sch10_values_constraint(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_basic.anvl"));
   anvil_schema schema = schema_doc ? anvil_schema_load(schema_doc) : NULL;
   TestBit.is_not_null(schema, "SCH10: schema loaded");
   if (schema) {
      // 'status' declares values := [ active, maintenance, out_of_service ];
      const char *ok = "#!aml\nasset_id := 1;\nstatus := active;\n";
      anvil_document ok_doc = anvil_load_buffer(ok, strlen(ok));
      if (ok_doc) {
         TestBit.is_true(anvil_schema_validate(schema, ok_doc), "SCH10: a legal member passes");
         anvil_dispose(ok_doc);
      }

      // FlyWire's own real convention (quoted strings) must work identically to the bare form.
      const char *ok_quoted = "#!aml\nasset_id := 1;\nstatus := \"maintenance\";\n";
      anvil_document ok_quoted_doc = anvil_load_buffer(ok_quoted, strlen(ok_quoted));
      if (ok_quoted_doc) {
         TestBit.is_true(anvil_schema_validate(schema, ok_quoted_doc),
                         "SCH10: a quoted legal member also passes");
         anvil_dispose(ok_quoted_doc);
      }

      const char *bad = "#!aml\nasset_id := 1;\nstatus := scrapped;\n";
      anvil_document bad_doc = anvil_load_buffer(bad, strlen(bad));
      if (bad_doc) {
         TestBit.is_false(anvil_schema_validate(schema, bad_doc), "SCH10: a non-member fails");
         anvil_schema_violation v = find_violation(schema, "status");
         TestBit.is_not_null(v, "SCH10: violation names 'status'");
         TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_VALUES,
                               anvil_schema_violation_get_category(v),
                               "SCH10: category is VALIDATION_VALUES");
         anvil_dispose(bad_doc);
      }
      anvil_schema_dispose(schema);
   }
   if (schema_doc) {
      anvil_dispose(schema_doc);
   }
}
/* ---------------------------------------------------------------------- *
 * SCH11 — a constraint inherited from a resolved custom type (no inline
 * override) is enforced using the type's own declared value
 * ---------------------------------------------------------------------- */
static void test_sch11_constraint_inherited_from_type(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_with_custom_type.anvl"));
   TestBit.is_not_null(schema_doc, "SCH11: schema document loaded");
   if (!schema_doc) {
      return;
   }
   anvil_schema schema = anvil_schema_load(schema_doc);
   TestBit.is_not_null(schema, "SCH11: schema loaded");
   if (schema) {
      // 'vin' declares type := types.VIN; with no inline size — VIN itself declares size := 17.
      const char *within = "#!aml\nvin := \"12345678901234567\";\n"; // 17 digits, verified
      anvil_document within_doc = anvil_load_buffer(within, strlen(within));
      if (within_doc) {
         TestBit.is_true(anvil_schema_validate(schema, within_doc),
                         "SCH11: exactly at the inherited size passes");
         anvil_dispose(within_doc);
      }

      const char *over = "#!aml\nvin := \"1234567890123456789\";\n"; // 19 digits, verified
      anvil_document over_doc = anvil_load_buffer(over, strlen(over));
      if (over_doc) {
         TestBit.is_false(anvil_schema_validate(schema, over_doc),
                          "SCH11: over the inherited size fails");
         TestBit.is_not_null(find_violation(schema, "vin"), "SCH11: violation names 'vin'");
         anvil_dispose(over_doc);
      }
      anvil_schema_dispose(schema);
   }
   anvil_dispose(schema_doc);
}
/* ---------------------------------------------------------------------- *
 * SCH12 — a field's own inline constraint overrides the resolved type's
 * constraint, rather than being merged with it
 * ---------------------------------------------------------------------- */
static void test_sch12_inline_constraint_overrides_type(void) {
   anvil_document schema_doc = anvil_load(fixture_path("schema_with_custom_type.anvl"));
   TestBit.is_not_null(schema_doc, "SCH12: schema document loaded");
   if (!schema_doc) {
      return;
   }
   anvil_schema schema = anvil_schema_load(schema_doc);
   TestBit.is_not_null(schema, "SCH12: schema loaded");
   if (schema) {
      // 'vin_override' declares type := types.VIN; size := 25; — the field's own 25 should win
      // over VIN's own 17. A 20-character value would fail against VIN's size alone, but
      // should pass here. 'vin' (required) also needs a legal value, or its own missing-field
      // violation would mask what this test is actually checking.
      const char *twenty_chars =
         "#!aml\nvin := \"12345678901234567\";\nvin_override := \"12345678901234567890\";\n";
      anvil_document doc = anvil_load_buffer(twenty_chars, strlen(twenty_chars));
      if (doc) {
         TestBit.is_true(anvil_schema_validate(schema, doc),
                         "SCH12: the field's own override size (25) is used, not VIN's (17)");
         anvil_dispose(doc);
      }
      anvil_schema_dispose(schema);
   }
   anvil_dispose(schema_doc);
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
   TestBit.run_ex("SCH08_size_constraint", NULL, test_sch08_size_constraint, th);
   TestBit.run_ex("SCH09_min_max_constraint", NULL, test_sch09_min_max_constraint, th);
   TestBit.run_ex("SCH10_values_constraint", NULL, test_sch10_values_constraint, th);
   TestBit.run_ex("SCH11_constraint_inherited_from_type", NULL,
                  test_sch11_constraint_inherited_from_type, th);
   TestBit.run_ex("SCH12_inline_constraint_overrides_type", NULL,
                  test_sch12_inline_constraint_overrides_type, th);

   return TestBit.report();
}
