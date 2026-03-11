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
#include <sigma.core/types.h>
#include <sigma.memory/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *get_anvl_path(const char *name) {
   static char path_buffer[512] = {0};
   snprintf(path_buffer, sizeof(path_buffer), "test/samples/%s.anvl", name);
   return path_buffer;
}

const char *get_source_path(const char *name) {
   static char path_buffer[512] = {0};
   snprintf(path_buffer, sizeof(path_buffer), "test/samples/%s", name);
   return path_buffer;
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
   strcpy(buffer, "deep := ");

   // Generate 8 levels of nesting (reduced from 15)
   for (int i = 0; i < 8; i++) {
      if (i % 3 == 0) {
         strcat(buffer, "{level := ");
      } else if (i % 3 == 1) {
         strcat(buffer, "[");
      } else {
         strcat(buffer, "(");
      }
   }

   // Add some content at the deepest level
   strcat(buffer, "42");

   // Close all nesting levels
   for (int i = 7; i >= 0; i--) {
      if (i % 3 == 0) {
         strcat(buffer, "}");
      } else if (i % 3 == 1) {
         strcat(buffer, "]");
      } else {
         strcat(buffer, ")");
      }
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