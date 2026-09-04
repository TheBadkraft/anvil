/*
 * SigmaCore
 * Copyright (c) 2025 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * ----------------------------------------------
 * File: map.c
 * Description: String-keyed hash map implementation with FNV-1a hashing
 */

#include <sigma/map.h>
#include <sigma/allocator.h>
#include <sigma/collections.h>
#include <sigma/math.h>
#include <string.h>
#include <sigma/internal/collections.h>

// FNV-1a 64-bit hash constants
#define FNV1A_OFFSET UINT64_C(14695981039346656037)
#define FNV1A_PRIME UINT64_C(1099511628211)

// Load factor threshold: resize when count / capacity > 0.5
#define LOAD_FACTOR_THRESHOLD 0.5

// Forward declarations - API functions
static map map_new(usize capacity);
static void map_init(map *m, usize capacity);
static void map_dispose(map m);
static int map_set(map m, const char *key, usize len, addr val);
static int map_get(map m, const char *key, usize len, addr *out_val);
static int map_has(map m, const char *key, usize len);
static int map_remove(map m, const char *key, usize len);
static usize map_count(map m);
static usize map_capacity(map m);
static sparse_iterator map_create_iterator(map m);

// Forward declarations - helper functions
static uint64_t fnv1a_hash(const char *data, usize len);
static int map_resize(map m, usize new_capacity);
static int map_find_slot(map m, const char *key, usize len, uint64_t hash, usize *out_idx);

// Forward declarations - sparse iterator helpers
static bool map_is_empty_slot(map m, usize index);
static int map_get_at(map m, usize index, object *out_entry);

// Forward declarations - Query helpers
static sc_queryable map_as_queryable(map m);
static sc_queryable map_keys(map m);
static sc_queryable map_values(map m);

/**
 * @brief Map bucket entry
 */
typedef struct {
   uint64_t hash;   // FNV-1a hash; 0 = empty slot, 1 = tombstone
   const char *key; // Key pointer (caller-owned)
   usize key_len;   // Key length in bytes
   addr value;      // Stored value
} map_slot;

/**
 * @brief Map structure
 */
struct sc_map_s {
   map_slot *buckets;
   usize count; // Number of occupied slots (excludes tombstones)
   usize capacity;
};

// Sparse iterator operations for Map
static const sc_sparse_i map_sparse_ops = {
   .is_empty_slot = (bool (*)(object, usize))map_is_empty_slot,
   .capacity = (usize (*)(object))map_capacity,
   .get_at = (int (*)(object, usize, object *))map_get_at,
};

// API interface definition
const sc_map_i Map = {
   .new = map_new,
   .init = map_init,
   .dispose = map_dispose,
   .set = map_set,
   .get = map_get,
   .has = map_has,
   .remove = map_remove,
   .count = map_count,
   .capacity = map_capacity,
   .create_iterator = map_create_iterator,
   .as_queryable = map_as_queryable,
   .keys = map_keys,
   .values = map_values,
};

// Helper/utility function definitions

/**
 * @brief FNV-1a 64-bit hash function
 */
static uint64_t fnv1a_hash(const char *data, usize len) {
   uint64_t hash = FNV1A_OFFSET;
   for (usize i = 0; i < len; i++) {
      hash ^= (uint8_t)data[i];
      hash *= FNV1A_PRIME;
   }
   // Ensure hash is never 0 or 1 (reserved for empty/tombstone)
   if (hash == 0 || hash == 1) {
      hash = 2;
   }
   return hash;
}

/**
 * @brief Find slot for key (for get/set/remove operations)
 * @param m The map
 * @param key Key bytes
 * @param len Key length
 * @param hash Pre-computed FNV-1a hash
 * @param out_idx Receives the slot index
 * @return 1 if key found; 0 if not found (out_idx = first empty/tombstone)
 */
static int map_find_slot(map m, const char *key, usize len, uint64_t hash, usize *out_idx) {
   usize mask = m->capacity - 1;
   usize idx = hash & mask;
   usize first_tombstone = m->capacity; // Invalid index

   for (usize probe = 0; probe < m->capacity; probe++) {
      map_slot *bucket = &m->buckets[idx];

      // Empty slot - key not found
      if (bucket->hash == 0) {
         // Return tombstone if we saw one, else this empty slot
         *out_idx = (first_tombstone < m->capacity) ? first_tombstone : idx;
         return 0;
      }

      // Tombstone - remember first one for insertion
      if (bucket->hash == 1) {
         if (first_tombstone >= m->capacity) {
            first_tombstone = idx;
         }
         idx = (idx + 1) & mask;
         continue;
      }

      // Occupied slot - check if it matches our key
      if (bucket->hash == hash && bucket->key_len == len && memcmp(bucket->key, key, len) == 0) {
         *out_idx = idx;
         return 1; // Found
      }

      idx = (idx + 1) & mask;
   }

   // Table full - return tombstone if we saw one
   *out_idx = (first_tombstone < m->capacity) ? first_tombstone : 0;
   return 0;
}

/**
 * @brief Resize map to new capacity
 * @param m The map
 * @param new_capacity New bucket count (must be power of 2)
 * @return 0 on success; -1 on allocation failure
 */
static int map_resize(map m, usize new_capacity) {
   map_slot *old_buckets = m->buckets;
   usize old_capacity = m->capacity;

   usize normalized_capacity = 0;
   if (Math.normalize_pow_2_min_checked(new_capacity, 8, &normalized_capacity) != SC_MATH_OK) {
      return ERR;
   }
   new_capacity = normalized_capacity;

   map_slot *new_buckets = Allocator.alloc(sizeof(map_slot) * new_capacity);
   if (!new_buckets) {
      return ERR;
   }

   m->buckets = new_buckets;
   m->capacity = new_capacity;
   m->count = 0;

   // Rehash all entries from old table
   for (usize i = 0; i < old_capacity; i++) {
      map_slot bucket = old_buckets[i];

      // Skip empty and tombstone slots
      if (bucket.hash > 1) {
         // Re-insert into new table
         usize idx;
         map_find_slot(m, bucket.key, bucket.key_len, bucket.hash, &idx);
         m->buckets[idx] = bucket;
         m->count++;
      }
   }

   // Dispose old bucket array
   if (old_buckets) {
      Allocator.dispose(old_buckets);
   }

   return OK;
}

// API function definitions

/**
 * @brief Create a new map
 */
static map map_new(usize capacity) {
   map m = Allocator.alloc(sizeof(struct sc_map_s));
   if (!m) {
      return NULL;
   }

   m->buckets = NULL;
   m->count = 0;
   m->capacity = 0;

   map_init(&m, capacity);
   if (!m || !m->buckets) {
      if (m) {
         Allocator.dispose(m);
      }
      return NULL;
   }

   return m;
}

/**
 * @brief Initialize map in-place
 */
static void map_init(map *m_ptr, usize capacity) {
   if (!m_ptr) {
      return;
   }

   map m = *m_ptr;
   if (!m) {
      m = Allocator.alloc(sizeof(struct sc_map_s));
      if (!m) {
         return;
      }
      *m_ptr = m;
      m->buckets = NULL;
      m->count = 0;
      m->capacity = 0;
   }

   if (m->buckets) {
      Allocator.dispose(m->buckets);
      m->buckets = NULL;
   }

   usize normalized_capacity = 0;
   if (Math.normalize_pow_2_min_checked(capacity, 8, &normalized_capacity) != SC_MATH_OK) {
      m->capacity = 0;
      m->count = 0;
      return;
   }
   capacity = normalized_capacity;

   m->buckets = Allocator.alloc(sizeof(map_slot) * capacity);
   if (!m->buckets) {
      m->capacity = 0;
      m->count = 0;
      return;
   }

   m->count = 0;
   m->capacity = capacity;
}

/**
 * @brief Dispose of map
 */
static void map_dispose(map m) {
   if (!m) {
      return;
   }

   if (m->buckets) {
      Allocator.dispose(m->buckets);
   }

   Allocator.dispose(m);
}

/**
 * @brief Insert or update entry
 */
static int map_set(map m, const char *key, usize len, addr val) {
   if (!m || !key || !m->buckets || m->capacity == 0) {
      return ERR;
   }

   uint64_t hash = fnv1a_hash(key, len);

   // Check if we need to resize
   double load = (double)(m->count + 1) / m->capacity;
   if (load > LOAD_FACTOR_THRESHOLD) {
      if (map_resize(m, m->capacity * 2) != OK) {
         return ERR;
      }
   }

   // Find insertion slot
   usize idx;
   int found = map_find_slot(m, key, len, hash, &idx);

   map_slot bucket = {.hash = hash, .key = key, .key_len = len, .value = val};
   m->buckets[idx] = bucket;

   // Only increment count if this is a new entry
   if (!found) {
      m->count++;
   }

   return OK;
}

/**
 * @brief Look up entry
 */
static int map_get(map m, const char *key, usize len, addr *out_val) {
   if (!m || !key || !out_val || !m->buckets || m->capacity == 0) {
      return 0;
   }

   uint64_t hash = fnv1a_hash(key, len);

   usize idx;
   if (map_find_slot(m, key, len, hash, &idx)) {
      *out_val = m->buckets[idx].value;
      return 1;
   }

   return 0;
}

/**
 * @brief Check if key exists
 */
static int map_has(map m, const char *key, usize len) {
   if (!m || !key || !m->buckets || m->capacity == 0) {
      return 0;
   }

   uint64_t hash = fnv1a_hash(key, len);
   usize idx;
   return map_find_slot(m, key, len, hash, &idx);
}

/**
 * @brief Remove entry
 */
static int map_remove(map m, const char *key, usize len) {
   if (!m || !key || !m->buckets || m->capacity == 0) {
      return 0;
   }

   uint64_t hash = fnv1a_hash(key, len);

   usize idx;
   if (map_find_slot(m, key, len, hash, &idx)) {
      // Mark as tombstone (hash = 1)
      map_slot tombstone = {.hash = 1};
      m->buckets[idx] = tombstone;
      m->count--;
      return 1;
   }

   return 0;
}

/**
 * @brief Get entry count
 */
static usize map_count(map m) { return m ? m->count : 0; }

/**
 * @brief Get bucket capacity
 */
static usize map_capacity(map m) { return m ? m->capacity : 0; }

/**
 * @brief Check if slot is empty (for sparse iterator)
 */
static bool map_is_empty_slot(map m, usize index) {
   if (!m || !m->buckets || index >= m->capacity) {
      return true;
   }

   map_slot bucket = m->buckets[index];

   // Empty (hash=0) or tombstone (hash=1) are considered empty
   return bucket.hash <= 1;
}

/**
 * @brief Get entry at index (for sparse iterator)
 * Returns pointer to map_entry in out_entry
 */
static int map_get_at(map m, usize index, object *out_entry) {
   if (!m || !out_entry || !m->buckets || index >= m->capacity) {
      return ERR;
   }

   map_slot bucket = m->buckets[index];

   if (bucket.hash <= 1) {
      return ERR; // Empty or tombstone
   }

   // Thread-local projection avoids cross-thread races while preserving API.
   static _Thread_local map_entry entry;
   entry.key = bucket.key;
   entry.key_len = bucket.key_len;
   entry.value = bucket.value;

   *(map_entry **)out_entry = &entry;
   return OK;
}

/**
 * @brief Create sparse iterator for map
 */
static sparse_iterator map_create_iterator(map m) {
   if (!m) {
      return NULL;
   }
   return sparse_iterator_new(m, &map_sparse_ops);
}

/**
 * @brief sc_query_advance_fn for Map.as_queryable — yields const map_entry *,
 *        skipping empty and tombstone slots.
 */
static bool map_entry_query_advance(sc_queryable *self, const void **out_element, usize *out_index) {
   map m = (map)self->source;
   while (self->index < self->bound) {
      usize i = self->index++;
      if (!map_is_empty_slot(m, i)) {
         object entry;
         if (map_get_at(m, i, &entry) == OK) {
            *out_element = entry;
            if (out_index) {
               *out_index = i;
            }
            return true;
         }
      }
   }
   return false;
}

/**
 * @brief sc_query_advance_fn for Map.keys — yields const sc_key_view *,
 *        skipping empty and tombstone slots.
 */
static bool map_keys_query_advance(sc_queryable *self, const void **out_element, usize *out_index) {
   map m = (map)self->source;
   while (self->index < self->bound) {
      usize i = self->index++;
      if (!map_is_empty_slot(m, i)) {
         object entry;
         if (map_get_at(m, i, &entry) == OK) {
            static _Thread_local sc_key_view view;
            const map_entry *e = entry;
            view.ptr = e->key;
            view.len = e->key_len;
            *out_element = &view;
            if (out_index) {
               *out_index = i;
            }
            return true;
         }
      }
   }
   return false;
}

/**
 * @brief sc_query_advance_fn for Map.values — yields const addr *,
 *        skipping empty and tombstone slots.
 */
static bool map_values_query_advance(sc_queryable *self, const void **out_element, usize *out_index) {
   map m = (map)self->source;
   while (self->index < self->bound) {
      usize i = self->index++;
      if (!map_is_empty_slot(m, i)) {
         object entry;
         if (map_get_at(m, i, &entry) == OK) {
            static _Thread_local addr value;
            value = ((const map_entry *)entry)->value;
            *out_element = &value;
            if (out_index) {
               *out_index = i;
            }
            return true;
         }
      }
   }
   return false;
}

/**
 * @brief Produce a heapless queryable over the map's entries
 */
static sc_queryable map_as_queryable(map m) {
   usize cap = m ? m->capacity : 0;
   return (sc_queryable){
       .source = m, .element_size = 0, .bound = cap, .index = 0,
       .advance = map_entry_query_advance};
}

/**
 * @brief Produce a heapless queryable over the map's keys
 */
static sc_queryable map_keys(map m) {
   usize cap = m ? m->capacity : 0;
   return (sc_queryable){
       .source = m, .element_size = 0, .bound = cap, .index = 0,
       .advance = map_keys_query_advance};
}

/**
 * @brief Produce a heapless queryable over the map's values
 */
static sc_queryable map_values(map m) {
   usize cap = m ? m->capacity : 0;
   return (sc_queryable){
       .source = m, .element_size = 0, .bound = cap, .index = 0,
       .advance = map_values_query_advance};
}
