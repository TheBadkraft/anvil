/*
 * Sigma.Collections
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
 * File: collections.c
 * Description: Source file for Sigma.Collections collection definitions and
 * interfaces
 *
 * Collection: A generic collection wrapper that provides a unified interface
 *             for arrays (parray/farray), with stride-aware operations.
 */

#include <sigma/allocator.h>
#include <sigma/collections.h>
#include <sigma/internal/array_base.h>
#include <sigma/internal/arrays.h>
#include <sigma/internal/collections.h>
#include <string.h>

#define COLLECTIONS_VERSION "1.2.0"

// Forward declarations of array structs for internal use
// Note: Now using unified sc_array_base from array_base.h

/* Iterator functions */
bool iter_next(iterator it);
object iter_current(iterator it);
void iter_reset(iterator it);
void iter_dispose(iterator it);

/* Sparse Iterator functions */
bool sparse_iter_next(sparse_iterator it);
usize sparse_iter_current_index(sparse_iterator it);
int sparse_iter_current_value(sparse_iterator it, object *out_value);
void sparse_iter_reset(sparse_iterator it);
void sparse_iter_dispose(sparse_iterator it);

/* Sparse iterator struct */
struct sparse_iterator_s {
  object sparse_coll;     // The sparse collection (slotarray or indexarray)
  const sc_sparse_i *ops; // Operations interface
  usize current;   // Current index (-1 means before start, need to find first)
  usize capacity;  // Cached capacity
  bool positioned; // Whether iterator is positioned at a valid slot
};

// create a collection view of array data
collection collection_create_view(void *array, usize stride, usize length,
                                  bool owns_buffer) {
  struct sc_collection *coll = Allocator.alloc(sizeof(struct sc_collection));
  if (!coll) {
    return NULL;
  }

  if (array) {
    // Copy handle from the array to determine storage type
    char array_handle = ((char *)array)[0];
    coll->array.handle[0] = array_handle;
    coll->array.handle[1] = ((char *)array)[1];

    if (array_handle == 'F') {
      // farray - store values like farray
      sc_array_base *farr = (sc_array_base *)array;
      coll->array.bucket = farr->bucket;
      coll->array.end = farr->end;
      coll->stride = stride; // Use provided stride for values
      coll->owns_buffer = owns_buffer;
    } else if (array_handle == 'P') {
      // parray - store pointers like parray
      sc_array_base *parr = (sc_array_base *)array;
      coll->array.bucket = parr->bucket;
      coll->array.end = parr->end;
      coll->stride = sizeof(void *); // Pointers
      coll->owns_buffer = owns_buffer;
    } else {
      // Unknown array - treat as farray (store values)
      coll->array.handle[0] = 'F';
      coll->array.handle[1] = '\0';
      coll->array.bucket = array;
      coll->array.end = NULL; // unknown
      coll->stride = stride;  // Use provided stride for values
      coll->owns_buffer = owns_buffer;
    }
  } else {
    // NULL array - treat as farray, store values
    coll->array.handle[0] = 'F';
    coll->array.handle[1] = '\0';
    coll->array.bucket = NULL;
    coll->array.end = NULL;
    coll->stride = stride;
    coll->owns_buffer = owns_buffer;
  }

  coll->length = length;
  return coll;
}

// set collection data from a buffer
void collection_set_data(collection coll, void *data, usize count) {
  if (!coll || !data) {
    return;
  }

  memcpy(coll->array.bucket, data, count * coll->stride);
  coll->length = count;
}

// collection accessor functions
void *collection_get_buffer(collection coll) {
  return coll ? coll->array.bucket : NULL;
}

inline void *collection_get_end(collection coll) {
  return coll ? coll->array.end : NULL;
}

inline usize collection_get_stride(collection coll) {
  return coll ? coll->stride : 0;
}

inline usize collection_get_length(collection coll) {
  return coll ? coll->length : 0;
}

inline void collection_set_length(collection coll, usize length) {
  if (coll) {
    coll->length = length;
  }
}

// create a new collection with the specified capacity and stride
collection collection_new(usize capacity, usize stride) {
  void *bucket;
  char *end;

  struct sc_collection *coll = array_alloc_struct_with_bucket(
      sizeof(struct sc_collection), 'P', stride, capacity, &bucket, &end);

  if (!coll) {
    return NULL;
  }

  coll->array.bucket = bucket;
  coll->array.end = end;
  coll->stride = stride;
  coll->length = 0;
  coll->owns_buffer = true;

  return coll;
}
// dispose of the collection
void collection_dispose(collection coll) {
  if (!coll) {
    return;
  }

  if (coll->owns_buffer && coll->array.bucket) {
    Allocator.dispose(coll->array.bucket);
  }
  Allocator.dispose(coll);
}
// get the Collections library version string
const char *collection_get_version(void) { return COLLECTIONS_VERSION; }
// get the count of elements in the collection
usize collection_count(collection coll) {
  if (!coll) {
    return 0;
  }
  return coll->length;
}
// grow the collection
int collection_grow(collection coll) {
  if (!coll) {
    return ERR;
  }
  usize current_capacity =
      ((char *)coll->array.end - (char *)coll->array.bucket) / coll->stride;
  usize new_capacity;
  if (current_capacity == 0) {
    new_capacity = 8; // Start with reasonable minimum capacity
  } else {
    new_capacity = current_capacity * 2;
  }
  void *new_buffer = Allocator.alloc(coll->stride * new_capacity);
  if (!new_buffer) {
    return ERR;
  }

  memcpy(new_buffer, coll->array.bucket, coll->stride * current_capacity);
  Allocator.dispose(coll->array.bucket);
  coll->array.bucket = new_buffer;
  coll->array.end = (char *)new_buffer + coll->stride * new_capacity;
  return OK;
}

// add an element to the collection
int collection_add(collection coll, object ptr) {
  if (!coll || !ptr) {
    return ERR;
  }

  usize capacity =
      ((char *)coll->array.end - (char *)coll->array.bucket) / coll->stride;
  if (coll->length >= capacity) {
    if (collection_grow(coll) != 0) {
      return ERR;
    }
  }

  void *dest = (char *)coll->array.bucket + coll->length * coll->stride;

  // Store according to the array primitive
  if (coll->array.handle[0] == 'F') {
    // farray: store values
    memcpy(dest, ptr, coll->stride);
  } else {
    // parray or raw: store pointers
    memcpy(dest, &ptr, coll->stride);
  }

  coll->length++;
  return OK;
}
// remove an element from the collection
int collection_remove(collection coll, object ptr) {
  if (!coll || !ptr) {
    return ERR;
  }

  for (usize i = 0; i < coll->length; ++i) {
    void *slot = (char *)coll->array.bucket + i * coll->stride;
    bool match;
    if (coll->array.handle[0] == 'P') {
      // parray: compare pointer values
      match = memcmp(slot, ptr, coll->stride) == 0;
    } else {
      // farray or raw: compare data values
      match = memcmp(slot, ptr, coll->stride) == 0;
    }
    if (match) {
      // Shift left
      for (usize j = i; j < coll->length - 1; ++j) {
        void *src = (char *)coll->array.bucket + (j + 1) * coll->stride;
        void *dest = (char *)coll->array.bucket + j * coll->stride;
        memcpy(dest, src, coll->stride);
      }
      // Zero the last
      memset((char *)coll->array.bucket + (coll->length - 1) * coll->stride, 0,
             coll->stride);
      coll->length--;
      return OK;
    }
  }
  return ERR; // not found
}
// clear the collection
void collection_clear(collection coll) {
  if (!coll || !coll->array.bucket) {
    return;
  }
  usize capacity =
      ((char *)coll->array.end - (char *)coll->array.bucket) / coll->stride;
  memset(coll->array.bucket, 0, capacity * coll->stride);
  coll->length = 0;
}
// get count
usize collection_get_count(collection coll) { return collection_count(coll); }

/* Create an iterator for a collection */
iterator collection_create_iterator(collection coll) {
  if (!coll)
    return NULL;
  iterator it = Allocator.alloc(sizeof(struct iterator_s));
  if (!it)
    return NULL;
  it->coll = coll;
  it->current = 0;
  return it;
}

/* Produce a heapless queryable over the collection's elements */
sc_queryable collection_as_queryable(collection coll) {
  usize length = coll ? coll->length : 0;
  usize stride = coll ? coll->stride : 0;
  return (sc_queryable){
      .source = coll, .element_size = stride, .bound = length, .index = 0,
      .advance = array_base_query_advance};
}

//  public interface implementation
const sc_collections_i Collections = {
    .add = collection_add,
    .remove = collection_remove,
    .clear = collection_clear,
    .count = collection_get_count,
    .create_iterator = collection_create_iterator,
    .create_view = collection_create_view,
    .dispose = collection_dispose,
    .version = collection_get_version,
    .as_queryable = collection_as_queryable,
};

/* Advances to next item and returns true if there is one */
bool iter_next(iterator it) {
  if (!it || !it->coll || it->current >= it->coll->length)
    return false;
  it->current++;
  return true;
}

/* Returns the current item, or NULL if none */
object iter_current(iterator it) {
  if (!it || !it->coll || it->current == 0 || it->current > it->coll->length)
    return NULL;
  return (char *)it->coll->array.bucket + (it->current - 1) * it->coll->stride;
}

/* Resets the iterator to the start */
void iter_reset(iterator it) {
  if (it)
    it->current = 0;
}

/* Disposes the iterator */
void iter_dispose(iterator it) {
  if (it)
    Allocator.dispose(it);
}

const sc_iterator_i Iterator = {
    .next = iter_next,
    .current = iter_current,
    .reset = iter_reset,
    .dispose = iter_dispose,
};

/* Sparse Iterator Implementation */

// Create a new sparse iterator
sparse_iterator sparse_iterator_new(object sparse_coll,
                                    const sc_sparse_i *ops) {
  if (!sparse_coll || !ops) {
    return NULL;
  }

  sparse_iterator it = Allocator.alloc(sizeof(struct sparse_iterator_s));
  if (!it) {
    return NULL;
  }

  it->sparse_coll = sparse_coll;
  it->ops = ops;
  it->current = 0;
  it->capacity = ops->capacity(sparse_coll);
  it->positioned = false; // Not yet positioned at first element

  return it;
}

// Advance to next occupied slot
bool sparse_iter_next(sparse_iterator it) {
  if (!it || !it->ops) {
    return false;
  }

  // If we were positioned at a slot, move past it
  if (it->positioned) {
    it->current++;
    it->positioned = false;
  }

  // Search for next non-empty slot
  while (it->current < it->capacity) {
    if (!it->ops->is_empty_slot(it->sparse_coll, it->current)) {
      it->positioned = true;
      return true; // Found occupied slot
    }
    it->current++;
  }

  return false; // No more occupied slots
}

// Get current slot index
usize sparse_iter_current_index(sparse_iterator it) {
  if (!it) {
    return 0;
  }
  return it->current;
}

// Get current value
int sparse_iter_current_value(sparse_iterator it, object *out_value) {
  if (!it || !out_value || !it->ops || !it->positioned) {
    return ERR;
  }

  return it->ops->get_at(it->sparse_coll, it->current, out_value);
}

// Reset iterator to beginning
void sparse_iter_reset(sparse_iterator it) {
  if (it) {
    it->current = 0;
    it->positioned = false;
  }
}

// Dispose iterator
void sparse_iter_dispose(sparse_iterator it) {
  if (it) {
    Allocator.dispose(it);
  }
}

// Sparse iterator interface
const sc_sparse_iterator_i SparseIterator = {
    .next = sparse_iter_next,
    .current_index = sparse_iter_current_index,
    .current_value = sparse_iter_current_value,
    .reset = sparse_iter_reset,
    .dispose = sparse_iter_dispose,
};