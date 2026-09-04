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
 * module.h - Root & Document structures                                  *
 * ---------------------------------------------------------------------- *
 * Author: BadKraft                                                       *
 * Created: 2026-07-13                                                    *
 * File: include/internal/module.h                                        *
 * ---------------------------------------------------------------------- *
 * Description:                                                           *
 * This header defines the root and document structures for Anvil.        *
 * ********************************************************************** */
#pragma once

#include "types.h"
#include "constants.h"
#include "errors.h"
// ----------------
#include <sigma/allocator.h>
#include <sigma/farray.h>
#include <sigma/list.h>
#include <sigma/map.h>
#include <sigma/types.h>

/* ---------------------------------------------------------------------- *
 * Forward Declarations:
 * ---------------------------------------------------------------------- */
//   - objects
typedef struct anvl_source_t *anvl_source;
typedef struct anvl_parser_t *anvl_parser;

struct anvl_mod_ctx_t {
   list docs;            // list of module_document
   list errors;          // list of anvl_error encountered during scanning/parsing
   anvl_parser parser;   // parser context for the module
   bump_allocator arena; // body-parse node arena, shared across every document in ctx->docs;
                         // NULL until mod_ctx_create_arena runs. See notes/document-body-parse.md
                         // "Arena-backed allocation".
   list statements;      // index of every anvl_statement allocated via Source.new_node; does not
                         // own the pointed-to nodes (the arena does) — see "Arena node iteration".
   list values;          // index of every anvl_value allocated via Source.new_node; same non-owning
                         // relationship to the arena as `statements` above.
   map identifiers;      // top-level statement name -> anvl_statement, built once by
                         // mod_resolve_context from every document's own doc->body (not the
                         // flat `statements` index above, which also includes nested
                         // statements). Shared by '$identifier' VarRef and 'base'/inheritance
                         // lookup alike. NULL until mod_resolve_context runs. See
                         // notes/resolution-phase.md.
};

typedef enum {
   ANVL_SOURCE_FROM_FILE = 0,
   ANVL_SOURCE_FROM_BUFFER,
} anvl_source_origin;

/* ---------------------------------------------------------------------- *
 * Source slice metadata — no-copy references into anvl_source buffer
 * ---------------------------------------------------------------------- */
typedef struct anvl_src_slice_t {
   const char *data;  // pointer to the start of the source buffer (raw bytes)
   const char *start; // pointer to the start of the slice within the source buffer
   const char *end;   // pointer to the end of the slice within the source buffer
} anvl_slice;
typedef anvl_slice *slice;

/* ---------------------------------------------------------------------- *
 * Header import / attribute metadata
 * ---------------------------------------------------------------------- */
typedef struct anvl_import_t {
   anvl_slice decl;          // "import \"path\"" (no trailing ';')
   anvl_slice path;          // "\"path\"" (quotes included)
   module_document resolved; // child document after import graph expansion; NULL until loaded
} anvl_import_t;
typedef anvl_import_t *anvl_import;

typedef struct anvl_attribute_t {
   anvl_slice key;   // identifier
   anvl_slice value; // value text; length 0 means flag attribute
} anvl_attribute_t;
typedef anvl_attribute_t *anvl_attribute;

/* ---------------------------------------------------------------------- *
 * Document header — collected before body parsing
 * ---------------------------------------------------------------------- */
struct anvl_doc_header_t {
   list imports;    // list of anvl_import
   list attributes; // list of anvl_attribute
};

/* ---------------------------------------------------------------------- *
 * Body value tree — see notes/document-body-parse.md
 *   - no collection/container may be empty (e.g. [] or () or {} are invalid)
 *   - tuples must have at least 2 elements (e.g. (1) is invalid)
 *   - assigning a `null` value to any type is valid (e.g. `foo := null;`)
 *   - ANVL's default is a weak type system
 * ---------------------------------------------------------------------- */
typedef enum {
   ANVL_VALUE_NONE = 0,
   ANVL_VALUE_NULL,       // explicit null value (e.g. `null` keyword)
   ANVL_VALUE_BOOL,       // explicit bare literal boolean value (e.g. `true` or `false`)
   ANVL_VALUE_NUMERIC,    // standard scalar numeric - integer, float, hex, exponential, etc.
   ANVL_VALUE_STRING,     // standard scalar quoted string; may contain escape sequences
   ANVL_VALUE_BLOB,       // standard scalar; tag stored separately, content in text span
   ANVL_VALUE_ARRAY,      // collection (e.g. `[1, 2, 3]`) - elements are anvl_value pointers
   ANVL_VALUE_TUPLE,      // collection (e.g. `(1, 2, 3)`) - elements are anvl_value pointers
   ANVL_VALUE_OBJECT,     // nested statement list, not key/value pairs
   ANVL_VALUE_IDENTIFIER, // bare symbol: an ordinary literal string (e.g. `name := David;`) - no implicit resolution
   ANVL_VALUE_VARREF,     // `$identifier`: static, resolve-once reference - see notes/document-body-parse.md
} anvl_value_type;

typedef struct anvl_value_t {
   // text & type are always set; the union below is set based on type
   anvl_value_type type;
   anvl_slice text; // full source span of the value
   union {
      struct {
         anvl_slice tag; // blob tag (e.g. @date, @sel); empty for non-blobs or untagged blobs
      } blob;
      struct {
         list items; // array/tuple: list of anvl_doc_value pointers
      } collection;
      struct {
         list statements; // object: list of anvl_doc_statement pointers
      } object;
      struct {
         anvl_slice target;  // identifier following '$', sigil excluded
         struct anvl_value_t *resolved; // set by Resolution; NULL until resolved, and stays NULL for a
                              // missing target, a reference cycle, or a target that names an
                              // anonymous (OBJECT_BLOCK) statement — none of those are parse-
                              // or resolve-time errors for a VarRef. Aliases the target's
                              // existing value node directly (safe: every anvl_value in a
                              // module_context shares one arena, disposed as a single block —
                              // see notes/resolution-phase.md "VarRef resolution").
      } varref;
   };
} anvl_value_t;
typedef anvl_value_t *anvl_value;

/* ---------------------------------------------------------------------- *
 * Body statements — two forms, both may carry `base` and `attributes`:
 *   ident [: base] [@[...]] := value;    (ANVL_STMT_ASSIGN)
 *   ident [: base] [@[...]] { stmts };   (ANVL_STMT_OBJECT_BLOCK)
 * ---------------------------------------------------------------------- */
typedef enum {
   ANVL_STMT_ASSIGN = 0,   // ident [: base] [@[...]] := value;
   ANVL_STMT_OBJECT_BLOCK, // ident [: base] [@[...]] { statements };
   ANVL_STMT_VARS,         // vars { ... }
   ANVL_STMT_USING,        // using "path";
} anvl_stmt_kind;

/* ---------------------------------------------------------------------- *
 * Arena node kind — selects allocation size/index for Source.new_node
 * ---------------------------------------------------------------------- */
typedef enum {
   ANVL_NODE_STATEMENT = 0,
   ANVL_NODE_VALUE,
} anvl_node_kind;

typedef struct anvl_statement_t {
   anvl_stmt_kind kind;
   anvl_slice span;  // full source span of the statement
   anvl_slice name;  // declared identifier; empty only for VARS/USING
   anvl_slice base;  // inheritance base; empty when absent
   anvl_value value; // ASSIGN only; NULL otherwise
   list body;        // OBJECT_BLOCK only; nested list of anvl_statement pointers
   list attributes;  // anvl_attribute pointers, or NULL
} anvl_statement_t;
typedef anvl_statement_t *anvl_statement;

/* ---------------------------------------------------------------------- *
 * Document structure
 * ---------------------------------------------------------------------- */
struct anvl_mod_doc_t {
   anvl_source source;               // source for the document
   module_context context;           // owning context, set on registration
   string filepath;                  // normalized path to the loaded module
   struct anvl_doc_header_t *header; // parsed header metadata
   farray body; // frozen array of anvl_statement pointers (this document's own top-level
                // statements only, in parse order); NULL until Source.finish_body runs at the
                // end of a parse. See notes/document-body-parse.md "Document body — accumulate
                // then freeze".
};

/* ---------------------------------------------------------------------- *
 * Internal module function prototypes
 * ---------------------------------------------------------------------- */
#if 1 // module functions
/**
 * @brief Initialize a module context.
 * @param[out] out_mod Pointer to the module object to initialize.
 * @param[out] out_err_code Pointer to the error code if initialization fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 */
anvl_result mod_initialize(AnvlMod *, anvl_err_code *);
/**
 * @brief Initialize a minimal module object with default values and `NULL` context.
 * @param[out] out_mod Pointer to the module object to initialize.
 * @param[out] out_err_code Pointer to the error code if initialization fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function allocates a new module object and initializes its members to default
 * values. The context is set to `NULL` and must be initialized separately using
 * `mod_attach_context()`. If allocation fails, an error code is returned.
 */
anvl_result mod_new(AnvlMod *, anvl_err_code *);
/**
 * @brief Dispose of a module, releasing all associated resources.
 * @param mod Pointer to the module to dispose.
 */
void mod_dispose(AnvlMod *);
/**
 * @brief Attach a context to a module.
 * @param mod Pointer to the module to attach the context to.
 * @param ctx The context to attach.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function attaches a context to the specified module. If the module already has a
 * context, it will be replaced. The caller is responsible for managing the lifetime of the context
 * and ensuring it is properly disposed of when no longer needed.
 */
anvl_result mod_attach_context(AnvlMod *, module_context);
#endif

/* ---------------------------------------------------------------------- *
 * Internal context function prototypes
 * ---------------------------------------------------------------------- */
#if 1 // module_context functions
/**
 * @brief Resolve the context specification, returning the default if NULL is provided.
 * @param[out] spec_ptr Pointer to the context specification to resolve.
 * @param[out] out_err_code Pointer to the error code if resolution fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function resolves the context specification for module initialization. If the
 * provided spec_ptr is NULL (no variable provided), it returns an error state. Otherwise, it
 * returns the default context specification. If a valid spec_ptr is provided, it updates the base
 * context specification with any non-zero values from the provided spec.
 */
anvl_result resolve_context_spec(context_spec *, anvl_err_code *);
/**
 * @brief Initialize a module context with the default context spec.
 * @param[out] out_ctx Pointer to the module context to initialize.
 * @param[out] out_err_code Pointer to the error code if initialization fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function initializes a module context based on the default context spec.
 */
anvl_result mod_ctx_initialize(context_spec, module_context *, anvl_err_code *);
/**
 * @brief Dispose of a module context, releasing all associated resources.
 * @param ctx The module context to dispose.
 */
void mod_ctx_dispose(module_context);
/**
 * @brief Register a document with the module context.
 * @param ctx The module context to register the document with.
 * @param doc The document to register.
 * @param namespace The namespace key for the document.
 * @param filepath The normalized path to the document file.
 * @param[out] out_id Pointer to the document identity to populate.
 * @param[out] out_err_code Pointer to the error code if registration fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function registers a document with the specified module context. It appends the
 * document to the context's document list, creates a document identity, resolves the namespace key
 */
anvl_result mod_ctx_register_doc(module_context, module_document, const char *filepath,
                                 anvl_err_code *);
/**
 * @brief Set the parser for the module context.
 * @param ctx The module context to set the parser for.
 * @param parser The parser to set for the context.
 * @details This function sets the parser for the specified module context. It ensures that both
 * the context and parser are valid before setting the parser. If either is NULL, the function
 * returns without making any changes.
 */
void mod_ctx_set_parser(module_context, anvl_parser);
/**
 * @brief Create the context's shared body-parse arena, sized to hold every document's output.
 * @param ctx The module context to create the arena on.
 * @param size Initial arena capacity in bytes (e.g. the sum of Source.length() across ctx->docs).
 * @param[out] out_err_code Pointer to the error code if creation fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details Call once, after mod_load_imports has fully expanded the import graph (so the size can
 * be computed from the complete document set) and before any doc_parse_body call. Not yet
 * implemented — stub for RED-state testing; see notes/document-body-parse.md.
 */
anvl_result mod_ctx_create_arena(module_context ctx, usize size, anvl_err_code *out_err_code);
/**
 * @brief Apply the working arena-sizing heuristic to a summed source length.
 * @param summed_source_length Sum of `Source.length()` across every document in the import graph —
 * typically `mod_load_imports`'s `out_body_size_hint`.
 * @return `max(summed_source_length * ANVL_ARENA_SIZE_MULTIPLIER, ANVL_ARENA_MIN_SIZE)`.
 * @details Pure arithmetic, no failure mode. See notes/document-body-parse.md "Initial sizing
 * heuristic" and the constants themselves (`include/constants.h`) for the reasoning behind the
 * multiplier and floor. The result is meant as the `size` argument to `mod_ctx_create_arena`. Not
 * yet implemented — stub for RED-state testing.
 */
usize mod_ctx_arena_size_hint(usize summed_source_length);
/**
 * @brief Resolve '$identifier' VarRefs and validate 'base' targets across every document in
 * the context.
 * @param ctx The module context to resolve. Every document in `ctx->docs` must already have
 * been through a successful `doc_parse_body` — Resolution is phase 4, run once, after every
 * body in the import graph has parsed; document order does not matter.
 * @param[out] out_err_code Pointer to the error code if resolution fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details Builds `ctx->identifiers` (every document's top-level statements, by name) first;
 * a duplicate top-level name anywhere in the context is a hard error
 * (`ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER`) and stops resolution immediately. Then, for every
 * statement in the context (any nesting depth) with a non-empty `base`: a target missing from
 * `ctx->identifiers` is `ANVL_ERR_RESOLVER_MISSING_BASE`; a target that names an anonymous
 * (`ANVL_STMT_OBJECT_BLOCK`) statement is `ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS` (anonymous
 * objects are immutable and cannot be inherited from). Finally, for every `ANVL_VALUE_VARREF`
 * in the context (any nesting depth), chases its target to a final concrete value, setting
 * `.varref.resolved` — a missing target, a reference cycle, or a target naming an anonymous
 * statement are not errors, just leave `.resolved` `NULL`. See notes/resolution-phase.md for
 * the full design.
 */
anvl_result mod_resolve_context(module_context ctx, anvl_err_code *out_err_code);
/**
 * @brief Clear all documents from the module context.
 * @param docs List of documents to clear.
 */
void mod_ctx_clear_docs(list);
/**
 * @brief Clear all errors from the module context.
 * @param errors List of errors to clear.
 */
void mod_ctx_clear_errs(list);
#endif

/* ---------------------------------------------------------------------- *
 * Internal document functions
 * ---------------------------------------------------------------------- */
#if 1 // module_document functions
/**
 * @brief Initialize a module document.
 * @param[out] out_doc Pointer to the module document to initialize.
 * @param[out] out_err_code Pointer to the error code if initialization fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function allocates and initializes a new module document. It sets the source to
 * `NULL` and prepares the document for further operations. If allocation fails, an error code is
 * returned.
 */
anvl_result doc_initialize(module_document *, anvl_err_code *);
/**
 * @brief Dispose of a module document, releasing all associated resources.
 * @param doc The module document to dispose.
 * @details This function disposes of the specified module document, releasing its source and any
 * other associated resources. After calling this function, the document should not be used.
 */
void doc_dispose(module_document);
/**
 * @brief Load the source for a document.
 * @param doc The document to load the source for.
 * @param origin The origin of the source: `ANVL_SOURCE_FROM_FILE` or `ANVL_SOURCE_FROM_BUFFER`.
 * @param source The source data: file path or buffer.
 * @param len The length of the source data.
 * @param out_err_code Pointer to the error code if loading fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function loads the source for the specified document. It initializes the source and
 * attaches it to the document. If loading fails, an error code is returned.
 */
anvl_result doc_load_source(module_document, anvl_source_origin, const char *, size_t,
                            anvl_err_code *);
/**
 * @brief Scan the document header for shebang, imports, and module attributes.
 * @param doc The document to scan. Must have a loaded source and be registered
 *   in the global source registry.
 * @param[out] out_err_code Pointer to the error code if scanning fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise `ANVL_RES_ERR`.
 * @details This function scans the leading header constructs and leaves the
 * source position at the first body statement. Errors are recorded via
 * `Source.set_error()`.
 */
anvl_result doc_scan_header(module_document, anvl_err_code *);

/**
 * @brief Recursively expand the import graph starting from a root document.
 * @param ctx The module context that owns the root document and will own all imported documents.
 * @param root The document whose header imports should be expanded.
 * @param[out] out_body_size_hint Optional; if non-NULL, receives the sum of `Source.length()`
 * across root and every newly-registered imported document (diamonds counted once). Pass NULL
 * if the caller doesn't need it. Intended as the `size` input to `mod_ctx_create_arena` — see
 * notes/document-body-parse.md "Arena-backed allocation".
 * @param[out] out_err_code Pointer to the error code if expansion fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function resolves each import path in `root->header->imports` relative to
 * `root->filepath`, loads the referenced file as a new document, registers it with `ctx`,
 * scans its header, and recursively expands its imports. Each `anvl_doc_import_t` has its
 * `resolved` field set to the child document. Cyclic imports and missing files are reported
 * as errors on the requesting document.
 */
anvl_result mod_load_imports(module_context ctx, module_document root, usize *out_body_size_hint,
                             anvl_err_code *out_err_code);

/**
 * @brief Parse the document body into a list of statements.
 * @param doc The document whose header has been scanned and whose imports have been loaded.
 * @param[out] out_err_code Pointer to the error code if parsing fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details See `notes/document-body-parse.md` for the full grammar. Not yet implemented — the
 * current body is a stub that always fails; see that document's TDD status.
 */
anvl_result doc_parse_body(module_document doc, anvl_err_code *out_err_code);

/**
 * @brief Unload the source from a document.
 * @param doc The document to unload the source from.
 * @details This function detaches and disposes of the source associated with the specified
 * document. After calling this function, the document's source will be set to `NULL`.
 */
void doc_unload_source(module_document);
bool doc_has_errors(module_document);
bool doc_set_error(module_document, anvl_err_code, usize, usize, const char *);
/**
 * @brief Scan the document header for shebang, imports, and module attributes.
 * @param doc The document whose source has been loaded and registered with a context.
 * @param[out] out_err_code Pointer to the error code if scanning fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise `ANVL_RES_ERR`.
 * @details This function scans the leading header constructs of the document source and populates
 * `doc->header` with import and attribute metadata. It stops at the first body statement and
 * leaves `doc->source->pos` at the first body character. Errors are reported via
 * `Source.set_error` and `out_err_code`.
 */
anvl_result doc_scan_header(module_document, anvl_err_code *);
#endif

/* ---------------------------------------------------------------------- *
 * Internal root functions - deprecated
 * ---------------------------------------------------------------------- */
anvl_result root_get_doc(module_context, usize, module_document *);