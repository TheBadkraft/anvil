/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_parray.c - Sigma tests: PArray vtable
 * ----------------------------------------------------------------------- *
 */
#include "sigma/parray.h"
#include "sigma/collections.h"
#include "sigma/slotarray.h"
#include "sigma/query.h"
#include "testbit.h"

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IP01 - PArray.new creates a valid array                            */
/* ----------------------------------------------------------------- */
static void test_ip01_parray_new(void) {
   parray arr = PArray.new(5);
   TestBit.is_not_null(arr, "IP01: PArray.new returns non-null");
   TestBit.is_equal_int(5, (long long)PArray.capacity(arr), "IP01: capacity is 5");
   PArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IP02 - PArray.set/get round-trip                                   */
/* ----------------------------------------------------------------- */
static void test_ip02_parray_set_get(void) {
   parray arr = PArray.new(4);
   int values[] = {100, 200, 300, 400};

   for (int i = 0; i < 4; i++) {
      TestBit.is_equal_int(0, (long long)PArray.set(arr, i, (addr)&values[i]), "IP02: set OK");
   }

   for (int i = 0; i < 4; i++) {
      addr out = 0;
      TestBit.is_equal_int(0, (long long)PArray.get(arr, i, &out), "IP02: get OK");
      TestBit.is_equal_int(values[i], (long long)*(int *)out, "IP02: value matches");
   }

   PArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IP03 - PArray.remove clears a slot to ADDR_EMPTY                   */
/* ----------------------------------------------------------------- */
static void test_ip03_parray_remove(void) {
   parray arr = PArray.new(3);
   int a = 1, b = 2, c = 3;
   PArray.set(arr, 0, (addr)&a);
   PArray.set(arr, 1, (addr)&b);
   PArray.set(arr, 2, (addr)&c);

   TestBit.is_equal_int(0, (long long)PArray.remove(arr, 1), "IP03: remove OK");

   addr out = 1;
   PArray.get(arr, 1, &out);
   TestBit.is_equal_int((long long)ADDR_EMPTY, (long long)out, "IP03: removed slot is ADDR_EMPTY");

   PArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IP04 - PArray.get/set out of bounds returns ERR                    */
/* ----------------------------------------------------------------- */
static void test_ip04_parray_out_of_bounds(void) {
   parray arr = PArray.new(2);
   addr out = 0;
   TestBit.is_true(PArray.get(arr, 10, &out) != 0, "IP04: get OOB is an error");
   TestBit.is_true(PArray.set(arr, 10, (addr)1) != 0, "IP04: set OOB is an error");
   PArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IP05 - PArray.as_collection reflects the array's contents          */
/* ----------------------------------------------------------------- */
static void test_ip05_parray_as_collection(void) {
   parray arr = PArray.new(3);
   int a = 1, b = 2, c = 3;
   PArray.set(arr, 0, (addr)&a);
   PArray.set(arr, 1, (addr)&b);
   PArray.set(arr, 2, (addr)&c);

   collection view = PArray.as_collection(arr);
   TestBit.is_not_null(view, "IP05: as_collection returns non-null");
   TestBit.is_equal_int(3, (long long)Collections.count(view), "IP05: view count matches array");

   Collections.dispose(view);
   PArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IP06 - PArray.as_slotarray creates a valid view                    */
/* ----------------------------------------------------------------- */
static void test_ip06_parray_as_slotarray(void) {
   parray arr = PArray.new(4);
   slotarray sa = PArray.as_slotarray(arr);
   TestBit.is_not_null(sa, "IP06: as_slotarray returns non-null");

   SlotArray.dispose(sa); /* view: does not own arr */
   PArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IP07 - PArray.as_queryable: Query.first finds a match, zero alloc  */
/* ----------------------------------------------------------------- */
static int target_value = 30;
static bool points_to_target(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   addr value = *(const addr *)element;
   return value != ADDR_EMPTY && *(int *)value == target_value;
}

static void test_ip07_parray_as_queryable_first(void) {
   parray arr = PArray.new(4);
   int values[] = {10, 20, 30, 40};
   for (int i = 0; i < 4; i++) {
      PArray.set(arr, i, (addr)&values[i]);
   }

   sc_queryable q = PArray.as_queryable(arr);
   usize idx = 999;
   TestBit.is_true(Query.first(q, points_to_target, NULL, &idx), "IP07: Query.first finds 30");
   TestBit.is_equal_int(2, (long long)idx, "IP07: 30 is at index 2");

   PArray.dispose(arr);
}

/* ----------------------------------------------------------------- */
/* IP08 - PArray.as_queryable: Query.next visits every element        */
/* ----------------------------------------------------------------- */
static void test_ip08_parray_as_queryable_next(void) {
   parray arr = PArray.new(3);
   int values[] = {5, 6, 7};
   for (int i = 0; i < 3; i++) {
      PArray.set(arr, i, (addr)&values[i]);
   }

   sc_queryable q = PArray.as_queryable(arr);
   const void *elem;
   int seen = 0;
   long long sum = 0;
   while (Query.next(&q, &elem, NULL)) {
      addr value = *(const addr *)elem;
      sum += *(int *)value;
      seen++;
   }

   TestBit.is_equal_int(3, (long long)seen, "IP08: Query.next visits all 3 elements");
   TestBit.is_equal_int(18, sum, "IP08: sum of visited elements is 18");

   PArray.dispose(arr);
}

int main(void) {
   TestBit.run_ex("IP01_parray_new", NULL, test_ip01_parray_new, td);
   TestBit.run_ex("IP02_parray_set_get", NULL, test_ip02_parray_set_get, td);
   TestBit.run_ex("IP03_parray_remove", NULL, test_ip03_parray_remove, td);
   TestBit.run_ex("IP04_parray_out_of_bounds", NULL, test_ip04_parray_out_of_bounds, td);
   TestBit.run_ex("IP05_parray_as_collection", NULL, test_ip05_parray_as_collection, td);
   TestBit.run_ex("IP06_parray_as_slotarray", NULL, test_ip06_parray_as_slotarray, td);
   TestBit.run_ex("IP07_parray_as_queryable_first", NULL, test_ip07_parray_as_queryable_first, td);
   TestBit.run_ex("IP08_parray_as_queryable_next", NULL, test_ip08_parray_as_queryable_next, td);

   return TestBit.report();
}
