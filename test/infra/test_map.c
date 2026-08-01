/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_map.c - Infrastructure tests: Map vtable
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/infra/test_map.c
 */
#include "sigma/collections.h"
#include "sigma/map.h"
#include "testbit.h"

#include <stdio.h>
#include <string.h>

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IM01 - Map.new creates a valid map                                */
/* ----------------------------------------------------------------- */
static void test_im01_map_new(void) {
   map m = Map.new(2);
   TestBit.is_not_null(m, "IM01: Map.new returns non-null");
   TestBit.is_true(Map.capacity(m) >= 8, "IM01: minimum capacity is 8");
   TestBit.is_equal_int(0, (long long)Map.count(m), "IM01: initial count is 0");
   Map.dispose(m);
}

/* ----------------------------------------------------------------- */
/* IM02 - Map.set/get/overwrite behavior                              */
/* ----------------------------------------------------------------- */
static void test_im02_map_set_get_update(void) {
   map m = Map.new(8);

   TestBit.is_equal_int(0, (long long)Map.set(m, "alpha", 5, 10), "IM02: set alpha OK");
   TestBit.is_equal_int(0, (long long)Map.set(m, "beta", 4, 20), "IM02: set beta OK");

   usize out = 0;
   TestBit.is_true(Map.get(m, "alpha", 5, &out) == 1, "IM02: get alpha found");
   TestBit.is_equal_int(10, (long long)out, "IM02: alpha value is 10");

   TestBit.is_equal_int(0, (long long)Map.set(m, "alpha", 5, 42), "IM02: overwrite alpha OK");
   TestBit.is_true(Map.get(m, "alpha", 5, &out) == 1, "IM02: get alpha after overwrite found");
   TestBit.is_equal_int(42, (long long)out, "IM02: alpha value updated to 42");

   TestBit.is_equal_int(2, (long long)Map.count(m), "IM02: overwrite does not increase count");
   Map.dispose(m);
}

/* ----------------------------------------------------------------- */
/* IM03 - Map.has/remove behavior                                     */
/* ----------------------------------------------------------------- */
static void test_im03_map_has_remove(void) {
   map m = Map.new(8);

   Map.set(m, "gone", 4, 9);
   TestBit.is_true(Map.has(m, "gone", 4) == 1, "IM03: has is true before remove");
   TestBit.is_true(Map.remove(m, "gone", 4) == 1, "IM03: remove existing key returns 1");
   TestBit.is_true(Map.has(m, "gone", 4) == 0, "IM03: has is false after remove");

   usize out = 0;
   TestBit.is_true(Map.get(m, "gone", 4, &out) == 0, "IM03: get removed key is not found");
   TestBit.is_true(Map.remove(m, "gone", 4) == 0, "IM03: removing missing key returns 0");
   TestBit.is_equal_int(0, (long long)Map.count(m), "IM03: count is 0 after remove");

   Map.dispose(m);
}

/* ----------------------------------------------------------------- */
/* IM04 - Map resizes and preserves values                            */
/* ----------------------------------------------------------------- */
static void test_im04_map_resize_preserves(void) {
   map m = Map.new(2);
   usize initial_capacity = Map.capacity(m);

   char keys[64][16];
   for (int i = 0; i < 64; i++) {
      snprintf(keys[i], sizeof(keys[i]), "k%02d", i);
      int rc = Map.set(m, keys[i], strlen(keys[i]), (usize)(1000 + i));
      TestBit.is_equal_int(0, (long long)rc, "IM04: set during resize path returns OK");
   }

   TestBit.is_true(Map.capacity(m) > initial_capacity, "IM04: capacity increased");
   TestBit.is_equal_int(64, (long long)Map.count(m), "IM04: count is 64 after inserts");

   for (int i = 0; i < 64; i++) {
      usize out = 0;
      int found = Map.get(m, keys[i], strlen(keys[i]), &out);
      TestBit.is_true(found == 1, "IM04: key found after resize");
      TestBit.is_equal_int(1000 + i, (long long)out, "IM04: value preserved after resize");
   }

   Map.dispose(m);
}

/* ----------------------------------------------------------------- */
/* IM05 - Sparse iterator visits active entries only                  */
/* ----------------------------------------------------------------- */
static void test_im05_map_sparse_iterator(void) {
   map m = Map.new(8);

   Map.set(m, "a", 1, 1);
   Map.set(m, "b", 1, 2);
   Map.set(m, "c", 1, 3);
   Map.remove(m, "b", 1); /* tombstone */
   Map.set(m, "d", 1, 4);

   sparse_iterator it = Map.create_iterator(m);
   TestBit.is_not_null(it, "IM05: create_iterator returns non-null");

   int seen = 0;
   long long sum = 0;

   while (SparseIterator.next(it)) {
      map_entry *entry = NULL;
      int rc = SparseIterator.current_value(it, (object *)&entry);
      TestBit.is_equal_int(0, (long long)rc, "IM05: current_value returns OK");
      TestBit.is_not_null(entry, "IM05: current entry is non-null");
      TestBit.is_true(entry->key_len > 0, "IM05: entry key_len is non-zero");
      sum += (long long)entry->value;
      seen++;
   }

   TestBit.is_equal_int(3, (long long)seen, "IM05: iterator skips tombstones and visits 3 entries");
   TestBit.is_equal_int(8, sum, "IM05: iterator sees expected values");

   SparseIterator.reset(it);
   TestBit.is_true(SparseIterator.next(it), "IM05: iterator reset rewinds traversal");

   SparseIterator.dispose(it);
   Map.dispose(m);
}

int main(void) {
   TestBit.run_ex("IM01_map_new", NULL, test_im01_map_new, td);
   TestBit.run_ex("IM02_map_set_get_update", NULL, test_im02_map_set_get_update, td);
   TestBit.run_ex("IM03_map_has_remove", NULL, test_im03_map_has_remove, td);
   TestBit.run_ex("IM04_map_resize_preserves", NULL, test_im04_map_resize_preserves, td);
   TestBit.run_ex("IM05_map_sparse_iterator", NULL, test_im05_map_sparse_iterator, td);

   return TestBit.report();
}
