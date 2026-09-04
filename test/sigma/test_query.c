/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_query.c - Sigma tests: Query mechanism (Query.next / Query.first)
 * ----------------------------------------------------------------------- *
 * Tests the generic sc_queryable/Query engine in isolation, via a small
 * hand-rolled advance function over a plain int[] — independent of any
 * real collection type. FArray/PArray/Collections/List/Map/SlotArray each
 * verify their own `as_queryable`/`keys`/`values` wiring in their own test
 * files; this file only proves the shared mechanism itself is correct.
 */
#include "sigma/query.h"
#include "testbit.h"

static void td(void) {}

/* ----------------------------------------------------------------- */
/* A minimal dense source: a plain int[] with an explicit count.      */
/* ----------------------------------------------------------------- */
typedef struct {
   int *values;
   usize count;
} int_source;

static bool int_source_advance(sc_queryable *self, const void **out_element, usize *out_index) {
   int_source *src = (int_source *)self->source;
   if (self->index >= src->count) {
      return false;
   }
   usize i = self->index++;
   *out_element = &src->values[i];
   if (out_index) {
      *out_index = i;
   }
   return true;
}

static sc_queryable int_source_as_queryable(int_source *src) {
   return (sc_queryable){.source = src, .element_size = sizeof(int), .index = 0,
                         .advance = int_source_advance};
}

static bool is_even(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   return (*(const int *)element % 2) == 0;
}

static bool always_false(const void *element, usize index, void *userdata) {
   (void)element;
   (void)index;
   (void)userdata;
   return false;
}

/* ----------------------------------------------------------------- */
/* QRY01 - Query.next visits every element in order, then exhausts    */
/* ----------------------------------------------------------------- */
static void test_qry01_next_visits_in_order(void) {
   int values[] = {10, 20, 30};
   int_source src = {values, 3};
   sc_queryable q = int_source_as_queryable(&src);

   const void *elem;
   usize idx;

   TestBit.is_true(Query.next(&q, &elem, &idx), "QRY01: first next succeeds");
   TestBit.is_equal_int(10, *(const int *)elem, "QRY01: first element is 10");
   TestBit.is_equal_int(0, (long long)idx, "QRY01: first index is 0");

   TestBit.is_true(Query.next(&q, &elem, &idx), "QRY01: second next succeeds");
   TestBit.is_equal_int(20, *(const int *)elem, "QRY01: second element is 20");
   TestBit.is_equal_int(1, (long long)idx, "QRY01: second index is 1");

   TestBit.is_true(Query.next(&q, &elem, &idx), "QRY01: third next succeeds");
   TestBit.is_equal_int(30, *(const int *)elem, "QRY01: third element is 30");
   TestBit.is_equal_int(2, (long long)idx, "QRY01: third index is 2");

   TestBit.is_false(Query.next(&q, &elem, &idx), "QRY01: fourth next exhausts the source");
}

/* ----------------------------------------------------------------- */
/* QRY02 - Query.first finds a match and reports its index            */
/* ----------------------------------------------------------------- */
static void test_qry02_first_finds_match(void) {
   int values[] = {1, 3, 4, 7};
   int_source src = {values, 4};
   sc_queryable q = int_source_as_queryable(&src);

   usize idx = 999;
   bool found = Query.first(q, is_even, NULL, &idx);

   TestBit.is_true(found, "QRY02: first finds the even element");
   TestBit.is_equal_int(2, (long long)idx, "QRY02: out_index is 2 (value 4)");
}

/* ----------------------------------------------------------------- */
/* QRY03 - Query.first returns false when no element matches          */
/* ----------------------------------------------------------------- */
static void test_qry03_first_no_match(void) {
   int values[] = {1, 3, 5, 7};
   int_source src = {values, 4};
   sc_queryable q = int_source_as_queryable(&src);

   usize idx = 999;
   bool found = Query.first(q, is_even, NULL, &idx);

   TestBit.is_false(found, "QRY03: first returns false when nothing matches");
   TestBit.is_equal_int(999, (long long)idx, "QRY03: out_index left untouched on no match");
}

/* ----------------------------------------------------------------- */
/* QRY04 - Query.first "any" usage: caller passes NULL for out_index  */
/* ----------------------------------------------------------------- */
static void test_qry04_first_any_usage(void) {
   int values[] = {2, 4, 6};
   int_source src = {values, 3};
   sc_queryable q = int_source_as_queryable(&src);

   TestBit.is_true(Query.first(q, is_even, NULL, NULL), "QRY04: any-style call with NULL out_index");
}

/* ----------------------------------------------------------------- */
/* QRY05 - Query.first stops scanning at the first match (early break)*/
/* ----------------------------------------------------------------- */
static int call_count = 0;
static bool counting_is_even(const void *element, usize index, void *userdata) {
   (void)index;
   (void)userdata;
   call_count++;
   return (*(const int *)element % 2) == 0;
}

static void test_qry05_first_early_break(void) {
   int values[] = {1, 1, 2, 1, 1};
   int_source src = {values, 5};
   sc_queryable q = int_source_as_queryable(&src);

   call_count = 0;
   usize idx = 999;
   bool found = Query.first(q, counting_is_even, NULL, &idx);

   TestBit.is_true(found, "QRY05: match found");
   TestBit.is_equal_int(2, (long long)idx, "QRY05: match at index 2");
   TestBit.is_equal_int(3, (long long)call_count,
                        "QRY05: predicate called exactly 3 times, not 5 (early break)");
}

/* ----------------------------------------------------------------- */
/* QRY06 - empty source: next and first both report nothing found     */
/* ----------------------------------------------------------------- */
static void test_qry06_empty_source(void) {
   int_source src = {NULL, 0};
   sc_queryable q = int_source_as_queryable(&src);

   const void *elem;
   TestBit.is_false(Query.next(&q, &elem, NULL), "QRY06: next on empty source returns false");

   sc_queryable q2 = int_source_as_queryable(&src);
   TestBit.is_false(Query.first(q2, is_even, NULL, NULL), "QRY06: first on empty source returns false");
}

/* ----------------------------------------------------------------- */
/* QRY07 - NULL-argument invariants                                   */
/* ----------------------------------------------------------------- */
static void test_qry07_null_invariants(void) {
   TestBit.is_false(Query.next(NULL, NULL, NULL), "QRY07: next(NULL, ...) returns false, no crash");

   sc_queryable zeroed = {0};
   const void *elem;
   TestBit.is_false(Query.next(&zeroed, &elem, NULL),
                    "QRY07: next on a zero-initialized queryable (NULL advance) returns false");

   int values[] = {1, 2, 3};
   int_source src = {values, 3};
   sc_queryable q = int_source_as_queryable(&src);
   TestBit.is_false(Query.first(q, NULL, NULL, NULL), "QRY07: first(..., NULL pred, ...) returns false");
}

/* ----------------------------------------------------------------- */
/* QRY08 - always-false predicate exhausts the whole source            */
/* ----------------------------------------------------------------- */
static void test_qry08_first_exhausts_on_never_match(void) {
   int values[] = {1, 2, 3, 4, 5};
   int_source src = {values, 5};
   sc_queryable q = int_source_as_queryable(&src);

   TestBit.is_false(Query.first(q, always_false, NULL, NULL),
                    "QRY08: predicate that never matches exhausts the source cleanly");
}

int main(void) {
   TestBit.run_ex("QRY01_next_visits_in_order", NULL, test_qry01_next_visits_in_order, td);
   TestBit.run_ex("QRY02_first_finds_match", NULL, test_qry02_first_finds_match, td);
   TestBit.run_ex("QRY03_first_no_match", NULL, test_qry03_first_no_match, td);
   TestBit.run_ex("QRY04_first_any_usage", NULL, test_qry04_first_any_usage, td);
   TestBit.run_ex("QRY05_first_early_break", NULL, test_qry05_first_early_break, td);
   TestBit.run_ex("QRY06_empty_source", NULL, test_qry06_empty_source, td);
   TestBit.run_ex("QRY07_null_invariants", NULL, test_qry07_null_invariants, td);
   TestBit.run_ex("QRY08_first_exhausts_on_never_match", NULL, test_qry08_first_exhausts_on_never_match, td);

   return TestBit.report();
}
