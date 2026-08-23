/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * constants.h - Internal constant definitions for Anvil                  *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * File: include/internal/constants.h                                     *
 * ********************************************************************** */
#pragma once

#define ANVL_MAX_NAMESPACE_LEN 256
#define ANVL_MAX_FILEPATH_LEN 1024

#define ANVL_CTX_DEFAULT_DOC_CAP 5
#define ANVL_CTX_DEFAULT_ERR_CAP 5
#define ANVL_CTX_DEFAULT_MAP_CAP 8 // must be power-of-two for map
// Node index lists grow with source size, not document count (see
// notes/document-body-parse.md "Arena node iteration") — a higher starting
// point than docs/errors, still just a growth hint via List.append.
#define ANVL_CTX_DEFAULT_NODE_CAP 32