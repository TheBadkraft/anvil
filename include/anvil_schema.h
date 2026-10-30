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

// The library is built with -fvisibility=hidden (src/sigma/* and other internals must never
// leak into a consumer's global dynamic symbol table -- see notes/symbol-visibility.md); this
// pragma pins every declaration below back to default visibility so the public ABI stays
// exported from libanvil.so exactly as before. `make check-exports` fails if a public header
// forgets it.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

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
 * @brief Stable violation categories — schema's own, independent set, not
 * tied to the core's internal errors.h numbering in any way (an earlier
 * revision numbered these to nod at that block; the two have no real
 * relationship, so the nod was dropped). Each distinct reason a field can
 * fail validation gets its own specific category, rather than folding
 * constraint failures into VALIDATION_TYPE_MISMATCH — a value can be the
 * right kind and still violate size/range/membership, and that's a
 * different thing than the wrong kind of value entirely.
 */
typedef enum {
   ANVIL_SCHEMA_ERR_NONE = 0,
   ANVIL_SCHEMA_ERR_VALIDATION_REQUIRED,      // a required field is absent
   ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH, // a field's value kind doesn't match its declared type
   ANVIL_SCHEMA_ERR_VALIDATION_SIZE,          // a field's value exceeds its declared size
   ANVIL_SCHEMA_ERR_VALIDATION_RANGE,         // a numeric field's value is outside its declared min/max
   ANVIL_SCHEMA_ERR_VALIDATION_VALUES,        // a field's value isn't one of its declared values
   ANVIL_SCHEMA_ERR_VALIDATION_UNKNOWN_FIELD, // a data field isn't declared in the schema
   ANVIL_SCHEMA_ERR_VALIDATION_ITEM_COUNT,    // an array has fewer than min_items / more than max_items
   ANVIL_SCHEMA_ERR_VALIDATION_TAG,           // a blob's @tag isn't the one the rule requires
   ANVIL_SCHEMA_ERR_VALIDATION_FORMAT,        // a string doesn't match the rule's `format`
   ANVIL_SCHEMA_ERR_VALIDATION_UNRESOLVED_TYPE, // a value is present for a rule whose `type` didn't resolve (fails closed)
} anvil_schema_err_code;

/**
 * @brief Build a schema (a ruleset) from a document's own top-level
 * statements.
 * @param doc The document to load. Must carry the @[schema] module
 * attribute — every one of its top-level statements is then treated,
 * without exception, as a field rule. A field's own `type :=` (if any) is
 * resolved via anvil_type_resolve, against a type registry built from
 * doc's own includes (anvil_type_registry_load_from_includes) — the same
 * `types.X` namespace any other document would see. A field rule with no
 * recognized `type :=` (including none at all — e.g. a FlyWire-style
 * `pooled` field) is still registered, just with no type to check against;
 * its `required :=` (if any) is still enforced. A rule is an ordinary nested object, so shape nests the same way:
 * `type := Object` may carry `fields := { name := { <rule> }; ... }` (recursively, unknown-field
 * checking included), and `type := Array` may carry `items := { <rule> }` for every element plus
 * `min_items` / `max_items`. A `@[types]` Object type may carry `fields` too, reusable by any field
 * that references it (an inline `fields` replaces the type's, never merges). Violations name the
 * full path, e.g. `links[2].target`; a top-level field reads exactly as its bare name. A malformed schema is still
 * returned, never rejected: what is wrong with it (an unknown type, a
 * malformed constraint, a top-level statement that isn't a rule, min > max,
 * and the type registry's own diagnostics) is reported through
 * anvil_schema_get_diagnostic, and the caller decides whether that is fatal.
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
 * @brief Options for anvil_schema_validate_value. A zero-initialised struct (with `size` set) is
 * exactly anvil_schema_validate's behavior. `size` is the caller's sizeof, so the struct can grow
 * at the end without breaking compiled callers: members beyond `size` read as their defaults.
 */
typedef struct anvil_schema_options {
   size_t size;        // sizeof(anvil_schema_options) as the caller compiled it
   bool skip_required; // don't enforce required-presence (patch semantics); every other check
                       // still applies to each field that IS present
   bool allow_unknown; // don't report fields the schema doesn't declare
} anvil_schema_options;

/**
 * @brief Validates an OBJECT *value*'s statements against a loaded schema exactly as if they were
 * a document's top level - the form a request body takes (`set := { ... };`). Like
 * anvil_schema_validate, collects every violation and replaces whatever the previous validate
 * call (either form) on this schema collected - so validating the elements of
 * `set := [ {...}, {...} ]` one by one means reading the violations between calls.
 * @param schema The schema to validate against.
 * @param object_value The value to check. A non-OBJECT value yields one
 * ANVIL_SCHEMA_ERR_VALIDATION_TYPE_MISMATCH violation with an empty field path.
 * @param opts Options, or NULL for the defaults.
 * @return true if no violations were found; false otherwise (including if schema or
 * object_value is NULL, in which case the previous violations are left untouched).
 */
bool anvil_schema_validate_value(anvil_schema schema, anvil_value object_value,
                                 const anvil_schema_options *opts);

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

/**
 * @brief Opaque handle to one schema diagnostic - something wrong with the schema *itself*
 * (as opposed to a violation, which is something wrong with a data document). Produced by
 * anvil_schema_load and valid until the schema is disposed; independent of the documents it was
 * loaded from.
 */
typedef struct anvil_schema_diagnostic_t *anvil_schema_diagnostic;

/**
 * @brief Stable diagnostic categories - this API's own set, appended-only. A schema with
 * diagnostics still loads and still validates (the weakened rule simply checks less); the
 * caller decides whether any diagnostic is fatal. Unknown rule keys (a consumer's own
 * extensions, e.g. FlyWire's `pooled`) are deliberately NOT diagnostics.
 */
typedef enum {
   ANVIL_SCHEMA_DIAG_NONE = 0,
   ANVIL_SCHEMA_DIAG_UNKNOWN_TYPE,            // a `type :=` names nothing the registry or the
                                              // native set knows; the field is left unchecked
   ANVIL_SCHEMA_DIAG_MALFORMED_CONSTRAINT,    // size/min/max isn't a valid number, `values` isn't
                                              // an array, or `required` isn't true/false
   ANVIL_SCHEMA_DIAG_RULE_NOT_OBJECT,         // a top-level statement isn't a `{ ... }` rule
   ANVIL_SCHEMA_DIAG_UNKNOWN_FORMAT,          // reserved: FR-2610-anvl-types-004
   ANVIL_SCHEMA_DIAG_SELF_REFERENCE,          // reserved: FR-2610-anvl-schema-001
   ANVIL_SCHEMA_DIAG_CONFLICTING_CONSTRAINTS, // e.g. min > max: nothing can satisfy the rule
   ANVIL_SCHEMA_DIAG_TYPE_REGISTRY,           // passed through from the type registry the schema
                                              // built from its includes; see
                                              // anvil_schema_diagnostic_get_type_category
} anvil_schema_diag_code;

/**
 * @brief How many diagnostics loading this schema produced, in the order found. The type
 * registry's own diagnostics (anvil_type_registry_get_diagnostic) for the registry built from
 * the schema's includes come first, so one call gives the full picture.
 * @param schema The schema to read. NULL returns 0.
 */
size_t anvil_schema_get_diagnostic_count(anvil_schema schema);

/**
 * @brief One diagnostic by index.
 * @return The handle, or NULL if schema is NULL or index is out of bounds.
 */
anvil_schema_diagnostic anvil_schema_get_diagnostic(anvil_schema schema, size_t index);

/**
 * @brief The diagnostic's category. NULL returns ANVIL_SCHEMA_DIAG_NONE.
 */
anvil_schema_diag_code anvil_schema_diagnostic_get_category(anvil_schema_diagnostic diag);

/**
 * @brief For an ANVIL_SCHEMA_DIAG_TYPE_REGISTRY diagnostic, the registry's own category;
 * ANVIL_TYPE_DIAG_NONE for every other diagnostic (or NULL).
 */
anvil_type_diag_code anvil_schema_diagnostic_get_type_category(anvil_schema_diagnostic diag);

/**
 * @brief The schema field the diagnostic is about (a top-level field name for now; nested paths
 * come with FR-2610-anvl-schema-001). For a TYPE_REGISTRY diagnostic, the type's name.
 * @param buf Destination buffer, or NULL to only query the required length.
 * @param buflen Size of buf in bytes, including room for the NUL terminator.
 * @return The full length (excluding NUL), regardless of buflen. 0 if diag is NULL.
 */
size_t anvil_schema_diagnostic_get_field(anvil_schema_diagnostic diag, char *buf, size_t buflen);

/**
 * @brief A human-readable description. Same buffer convention as get_field.
 */
size_t anvil_schema_diagnostic_get_message(anvil_schema_diagnostic diag, char *buf, size_t buflen);

/* ---------------------------------------------------------------------- *
 * Introspection - read a loaded schema's rules back (read-only)
 * ---------------------------------------------------------------------- */

/**
 * @brief Opaque handle to one field rule of a loaded schema (a top-level field, an Object rule's
 * nested field, or an Array rule's element rule). Lifetime is tied to the schema it came from, and
 * independent of the document the schema was loaded from.
 */
typedef struct anvil_schema_field_t *anvil_schema_field;

/**
 * @brief How many field rules the schema declares at its top level (a top-level statement that
 * isn't a `{ ... }` rule is skipped, and reported as a diagnostic).
 * @param schema The schema to read. NULL returns 0.
 */
size_t anvil_schema_get_field_count(anvil_schema schema);

/**
 * @brief The field rule at `index`, in declaration order.
 * @return The handle, or NULL if schema is NULL or index is out of bounds.
 */
anvil_schema_field anvil_schema_get_field(anvil_schema schema, size_t index);

/**
 * @brief The top-level field rule named `name`.
 * @return The handle, or NULL if schema or name is NULL, or there is no such field.
 */
anvil_schema_field anvil_schema_find_field(anvil_schema schema, const char *name);

/**
 * @brief The rule's name (for an Array's element rule, the empty string).
 * @param buf Destination buffer, or NULL to only query the required length.
 * @param buflen Size of buf in bytes, including room for the NUL terminator.
 * @return The full length (excluding NUL), regardless of buflen - same snprintf-style convention
 * as anvil_statement_get_name. 0 if field is NULL.
 *
 * Every accessor below reports the EFFECTIVE rule: what the field declared inline, with whatever
 * it didn't declare filled in from its resolved `types.X` - exactly what validation enforces.
 */
size_t anvil_schema_field_get_name(anvil_schema_field field, char *buf, size_t buflen);

/**
 * @brief The resolved kind (`Numeric`, `String`, ...). ANVIL_TYPE_UNKNOWN if the rule declares no
 * `type :=`, or names one that doesn't resolve (that is also a load diagnostic).
 */
anvil_type_kind anvil_schema_field_get_kind(anvil_schema_field field);

/**
 * @brief The `type :=` as written, e.g. `Numeric` or `types.Int32` - how a consumer learns which
 * custom type a field uses. Same buffer convention as get_name; 0 if none was declared.
 */
size_t anvil_schema_field_get_type_name(anvil_schema_field field, char *buf, size_t buflen);

/// `required := true` (false for NULL).
bool anvil_schema_field_is_required(anvil_schema_field field);
/// `nullable := true`, declared or inherited (false for NULL).
bool anvil_schema_field_is_nullable(anvil_schema_field field);
/// `integer := true`, declared or inherited (false for NULL).
bool anvil_schema_field_is_integer(anvil_schema_field field);

/// Each constraint getter returns whether the constraint is declared (inline or inherited), the
/// same "declared, not a default" convention as the type getters; `out` is untouched otherwise.
bool anvil_schema_field_get_size(anvil_schema_field field, long long *out_size);
bool anvil_schema_field_get_min_numeric(anvil_schema_field field, anvil_numeric_t *out_min);
bool anvil_schema_field_get_max_numeric(anvil_schema_field field, anvil_numeric_t *out_max);
bool anvil_schema_field_get_min_items(anvil_schema_field field, long long *out_min_items);
bool anvil_schema_field_get_max_items(anvil_schema_field field, long long *out_max_items);

/// The `values` labels. 0 / 0 when none are declared; same buffer convention as get_name. A label
/// may contain NUL bytes: the returned length is its exact byte length and the copy includes them.
size_t anvil_schema_field_get_value_count(anvil_schema_field field);
size_t anvil_schema_field_get_value(anvil_schema_field field, size_t index, char *buf, size_t buflen);

/// The `format` name and the required blob `tag`; 0 when not declared. Same buffer convention.
size_t anvil_schema_field_get_format(anvil_schema_field field, char *buf, size_t buflen);
size_t anvil_schema_field_get_tag(anvil_schema_field field, char *buf, size_t buflen);

/// An Object rule's nested field rules (`fields`), in declaration order; find by name. 0 / NULL
/// when the rule declares none.
size_t anvil_schema_field_get_nested_count(anvil_schema_field field);
anvil_schema_field anvil_schema_field_get_nested(anvil_schema_field field, size_t index);
anvil_schema_field anvil_schema_field_find_nested(anvil_schema_field field, const char *name);

/// An Array rule's element rule (`items`), or NULL.
anvil_schema_field anvil_schema_field_get_items(anvil_schema_field field);

/**
 * @brief Extension keys: every rule key the schema did not itself consume (a consumer's own
 * annotations such as FlyWire's `pooled` and `mask`), in declaration order. They are never
 * diagnostics - the schema reads what it knows and passes the rest on.
 */
size_t anvil_schema_field_get_extra_count(anvil_schema_field field);

/// The key's name at `index`. Same buffer convention as get_name; 0 if out of bounds.
size_t anvil_schema_field_get_extra_name(anvil_schema_field field, size_t index, char *buf,
                                         size_t buflen);

/**
 * @brief The key's value, readable with the ordinary anvil_value accessors. It is a copy owned by
 * the schema (so it stays valid after the source document is disposed) and lives until the schema
 * is disposed - never dispose it.
 * @return The value, or NULL if index is out of bounds or the value could not be copied (a
 * `$variable` reference cannot be, since a copy has no document to resolve it against).
 */
anvil_value anvil_schema_field_get_extra_value(anvil_schema_field field, size_t index);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif
