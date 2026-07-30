/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * helpers.c - Test utility functions
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/utilities/helpers.c
 */

#include "helpers.h"
#include "anvil.h"
// -----------------------------------------------------------------
#include <stdio.h>
#include <sigma/strings.h>

static char path_buffer[512];

const char *fixture_path(const char *name) {
   snprintf(path_buffer, sizeof(path_buffer), "../../test/fixtures/%s", name);
   return path_buffer;
}
