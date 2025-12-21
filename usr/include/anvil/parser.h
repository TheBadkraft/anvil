/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * This software is proprietary and confidential. Unauthorized copying,
 * distribution, modification, or use of this software, via any medium,
 * is strictly prohibited without express written permission from the
 * copyright holder.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * parser.h - Parser interface for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: include/parser.h
 * ------------------------------------------------------------------
 * Description:
 * Parser interface and structures for Anvil AML/ASL parsing
 * ------------------------------------------------------------------
 */
#pragma once

#include "anvil.h"
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Parser Interface                                                   */
/* ------------------------------------------------------------------ */
bool anvl_parse(context ctx);