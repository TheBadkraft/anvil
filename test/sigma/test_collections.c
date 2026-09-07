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
#include "fake_arena.h"

static void td(void) {}
static void td_arena(void) { fake_arena_reset(); }

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

/* ----------------------------------------------------------------- */
/* IC04 - create_with_allocator(NULL) behaves exactly like the plain   */
/*        global-Allocator-backed path (FR-2603-sigma-collections-007) */
/* ----------------------------------------------------------------- */
static void test_ic04_create_with_allocator_null_is_default_path(void) {
   collection coll = Collections.create_with_allocator(4, sizeof(int), NULL);
   TestBit.is_not_null(coll, "IC04: create_with_allocator(NULL) returns non-null");

   int a = 1, b = 2;
   TestBit.is_equal_int(0, (long long)Collections.add(coll, &a), "IC04: add OK");
   TestBit.is_equal_int(0, (long long)Collections.add(coll, &b), "IC04: add OK");
   TestBit.is_equal_int(2, (long long)Collections.count(coll), "IC04: count is 2");

   Collections.dispose(coll);
}

/* ----------------------------------------------------------------- */
/* IC05 - create_with_allocator(&use) grows through the override and   */
/*        orphans the old buffer instead of releasing it               */
/* ----------------------------------------------------------------- */
static void test_ic05_create_with_allocator_grow_orphans(void) {
   fake_arena_reset();

   collection coll = Collections.create_with_allocator(2, sizeof(int), &fake_arena_use);
   TestBit.is_not_null(coll, "IC05: create_with_allocator(&use) returns non-null");
   TestBit.is_true(fake_arena_alloc_calls >= 1, "IC05: initial buffer allocated through override");

   int before_grow_calls = fake_arena_alloc_calls;
   int values[8];
   for (int i = 0; i < 8; i++) {
      values[i] = i * 10;
      TestBit.is_equal_int(0, (long long)Collections.add(coll, &values[i]), "IC05: add OK while growing");
   }

   TestBit.is_equal_int(8, (long long)Collections.count(coll), "IC05: count is 8 after growth");
   TestBit.is_true(fake_arena_alloc_calls > before_grow_calls,
                   "IC05: growth allocated new buffer(s) through the override");
   TestBit.is_equal_int(0, (long long)fake_arena_release_calls,
                        "IC05: growth never released the orphaned buffer through the override");

   Collections.dispose(coll);
}

/* ----------------------------------------------------------------- */
/* IC06 - collection_dispose skips releasing the bucket through the    */
/*        override — the bound allocator owns bulk-reclaiming it       */
/* ----------------------------------------------------------------- */
static void test_ic06_dispose_skips_release_when_overridden(void) {
   fake_arena_reset();

   collection coll = Collections.create_with_allocator(4, sizeof(int), &fake_arena_use);
   int a = 1;
   Collections.add(coll, &a);

   Collections.dispose(coll);

   TestBit.is_equal_int(0, (long long)fake_arena_release_calls,
                        "IC06: dispose never released the buffer through the override");
}

int main(void) {
   TestBit.run_ex("IC01_create_view_count", NULL, test_ic01_create_view_count, td);
   TestBit.run_ex("IC02_as_queryable_first", NULL, test_ic02_as_queryable_first, td);
   TestBit.run_ex("IC03_as_queryable_next", NULL, test_ic03_as_queryable_next, td);
   TestBit.run_ex("IC04_create_with_allocator_null_is_default_path", NULL,
                  test_ic04_create_with_allocator_null_is_default_path, td);
   TestBit.run_ex("IC05_create_with_allocator_grow_orphans", NULL,
                  test_ic05_create_with_allocator_grow_orphans, td_arena);
   TestBit.run_ex("IC06_dispose_skips_release_when_overridden", NULL,
                  test_ic06_dispose_skips_release_when_overridden, td_arena);

   return TestBit.report();
}
