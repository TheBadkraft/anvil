/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2026-03-12
 * File: test/unit/test_types.c
 * ------------------------------------------------------------------
 * Description:
 * Unit tests for types.c — type name utilities.
 * ------------------------------------------------------------------
 * Copyright (c) 2025–2026 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */

#include "anvil.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <string.h>

static void set_config(FILE **logger) {
   *logger = fopen("logs/test_types.log", "w");
}
static void set_teardown() {
}

// ============================================================================
// TY01–TY05: anvl_value_type_name
// ============================================================================

// TY01 — ANVL_VALUE_SCALAR has a non-null, non-empty name
static void test_value_type_name_scalar(void) {
   const char *name = anvl_value_type_name(ANVL_VALUE_SCALAR);
   Assert.isNotNull((void *)name, "SCALAR type name should not be NULL");
   Assert.isTrue(strlen(name) > 0, "SCALAR type name should not be empty");
}

// TY02 — ANVL_VALUE_OBJECT has a non-null, non-empty name
static void test_value_type_name_object(void) {
   const char *name = anvl_value_type_name(ANVL_VALUE_OBJECT);
   Assert.isNotNull((void *)name, "OBJECT type name should not be NULL");
   Assert.isTrue(strlen(name) > 0, "OBJECT type name should not be empty");
}

// TY03 — ANVL_VALUE_ARRAY, TUPLE, BLOB all return distinct names
static void test_value_type_name_distinct(void) {
   const char *arr   = anvl_value_type_name(ANVL_VALUE_ARRAY);
   const char *tuple = anvl_value_type_name(ANVL_VALUE_TUPLE);
   const char *blob  = anvl_value_type_name(ANVL_VALUE_BLOB);
   Assert.isNotNull((void *)arr,   "ARRAY type name should not be NULL");
   Assert.isNotNull((void *)tuple, "TUPLE type name should not be NULL");
   Assert.isNotNull((void *)blob,  "BLOB type name should not be NULL");
   Assert.isTrue(strcmp(arr, tuple) != 0, "ARRAY and TUPLE names should differ");
   Assert.isTrue(strcmp(arr, blob) != 0,  "ARRAY and BLOB names should differ");
   Assert.isTrue(strcmp(tuple, blob) != 0,"TUPLE and BLOB names should differ");
}

// TY04 — ANVL_VALUE_VARREF and INTERP_STRING names are non-null
static void test_value_type_name_varref_interp(void) {
   const char *varref = anvl_value_type_name(ANVL_VALUE_VARREF);
   const char *interp = anvl_value_type_name(ANVL_VALUE_INTERP_STRING);
   Assert.isNotNull((void *)varref, "VARREF type name should not be NULL");
   Assert.isNotNull((void *)interp, "INTERP_STRING type name should not be NULL");
}

// TY05 — out-of-range value returns non-null fallback (no crash / no NULL)
static void test_value_type_name_out_of_range(void) {
   const char *name = anvl_value_type_name((anvl_value_type)9999);
   Assert.isNotNull((void *)name, "Out-of-range value type should return fallback, not NULL");
}

// ============================================================================
// TY06–TY10: anvl_stmt_type_name
// ============================================================================

// TY06 — ANVL_STMT_ASSN has a non-null, non-empty name
static void test_stmt_type_name_assn(void) {
   const char *name = anvl_stmt_type_name(ANVL_STMT_ASSN);
   Assert.isNotNull((void *)name, "ASSN stmt name should not be NULL");
   Assert.isTrue(strlen(name) > 0, "ASSN stmt name should not be empty");
}

// TY07 — ANVL_STMT_FUNC has a non-null, non-empty name
static void test_stmt_type_name_func(void) {
   const char *name = anvl_stmt_type_name(ANVL_STMT_FUNC);
   Assert.isNotNull((void *)name, "FUNC stmt name should not be NULL");
   Assert.isTrue(strlen(name) > 0, "FUNC stmt name should not be empty");
}

// TY08 — ANVL_STMT_MSSG has a non-null, non-empty name
static void test_stmt_type_name_mssg(void) {
   const char *name = anvl_stmt_type_name(ANVL_STMT_MSSG);
   Assert.isNotNull((void *)name, "MSSG stmt name should not be NULL");
   Assert.isTrue(strlen(name) > 0, "MSSG stmt name should not be empty");
}

// TY09 — all three stmt type names are distinct
static void test_stmt_type_names_distinct(void) {
   const char *assn = anvl_stmt_type_name(ANVL_STMT_ASSN);
   const char *func = anvl_stmt_type_name(ANVL_STMT_FUNC);
   const char *mssg = anvl_stmt_type_name(ANVL_STMT_MSSG);
   Assert.isTrue(strcmp(assn, func) != 0, "ASSN and FUNC stmt names should differ");
   Assert.isTrue(strcmp(assn, mssg) != 0, "ASSN and MSSG stmt names should differ");
   Assert.isTrue(strcmp(func, mssg) != 0, "FUNC and MSSG stmt names should differ");
}

// TY10 — out-of-range stmt type returns non-null fallback
static void test_stmt_type_name_out_of_range(void) {
   const char *name = anvl_stmt_type_name((anvl_stmt_type)9999);
   Assert.isNotNull((void *)name, "Out-of-range stmt type should return fallback, not NULL");
}

// ============================================================================
// Registration
// ============================================================================

__attribute__((constructor)) static void register_test_types(void) {
   testset("Types", set_config, set_teardown);

   testcase("TY01 value_type_name SCALAR",            test_value_type_name_scalar);
   testcase("TY02 value_type_name OBJECT",            test_value_type_name_object);
   testcase("TY03 value_type_name distinct names",    test_value_type_name_distinct);
   testcase("TY04 value_type_name VARREF/INTERP",     test_value_type_name_varref_interp);
   testcase("TY05 value_type_name out-of-range safe", test_value_type_name_out_of_range);
   testcase("TY06 stmt_type_name ASSN",               test_stmt_type_name_assn);
   testcase("TY07 stmt_type_name FUNC",               test_stmt_type_name_func);
   testcase("TY08 stmt_type_name MSSG",               test_stmt_type_name_mssg);
   testcase("TY09 stmt_type_names distinct",          test_stmt_type_names_distinct);
   testcase("TY10 stmt_type_name out-of-range safe",  test_stmt_type_name_out_of_range);
}
