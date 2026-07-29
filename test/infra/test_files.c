/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_files.c - Infrastructure tests: Files vtable
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/infra/test_files.c
 */
#include "internal/files.h"
#include "../utilities/helpers.h"
#include "testbit.h"
#include <stdlib.h>
#include <string.h>

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IF01 — load valid fixture file                                      */
/* ----------------------------------------------------------------- */
static void test_if01_load_valid_file(void) {
    const char *src = NULL;
    size_t len = 0;

    bool ok = Files.load("../../test/fixtures/f01_bare_literal.anvl", &src, &len);
    TestBit.is_true(ok, "IF01: load returns true for valid file");
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

    bool ok = Files.load("../../test/fixtures/does_not_exist.anvl", &src, &len);
    TestBit.is_false(ok, "IF02: load returns false for missing file");
    TestBit.is_null((void *)src, "IF02: source buffer remains null");
    TestBit.is_equal_int(0, (long long)len, "IF02: length remains zero");
}

/* ----------------------------------------------------------------- *
 * IF03 — null path                                                   *
 * ----------------------------------------------------------------- */
static void test_if03_null_path(void) {
    const char *src = NULL;
    size_t len = 0;

    bool ok = Files.load(NULL, &src, &len);
    TestBit.is_false(ok, "IF03: load returns false for null path");
}

/* ----------------------------------------------------------------- *
 * IF04 — get fixture_path                                            *
 * ----------------------------------------------------------------- */
static void test_if04_fixture_path(void) {
    const char *path = fixture_path("f01_bare_literal.anvl");
    TestBit.is_not_null((void *)path, "IF04: fixture_path returns non-null");
    TestBit.is_true(strstr(path, "test/fixtures/f01_bare_literal.anvl") != NULL,
                    "IF04: fixture_path returns correct path");
}


int main(void) {
    TestBit.run_ex("IF01_load_valid_file",   NULL, test_if01_load_valid_file,   td);
    TestBit.run_ex("IF02_load_missing_file", NULL, test_if02_load_missing_file, td);
    TestBit.run_ex("IF03_null_path",         NULL, test_if03_null_path,         td);
    TestBit.run_ex("IF04_fixture_path",      NULL, test_if04_fixture_path,      td);

    return TestBit.report();
}