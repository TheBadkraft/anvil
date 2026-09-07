/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * test_stack.c - Sigma tests: Stack vtable
 * ----------------------------------------------------------------------- *
 */
#include "sigma/stack.h"
#include "testbit.h"
#include "fake_arena.h"

static void td(void) {}
static void td_arena(void) { fake_arena_reset(); }

/* ----------------------------------------------------------------- */
/* ST01 - Stack.new creates a valid, empty stack                      */
/* ----------------------------------------------------------------- */
static void test_st01_stack_new(void) {
   stack s = Stack.new(4);
   TestBit.is_not_null(s, "ST01: Stack.new returns non-null");
   TestBit.is_true(Stack.is_empty(s), "ST01: new stack is empty");
   TestBit.is_equal_int(0, (long long)Stack.count(s), "ST01: new stack count is 0");
   Stack.dispose(s);
}

/* ----------------------------------------------------------------- */
/* ST02 - push/pop is LIFO order                                      */
/* ----------------------------------------------------------------- */
static void test_st02_push_pop_lifo(void) {
   stack s = Stack.new(4);
   int a = 1, b = 2, c = 3;

   TestBit.is_equal_int(0, (long long)Stack.push(s, &a), "ST02: push a OK");
   TestBit.is_equal_int(0, (long long)Stack.push(s, &b), "ST02: push b OK");
   TestBit.is_equal_int(0, (long long)Stack.push(s, &c), "ST02: push c OK");
   TestBit.is_equal_int(3, (long long)Stack.count(s), "ST02: count is 3 after 3 pushes");

   object out = Stack.pop(s);
   TestBit.is_true(out == &c, "ST02: pop returns c (last pushed)");
   out = Stack.pop(s);
   TestBit.is_true(out == &b, "ST02: pop returns b");
   out = Stack.pop(s);
   TestBit.is_true(out == &a, "ST02: pop returns a (first pushed, popped last)");

   TestBit.is_true(Stack.is_empty(s), "ST02: stack empty after popping everything");

   Stack.dispose(s);
}

/* ----------------------------------------------------------------- */
/* ST03 - peek returns the top without removing it                    */
/* ----------------------------------------------------------------- */
static void test_st03_peek_does_not_mutate(void) {
   stack s = Stack.new(4);
   int a = 1, b = 2;
   Stack.push(s, &a);
   Stack.push(s, &b);

   TestBit.is_true(Stack.peek(s) == &b, "ST03: peek returns top (b)");
   TestBit.is_true(Stack.peek(s) == &b, "ST03: peek again still returns b");
   TestBit.is_equal_int(2, (long long)Stack.count(s), "ST03: peek does not change count");

   TestBit.is_true(Stack.pop(s) == &b, "ST03: pop after peek still returns b");

   Stack.dispose(s);
}

/* ----------------------------------------------------------------- */
/* ST04 - pop/peek on an empty stack return NULL                      */
/* ----------------------------------------------------------------- */
static void test_st04_pop_peek_empty(void) {
   stack s = Stack.new(4);

   TestBit.is_null(Stack.pop(s), "ST04: pop on empty stack returns NULL");
   TestBit.is_null(Stack.peek(s), "ST04: peek on empty stack returns NULL");

   Stack.dispose(s);
}

/* ----------------------------------------------------------------- */
/* ST05 - clear resets the stack to empty                             */
/* ----------------------------------------------------------------- */
static void test_st05_clear(void) {
   stack s = Stack.new(4);
   int a = 1, b = 2;
   Stack.push(s, &a);
   Stack.push(s, &b);

   Stack.clear(s);

   TestBit.is_true(Stack.is_empty(s), "ST05: stack empty after clear");
   TestBit.is_equal_int(0, (long long)Stack.count(s), "ST05: count is 0 after clear");

   Stack.dispose(s);
}

/* ----------------------------------------------------------------- */
/* ST06 - mark/restore truncates to a saved depth, is a real truncate */
/* ----------------------------------------------------------------- */
static void test_st06_mark_restore(void) {
   stack s = Stack.new(4);
   int a = 1, b = 2, c = 3, d = 4;
   Stack.push(s, &a);
   Stack.push(s, &b);

   usize m = Stack.mark(s);
   TestBit.is_equal_int(2, (long long)m, "ST06: mark returns current depth (2)");

   Stack.push(s, &c);
   TestBit.is_equal_int(3, (long long)Stack.count(s), "ST06: count is 3 after pushing past mark");

   Stack.restore(s, m);
   TestBit.is_equal_int(2, (long long)Stack.count(s), "ST06: restore truncates back to marked depth");
   TestBit.is_true(Stack.peek(s) == &b, "ST06: top after restore is b (c was discarded)");

   /* proves it's a real truncate, not a cosmetic count: pushing again reuses the slot */
   Stack.push(s, &d);
   TestBit.is_equal_int(2, (long long)m, "ST06: original mark value unaffected by reuse");
   TestBit.is_true(Stack.peek(s) == &d, "ST06: top after push-after-restore is d");
   TestBit.is_equal_int(3, (long long)Stack.count(s), "ST06: count is 3 after pushing after restore");

   Stack.dispose(s);
}

/* ----------------------------------------------------------------- */
/* ST07 - new_with_allocator(NULL) behaves exactly like Stack.new      */
/*        (FR-2603-sigma-collections-007/011)                          */
/* ----------------------------------------------------------------- */
static void test_st07_new_with_allocator_null_is_default_path(void) {
   stack s = Stack.new_with_allocator(4, NULL);
   TestBit.is_not_null(s, "ST07: new_with_allocator(NULL) returns non-null");

   int a = 1;
   Stack.push(s, &a);
   TestBit.is_equal_int(1, (long long)Stack.count(s), "ST07: push works via default path");

   Stack.dispose(s);
}

/* ----------------------------------------------------------------- */
/* ST08 - new_with_allocator(&use) grows through the override and      */
/*        orphans the old buffer instead of releasing it               */
/* ----------------------------------------------------------------- */
static void test_st08_new_with_allocator_grow_orphans(void) {
   fake_arena_reset();

   stack s = Stack.new_with_allocator(2, &fake_arena_use);
   TestBit.is_not_null(s, "ST08: new_with_allocator(&use) returns non-null");
   TestBit.is_true(fake_arena_alloc_calls >= 1, "ST08: initial buffer allocated through override");

   int before_grow_calls = fake_arena_alloc_calls;
   int vals[8];
   for (int i = 0; i < 8; i++) {
      vals[i] = i * 10;
      TestBit.is_equal_int(0, (long long)Stack.push(s, &vals[i]), "ST08: push OK while growing");
   }

   TestBit.is_equal_int(8, (long long)Stack.count(s), "ST08: count is 8 after growth");
   TestBit.is_true(fake_arena_alloc_calls > before_grow_calls,
                   "ST08: growth allocated new buffer(s) through the override");
   TestBit.is_equal_int(0, (long long)fake_arena_release_calls,
                        "ST08: growth never released the orphaned buffer through the override");

   TestBit.is_true(Stack.peek(s) == &vals[7], "ST08: top is correct after arena-backed growth");

   Stack.dispose(s);
}

/* ----------------------------------------------------------------- */
/* ST09 - dispose skips releasing the bucket through the override      */
/*        — the bound allocator owns bulk-reclaiming it                */
/* ----------------------------------------------------------------- */
static void test_st09_dispose_skips_release_when_overridden(void) {
   fake_arena_reset();

   stack s = Stack.new_with_allocator(4, &fake_arena_use);
   int a = 1;
   Stack.push(s, &a);

   Stack.dispose(s);

   TestBit.is_equal_int(0, (long long)fake_arena_release_calls,
                        "ST09: dispose never released the buffer through the override");
}

int main(void) {
   TestBit.run_ex("ST01_stack_new", NULL, test_st01_stack_new, td);
   TestBit.run_ex("ST02_push_pop_lifo", NULL, test_st02_push_pop_lifo, td);
   TestBit.run_ex("ST03_peek_does_not_mutate", NULL, test_st03_peek_does_not_mutate, td);
   TestBit.run_ex("ST04_pop_peek_empty", NULL, test_st04_pop_peek_empty, td);
   TestBit.run_ex("ST05_clear", NULL, test_st05_clear, td);
   TestBit.run_ex("ST06_mark_restore", NULL, test_st06_mark_restore, td);
   TestBit.run_ex("ST07_new_with_allocator_null_is_default_path", NULL,
                  test_st07_new_with_allocator_null_is_default_path, td);
   TestBit.run_ex("ST08_new_with_allocator_grow_orphans", NULL,
                  test_st08_new_with_allocator_grow_orphans, td_arena);
   TestBit.run_ex("ST09_dispose_skips_release_when_overridden", NULL,
                  test_st09_dispose_skips_release_when_overridden, td_arena);

   return TestBit.report();
}
