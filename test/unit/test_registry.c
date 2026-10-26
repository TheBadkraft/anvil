/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * test_registry.c - Unit tests for the context-scoped source registry    *
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
#include <sigma/list.h>
#include <sigma/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The registry is scoped to one module_context (BR-2610-anvl-registry-001): these tests build their
 * own contexts, and two contexts never see each other's documents. */

// A stub document whose source holds `text`, not yet registered anywhere.
static module_document make_doc(const char *text) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   module_document doc = Debug.stub_doc(NULL);
   Source.create(&doc->source, &err_code);
   Source.from_buffer(&doc->source, text, strlen(text), &err_code);
   return doc;
}

static module_context make_ctx(void) {
   module_context ctx = NULL;
   anvl_err_code err_code = ANVL_ERR_NONE;
   mod_ctx_initialize(NULL, &ctx, &err_code);
   return ctx;
}

/* REG00 - a fresh context's registry starts empty */
static void test_reg00_registry_starts_empty(void) {
   module_context ctx = make_ctx();
   TestBit.is_not_null(ctx, "REG00: context created");
   TestBit.is_equal_int(0, (long long)Registry.count(ctx), "REG00: registry count is 0 at start");
   TestBit.is_equal_int(0, (long long)Registry.count(NULL), "REG00: count(NULL) is 0");
   Debug.dispose_ctx(ctx);
}

/* REG01 - add a document and find it by hash; its source points back at it */
static void test_reg01_add_and_find(void) {
   module_context ctx = make_ctx();
   module_document doc = make_doc("name := test\n");
   uint64_t hash = Source.hash(doc->source);
   TestBit.is_true(hash != 0, "REG01: source hash is non-zero");
   TestBit.is_true(doc->source->owner == NULL, "REG01: no owner before registration");

   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, Registry.add(ctx, doc, &err_code), "REG01: add returns OK");
   TestBit.is_equal_int(1, (long long)Registry.count(ctx), "REG01: count is 1 after add");
   TestBit.is_true(Registry.find(ctx, hash) == doc, "REG01: found by hash");
   TestBit.is_true(doc->source->owner == doc, "REG01: the source's owner is its document");

   Registry.remove(ctx, hash);
   Debug.dispose_doc(doc);
   Debug.dispose_ctx(ctx);
}

/* REG02 - find on an unknown hash, or with no context, returns NULL */
static void test_reg02_find_unknown_returns_null(void) {
   module_context ctx = make_ctx();
   TestBit.is_null(Registry.find(ctx, UINT64_MAX), "REG02: unknown hash returns NULL");
   TestBit.is_null(Registry.find(ctx, 0), "REG02: hash 0 returns NULL");
   TestBit.is_null(Registry.find(NULL, 1), "REG02: no context returns NULL");
   Debug.dispose_ctx(ctx);
}

/* REG03 - a duplicate hash in the SAME context fails with the dedicated code */
static void test_reg03_duplicate_hash_fails(void) {
   module_context ctx = make_ctx();
   module_document doc1 = make_doc("name := test\n");
   module_document doc2 = make_doc("name := test\n");
   TestBit.is_equal_int((long long)Source.hash(doc1->source), (long long)Source.hash(doc2->source),
                        "REG03: identical buffers have identical hashes");
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, Registry.add(ctx, doc1, &err_code), "REG03: first add succeeds");
   TestBit.is_equal_int(ANVL_RES_ERR, Registry.add(ctx, doc2, &err_code),
                        "REG03: duplicate add to the same context fails");
   TestBit.is_equal_int(ANVL_ERR_SOURCE_ALREADY_LOADED, err_code,
                        "REG03: with its own specific code, not a parser code");
   TestBit.is_true(doc2->source->owner == NULL && doc1->source->owner == doc1,
                   "REG03: the loser has no owner and the winner keeps its own");
   Registry.remove(ctx, Source.hash(doc1->source));
   Debug.dispose_doc(doc1);
   Debug.dispose_doc(doc2);
   Debug.dispose_ctx(ctx);
}

/* REG04 - the SAME hash in two different contexts is fine, and they never see each other */
static void test_reg04_contexts_are_independent(void) {
   module_context a = make_ctx(), b = make_ctx();
   module_document da = make_doc("name := test\n");
   module_document db = make_doc("name := test\n");
   uint64_t hash = Source.hash(da->source);
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_OK, Registry.add(a, da, &err_code), "REG04: add to context A");
   TestBit.is_equal_int(ANVL_RES_OK, Registry.add(b, db, &err_code),
                        "REG04: identical content added to context B succeeds");
   TestBit.is_true(Registry.find(a, hash) == da && Registry.find(b, hash) == db,
                   "REG04: each context finds its own document");
   TestBit.is_true(da->source->owner == da && db->source->owner == db,
                   "REG04: each source points at its own document");
   Registry.remove(a, hash);
   TestBit.is_null(Registry.find(a, hash), "REG04: removing from A unregisters it there");
   TestBit.is_true(Registry.find(b, hash) == db && db->source->owner == db,
                   "REG04: and leaves B's registration untouched");
   Registry.remove(b, hash);
   Debug.dispose_doc(da);
   Debug.dispose_doc(db);
   Debug.dispose_ctx(a);
   Debug.dispose_ctx(b);
}

/* REG05 - remove by hash unregisters the document and clears its source's owner */
static void test_reg05_remove_unregisters(void) {
   module_context ctx = make_ctx();
   module_document doc = make_doc("name := test\n");
   uint64_t hash = Source.hash(doc->source);
   anvl_err_code err_code = ANVL_ERR_NONE;
   Registry.add(ctx, doc, &err_code);
   TestBit.is_equal_int(1, (long long)Registry.count(ctx), "REG05: count is 1 before remove");
   Registry.remove(ctx, hash);
   TestBit.is_equal_int(0, (long long)Registry.count(ctx), "REG05: count is 0 after remove");
   TestBit.is_null(Registry.find(ctx, hash), "REG05: removed document not found");
   TestBit.is_null(doc->source->owner, "REG05: and its source has no owner any more");
   Registry.remove(ctx, hash); // removing something absent is a no-op
   Registry.remove(NULL, hash);
   Debug.dispose_doc(doc);
   Debug.dispose_ctx(ctx);
}

/* REG06 - add rejects a NULL context/document and a source without a hash */
static void test_reg06_add_rejects_invalid(void) {
   module_context ctx = make_ctx();
   anvl_err_code err_code = ANVL_ERR_NONE;
   TestBit.is_equal_int(ANVL_RES_ERR, Registry.add(ctx, NULL, &err_code), "REG06: NULL document");
   TestBit.is_equal_int(ANVL_ERR_INVALID_ARGUMENT, err_code, "REG06: INVALID_ARGUMENT for NULL doc");
   module_document doc = Debug.stub_doc(NULL);
   TestBit.is_equal_int(ANVL_RES_ERR, Registry.add(ctx, doc, &err_code),
                        "REG06: a document without a source fails");
   module_document ok = make_doc("name := test\n");
   TestBit.is_equal_int(ANVL_RES_ERR, Registry.add(NULL, ok, &err_code), "REG06: NULL context fails");
   Debug.dispose_doc(doc);
   Debug.dispose_doc(ok);
   Debug.dispose_ctx(ctx);
}

/* REG07 - clear unregisters everything in a context and clears the owners */
static void test_reg07_clear_empties_registry(void) {
   module_context ctx = make_ctx();
   module_document doc = make_doc("name := test\n");
   anvl_err_code err_code = ANVL_ERR_NONE;
   Registry.add(ctx, doc, &err_code);
   List.append(ctx->docs, doc);
   TestBit.is_equal_int(1, (long long)Registry.count(ctx), "REG07: count is 1 before clear");
   Registry.clear(ctx);
   TestBit.is_equal_int(0, (long long)Registry.count(ctx), "REG07: count is 0 after clear");
   TestBit.is_null(doc->source->owner, "REG07: the owner is cleared");
   Registry.clear(ctx); // idempotent
   List.remove(ctx->docs, 0);
   Debug.dispose_doc(doc);
   Debug.dispose_ctx(ctx);
}

/* REG08 - the dedicated code has a real message and name */
static void test_reg08_already_loaded_code_has_message(void) {
   const char *msg = anvl_error_code_message(ANVL_ERR_SOURCE_ALREADY_LOADED);
   TestBit.is_true(strcmp(msg, "Unknown error") != 0 && strstr(msg, "identical") != NULL,
                   "REG08: the message says an identical document is already loaded");
   const char *name = anvl_error_code_name(ANVL_ERR_SOURCE_ALREADY_LOADED);
   TestBit.is_true(strcmp(name, "ANVL_ERR_SOURCE_ALREADY_LOADED") == 0, "REG08: and it has a name");
}

/* REG09 - a source with no owner (never registered, or unregistered) is handled cleanly by every
 * Source function that used to ask the registry: false / error / NULL, never a crash. This is what
 * "Registry.find returned NULL" meant before the back-pointer. */
static void test_reg09_ownerless_source_is_handled_cleanly(void) {
   module_context ctx = make_ctx();
   module_document doc = make_doc("name := test\n");
   anvl_err_code err_code = ANVL_ERR_NONE;
   Registry.add(ctx, doc, &err_code);
   doc->context = ctx; // mod_ctx_register_doc does this; the facade alone only registers
   mod_ctx_create_arena(ctx, 4096, &err_code);

   // Registered: everything works.
   TestBit.is_true(Source.get_arena(doc->source, &err_code) != NULL, "REG09: arena while registered");
   TestBit.is_equal_int(ANVL_RES_OK,
                        Source.set_error(doc->source, ANVL_ERR_PARSER_UNEXPECTED_TOKEN, 1, 1, NULL, &err_code),
                        "REG09: set_error while registered");
   TestBit.is_true(Source.has_errors(doc->source), "REG09: has_errors while registered");

   // Unregistered with the source still alive: the same calls fail cleanly.
   Registry.remove(ctx, Source.hash(doc->source));
   err_code = ANVL_ERR_NONE;
   TestBit.is_false(Source.has_errors(doc->source), "REG09: has_errors is false without an owner");
   TestBit.is_equal_int(ANVL_RES_ERR,
                        Source.set_error(doc->source, ANVL_ERR_PARSER_UNEXPECTED_CHAR, 1, 1, NULL, &err_code),
                        "REG09: set_error fails without an owner");
   TestBit.is_true(err_code != ANVL_ERR_NONE, "REG09: with an error code");
   TestBit.is_null(Source.get_arena(doc->source, &err_code), "REG09: no arena without an owner");
   TestBit.is_null(Source.new_node(doc->source, ANVL_NODE_VALUE, &err_code),
                   "REG09: no node without an owner");
   TestBit.is_false(Source.has_errors(NULL), "REG09: NULL source");

   Debug.dispose_doc(doc);
   Debug.dispose_ctx(ctx);
}

/* REG10 (BR-2610-anvl-errors-001) - the message and name tables are COMPLETE: every anvl_err_code
 * declared in errors.h has a specific message and a name. The errors analogue of VT11: the codes are
 * read from the header, so a code added without a table entry fails here. */
static void test_reg10_error_tables_are_complete(void) {
   FILE *f = fopen("../../include/errors.h", "rb");
   TestBit.is_not_null(f, "REG10: errors.h readable");
   if (!f) {
      return;
   }
   char line[256];
   int checked = 0;
   bool in_enum = false;
   while (fgets(line, sizeof(line), f)) {
      if (strstr(line, "typedef enum")) {
         in_enum = true;
      }
      if (in_enum && strstr(line, "} anvl_err_code;")) {
         break;
      }
      char name[96];
      unsigned long value;
      const char *p = strstr(line, "ANVL_ERR_");
      if (in_enum && p && sscanf(p, "%95[A-Z0-9_] = %lu", name, &value) == 2) {
         const char *msg = anvl_error_code_message((anvl_err_code)value);
         const char *nm = anvl_error_code_name((anvl_err_code)value);
         bool ok = msg && *msg && strcmp(msg, "Unknown error") != 0 && nm &&
                   strcmp(nm, "ANVL_ERR_UNKNOWN") != 0 && strncmp(nm, "ANVL_ERR_", 9) == 0;
         if (!ok) {
            printf("    REG10: %s (%lu) has no message (\"%s\") or name (\"%s\")\n", name, value,
                   msg ? msg : "(null)", nm ? nm : "(null)");
         }
         TestBit.is_true(ok, name);
         checked++;
      }
   }
   fclose(f);
   TestBit.is_true(checked > 90, "REG10: the header scan found the whole enum");
   TestBit.is_true(strcmp(anvl_error_code_message((anvl_err_code)123456), "Unknown error") == 0,
                   "REG10: a genuinely unknown code still reads \"Unknown error\" (defensive)");
}

int main(void) {
   TestBit.run_ex("REG00_registry_starts_empty", NULL, test_reg00_registry_starts_empty, NULL);
   TestBit.run_ex("REG01_add_and_find", NULL, test_reg01_add_and_find, NULL);
   TestBit.run_ex("REG02_find_unknown_returns_null", NULL, test_reg02_find_unknown_returns_null, NULL);
   TestBit.run_ex("REG03_duplicate_hash_fails", NULL, test_reg03_duplicate_hash_fails, NULL);
   TestBit.run_ex("REG04_contexts_are_independent", NULL, test_reg04_contexts_are_independent, NULL);
   TestBit.run_ex("REG05_remove_unregisters", NULL, test_reg05_remove_unregisters, NULL);
   TestBit.run_ex("REG06_add_rejects_invalid", NULL, test_reg06_add_rejects_invalid, NULL);
   TestBit.run_ex("REG07_clear_empties_registry", NULL, test_reg07_clear_empties_registry, NULL);
   TestBit.run_ex("REG08_already_loaded_code_has_message", NULL,
                  test_reg08_already_loaded_code_has_message, NULL);
   TestBit.run_ex("REG09_ownerless_source_is_handled_cleanly", NULL,
                  test_reg09_ownerless_source_is_handled_cleanly, NULL);

   TestBit.run_ex("REG10_error_tables_are_complete", NULL, test_reg10_error_tables_are_complete, NULL);

   return TestBit.report();
}
