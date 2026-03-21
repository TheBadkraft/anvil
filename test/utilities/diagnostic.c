/*
 * diagnostic.c - Memory leak diagnostic for source_create
 */

#include "anvil.h"
#include <sigma.memory/memory.h>
#include <sigma.test/sigtest.h>
#include <stdio.h>
#include <string.h>

// Atomic function 1: Allocate source struct
static source alloc_source_struct(void) {
   source src = Allocator.alloc(sizeof(struct anvl_source_t));
   if (src) memset(src, 0, sizeof(struct anvl_source_t));
   return src; // Returns allocated struct or NULL on failure
}

// Atomic function 2: Allocate buffer
static char *alloc_buffer(const char *data, usize len) {
   if (!data || len == 0) {
      return NULL;
   }
   char *buffer = Allocator.alloc(len + 1); // +1 for null terminator
   return buffer;                  // Returns allocated buffer or NULL on failure
}

// Atomic function 3: Copy data to buffer
static bool copy_data_to_buffer(char *buffer, const char *data, usize len) {
   if (!buffer || !data) {
      return false;
   }
   memcpy(buffer, data, len);
   buffer[len] = '\0'; // null terminate
   return true;        // Returns true on success
}

// Atomic function 4: Set buffer pointers
static bool set_buffer_pointers(source src, char *buffer, usize len) {
   if (!src) {
      return false;
   }
   src->buffer.bucket = buffer;
   src->buffer.end = buffer + len;
   return true; // Returns true on success
}

// Atomic function 5: Detect dialect
static anvl_dialect detect_dialect(const char *data, usize len) {
   if (len >= ANVL_SHEBANG_LEN && strncmp(data, ANVL_SHEBANG_AML, ANVL_SHEBANG_LEN) == 0)
      return ANVL_DIALECT_AML;
   return ANVL_DIALECT_ASL;
}

// Atomic function 6: Initialize source fields
static bool init_source_fields(source src, anvl_dialect dialect) {
   if (!src)
      return false;
   src->dialect = dialect;
   src->stride = sizeof(char);
   src->pos = 0;
   src->line = 1;
   src->col = 1;
   return true; // Returns true on success
}

// Diagnostic version of source_create - split into atomic parts
static source source_create_diagnostic(const char *data, usize len) {
   // Step 1: Allocate source struct
   source src = alloc_source_struct();
   if (!src)
      return NULL;

   char *buffer = NULL;
   bool success = false;

   // Step 2: Allocate buffer
   buffer = alloc_buffer(data, len);
   if (data && len > 0 && !buffer) {
      goto cleanup;
   }

   // Step 3: Copy data to buffer
   if (buffer && !copy_data_to_buffer(buffer, data, len)) {
      goto cleanup;
   }

   // Step 4: Set buffer pointers
   if (!set_buffer_pointers(src, buffer, len)) {
      goto cleanup;
   }

   // Step 5: Detect dialect
   anvl_dialect dialect = (data && len > 0) ? detect_dialect(data, len) : ANVL_DIALECT_ASL;

   // Step 6: Initialize source fields
   if (!init_source_fields(src, dialect)) {
      goto cleanup;
   }

   success = true;

cleanup:
   if (!success) {
      if (buffer)
         Allocator.free(buffer);
      if (src)
         Allocator.free(src);
      return NULL;
   }

   return src;
}

// Test function to run diagnostics
void run_memory_diagnostic(void) {
   fprintf(stderr, "Running memory leak diagnostic...\n");

   // Test case 1: Empty source
   fprintf(stderr, "Test 1: Empty source\n");
   source src1 = source_create_diagnostic(NULL, 0);
   if (src1) {
      fprintf(stderr, "  Created empty source successfully\n");
      // Dispose to check for leaks
      Source.dispose(src1);
      fprintf(stderr, "  Disposed empty source\n");
   } else {
      fprintf(stderr, "  Failed to create empty source\n");
   }

   // Test case 2: Source with data
   fprintf(stderr, "Test 2: Source with data\n");
   const char *test_data = "#!anvl-aml\nsome content";
   source src2 = source_create_diagnostic(test_data, strlen(test_data));
   if (src2) {
      fprintf(stderr, "  Created source with data successfully\n");
      // Dispose to check for leaks
      Source.dispose(src2);
      fprintf(stderr, "  Disposed source with data\n");
   } else {
      fprintf(stderr, "  Failed to create source with data\n");
   }

   fprintf(stderr, "Diagnostic complete - check memory state\n");
}