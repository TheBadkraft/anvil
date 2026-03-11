#include "test_common.h"
#include "../include/anvil.h"
#include "../include/context.h"
#include <stdio.h>

// Single minimal test to trace memory allocation
static void test_single_reject(void) {
   Memory.init();
   
   const char *content = "#!amp\nmsg := {key: \"value\"}";
   
   fprintf(stderr, "\n=== STARTING PARSE ===\n");
   fprintf(stderr, "Input: %s\n", content);
   
   // Create envelope and parse
   anvl_envelope env;
   env.content = (char *)content;
   env.content_len = strlen(content);
   env.name = "test.amp";
   env.name_len = strlen(env.name);
   env.source_type = ANVL_SOURCE_AML;
   
   bool result = Anvil.parse_envelope(&env);
   fprintf(stderr, "Parse result: %s\n", result ? "SUCCESS" : "FAIL");
   
   if (env.context) {
      fprintf(stderr, "Context returned: %p\n", (void *)env.context);
      Context.dispose(env.context);
      fprintf(stderr, "Context disposed\n");
   }
   
   fprintf(stderr, "=== PARSE COMPLETE ===\n");
   
   Memory.teardown();
}

void setup_test_leak(void) {
   // Setup
}

void teardown_test_leak(void) {
   Anvil.cleanup();
}

int main(void) {
   testset("leak_debug", setup_test_leak, teardown_test_leak);
   testcase("single reject", test_single_reject);
   return 0;
}
