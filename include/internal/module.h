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
   list docs;          // list of module_document
   list errors;        // list of anvl_error encountered during scanning/parsing
   anvl_parser parser; // parser context for the module
};

typedef enum {
   ANVL_SOURCE_FROM_FILE = 0,
   ANVL_SOURCE_FROM_BUFFER,
} anvl_source_origin;

/* ---------------------------------------------------------------------- *
 * Source slice metadata — no-copy references into anvl_source buffer
 * ---------------------------------------------------------------------- */
typedef struct anvl_src_slice_t {
   char *data;  // pointer to the start of the source buffer (raw bytes)
   char *start; // pointer to the start of the slice within the source buffer
   char *end;   // pointer to the end of the slice within the source buffer
} anvl_slice;
typedef anvl_slice *slice;

/* ---------------------------------------------------------------------- *
 * Header import / attribute metadata
 * ---------------------------------------------------------------------- */
typedef struct anvl_doc_import_t {
   anvl_slice decl;          // "import \"path\"" (no trailing ';')
   anvl_slice path;          // "\"path\"" (quotes included)
   module_document resolved; // child document after import graph expansion; NULL until loaded
} anvl_import;
typedef anvl_import *anvl_doc_import;

typedef struct anvl_doc_attribute_t {
   anvl_slice key;   // identifier
   anvl_slice value; // value text; length 0 means flag attribute
} anvl_doc_attribute_t;
typedef anvl_doc_attribute_t *anvl_doc_attribute;

/* ---------------------------------------------------------------------- *
 * Document header — collected before body parsing
 * ---------------------------------------------------------------------- */
struct anvl_doc_header_t {
   list imports;    // list of anvl_doc_import
   list attributes; // list of anvl_doc_attribute
};

/* ---------------------------------------------------------------------- *
 * Body value tree — see notes/document-body-parse.md
 * ---------------------------------------------------------------------- */
typedef enum {
   ANVL_VALUE_NONE = 0,
   ANVL_VALUE_NULL,
   ANVL_VALUE_BOOL,
   ANVL_VALUE_INTEGER,
   ANVL_VALUE_FLOAT,
   ANVL_VALUE_STRING,
   ANVL_VALUE_BLOB,       // standard scalar; tag stored separately, content in text span
   ANVL_VALUE_ARRAY,
   ANVL_VALUE_TUPLE,
   ANVL_VALUE_OBJECT,     // nested statement list, not key/value pairs
   ANVL_VALUE_IDENTIFIER, // bare symbol: static reference to another statement's value
} anvl_value_type;

typedef struct anvl_doc_value_t {
   anvl_value_type type;
   anvl_slice text; // full source span of the value
   anvl_slice tag;  // blob tag (e.g. @date, @sel); empty for non-blobs or untagged blobs
   union {
      struct {
         list items; // array/tuple: list of anvl_doc_value pointers
      } collection;
      struct {
         list statements; // object: list of anvl_doc_statement pointers
      } object;
   };
} anvl_doc_value_t;
typedef anvl_doc_value_t *anvl_doc_value;

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

typedef struct anvl_doc_statement_t {
   anvl_stmt_kind kind;
   anvl_slice span;      // full source span of the statement
   anvl_slice name;      // declared identifier; empty only for VARS/USING
   anvl_slice base;      // inheritance base; empty when absent
   anvl_doc_value value; // ASSIGN only; NULL otherwise
   list body;            // OBJECT_BLOCK only; nested list of anvl_doc_statement pointers
   list attributes;      // anvl_doc_attribute pointers, or NULL
} anvl_doc_statement_t;
typedef anvl_doc_statement_t *anvl_doc_statement;

/* ---------------------------------------------------------------------- *
 * Document structure
 * ---------------------------------------------------------------------- */
struct anvl_mod_doc_t {
   anvl_source source;               // source for the document
   module_context context;           // owning context, set on registration
   string filepath;                  // normalized path to the loaded module
   struct anvl_doc_header_t *header; // parsed header metadata
   list body;                        // list of anvl_doc_statement pointers; NULL until doc_parse_body runs
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
void mod_ctx_set_parser(module_context, anvl_parser);
void mod_ctx_add_doc(module_context, module_document);
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
 * @param[out] out_err_code Pointer to the error code if expansion fails.
 * @return Anvl result: `ANVL_RES_OK` on success; otherwise, `ANVL_RES_ERR`.
 * @details This function resolves each import path in `root->header->imports` relative to
 * `root->filepath`, loads the referenced file as a new document, registers it with `ctx`,
 * scans its header, and recursively expands its imports. Each `anvl_doc_import_t` has its
 * `resolved` field set to the child document. Cyclic imports and missing files are reported
 * as errors on the requesting document.
 */
anvl_result mod_load_imports(module_context ctx, module_document root, anvl_err_code *out_err_code);

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