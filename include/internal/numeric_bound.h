/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 *                                                                        *
 * This software is proprietary and confidential. Unauthorized copying,   *
 * distribution, modification, or use of this software, via any medium,   *
 * is strictly prohibited without express written permission from the     *
 * copyright holder.                                                      *
 *                                                                        *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * internal/numeric_bound.h - exact numeric constraint reading/comparison *
 * ---------------------------------------------------------------------- *
 * Shared by anvil_types.c (which defines these) and schema.c. Not public  *
 * API: schema.c reaches a type's fractional bounds here until the public  *
 * *_numeric getters land (FR-2610-anvl-types-005).                        *
 * ********************************************************************** */

#pragma once

#include "anvil_flat.h"
#include "anvil_type_registry.h"
#include "anvil_types.h"
#include <stdbool.h>

/// Reads a constraint declaration (`min`/`max`) exactly. False when val isn't a NUMERIC or
/// overflows - the caller treats that as "constraint not declared", never as 0.
bool anvil_numeric_read_bound(anvil_value val, anvil_numeric_t *out);

/// Reads an integral, non-overflowing, non-negative `size` declaration; false otherwise.
bool anvil_numeric_read_size(anvil_value val, long long *out);

/// Exact three-way comparison of data against bound: <0, 0, >0. int64 pairs compare as int64,
/// mixed pairs without rounding, and an overflowed data value compares by its sign alone
/// (its magnitude exceeds every finite bound).
int anvil_numeric_compare(anvil_value data, anvil_numeric_t bound);

/// Exact three-way comparison of two non-overflowed numbers (used to detect min > max).
int anvil_numeric_compare_bounds(anvil_numeric_t a, anvil_numeric_t b);

/// A type definition's declared bound, fractional or not. False when not declared.
bool anvil_type_def_numeric_min(anvil_type_def def, anvil_numeric_t *out);
bool anvil_type_def_numeric_max(anvil_type_def def, anvil_numeric_t *out);
