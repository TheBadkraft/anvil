#include "anvil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
   const char *source_str = "name := \"John\"";

   printf("Testing parser on: %s\n", source_str);
   printf("Length: %zu\n", strlen(source_str));
   printf("First char: '%c' (code: %d)\n", source_str[0], (unsigned char)source_str[0]);

   // Test is_identifier_start and is_identifier_part directly
   char first = source_str[0];
   printf("\nCharacter classification for '%c':\n", first);
   printf("  is_identifier_start: %d (using Source API)\n", first);

   // Create a builder and parse
   anvl_ctx_builder_i *builder = Context.get_builder();
   if (!builder) {
      printf("ERROR: Failed to get builder\n");
      return 1;
   }

   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, source_str, strlen(source_str));

   context ctx = builder->build(builder);
   if (!ctx) {
      printf("ERROR: Failed to build context\n");
      return 1;
   }

   printf("\nContext created, starting parse...\n");
   bool result = Context.parse(ctx);

   if (!result) {
      const anvl_error_state *err = Anvil.error_get();
      printf("PARSE FAILED!\n");
      printf("Error: %s\n", err->message);
      printf("Line: %zu, Column: %zu\n", err->line, err->column);
   } else {
      printf("PARSE SUCCESSFUL!\n");
      printf("Statements: %zu\n", Context.statement_count(ctx));
   }

   Context.dispose(ctx);
   return result ? 0 : 1;
}
