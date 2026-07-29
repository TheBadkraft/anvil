/*
 * Copyright (c) 2025-26 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ----------------------------------------------------------------------- *
 * parser.h - Parser interface for Anvil
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * Created: 2025-12-14
 * File: include/internal/parser.h
 * ----------------------------------------------------------------------- *
 * Description:
 * Parser interface and structures for Anvil AML/ASL parsing
 * ----------------------------------------------------------------------- *
 */
#pragma once

#include "anvil.h"
#include "internal/module.h"
// -----------------------------------------------------------------
#include <stdbool.h>

typedef struct anvl_parser_vt {
  bool (*parse)(anvl_doc doc);
  bool (*parse_statement)(anvl_doc doc);
  bool (*parse_value)(anvl_doc doc);
  void (*reset)(anvl_doc doc);
} anvl_parser_vt;

typedef struct anvl_parser {
  const anvl_parser_vt *p;
} anvl_parser;

/* ----------------------------------------------------------------- */
/* Parser Interface                                                   */
/* ----------------------------------------------------------------- */
bool parser_new(anvl_doc doc);
void parser_dispose(parser parser);