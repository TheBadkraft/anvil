/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * helpers.h - Test utility functions
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: test/utilities/helpers.h
 */
#pragma once

#include "internal/source.h"
#include "errors.h"
#include "types.h"

/* Path resolution */
const char *fixture_path(const char *name);

/* Context specification reset */
anvl_result reset_context_spec_defaults(anvl_err_code *out_err_code);
