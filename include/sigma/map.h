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
 * File: map.h
 * Description: String-keyed hash map with FNV-1a hashing
 */
#pragma once

#include <sigma/allocator.h>
#include <sigma/types.h>

/**
 * @brief Opaque map handle
 */
struct sc_map_s;
typedef struct sc_map_s *map;

/**
 * @brief Forward declaration for sparse iterator
 */
struct sparse_iterator_s;
typedef struct sparse_iterator_s *sparse_iterator;

/**
 * @brief Map entry structure for iteration
 * Returned by iterator to access key-value pairs
 */
typedef struct {
   const char *key; /**< Key pointer */
   usize key_len;   /**< Key length in bytes */
   addr value;      /**< Stored value (address-sized) */
} map_entry;

/**
 * @brief Map interface for string-keyed hash map operations
 *
 * Implementation details:
 * - FNV-1a 64-bit hashing
 * - Open addressing with linear probing
 * - Load factor: 0.5 (resizes at 50% capacity)
 * - Capacity always power of 2
 * - Keys: caller-owned pointers (arena or malloc'd)
 * - Values: pointer-sized (addr / uintptr_t)
 * - Thread model: thread-compatible, not internally synchronized
 * - Concurrent reads/writes require caller-provided synchronization
 */
typedef struct sc_map_i {
   /**
    * @brief Create a new map with the given initial capacity
    * @param capacity Initial bucket count hint (rounded up to power of 2)
    * @return New map or NULL on allocation failure
    *
    * Example:
    * @code
    * map m = Map.new(32);  // Creates map with 32 buckets
    * @endcode
    */
   map (*new)(usize capacity);

   /**
    * @brief Initialize a map handle
    * @param m Pointer to map handle to initialize
    * @param capacity Initial bucket count hint
    *
    * Example:
    * @code
    * map m = NULL;
    * Map.init(&m, 16);
    * @endcode
    */
   void (*init)(map *m, usize capacity);

   /**
    * @brief Dispose of map and free resources
    * @param m The map to dispose
    *
    * Note: Does NOT free keys or values - caller manages those lifetimes
    */
   void (*dispose)(map m);

   /**
    * @brief Insert or update a string-keyed entry
    * @param m The map
    * @param key Key bytes (not required to be NUL-terminated)
    * @param len Key length in bytes
    * @param val Value to store (pointer-sized, caller interprets)
    * @return 0 on success; -1 on allocation failure
    *
    * Behavior:
    * - If key exists: updates value
    * - If key doesn't exist: inserts new entry
    * - Automatically resizes at 50% load factor
    * - Key pointer must remain valid while entry exists
    *
    * Example:
    * @code
    * const char *key = "field_name";
    * Map.set(m, key, strlen(key), (addr)field_index);
    * @endcode
    */
   int (*set)(map m, const char *key, usize len, addr val);

   /**
    * @brief Look up a string-keyed entry
    * @param m The map
    * @param key Key bytes
    * @param len Key length in bytes
    * @param out_val Receives the stored value on success
    * @return 1 if found (out_val written); 0 if not found
    *
    * Example:
    * @code
    * addr value;
    * if (Map.get(m, "key", 3, &value)) {
    *     printf("Found\n");
    * }
    * @endcode
    */
   int (*get)(map m, const char *key, usize len, addr *out_val);

   /**
    * @brief Test whether a key is present
    * @param m The map
    * @param key Key bytes
    * @param len Key length in bytes
    * @return 1 if present; 0 if absent
    *
    * Example:
    * @code
    * if (Map.has(m, "key", 3)) {
    *     // Key exists
    * }
    * @endcode
    */
   int (*has)(map m, const char *key, usize len);

   /**
    * @brief Remove a key from the map
    * @param m The map
    * @param key Key bytes
    * @param len Key length in bytes
    * @return 1 if key was present and removed; 0 if key was absent
    *
    * Note: Does NOT free the key pointer - caller manages key lifetime
    *
    * Example:
    * @code
    * if (Map.remove(m, "old_key", 7)) {
    *     printf("Removed\n");
    * }
    * @endcode
    */
   int (*remove)(map m, const char *key, usize len);

   /**
    * @brief Return the number of entries currently stored
    * @param m The map
    * @return Entry count
    */
   usize (*count)(map m);

   /**
    * @brief Return the current bucket capacity
    * @param m The map
    * @return Bucket capacity (always power of 2)
    */
   usize (*capacity)(map m);

   /**
    * @brief Create iterator for map entries
    * @param m The map to iterate over
    * @return Sparse iterator or NULL on failure
    *
    * Iterates only over occupied slots (skips empty and tombstones).
    * Use SparseIterator interface to access entries.
    *
    * Iterator lifetime and mutation rules:
    * - Any set/remove operation can invalidate iterator traversal state.
    * - Recreate iterators after mutating map contents.
    *
    * Example:
    * @code
    * sparse_iterator it = Map.create_iterator(m);
    * while (SparseIterator.next(it)) {
    *     map_entry *entry;
    *     SparseIterator.current_value(it, (object *)&entry);
    *     printf("%.*s => %zu\n", (int)entry->key_len, entry->key, entry->value);
    * }
    * SparseIterator.dispose(it);
    * @endcode
    */
   sparse_iterator (*create_iterator)(map m);

} sc_map_i;

/**
 * @brief Global Map interface instance
 */
extern const sc_map_i Map;
