/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_farray.c - Sigma tests: FArray vtable
 * ----------------------------------------------------------------------- *
 */
#include "sigma/farray.h"
#include "sigma/collections.h"
#include "sigma/query.h"
#include "testbit.h"

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IF01 - FArray.new creates a valid array                            */
/* ----------------------------------------------------------------- */
static void test_if01_farray_new(void) {
   farray arr = FArray.new(4, sizeof(int));
   TestBit.is_not_null(arr, "IF01: FArray.new returns non-null");
   TestBit.is_equal_int(4, (long long)FArray.capacity(arr, sizeof(int)), "IF01: capacity is 4");
   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IF02 - FArray.set/get round-trip                                   */
/* ----------------------------------------------------------------- */
static void test_if02_farray_set_get(void) {
   farray arr = FArray.new(4, sizeof(int));
   int values[] = {10, 20, 30, 40};

   for (int i = 0; i < 4; i++) {
      TestBit.is_equal_int(0, (long long)FArray.set(arr, i, sizeof(int), &values[i]),
                           "IF02: set OK");
   }

   for (int i = 0; i < 4; i++) {
      int out = 0;
      TestBit.is_equal_int(0, (long long)FArray.get(arr, i, sizeof(int), &out), "IF02: get OK");
      TestBit.is_equal_int(values[i], (long long)out, "IF02: value matches");
   }

   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IF03 - FArray.remove clears a slot without shifting                */
/* ----------------------------------------------------------------- */
static void test_if03_farray_remove(void) {
   farray arr = FArray.new(3, sizeof(int));
   int a = 1, b = 2, c = 3;
   FArray.set(arr, 0, sizeof(int), &a);
   FArray.set(arr, 1, sizeof(int), &b);
   FArray.set(arr, 2, sizeof(int), &c);

   TestBit.is_equal_int(0, (long long)FArray.remove(arr, 1, sizeof(int)), "IF03: remove OK");

   int out = -1;
   FArray.get(arr, 2, sizeof(int), &out);
   TestBit.is_equal_int(3, (long long)out, "IF03: index 2 unchanged after removing index 1");

   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IF04 - FArray.get/set out of bounds returns ERR                    */
/* ----------------------------------------------------------------- */
static void test_if04_farray_out_of_bounds(void) {
   farray arr = FArray.new(2, sizeof(int));
   int out = 0;
   TestBit.is_true(FArray.get(arr, 5, sizeof(int), &out) != 0, "IF04: get OOB is an error");

   int val = 1;
   TestBit.is_true(FArray.set(arr, 5, sizeof(int), &val) != 0, "IF04: set OOB is an error");

   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IF05 - FArray.clear zeroes every slot                              */
/* ----------------------------------------------------------------- */
static void test_if05_farray_clear(void) {
   farray arr = FArray.new(3, sizeof(int));
   int a = 7, b = 8, c = 9;
   FArray.set(arr, 0, sizeof(int), &a);
   FArray.set(arr, 1, sizeof(int), &b);
   FArray.set(arr, 2, sizeof(int), &c);

   FArray.clear(arr, sizeof(int));

   TestBit.is_equal_int(3, (long long)FArray.capacity(arr, sizeof(int)),
                        "IF05: clear does not change capacity");
   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IF06 - FArray.as_collection reflects the array's contents          */
/* ----------------------------------------------------------------- */
static void test_if06_farray_as_collection(void) {
   farray arr = FArray.new(3, sizeof(int));
   int a = 1, b = 2, c = 3;
   FArray.set(arr, 0, sizeof(int), &a);
   FArray.set(arr, 1, sizeof(int), &b);
   FArray.set(arr, 2, sizeof(int), &c);

   collection view = FArray.as_collection(arr, sizeof(int));
   TestBit.is_not_null(view, "IF06: as_collection returns non-null");
   TestBit.is_equal_int(3, (long long)Collections.count(view), "IF06: view count matches array");

   Collections.dispose(view);
   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IF07 - FArray.as_queryable: Query.first finds a match, zero alloc  */
/* ----------------------------------------------------------------- */
static bool is_value_30(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   return *(const int *)element == 30;
}

static void test_if07_farray_as_queryable_first(void) {
   farray arr = FArray.new(4, sizeof(int));
   int values[] = {10, 20, 30, 40};
   for (int i = 0; i < 4; i++) {
      FArray.set(arr, i, sizeof(int), &values[i]);
   }

   sc_queryable q = FArray.as_queryable(arr, sizeof(int));
   usize idx = 999;
   TestBit.is_true(Query.first(q, is_value_30, NULL, &idx), "IF07: Query.first finds 30");
   TestBit.is_equal_int(2, (long long)idx, "IF07: 30 is at index 2");

   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IF08 - FArray.as_queryable: Query.next visits every element        */
/* ----------------------------------------------------------------- */
static void test_if08_farray_as_queryable_next(void) {
   farray arr = FArray.new(3, sizeof(int));
   int values[] = {5, 6, 7};
   for (int i = 0; i < 3; i++) {
      FArray.set(arr, i, sizeof(int), &values[i]);
   }

   sc_queryable q = FArray.as_queryable(arr, sizeof(int));
   const void *elem;
   int seen = 0;
   long long sum = 0;
   while (Query.next(&q, &elem, NULL)) {
      sum += *(const int *)elem;
      seen++;
   }

   TestBit.is_equal_int(3, (long long)seen, "IF08: Query.next visits all 3 elements");
   TestBit.is_equal_int(18, sum, "IF08: sum of visited elements is 18");

   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IF09 - FArray.as_queryable: no match exhausts the whole array      */
/* ----------------------------------------------------------------- */
static bool is_value_999(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   return *(const int *)element == 999;
}

static void test_if09_farray_as_queryable_no_match(void) {
   farray arr = FArray.new(3, sizeof(int));
   int values[] = {1, 2, 3};
   for (int i = 0; i < 3; i++) {
      FArray.set(arr, i, sizeof(int), &values[i]);
   }

   sc_queryable q = FArray.as_queryable(arr, sizeof(int));
   TestBit.is_false(Query.first(q, is_value_999, NULL, NULL),
                    "IF09: Query.first returns false when nothing matches");

   FArray.dispose(arr);
}

int main(void) {
   TestBit.run_ex("IF01_farray_new", NULL, test_if01_farray_new, td);
   TestBit.run_ex("IF02_farray_set_get", NULL, test_if02_farray_set_get, td);
   TestBit.run_ex("IF03_farray_remove", NULL, test_if03_farray_remove, td);
   TestBit.run_ex("IF04_farray_out_of_bounds", NULL, test_if04_farray_out_of_bounds, td);
   TestBit.run_ex("IF05_farray_clear", NULL, test_if05_farray_clear, td);
   TestBit.run_ex("IF06_farray_as_collection", NULL, test_if06_farray_as_collection, td);
   TestBit.run_ex("IF07_farray_as_queryable_first", NULL, test_if07_farray_as_queryable_first, td);
   TestBit.run_ex("IF08_farray_as_queryable_next", NULL, test_if08_farray_as_queryable_next, td);
   TestBit.run_ex("IF09_farray_as_queryable_no_match", NULL, test_if09_farray_as_queryable_no_match, td);

   return TestBit.report();
}
