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
#include "anvil_schema_vtable.h"
#include "anvil_vtable.h"
#include "anvil_types.h"
#include "internal/source_registry.h"
#include "testbit.h"
// ----------------
#include "../utilities/helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
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
 * BR-2610-anvl-types-001 — numeric constraints and data values keep their
 * full magnitude: fractional, exponent, long and overflowed forms
 * ---------------------------------------------------------------------- */

// Validates `data` against a one-field schema built from `schema_src`; returns the field's
// violation category, or 0 when the data passed. Keeps each numeric case to a single line.
static int numeric_case(const char *schema_src, const char *data, const char *field) {
   anvil_document sdoc = anvil_load_buffer(schema_src, strlen(schema_src));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   int category = -1;
   if (schema) {
      anvil_document ddoc = anvil_load_buffer(data, strlen(data));
      if (ddoc) {
         bool ok = anvil_schema_validate(schema, ddoc);
         anvil_schema_violation v = ok ? NULL : find_violation(schema, field);
         category = ok ? 0 : (v ? (int)anvil_schema_violation_get_category(v) : -2);
         if (!ok) {
            TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                                 "numeric case: exactly one violation");
         }
         anvil_dispose(ddoc);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
   return category;
}

#define SCHEMA_MAX3 "#!aml\n@[schema]\nscore := { type := Numeric; max := 3; };\n"
#define RANGE ANVIL_SCHEMA_ERR_VALIDATION_RANGE

static void test_sch13_fractional_data_vs_integral_bound(void) {
   TestBit.is_equal_int(RANGE, numeric_case(SCHEMA_MAX3, "#!aml\nscore := 3.7;\n", "score"),
                        "SCH13: 3.7 exceeds max := 3");
   TestBit.is_equal_int(0, numeric_case(SCHEMA_MAX3, "#!aml\nscore := 3;\n", "score"),
                        "SCH13: 3 satisfies max := 3");
   TestBit.is_equal_int(0, numeric_case(SCHEMA_MAX3, "#!aml\nscore := 2.9;\n", "score"),
                        "SCH13: 2.9 satisfies max := 3");
}

static void test_sch14_fractional_bound(void) {
   const char *src = "#!aml\n@[schema]\nscore := { type := Numeric; min := 0.5; };\n";
   TestBit.is_equal_int(RANGE, numeric_case(src, "#!aml\nscore := 0.2;\n", "score"),
                        "SCH14: 0.2 is below min := 0.5");
   TestBit.is_equal_int(0, numeric_case(src, "#!aml\nscore := 0.6;\n", "score"),
                        "SCH14: 0.6 satisfies min := 0.5");
   TestBit.is_equal_int(RANGE, numeric_case(src, "#!aml\nscore := 0;\n", "score"),
                        "SCH14: integral 0 is below min := 0.5");
   TestBit.is_equal_int(0, numeric_case(src, "#!aml\nscore := 1;\n", "score"),
                        "SCH14: integral 1 satisfies min := 0.5");
}

static void test_sch15_exponent_bound(void) {
   const char *src = "#!aml\n@[schema]\nscore := { type := Numeric; max := 1e+3; };\n";
   TestBit.is_equal_int(0, numeric_case(src, "#!aml\nscore := 999;\n", "score"),
                        "SCH15: 999 satisfies max := 1e+3");
   TestBit.is_equal_int(RANGE, numeric_case(src, "#!aml\nscore := 1001;\n", "score"),
                        "SCH15: 1001 exceeds max := 1e+3");
   TestBit.is_equal_int(RANGE, numeric_case(SCHEMA_MAX3, "#!aml\nscore := 5e+10;\n", "score"),
                        "SCH15: data 5e+10 is not read as 5");
}

static void test_sch15b_unsigned_exponent(void) {
   // FR-2610-anvl-parser-002: 1e308 used to be a bare word and failed Numeric as TYPE_MISMATCH.
   const char *any = "#!aml\n@[schema]\nscore := { type := Numeric; };\n";
   TestBit.is_equal_int(0, numeric_case(any, "#!aml\nscore := 1e308;\n", "score"),
                        "SCH15b: 1e308 satisfies Numeric");
   TestBit.is_equal_int(0, numeric_case(any, "#!aml\nscore := 1E3;\n", "score"),
                        "SCH15b: 1E3 satisfies Numeric");
   const char *src = "#!aml\n@[schema]\nscore := { type := Numeric; max := 1e3; };\n";
   TestBit.is_equal_int(0, numeric_case(src, "#!aml\nscore := 999;\n", "score"),
                        "SCH15b: 999 satisfies max := 1e3");
   TestBit.is_equal_int(RANGE, numeric_case(src, "#!aml\nscore := 1001;\n", "score"),
                        "SCH15b: 1001 exceeds max := 1e3");
   TestBit.is_equal_int(RANGE, numeric_case(SCHEMA_MAX3, "#!aml\nscore := 5e10;\n", "score"),
                        "SCH15b: data 5e10 is not read as 5");
}

static void test_sch16_long_numeric_text(void) {
   // 39 characters, real value 1e7: a 31-character cut reads it as 0.
   TestBit.is_equal_int(RANGE,
      numeric_case("#!aml\n@[schema]\nscore := { type := Numeric; max := 1000; };\n",
                   "#!aml\nscore := 0.000000000000000000000000000000001e+40;\n", "score"),
      "SCH16: a numeric longer than 31 chars is compared by real magnitude");
}

static void test_sch17_overflowed_data_compares_by_sign(void) {
   const char *max_src = "#!aml\n@[schema]\nscore := { type := Numeric; max := 1000; };\n";
   const char *min_src = "#!aml\n@[schema]\nscore := { type := Numeric; min := 0; };\n";
   TestBit.is_equal_int(RANGE, numeric_case(max_src, "#!aml\nscore := 1e+999;\n", "score"),
                        "SCH17: +overflow violates any max");
   TestBit.is_equal_int(0, numeric_case(min_src, "#!aml\nscore := 1e+999;\n", "score"),
                        "SCH17: +overflow satisfies any min");
   TestBit.is_equal_int(RANGE, numeric_case(min_src, "#!aml\nscore := -1e+999;\n", "score"),
                        "SCH17: -overflow violates any min");
   TestBit.is_equal_int(0, numeric_case(max_src, "#!aml\nscore := -1e+999;\n", "score"),
                        "SCH17: -overflow satisfies any max");
   TestBit.is_equal_int(RANGE,
                        numeric_case(max_src, "#!aml\nscore := 99999999999999999999;\n", "score"),
                        "SCH17: an integral literal beyond int64 violates max");
}

static void test_sch18_exact_comparison_near_limits(void) {
   const char *src = "#!aml\n@[schema]\nscore := { type := Numeric; max := 9007199254740992; };\n";
   TestBit.is_equal_int(0, numeric_case(src, "#!aml\nscore := 9007199254740992;\n", "score"),
                        "SCH18: 2^53 satisfies max := 2^53");
   TestBit.is_equal_int(RANGE, numeric_case(src, "#!aml\nscore := 9007199254740993;\n", "score"),
                        "SCH18: 2^53+1 exceeds max := 2^53 (no double rounding)");
   const char *big = "#!aml\n@[schema]\nscore := { type := Numeric; max := 9223372036854775807; };\n";
   TestBit.is_equal_int(0, numeric_case(big, "#!aml\nscore := 9223372036854775806;\n", "score"),
                        "SCH18: int64 max - 1 satisfies max := int64 max");
   TestBit.is_equal_int(RANGE, numeric_case(big, "#!aml\nscore := 9.3e+18;\n", "score"),
                        "SCH18: a fractional-form value above int64 max exceeds it");
}

static void test_sch19_fractional_bound_inherited_from_type(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_numeric_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH19: schema loaded");
   if (schema) {
      const char *low = "#!aml\nshare := 0.2;\n";
      anvil_document ldoc = anvil_load_buffer(low, strlen(low));
      if (ldoc) {
         TestBit.is_false(anvil_schema_validate(schema, ldoc),
                          "SCH19: 0.2 is below the inherited min := 0.5");
         TestBit.is_not_null(find_violation(schema, "share"), "SCH19: violation names 'share'");
         anvil_dispose(ldoc);
      }
      const char *ok = "#!aml\nshare := 0.6;\n";
      anvil_document odoc = anvil_load_buffer(ok, strlen(ok));
      if (odoc) {
         TestBit.is_true(anvil_schema_validate(schema, odoc),
                         "SCH19: 0.6 satisfies the inherited min := 0.5");
         anvil_dispose(odoc);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch20_malformed_constraints_are_not_declared(void) {
   // A non-numeric or overflowing bound is "not declared", never silently 0.
   TestBit.is_equal_int(0,
      numeric_case("#!aml\n@[schema]\nscore := { type := Numeric; min := \"abc\"; };\n",
                   "#!aml\nscore := -5;\n", "score"),
      "SCH20: non-numeric min is not declared");
   TestBit.is_equal_int(0,
      numeric_case("#!aml\n@[schema]\nscore := { type := Numeric; max := 1e+999; };\n",
                   "#!aml\nscore := 5;\n", "score"),
      "SCH20: overflowing max is not declared");
   // size stays integral: a fractional size is malformed, not truncated to 2.
   TestBit.is_equal_int(0,
      numeric_case("#!aml\n@[schema]\nname := { type := String; size := 2.5; };\n",
                   "#!aml\nname := \"abcdef\";\n", "name"),
      "SCH20: fractional size is not declared");
}


/* ---------------------------------------------------------------------- *
 * BR-2610-anvl-schema-001 — no fixed-buffer truncation: long type names,
 * rule keys, enum labels, field names and violation text keep their full length
 * ---------------------------------------------------------------------- */
static void test_sch21_long_type_names_resolve_exactly(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_long_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH21: schema loaded");
   if (schema) {
      // ref_b -> 80-char String type, ref_a -> 80-char Numeric type; the two share their first 63.
      const char *good = "#!aml\nref_b := \"abc\";\nref_a := 5;\n";
      anvil_document gdoc = anvil_load_buffer(good, strlen(good));
      if (gdoc) {
         TestBit.is_true(anvil_schema_validate(schema, gdoc),
                         "SCH21: each reference resolves to its own 80-char type");
         anvil_dispose(gdoc);
      }
      const char *bad = "#!aml\nref_b := 5;\nref_a := \"abc\";\n";
      anvil_document bdoc = anvil_load_buffer(bad, strlen(bad));
      if (bdoc) {
         TestBit.is_false(anvil_schema_validate(schema, bdoc), "SCH21: swapped kinds fail");
         TestBit.is_equal_int(2, (long long)anvil_schema_get_violation_count(schema),
                              "SCH21: both references are checked against their own type");
         anvil_dispose(bdoc);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch22_long_enum_labels_compare_in_full(void) {
   char label[201], same[201], diff[201], src[512], data[512];
   memset(label, 'a', 200); label[200] = '\0';
   memcpy(same, label, 201);
   memcpy(diff, label, 201);
   diff[150] = 'b'; // differs only at character 150, past any 127-char prefix
   snprintf(src, sizeof(src),
            "#!aml\n@[schema]\nstatus := { type := enum; values := [ \"%s\" ]; };\n", label);
   snprintf(data, sizeof(data), "#!aml\nstatus := \"%s\";\n", same);
   TestBit.is_equal_int(0, numeric_case(src, data, "status"),
                        "SCH22: the identical 200-char label is accepted");
   snprintf(data, sizeof(data), "#!aml\nstatus := \"%s\";\n", diff);
   TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_VALUES, numeric_case(src, data, "status"),
                        "SCH22: a value differing only at character 150 is rejected");
}

static void test_sch23_long_rule_key_is_not_a_known_key(void) {
   const char *src =
      "#!aml\n@[schema]\n"
      "score := { type := Numeric; max := 3; maxxxxxxxxxxxxxxxxxxxxxx := 1; "
      "required_by_flywire_extension := true; };\n";
   TestBit.is_equal_int(0, numeric_case(src, "#!aml\nscore := 3;\n", "score"),
                        "SCH23: an extension key does not alter max or required");
}

static void test_sch24_long_field_and_violation_names(void) {
   char a[81], b[81], src[512], data[512];
   memset(a, 'f', 64); memset(a + 64, 'A', 16); a[80] = '\0'; // 64-char shared prefix
   memset(b, 'f', 64); memset(b + 64, 'B', 16); b[80] = '\0';
   snprintf(src, sizeof(src), "#!aml\n@[schema]\n%s := { type := Numeric; required := true; };\n", a);
   anvil_document sdoc = anvil_load_buffer(src, strlen(src));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH24: schema with an 80-char field name loaded");
   if (schema) {
      // Declared field present, plus an undeclared one sharing its first 64 chars.
      snprintf(data, sizeof(data), "#!aml\n%s := 1;\n%s := 2;\n", a, b);
      anvil_document ddoc = anvil_load_buffer(data, strlen(data));
      if (ddoc) {
         TestBit.is_false(anvil_schema_validate(schema, ddoc), "SCH24: the undeclared field fails");
         TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                              "SCH24: exactly the undeclared field is reported");
         anvil_schema_violation v = anvil_schema_get_violation(schema, 0);
         TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD,
                              anvil_schema_violation_get_category(v), "SCH24: it is UNKNOWN_FIELD");
         char out[128] = {0};
         TestBit.is_equal_int(80, (long long)anvil_schema_violation_get_field(v, NULL, 0),
                              "SCH24: violation field reports its full length");
         anvil_schema_violation_get_field(v, out, sizeof(out));
         TestBit.is_true(strcmp(out, b) == 0, "SCH24: violation field is the full, exact name");
         anvil_dispose(ddoc);
      }
      // Missing required 80-char field: the REQUIRED violation names it in full.
      const char *none = "#!aml\n";
      anvil_document ndoc = anvil_load_buffer(none, strlen(none));
      if (ndoc) {
         TestBit.is_false(anvil_schema_validate(schema, ndoc), "SCH24: required field missing");
         anvil_schema_violation v = anvil_schema_get_violation(schema, 0);
         char out[128] = {0};
         anvil_schema_violation_get_field(v, out, sizeof(out));
         TestBit.is_true(strcmp(out, a) == 0, "SCH24: REQUIRED names the full 80-char field");
         anvil_dispose(ndoc);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-schema-004 — schema load diagnostics
 * ---------------------------------------------------------------------- */

// Loads a schema from buffer source and hands back both the schema and its document.
static anvil_schema diag_schema(const char *src, anvil_document *out_doc) {
   *out_doc = anvil_load_buffer(src, strlen(src));
   return *out_doc ? anvil_schema_load(*out_doc) : NULL;
}

static void diag_expect(anvil_schema schema, size_t index, anvil_schema_diag_code code,
                        const char *field, const char *msg_part, const char *label) {
   anvil_schema_diagnostic d = anvil_schema_get_diagnostic(schema, index);
   TestBit.is_not_null(d, label);
   if (!d) {
      return;
   }
   TestBit.is_equal_int(code, anvil_schema_diagnostic_get_category(d), label);
   char out[512] = {0};
   anvil_schema_diagnostic_get_field(d, out, sizeof(out));
   TestBit.is_true(strcmp(out, field) == 0, label);
   anvil_schema_diagnostic_get_message(d, out, sizeof(out));
   TestBit.is_true(msg_part == NULL || strstr(out, msg_part) != NULL, label);
}

static void test_sch25_unknown_type(void) {
   anvil_document doc = NULL;
   anvil_schema schema = diag_schema(
      "#!aml\n@[schema]\npriority := { type := types.Prority; required := true; };\n", &doc);
   TestBit.is_not_null(schema, "SCH25: the schema still loads");
   if (schema) {
      TestBit.is_equal_int(1, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH25: one diagnostic");
      diag_expect(schema, 0, ANVIL_SCHEMA_DIAG_UNKNOWN_TYPE, "priority", "Prority",
                  "SCH25: UNKNOWN_TYPE at 'priority' naming the type");
      anvil_schema_dispose(schema);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_sch26_malformed_constraints(void) {
   anvil_document doc = NULL;
   anvil_schema schema = diag_schema(
      "#!aml\n@[schema]\n"
      "a := { type := String; size := abc; };\n"
      "b := { type := Numeric; min := \"x\"; };\n"
      "c := { type := Numeric; max := 1e+999; };\n"
      "d := { type := enum; values := red; };\n"
      "e := { type := String; required := maybe; };\n", &doc);
   TestBit.is_not_null(schema, "SCH26: the schema still loads");
   if (schema) {
      TestBit.is_equal_int(5, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH26: each malformed constraint is reported once");
      diag_expect(schema, 0, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, "a", "size",
                  "SCH26: size := abc");
      diag_expect(schema, 1, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, "b", "min",
                  "SCH26: non-numeric min");
      diag_expect(schema, 2, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, "c", "max",
                  "SCH26: overflowing max");
      diag_expect(schema, 3, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, "d", "values",
                  "SCH26: values that isn't an array");
      diag_expect(schema, 4, ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, "e", "required",
                  "SCH26: required that isn't true/false");
      // The weakened rules still validate: a malformed size is "not declared", never 0.
      const char *data = "#!aml\na := \"anything at all\";\n";
      anvil_document ddoc = anvil_load_buffer(data, strlen(data));
      if (ddoc) {
         anvil_schema_validate(schema, ddoc);
         TestBit.is_true(find_violation(schema, "a") == NULL,
                         "SCH26: the unreadable size does not become a size of 0");
         anvil_dispose(ddoc);
      }
      anvil_schema_dispose(schema);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_sch27_rule_not_object(void) {
   anvil_document doc = NULL;
   anvil_schema schema = diag_schema("#!aml\n@[schema]\nbad := 5;\ngood := { type := Numeric; };\n",
                                     &doc);
   TestBit.is_not_null(schema, "SCH27: the schema still loads");
   if (schema) {
      TestBit.is_equal_int(1, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH27: one diagnostic");
      diag_expect(schema, 0, ANVIL_SCHEMA_DIAG_RULE_NOT_OBJECT, "bad", NULL,
                  "SCH27: RULE_NOT_OBJECT naming 'bad'");
      anvil_schema_dispose(schema);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_sch28_conflicting_constraints(void) {
   anvil_document doc = NULL;
   anvil_schema schema = diag_schema(
      "#!aml\n@[schema]\n"
      "a := { type := Numeric; min := 10; max := 1; };\n"
      "b := { type := Numeric; min := 1; max := 1; };\n"
      "c := { type := Numeric; min := 0.5; max := 0.25; };\n", &doc);
   if (schema) {
      TestBit.is_equal_int(2, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH28: min > max reported (equal bounds are satisfiable)");
      diag_expect(schema, 0, ANVIL_SCHEMA_DIAG_CONFLICTING_CONSTRAINTS, "a", NULL,
                  "SCH28: integral min > max");
      diag_expect(schema, 1, ANVIL_SCHEMA_DIAG_CONFLICTING_CONSTRAINTS, "c", NULL,
                  "SCH28: fractional min > max");
      anvil_schema_dispose(schema);
   } else {
      TestBit.is_not_null(schema, "SCH28: schema loaded");
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_sch29_extension_keys_are_not_diagnostics(void) {
   anvil_document doc = NULL;
   anvil_schema schema = diag_schema(
      "#!aml\n@[schema]\nname := { type := String; pooled := true; mask := \"***\"; };\n"
      "bare := { pooled := true; };\n", &doc);
   TestBit.is_not_null(schema, "SCH29: schema loaded");
   if (schema) {
      TestBit.is_equal_int(0, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH29: unknown rule keys and an untyped rule produce no diagnostics");
      anvil_schema_dispose(schema);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_sch30_well_formed_fixtures_have_no_diagnostics(void) {
   const char *fixtures[] = {"schema_basic.anvl", "schema_with_custom_type.anvl",
                             "schema_numeric_consumer.anvl", "schema_long_consumer.anvl"};
   for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
      anvil_document doc = anvil_load(fixture_path(fixtures[i]));
      anvil_schema schema = doc ? anvil_schema_load(doc) : NULL;
      TestBit.is_not_null(schema, fixtures[i]);
      if (schema) {
         TestBit.is_equal_int(0, (long long)anvil_schema_get_diagnostic_count(schema), fixtures[i]);
         anvil_schema_dispose(schema);
      }
      if (doc) {
         anvil_dispose(doc);
      }
   }
}

static void test_sch31_registry_diagnostics_pass_through(void) {
   anvil_document doc = anvil_load(fixture_path("schema_dup_types_consumer.anvl"));
   anvil_schema schema = doc ? anvil_schema_load(doc) : NULL;
   TestBit.is_not_null(schema, "SCH31: schema loaded");
   if (schema) {
      TestBit.is_equal_int(1, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH31: the registry's collision appears in the schema's list");
      anvil_schema_diagnostic d = anvil_schema_get_diagnostic(schema, 0);
      if (d) {
         TestBit.is_equal_int(ANVIL_SCHEMA_DIAG_TYPE_REGISTRY, anvil_schema_diagnostic_get_category(d),
                              "SCH31: category is TYPE_REGISTRY");
         TestBit.is_equal_int(ANVIL_TYPE_DIAG_NAME_COLLISION,
                              anvil_schema_diagnostic_get_type_category(d),
                              "SCH31: the registry's own category is preserved");
         char out[512] = {0};
         anvil_schema_diagnostic_get_field(d, out, sizeof(out));
         TestBit.is_true(strcmp(out, "VIN") == 0, "SCH31: field is the type's name");
         anvil_schema_diagnostic_get_message(d, out, sizeof(out));
         TestBit.is_true(strstr(out, "types_dup_a.anvl") && strstr(out, "types_dup_b.anvl"),
                         "SCH31: the message names both documents");
      }
      anvil_schema_dispose(schema);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_sch32_diagnostics_outlive_documents_and_null_safety(void) {
   anvil_document doc = NULL;
   anvil_schema schema = diag_schema("#!aml\n@[schema]\nbad := 5;\n", &doc);
   if (doc) {
      anvil_dispose(doc);
   }
   if (schema) {
      anvil_schema_diagnostic d = anvil_schema_get_diagnostic(schema, 0);
      TestBit.is_not_null(d, "SCH32: diagnostic survives disposal of the schema document");
      char out[64] = {0};
      TestBit.is_true(anvil_schema_diagnostic_get_field(d, out, sizeof(out)) == 3 &&
                         strcmp(out, "bad") == 0,
                      "SCH32: and is still readable");
      TestBit.is_true(anvil_schema_get_diagnostic(schema, 1) == NULL,
                      "SCH32: out-of-bounds index is NULL");
      anvil_schema_dispose(schema);
   } else {
      TestBit.is_not_null(schema, "SCH32: schema loaded");
   }
   TestBit.is_equal_int(0, (long long)anvil_schema_get_diagnostic_count(NULL), "SCH32: count(NULL)");
   TestBit.is_true(anvil_schema_get_diagnostic(NULL, 0) == NULL, "SCH32: get(NULL)");
   TestBit.is_equal_int(ANVIL_SCHEMA_DIAG_NONE, anvil_schema_diagnostic_get_category(NULL),
                        "SCH32: category(NULL)");
   TestBit.is_equal_int(ANVIL_TYPE_DIAG_NONE, anvil_schema_diagnostic_get_type_category(NULL),
                        "SCH32: type_category(NULL)");
   TestBit.is_equal_int(0, (long long)anvil_schema_diagnostic_get_field(NULL, NULL, 0),
                        "SCH32: field(NULL)");
   TestBit.is_equal_int(0, (long long)anvil_schema_diagnostic_get_message(NULL, NULL, 0),
                        "SCH32: message(NULL)");
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-schema-002 — anvil_schema_validate_value and options
 * ---------------------------------------------------------------------- */
#define SCHEMA_TICKET \
   "#!aml\n@[schema]\n" \
   "status := { type := enum; values := [ open, closed ]; required := true; };\n" \
   "title := { type := String; };\n"

// The value of the top-level statement `name` in doc.
static anvil_value value_of(anvil_document doc, const char *name) {
   anvil_statement st = anvil_document_find_statement(doc, name);
   return st ? anvil_statement_get_value(st) : NULL;
}

static anvil_schema_options opts_of(bool skip_required, bool allow_unknown) {
   anvil_schema_options o = {.size = sizeof(o)};
   o.skip_required = skip_required;
   o.allow_unknown = allow_unknown;
   return o;
}

static void test_sch33_validate_object_value(void) {
   anvil_document sdoc = anvil_load_buffer(SCHEMA_TICKET, strlen(SCHEMA_TICKET));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   const char *src = "#!aml\nset := { status := open; title := \"x\"; };\n";
   anvil_document ddoc = anvil_load_buffer(src, strlen(src));
   TestBit.is_true(schema && ddoc, "SCH33: schema and data loaded");
   if (schema && ddoc) {
      TestBit.is_true(anvil_schema_validate_value(schema, value_of(ddoc, "set"), NULL),
                      "SCH33: the object under 'set' validates with default options");
      TestBit.is_equal_int(0, (long long)anvil_schema_get_violation_count(schema),
                           "SCH33: zero violations");
      anvil_schema_options zero = {.size = sizeof(zero)};
      TestBit.is_true(anvil_schema_validate_value(schema, value_of(ddoc, "set"), &zero),
                      "SCH33: and with a zero-initialised options struct");
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (ddoc) {
      anvil_dispose(ddoc);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch34_skip_required(void) {
   anvil_document sdoc = anvil_load_buffer(SCHEMA_TICKET, strlen(SCHEMA_TICKET));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   const char *src = "#!aml\nset := { title := \"x\"; };\n";
   anvil_document ddoc = anvil_load_buffer(src, strlen(src));
   TestBit.is_true(schema && ddoc, "SCH34: schema and data loaded");
   if (schema && ddoc) {
      TestBit.is_false(anvil_schema_validate_value(schema, value_of(ddoc, "set"), NULL),
                       "SCH34: a missing required field fails by default");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH34: one violation");
      TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,
                           anvil_schema_violation_get_category(anvil_schema_get_violation(schema, 0)),
                           "SCH34: it is REQUIRED");
      anvil_schema_options patch = opts_of(true, false);
      TestBit.is_true(anvil_schema_validate_value(schema, value_of(ddoc, "set"), &patch),
                      "SCH34: skip_required passes the same value");
      TestBit.is_equal_int(0, (long long)anvil_schema_get_violation_count(schema),
                           "SCH34: and the previous violations were replaced");
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (ddoc) {
      anvil_dispose(ddoc);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch35_skip_required_still_checks_present_fields(void) {
   anvil_document sdoc = anvil_load_buffer(SCHEMA_TICKET, strlen(SCHEMA_TICKET));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   const char *src = "#!aml\nset := { status := bogus; };\n";
   anvil_document ddoc = anvil_load_buffer(src, strlen(src));
   TestBit.is_true(schema && ddoc, "SCH35: schema and data loaded");
   if (schema && ddoc) {
      anvil_schema_options patch = opts_of(true, false);
      TestBit.is_false(anvil_schema_validate_value(schema, value_of(ddoc, "set"), &patch),
                       "SCH35: an invalid present field fails under skip_required");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH35: one violation");
      TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_VALUES,
                           anvil_schema_violation_get_category(anvil_schema_get_violation(schema, 0)),
                           "SCH35: it is VALUES");
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (ddoc) {
      anvil_dispose(ddoc);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch36_allow_unknown(void) {
   anvil_document sdoc = anvil_load_buffer(SCHEMA_TICKET, strlen(SCHEMA_TICKET));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   const char *src = "#!aml\nset := { status := open; extra := 1; };\n";
   anvil_document ddoc = anvil_load_buffer(src, strlen(src));
   TestBit.is_true(schema && ddoc, "SCH36: schema and data loaded");
   if (schema && ddoc) {
      TestBit.is_false(anvil_schema_validate_value(schema, value_of(ddoc, "set"), NULL),
                       "SCH36: an undeclared field fails by default");
      TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD,
                           anvil_schema_violation_get_category(anvil_schema_get_violation(schema, 0)),
                           "SCH36: as UNKNOWN_FIELD");
      anvil_schema_options lax = opts_of(false, true);
      TestBit.is_true(anvil_schema_validate_value(schema, value_of(ddoc, "set"), &lax),
                      "SCH36: allow_unknown suppresses it");
      TestBit.is_equal_int(0, (long long)anvil_schema_get_violation_count(schema),
                           "SCH36: zero violations");
      // allow_unknown does not relax anything else
      const char *bad = "#!aml\nset := { extra := 1; };\n";
      anvil_document bdoc = anvil_load_buffer(bad, strlen(bad));
      if (bdoc) {
         TestBit.is_false(anvil_schema_validate_value(schema, value_of(bdoc, "set"), &lax),
                          "SCH36: a missing required field still fails under allow_unknown");
         anvil_dispose(bdoc);
      }
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (ddoc) {
      anvil_dispose(ddoc);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch37_non_object_value(void) {
   anvil_document sdoc = anvil_load_buffer(SCHEMA_TICKET, strlen(SCHEMA_TICKET));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   const char *src = "#!aml\nlist := [ 1, 2 ];\nnum := 5;\n";
   anvil_document ddoc = anvil_load_buffer(src, strlen(src));
   TestBit.is_true(schema && ddoc, "SCH37: schema and data loaded");
   if (schema && ddoc) {
      TestBit.is_false(anvil_schema_validate_value(schema, value_of(ddoc, "list"), NULL),
                       "SCH37: an ARRAY value fails");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH37: exactly one violation");
      anvil_schema_violation v = anvil_schema_get_violation(schema, 0);
      TestBit.is_equal_int(ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                           anvil_schema_violation_get_category(v), "SCH37: TYPE_MISMATCH");
      TestBit.is_equal_int(0, (long long)anvil_schema_violation_get_field(v, NULL, 0),
                           "SCH37: with an empty field path");
      TestBit.is_false(anvil_schema_validate_value(schema, value_of(ddoc, "num"), NULL),
                       "SCH37: a scalar fails too");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH37: still exactly one (replaced, not accumulated)");
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (ddoc) {
      anvil_dispose(ddoc);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch38_each_call_replaces_violations(void) {
   anvil_document sdoc = anvil_load_buffer(SCHEMA_TICKET, strlen(SCHEMA_TICKET));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   const char *src = "#!aml\nset := [ { title := \"a\"; }, { status := open; } ];\n";
   anvil_document ddoc = anvil_load_buffer(src, strlen(src));
   TestBit.is_true(schema && ddoc, "SCH38: schema and data loaded");
   if (schema && ddoc) {
      anvil_value arr = value_of(ddoc, "set");
      TestBit.is_equal_int(2, (long long)anvil_value_get_count(arr), "SCH38: two elements");
      TestBit.is_false(anvil_schema_validate_value(schema, anvil_value_get_element(arr, 0), NULL),
                       "SCH38: the first element is missing 'status'");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH38: one violation after the first call");
      TestBit.is_true(anvil_schema_validate_value(schema, anvil_value_get_element(arr, 1), NULL),
                      "SCH38: the second element is valid");
      TestBit.is_equal_int(0, (long long)anvil_schema_get_violation_count(schema),
                           "SCH38: the second call replaced the first call's violations");
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (ddoc) {
      anvil_dispose(ddoc);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch39_options_forward_compatible_and_null_safety(void) {
   anvil_document sdoc = anvil_load_buffer(SCHEMA_TICKET, strlen(SCHEMA_TICKET));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   const char *src = "#!aml\nset := { title := \"x\"; };\n";
   anvil_document ddoc = anvil_load_buffer(src, strlen(src));
   if (schema && ddoc) {
      // A caller compiled against an older struct that ended before skip_required: size says so,
      // and the (garbage) members past it must read as defaults.
      anvil_schema_options old = opts_of(true, true);
      old.size = sizeof(size_t);
      TestBit.is_false(anvil_schema_validate_value(schema, value_of(ddoc, "set"), &old),
                       "SCH39: members beyond opts->size are ignored (defaults apply)");
      anvil_schema_options none = opts_of(true, true);
      none.size = 0;
      TestBit.is_false(anvil_schema_validate_value(schema, value_of(ddoc, "set"), &none),
                       "SCH39: size 0 means all defaults");
      anvil_value set = value_of(ddoc, "set");
      TestBit.is_false(anvil_schema_validate_value(NULL, set, NULL), "SCH39: NULL schema");
      TestBit.is_false(anvil_schema_validate_value(schema, NULL, NULL), "SCH39: NULL value");
   } else {
      TestBit.is_true(false, "SCH39: schema and data loaded");
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (ddoc) {
      anvil_dispose(ddoc);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch40_document_form_equals_value_form(void) {
   // The document form is the value form with default options: same violations, same order.
   const char *schema_src = SCHEMA_TICKET;
   const char *doc_src = "#!aml\nstatus := bogus;\ntitle := 5;\nextra := 1;\n";
   const char *obj_src = "#!aml\nset := { status := bogus; title := 5; extra := 1; };\n";
   anvil_document sdoc = anvil_load_buffer(schema_src, strlen(schema_src));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   anvil_document d1 = anvil_load_buffer(doc_src, strlen(doc_src));
   anvil_document d2 = anvil_load_buffer(obj_src, strlen(obj_src));
   TestBit.is_true(schema && d1 && d2, "SCH40: loaded");
   if (schema && d1 && d2) {
      anvil_schema_validate_value(schema, value_of(d2, "set"), NULL);
      size_t n1 = anvil_schema_get_violation_count(schema);
      int cats[8] = {0};
      char fields[8][16] = {{0}};
      for (size_t i = 0; i < n1 && i < 8; i++) {
         anvil_schema_violation v = anvil_schema_get_violation(schema, i);
         cats[i] = (int)anvil_schema_violation_get_category(v);
         anvil_schema_violation_get_field(v, fields[i], sizeof(fields[i]));
      }
      TestBit.is_equal_int(3, (long long)n1, "SCH40: three violations (values, type, unknown)");
      anvil_schema_validate(schema, d1);
      TestBit.is_equal_int((long long)n1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH40: the document form finds the same number");
      for (size_t i = 0; i < n1 && i < 8; i++) {
         anvil_schema_violation v = anvil_schema_get_violation(schema, i);
         char f[16] = {0};
         anvil_schema_violation_get_field(v, f, sizeof(f));
         TestBit.is_true(cats[i] == (int)anvil_schema_violation_get_category(v) &&
                            strcmp(fields[i], f) == 0,
                         "SCH40: same category and field at each position");
      }
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (d1) {
      anvil_dispose(d1);
   }
   if (d2) {
      anvil_dispose(d2);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-schema-001 — nested validation, element rules, field paths
 * ---------------------------------------------------------------------- */

// Violation `index`'s field path and category, for compact assertions.
static void violation_at(anvil_schema schema, size_t index, char *path, size_t cap, int *category) {
   anvil_schema_violation v = anvil_schema_get_violation(schema, index);
   path[0] = '\0';
   *category = -1;
   if (v) {
      anvil_schema_violation_get_field(v, path, cap);
      *category = (int)anvil_schema_violation_get_category(v);
   }
}

// Validates `data` (a whole document) against a schema built from `schema_src`; leaves the schema
// for the caller to inspect. Returns the validate result; *out_* receive what to dispose.
static bool nested_validate(const char *schema_src, const char *data, anvil_schema *out_schema,
                            anvil_document *out_sdoc, anvil_document *out_ddoc) {
   *out_sdoc = anvil_load_buffer(schema_src, strlen(schema_src));
   *out_schema = *out_sdoc ? anvil_schema_load(*out_sdoc) : NULL;
   *out_ddoc = anvil_load_buffer(data, strlen(data));
   if (!*out_schema || !*out_ddoc) {
      return true;
   }
   return anvil_schema_validate(*out_schema, *out_ddoc);
}

static void nested_free(anvil_schema schema, anvil_document sdoc, anvil_document ddoc) {
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
   if (ddoc) {
      anvil_dispose(ddoc);
   }
}

#define NESTED_PATH_IS(i, expected, cat, label)                                                  \
   do {                                                                                          \
      char p_[128];                                                                              \
      int c_;                                                                                    \
      violation_at(schema, (i), p_, sizeof(p_), &c_);                                            \
      TestBit.is_true(strcmp(p_, (expected)) == 0 && c_ == (int)(cat), (label));                  \
   } while (0)

static void test_sch41_object_field_rules(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\n"
      "assignee := { type := Object; fields := { id := { type := Numeric; required := true; }; }; };\n",
      "#!aml\nassignee := { name := bob; };\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH41: schema loaded");
   if (schema) {
      TestBit.is_false(ok, "SCH41: a nested violation fails validation");
      TestBit.is_equal_int(2, (long long)anvil_schema_get_violation_count(schema),
                           "SCH41: two violations");
      NESTED_PATH_IS(0, "assignee.id", ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,
                     "SCH41: assignee.id is REQUIRED");
      NESTED_PATH_IS(1, "assignee.name", ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD,
                     "SCH41: assignee.name is UNKNOWN_FIELD");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch42_array_item_rules(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\n"
      "labels := { type := Array; items := { type := String; values := [ bug, feature ]; }; };\n",
      "#!aml\nlabels := [ bug, nope, feature ];\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH42: schema loaded");
   if (schema) {
      TestBit.is_false(ok, "SCH42: a bad element fails validation");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH42: exactly one violation");
      NESTED_PATH_IS(0, "labels[1]", ANVIL_SCHEMA_ERR_VALIDATION_VALUES,
                     "SCH42: VALUES at labels[1]");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch43_array_of_objects(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\n"
      "links := { type := Array; items := { type := Object; fields := {\n"
      "   kind := { type := enum; values := [ blocks, relates ]; required := true; };\n"
      "   target := { type := Numeric; required := true; };\n"
      "}; }; };\n",
      "#!aml\nlinks := [ { kind := blocks; target := 1; }, { kind := bogus; target := 2; },\n"
      "                 { kind := relates; } ];\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH43: schema loaded");
   if (schema) {
      TestBit.is_false(ok, "SCH43: violations found");
      TestBit.is_equal_int(2, (long long)anvil_schema_get_violation_count(schema),
                           "SCH43: two violations");
      NESTED_PATH_IS(0, "links[1].kind", ANVIL_SCHEMA_ERR_VALIDATION_VALUES,
                     "SCH43: links[1].kind VALUES");
      NESTED_PATH_IS(1, "links[2].target", ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,
                     "SCH43: links[2].target REQUIRED");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch44_item_count(void) {
   const char *schema_src =
      "#!aml\n@[schema]\n"
      "labels := { type := Array; min_items := 2; max_items := 3; items := { type := String; }; };\n";
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(schema_src, "#!aml\nlabels := [ a, b, c, d ];\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH44: schema loaded");
   if (schema) {
      TestBit.is_false(ok, "SCH44: 4 items exceed max_items := 3");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH44: one violation");
      NESTED_PATH_IS(0, "labels", ANVIL_SCHEMA_ERR_VALIDATION_ITEM_COUNT,
                     "SCH44: ITEM_COUNT at 'labels'");
   }
   nested_free(schema, sdoc, ddoc);

   ok = nested_validate(schema_src, "#!aml\nlabels := [ a ];\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH44: 1 item is below min_items := 2");
      NESTED_PATH_IS(0, "labels", ANVIL_SCHEMA_ERR_VALIDATION_ITEM_COUNT,
                     "SCH44: ITEM_COUNT for too few");
   }
   nested_free(schema, sdoc, ddoc);

   ok = nested_validate(schema_src, "#!aml\nlabels := [ a, b ];\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_true(ok, "SCH44: 2 items are within 2..3");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch45_object_type_with_fields_is_reusable(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_nested_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH45: schema loaded");
   if (schema) {
      TestBit.is_equal_int(0, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH45: no diagnostics for the type's nested rules");
      const char *src = "#!aml\nowner := { name := \"toolongname\"; };\n"
                        "reporter := { id := 1; name := \"ok\"; };\n";
      anvil_document ddoc = anvil_load_buffer(src, strlen(src));
      if (ddoc) {
         TestBit.is_false(anvil_schema_validate(schema, ddoc), "SCH45: violations found");
         TestBit.is_equal_int(2, (long long)anvil_schema_get_violation_count(schema),
                              "SCH45: owner.id and owner.name only; reporter is clean");
         NESTED_PATH_IS(0, "owner.id", ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,
                        "SCH45: owner.id REQUIRED (from the type's fields)");
         NESTED_PATH_IS(1, "owner.name", ANVIL_SCHEMA_ERR_VALIDATION_SIZE,
                        "SCH45: owner.name SIZE (from the type's fields)");
         anvil_dispose(ddoc);
      }
      // The second reference validates its own nested contents too.
      const char *src2 = "#!aml\nowner := { id := 1; };\nreporter := { name := \"x\"; };\n";
      anvil_document d2 = anvil_load_buffer(src2, strlen(src2));
      if (d2) {
         anvil_schema_validate(schema, d2);
         NESTED_PATH_IS(0, "reporter.id", ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,
                        "SCH45: reporter.id REQUIRED - the shape is reusable");
         anvil_dispose(d2);
      }
      // An inline `fields` replaces the type's, never merges.
      const char *src3 = "#!aml\noverride := { code := \"c\"; };\n";
      anvil_document d3 = anvil_load_buffer(src3, strlen(src3));
      if (d3) {
         TestBit.is_true(anvil_schema_validate(schema, d3),
                         "SCH45: inline fields replace the type's (no inherited required id)");
         anvil_dispose(d3);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch46_top_level_paths_unchanged_and_order(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\n"
      "a := { type := Object; fields := { x := { type := Numeric; }; y := { type := Numeric; }; }; };\n"
      "b := { type := Numeric; };\n",
      "#!aml\na := { y := hi; x := hi; z := 1; };\nb := hi;\nc := 1;\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH46: schema loaded");
   if (schema) {
      TestBit.is_false(ok, "SCH46: violations found");
      TestBit.is_equal_int(5, (long long)anvil_schema_get_violation_count(schema),
                           "SCH46: five violations");
      NESTED_PATH_IS(0, "a.x", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                     "SCH46: schema order, depth first: a.x");
      NESTED_PATH_IS(1, "a.y", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH, "SCH46: then a.y");
      NESTED_PATH_IS(2, "a.z", ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD,
                     "SCH46: a's unknown field right after a's declared ones");
      NESTED_PATH_IS(3, "b", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                     "SCH46: a top-level field is its bare name, no leading dot");
      NESTED_PATH_IS(4, "c", ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD,
                     "SCH46: top-level unknown fields come last");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch47_scalar_item_rules(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\nscores := { type := Array; items := { type := Numeric; max := 3; }; };\n",
      "#!aml\nscores := [ 1, 5, 2 ];\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH47: an out-of-range element fails");
      NESTED_PATH_IS(0, "scores[1]", ANVIL_SCHEMA_ERR_VALIDATION_RANGE, "SCH47: RANGE at scores[1]");
   } else {
      TestBit.is_not_null(schema, "SCH47: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch48_self_reference_is_a_diagnostic(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_selfref_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH48: a self-referential type still loads (and terminates)");
   if (schema) {
      bool found = false;
      for (size_t i = 0; i < anvil_schema_get_diagnostic_count(schema); i++) {
         if (anvil_schema_diagnostic_get_category(anvil_schema_get_diagnostic(schema, i)) ==
             ANVIL_SCHEMA_DIAG_SELF_REFERENCE) {
            found = true;
         }
      }
      TestBit.is_true(found, "SCH48: reported as SELF_REFERENCE");
      const char *src = "#!aml\nroot := { label := 5; };\n";
      anvil_document ddoc = anvil_load_buffer(src, strlen(src));
      if (ddoc) {
         anvil_schema_validate(schema, ddoc);
         NESTED_PATH_IS(0, "root.label", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                        "SCH48: the non-recursive part of the shape still validates");
         anvil_dispose(ddoc);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch49_nested_diagnostics_carry_paths(void) {
   anvil_document doc = NULL;
   const char *src =
      "#!aml\n@[schema]\n"
      "a := { type := Object; fields := { bad := 5; ghost := { type := types.Nope; }; }; };\n"
      "l := { type := Array; items := 5; min_items := abc; };\n";
   doc = anvil_load_buffer(src, strlen(src));
   anvil_schema schema = doc ? anvil_schema_load(doc) : NULL;
   TestBit.is_not_null(schema, "SCH49: schema loaded");
   if (schema) {
      TestBit.is_equal_int(4, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH49: four diagnostics");
      const struct { anvil_schema_diag_code code; const char *path; } want[] = {
         {ANVIL_SCHEMA_DIAG_RULE_NOT_OBJECT, "a.bad"},
         {ANVIL_SCHEMA_DIAG_UNKNOWN_TYPE, "a.ghost"},
         {ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, "l"},
         {ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT, "l"},
      };
      for (size_t i = 0; i < 4; i++) {
         anvil_schema_diagnostic d = anvil_schema_get_diagnostic(schema, i);
         char path[64] = {0};
         anvil_schema_diagnostic_get_field(d, path, sizeof(path));
         TestBit.is_true(d && anvil_schema_diagnostic_get_category(d) == want[i].code &&
                            strcmp(path, want[i].path) == 0,
                         "SCH49: nested diagnostic has the right category and path");
      }
      anvil_schema_dispose(schema);
   }
   if (doc) {
      anvil_dispose(doc);
   }
}

static void test_sch50_nested_with_validate_value_options(void) {
   const char *schema_src =
      "#!aml\n@[schema]\n"
      "assignee := { type := Object; required := true; fields := {\n"
      "   id := { type := Numeric; required := true; }; }; };\n";
   anvil_document sdoc = anvil_load_buffer(schema_src, strlen(schema_src));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   const char *src = "#!aml\nset := { assignee := { extra := 1; }; };\n";
   anvil_document ddoc = anvil_load_buffer(src, strlen(src));
   TestBit.is_true(schema && ddoc, "SCH50: loaded");
   if (schema && ddoc) {
      anvil_statement st = anvil_document_find_statement(ddoc, "set");
      anvil_value set = st ? anvil_statement_get_value(st) : NULL;
      TestBit.is_false(anvil_schema_validate_value(schema, set, NULL), "SCH50: default fails");
      TestBit.is_equal_int(2, (long long)anvil_schema_get_violation_count(schema),
                           "SCH50: assignee.id REQUIRED and assignee.extra UNKNOWN");
      anvil_schema_options patch = {.size = sizeof(patch), .skip_required = true,
                                    .allow_unknown = true};
      TestBit.is_true(anvil_schema_validate_value(schema, set, &patch),
                      "SCH50: skip_required and allow_unknown apply at every depth");
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (ddoc) {
      anvil_dispose(ddoc);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-schema-003 — nullable: an explicit null is legal when declared
 * ---------------------------------------------------------------------- */
static void test_sch51_null_without_nullable(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate("#!aml\n@[schema]\nx := { type := Numeric; };\n",
                             "#!aml\nx := null;\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH51: schema loaded");
   if (schema) {
      TestBit.is_false(ok, "SCH51: null for a non-nullable typed field fails");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH51: one violation");
      NESTED_PATH_IS(0, "x", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                     "SCH51: TYPE_MISMATCH at x");
      char msg[128] = {0};
      anvil_schema_violation_get_message(anvil_schema_get_violation(schema, 0), msg, sizeof(msg));
      TestBit.is_true(strstr(msg, "not nullable") != NULL, "SCH51: the message says not nullable");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch52_nullable_accepts_null(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate("#!aml\n@[schema]\nx := { type := Numeric; nullable := true; };\n",
                             "#!aml\nx := null;\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH52: schema loaded");
   if (schema) {
      TestBit.is_true(ok, "SCH52: null passes for a nullable field");
      TestBit.is_equal_int(0, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH52: `nullable` is a known key, no diagnostic");
   }
   nested_free(schema, sdoc, ddoc);

   // nullable only adds null: a wrong-kind non-null value is still a mismatch.
   ok = nested_validate("#!aml\n@[schema]\nx := { type := Numeric; nullable := true; };\n",
                        "#!aml\nx := hi;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH52: nullable does not relax the kind of non-null values");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch53_null_skips_other_constraints(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\n"
      "s := { type := String; nullable := true; size := 1; values := [ a, b ]; };\n"
      "n := { type := Numeric; nullable := true; min := 5; };\n",
      "#!aml\ns := null;\nn := null;\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH53: schema loaded");
   if (schema) {
      TestBit.is_true(ok, "SCH53: values, size and range are not evaluated for null");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch54_required_and_nullable(void) {
   const char *src = "#!aml\n@[schema]\nx := { required := true; nullable := true; };\n";
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(src, "#!aml\nother := 1;\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH54: schema loaded");
   if (schema) {
      TestBit.is_false(ok, "SCH54: absent still fails a required field");
      NESTED_PATH_IS(0, "x", ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED, "SCH54: REQUIRED when absent");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nx := null;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_true(ok, "SCH54: present-and-null satisfies required + nullable");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch55_nullable_inherited_from_type(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_nullable_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH55: schema loaded");
   if (schema) {
      const char *src = "#!aml\ninherits := null;\nstrict := null;\nrestated := null;\n";
      anvil_document ddoc = anvil_load_buffer(src, strlen(src));
      if (ddoc) {
         TestBit.is_false(anvil_schema_validate(schema, ddoc), "SCH55: one field rejects null");
         TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                              "SCH55: exactly one violation");
         NESTED_PATH_IS(0, "strict", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                        "SCH55: `strict` declares nullable := false over the type's true");
         anvil_dispose(ddoc);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch56_nullable_at_every_depth(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\n"
      "o := { type := Object; fields := { a := { type := Numeric; nullable := true; };\n"
      "                                   b := { type := Numeric; }; }; };\n"
      "l := { type := Array; items := { type := String; nullable := true; }; };\n",
      "#!aml\no := { a := null; b := null; };\nl := [ x, null, y ];\n", &schema, &sdoc, &ddoc);
   TestBit.is_not_null(schema, "SCH56: schema loaded");
   if (schema) {
      TestBit.is_false(ok, "SCH56: o.b rejects null");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH56: only o.b; o.a and l[1] accept null");
      NESTED_PATH_IS(0, "o.b", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH, "SCH56: o.b");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch57_untyped_field_and_malformed_nullable(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   // No declared type: there is nothing for null to violate (unchanged behavior).
   bool ok = nested_validate("#!aml\n@[schema]\nx := { pooled := true; };\n",
                             "#!aml\nx := null;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_true(ok, "SCH57: null for an untyped field is not a violation");
   } else {
      TestBit.is_not_null(schema, "SCH57: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);

   ok = nested_validate("#!aml\n@[schema]\nx := { type := Numeric; nullable := maybe; };\n",
                        "#!aml\nx := 1;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_equal_int(1, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH57: a malformed nullable is diagnosed");
      TestBit.is_equal_int(ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT,
                           anvil_schema_diagnostic_get_category(anvil_schema_get_diagnostic(schema, 0)),
                           "SCH57: as MALFORMED_CONSTRAINT");
      TestBit.is_true(ok, "SCH57: and the field stays non-nullable but validates");
   } else {
      TestBit.is_not_null(schema, "SCH57: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-types-005 — integer := true (value-based) and fractional bounds
 * ---------------------------------------------------------------------- */
#define INT_SCHEMA "#!aml\n@[schema]\nn := { type := Numeric; integer := true; };\n"

static void test_sch58_integer_is_value_based(void) {
   const struct { const char *data; bool ok; const char *label; } cases[] = {
      {"#!aml\nn := 2;\n", true, "SCH58: 2 passes"},
      {"#!aml\nn := -3;\n", true, "SCH58: -3 passes"},
      {"#!aml\nn := 2.5;\n", false, "SCH58: 2.5 fails"},
      {"#!aml\nn := 2.0;\n", true, "SCH58: 2.0 passes (value-based, decision D2)"},
      {"#!aml\nn := 1e+3;\n", true, "SCH58: 1e+3 passes (value-based)"},
      {"#!aml\nn := 1.5e+3;\n", true, "SCH58: 1.5e+3 is 1500, passes"},
      {"#!aml\nn := 1e-1;\n", false, "SCH58: 1e-1 is 0.1, fails"},
      {"#!aml\nn := 99999999999999999999;\n", true, "SCH58: an integer literal past int64 passes"},
      {"#!aml\nn := 0.5;\n", false, "SCH58: 0.5 fails"},
   };
   for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
      anvil_schema schema; anvil_document sdoc, ddoc;
      bool ok = nested_validate(INT_SCHEMA, cases[i].data, &schema, &sdoc, &ddoc);
      TestBit.is_true(schema != NULL && ok == cases[i].ok, cases[i].label);
      if (schema && !ok) {
         TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                              "SCH58: exactly one violation");
         NESTED_PATH_IS(0, "n", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                        "SCH58: TYPE_MISMATCH at n");
         char msg[128] = {0};
         anvil_schema_violation_get_message(anvil_schema_get_violation(schema, 0), msg, sizeof(msg));
         TestBit.is_true(strstr(msg, "must be an integer") != NULL,
                         "SCH58: with the distinct message");
      }
      nested_free(schema, sdoc, ddoc);
   }
}

static void test_sch59_integer_with_range_and_null(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   const char *src = "#!aml\n@[schema]\npriority := { type := Numeric; integer := true; min := 1; max := 4; };\n";
   bool ok = nested_validate(src, "#!aml\npriority := 2.5;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH59: 2.5 inside 1..4 still fails integer");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH59: only the integer violation");
   } else {
      TestBit.is_not_null(schema, "SCH59: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\npriority := 9.5;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH59: 9.5 fails");
      TestBit.is_equal_int(2, (long long)anvil_schema_get_violation_count(schema),
                           "SCH59: integer and range are independent checks");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate("#!aml\n@[schema]\nn := { type := Numeric; integer := true; nullable := true; };\n",
                        "#!aml\nn := null;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_true(ok, "SCH59: a nullable null is not an integer check");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate("#!aml\n@[schema]\ns := { type := String; integer := true; };\n",
                        "#!aml\ns := \"a.b\";\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_true(ok, "SCH59: integer is ignored for a non-numeric value");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch60_integer_inherited_from_type(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_integer_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH60: schema loaded");
   if (schema) {
      const char *src = "#!aml\ninherits := 2.5;\nrelaxed := 2.5;\nrestated := 2.5;\n";
      anvil_document ddoc = anvil_load_buffer(src, strlen(src));
      if (ddoc) {
         TestBit.is_false(anvil_schema_validate(schema, ddoc), "SCH60: violations found");
         TestBit.is_equal_int(2, (long long)anvil_schema_get_violation_count(schema),
                              "SCH60: inherits and restated fail; relaxed overrides");
         NESTED_PATH_IS(0, "inherits", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                        "SCH60: `inherits` gets integer from its type");
         NESTED_PATH_IS(1, "restated", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH,
                        "SCH60: `restated` too");
         anvil_dispose(ddoc);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch61_integer_nested_and_malformed(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\nids := { type := Array; items := { type := Numeric; integer := true; }; };\n",
      "#!aml\nids := [ 1, 2.5, 3 ];\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH61: a fractional array element fails");
      NESTED_PATH_IS(0, "ids[1]", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH, "SCH61: at ids[1]");
   } else {
      TestBit.is_not_null(schema, "SCH61: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate("#!aml\n@[schema]\nn := { type := Numeric; integer := maybe; };\n",
                        "#!aml\nn := 2.5;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_equal_int(1, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH61: a malformed integer is diagnosed");
      TestBit.is_true(ok, "SCH61: and treated as not declared");
   } else {
      TestBit.is_not_null(schema, "SCH61: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-types-003 — Blob (decision D1: String accepts blobs, Blob is its own primitive)
 * ---------------------------------------------------------------------- */
static void test_sch62_string_accepts_blob(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   const char *src = "#!aml\n@[schema]\nbody := { type := String; };\n";
   bool ok = nested_validate(src, "#!aml\nbody := `multi-line\ntext`;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && ok, "SCH62: a blob satisfies type := String");
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nbody := \"quoted\";\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && ok, "SCH62: a quoted string still passes (regression)");
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nbody := bare;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && ok, "SCH62: a bare word still passes (regression)");
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nbody := 5;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && !ok, "SCH62: a number still fails (regression)");
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate("#!aml\n@[schema]\ne := { type := enum; values := [ a, b ]; };\n",
                        "#!aml\ne := `a`;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && !ok, "SCH62: an enum does not accept a blob");
   nested_free(schema, sdoc, ddoc);
}

static void test_sch63_string_size_measures_blob_payload(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   const char *src = "#!aml\n@[schema]\nbody := { type := String; size := 5; };\n";
   bool ok = nested_validate(src, "#!aml\nbody := `abcdef`;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH63: a 6-byte blob exceeds size := 5");
      NESTED_PATH_IS(0, "body", ANVIL_SCHEMA_ERR_VALIDATION_SIZE, "SCH63: SIZE at body");
   } else {
      TestBit.is_not_null(schema, "SCH63: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nbody := @txt`abcde`;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && ok, "SCH63: a 5-byte payload fits (the tag is not payload)");
   nested_free(schema, sdoc, ddoc);
}

static void test_sch64_blob_accepts_only_blobs(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   const char *src = "#!aml\n@[schema]\nb := { type := Blob; };\n";
   bool ok = nested_validate(src, "#!aml\nb := \"quoted\";\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH64: a quoted string fails type := Blob");
      NESTED_PATH_IS(0, "b", ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH, "SCH64: TYPE_MISMATCH");
   } else {
      TestBit.is_not_null(schema, "SCH64: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nb := bare;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && !ok, "SCH64: a bare word fails type := Blob");
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nb := `payload`;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && ok, "SCH64: a blob passes");
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nb := @png`payload`;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && ok, "SCH64: a tagged blob passes when no tag is required");
   nested_free(schema, sdoc, ddoc);
}

static void test_sch65_blob_tag_constraint(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   const char *src = "#!aml\n@[schema]\nb := { type := Blob; tag := png; };\n";
   bool ok = nested_validate(src, "#!aml\nb := @png`x`;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && ok, "SCH65: an @png blob satisfies tag := png");
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nb := `x`;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH65: an untagged blob fails");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH65: one violation");
      NESTED_PATH_IS(0, "b", ANVIL_SCHEMA_ERR_VALIDATION_TAG, "SCH65: TAG at b");
   } else {
      TestBit.is_not_null(schema, "SCH65: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nb := @jpg`x`;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH65: an @jpg blob fails");
      NESTED_PATH_IS(0, "b", ANVIL_SCHEMA_ERR_VALIDATION_TAG, "SCH65: TAG for the wrong tag");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nb := @pngx`x`;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && !ok, "SCH65: tags compare in full, not by prefix");
   nested_free(schema, sdoc, ddoc);
   // A quoted tag value reads the same as a bare one.
   ok = nested_validate("#!aml\n@[schema]\nb := { type := Blob; tag := \"png\"; };\n",
                        "#!aml\nb := @png`x`;\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && ok, "SCH65: tag := \"png\" (quoted) works too");
   nested_free(schema, sdoc, ddoc);
}

static void test_sch66_blob_size_and_inheritance(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate("#!aml\n@[schema]\nb := { type := Blob; size := 3; };\n",
                             "#!aml\nb := `abcd`;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH66: a Blob's size is its payload byte length");
      NESTED_PATH_IS(0, "b", ANVIL_SCHEMA_ERR_VALIDATION_SIZE, "SCH66: SIZE");
   } else {
      TestBit.is_not_null(schema, "SCH66: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);

   anvil_document tdoc = anvil_load(fixture_path("schema_blob_consumer.anvl"));
   anvil_schema tschema = tdoc ? anvil_schema_load(tdoc) : NULL;
   TestBit.is_not_null(tschema, "SCH66: type-based schema loaded");
   if (tschema) {
      TestBit.is_equal_int(0, (long long)anvil_schema_get_diagnostic_count(tschema),
                           "SCH66: no diagnostics");
      const char *data = "#!aml\nimage := @png`x`;\nbad_image := @jpg`x`;\nother := @jpg`x`;\n";
      anvil_document d = anvil_load_buffer(data, strlen(data));
      if (d) {
         TestBit.is_false(anvil_schema_validate(tschema, d), "SCH66: one tag mismatch");
         TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(tschema),
                              "SCH66: only `bad_image`");
         anvil_schema_violation v = anvil_schema_get_violation(tschema, 0);
         char path[32] = {0};
         anvil_schema_violation_get_field(v, path, sizeof(path));
         TestBit.is_true(strcmp(path, "bad_image") == 0 &&
                            anvil_schema_violation_get_category(v) == ANVIL_SCHEMA_ERR_VALIDATION_TAG,
                         "SCH66: TAG at bad_image (inherited from the type); `other` overrides it");
         anvil_dispose(d);
      }
      anvil_schema_dispose(tschema);
   }
   if (tdoc) {
      anvil_dispose(tdoc);
   }
}

static void test_sch67_blob_nested_and_nullable(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\n"
      "files := { type := Array; items := { type := Blob; tag := png; nullable := true; }; };\n",
      "#!aml\nfiles := [ @png`a`, null, @gif`b` ];\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH67: the @gif element fails");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH67: only the gif; the null is nullable");
      NESTED_PATH_IS(0, "files[2]", ANVIL_SCHEMA_ERR_VALIDATION_TAG, "SCH67: TAG at files[2]");
   } else {
      TestBit.is_not_null(schema, "SCH67: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-types-004 — format := iso8601_date | iso8601_datetime | uuid
 * ---------------------------------------------------------------------- */
// One data value against a one-field format schema: the violation category at `v`, 0 for none.
static int format_case(const char *format, const char *value) {
   char schema_src[256], data[512];
   snprintf(schema_src, sizeof(schema_src),
            "#!aml\n@[schema]\nv := { type := String; format := %s; };\n", format);
   snprintf(data, sizeof(data), "#!aml\nv := \"%s\";\n", value);
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(schema_src, data, &schema, &sdoc, &ddoc);
   int result = -1;
   if (schema) {
      if (ok) {
         result = 0;
      } else {
         result = (int)anvil_schema_get_violation_count(schema) == 1
                     ? (int)anvil_schema_violation_get_category(anvil_schema_get_violation(schema, 0))
                     : -2;
         char path[16] = {0};
         anvil_schema_violation_get_field(anvil_schema_get_violation(schema, 0), path, sizeof(path));
         if (strcmp(path, "v") != 0) {
            result = -3;
         }
      }
   }
   nested_free(schema, sdoc, ddoc);
   return result;
}

static void test_sch68_format_date(void) {
   int F = ANVIL_SCHEMA_ERR_VALIDATION_FORMAT;
   TestBit.is_equal_int(0, format_case("iso8601_date", "2026-02-28"), "SCH68: 2026-02-28 passes");
   TestBit.is_equal_int(F, format_case("iso8601_date", "2026-02-29"),
                        "SCH68: 2026-02-29 fails with exactly one FORMAT naming the field");
   TestBit.is_equal_int(0, format_case("iso8601_date", "2028-02-29"), "SCH68: leap day passes");
   TestBit.is_equal_int(F, format_case("iso8601_date", "2026-13-01"), "SCH68: month 13 fails");
   TestBit.is_equal_int(F, format_case("iso8601_date", "yesterday"), "SCH68: free text fails");
}

static void test_sch69_format_datetime(void) {
   int F = ANVIL_SCHEMA_ERR_VALIDATION_FORMAT;
   TestBit.is_equal_int(0, format_case("iso8601_datetime", "2026-10-25T14:03:00Z"), "SCH69: Z");
   TestBit.is_equal_int(0, format_case("iso8601_datetime", "2026-10-25T14:03:00.123-05:00"),
                        "SCH69: fraction and offset");
   TestBit.is_equal_int(F, format_case("iso8601_datetime", "2026-10-25 14:03:00Z"), "SCH69: space");
   TestBit.is_equal_int(F, format_case("iso8601_datetime", "2026-10-25T14:03:00"), "SCH69: no offset");
}

static void test_sch70_format_uuid(void) {
   int F = ANVIL_SCHEMA_ERR_VALIDATION_FORMAT;
   TestBit.is_equal_int(0, format_case("uuid", "0f8fad5b-d9cb-469f-a165-70867728950e"), "SCH70: ok");
   TestBit.is_equal_int(0, format_case("uuid", "0F8FAD5B-D9CB-469F-A165-70867728950E"), "SCH70: upper");
   TestBit.is_equal_int(F, format_case("uuid", "0f8fad5b-d9cb-469f-a165-70867728950"), "SCH70: 35 chars");
}

static void test_sch71_format_inherited_and_scope(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_format_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH71: schema loaded");
   if (schema) {
      TestBit.is_equal_int(0, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH71: no diagnostics");
      const char *src = "#!aml\ncreated_at := \"yesterday\";\nother := \"also bad\";\n"
                        "body := `2026-10-25T14:03:00Z`;\nbad_body := `nope`;\nn := 5;\n";
      anvil_document ddoc = anvil_load_buffer(src, strlen(src));
      if (ddoc) {
         TestBit.is_false(anvil_schema_validate(schema, ddoc), "SCH71: violations found");
         TestBit.is_equal_int(2, (long long)anvil_schema_get_violation_count(schema),
                              "SCH71: created_at (inherited) and bad_body (a blob's payload is "
                              "text); nothing for `other` (no format) or the numeric `n`");
         NESTED_PATH_IS(0, "created_at", ANVIL_SCHEMA_ERR_VALIDATION_FORMAT,
                        "SCH71: created_at inherits the type's format");
         NESTED_PATH_IS(1, "bad_body", ANVIL_SCHEMA_ERR_VALIDATION_FORMAT,
                        "SCH71: a blob's payload is checked");
         anvil_dispose(ddoc);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch72_unknown_format_is_a_diagnostic(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate("#!aml\n@[schema]\nv := { type := String; format := nope; };\n",
                             "#!aml\nv := \"anything\";\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_equal_int(1, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH72: one load diagnostic");
      anvil_schema_diagnostic d = anvil_schema_get_diagnostic(schema, 0);
      char path[16] = {0};
      anvil_schema_diagnostic_get_field(d, path, sizeof(path));
      TestBit.is_true(anvil_schema_diagnostic_get_category(d) == ANVIL_SCHEMA_DIAG_UNKNOWN_FORMAT &&
                         strcmp(path, "v") == 0,
                      "SCH72: UNKNOWN_FORMAT at v");
      TestBit.is_true(ok, "SCH72: and not a violation on every data document");
   } else {
      TestBit.is_not_null(schema, "SCH72: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch73_format_nested_and_nullable(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\n"
      "ids := { type := Array; items := { type := String; format := uuid; nullable := true; }; };\n",
      "#!aml\nids := [ \"0f8fad5b-d9cb-469f-a165-70867728950e\", null, \"nope\" ];\n",
      &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH73: the bad element fails");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema),
                           "SCH73: only ids[2]; the null is nullable");
      NESTED_PATH_IS(0, "ids[2]", ANVIL_SCHEMA_ERR_VALIDATION_FORMAT, "SCH73: FORMAT at ids[2]");
   } else {
      TestBit.is_not_null(schema, "SCH73: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
}


/* ---------------------------------------------------------------------- *
 * FR-2610-anvl-schema-005 — introspection
 * ---------------------------------------------------------------------- */
static void fname_is(anvil_schema_field f, const char *expected, const char *label) {
   char buf[64] = {0};
   size_t len = anvil_schema_field_get_name(f, buf, sizeof(buf));
   TestBit.is_true(f != NULL && strcmp(buf, expected) == 0 && len == strlen(expected), label);
}

static void test_sch74_fields_in_order_and_find(void) {
   anvil_document sdoc = anvil_load_buffer(
      "#!aml\n@[schema]\na := { type := Numeric; };\nb := { type := String; };\nc := { required := true; };\nnot_a_rule := 5;\n",
      strlen("#!aml\n@[schema]\na := { type := Numeric; };\nb := { type := String; };\nc := { required := true; };\nnot_a_rule := 5;\n"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH74: schema loaded");
   if (schema) {
      TestBit.is_equal_int(3, (long long)anvil_schema_get_field_count(schema),
                           "SCH74: three rules (the non-rule statement is skipped)");
      fname_is(anvil_schema_get_field(schema, 0), "a", "SCH74: first is a");
      fname_is(anvil_schema_get_field(schema, 1), "b", "SCH74: second is b");
      fname_is(anvil_schema_get_field(schema, 2), "c", "SCH74: third is c");
      TestBit.is_true(anvil_schema_find_field(schema, "b") == anvil_schema_get_field(schema, 1),
                      "SCH74: find_field(\"b\") is index 1's handle");
      TestBit.is_true(anvil_schema_find_field(schema, "nope") == NULL, "SCH74: find of a missing name");
      TestBit.is_true(anvil_schema_find_field(schema, "not_a_rule") == NULL,
                      "SCH74: a skipped statement is not a field");
      TestBit.is_true(anvil_schema_get_field(schema, 3) == NULL, "SCH74: out-of-bounds index");
      TestBit.is_equal_int(ANVIL_TYPE_NUMERIC, anvil_schema_field_get_kind(anvil_schema_get_field(schema, 0)),
                           "SCH74: a is Numeric");
      TestBit.is_equal_int(ANVIL_TYPE_UNKNOWN, anvil_schema_field_get_kind(anvil_schema_get_field(schema, 2)),
                           "SCH74: an untyped rule has no kind");
      TestBit.is_equal_int(0, (long long)anvil_schema_field_get_type_name(anvil_schema_get_field(schema, 2), NULL, 0),
                           "SCH74: and no type name");
      TestBit.is_true(!anvil_schema_field_is_required(anvil_schema_get_field(schema, 0)) &&
                         anvil_schema_field_is_required(anvil_schema_get_field(schema, 2)),
                      "SCH74: required reads back");
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch75_null_safety(void) {
   char buf[8] = "x";
   anvil_numeric_t n;
   long long ll;
   TestBit.is_equal_int(0, (long long)anvil_schema_get_field_count(NULL), "SCH75: count(NULL)");
   TestBit.is_true(anvil_schema_get_field(NULL, 0) == NULL, "SCH75: get_field(NULL)");
   TestBit.is_true(anvil_schema_find_field(NULL, "a") == NULL, "SCH75: find_field(NULL)");
   TestBit.is_equal_int(0, (long long)anvil_schema_field_get_name(NULL, buf, sizeof(buf)), "SCH75: name");
   TestBit.is_equal_int(ANVIL_TYPE_UNKNOWN, anvil_schema_field_get_kind(NULL), "SCH75: kind");
   TestBit.is_equal_int(0, (long long)anvil_schema_field_get_type_name(NULL, buf, sizeof(buf)), "SCH75: type name");
   TestBit.is_true(!anvil_schema_field_is_required(NULL) && !anvil_schema_field_is_nullable(NULL) &&
                      !anvil_schema_field_is_integer(NULL), "SCH75: flags");
   TestBit.is_true(!anvil_schema_field_get_size(NULL, &ll) && !anvil_schema_field_get_min_numeric(NULL, &n) &&
                      !anvil_schema_field_get_max_numeric(NULL, &n) &&
                      !anvil_schema_field_get_min_items(NULL, &ll) && !anvil_schema_field_get_max_items(NULL, &ll),
                   "SCH75: constraint getters");
   TestBit.is_equal_int(0, (long long)anvil_schema_field_get_value_count(NULL), "SCH75: value count");
   TestBit.is_equal_int(0, (long long)anvil_schema_field_get_value(NULL, 0, buf, sizeof(buf)), "SCH75: value");
   TestBit.is_equal_int(0, (long long)anvil_schema_field_get_format(NULL, buf, sizeof(buf)), "SCH75: format");
   TestBit.is_equal_int(0, (long long)anvil_schema_field_get_tag(NULL, buf, sizeof(buf)), "SCH75: tag");
   TestBit.is_equal_int(0, (long long)anvil_schema_field_get_nested_count(NULL), "SCH75: nested count");
   TestBit.is_true(anvil_schema_field_get_nested(NULL, 0) == NULL && anvil_schema_field_find_nested(NULL, "a") == NULL &&
                      anvil_schema_field_get_items(NULL) == NULL, "SCH75: nested/items");
   TestBit.is_equal_int(0, (long long)anvil_schema_field_get_extra_count(NULL), "SCH75: extra count");
   TestBit.is_equal_int(0, (long long)anvil_schema_field_get_extra_name(NULL, 0, buf, sizeof(buf)), "SCH75: extra name");
   TestBit.is_true(anvil_schema_field_get_extra_value(NULL, 0) == NULL, "SCH75: extra value");
}

static void test_sch76_constraints_read_back_as_enforced(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_introspect_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH76: schema loaded");
   if (schema) {
      char buf[64];
      long long ll = 0;
      anvil_numeric_t n;

      anvil_schema_field id = anvil_schema_find_field(schema, "id");
      TestBit.is_equal_int(ANVIL_TYPE_NUMERIC, anvil_schema_field_get_kind(id), "SCH76: id kind from its type");
      anvil_schema_field_get_type_name(id, buf, sizeof(buf));
      TestBit.is_true(strcmp(buf, "types.Id32") == 0, "SCH76: id's type name as written");
      TestBit.is_true(anvil_schema_field_is_required(id) && anvil_schema_field_is_integer(id) &&
                         !anvil_schema_field_is_nullable(id), "SCH76: required inline, integer inherited");
      TestBit.is_true(anvil_schema_field_get_min_numeric(id, &n) && n.is_integral && n.as_int64 == 0,
                      "SCH76: inherited min");
      TestBit.is_true(anvil_schema_field_get_max_numeric(id, &n) && n.as_int64 == 2147483647,
                      "SCH76: inherited max");
      TestBit.is_false(anvil_schema_field_get_size(id, &ll), "SCH76: undeclared size is not a default");

      anvil_schema_field status = anvil_schema_find_field(schema, "status");
      TestBit.is_equal_int(ANVIL_TYPE_ENUM, anvil_schema_field_get_kind(status), "SCH76: enum kind");
      TestBit.is_true(anvil_schema_field_is_nullable(status), "SCH76: nullable inline");
      TestBit.is_equal_int(2, (long long)anvil_schema_field_get_value_count(status), "SCH76: two inherited values");
      TestBit.is_equal_int(6, (long long)anvil_schema_field_get_value(status, 1, buf, sizeof(buf)),
                           "SCH76: value 1 length");
      TestBit.is_true(strcmp(buf, "closed") == 0, "SCH76: value 1 is closed");
      TestBit.is_equal_int(0, (long long)anvil_schema_field_get_value(status, 2, buf, sizeof(buf)),
                           "SCH76: value index out of bounds");

      anvil_schema_field created = anvil_schema_find_field(schema, "created");
      TestBit.is_equal_int(16, (long long)anvil_schema_field_get_format(created, buf, sizeof(buf)),
                           "SCH76: inherited format length");
      TestBit.is_true(strcmp(buf, "iso8601_datetime") == 0, "SCH76: inherited format");

      anvil_schema_field img = anvil_schema_find_field(schema, "img");
      TestBit.is_equal_int(ANVIL_TYPE_BLOB, anvil_schema_field_get_kind(img), "SCH76: blob kind");
      anvil_schema_field_get_tag(img, buf, sizeof(buf));
      TestBit.is_true(strcmp(buf, "png") == 0, "SCH76: inherited tag");
      TestBit.is_true(anvil_schema_field_get_size(img, &ll) && ll == 100, "SCH76: inline size");

      anvil_schema_field score = anvil_schema_find_field(schema, "score");
      TestBit.is_true(anvil_schema_field_get_min_numeric(score, &n) && !n.is_integral && n.as_double == 0.5,
                      "SCH76: fractional min");
      TestBit.is_true(anvil_schema_field_get_max_numeric(score, &n) && n.as_double == 1000.0,
                      "SCH76: exponent max");

      anvil_schema_field labels = anvil_schema_find_field(schema, "labels");
      TestBit.is_true(anvil_schema_field_get_min_items(labels, &ll) && ll == 1, "SCH76: min_items");
      TestBit.is_true(anvil_schema_field_get_max_items(labels, &ll) && ll == 5, "SCH76: max_items");
      TestBit.is_false(anvil_schema_field_get_min_items(score, &ll), "SCH76: undeclared min_items");

      // Agreement with validation: what reads back as min is what validation enforces.
      const char *bad = "#!aml\nid := -1;\n";
      anvil_document ddoc = anvil_load_buffer(bad, strlen(bad));
      if (ddoc) {
         anvil_schema_validate(schema, ddoc);
         char path[16] = {0};
         anvil_schema_violation_get_field(anvil_schema_get_violation(schema, 0), path, sizeof(path));
         TestBit.is_true(strcmp(path, "id") == 0 &&
                            anvil_schema_violation_get_category(anvil_schema_get_violation(schema, 0)) ==
                               ANVIL_SCHEMA_ERR_VALIDATION_RANGE,
                         "SCH76: -1 violates the min that introspection reported");
         anvil_dispose(ddoc);
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch77_extension_keys(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_introspect_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH77: schema loaded");
   if (schema) {
      TestBit.is_equal_int(0, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH77: extension keys are not diagnostics");
      anvil_schema_field id = anvil_schema_find_field(schema, "id");
      TestBit.is_equal_int(2, (long long)anvil_schema_field_get_extra_count(id),
                           "SCH77: pooled and mask are the extras (type/required are not)");
      char name[32] = {0};
      anvil_schema_field_get_extra_name(id, 0, name, sizeof(name));
      TestBit.is_true(strcmp(name, "pooled") == 0, "SCH77: first extra is pooled");
      anvil_value pooled = anvil_schema_field_get_extra_value(id, 0);
      char text[32] = {0};
      anvil_value_get_text(pooled, text, sizeof(text));
      TestBit.is_true(pooled && anvil_value_get_type(pooled) == ANVIL_VALUE_BOOL && strcmp(text, "true") == 0,
                      "SCH77: pooled := true is a readable BOOL");
      anvil_schema_field_get_extra_name(id, 1, name, sizeof(name));
      TestBit.is_true(strcmp(name, "mask") == 0, "SCH77: second extra is mask");
      anvil_value mask = anvil_schema_field_get_extra_value(id, 1);
      TestBit.is_true(mask && anvil_value_get_type(mask) == ANVIL_VALUE_ARRAY &&
                         anvil_value_get_count(mask) == 3, "SCH77: mask is a 3-element ARRAY");
      anvil_numeric_t third = anvil_value_get_numeric(anvil_value_get_element(mask, 2));
      TestBit.is_true(third.is_integral && third.as_int64 == 4, "SCH77: mask[2] reads as 4");
      TestBit.is_true(anvil_schema_field_get_extra_value(id, 2) == NULL, "SCH77: extra index out of bounds");

      anvil_schema_field bare = anvil_schema_find_field(schema, "bare");
      TestBit.is_equal_int(1, (long long)anvil_schema_field_get_extra_count(bare),
                           "SCH77: an untyped FlyWire-style rule is all extras");
      TestBit.is_equal_int(ANVIL_TYPE_UNKNOWN, anvil_schema_field_get_kind(bare), "SCH77: with no kind");

      anvil_schema_field items = anvil_schema_field_get_items(anvil_schema_find_field(schema, "labels"));
      TestBit.is_equal_int(1, (long long)anvil_schema_field_get_extra_count(items),
                           "SCH77: extras on a nested (items) rule too");
      anvil_value note = anvil_schema_field_get_extra_value(items, 0);
      anvil_value_get_text(note, text, sizeof(text));
      TestBit.is_true(note && anvil_value_get_type(note) == ANVIL_VALUE_STRING && strcmp(text, "item") == 0,
                      "SCH77: a string extra keeps its content (unquoted)");
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch78_nested_rules_reachable(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_introspect_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH78: schema loaded");
   if (schema) {
      anvil_schema_field owner = anvil_schema_find_field(schema, "owner");
      TestBit.is_equal_int(2, (long long)anvil_schema_field_get_nested_count(owner), "SCH78: two nested fields");
      fname_is(anvil_schema_field_get_nested(owner, 0), "uid", "SCH78: first nested is uid");
      fname_is(anvil_schema_field_get_nested(owner, 1), "nick", "SCH78: second nested is nick");
      anvil_schema_field uid = anvil_schema_field_find_nested(owner, "uid");
      TestBit.is_true(uid == anvil_schema_field_get_nested(owner, 0) && anvil_schema_field_is_required(uid),
                      "SCH78: find_nested and nested rule flags");
      TestBit.is_true(anvil_schema_field_find_nested(owner, "zzz") == NULL, "SCH78: missing nested name");
      TestBit.is_true(anvil_schema_field_get_nested(owner, 2) == NULL, "SCH78: nested index out of bounds");
      TestBit.is_equal_int(0, (long long)anvil_schema_field_get_nested_count(anvil_schema_find_field(schema, "name")),
                           "SCH78: a scalar rule has no nested fields");

      anvil_schema_field labels = anvil_schema_find_field(schema, "labels");
      anvil_schema_field items = anvil_schema_field_get_items(labels);
      TestBit.is_not_null(items, "SCH78: the Array rule exposes its items rule");
      TestBit.is_equal_int(ANVIL_TYPE_STRING, anvil_schema_field_get_kind(items), "SCH78: items kind");
      TestBit.is_equal_int(2, (long long)anvil_schema_field_get_value_count(items), "SCH78: items values");
      TestBit.is_equal_int(0, (long long)anvil_schema_field_get_name(items, NULL, 0), "SCH78: items rule name is empty");
      TestBit.is_true(anvil_schema_field_get_items(anvil_schema_find_field(schema, "name")) == NULL,
                      "SCH78: a scalar rule has no items rule");
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch79_introspection_outlives_the_documents(void) {
   anvil_document sdoc = anvil_load(fixture_path("schema_introspect_consumer.anvl"));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   if (sdoc) {
      anvil_dispose(sdoc); // the schema must not lean on it
   }
   TestBit.is_not_null(schema, "SCH79: schema loaded");
   if (schema) {
      anvil_schema_field id = anvil_schema_find_field(schema, "id");
      anvil_value mask = anvil_schema_field_get_extra_value(id, 1);
      char buf[32] = {0};
      TestBit.is_true(mask && anvil_value_get_count(mask) == 3, "SCH79: extras readable after disposal");
      anvil_schema_field_get_type_name(id, buf, sizeof(buf));
      TestBit.is_true(strcmp(buf, "types.Id32") == 0, "SCH79: type name readable");
      anvil_schema_field_get_format(anvil_schema_find_field(schema, "created"), buf, sizeof(buf));
      TestBit.is_true(strcmp(buf, "iso8601_datetime") == 0, "SCH79: format readable");
      anvil_schema_field_get_value(anvil_schema_find_field(schema, "status"), 0, buf, sizeof(buf));
      TestBit.is_true(strcmp(buf, "open") == 0, "SCH79: values readable");
      anvil_schema_dispose(schema);
   }
}

static void test_sch80_extra_value_kinds(void) {
   const char *src =
      "#!aml\n@[schema]\n"
      "f := { n := 7; s := \"a b\\\"c\"; b := @png`bytes here`; o := { k := 1; }; t := ( 1, 2 ); z := null; w := bareword; };\n";
   anvil_document sdoc = anvil_load_buffer(src, strlen(src));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   if (sdoc) {
      anvil_dispose(sdoc);
   }
   TestBit.is_not_null(schema, "SCH80: schema loaded");
   if (schema) {
      anvil_schema_field f = anvil_schema_find_field(schema, "f");
      TestBit.is_equal_int(7, (long long)anvil_schema_field_get_extra_count(f), "SCH80: seven extras");
      const anvil_value_type want[] = {ANVIL_VALUE_NUMERIC, ANVIL_VALUE_STRING, ANVIL_VALUE_BLOB,
                                       ANVIL_VALUE_OBJECT, ANVIL_VALUE_TUPLE, ANVIL_VALUE_NULL,
                                       ANVIL_VALUE_BARE};
      for (size_t i = 0; i < 7; i++) {
         anvil_value v = anvil_schema_field_get_extra_value(f, i);
         TestBit.is_true(v && anvil_value_get_type(v) == want[i], "SCH80: each extra keeps its kind");
      }
      char tag[16] = {0}, bytes[32] = {0};
      anvil_value b = anvil_schema_field_get_extra_value(f, 2);
      anvil_value_get_blob_tag(b, tag, sizeof(tag));
      size_t blen = anvil_value_get_bytes(b, bytes, sizeof(bytes));
      TestBit.is_true(strcmp(tag, "png") == 0 && blen == 10 && memcmp(bytes, "bytes here", 10) == 0,
                      "SCH80: a blob extra keeps its tag and payload");
      char sbytes[32] = {0};
      anvil_value_get_bytes(anvil_schema_field_get_extra_value(f, 1), sbytes, sizeof(sbytes));
      TestBit.is_true(strcmp(sbytes, "a b\\\"c") == 0, "SCH80: a string extra keeps its escapes verbatim");
      TestBit.is_equal_int(1, (long long)anvil_value_get_count(anvil_schema_field_get_extra_value(f, 3)),
                           "SCH80: an object extra keeps its members");
      anvil_schema_dispose(schema);
   }
}


/* ---------------------------------------------------------------------- *
 * SR-2610-anvl-docs-001 item 4 — pin the documented constraint semantics
 * ---------------------------------------------------------------------- */
static void test_sch81_size_counts_bytes_and_schemas_are_closed(void) {
   const char *src = "#!aml\n@[schema]\nname := { type := String; size := 5; };\n";
   anvil_schema schema; anvil_document sdoc, ddoc;
   // "héllo" is 5 characters but 6 UTF-8 bytes (é is 2): size counts bytes.
   bool ok = nested_validate(src, "#!aml\nname := \"h\xC3\xA9llo\";\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH81: 5 characters / 6 bytes exceeds size := 5");
      NESTED_PATH_IS(0, "name", ANVIL_SCHEMA_ERR_VALIDATION_SIZE, "SCH81: SIZE at name");
   } else {
      TestBit.is_not_null(schema, "SCH81: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
   ok = nested_validate(src, "#!aml\nname := \"hello\";\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema != NULL && ok, "SCH81: 5 ASCII bytes fit");
   nested_free(schema, sdoc, ddoc);
   // Closed by default: an undeclared field is a violation unless the caller opts out.
   ok = nested_validate(src, "#!aml\nname := \"ok\";\nextra := 1;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH81: unknown data fields are violations by default");
      NESTED_PATH_IS(0, "extra", ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD, "SCH81: UNKNOWN_FIELD");
   }
   nested_free(schema, sdoc, ddoc);
}


struct vt_entry {
   const char *flat_name;
   const void *flat_fn;
   const void *vtable_member;
};
#define VT_MEMBER(flat, member) {#flat, (const void *)flat, (const void *)(member)}

// Every function declared in `header` must have a vtable member (pointer-identical to it), and
// the table must not name one the header no longer declares: a function added to the flat header
// without a member fails here.
static void check_vtable_complete(const char *header, const struct vt_entry *table, size_t table_len,
                                  const char *label) {
   char *text = test_read_whole_file(header);
   TestBit.is_not_null(text, label);
   if (!text) {
      return;
   }
   static char names[256][96];
   size_t found = test_header_function_names(text, names, 256);
   free(text);
   TestBit.is_true(found > 10, label);
   for (size_t i = 0; i < found; i++) {
      const struct vt_entry *entry = NULL;
      for (size_t j = 0; j < table_len; j++) {
         if (strcmp(table[j].flat_name, names[i]) == 0) {
            entry = &table[j];
         }
      }
      if (!entry) {
         printf("    %s: %s is declared but has no vtable member\n", label, names[i]);
      }
      TestBit.is_true(entry != NULL && entry->vtable_member != NULL &&
                         entry->vtable_member == entry->flat_fn, names[i]);
   }
   for (size_t j = 0; j < table_len; j++) {
      bool declared = false;
      for (size_t i = 0; i < found; i++) {
         declared = declared || strcmp(table[j].flat_name, names[i]) == 0;
      }
      TestBit.is_true(declared, table[j].flat_name);
   }
}

static void test_sch82_schema_vtable_matches_flat_and_is_complete(void) {
   const struct vt_entry table[] = {
      VT_MEMBER(anvil_schema_load, Schema.load),
      VT_MEMBER(anvil_schema_dispose, Schema.dispose),
      VT_MEMBER(anvil_schema_validate, Schema.validate),
      VT_MEMBER(anvil_schema_validate_value, Schema.validate_value),
      VT_MEMBER(anvil_schema_get_violation_count, Schema.get_violation_count),
      VT_MEMBER(anvil_schema_get_violation, Schema.get_violation),
      VT_MEMBER(anvil_schema_violation_get_category, Schema.violation_get_category),
      VT_MEMBER(anvil_schema_violation_get_field, Schema.violation_get_field),
      VT_MEMBER(anvil_schema_violation_get_message, Schema.violation_get_message),
      VT_MEMBER(anvil_schema_get_diagnostic_count, Schema.get_diagnostic_count),
      VT_MEMBER(anvil_schema_get_diagnostic, Schema.get_diagnostic),
      VT_MEMBER(anvil_schema_diagnostic_get_category, Schema.diagnostic_get_category),
      VT_MEMBER(anvil_schema_diagnostic_get_type_category, Schema.diagnostic_get_type_category),
      VT_MEMBER(anvil_schema_diagnostic_get_field, Schema.diagnostic_get_field),
      VT_MEMBER(anvil_schema_diagnostic_get_message, Schema.diagnostic_get_message),
      VT_MEMBER(anvil_schema_get_field_count, Schema.get_field_count),
      VT_MEMBER(anvil_schema_get_field, Schema.get_field),
      VT_MEMBER(anvil_schema_find_field, Schema.find_field),
      VT_MEMBER(anvil_schema_field_get_name, Schema.field_get_name),
      VT_MEMBER(anvil_schema_field_get_kind, Schema.field_get_kind),
      VT_MEMBER(anvil_schema_field_get_type_name, Schema.field_get_type_name),
      VT_MEMBER(anvil_schema_field_is_required, Schema.field_is_required),
      VT_MEMBER(anvil_schema_field_is_nullable, Schema.field_is_nullable),
      VT_MEMBER(anvil_schema_field_is_integer, Schema.field_is_integer),
      VT_MEMBER(anvil_schema_field_get_size, Schema.field_get_size),
      VT_MEMBER(anvil_schema_field_get_min_numeric, Schema.field_get_min_numeric),
      VT_MEMBER(anvil_schema_field_get_max_numeric, Schema.field_get_max_numeric),
      VT_MEMBER(anvil_schema_field_get_min_items, Schema.field_get_min_items),
      VT_MEMBER(anvil_schema_field_get_max_items, Schema.field_get_max_items),
      VT_MEMBER(anvil_schema_field_get_value_count, Schema.field_get_value_count),
      VT_MEMBER(anvil_schema_field_get_value, Schema.field_get_value),
      VT_MEMBER(anvil_schema_field_get_format, Schema.field_get_format),
      VT_MEMBER(anvil_schema_field_get_tag, Schema.field_get_tag),
      VT_MEMBER(anvil_schema_field_get_nested_count, Schema.field_get_nested_count),
      VT_MEMBER(anvil_schema_field_get_nested, Schema.field_get_nested),
      VT_MEMBER(anvil_schema_field_find_nested, Schema.field_find_nested),
      VT_MEMBER(anvil_schema_field_get_items, Schema.field_get_items),
      VT_MEMBER(anvil_schema_field_get_extra_count, Schema.field_get_extra_count),
      VT_MEMBER(anvil_schema_field_get_extra_name, Schema.field_get_extra_name),
      VT_MEMBER(anvil_schema_field_get_extra_value, Schema.field_get_extra_value),
   };
   check_vtable_complete("../../include/anvil_schema.h", table, sizeof(table) / sizeof(table[0]),
                         "SCH82");
}

// A load -> validate -> read-violations round trip through `Schema` only (no flat calls).
static void test_sch83_schema_vtable_round_trip(void) {
   if (!Schema.load || !Schema.validate || !Schema.get_violation) {
      TestBit.is_true(false, "SCH83: the Schema vtable is populated");
      return;
   }
   const char *schema_src = "#!aml\n@[schema]\nn := { type := Numeric; max := 3; };\n"
                            "s := { type := String; pooled := true; };\n";
   const char *data_src = "#!aml\nn := 5;\nzz := 1;\n";
   anvil_document sdoc = Anvil.load_buffer(schema_src, strlen(schema_src));
   anvil_document ddoc = Anvil.load_buffer(data_src, strlen(data_src));
   anvil_schema schema = Schema.load(sdoc);
   TestBit.is_not_null(schema, "SCH83: Schema.load");
   if (schema) {
      TestBit.is_false(Schema.validate(schema, ddoc), "SCH83: Schema.validate fails the document");
      TestBit.is_equal_int(2, (long long)Schema.get_violation_count(schema), "SCH83: two violations");
      char path[16] = {0};
      anvil_schema_violation v = Schema.get_violation(schema, 0);
      Schema.violation_get_field(v, path, sizeof(path));
      TestBit.is_true(strcmp(path, "n") == 0 &&
                         Schema.violation_get_category(v) == ANVIL_SCHEMA_ERR_VALIDATION_RANGE,
                      "SCH83: RANGE at n");
      TestBit.is_equal_int(2, (long long)Schema.get_field_count(schema), "SCH83: two rules");
      anvil_schema_field s = Schema.find_field(schema, "s");
      TestBit.is_equal_int(1, (long long)Schema.field_get_extra_count(s), "SCH83: introspection and extras");
      Schema.dispose(schema);
   }
   Anvil.dispose(sdoc);
   Anvil.dispose(ddoc);
}


/* ---------------------------------------------------------------------- *
 * BR-2610-anvl-registry-001 part 2 - identical text across independently loaded documents
 * ---------------------------------------------------------------------- */
// The anvil.net repro in C: ValidateField(text) while a document of the same text is alive.
static void test_sch84_validate_while_same_text_document_is_alive(void) {
   const char *schema_src = "#!aml\n@[schema]\nn := { type := Numeric; max := 3; };\n";
   const char *data_src = "#!aml\nset := { n := 5; };\n";
   anvil_document sdoc = anvil_load_buffer(schema_src, strlen(schema_src));
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   anvil_document held = anvil_load_buffer(data_src, strlen(data_src)); // the caller's own copy
   TestBit.is_true(schema && held && !anvil_has_errors(held), "SCH84: schema and the held document");
   if (schema && held) {
      // "ValidateField(text)": parse the same text again and validate its value.
      anvil_document again = anvil_load_buffer(data_src, strlen(data_src));
      TestBit.is_true(again && !anvil_has_errors(again),
                      "SCH84: the same text loads again while the first copy is alive");
      if (again) {
         anvil_statement st = anvil_document_find_statement(again, "set");
         anvil_value set = st ? anvil_statement_get_value(st) : NULL;
         TestBit.is_false(anvil_schema_validate_value(schema, set, NULL), "SCH84: and validates (fails range)");
         TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema), "SCH84: one violation");
         anvil_dispose(again);
      }
      anvil_dispose(held);
      held = NULL;
   }
   if (held) {
      anvil_dispose(held);
   }
   if (schema) {
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

// Extension-key extras are copied into fragment documents; two with identical text (a very common
// `pooled := true`) must both be readable, in one schema and across two schemas.
static void test_sch85_identical_extras_are_all_readable(void) {
   const char *src = "#!aml\n@[schema]\n"
                     "a := { type := Numeric; pooled := true; tags := [ 1, 2 ]; };\n"
                     "b := { type := Numeric; pooled := true; tags := [ 1, 2 ]; };\n";
   anvil_document d1 = anvil_load_buffer(src, strlen(src));
   anvil_schema s1 = d1 ? anvil_schema_load(d1) : NULL;
   anvil_document d2 = anvil_load(fixture_path("schema_introspect_consumer.anvl"));
   anvil_schema s2 = d2 ? anvil_schema_load(d2) : NULL;
   anvil_document d3 = anvil_load_buffer(src, strlen(src)); // identical to d1, d1 still alive
   anvil_schema s3 = d3 ? anvil_schema_load(d3) : NULL;
   TestBit.is_true(s1 && s2 && s3, "SCH85: three schemas, two with identical text, all alive");
   if (s1 && s3) {
      for (int which = 0; which < 2; which++) {
         anvil_schema sc = which ? s3 : s1;
         const char *fields[] = {"a", "b"};
         for (int f = 0; f < 2; f++) {
            anvil_schema_field fld = anvil_schema_find_field(sc, fields[f]);
            anvil_value pooled = anvil_schema_field_get_extra_value(fld, 0);
            anvil_value tags = anvil_schema_field_get_extra_value(fld, 1);
            TestBit.is_true(pooled && anvil_value_get_type(pooled) == ANVIL_VALUE_BOOL &&
                               tags && anvil_value_get_count(tags) == 2,
                            "SCH85: every identical extra is readable (none silently NULL)");
         }
      }
   }
   if (s3) anvil_schema_dispose(s3);
   if (d3) anvil_dispose(d3);
   if (s2) anvil_schema_dispose(s2);
   if (d2) anvil_dispose(d2);
   if (s1) anvil_schema_dispose(s1);
   if (d1) anvil_dispose(d1);
}

/* ---------------------------------------------------------------------- *
 * BR-2610-anvl-nul-001 - strings containing a NUL are carried exactly through the schema
 * ---------------------------------------------------------------------- */
#define LOAD_LIT(lit) anvil_load_buffer((lit), sizeof(lit) - 1)

static bool bytes_are(anvil_schema_field f, size_t idx, const char *want, size_t want_len) {
   char buf[16];
   memset(buf, 0x7f, sizeof(buf));
   size_t n = anvil_schema_field_get_value(f, idx, buf, sizeof(buf));
   return n == want_len && memcmp(buf, want, want_len) == 0 && buf[want_len] == '\0';
}

static void test_sch86_enum_labels_with_nul(void) {
   anvil_document sdoc = LOAD_LIT(
      "#!aml\n@[schema]\nx := { type := enum; values := [ ok, \"a\0b\" ]; };\n");
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH86: schema loaded");
   if (schema) {
      anvil_schema_field x = anvil_schema_find_field(schema, "x");
      TestBit.is_equal_int(3, (long long)anvil_schema_field_get_value(x, 1, NULL, 0),
                           "SCH86: the inline label's length is 3, not 1");
      TestBit.is_true(bytes_are(x, 1, "a\0b", 3), "SCH86: inline label bytes 61 00 62");
      TestBit.is_true(bytes_are(x, 0, "ok", 2), "SCH86: ordinary labels unaffected");
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
   // Inherited from a type: the type's copy is length-carried, and so is the field's copy of it.
   anvil_document tdoc = LOAD_LIT("#!aml\n@[types]\nT := { type := enum; values := [ ok, \"a\0b\" ]; };\n");
   anvil_document cdoc = anvil_load(fixture_path("schema_nul_consumer.anvl"));
   anvil_schema inherited = cdoc ? anvil_schema_load(cdoc) : NULL;
   TestBit.is_not_null(inherited, "SCH86: schema with an inherited NUL label loaded");
   if (inherited) {
      anvil_schema_field y = anvil_schema_find_field(inherited, "y");
      TestBit.is_equal_int(2, (long long)anvil_schema_field_get_value_count(y), "SCH86: two inherited labels");
      TestBit.is_true(bytes_are(y, 1, "a\0b", 3), "SCH86: the inherited label keeps its NUL");
      anvil_schema_dispose(inherited);
   }
   if (cdoc) anvil_dispose(cdoc);
   if (tdoc) anvil_dispose(tdoc);
}

static void test_sch87_values_membership_is_by_length(void) {
   const char *want_ok = "SCH87";
   (void)want_ok;
   anvil_document s1 = LOAD_LIT("#!aml\n@[schema]\nv := { type := String; values := [ \"a\" ]; };\n");
   anvil_document s2 = LOAD_LIT("#!aml\n@[schema]\nv := { type := String; values := [ \"a\0b\" ]; };\n");
   anvil_schema only_a = s1 ? anvil_schema_load(s1) : NULL;
   anvil_schema a_nul_b = s2 ? anvil_schema_load(s2) : NULL;
   TestBit.is_true(only_a && a_nul_b, "SCH87: both schemas loaded");
   if (only_a && a_nul_b) {
      anvil_document d_a = LOAD_LIT("#!aml\nv := \"a\";\n");
      anvil_document d_anb = LOAD_LIT("#!aml\nv := \"a\0b\";\n");
      anvil_document d_anc = LOAD_LIT("#!aml\nv := \"a\0c\";\n");
      TestBit.is_true(anvil_schema_validate(only_a, d_a), "SCH87: values [a] accepts \"a\"");
      TestBit.is_false(anvil_schema_validate(only_a, d_anb),
                       "SCH87: values [a] REJECTS \"a<NUL>b\" (no false accept by prefix)");
      TestBit.is_true(anvil_schema_validate(a_nul_b, d_anb), "SCH87: values [a<NUL>b] accepts \"a<NUL>b\"");
      TestBit.is_false(anvil_schema_validate(a_nul_b, d_a), "SCH87: and rejects \"a\" (no false accept)");
      TestBit.is_false(anvil_schema_validate(a_nul_b, d_anc),
                       "SCH87: and rejects \"a<NUL>c\" (same length, differs after the NUL)");
      if (d_a) anvil_dispose(d_a);
      if (d_anb) anvil_dispose(d_anb);
      if (d_anc) anvil_dispose(d_anc);
   }
   if (only_a) anvil_schema_dispose(only_a);
   if (a_nul_b) anvil_schema_dispose(a_nul_b);
   if (s1) anvil_dispose(s1);
   if (s2) anvil_dispose(s2);
}

static void test_sch88_extras_with_nul(void) {
   anvil_document sdoc = LOAD_LIT(
      "#!aml\n@[schema]\n"
      "x := { type := String; note := \"a\0b\"; plain := \"ab\"; list := [ \"p\0q\", 2 ]; "
      "blob := @t`x\0y`; nested := { k := \"m\0n\"; }; };\n");
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH88: schema loaded");
   if (schema) {
      anvil_schema_field x = anvil_schema_find_field(schema, "x");
      TestBit.is_equal_int(5, (long long)anvil_schema_field_get_extra_count(x), "SCH88: five extras");
      char buf[16];
      anvil_value note = anvil_schema_field_get_extra_value(x, 0);
      TestBit.is_true(note && anvil_value_get_type(note) == ANVIL_VALUE_STRING,
                      "SCH88: a string extra containing a NUL is a STRING (it used to be NULL-typed)");
      size_t n = note ? anvil_value_get_bytes(note, buf, sizeof(buf)) : 0;
      TestBit.is_true(n == 3 && memcmp(buf, "a\0b", 3) == 0, "SCH88: with bytes 61 00 62");
      n = note ? anvil_value_get_text(note, buf, sizeof(buf)) : 0;
      TestBit.is_true(n == 3 && memcmp(buf, "a\0b", 3) == 0, "SCH88: and text 61 00 62");
      anvil_value plain = anvil_schema_field_get_extra_value(x, 1);
      TestBit.is_true(plain && anvil_value_get_type(plain) == ANVIL_VALUE_STRING &&
                         anvil_value_get_text(plain, buf, sizeof(buf)) == 2, "SCH88: \"ab\" still works");
      anvil_value list = anvil_schema_field_get_extra_value(x, 2);
      TestBit.is_true(list && anvil_value_get_type(list) == ANVIL_VALUE_ARRAY && anvil_value_get_count(list) == 2,
                      "SCH88: an array extra whose element holds a NUL survives");
      anvil_value el = list ? anvil_value_get_element(list, 0) : NULL;
      n = el ? anvil_value_get_bytes(el, buf, sizeof(buf)) : 0;
      TestBit.is_true(n == 3 && memcmp(buf, "p\0q", 3) == 0, "SCH88: and the element keeps its NUL");
      anvil_value blob = anvil_schema_field_get_extra_value(x, 3);
      n = blob ? anvil_value_get_bytes(blob, buf, sizeof(buf)) : 0;
      TestBit.is_true(blob && anvil_value_get_type(blob) == ANVIL_VALUE_BLOB && n == 3 &&
                         memcmp(buf, "x\0y", 3) == 0, "SCH88: a blob extra keeps its NUL payload");
      anvil_value nested = anvil_schema_field_get_extra_value(x, 4);
      TestBit.is_true(nested && anvil_value_get_type(nested) == ANVIL_VALUE_OBJECT && anvil_value_get_count(nested) == 1,
                      "SCH88: an object extra with a NUL string survives");
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

static void test_sch89_nul_in_identifier_positions_is_malformed(void) {
   anvil_document sdoc = LOAD_LIT(
      "#!aml\n@[schema]\n"
      "a := { type := \"String\0x\"; };\n"
      "b := { type := String; format := \"uuid\0x\"; };\n"
      "c := { type := Blob; tag := \"png\0x\"; };\n"
      "d := { type := Numeric; required := \"true\0x\"; nullable := \"true\0x\"; integer := \"true\0x\"; };\n");
   anvil_schema schema = sdoc ? anvil_schema_load(sdoc) : NULL;
   TestBit.is_not_null(schema, "SCH89: schema loaded");
   if (schema) {
      TestBit.is_equal_int(0, (long long)anvil_schema_field_get_kind(anvil_schema_find_field(schema, "a")),
                           "SCH89: `String<NUL>x` is not the String type");
      char buf[16];
      TestBit.is_equal_int(0, (long long)anvil_schema_field_get_format(anvil_schema_find_field(schema, "b"), buf, sizeof(buf)),
                           "SCH89: `uuid<NUL>x` is not the uuid format");
      TestBit.is_equal_int(0, (long long)anvil_schema_field_get_tag(anvil_schema_find_field(schema, "c"), buf, sizeof(buf)),
                           "SCH89: a tag with a NUL is not declared");
      anvil_schema_field d = anvil_schema_find_field(schema, "d");
      TestBit.is_true(!anvil_schema_field_is_required(d) && !anvil_schema_field_is_nullable(d) &&
                         !anvil_schema_field_is_integer(d), "SCH89: `true<NUL>x` is not true");
      TestBit.is_equal_int(6, (long long)anvil_schema_get_diagnostic_count(schema),
                           "SCH89: each is diagnosed (type, format, tag, required, nullable, integer)");
      for (size_t i = 0; i < anvil_schema_get_diagnostic_count(schema); i++) {
         char msg[256] = {0};
         anvil_schema_diagnostic_get_message(anvil_schema_get_diagnostic(schema, i), msg, sizeof(msg));
         TestBit.is_true(strstr(msg, "NUL") != NULL, "SCH89: and the message says the value contains a NUL");
      }
      anvil_schema_dispose(schema);
   }
   if (sdoc) {
      anvil_dispose(sdoc);
   }
}

// A type's nested `fields` are stored as source text and re-parsed with a length; a NUL inside a
// nested string must not truncate that text.
static void test_sch90_type_fields_text_with_nul(void) {
   anvil_document cdoc = anvil_load(fixture_path("schema_nul_fields_consumer.anvl"));
   anvil_schema schema = cdoc ? anvil_schema_load(cdoc) : NULL;
   TestBit.is_not_null(schema, "SCH90: schema loaded");
   if (schema) {
      TestBit.is_equal_int(0, (long long)anvil_schema_get_diagnostic_count(schema), "SCH90: no diagnostics");
      anvil_schema_field owner = anvil_schema_find_field(schema, "owner");
      anvil_schema_field kind = anvil_schema_field_find_nested(owner, "kind");
      TestBit.is_not_null(kind, "SCH90: the nested rule after the NUL-bearing one is present");
      TestBit.is_true(kind && bytes_are(kind, 1, "a\0b", 3), "SCH90: the nested enum label keeps its NUL");
      anvil_schema_field after = anvil_schema_field_find_nested(owner, "after");
      TestBit.is_not_null(after, "SCH90: and the rules declared after it were not cut off");
      anvil_schema_dispose(schema);
   }
   if (cdoc) anvil_dispose(cdoc);
}


// The pure-identifier positions (a statement name, a rule key, a bare word) cannot hold a NUL at all:
// the core rejects the document, so the registry and schema never see one. Pinned so a change there
// is noticed.
static void test_sch91_nul_in_names_and_bare_words_is_a_load_error(void) {
   anvil_document a = LOAD_LIT("#!aml\nab\0c := 1;\nz := 2;\n");
   anvil_document b = LOAD_LIT("#!aml\n@[schema]\nx := { ty\0pe := Numeric; };\n");
   anvil_document c = LOAD_LIT("#!aml\nv := bar\0e;\n");
   TestBit.is_true(a && anvil_has_errors(a), "SCH91: a statement name with a NUL is a load error");
   TestBit.is_true(b && anvil_has_errors(b), "SCH91: a rule key with a NUL is a load error");
   TestBit.is_true(c && anvil_has_errors(c), "SCH91: a bare word with a NUL is a load error");
   if (a) anvil_dispose(a);
   if (b) anvil_dispose(b);
   if (c) anvil_dispose(c);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- *
 * BR-2610-anvl-schema-002 — a rule whose `type` didn't resolve fails closed
 * ---------------------------------------------------------------------- */
#define UT ANVIL_SCHEMA_ERR_VALIDATION_UNRESOLVED_TYPE

static void test_sch92_unresolved_top_level(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate("#!aml\n@[schema]\np := { type := types.Ghost; };\n",
                             "#!aml\np := \"anything\";\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH92: a present value for an unresolved type is not valid");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema), "SCH92: one violation");
      NESTED_PATH_IS(0, "p", UT, "SCH92: UNRESOLVED_TYPE at p");
      TestBit.is_true(anvil_schema_get_diagnostic_count(schema) == 1, "SCH92: load diagnostic still reported");
   } else {
      TestBit.is_not_null(schema, "SCH92: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch93_unresolved_nested_and_items(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate(
      "#!aml\n@[schema]\n"
      "o := { type := Object; fields := { a := { type := Nope; }; b := { type := String; }; }; };\n"
      "l := { type := Array; items := { type := Nope; }; };\n",
      "#!aml\no := { a := 1; b := \"x\"; };\nl := [ 1, \"two\" ];\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH93: invalid");
      TestBit.is_equal_int(3, (long long)anvil_schema_get_violation_count(schema),
                           "SCH93: o.a, l[0], l[1]");
      NESTED_PATH_IS(0, "o.a", UT, "SCH93: nested");
      NESTED_PATH_IS(1, "l[0]", UT, "SCH93: item 0");
      NESTED_PATH_IS(2, "l[1]", UT, "SCH93: item 1");
   } else {
      TestBit.is_not_null(schema, "SCH93: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch94_unresolved_null_and_nullable(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   bool ok = nested_validate("#!aml\n@[schema]\np := { type := Ghost; nullable := true; };\n",
                             "#!aml\np := null;\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH94: even an explicit null is not valid");
      NESTED_PATH_IS(0, "p", UT, "SCH94: UNRESOLVED_TYPE");
   } else {
      TestBit.is_not_null(schema, "SCH94: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch95_unresolved_absent_and_options(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   // Absent + optional: no value passed through, so valid.
   bool ok = nested_validate("#!aml\n@[schema]\np := { type := Ghost; };\nq := { type := String; };\n",
                             "#!aml\nq := \"x\";\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(schema && ok, "SCH95: an absent optional unresolved field is valid");
   nested_free(schema, sdoc, ddoc);

   // Absent + required: REQUIRED as ever; skip_required lets the absence pass.
   ok = nested_validate("#!aml\n@[schema]\np := { type := Ghost; required := true; };\nq := { type := String; };\n",
                        "#!aml\nset := { q := \"x\"; };\n", &schema, &sdoc, &ddoc);
   if (schema) {
      TestBit.is_false(ok, "SCH95: absent required unresolved field fails");
      NESTED_PATH_IS(0, "p", ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED, "SCH95: REQUIRED, not UNRESOLVED");
      anvil_schema_options o = opts_of(true, false);
      TestBit.is_true(anvil_schema_validate_value(schema, value_of(ddoc, "set"), &o),
                      "SCH95: skip_required: absence passes");
   } else {
      TestBit.is_not_null(schema, "SCH95: schema loaded");
   }
   nested_free(schema, sdoc, ddoc);

   // Present: skip_required and allow_unknown don't excuse it.
   ok = nested_validate("#!aml\n@[schema]\np := { type := Ghost; };\n",
                        "#!aml\nset := { p := 1; extra := 2; };\n", &schema, &sdoc, &ddoc);
   if (schema) {
      anvil_schema_options o = opts_of(true, true);
      TestBit.is_false(anvil_schema_validate_value(schema, value_of(ddoc, "set"), &o),
                       "SCH95: skip_required + allow_unknown still reject a present value");
      TestBit.is_equal_int(1, (long long)anvil_schema_get_violation_count(schema), "SCH95: one violation");
      NESTED_PATH_IS(0, "p", UT, "SCH95: UNRESOLVED_TYPE under options");
   } else {
      TestBit.is_not_null(schema, "SCH95: schema loaded 2");
   }
   nested_free(schema, sdoc, ddoc);
}

static void test_sch96_unresolved_value_form_and_later_resolution(void) {
   anvil_schema schema; anvil_document sdoc, ddoc;
   // A repeated `type` can no longer be written (BR-2610-anvl-parser-005: a repeated object field
   // is a load error), so there is no "later resolving type wins": the schema document is the error.
   (void)nested_validate("#!aml\n@[schema]\np := { type := Ghost; type := String; };\n",
                         "#!aml\np := \"x\";\n", &schema, &sdoc, &ddoc);
   TestBit.is_true(sdoc && anvil_has_errors(sdoc), "SCH96: a repeated type is a load error");
   nested_free(schema, sdoc, ddoc);
}

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

   TestBit.run_ex("SCH13_fractional_data_vs_integral_bound", NULL,
                  test_sch13_fractional_data_vs_integral_bound, th);
   TestBit.run_ex("SCH14_fractional_bound", NULL, test_sch14_fractional_bound, th);
   TestBit.run_ex("SCH15_exponent_bound", NULL, test_sch15_exponent_bound, th);
   TestBit.run_ex("SCH15b_unsigned_exponent", NULL, test_sch15b_unsigned_exponent, th);
   TestBit.run_ex("SCH16_long_numeric_text", NULL, test_sch16_long_numeric_text, th);
   TestBit.run_ex("SCH17_overflowed_data_compares_by_sign", NULL,
                  test_sch17_overflowed_data_compares_by_sign, th);
   TestBit.run_ex("SCH18_exact_comparison_near_limits", NULL,
                  test_sch18_exact_comparison_near_limits, th);
   TestBit.run_ex("SCH19_fractional_bound_inherited_from_type", NULL,
                  test_sch19_fractional_bound_inherited_from_type, th);
   TestBit.run_ex("SCH20_malformed_constraints_are_not_declared", NULL,
                  test_sch20_malformed_constraints_are_not_declared, th);

   TestBit.run_ex("SCH21_long_type_names_resolve_exactly", NULL,
                  test_sch21_long_type_names_resolve_exactly, th);
   TestBit.run_ex("SCH22_long_enum_labels_compare_in_full", NULL,
                  test_sch22_long_enum_labels_compare_in_full, th);
   TestBit.run_ex("SCH23_long_rule_key_is_not_a_known_key", NULL,
                  test_sch23_long_rule_key_is_not_a_known_key, th);
   TestBit.run_ex("SCH24_long_field_and_violation_names", NULL,
                  test_sch24_long_field_and_violation_names, th);

   TestBit.run_ex("SCH25_UNKNOWN_TYPE", NULL,
                  test_sch25_unknown_type, th);
   TestBit.run_ex("SCH26_MALFORMED_CONSTRAINTS", NULL,
                  test_sch26_malformed_constraints, th);
   TestBit.run_ex("SCH27_RULE_NOT_OBJECT", NULL,
                  test_sch27_rule_not_object, th);
   TestBit.run_ex("SCH28_CONFLICTING_CONSTRAINTS", NULL,
                  test_sch28_conflicting_constraints, th);
   TestBit.run_ex("SCH29_EXTENSION_KEYS_ARE_NOT_DIAGNOSTICS", NULL,
                  test_sch29_extension_keys_are_not_diagnostics, th);
   TestBit.run_ex("SCH30_WELL_FORMED_FIXTURES_HAVE_NO_DIAGNOSTICS", NULL,
                  test_sch30_well_formed_fixtures_have_no_diagnostics, th);
   TestBit.run_ex("SCH31_REGISTRY_DIAGNOSTICS_PASS_THROUGH", NULL,
                  test_sch31_registry_diagnostics_pass_through, th);
   TestBit.run_ex("SCH32_DIAGNOSTICS_OUTLIVE_DOCUMENTS_AND_NULL_SAFETY", NULL,
                  test_sch32_diagnostics_outlive_documents_and_null_safety, th);

   TestBit.run_ex("SCH33_VALIDATE_OBJECT_VALUE", NULL,
                  test_sch33_validate_object_value, th);
   TestBit.run_ex("SCH34_SKIP_REQUIRED", NULL,
                  test_sch34_skip_required, th);
   TestBit.run_ex("SCH35_SKIP_REQUIRED_STILL_CHECKS_PRESENT_FIELDS", NULL,
                  test_sch35_skip_required_still_checks_present_fields, th);
   TestBit.run_ex("SCH36_ALLOW_UNKNOWN", NULL,
                  test_sch36_allow_unknown, th);
   TestBit.run_ex("SCH37_NON_OBJECT_VALUE", NULL,
                  test_sch37_non_object_value, th);
   TestBit.run_ex("SCH38_EACH_CALL_REPLACES_VIOLATIONS", NULL,
                  test_sch38_each_call_replaces_violations, th);
   TestBit.run_ex("SCH39_OPTIONS_FORWARD_COMPATIBLE_AND_NULL_SAFETY", NULL,
                  test_sch39_options_forward_compatible_and_null_safety, th);
   TestBit.run_ex("SCH40_DOCUMENT_FORM_EQUALS_VALUE_FORM", NULL,
                  test_sch40_document_form_equals_value_form, th);

   TestBit.run_ex("SCH41_OBJECT_FIELD_RULES", NULL,
                  test_sch41_object_field_rules, th);
   TestBit.run_ex("SCH42_ARRAY_ITEM_RULES", NULL,
                  test_sch42_array_item_rules, th);
   TestBit.run_ex("SCH43_ARRAY_OF_OBJECTS", NULL,
                  test_sch43_array_of_objects, th);
   TestBit.run_ex("SCH44_ITEM_COUNT", NULL,
                  test_sch44_item_count, th);
   TestBit.run_ex("SCH45_OBJECT_TYPE_WITH_FIELDS_IS_REUSABLE", NULL,
                  test_sch45_object_type_with_fields_is_reusable, th);
   TestBit.run_ex("SCH46_TOP_LEVEL_PATHS_UNCHANGED_AND_ORDER", NULL,
                  test_sch46_top_level_paths_unchanged_and_order, th);
   TestBit.run_ex("SCH47_SCALAR_ITEM_RULES", NULL,
                  test_sch47_scalar_item_rules, th);
   TestBit.run_ex("SCH48_SELF_REFERENCE_IS_A_DIAGNOSTIC", NULL,
                  test_sch48_self_reference_is_a_diagnostic, th);
   TestBit.run_ex("SCH49_NESTED_DIAGNOSTICS_CARRY_PATHS", NULL,
                  test_sch49_nested_diagnostics_carry_paths, th);
   TestBit.run_ex("SCH50_NESTED_WITH_VALIDATE_VALUE_OPTIONS", NULL,
                  test_sch50_nested_with_validate_value_options, th);

   TestBit.run_ex("SCH51_NULL_WITHOUT_NULLABLE", NULL,
                  test_sch51_null_without_nullable, th);
   TestBit.run_ex("SCH52_NULLABLE_ACCEPTS_NULL", NULL,
                  test_sch52_nullable_accepts_null, th);
   TestBit.run_ex("SCH53_NULL_SKIPS_OTHER_CONSTRAINTS", NULL,
                  test_sch53_null_skips_other_constraints, th);
   TestBit.run_ex("SCH54_REQUIRED_AND_NULLABLE", NULL,
                  test_sch54_required_and_nullable, th);
   TestBit.run_ex("SCH55_NULLABLE_INHERITED_FROM_TYPE", NULL,
                  test_sch55_nullable_inherited_from_type, th);
   TestBit.run_ex("SCH56_NULLABLE_AT_EVERY_DEPTH", NULL,
                  test_sch56_nullable_at_every_depth, th);
   TestBit.run_ex("SCH57_UNTYPED_FIELD_AND_MALFORMED_NULLABLE", NULL,
                  test_sch57_untyped_field_and_malformed_nullable, th);

   TestBit.run_ex("SCH58_INTEGER_IS_VALUE_BASED", NULL,
                  test_sch58_integer_is_value_based, th);
   TestBit.run_ex("SCH59_INTEGER_WITH_RANGE_AND_NULL", NULL,
                  test_sch59_integer_with_range_and_null, th);
   TestBit.run_ex("SCH60_INTEGER_INHERITED_FROM_TYPE", NULL,
                  test_sch60_integer_inherited_from_type, th);
   TestBit.run_ex("SCH61_INTEGER_NESTED_AND_MALFORMED", NULL,
                  test_sch61_integer_nested_and_malformed, th);

   TestBit.run_ex("SCH62_STRING_ACCEPTS_BLOB", NULL,
                  test_sch62_string_accepts_blob, th);
   TestBit.run_ex("SCH63_STRING_SIZE_MEASURES_BLOB_PAYLOAD", NULL,
                  test_sch63_string_size_measures_blob_payload, th);
   TestBit.run_ex("SCH64_BLOB_ACCEPTS_ONLY_BLOBS", NULL,
                  test_sch64_blob_accepts_only_blobs, th);
   TestBit.run_ex("SCH65_BLOB_TAG_CONSTRAINT", NULL,
                  test_sch65_blob_tag_constraint, th);
   TestBit.run_ex("SCH66_BLOB_SIZE_AND_INHERITANCE", NULL,
                  test_sch66_blob_size_and_inheritance, th);
   TestBit.run_ex("SCH67_BLOB_NESTED_AND_NULLABLE", NULL,
                  test_sch67_blob_nested_and_nullable, th);

   TestBit.run_ex("SCH68_FORMAT_DATE", NULL,
                  test_sch68_format_date, th);
   TestBit.run_ex("SCH69_FORMAT_DATETIME", NULL,
                  test_sch69_format_datetime, th);
   TestBit.run_ex("SCH70_FORMAT_UUID", NULL,
                  test_sch70_format_uuid, th);
   TestBit.run_ex("SCH71_FORMAT_INHERITED_AND_SCOPE", NULL,
                  test_sch71_format_inherited_and_scope, th);
   TestBit.run_ex("SCH72_UNKNOWN_FORMAT_IS_A_DIAGNOSTIC", NULL,
                  test_sch72_unknown_format_is_a_diagnostic, th);
   TestBit.run_ex("SCH73_FORMAT_NESTED_AND_NULLABLE", NULL,
                  test_sch73_format_nested_and_nullable, th);

   TestBit.run_ex("SCH74_FIELDS_IN_ORDER_AND_FIND", NULL,
                  test_sch74_fields_in_order_and_find, th);
   TestBit.run_ex("SCH75_NULL_SAFETY", NULL,
                  test_sch75_null_safety, th);
   TestBit.run_ex("SCH76_CONSTRAINTS_READ_BACK_AS_ENFORCED", NULL,
                  test_sch76_constraints_read_back_as_enforced, th);
   TestBit.run_ex("SCH77_EXTENSION_KEYS", NULL,
                  test_sch77_extension_keys, th);
   TestBit.run_ex("SCH78_NESTED_RULES_REACHABLE", NULL,
                  test_sch78_nested_rules_reachable, th);
   TestBit.run_ex("SCH79_INTROSPECTION_OUTLIVES_THE_DOCUMENTS", NULL,
                  test_sch79_introspection_outlives_the_documents, th);
   TestBit.run_ex("SCH80_EXTRA_VALUE_KINDS", NULL,
                  test_sch80_extra_value_kinds, th);

   TestBit.run_ex("SCH81_SIZE_COUNTS_BYTES_AND_SCHEMAS_ARE_CLOSED", NULL,
                  test_sch81_size_counts_bytes_and_schemas_are_closed, th);

   TestBit.run_ex("SCH82_SCHEMA_VTABLE_MATCHES_FLAT_AND_IS_COMPLETE", NULL,
                  test_sch82_schema_vtable_matches_flat_and_is_complete, th);
   TestBit.run_ex("SCH83_SCHEMA_VTABLE_ROUND_TRIP", NULL, test_sch83_schema_vtable_round_trip, th);

   TestBit.run_ex("SCH84_VALIDATE_WHILE_SAME_TEXT_DOCUMENT_IS_ALIVE", NULL,
                  test_sch84_validate_while_same_text_document_is_alive, th);
   TestBit.run_ex("SCH85_IDENTICAL_EXTRAS_ARE_ALL_READABLE", NULL,
                  test_sch85_identical_extras_are_all_readable, th);

   TestBit.run_ex("SCH86_ENUM_LABELS_WITH_NUL", NULL,
                  test_sch86_enum_labels_with_nul, th);
   TestBit.run_ex("SCH87_VALUES_MEMBERSHIP_IS_BY_LENGTH", NULL,
                  test_sch87_values_membership_is_by_length, th);
   TestBit.run_ex("SCH88_EXTRAS_WITH_NUL", NULL,
                  test_sch88_extras_with_nul, th);
   TestBit.run_ex("SCH89_NUL_IN_IDENTIFIER_POSITIONS_IS_MALFORMED", NULL,
                  test_sch89_nul_in_identifier_positions_is_malformed, th);
   TestBit.run_ex("SCH90_TYPE_FIELDS_TEXT_WITH_NUL", NULL,
                  test_sch90_type_fields_text_with_nul, th);

   TestBit.run_ex("SCH91_NUL_IN_NAMES_AND_BARE_WORDS_IS_A_LOAD_ERROR", NULL,
                  test_sch91_nul_in_names_and_bare_words_is_a_load_error, th);

   TestBit.run_ex("SCH92_UNRESOLVED_TOP_LEVEL", NULL, test_sch92_unresolved_top_level, th);
   TestBit.run_ex("SCH93_UNRESOLVED_NESTED_AND_ITEMS", NULL, test_sch93_unresolved_nested_and_items, th);
   TestBit.run_ex("SCH94_UNRESOLVED_NULL", NULL, test_sch94_unresolved_null_and_nullable, th);
   TestBit.run_ex("SCH95_UNRESOLVED_ABSENT_AND_OPTIONS", NULL, test_sch95_unresolved_absent_and_options, th);
   TestBit.run_ex("SCH96_UNRESOLVED_LATER_RESOLUTION", NULL, test_sch96_unresolved_value_form_and_later_resolution, th);

   return TestBit.report();
}
