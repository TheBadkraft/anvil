/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_slotarray.c - Sigma tests: SlotArray vtable
 * ----------------------------------------------------------------------- *
 */
#include "sigma/slotarray.h"
#include "sigma/collections.h"
#include "sigma/query.h"
#include "testbit.h"

static void td(void) {}

/* ----------------------------------------------------------------- */
/* IS01 - SlotArray.new/add/get_at round-trip                         */
/* ----------------------------------------------------------------- */
static void test_is01_slotarray_new_add_get(void) {
   slotarray sa = SlotArray.new(4);
   TestBit.is_not_null(sa, "IS01: SlotArray.new returns non-null");

   int a = 10, b = 20;
   int idx_a = SlotArray.add(sa, &a);
   int idx_b = SlotArray.add(sa, &b);
   TestBit.is_true(idx_a >= 0, "IS01: add a returns a valid slot");
   TestBit.is_true(idx_b >= 0, "IS01: add b returns a valid slot");

   object out = NULL;
   TestBit.is_equal_int(0, (long long)SlotArray.get_at(sa, idx_a, &out), "IS01: get_at a OK");
   TestBit.is_equal_int(10, (long long)*(int *)out, "IS01: value at a is 10");

   SlotArray.dispose(sa);
}

/* ----------------------------------------------------------------- */
/* IS02 - SlotArray.remove_at frees a slot for reuse                  */
/* ----------------------------------------------------------------- */
static void test_is02_slotarray_remove_reuses_slot(void) {
   slotarray sa = SlotArray.new(2);
   int a = 1, b = 2, c = 3;
   int idx_a = SlotArray.add(sa, &a);
   SlotArray.add(sa, &b);

   TestBit.is_equal_int(0, (long long)SlotArray.remove_at(sa, idx_a), "IS02: remove_at a OK");
   TestBit.is_true(SlotArray.is_empty_slot(sa, idx_a), "IS02: slot a is empty after remove");

   int idx_c = SlotArray.add(sa, &c);
   TestBit.is_equal_int(idx_a, (long long)idx_c, "IS02: freed slot is reused by next add");

   SlotArray.dispose(sa);
}

/* ----------------------------------------------------------------- */
/* IS03 - SlotArray.clear empties every slot                          */
/* ----------------------------------------------------------------- */
static void test_is03_slotarray_clear(void) {
   slotarray sa = SlotArray.new(3);
   int a = 1, b = 2;
   SlotArray.add(sa, &a);
   SlotArray.add(sa, &b);

   SlotArray.clear(sa);

   for (usize i = 0; i < SlotArray.capacity(sa); i++) {
      TestBit.is_true(SlotArray.is_empty_slot(sa, i), "IS03: every slot empty after clear");
   }

   SlotArray.dispose(sa);
}

/* ----------------------------------------------------------------- */
/* IS04 - SlotArray.create_iterator visits only occupied slots        */
/* ----------------------------------------------------------------- */
static void test_is04_slotarray_sparse_iterator(void) {
   slotarray sa = SlotArray.new(4);
   int a = 1, b = 2, c = 3;
   int idx_a = SlotArray.add(sa, &a);
   SlotArray.add(sa, &b);
   SlotArray.add(sa, &c);
   SlotArray.remove_at(sa, idx_a);

   sparse_iterator it = SlotArray.create_iterator(sa);
   TestBit.is_not_null(it, "IS04: create_iterator returns non-null");

   int seen = 0;
   while (SparseIterator.next(it)) {
      seen++;
   }
   TestBit.is_equal_int(2, (long long)seen, "IS04: iterator visits only the 2 occupied slots");

   SparseIterator.dispose(it);
   SlotArray.dispose(sa);
}

/* ----------------------------------------------------------------- */
/* IS05 - SlotArray.as_queryable skips empty slots                    */
/* ----------------------------------------------------------------- */
static void test_is05_slotarray_as_queryable_skips_empty(void) {
   slotarray sa = SlotArray.new(4);
   int a = 1, b = 2, c = 3;
   int idx_a = SlotArray.add(sa, &a);
   SlotArray.add(sa, &b);
   SlotArray.add(sa, &c);
   SlotArray.remove_at(sa, idx_a);

   sc_queryable q = SlotArray.as_queryable(sa);
   const void *elem;
   int seen = 0;
   long long sum = 0;
   while (Query.next(&q, &elem, NULL)) {
      addr value = *(const addr *)elem;
      sum += *(int *)value;
      seen++;
   }

   TestBit.is_equal_int(2, (long long)seen, "IS05: as_queryable visits only the 2 occupied slots");
   TestBit.is_equal_int(5, sum, "IS05: as_queryable sees expected values (2+3)");

   SlotArray.dispose(sa);
}

/* ----------------------------------------------------------------- */
/* IS06 - SlotArray.as_queryable + Query.first finds a match           */
/* ----------------------------------------------------------------- */
static bool points_to_3(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   addr value = *(const addr *)element;
   return value != ADDR_EMPTY && *(int *)value == 3;
}

static void test_is06_slotarray_as_queryable_first(void) {
   slotarray sa = SlotArray.new(4);
   int a = 1, b = 2, c = 3;
   SlotArray.add(sa, &a);
   SlotArray.add(sa, &b);
   SlotArray.add(sa, &c);

   TestBit.is_true(Query.first(SlotArray.as_queryable(sa), points_to_3, NULL, NULL),
                   "IS06: Query.first finds the element pointing to 3");

   SlotArray.dispose(sa);
}

int main(void) {
   TestBit.run_ex("IS01_slotarray_new_add_get", NULL, test_is01_slotarray_new_add_get, td);
   TestBit.run_ex("IS02_slotarray_remove_reuses_slot", NULL, test_is02_slotarray_remove_reuses_slot, td);
   TestBit.run_ex("IS03_slotarray_clear", NULL, test_is03_slotarray_clear, td);
   TestBit.run_ex("IS04_slotarray_sparse_iterator", NULL, test_is04_slotarray_sparse_iterator, td);
   TestBit.run_ex("IS05_slotarray_as_queryable_skips_empty", NULL,
                  test_is05_slotarray_as_queryable_skips_empty, td);
   TestBit.run_ex("IS06_slotarray_as_queryable_first", NULL, test_is06_slotarray_as_queryable_first, td);

   return TestBit.report();
}
