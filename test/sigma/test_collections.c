/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_collections.c - Sigma tests: Collections vtable
 * ----------------------------------------------------------------------- *
 */
#include "sigma/collections.h"
#include "sigma/farray.h"
#include "testbit.h"

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IC01 - Collections.create_view/count reflect the source array      */
/* ----------------------------------------------------------------- */
static void test_ic01_create_view_count(void) {
   farray arr = FArray.new(3, sizeof(int));
   int a = 1, b = 2, c = 3;
   FArray.set(arr, 0, sizeof(int), &a);
   FArray.set(arr, 1, sizeof(int), &b);
   FArray.set(arr, 2, sizeof(int), &c);

   collection view = FArray.as_collection(arr, sizeof(int));
   TestBit.is_not_null(view, "IC01: as_collection returns non-null");
   TestBit.is_equal_int(3, (long long)Collections.count(view), "IC01: count matches source");

   Collections.dispose(view);
   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IC02 - Collections.as_queryable: Query.first finds a match          */
/* ----------------------------------------------------------------- */
static bool is_value_30(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   return *(const int *)element == 30;
}

static void test_ic02_as_queryable_first(void) {
   farray arr = FArray.new(4, sizeof(int));
   int values[] = {10, 20, 30, 40};
   for (int i = 0; i < 4; i++) {
      FArray.set(arr, i, sizeof(int), &values[i]);
   }
   collection view = FArray.as_collection(arr, sizeof(int));

   sc_queryable q = Collections.as_queryable(view);
   usize idx = 999;
   TestBit.is_true(Query.first(q, is_value_30, NULL, &idx), "IC02: Query.first finds 30");
   TestBit.is_equal_int(2, (long long)idx, "IC02: 30 is at index 2");

   Collections.dispose(view);
   FArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IC03 - Collections.as_queryable: Query.next visits every element    */
/* ----------------------------------------------------------------- */
static void test_ic03_as_queryable_next(void) {
   farray arr = FArray.new(3, sizeof(int));
   int values[] = {5, 6, 7};
   for (int i = 0; i < 3; i++) {
      FArray.set(arr, i, sizeof(int), &values[i]);
   }
   collection view = FArray.as_collection(arr, sizeof(int));

   sc_queryable q = Collections.as_queryable(view);
   const void *elem;
   int seen = 0;
   while (Query.next(&q, &elem, NULL)) {
      seen++;
   }
   TestBit.is_equal_int(3, (long long)seen, "IC03: Query.next visits all 3 elements");

   Collections.dispose(view);
   FArray.dispose(arr);
}

int main(void) {
   TestBit.run_ex("IC01_create_view_count", NULL, test_ic01_create_view_count, td);
   TestBit.run_ex("IC02_as_queryable_first", NULL, test_ic02_as_queryable_first, td);
   TestBit.run_ex("IC03_as_queryable_next", NULL, test_ic03_as_queryable_next, td);

   return TestBit.report();
}
