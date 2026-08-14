/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_registry.c - Unit tests for source hash registry                  *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: test/unit/test_registry.c                                        *
 * ********************************************************************** */

#include "anvil.h"
#include "types.h"
#include "internal/constants.h"
#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
#include "testbit.h"
#include "std.h"
// ----------------
#include "../utilities/debug.h"
#include <sigma/types.h>

static void tr(void) { Registry.clear(); }

/* ---------------------------------------------------------------------- *
 * REG00 — Registry starts empty
 * ---------------------------------------------------------------------- */
static void test_reg00_registry_starts_empty(void) {
   TestBit.is_equal_int(0, (long long)Registry.count(), "REG00: registry count is 0 at start");
}

/* ---------------------------------------------------------------------- *
 * REG01 — add a document and find it by hash
 * ---------------------------------------------------------------------- */
static void test_reg01_add_and_find(void) {
   module_document doc = Debug.stub_doc(NULL);
   TestBit.is_not_null(doc, "REG01: stub document allocated");

   // create then load a buffer so the source has a non-zero hash
   const char *buffer = "name := test\n";
   anvl_err_code err_code = ANVL_ERR_NONE;
   Source.create(&doc->source, &err_code);
   Source.from_buffer(&doc->source, buffer, strlen(buffer), &err_code);
   TestBit.is_equal_int(ANVL_ERR_NONE, err_code, "REG01: buffer load succeeds");

   uint64_t hash = Source.hash(doc->source);
   TestBit.is_true(hash != 0, "REG01: source hash is non-zero");

   anvl_result res = Registry.add(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "REG01: registry add returns OK");
   TestBit.is_equal_int(1, (long long)Registry.count(), "REG01: registry count is 1 after add");

   module_document found = Registry.find(hash);
   TestBit.is_not_null(found, "REG01: registered document found by hash");
   TestBit.is_true(found == doc, "REG01: found document matches registered document");

   Debug.dispose_doc(doc);
}

/* ---------------------------------------------------------------------- *
 * REG02 — find on unknown hash returns NULL
 * ---------------------------------------------------------------------- */
static void test_reg02_find_unknown_returns_null(void) {
   module_document found = Registry.find(UINT64_MAX);
   TestBit.is_null(found, "REG02: find on unknown hash returns NULL");
}

/* ---------------------------------------------------------------------- *
 * REG03 — duplicate hash registration fails
 * ---------------------------------------------------------------------- */
static void test_reg03_duplicate_hash_fails(void) {
   const char *buffer = "name := test\n";
   anvl_err_code err_code = ANVL_ERR_NONE;

   module_document doc1 = Debug.stub_doc(NULL);
   Source.create(&doc1->source, &err_code);
   Source.from_buffer(&doc1->source, buffer, strlen(buffer), &err_code);
   uint64_t hash = Source.hash(doc1->source);

   module_document doc2 = Debug.stub_doc(NULL);
   Source.create(&doc2->source, &err_code);
   Source.from_buffer(&doc2->source, buffer, strlen(buffer), &err_code);

   TestBit.is_equal_int((long long)hash, (long long)Source.hash(doc2->source),
                        "REG03: identical buffers have identical hashes");

   anvl_result res = Registry.add(doc1, &err_code);
   TestBit.is_equal_int(ANVL_RES_OK, res, "REG03: first add succeeds");

   res = Registry.add(doc2, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "REG03: duplicate add fails");

   Debug.dispose_doc(doc1);
   Debug.dispose_doc(doc2);
}

/* ---------------------------------------------------------------------- *
 * REG04 — remove by hash unregisters document
 * ---------------------------------------------------------------------- */
static void test_reg04_remove_unregisters(void) {
   const char *buffer = "name := test\n";
   anvl_err_code err_code = ANVL_ERR_NONE;

   module_document doc = Debug.stub_doc(NULL);
   Source.create(&doc->source, &err_code);
   Source.from_buffer(&doc->source, buffer, strlen(buffer), &err_code);
   uint64_t hash = Source.hash(doc->source);

   Registry.add(doc, &err_code);
   TestBit.is_equal_int(1, (long long)Registry.count(), "REG04: registry count is 1 before remove");

   Registry.remove(hash);
   TestBit.is_equal_int(0, (long long)Registry.count(), "REG04: registry count is 0 after remove");
   TestBit.is_null(Registry.find(hash), "REG04: removed document not found");

   Debug.dispose_doc(doc);
}

/* ---------------------------------------------------------------------- *
 * REG05 — add rejects null document and source without hash
 * ---------------------------------------------------------------------- */
static void test_reg05_add_rejects_invalid(void) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   anvl_result res = Registry.add(NULL, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "REG05: add NULL document fails");
   TestBit.is_equal_int(ANVL_ERR_INVALID_ARGUMENT, err_code,
                        "REG05: err_code is INVALID_ARGUMENT for NULL doc");

   module_document doc = Debug.stub_doc(NULL);
   res = Registry.add(doc, &err_code);
   TestBit.is_equal_int(ANVL_RES_ERR, res, "REG05: add document without source hash fails");

   Debug.dispose_doc(doc);
}

/* ---------------------------------------------------------------------- *
 * REG06 — clear empties the registry
 * ---------------------------------------------------------------------- */
static void test_reg06_clear_empties_registry(void) {
   const char *buffer = "name := test\n";
   anvl_err_code err_code = ANVL_ERR_NONE;

   module_document doc = Debug.stub_doc(NULL);
   Source.create(&doc->source, &err_code);
   Source.from_buffer(&doc->source, buffer, strlen(buffer), &err_code);
   Registry.add(doc, &err_code);
   TestBit.is_equal_int(1, (long long)Registry.count(), "REG06: registry count is 1 before clear");

   Registry.clear();
   TestBit.is_equal_int(0, (long long)Registry.count(), "REG06: registry count is 0 after clear");

   Debug.dispose_doc(doc);
}

/* ---------------------------------------------------------------------- *
 * REG07 — release keeps registry alive until final reference
 * ---------------------------------------------------------------------- */
static void test_reg07_release_survives_multiple_refs(void) {
   const char *buffer = "name := test\n";
   anvl_err_code err_code = ANVL_ERR_NONE;

   Registry.init();
   Registry.init();

   Registry.release();

   module_document doc = Debug.stub_doc(NULL);
   Source.create(&doc->source, &err_code);
   Source.from_buffer(&doc->source, buffer, strlen(buffer), &err_code);
   uint64_t hash = Source.hash(doc->source);

   TestBit.is_equal_int(ANVL_RES_OK, Registry.add(doc, &err_code),
                        "REG07: add succeeds while registry has remaining ref");
   TestBit.is_equal_int(1, (long long)Registry.count(), "REG07: registry count is 1 after add");

   Registry.release();
   TestBit.is_equal_int(0, (long long)Registry.count(),
                        "REG07: registry count is 0 after final release");
   TestBit.is_null(Registry.find(hash), "REG07: document not found after final release");

   Debug.dispose_doc(doc);
}

/* ---------------------------------------------------------------------- *
 * REG08 — release on uninitialized registry is a no-op
 * ---------------------------------------------------------------------- */
static void test_reg08_release_noop_when_uninitialized(void) {
   Registry.clear();
   Registry.release();
   TestBit.is_equal_int(0, (long long)Registry.count(),
                        "REG08: release on cleared registry keeps count at 0");
}

int main(void) {
   TestBit.run_ex("REG00_registry_starts_empty", NULL, test_reg00_registry_starts_empty, tr);
   TestBit.run_ex("REG01_add_and_find", NULL, test_reg01_add_and_find, tr);
   TestBit.run_ex("REG02_find_unknown_returns_null", NULL, test_reg02_find_unknown_returns_null,
                  tr);
   TestBit.run_ex("REG03_duplicate_hash_fails", NULL, test_reg03_duplicate_hash_fails, tr);
   TestBit.run_ex("REG04_remove_unregisters", NULL, test_reg04_remove_unregisters, tr);
   TestBit.run_ex("REG05_add_rejects_invalid", NULL, test_reg05_add_rejects_invalid, tr);
   TestBit.run_ex("REG06_clear_empties_registry", NULL, test_reg06_clear_empties_registry, tr);
   TestBit.run_ex("REG07_release_survives_multiple_refs", NULL,
                  test_reg07_release_survives_multiple_refs, tr);
   TestBit.run_ex("REG08_release_noop_when_uninitialized", NULL,
                  test_reg08_release_noop_when_uninitialized, tr);

   return TestBit.report();
}
