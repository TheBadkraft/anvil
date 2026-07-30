/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_files.c - Infrastructure tests: Files vtable
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/infra/test_files.c
 */

#include "types.h"
#include "internal/files.h"
#include "../utilities/helpers.h"
#include "testbit.h"
#include "std.h"

static void td(void) {}

/* ----------------------------------------------------------------- *
 * IF00 — get fixture_path                                            *
 * ----------------------------------------------------------------- */
static void test_if00_fixture_path(void) {
   const char *path = fixture_path("f01_bare_literal.anvl");
   const char *expected = "../../test/fixtures/f01_bare_literal.anvl";
   TestBit.is_not_null((void *)path, "IF00: fixture_path returns non-null");
   TestBit.is_equal_str(expected, path, "IF00: fixture_path returns correct path");
}
/* ----------------------------------------------------------------- */
/* IF01 — load valid fixture file                                      */
/* ----------------------------------------------------------------- */
static void test_if01_load_valid_file(void) {
   const char *src = NULL;
   size_t len = 0;
   anvl_err_code err_code = ANVL_ERR_NONE;
   
   anvl_result res = Files.load("../../test/fixtures/f01_bare_literal.anvl", &src, &len, &err_code);
   TestBit.is_true(res == ANVL_RES_OK, "IF01: load returns true for valid file");
   TestBit.is_true(err_code == ANVL_ERR_NONE, "IF01: err_code is ANVL_ERR_NONE for valid file");
   TestBit.is_not_null((void *)src, "IF01: source buffer is non-null");
   TestBit.is_true(len > 0, "IF01: source length is non-zero");

   free((void *)src);
}
/* ----------------------------------------------------------------- *
 * IF02 — load non-existent file                                      *
 * ----------------------------------------------------------------- */
static void test_if02_load_missing_file(void) {
   const char *src = NULL;
   size_t len = 0;

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = Files.load("../../test/fixtures/does_not_exist.anvl", &src, &len, &err_code);
   TestBit.is_true(res == ANVL_RES_ERR, "IF02: load returns ANVL_RES_ERR for missing file");
   TestBit.is_true(err_code == ANVL_ERR_FILE_NOT_FOUND, "IF02: err_code is ANVL_ERR_FILE_NOT_FOUND for missing file");
   TestBit.is_null((void *)src, "IF02: source buffer remains null");
   TestBit.is_equal_int(0, (long long)len, "IF02: length remains zero");
}
/* ----------------------------------------------------------------- *
 * IF03 — null path                                                   *
 * ----------------------------------------------------------------- */
static void test_if03_null_path(void) {
   const char *src = NULL;
   size_t len = 0;

   anvl_err_code err_code = ANVL_ERR_NONE;
   anvl_result res = Files.load(NULL, &src, &len, &err_code);
   TestBit.is_true(res == ANVL_RES_ERR, "IF03: load returns ANVL_RES_ERR for null path");
   TestBit.is_true(err_code == ANVL_ERR_FILE_INVALID_PATH, "IF03: err_code is ANVL_ERR_FILE_INVALID_PATH for null path");
}

int main(void) {
   TestBit.run_ex("IF00_fixture_path", NULL, test_if00_fixture_path, td);
   TestBit.run_ex("IF01_load_valid_file", NULL, test_if01_load_valid_file, td);
   TestBit.run_ex("IF02_load_missing_file", NULL, test_if02_load_missing_file, td);
   TestBit.run_ex("IF03_null_path", NULL, test_if03_null_path, td);

   return TestBit.report();
}