/*
 * test_diagnostic.c - Test the memory leak diagnostic
 */

#include "anvil.h"
#include "utilities/helpers.h"
#include <sigma.memory/memory.h>
#include <sigtest/sigtest.h>
#include <string.h>

// Include the diagnostic implementation
#include "utilities/diagnostic.c"

// Forward declare the diagnostic function (now defined above)
// void run_memory_diagnostic(void);

static void set_config(FILE **logger) {
   // configure logging stream
   *logger = fopen("logs/test_diagnostic.log", "w");
}

static void set_teardown() {
   Anvil.cleanup();
}

static void test_memory_diagnostic(void) {
   run_memory_diagnostic();
}

__attribute__((constructor)) static void register_test_diagnostic(void) {
   // Diagnostic test - commented out to keep inert
   // testset("Memory Diagnostic", set_config, set_teardown);
   // testcase("Memory Leak Diagnostic", test_memory_diagnostic);
}