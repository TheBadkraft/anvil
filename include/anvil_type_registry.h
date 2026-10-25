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
 * anvil_type_registry.h - Public API for Anvil's opt-in type registry    *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Part of ANVL proper, just opt-in (src/anvil_types.c) — not the add-on;  *
 * schema.c is the actual add-on, layered on top of this, once it exists. *
 * Built entirely on the public flat API (anvil_flat.h); no core parser/  *
 * resolver changes required. See notes/native-schema.md for the design  *
 * this implements: a @[types]-attributed document's top-level statements*
 * are, without exception, type definitions — never ordinary data.       *
 * ********************************************************************** */
#pragma once

#include "anvil_types.h"
#include <stddef.h>

/**
 * @brief Opaque handle to a loaded type registry — the result of walking one
 * @[types]-attributed document's own top-level statements.
 *
 * Independent of the anvil_document it was built from once loaded (every
 * type definition's data is copied out at load time) — safe to keep using
 * after that document is disposed. Dispose with anvil_type_registry_dispose.
 */
typedef struct anvil_type_registry_t *anvil_type_registry;

/**
 * @brief Opaque handle to one type definition within a registry.
 *
 * Lifetime is tied to the anvil_type_registry it came from — never valid
 * after that registry is disposed.
 */
typedef struct anvil_type_def_t *anvil_type_def;

/**
 * @brief The native primitive (or enum) a type definition is built on — see
 * notes/native-schema.md's "Decided — native primitive set". ANVIL_TYPE_ENUM
 * is a seventh, engine-understood kind (type := enum;), not one of the six
 * true native primitives.
 */
typedef enum {
   ANVIL_TYPE_UNKNOWN = 0, // absent, or a `type :=` value that names nothing recognized
   ANVIL_TYPE_NUMERIC,
   ANVIL_TYPE_STRING,
   ANVIL_TYPE_BOOL,
   ANVIL_TYPE_OBJECT,
   ANVIL_TYPE_TUPLE,
   ANVIL_TYPE_ARRAY,
   ANVIL_TYPE_ENUM,
} anvil_type_kind;

/**
 * @brief Build a registry from a document's own top-level statements.
 * @param doc The document to load. Must carry the @[types] module attribute
 * — every one of its top-level statements is then treated, without
 * exception, as a type definition (see notes/native-schema.md). A statement
 * that doesn't have a valid type-definition shape (no `type :=` field naming
 * a recognized kind) is skipped, not registered, and reported as an
 * ANVIL_TYPE_DIAG_UNKNOWN_BASE_TYPE diagnostic.
 * @return The registry handle, or NULL if doc is NULL or does not carry
 * @[types]. Dispose with anvil_type_registry_dispose once done.
 */
anvil_type_registry anvil_type_registry_load(anvil_document doc);

/**
 * @brief Build a registry from a document's include graph, the mechanism
 * behind the `types.X` namespace: `doc` itself does not need to carry
 * @[types] at all. Includes are followed transitively, depth-first in
 * include order, each document visited at most once; every document on the
 * way that carries @[types] has its type definitions merged into one combined
 * registry, keyed by bare name (e.g. "VIN", not "types.VIN"). A document
 * without @[types] is walked through but contributes nothing. Deeper
 * includes merge first, so a definition closer to `doc` overrides a deeper
 * one - `types.` is a flat namespace, independent of which included file a
 * given name actually came from. Any name declared twice resolves last-write
 * and is reported through the registry's diagnostics.
 * @param doc The (typically non-@[types]) document whose includes to walk.
 * @return The registry handle (real but empty if `doc` has no @[types]
 * includes), or NULL only if doc itself is NULL. Dispose with
 * anvil_type_registry_dispose once done.
 */
anvil_type_registry anvil_type_registry_load_from_includes(anvil_document doc);

/**
 * @brief Release a registry. Safe to call with NULL (no-op).
 */
void anvil_type_registry_dispose(anvil_type_registry reg);

/**
 * @brief Look up one type definition by its declared name.
 * @param reg The registry to search.
 * @param name The type's declared name (e.g. "VIN").
 * @return The definition handle, or NULL if reg/name is NULL, or no type by
 * that name was registered.
 */
anvil_type_def anvil_type_registry_find(anvil_type_registry reg, const char *name);

/**
 * @brief Resolves any type reference to one uniform, queryable handle — a
 * bare native primitive name, the bare `enum` kind, or a registered custom
 * type (e.g. "VIN") — regardless of source. The native/enum vocabulary
 * (notes/native-schema.md's "Decided — native primitive set") is checked
 * first and always available even without a registry; a name that isn't
 * one of those falls through to anvil_type_registry_find(reg, name). This
 * is what a schema field's `type :=` should always resolve through, so
 * every caller reads size/min/max/values through the same accessors no
 * matter which of those two sources actually produced the definition.
 * @param reg The registry to fall back to for custom types, or NULL if
 * only native/enum resolution is needed (e.g. a document with no includes).
 * @param name The type name to resolve (e.g. "Numeric", "enum", "VIN").
 * @return The definition handle, or NULL if name is NULL, or name is
 * neither a native/enum name nor found in reg (including when reg is NULL
 * and name isn't native/enum). Native/enum handles have a lifetime
 * independent of any registry (safe to use after the registry that would
 * otherwise have been consulted is disposed); a registry-sourced handle
 * still follows anvil_type_registry_find's own lifetime rules.
 */
anvil_type_def anvil_type_resolve(anvil_type_registry reg, const char *name);

/**
 * @brief A type definition's own kind.
 * @param def The definition to read. NULL returns ANVIL_TYPE_UNKNOWN.
 */
anvil_type_kind anvil_type_def_get_kind(anvil_type_def def);

/**
 * @brief A type definition's declared `size`, if any.
 * @param def The definition to read.
 * @param out_size Set to the declared size on success; untouched otherwise.
 * @return true if `size` was declared; false if def is NULL or it wasn't.
 */
bool anvil_type_def_get_size(anvil_type_def def, long long *out_size);

/**
 * @brief A type definition's declared `min`, if any.
 * @param def The definition to read.
 * @param out_min Set to the declared minimum on success; untouched otherwise.
 * @return true if `min` was declared as an integral number that fits; false if def is NULL, it
 * wasn't declared, or it is fractional/exponent-form (never truncated to fit - use
 * anvil_type_def_get_min_numeric for those).
 */
bool anvil_type_def_get_min(anvil_type_def def, long long *out_min);

/**
 * @brief A type definition's declared `min`, exactly - fractional and exponent forms included.
 * @param def The definition to read.
 * @param out_min Set to the declared minimum on success; untouched otherwise. See
 * anvil_numeric_t: `is_integral` reflects the spelling, so `1e+3` reads as the double 1000.
 * @return true if `min` was declared as a valid number; false if def is NULL, it wasn't
 * declared, or it was malformed (reported as a MALFORMED_CONSTRAINT diagnostic).
 */
bool anvil_type_def_get_min_numeric(anvil_type_def def, anvil_numeric_t *out_min);

/**
 * @brief A type definition's declared `max`, if any.
 * @param def The definition to read.
 * @param out_max Set to the declared maximum on success; untouched otherwise.
 * @return true if `max` was declared as an integral number that fits; false if def is NULL, it
 * wasn't declared, or it is fractional/exponent-form (never truncated - see
 * anvil_type_def_get_max_numeric).
 */
bool anvil_type_def_get_max(anvil_type_def def, long long *out_max);

/**
 * @brief A type definition's declared `max`, exactly. Same contract as
 * anvil_type_def_get_min_numeric.
 */
bool anvil_type_def_get_max_numeric(anvil_type_def def, anvil_numeric_t *out_max);

/**
 * @brief The number of enum member labels a type declared via `values`.
 * @param def The definition to read. NULL, or a non-ANVIL_TYPE_ENUM kind,
 * or one with no `values` array, all return 0.
 */
size_t anvil_type_def_get_value_count(anvil_type_def def);

/**
 * @brief One enum member's own label text, by its ordinal (array index).
 * @param def The definition to read.
 * @param index Zero-based ordinal — matches the member's own implicit
 * numeric value (see notes/native-schema.md's enum design).
 * @param buf Destination buffer, or NULL to only query the required length.
 * @param buflen Size of buf in bytes, including room for the NUL terminator.
 * @return The full label length (excluding NUL), regardless of buflen —
 * same snprintf-style convention as anvil_statement_get_name. 0 if def is
 * NULL or index is out of bounds.
 */
size_t anvil_type_def_get_value(anvil_type_def def, size_t index, char *buf, size_t buflen);

/**
 * @brief Opaque handle to one registry diagnostic - something the registry noticed while
 * loading that a consumer can't otherwise see (a name collision, a malformed definition).
 *
 * The registry reports; whether a diagnostic is fatal is the consumer's call. Lifetime is tied
 * to the anvil_type_registry it came from. Independent of the source documents: every string is
 * copied out at load time, so diagnostics stay readable after those documents are disposed.
 */
typedef struct anvil_type_diagnostic_t *anvil_type_diagnostic;

/**
 * @brief Stable diagnostic categories - this API's own set, independent of errors.h. Only ever
 * appended to.
 */
typedef enum {
   ANVIL_TYPE_DIAG_NONE = 0,
   ANVIL_TYPE_DIAG_NAME_COLLISION,        // a name was defined more than once; the later one wins
   ANVIL_TYPE_DIAG_UNKNOWN_BASE_TYPE,     // `type :=` is missing or names no recognized kind;
                                          // the definition is not registered
   ANVIL_TYPE_DIAG_MALFORMED_CONSTRAINT,  // a size/min/max isn't a valid number; that constraint
                                          // is treated as not declared
   ANVIL_TYPE_DIAG_MALFORMED_ENUM_VALUES, // `values` is not an array; no labels are declared
} anvil_type_diag_code;

/**
 * @brief How many diagnostics loading this registry produced, in the order they were found.
 * @param reg The registry to read. NULL returns 0.
 */
size_t anvil_type_registry_get_diagnostic_count(anvil_type_registry reg);

/**
 * @brief One diagnostic by index.
 * @return The handle, or NULL if reg is NULL or index is out of bounds.
 */
anvil_type_diagnostic anvil_type_registry_get_diagnostic(anvil_type_registry reg, size_t index);

/**
 * @brief The diagnostic's category. NULL returns ANVIL_TYPE_DIAG_NONE.
 */
anvil_type_diag_code anvil_type_diagnostic_get_category(anvil_type_diagnostic diag);

/**
 * @brief The name of the type the diagnostic is about (e.g. "VIN").
 * @param buf Destination buffer, or NULL to only query the required length.
 * @param buflen Size of buf in bytes, including room for the NUL terminator.
 * @return The full length (excluding NUL), regardless of buflen. 0 if diag is NULL.
 */
size_t anvil_type_diagnostic_get_name(anvil_type_diagnostic diag, char *buf, size_t buflen);

/**
 * @brief The path of the document the problem was found in (for a NAME_COLLISION, the later
 * document - the one whose definition won). Same buffer convention as get_name.
 */
size_t anvil_type_diagnostic_get_source(anvil_type_diagnostic diag, char *buf, size_t buflen);

/**
 * @brief A human-readable description. A NAME_COLLISION message names both documents. Same
 * buffer convention as get_name.
 */
size_t anvil_type_diagnostic_get_message(anvil_type_diagnostic diag, char *buf, size_t buflen);
