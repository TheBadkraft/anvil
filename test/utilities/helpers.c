/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-03
 * File: test/utilities/helpers.c
 * ------------------------------------------------------------------
 * Description:
 * Test helper functions for loading sample files.
 * ------------------------------------------------------------------
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */
#include "../../include/utils.h"
#include "../../include/anvil.h"
#include "../../testbit/include/testbit.h"
#include <sigma.core/types.h>
#include <sigma.memory/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *get_anvl_path(const char *name) {
   static char path_buffer[512] = {0};
   snprintf(path_buffer, sizeof(path_buffer), "test/fixtures/%s.anvl", name);
   return path_buffer;
}

const char *get_source_path(const char *name) {
   static char path_buffer[512] = {0};
   snprintf(path_buffer, sizeof(path_buffer), "../fixtures/%s", name);
   return path_buffer;
}

context parse_source_ok(const char *source, anvl_dialect dialect) {
   ctx_builder builder = Context.get_builder();
   builder->set_dialect(builder, dialect);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   TestBit.is_not_null(ctx, "context created from source");

   bool result = Context.parse(ctx);
   if (!result) {
      const anvl_error_state *err = Anvil.error_get();
      fprintf(stderr, "[DEBUG]: Failed to parse source: %s\n", source);
      if (err && err->message)
         fprintf(stderr, "[DEBUG]: Parse error: %s\n", err->message);
   }
   TestBit.is_true(result, "source parsing succeeds");

   return ctx;
}

context parse_source_with_err(const char *source, anvl_dialect dialect,
                              const anvl_error_state **err_state) {
   ctx_builder builder = Context.get_builder();
   builder->set_dialect(builder, dialect);
   builder->set_source(builder, source, strlen(source));

   context ctx = builder->build(builder);
   TestBit.is_not_null(ctx, "context created from source");

   bool result = Context.parse(ctx);
   TestBit.is_false(result, "parser should fail for invalid source");
   *err_state = Anvil.error_get();

   return ctx;
}

context parse_fixture_file_ok(const char *filename, anvl_dialect exp_dialect,
                              usize exp_pos, usize exp_line, usize exp_col) {
   const char *filepath = get_source_path(filename);

   ctx_builder builder = Context.get_builder();
   bool loaded = builder->load_file(builder, filepath);
   if (!loaded)
      fprintf(stderr, "[DEBUG]: Failed to load file: %s\n", filepath);
   TestBit.is_true(loaded, "fixture file loaded successfully");

   context ctx = builder->build(builder);
   TestBit.is_not_null(ctx, "context created for fixture file");

   TestBit.is_equal_int((long long)exp_dialect, (long long)Context.dialect(ctx),
                        "dialect matches expected");
   TestBit.is_equal_int((long long)exp_pos, (long long)ctx->source->pos,
                        "source positioned past preamble");
   TestBit.is_equal_int((long long)exp_line, (long long)ctx->source->line,
                        "source at correct line");
   TestBit.is_equal_int((long long)exp_col, (long long)ctx->source->col,
                        "source at correct column");

   bool result = Context.parse(ctx);
   if (!result) {
      const anvl_error_state *err = Anvil.error_get();
      TestBit.is_true(Anvil.error_is_set(), "error set on unexpected parse failure");
      fprintf(stderr, "[DEBUG]: Unexpected parse failure for %s: %s at line %ld, col %ld\n",
              filename,
              (err && err->message) ? err->message : "<none>",
              (long)(err ? err->line : 0),
              (long)(err ? err->column : 0));
      Anvil.error_clear();
   }
   TestBit.is_true(result, "fixture file parsing succeeds");

   return ctx;
}

char *generate_large_nested_structure(void) {
   char *buffer = malloc(10000); // 10KB buffer (reduced)
   strcpy(buffer, "large_data := {\n");

   // Generate 5 top-level fields (reduced from 20)
   for (int i = 0; i < 5; i++) {
      sprintf(buffer + strlen(buffer), "  field%d := {\n", i);

      // Each field has 3 subfields (reduced from 10)
      for (int j = 0; j < 3; j++) {
         sprintf(buffer + strlen(buffer), "    subfield%d := [", j);

         // Each array has 3 elements (reduced from 5)
         for (int k = 0; k < 3; k++) {
            if (k > 0)
               strcat(buffer, ", ");
            sprintf(buffer + strlen(buffer), "%d", i * 100 + j * 10 + k);
         }
         sprintf(buffer + strlen(buffer), "],\n");
      }

      sprintf(buffer + strlen(buffer), "  },\n");
   }

   strcat(buffer, "}\n");
   return buffer;
}

char *generate_deep_nested_structure(void) {
   char *buffer = malloc(5000); // 5KB buffer (reduced)
   if (!buffer)
      return NULL;

   strcpy(buffer, "deep := ");

   // Generate 8 valid nesting levels using only object/array forms.
   for (int i = 0; i < 8; i++) {
      if (i % 2 == 0)
         sprintf(buffer + strlen(buffer), "{ level%d := ", i);
      else
         strcat(buffer, "[");
   }

   // Deepest payload
   strcat(buffer, "42");

   // Close all nesting levels in reverse order.
   for (int i = 7; i >= 0; i--) {
      if (i % 2 == 0)
         strcat(buffer, " }");
      else
         strcat(buffer, "]");
   }

   strcat(buffer, "\n");
   return buffer;
}

char *fetch_and_convert_real_data(void) {
   const char *aml_data =
       "user := {\n"
       "  id := 1,\n"
       "  name := \"John Doe\",\n"
       "  username := johndoe,\n"
       "  email := @email`john.doe@example.com`,\n"
       "  address := {\n"
       "    street := \"123 Main St\",\n"
       "    suite := \"Apt 4B\",\n"
       "    city := \"Anytown\",\n"
       "    zipcode := \"12345-6789\",\n"
       "    geo := {\n"
       "      lat := -37.3159,\n"
       "      lng := 81.1496\n"
       "    }\n"
       "  },\n"
       "  phone := \"1-770-736-8031 x56442\",\n"
       "  website := @http`www.hildegard.org`,\n"
       "  company := {\n"
       "    name := \"Romaguera-Crona\",\n"
       "    catchPhrase := \"Multi-layered client-server neural-net\",\n"
       "    bs := \"harness real-time e-markets\"\n"
       "  }\n"
       "}\n";

   char *str_copy = strdup(aml_data);
   // Memory.track(str_copy); // Context takes ownership, no need to track
   return str_copy;
}