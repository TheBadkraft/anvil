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
#include "sigma/query.h"
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

   addr out = 0;
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

   addr out = 0;
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
      int rc = Map.set(m, keys[i], strlen(keys[i]), (addr)(1000 + i));
      TestBit.is_equal_int(0, (long long)rc, "IM04: set during resize path returns OK");
   }

   TestBit.is_true(Map.capacity(m) > initial_capacity, "IM04: capacity increased");
   TestBit.is_equal_int(64, (long long)Map.count(m), "IM04: count is 64 after inserts");

   for (int i = 0; i < 64; i++) {
      addr out = 0;
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

/* ----------------------------------------------------------------- */
/* IM06 - Map.as_queryable yields entries, skipping tombstones        */
/* ----------------------------------------------------------------- */
static void test_im06_map_as_queryable_skips_tombstones(void) {
   map m = Map.new(8);
   Map.set(m, "a", 1, 1);
   Map.set(m, "b", 1, 2);
   Map.set(m, "c", 1, 3);
   Map.remove(m, "b", 1); /* tombstone */
   Map.set(m, "d", 1, 4);

   sc_queryable q = Map.as_queryable(m);
   const void *elem;
   int seen = 0;
   long long sum = 0;
   while (Query.next(&q, &elem, NULL)) {
      const map_entry *entry = elem;
      TestBit.is_true(entry->key_len > 0, "IM06: entry key_len is non-zero");
      sum += (long long)entry->value;
      seen++;
   }

   TestBit.is_equal_int(3, (long long)seen, "IM06: as_queryable skips tombstones, visits 3 entries");
   TestBit.is_equal_int(8, sum, "IM06: as_queryable sees expected values (1+3+4)");

   Map.dispose(m);
}

/* ----------------------------------------------------------------- */
/* IM07 - Map.as_queryable + Query.first: find entry by value          */
/* ----------------------------------------------------------------- */
static bool value_is_3(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   return ((const map_entry *)element)->value == 3;
}

static bool value_is_999(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   return ((const map_entry *)element)->value == 999;
}

static void test_im07_map_as_queryable_first_by_value(void) {
   map m = Map.new(8);
   Map.set(m, "a", 1, 1);
   Map.set(m, "b", 1, 2);
   Map.set(m, "c", 1, 3);

   TestBit.is_true(Query.first(Map.as_queryable(m), value_is_3, NULL, NULL),
                   "IM07: Query.first finds entry with value 3 — Map.get can't do this by value");
   TestBit.is_false(Query.first(Map.as_queryable(m), value_is_999, NULL, NULL),
                    "IM07: Query.first returns false for a value that isn't present");

   Map.dispose(m);
}

/* ----------------------------------------------------------------- */
/* IM08 - Map.keys yields every key, skipping tombstones              */
/* ----------------------------------------------------------------- */
static void test_im08_map_keys(void) {
   map m = Map.new(8);
   Map.set(m, "alpha", 5, 1);
   Map.set(m, "beta", 4, 2);
   Map.remove(m, "beta", 4); /* tombstone */
   Map.set(m, "gamma", 5, 3);

   sc_queryable q = Map.keys(m);
   const void *elem;
   int seen = 0;
   bool saw_alpha = false, saw_gamma = false, saw_beta = false;
   while (Query.next(&q, &elem, NULL)) {
      const sc_key_view *k = elem;
      if (k->len == 5 && memcmp(k->ptr, "alpha", 5) == 0) saw_alpha = true;
      if (k->len == 5 && memcmp(k->ptr, "gamma", 5) == 0) saw_gamma = true;
      if (k->len == 4 && memcmp(k->ptr, "beta", 4) == 0) saw_beta = true;
      seen++;
   }

   TestBit.is_equal_int(2, (long long)seen, "IM08: keys visits 2 live entries");
   TestBit.is_true(saw_alpha, "IM08: alpha present");
   TestBit.is_true(saw_gamma, "IM08: gamma present");
   TestBit.is_false(saw_beta, "IM08: removed key beta not present");

   Map.dispose(m);
}

/* ----------------------------------------------------------------- */
/* IM09 - Map.values yields every value, skipping tombstones          */
/* ----------------------------------------------------------------- */
static void test_im09_map_values(void) {
   map m = Map.new(8);
   Map.set(m, "a", 1, 10);
   Map.set(m, "b", 1, 20);
   Map.remove(m, "a", 1); /* tombstone */
   Map.set(m, "c", 1, 30);

   sc_queryable q = Map.values(m);
   const void *elem;
   int seen = 0;
   long long sum = 0;
   while (Query.next(&q, &elem, NULL)) {
      sum += (long long)*(const addr *)elem;
      seen++;
   }

   TestBit.is_equal_int(2, (long long)seen, "IM09: values visits 2 live entries");
   TestBit.is_equal_int(50, sum, "IM09: values sees expected values (20+30)");

   Map.dispose(m);
}

/* ----------------------------------------------------------------- */
/* IM10 - removed-then-reinserted key yields exactly once, fresh value*/
/* ----------------------------------------------------------------- */
static void test_im10_map_reinsert_no_stale_yield(void) {
   map m = Map.new(8);
   Map.set(m, "x", 1, 1);
   Map.remove(m, "x", 1);
   Map.set(m, "x", 1, 99);

   sc_queryable q = Map.as_queryable(m);
   const void *elem;
   int seen = 0;
   long long last_value = -1;
   while (Query.next(&q, &elem, NULL)) {
      const map_entry *entry = elem;
      if (entry->key_len == 1 && entry->key[0] == 'x') {
         seen++;
         last_value = (long long)entry->value;
      }
   }

   TestBit.is_equal_int(1, (long long)seen, "IM10: reinserted key yields exactly once");
   TestBit.is_equal_int(99, last_value, "IM10: reinserted key carries the fresh value");

   Map.dispose(m);
}

int main(void) {
   TestBit.run_ex("IM01_map_new", NULL, test_im01_map_new, td);
   TestBit.run_ex("IM02_map_set_get_update", NULL, test_im02_map_set_get_update, td);
   TestBit.run_ex("IM03_map_has_remove", NULL, test_im03_map_has_remove, td);
   TestBit.run_ex("IM04_map_resize_preserves", NULL, test_im04_map_resize_preserves, td);
   TestBit.run_ex("IM05_map_sparse_iterator", NULL, test_im05_map_sparse_iterator, td);
   TestBit.run_ex("IM06_map_as_queryable_skips_tombstones", NULL,
                  test_im06_map_as_queryable_skips_tombstones, td);
   TestBit.run_ex("IM07_map_as_queryable_first_by_value", NULL,
                  test_im07_map_as_queryable_first_by_value, td);
   TestBit.run_ex("IM08_map_keys", NULL, test_im08_map_keys, td);
   TestBit.run_ex("IM09_map_values", NULL, test_im09_map_values, td);
   TestBit.run_ex("IM10_map_reinsert_no_stale_yield", NULL, test_im10_map_reinsert_no_stale_yield, td);

   return TestBit.report();
}
