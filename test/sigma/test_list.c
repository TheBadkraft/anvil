/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_list.c - Infrastructure tests: List vtable
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/infra/test_list.c
 */
#include "sigma/list.h"
#include "sigma/query.h"
#include "testbit.h"

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IL01 — List.new creates a valid list                               */
/* ----------------------------------------------------------------- */
static void test_il01_list_new(void) {
   list lst = List.new(4, sizeof(void *));
   TestBit.is_not_null(lst, "IL01: List.new returns non-null");
   TestBit.is_equal_int(0, (long long)List.size(lst), "IL01: initial size is 0");
   TestBit.is_true(List.capacity(lst) >= 4, "IL01: capacity at least 4");
   List.dispose(lst);
}

/* ----------------------------------------------------------------- */
/* IL02 — List.append and size                                        */
/* ----------------------------------------------------------------- */
static void test_il02_list_append(void) {
   list lst = List.new(4, sizeof(void *));

   int a = 1, b = 2, c = 3;
   List.append(lst, &a);
   List.append(lst, &b);
   List.append(lst, &c);

   TestBit.is_equal_int(3, (long long)List.size(lst), "IL02: size is 3 after 3 appends");
   List.dispose(lst);
}

/* ----------------------------------------------------------------- */
/* IL03 — List.get retrieves correct element                          */
/* ----------------------------------------------------------------- */
static void test_il03_list_get(void) {
   list lst = List.new(4, sizeof(void *));

   int a = 10, b = 20, c = 30;
   List.append(lst, &a);
   List.append(lst, &b);
   List.append(lst, &c);

   void *out = NULL;
   int r = List.get(lst, 1, &out);
   TestBit.is_equal_int(0, (long long)r, "IL03: get at index 1 returns OK");
   TestBit.is_not_null(out, "IL03: out value is non-null");
   TestBit.is_equal_int(20, (long long)*(int *)out, "IL03: value at index 1 is 20");

   List.dispose(lst);
}

/* ----------------------------------------------------------------- */
/* IL04 — List.get out of bounds returns ERR                          */
/* ----------------------------------------------------------------- */
static void test_il04_list_get_oob(void) {
   list lst = List.new(4, sizeof(void *));

   int a = 1;
   List.append(lst, &a);

   void *out = NULL;
   int r = List.get(lst, 5, &out);
   TestBit.is_equal_int(-1, (long long)r, "IL04: get out of bounds returns ERR");

   List.dispose(lst);
}

/* ----------------------------------------------------------------- */
/* IL05 — List.remove_at shifts elements correctly                    */
/* ----------------------------------------------------------------- */
static void test_il05_list_remove(void) {
   list lst = List.new(4, sizeof(void *));

   int a = 1, b = 2, c = 3;
   List.append(lst, &a);
   List.append(lst, &b);
   List.append(lst, &c);

   List.remove(lst, 1);
   TestBit.is_equal_int(2, (long long)List.size(lst), "IL05: size is 2 after remove");

   void *out = NULL;
   List.get(lst, 1, &out);
   TestBit.is_equal_int(3, (long long)*(int *)out, "IL05: index 1 is now 3 after remove");

   List.dispose(lst);
}

/* ----------------------------------------------------------------- */
/* IL06 — List.clear resets size to zero                              */
/* ----------------------------------------------------------------- */
static void test_il06_list_clear(void) {
   list lst = List.new(4, sizeof(void *));

   int a = 1, b = 2;
   List.append(lst, &a);
   List.append(lst, &b);
   List.clear(lst);

   TestBit.is_equal_int(0, (long long)List.size(lst), "IL06: size is 0 after clear");
   List.dispose(lst);
}

/* ----------------------------------------------------------------- */
/* IL07 — List grows beyond initial capacity                          */
/* ----------------------------------------------------------------- */
static void test_il07_list_grow(void) {
   list lst = List.new(2, sizeof(void *));

   int vals[8];
   for (int i = 0; i < 8; i++) {
      vals[i] = i * 10;
      List.append(lst, &vals[i]);
   }

   TestBit.is_equal_int(8, (long long)List.size(lst), "IL07: size is 8 after growing");

   void *out = NULL;
   List.get(lst, 7, &out);
   TestBit.is_equal_int(70, (long long)*(int *)out, "IL07: last element is 70");

   List.dispose(lst);
}

/* ----------------------------------------------------------------- */
/* IL08 — List iterator traverses in-order                            */
/* ----------------------------------------------------------------- */
static void test_il08_list_iterator(void) {
   list lst = List.new(4, sizeof(void *));

   int a = 11, b = 22, c = 33;
   List.append(lst, &a);
   List.append(lst, &b);
   List.append(lst, &c);

   iterator it = List.create_iterator(lst);
   TestBit.is_not_null(it, "IL08: create_iterator returns non-null");

   int expected[3] = {11, 22, 33};
   int idx = 0;

   while (Iterator.next(it)) {
      void *slot = Iterator.current(it);
      TestBit.is_not_null(slot, "IL08: iterator current slot is non-null");

      int *value = *(int **)slot;
      TestBit.is_equal_int(expected[idx], (long long)*value,
                           "IL08: iterator value matches expected order");
      idx++;
   }

   TestBit.is_equal_int(3, (long long)idx, "IL08: iterator visits all list elements");

   Iterator.reset(it);
   TestBit.is_true(Iterator.next(it), "IL08: iterator reset rewinds to start");
   void *first_slot = Iterator.current(it);
   int *first_value = *(int **)first_slot;
   TestBit.is_equal_int(11, (long long)*first_value, "IL08: first value after reset is 11");

   Iterator.dispose(it);
   List.dispose(lst);
}

/* ----------------------------------------------------------------- */
/* IL09 — List.as_queryable: Query.first finds a match, zero alloc    */
/* ----------------------------------------------------------------- */
static bool list_points_to_30(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   int *stored = *(int *const *)element;
   return stored && *stored == 30;
}

static void test_il09_list_as_queryable_first(void) {
   list lst = List.new(4, sizeof(void *));
   int a = 10, b = 20, c = 30, d = 40;
   List.append(lst, &a);
   List.append(lst, &b);
   List.append(lst, &c);
   List.append(lst, &d);

   sc_queryable q = List.as_queryable(lst);
   usize idx = 999;
   TestBit.is_true(Query.first(q, list_points_to_30, NULL, &idx), "IL09: Query.first finds 30");
   TestBit.is_equal_int(2, (long long)idx, "IL09: 30 is at index 2");

   List.dispose(lst);
}

/* ----------------------------------------------------------------- */
/* IL10 — List.as_queryable: Query.next visits every element in order */
/* ----------------------------------------------------------------- */
static void test_il10_list_as_queryable_next(void) {
   list lst = List.new(4, sizeof(void *));
   int a = 1, b = 2, c = 3;
   List.append(lst, &a);
   List.append(lst, &b);
   List.append(lst, &c);

   sc_queryable q = List.as_queryable(lst);
   const void *elem;
   int expected[3] = {1, 2, 3};
   int seen = 0;
   while (Query.next(&q, &elem, NULL)) {
      int *stored = *(int *const *)elem;
      TestBit.is_equal_int(expected[seen], (long long)*stored, "IL10: element matches expected order");
      seen++;
   }
   TestBit.is_equal_int(3, (long long)seen, "IL10: Query.next visits all 3 elements");

   List.dispose(lst);
}

int main(void) {
   TestBit.run_ex("IL01_list_new", NULL, test_il01_list_new, td);
   TestBit.run_ex("IL02_list_append", NULL, test_il02_list_append, td);
   TestBit.run_ex("IL03_list_get", NULL, test_il03_list_get, td);
   TestBit.run_ex("IL04_list_get_oob", NULL, test_il04_list_get_oob, td);
   TestBit.run_ex("IL05_list_remove", NULL, test_il05_list_remove, td);
   TestBit.run_ex("IL06_list_clear", NULL, test_il06_list_clear, td);
   TestBit.run_ex("IL07_list_grow", NULL, test_il07_list_grow, td);
   TestBit.run_ex("IL08_list_iterator", NULL, test_il08_list_iterator, td);
   TestBit.run_ex("IL09_list_as_queryable_first", NULL, test_il09_list_as_queryable_first, td);
   TestBit.run_ex("IL10_list_as_queryable_next", NULL, test_il10_list_as_queryable_next, td);

   return TestBit.report();
}