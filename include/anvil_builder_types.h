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
 * anvil_builder_types.h - Handles shared by the builder headers          *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * The builder is part of the writer add-on: it sits on the streaming     *
 * writer (anvil_writer.h), shares its dialect and error enums, and is    *
 * compiled with it (WITH_WRITER). It includes no reader header.          *
 * See FR/FR-2609-anvl-writer-001.md.                                     *
 * ********************************************************************** */
#pragma once

#include "anvil_writer_types.h"

/**
 * @brief Opaque handle to a document under construction. Owns every node and member made
 * through it; dispose with anvil_builder_dispose. Not thread-safe.
 */
typedef struct anvil_builder_t *anvil_builder;

/**
 * @brief Opaque handle to a value (scalar, array, tuple or object). Created detached, then
 * attached exactly once - as an array/tuple element (anvil_builder_append) or as a statement's
 * value (anvil_builder_add). Owned by its builder: valid until anvil_builder_dispose, and
 * never to be passed to a different builder.
 */
typedef struct anvil_node_t *anvil_node;

/**
 * @brief Opaque handle to a statement in the document or in an object: a name, an optional
 * base, attributes, and its value. Owned by its builder, like anvil_node.
 */
typedef struct anvil_member_t *anvil_member;
