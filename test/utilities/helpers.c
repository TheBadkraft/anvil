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
#include <sigcore/types.h>
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