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
 * anvil_schema.h - Public API for AnvilSchema                            *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * The actual add-on, layered on top of ANVL proper (unlike anvil_types.c,*
 * which is opt-in but part of ANVL proper itself). Depends on the public *
 * type-registry API (anvil_type_registry.h) and the core public flat API *
 * (anvil_flat.h) only — no internal headers. See docs/schema/README.md   *
 * for why this needs no new grammar, and notes/native-schema.md for the  *
 * full design record.                                                   *
 * ********************************************************************** */
#pragma once

#include "anvil_type_registry.h"
#include "anvil_types.h"
#include <stddef.h>

/**
 * @brief Opaque handle to a loaded schema — a ruleset built from a
 * @[schema]-attributed document's own top-level statements, each treated
 * as one field rule.
 *
 * Independent of the anvil_document it was built from once loaded (every
 * field rule's data is copied out at load time) — safe to keep using after
 * that document is disposed. Dispose with anvil_schema_dispose.
 */
typedef struct anvil_schema_t *anvil_schema;

/**
 * @brief Opaque handle to one violation found by the most recent
 * anvil_schema_validate call against a given anvil_schema.
 *
 * Lifetime is tied to the anvil_schema it came from, and specifically to
 * its *last* anvil_schema_validate call — calling validate again replaces
 * every previously-returned violation handle. Never valid after the
 * schema itself is disposed.
 */
typedef struct anvil_schema_violation_t *anvil_schema_violation;

/**
 * @brief Stable violation categories. Numeric values deliberately mirror
 * the reserved "Schema Errors (46xx)" block in the core's own internal
 * errors.h — a namespace nod, not a shared enum; this is schema's own,
 * independent, public one.
 */
typedef enum {
   ANVIL_SCHEMA_ERR_NONE = 0,
   ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED = 4604,      // a required field is absent
   ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH = 4605, // a field's value doesn't match its declared type
   ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD = 4606, // a data field isn't declared in the schema
} anvil_schema_err_code;

/**
 * @brief Build a schema (a ruleset) from a document's own top-level
 * statements.
 * @param doc The document to load. Must carry the @[schema] module
 * attribute — every one of its top-level statements is then treated,
 * without exception, as a field rule. A field's own `type :=` (if any) is
 * resolved via anvil_type_resolve, against a type registry built from
 * doc's own imports (anvil_type_registry_load_from_imports) — the same
 * `types.X` namespace any other document would see. A field rule with no
 * recognized `type :=` (including none at all — e.g. a FlyWire-style
 * `pooled` field) is still registered, just with no type to check against;
 * its `required :=` (if any) is still enforced.
 * @return The schema handle, or NULL if doc is NULL or does not carry
 * @[schema]. Dispose with anvil_schema_dispose once done.
 */
anvil_schema anvil_schema_load(anvil_document doc);

/**
 * @brief Release a schema. Safe to call with NULL (no-op).
 */
void anvil_schema_dispose(anvil_schema schema);

/**
 * @brief Validates a *data* document's own top-level statements against a
 * loaded schema, collecting every violation found — never fail-fast,
 * deliberately unlike the core parser's own per-phase behavior. Replaces
 * whatever violations a previous call against this same schema collected.
 * @param schema The schema to validate against.
 * @param data_doc The document to check.
 * @return true if no violations were found; false otherwise (including if
 * schema or data_doc is NULL).
 */
bool anvil_schema_validate(anvil_schema schema, anvil_document data_doc);

/**
 * @brief The number of violations the most recent anvil_schema_validate
 * call against this schema found.
 * @param schema The schema to read. NULL, or one never validated against
 * anything, returns 0.
 */
size_t anvil_schema_get_violation_count(anvil_schema schema);

/**
 * @brief One violation from the most recent anvil_schema_validate call,
 * by index.
 * @param schema The schema to read.
 * @param index Zero-based index, in the order violations were found.
 * @return The violation handle, or NULL if schema is NULL or index is out
 * of bounds.
 */
anvil_schema_violation anvil_schema_get_violation(anvil_schema schema, size_t index);

/**
 * @brief A violation's own category.
 * @param v The violation to read. NULL returns ANVIL_SCHEMA_ERR_NONE.
 */
anvil_schema_err_code anvil_schema_violation_get_category(anvil_schema_violation v);

/**
 * @brief The name of the field a violation is about.
 * @param v The violation to read. NULL writes nothing and returns 0.
 * @param buf Destination buffer, or NULL to only query the required length.
 * @param buflen Size of buf in bytes, including room for the NUL terminator.
 * @return The full name length (excluding NUL), regardless of buflen —
 * same snprintf-style convention used throughout the public API.
 */
size_t anvil_schema_violation_get_field(anvil_schema_violation v, char *buf, size_t buflen);

/**
 * @brief A human-readable message describing a violation.
 * @param v The violation to read. NULL writes nothing and returns 0.
 * @param buf Destination buffer, or NULL to only query the required length.
 * @param buflen Size of buf in bytes, including room for the NUL terminator.
 * @return The full message length (excluding NUL), regardless of buflen.
 */
size_t anvil_schema_violation_get_message(anvil_schema_violation v, char *buf, size_t buflen);
