/*
 * Sigma Collections
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------------------------------- *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------------------------------- *
 * File: stack.c
 * Description: Source file for Sigma Collections stack definitions and interfaces
 */

#include <sigma/stack.h>
#include <sigma/internal/collections.h>
// -----------------------------------------------------------------
#include <string.h>

//  declare the Stack struct: composed over Collection
struct sc_stack {
  collection coll; // underlying collection
};

static stack stack_new(usize capacity) {
  struct sc_stack *s = Allocator.alloc(sizeof(struct sc_stack));
  if (!s) {
    return NULL;
  }

  s->coll = collection_new(capacity, sizeof(object));
  if (!s->coll) {
    Allocator.dispose(s);
    return NULL;
  }

  return s;
}

// create a new stack bound to a caller-supplied allocator-use
// (FR-2603-sigma-collections-007/011). use == NULL is identical to
// stack_new; use != NULL threads the override through to the underlying
// collection, exactly like List.new_with_allocator.
static stack stack_new_with_allocator(usize capacity, sc_alloc_use_t *use) {
  if (!use) {
    return stack_new(capacity);
  }

  struct sc_stack *s = Allocator.alloc(sizeof(struct sc_stack));
  if (!s) {
    return NULL;
  }

  s->coll = collection_new_with_allocator(capacity, sizeof(object), use);
  if (!s->coll) {
    Allocator.dispose(s);
    return NULL;
  }

  return s;
}

static void stack_dispose(stack s) {
  if (!s) {
    return;
  }

  collection_dispose(s->coll);
  Allocator.dispose(s);
}

static int stack_push(stack s, object value) {
  if (!s) {
    return ERR;
  }
  return collection_add(s->coll, value);
}

static object stack_pop(stack s) {
  if (!s) {
    return NULL;
  }
  usize length = collection_get_length(s->coll);
  if (length == 0) {
    return NULL;
  }

  usize stride = collection_get_stride(s->coll);
  void *src = (char *)collection_get_buffer(s->coll) + (length - 1) * stride;
  object value;
  memcpy(&value, src, stride);

  collection_set_length(s->coll, length - 1);
  return value;
}

static object stack_peek(stack s) {
  if (!s) {
    return NULL;
  }
  usize length = collection_get_length(s->coll);
  if (length == 0) {
    return NULL;
  }

  usize stride = collection_get_stride(s->coll);
  void *src = (char *)collection_get_buffer(s->coll) + (length - 1) * stride;
  object value;
  memcpy(&value, src, stride);
  return value;
}

static bool stack_is_empty(stack s) {
  return !s || collection_get_length(s->coll) == 0;
}

static usize stack_count(stack s) {
  if (!s) {
    return 0;
  }
  return collection_get_length(s->coll);
}

static void stack_clear(stack s) {
  if (!s) {
    return;
  }
  collection_clear(s->coll);
}

static usize stack_mark(stack s) {
  if (!s) {
    return 0;
  }
  return collection_get_length(s->coll);
}

static void stack_restore(stack s, usize mark) {
  if (!s) {
    return;
  }
  if (mark > collection_get_length(s->coll)) {
    return; // a mark greater than the current depth is a no-op
  }
  collection_set_length(s->coll, mark);
}

//  public interface implementation
const sc_stack_i Stack = {
    .new = stack_new,
    .new_with_allocator = stack_new_with_allocator,
    .dispose = stack_dispose,
    .push = stack_push,
    .pop = stack_pop,
    .peek = stack_peek,
    .is_empty = stack_is_empty,
    .count = stack_count,
    .clear = stack_clear,
    .mark = stack_mark,
    .restore = stack_restore,
};
