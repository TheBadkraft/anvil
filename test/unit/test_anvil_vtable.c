/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_anvil_vtable.c - Unit tests for the vtable convenience layer      *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_anvil_vtable.c                                    *
 * ---------------------------------------------------------------------- *
 * Every vtable field must be the *exact same function* as its flat       *
 * counterpart (pointer identity, not just matching behavior) — this is   *
 * the mechanical drift check notes/public-api.md calls for. One          *
 * end-to-end smoke test proves the vtable alone (never naming a flat     *
 * function) is genuinely usable on its own.                              *
 * ********************************************************************** */

#include "anvil_flat.h"
#include "anvil_types.h"
#include "anvil_vtable.h"
#include "internal/source_registry.h"
#include "testbit.h"
// ----------------
#include "../utilities/helpers.h"

static void th(void) {
   (void)reset_context_spec_defaults(NULL);
   Registry.clear();
}

/* ---------------------------------------------------------------------- *
 * VT01 — every Anvil vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt01_anvil_matches_flat(void) {
   TestBit.is_true(Anvil.load == anvil_load, "VT01: Anvil.load is anvil_load");
   TestBit.is_true(Anvil.load_buffer == anvil_load_buffer, "VT01: Anvil.load_buffer is anvil_load_buffer");
   TestBit.is_true(Anvil.dispose == anvil_dispose, "VT01: Anvil.dispose is anvil_dispose");
   TestBit.is_true(Anvil.has_errors == anvil_has_errors,
                   "VT01: Anvil.has_errors is anvil_has_errors");
   TestBit.is_true(Anvil.get_error == anvil_get_error, "VT01: Anvil.get_error is anvil_get_error");
   TestBit.is_true(Anvil.get_version == anvil_get_version,
                   "VT01: Anvil.get_version is anvil_get_version");
}
/* ---------------------------------------------------------------------- *
 * VT02 — every Statement vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt02_statement_matches_flat(void) {
   TestBit.is_true(Statement.get == anvil_statement_get, "VT02: Statement.get is anvil_statement_get");
   TestBit.is_true(Statement.get_value == anvil_statement_get_value,
                   "VT02: Statement.get_value is anvil_statement_get_value");
   TestBit.is_true(Statement.get_name == anvil_statement_get_name,
                   "VT02: Statement.get_name is anvil_statement_get_name");
   TestBit.is_true(Statement.get_attribute_count == anvil_statement_get_attribute_count,
                   "VT02: Statement.get_attribute_count is anvil_statement_get_attribute_count");
   TestBit.is_true(Statement.get_attribute == anvil_statement_get_attribute,
                   "VT02: Statement.get_attribute is anvil_statement_get_attribute");
   TestBit.is_true(Statement.find_attribute == anvil_statement_find_attribute,
                   "VT02: Statement.find_attribute is anvil_statement_find_attribute");
}
/* ---------------------------------------------------------------------- *
 * VT03 — every Value vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt03_value_matches_flat(void) {
   TestBit.is_true(Value.get_type == anvil_value_get_type, "VT03: Value.get_type is anvil_value_get_type");
   TestBit.is_true(Value.get_text == anvil_value_get_text, "VT03: Value.get_text is anvil_value_get_text");
   TestBit.is_true(Value.get_count == anvil_value_get_count,
                   "VT03: Value.get_count is anvil_value_get_count");
   TestBit.is_true(Value.get_element == anvil_value_get_element,
                   "VT03: Value.get_element is anvil_value_get_element");
   TestBit.is_true(Value.get_statement == anvil_value_get_statement,
                   "VT03: Value.get_statement is anvil_value_get_statement");
}
/* ---------------------------------------------------------------------- *
 * VT05 — every Document vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt05_document_matches_flat(void) {
   TestBit.is_true(Document.get_attribute_count == anvil_document_get_attribute_count,
                   "VT05: Document.get_attribute_count is anvil_document_get_attribute_count");
   TestBit.is_true(Document.get_attribute == anvil_document_get_attribute,
                   "VT05: Document.get_attribute is anvil_document_get_attribute");
   TestBit.is_true(Document.find_attribute == anvil_document_find_attribute,
                   "VT05: Document.find_attribute is anvil_document_find_attribute");
}
/* ---------------------------------------------------------------------- *
 * VT06 — every Attribute vtable field is pointer-identical to its flat
 * counterpart
 * ---------------------------------------------------------------------- */
static void test_vt06_attribute_matches_flat(void) {
   TestBit.is_true(Attribute.get_key == anvil_attribute_get_key,
                   "VT06: Attribute.get_key is anvil_attribute_get_key");
   TestBit.is_true(Attribute.get_value == anvil_attribute_get_value,
                   "VT06: Attribute.get_value is anvil_attribute_get_value");
}
/* ---------------------------------------------------------------------- *
 * VT04 — end-to-end smoke test using only the vtable, never a flat
 * function name, proving the vtable alone is genuinely usable
 * ---------------------------------------------------------------------- */
static void test_vt04_vtable_only_smoke_test(void) {
   anvil_document doc = Anvil.load(fixture_path("f01_bare_literal.anvl"));
   TestBit.is_not_null(doc, "VT04: document loaded via the vtable");
   if (!doc) {
      return;
   }
   TestBit.is_false(Anvil.has_errors(doc), "VT04: no errors, via the vtable");

   anvil_statement name = Statement.get(doc, "name");
   TestBit.is_not_null(name, "VT04: 'name' statement found via the vtable");
   anvil_value name_val = Statement.get_value(name);
   TestBit.is_equal_int(ANVIL_VALUE_IDENTIFIER, Value.get_type(name_val),
                        "VT04: 'name' is ANVIL_VALUE_IDENTIFIER, via the vtable");

   Anvil.dispose(doc);
}

/* ---------------------------------------------------------------------- *
 * Test runner
 * ---------------------------------------------------------------------- */
int main(void) {
   TestBit.run_ex("VT01_anvil_matches_flat", NULL, test_vt01_anvil_matches_flat, th);
   TestBit.run_ex("VT02_statement_matches_flat", NULL, test_vt02_statement_matches_flat, th);
   TestBit.run_ex("VT03_value_matches_flat", NULL, test_vt03_value_matches_flat, th);
   TestBit.run_ex("VT04_vtable_only_smoke_test", NULL, test_vt04_vtable_only_smoke_test, th);
   TestBit.run_ex("VT05_document_matches_flat", NULL, test_vt05_document_matches_flat, th);
   TestBit.run_ex("VT06_attribute_matches_flat", NULL, test_vt06_attribute_matches_flat, th);

   return TestBit.report();
}
