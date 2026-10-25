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
 * internal/format.h - the closed set of lexical string formats           *
 * ---------------------------------------------------------------------- *
 * Hand-written validators (no regex engine; the core stays dependency-   *
 * free), defined in anvil_types.c and shared with schema.c. A small,      *
 * closed set on purpose: arbitrary patterns and time-zone awareness are   *
 * out of scope.                                                           *
 * ********************************************************************** */

#pragma once

#include <stdbool.h>

/// True if `name` is one of the known formats.
bool anvil_format_known(const char *name);

/// True if `text` is lexically valid for the known format `name`. An unknown name is never valid.
///   iso8601_date      YYYY-MM-DD with a real calendar check (month length, leap years)
///   iso8601_datetime  RFC 3339: YYYY-MM-DDTHH:MM:SS[.fraction](Z|+HH:MM|-HH:MM); the offset is
///                     required, 'T' and 'Z' are upper case, seconds may be 60 (leap second)
///   uuid              8-4-4-4-12 hexadecimal digits, either case, any version
bool anvil_format_valid(const char *name, const char *text);
