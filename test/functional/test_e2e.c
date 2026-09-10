/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_e2e.c - Functional/end-to-end tests for the shipped libanvil.a    *
 * ---------------------------------------------------------------------- *
 * Deliberately different from test/unit: these link ONLY against the     *
 * built lib/{debug,release}/libanvil.a and the public headers under      *
 * include/ — no source file from src/ is compiled directly, and no       *
 * internal/ header is included. The point is to prove the distributable *
 * artifact itself works for a real consumer, not to re-exercise every    *
 * corner case (that's test/unit's job). See notes/native-schema.md and   *
 * the root Makefile.                                                     *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_schema.h"
#include "anvil_type_registry.h"
#include "testbit.h"
#include <string.h>

/* ---------------------------------------------------------------------- *
 * FN01 - core AML parse works end to end through the linked library
 * ---------------------------------------------------------------------- */
static void test_fn01_core_parse(void) {
   const char *src = "#!aml\nname := \"widget\";\ncount := 42;\n";
   anvil_document doc = anvil_load_buffer(src, strlen(src));
   TestBit.is_not_null(doc, "FN01: document loaded");
   if (doc) {
      TestBit.is_false(anvil_has_errors(doc), "FN01: no parse errors");
      anvil_statement stmt = anvil_statement_get(doc, "count");
      TestBit.is_not_null(stmt, "FN01: 'count' statement found");
      if (stmt) {
         anvil_value val = anvil_statement_get_value(stmt);
         TestBit.is_equal_int(ANVIL_VALUE_NUMERIC, anvil_value_get_type(val),
                               "FN01: 'count' is Numeric");
         char text[16] = {0};
         anvil_value_get_text(val, text, sizeof(text));
         TestBit.is_equal_str("42", text, "FN01: 'count' reads back as \"42\"");
      }
      anvil_dispose(doc);
   }
}

/* ---------------------------------------------------------------------- *
 * FN02 - AMP's restrictions (no objects/attributes) are real, not stubbed
 * ---------------------------------------------------------------------- */
static void test_fn02_amp_restrictions(void) {
   const char *src = "#!amp\nconfig := { host := localhost; };\n";
   anvil_document doc = anvil_load_buffer(src, strlen(src));
   TestBit.is_not_null(doc, "FN02: document handle returned");
   if (doc) {
      TestBit.is_true(anvil_has_errors(doc), "FN02: AMP rejects an object value");
      anvil_dispose(doc);
   }
}

/* ---------------------------------------------------------------------- *
 * FN03 - the opt-in type registry resolves a custom type end to end
 * ---------------------------------------------------------------------- */
static void test_fn03_type_registry(void) {
   const char *src = "#!aml\n@[types]\n\nVIN := {\n   type := String;\n   size := 17;\n};\n";
   anvil_document doc = anvil_load_buffer(src, strlen(src));
   TestBit.is_not_null(doc, "FN03: types document loaded");
   if (doc) {
      anvil_type_registry reg = anvil_type_registry_load(doc);
      TestBit.is_not_null(reg, "FN03: registry built");
      if (reg) {
         anvil_type_def def = anvil_type_resolve(reg, "VIN");
         TestBit.is_not_null(def, "FN03: 'VIN' resolves");
         if (def) {
            TestBit.is_equal_int(ANVIL_TYPE_STRING, anvil_type_def_get_kind(def),
                                  "FN03: 'VIN' is kind String");
            long long size = 0;
            TestBit.is_true(anvil_type_def_get_size(def, &size), "FN03: 'VIN' declares size");
            TestBit.is_equal_int(17, (int)size, "FN03: 'VIN' size is 17");
         }
         anvil_type_registry_dispose(reg);
      }
      anvil_dispose(doc);
   }
}

/* ---------------------------------------------------------------------- *
 * FN04 - AnvilSchema validates a clean data document end to end
 * ---------------------------------------------------------------------- */
static void test_fn04_schema_valid(void) {
   const char *schema_src = "#!aml\n@[schema, table=assets]\n\n"
                             "asset_id := { type := Numeric; required := true; };\n"
                             "status := { type := enum; values := [ active, retired ]; };\n";
   anvil_document schema_doc = anvil_load_buffer(schema_src, strlen(schema_src));
   TestBit.is_not_null(schema_doc, "FN04: schema document loaded");
   if (schema_doc) {
      anvil_schema schema = anvil_schema_load(schema_doc);
      TestBit.is_not_null(schema, "FN04: schema built");
      if (schema) {
         const char *data_src = "#!aml\nasset_id := 7;\nstatus := active;\n";
         anvil_document data_doc = anvil_load_buffer(data_src, strlen(data_src));
         if (data_doc) {
            TestBit.is_true(anvil_schema_validate(schema, data_doc),
                             "FN04: clean data document validates");
            TestBit.is_equal_int(0, (int)anvil_schema_get_violation_count(schema),
                                  "FN04: zero violations");
            anvil_dispose(data_doc);
         }
         anvil_schema_dispose(schema);
      }
      anvil_dispose(schema_doc);
   }
}

/* ---------------------------------------------------------------------- *
 * FN05 - AnvilSchema catches a real violation (required + constraint) end
 * to end, through the linked library alone
 * ---------------------------------------------------------------------- */
static void test_fn05_schema_violations(void) {
   const char *schema_src = "#!aml\n@[schema, table=assets]\n\n"
                             "asset_id := { type := Numeric; required := true; };\n"
                             "status := { type := enum; values := [ active, retired ]; };\n";
   anvil_document schema_doc = anvil_load_buffer(schema_src, strlen(schema_src));
   TestBit.is_not_null(schema_doc, "FN05: schema document loaded");
   if (schema_doc) {
      anvil_schema schema = anvil_schema_load(schema_doc);
      TestBit.is_not_null(schema, "FN05: schema built");
      if (schema) {
         // asset_id (required) is missing; status isn't one of the declared values.
         const char *bad_src = "#!aml\nstatus := scrapped;\n";
         anvil_document bad_doc = anvil_load_buffer(bad_src, strlen(bad_src));
         if (bad_doc) {
            TestBit.is_false(anvil_schema_validate(schema, bad_doc),
                              "FN05: bad data document fails validation");
            TestBit.is_equal_int(2, (int)anvil_schema_get_violation_count(schema),
                                  "FN05: two violations collected in one pass");
            anvil_dispose(bad_doc);
         }
         anvil_schema_dispose(schema);
      }
      anvil_dispose(schema_doc);
   }
}

/* ---------------------------------------------------------------------- *
 * FN06 - a fully minified document (no separator at all right after the
 * shebang) parses cleanly end to end through the linked library, and an
 * unrecognized shebang dialect is a real error, not a silent fall-through
 * ---------------------------------------------------------------------- */
static void test_fn06_minified_shebang(void) {
   const char *minified = "#!amlname := 1;status := active;";
   anvil_document doc = anvil_load_buffer(minified, strlen(minified));
   TestBit.is_not_null(doc, "FN06: minified document loaded");
   if (doc) {
      TestBit.is_false(anvil_has_errors(doc), "FN06: no parse errors despite zero separator");
      anvil_statement stmt = anvil_statement_get(doc, "name");
      TestBit.is_not_null(stmt, "FN06: 'name' statement found");
      anvil_dispose(doc);
   }

   const char *bad_dialect = "#!xyz\nname := 1;";
   anvil_document bad_doc = anvil_load_buffer(bad_dialect, strlen(bad_dialect));
   TestBit.is_not_null(bad_doc, "FN06: document handle returned for unrecognized dialect");
   if (bad_doc) {
      TestBit.is_true(anvil_has_errors(bad_doc),
                      "FN06: unrecognized shebang dialect is a real error, not silent AML");
      anvil_dispose(bad_doc);
   }
}

int main(void) {
   TestBit.run("FN01_core_parse", test_fn01_core_parse);
   TestBit.run("FN02_amp_restrictions", test_fn02_amp_restrictions);
   TestBit.run("FN03_type_registry", test_fn03_type_registry);
   TestBit.run("FN04_schema_valid", test_fn04_schema_valid);
   TestBit.run("FN05_schema_violations", test_fn05_schema_violations);
   TestBit.run("FN06_minified_shebang", test_fn06_minified_shebang);
   return TestBit.report();
}
