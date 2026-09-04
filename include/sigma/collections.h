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
 * File: collections.h
 * Description: Header file for Sigma Collections collections definitions and interfaces
 *
 * Collections: The core generic collection structures used within Sigma Collections.
 *              This includes Iterator and query mechanisms that operate across
 *              all collection types.
 */
#pragma once

#include <sigma/allocator.h>
#include <sigma/collection.h>
#include <sigma/farray.h>
#include <sigma/parray.h>
#include <sigma/query.h>

/* Forward declarations */
typedef struct iterator_s *iterator;
typedef struct sparse_iterator_s *sparse_iterator;

/* Iterator struct */
struct iterator_s {
   struct sc_collection *coll; /* The collection to iterate over */
   size_t current;             /* Current index */
};

/* Public interface for collections operations                */
/* ============================================================ */
typedef struct sc_collections_i {
   /**
    * @brief Add an element to the collection.
    * @param coll The collection to add to
    * @param ptr Pointer to the element to add
    * @return 0 on OK; otherwise non-zero
    */
   int (*add)(collection, object);
   /**
    * @brief Remove an element from the collection.
    * @param coll The collection to remove from
    * @param ptr Pointer to the element to remove
    * @return 0 on OK; otherwise non-zero
    */
   int (*remove)(collection, object);
   /**
    * @brief Clear all elements from the collection.
    * @param coll The collection to clear
    */
   void (*clear)(collection);
   /**
    * @brief Get the number of elements in the collection.
    * @param coll The collection to query
    * @return Number of elements
    */
   usize (*count)(collection);
   /**
    * @brief Create an iterator for the collection.
    * @param coll The collection to iterate over
    * @return New iterator instance, or NULL on failure
    */
   iterator (*create_iterator)(collection);
   /**
    * @brief Create a collection view of array data.
    * @param array The array (farray or parray) to create view of
    * @param stride Size of each element
    * @param length Number of elements
    * @param owns_buffer Whether the collection owns the buffer
    * @return New collection instance, or NULL on failure
    */
   collection (*create_view)(void *array, usize stride, usize length, bool owns_buffer);
   /**
    * @brief Dispose of the collection and free its resources.
    * @param coll The collection to dispose
    */
   void (*dispose)(collection);
   /**
    * @brief Get the Collections library version string.
    * @return Version string
    */
   const char *(*version)(void);
   /**
    * @brief Configure the allocator used by Collections operations.
    * @param use Pointer to sc_alloc_use_t or NULL to restore malloc/free fallback
    */
   void (*alloc_use)(sc_alloc_use_t *use);
   /**
    * @brief Produce a heapless queryable over an existing collection's
    *        elements, for Query.next/Query.first.
    * @param coll The collection to query
    * @return An sc_queryable ready to scan
    */
   sc_queryable (*as_queryable)(collection coll);
} sc_collections_i;
extern const sc_collections_i Collections;

/* New Iterator interface - simplified */

typedef struct sc_iterator_i {
   bool (*next)(iterator);      /**< Advances to next item and returns true if there is one */
   object (*current)(iterator); /**< Returns the current item */
   void (*reset)(iterator);     /**< Resets the iterator to the start */
   void (*dispose)(iterator);   /**< Disposes the iterator */
} sc_iterator_i;

extern const sc_iterator_i Iterator;

/* Sparse Iterator interface - for sparse collections */

typedef struct sc_sparse_iterator_i {
   /**
    * @brief Advance to next occupied slot
    * @param it The sparse iterator
    * @return true if found occupied slot, false if no more
    */
   bool (*next)(sparse_iterator it);
   /**
    * @brief Get current slot index
    * @param it The sparse iterator
    * @return Current slot index
    */
   usize (*current_index)(sparse_iterator it);
   /**
    * @brief Get current value at current slot
    * @param it The sparse iterator
    * @param out_value Pointer to store the retrieved value
    * @return 0 on OK; otherwise non-zero
    */
   int (*current_value)(sparse_iterator it, object *out_value);
   /**
    * @brief Reset iterator to beginning
    * @param it The sparse iterator
    */
   void (*reset)(sparse_iterator it);
   /**
    * @brief Dispose iterator and free resources
    * @param it The sparse iterator
    */
   void (*dispose)(sparse_iterator it);
} sc_sparse_iterator_i;

extern const sc_sparse_iterator_i SparseIterator;