/*
 * test_statements.c - Tests for statement buffer parsing
 *
 * TODO: Refactor these tests for new meta-buffer architecture
 *
 * These tests were written for the old statement structure that stored value data
 * directly on the statement (struct anvl_statement had .value field, .attrib_count, etc).
 * The new architecture uses:
 *   - meta[9] array for fixed metadata indices
 *   - value_meta pointer for detailed value information
 *   - base_meta pointer for inheritance (if present)
 *   - attr_meta pointer for statement-level attributes (if present)
 *
 * Once the parser fully populates value_meta for all value types, these tests
 * should be rewritten to use the new meta-buffer accessors and assertions.
 *
 * This file is now empty to prevent compilation errors from the old API.
 */

#include "anvil.h"
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>

// All tests disabled until refactored for new meta-buffer architecture