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
 * File: stack.h
 * Description: Header file for Sigma Collections stack definitions and interfaces
 *
 * Stack:   A LIFO collection composed over Collection (FR-2603-sigma-collections-011)
 *          — the same relationship List already has. Collection's default
 *          construction mode already stores elements as pointers (reference
 *          semantics), so Stack gets that, plus arena-aware growth via
 *          FR-2603-sigma-collections-007's allocator override, at zero new
 *          growth logic — push/pop/peek/clear are thin wrappers over
 *          collection_add/collection_get_length/collection_set_length.
 */
#pragma once

#include <sigma/allocator.h>
#include <sigma/collections.h>

// forward declaration of the stack structure
struct sc_stack;
typedef struct sc_stack *stack;

/* Public interface for stack operations                        */
/* ============================================================ */
typedef struct sc_stack_i {
   /**
    * @brief Create a new stack with the specified initial capacity.
    * @param capacity Initial stack capacity
    * @return New stack instance, or NULL on failure
    */
   stack (*new)(usize capacity);
   /**
    * @brief Create a new stack bound to a caller-supplied allocator-use
    *        (FR-2603-sigma-collections-007/011). The instance grows/orphans
    *        through `use` instead of the global Allocator/Application
    *        facade; NULL behaves exactly like `Stack.new`.
    * @param capacity Initial stack capacity
    * @param use Allocator-use to bind this instance to, or NULL
    * @return New stack instance, or NULL on failure
    */
   stack (*new_with_allocator)(usize capacity, sc_alloc_use_t *use);
   /**
    * @brief Dispose of the stack and free associated resources.
    * @param s The stack to dispose of
    */
   void (*dispose)(stack s);
   /**
    * @brief Push a value onto the top of the stack.
    * @param s The stack to push onto
    * @param value The value to push
    * @return 0 on OK; otherwise non-zero
    */
   int (*push)(stack s, object value);
   /**
    * @brief Remove and return the value at the top of the stack.
    * @param s The stack to pop from
    * @return The top value, or NULL if the stack is empty
    */
   object (*pop)(stack s);
   /**
    * @brief Return the value at the top of the stack without removing it.
    * @param s The stack to peek at
    * @return The top value, or NULL if the stack is empty
    */
   object (*peek)(stack s);
   /**
    * @brief Check whether the stack has no elements.
    * @param s The stack to query
    * @return true if empty; otherwise false
    */
   bool (*is_empty)(stack s);
   /**
    * @brief Get the current number of elements on the stack.
    * @param s The stack to query
    * @return Current element count
    */
   usize (*count)(stack s);
   /**
    * @brief Clear the contents of the stack.
    * @param s The stack to clear
    */
   void (*clear)(stack s);
   /**
    * @brief Save the stack's current depth as a bookmark.
    * @param s The stack to mark
    * @return The current depth (== count(s))
    */
   usize (*mark)(stack s);
   /**
    * @brief Truncate the stack back to a previously saved depth, discarding
    *        everything pushed after it. O(1) — no allocation, no zeroing of
    *        the discarded region.
    * @param s The stack to restore
    * @param mark A depth previously returned by `mark`; a value greater than
    *        the current depth is a no-op.
    */
   void (*restore)(stack s, usize mark);
} sc_stack_i;
extern const sc_stack_i Stack;
