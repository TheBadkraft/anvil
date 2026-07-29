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


#include "source.h"
#include "internal/parser.h"
#include "internal/root.h"

/* Path resolution */
const char *fixture_path(const char *name);

/* Create doc & parser boilerplate */
bool create_doc_with_parser(anvl_doc *doc, const char *filepath);