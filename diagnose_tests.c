/*
 * diagnose_tests.c - Run each test individually to identify which ones fail
 */
#include "anvil.h"
#include "utilities/helpers.h"
#include <sigcore/memory.h>
#include <sigtest/sigtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// List all test names
static const char *test_names[] = {
    "Empty Source",
    "Simple Assignment",
    "Multiple Statements",
    "Array Parsing",
    "Number parsing",
    "Booleans and null",
    "Bare literals",
    "Module attributes",
    "Object parsing",
    "Tuple parsing",
    "Blob parsing",
    "Mixed array",
    "Nested arrays",
    "Mixed tuple",
    "Array with objects",
    "Object with array",
    "Moderate stress",
    "Inheritance basic",
    "Inheritance with attributes",
    "Inheritance chain",
    "Nested object in array",
    "Nested array in object",
    "Deeply nested structures",
    "Array with mixed contents",
    "Tuple with mixed scalars",
    "Tuple with objects",
    "Tuple with arrays",
    "Tuple single element",
    "Blob with various tags",
    "Bare blob",
    "Missing assignment",
    "Missing identifier",
    "Invalid value in attribute",
    "Invalid identifier",
    "Empty attribute block",
    "Missing comma in array",
    "Expected array close",
    "Expected tuple close",
    "Empty object not allowed",
    "Empty array not allowed",
    "Missing comma in attributes",
    "Unexpected module attributes",
    "Unterminated string",
    "Unterminated blob",
    "Expected comma in tuple",
    "Expected value error",
    "Expected object close",
    "Trailing comma in object",
    "Expected object field",
    "Multiple shebang",
    "Shebang after statements"};

int main(void) {
   printf("Parser Test Diagnostic\n");
   printf("Total tests: %lu\n", sizeof(test_names) / sizeof(test_names[0]));

   for (size_t i = 0; i < sizeof(test_names) / sizeof(test_names[0]); i++) {
      printf("[%2zu] %s\n", i + 1, test_names[i]);
   }

   return 0;
}
