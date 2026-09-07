/*
 * SigmaCore
 * Copyright (c) 2025 David Boarman (BadKraft) and contributors
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
 * File:  internal/collections.h
 * Description: Header file for SigmaCore internal collection functions
 */
#pragma once

#include <sigma/allocator.h>
#include <sigma/internal/array_base.h>

// forward declarations
struct sc_pointer_array;
typedef struct sc_pointer_array *parray;
struct sc_slotarray;
typedef struct sc_slotarray *slotarray;
struct sparse_iterator_s;
typedef struct sparse_iterator_s *sparse_iterator;

// collection structure (internal)
struct sc_collection {
    sc_array_base array;
    usize stride;
    usize length;
    bool owns_buffer;
    // FR-2603-sigma-collections-007: NULL = global Allocator/Application
    // facade (every existing caller, unchanged behavior); non-NULL = this
    // instance's buffer is allocated/grown/orphaned through `alloc_use`
    // instead, and collection_dispose skips freeing the buffer.
    sc_alloc_use_t *alloc_use;
};

// array internal functions
addr array_get_bucket_start(parray arr);
addr array_get_bucket_end(parray arr);
addr *array_get_bucket(parray arr);

// collection internal functions
collection collection_new(usize capacity, usize stride);
collection collection_new_with_allocator(usize capacity, usize stride, sc_alloc_use_t *use);
void collection_dispose(collection coll);
int collection_add(collection coll, object ptr);
int collection_grow(collection coll);
void collection_clear(collection coll);
void collection_set_data(collection coll, void *data, usize count);

// collection accessor functions
void *collection_get_buffer(collection coll);
void *collection_get_end(collection coll);
usize collection_get_stride(collection coll);
usize collection_get_length(collection coll);
void collection_set_length(collection coll, usize length);

// array collection helpers
collection array_create_collection_view(void *buffer, void *end, usize stride, usize length,
                                        bool owns_buffer);

// slotarray internal functions
slotarray slotarray_create_view(parray arr);

// sparse collection interface (internal)
typedef struct sc_sparse_i {
    bool (*is_empty_slot)(object, usize);
    usize (*capacity)(object);
    int (*get_at)(object, usize, object *);
} sc_sparse_i;

// sparse iterator internal functions
sparse_iterator sparse_iterator_new(object sparse_coll, const sc_sparse_i *ops);