/* *********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium,    *
 * is strictly prohibited without express written permission from the      *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * internal/source.h - Source API for Anvil                                *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-13                                                     *
 * File: include/internal/source.h                                         *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * This header defines the source API for Anvil.                           *
 * *********************************************************************** */
#pragma once

#include "anvil.h"
#include "std.h"
#include "internal/module.h"
// -----------------------------------------------------------------
#include <sigma/list.h>
#include <sigma/types.h>
#include <sigma/memory.h>
#include <sigma/strings.h>

/* ----------------------------------------------------------------- *
 * Source structure                                                        *
 * ----------------------------------------------------------------- */
struct anvl_source_t {
   // anvl_err_code err_code; (why would we have this here ... ???)

   // farray-compatible buffer structure
   struct {
      void *bucket; // pointer to first element (raw bytes)
      void *end;    // one past allocated memory
   } buffer;

   // metadata for source content
   usize length; // length of the buffer in bytes
   anvl_dialect dialect;
   usize stride; // size of each element in bytes (1 for byte arrays)
   usize pos;
   usize line;
   usize col;
   uint64_t hash;    // FNV-1a 64-bit content hash; 0 = no content loaded
   bool has_shebang; // true when the source begins with a #! dialect directive
};

/**
 * @name Source Interface
 * @brief Function pointers for source operations.
 * @details This structure defines the interface for source operations, including creation,
 * disposal, dialect detection, error handling, and position management. Each function pointer
 * corresponds to a specific operation that can be performed on an `anvl_source` object.
 */
typedef struct anvl_source_i {
   /**
    * @brief Create an empty source object.
    * @param[out] out_src Receives the created source object on success.
    * @param[out] out_err_code Receives ANVL_ERR_NONE on success, otherwise a failure code.
    * @return Anvil result indicating success or failure.
    */
   anvl_result (*create)(anvl_source *, anvl_err_code *);
   /**
    * @brief Create a source object from a file path.
    * @param[in] filepath Input source file path.
    * @param[out] out_src Receives the created source object on success.
    * @param[out] out_err_code Receives ANVL_ERR_NONE on success, otherwise a failure code.
    * @return Anvil result indicating success or failure.
    */
   anvl_result (*from_file)(anvl_source *, const char *, anvl_err_code *);
   /**
    * @brief Create a source object from a memory buffer.
    * @param[in] buffer Input source buffer.
    * @param[in] len Length of the input buffer.
    * @param[out] out_src Receives the created source object on success.
    * @param[out] out_err_code Receives ANVL_ERR_NONE on success, otherwise a failure code.
    * @return Anvil result indicating success or failure.
    */
   anvl_result (*from_buffer)(anvl_source *, const char *, size_t, anvl_err_code *);
   /**
    * @brief Dispose of a source object, releasing all associated resources.
    * @param src The source object to dispose.
    * @details This function disposes of the specified source object, releasing its buffer and any
    * other associated resources. After calling this function, the source object should not be used.
    */
   void (*dispose)(anvl_source);
   /**
    * @brief Get the source dialect (AMP, AML, ASL, or error) for the given source object.
    * @param src The source object to query.
    * @return The dialect of the source object.
    * @details This function retrieves the dialect of the specified source object. The dialect can
    * be one of the following: ANVL_DIALECT_AMP, ANVL_DIALECT_AML, ANVL_DIALECT_ASL, or
    * ANVL_DIALECT_ERROR.
    */
   anvl_dialect (*dialect)(anvl_source);
   /**
    * @brief Get the FNV-1a 64-bit content hash for the given source object.
    * @param src The source object to query.
    * @return The content hash, or 0 if no content has been loaded.
    */
   uint64_t (*hash)(anvl_source);

   // Error handling - per source object
   /**
    * @brief Check if the source object has any recorded errors.
    * @param src The source object to query.
    * @return true if the source object has errors; false otherwise.
    * @details This function checks if the specified source object has any recorded errors. It
    * returns true if there are errors, and false if there are none. If the source object is NULL,
    * it returns false.
    */
   bool (*has_errors)(anvl_source);
   /**
    * @brief Record a parser/scanner error against the source's owning document.
    * @param src The source object where the error occurred.
    * @param code The Anvil error code.
    * @param line The 1-based line number of the error.
    * @param column The 1-based column number of the error.
    * @param file The file path associated with the error (may be NULL).
    * @param[out] out_err_code Receives a failure code if the error cannot be recorded.
    * @return `ANVL_RES_OK` on success; otherwise `ANVL_RES_ERR`.
    * @details Looks up the source in the global registry via its content hash and
    * appends the error to the owning document's context error list.
    */
   anvl_result (*set_error)(anvl_source, anvl_err_code, usize, usize, const char *,
                            anvl_err_code *);
   /**
    * @brief Look up the shared body-parse arena for the source's owning module context.
    * @param src The source object whose owning context's arena is wanted.
    * @param[out] out_err_code Receives a failure code if the arena cannot be retrieved.
    * @return The context's `bump_allocator`, or NULL if the source is unregistered or the
    * context hasn't created its arena yet (see `mod_ctx_create_arena`).
    * @details Same "the parser only ever holds a `source` handle" pattern as `has_errors`/
    * `set_error`: looks the owning document up in the global registry via the source's content
    * hash, then returns `doc->context->arena`. Pure lookup — never creates the arena itself.
    */
   bump_allocator (*get_arena)(anvl_source, anvl_err_code *);
   /**
    * @brief Allocate a new arena-backed statement or value node and index it on the owning
    * module context, in one call.
    * @param src The source object whose owning context's arena the node is allocated from.
    * @param kind Which node type to allocate — selects the allocation size
    * (`sizeof(anvl_statement_t)` or `sizeof(anvl_value_t)`) and which index list (`ctx->statements`
    * or `ctx->values`) the returned pointer is appended to.
    * @param[out] out_err_code Receives a failure code if the node cannot be allocated.
    * @return A zeroed pointer to the new node, or NULL on failure (unregistered source, or the
    * context's arena hasn't been created yet via `mod_ctx_create_arena`).
    * @details Same registry-lookup pattern as `get_arena`/`set_error`. This is the parser's one
    * entry point for building statement/value nodes — it never calls `get_arena` directly for node
    * construction, so the arena allocation and the context's node index can never drift apart. See
    * notes/document-body-parse.md "Arena node iteration".
    */
   void *(*new_node)(anvl_source, anvl_node_kind, anvl_err_code *);
   /**
    * @brief Initialize a slice from a start/end pointer pair, filling in the buffer-base `data`
    * field correctly.
    * @param src The source the slice is cut from.
    * @param[out] out_slice Receives the constructed slice. Untouched if NULL.
    * @details Pure initialization, no failure mode, no registry lookup — unlike `get_arena`/
    * `new_node`/`set_error`, this depends only on `src` itself, not its owning document. Exists
    * to stop every slice-building call site from repeating `data`/`start`/`end` by hand, which
    * has already produced one real bug (`data` set equal to `start` instead of the buffer base —
    * see notes/document-body-parse.md "Source.new_slice — centralizing slice construction").
    */
   void (*init_slice)(anvl_source src, anvl_slice *out_slice);
   /**
    * @brief Freeze a parser's finished top-level statement list into the owning document's body.
    * @param src The source whose owning document's body is being finished.
    * @param statements A list of `anvl_statement` pointers, in parse order. Ownership transfers
    * to this call regardless of outcome — the caller must not use or dispose it afterward.
    * @param[out] out_err_code Receives a failure code if the source is unregistered.
    * @return `ANVL_RES_OK` on success; otherwise `ANVL_RES_ERR`.
    * @details Converts `statements` into a frozen `farray` (dense, no further growth — the right
    * shape for a body that's done being built, unlike the growable `list` used while accumulating
    * it) and assigns it to `doc->body`, then disposes `statements`. Intended to be called once, at
    * the end of `parse_source`, on both the success and failure exit paths — a document that fails
    * partway through still has every statement parsed before the failure point preserved in
    * `doc->body`, not silently dropped. See notes/document-body-parse.md "Document body —
    * accumulate then freeze".
    */
   anvl_result (*finish_body)(anvl_source src, list statements, anvl_err_code *out_err_code);

   // Position & EOF
   /**
    * @brief Get the current position in the source object.
    * @param src The source object to query.
    * @return The current position in the source object, or 0 if the source is NULL.
    */
   usize (*position)(anvl_source);
   /**
    * @brief Get the current line number in the source object.
    * @param src The source object to query.
    * @return The current line number in the source object, or 0 if the source is NULL.
    */
   usize (*line)(anvl_source);
   /**
    * @brief Get the current column number in the source object.
    * @param src The source object to query.
    * @return The current column number in the source object, or 0 if the source is NULL.
    */
   usize (*column)(anvl_source);
   /**
    * @brief Check if the source object has reached the end of its content.
    * @param src The source object to query.
    * @return true if the source object is at EOF; false otherwise.
    * @details This function checks if the specified source object has reached the end of its
    */
   bool (*is_eof)(anvl_source);
   /**
    * @brief Check if the source object has reached the end of its content with an offset.
    * @param src The source object to query.
    * @param offset The offset to check from the current position.
    * @return true if the source object is at EOF with the given offset; false otherwise.
    * @details This function checks if the specified source object has reached the end of its
    * content when considering the given offset from the current position.
    */
   bool (*is_eof_offset)(anvl_source, usize);

   // Character peek
   /**
    * @brief Peek at the character at the current position in the source object.
    * @param src The source object to query.
    * @return The character at the current position, or '\0' if the source is NULL or at EOF.
    * @details This function retrieves the character at the current position in the specified source
    * object without advancing the position. If the source is NULL or at EOF, it returns '\0'.
    */
   char (*peek)(anvl_source);
   /**
    * @brief Peek at the character at an offset from the current position in the source object.
    * @param src The source object to query.
    * @param offset The offset from the current position to peek at.
    * @return The character at the specified offset, or '\0' if the source is NULL or at EOF.
    * @details This function retrieves the character at the specified offset from the current
    * position in the source object without advancing the position. If the source is NULL or at EOF,
    * it returns '\0'.
    */
   char (*peek_offset)(anvl_source, usize);

   // String matching (returns length if matches, 0 if not)
   /**
    * @brief Match a string against the source content at the current position.
    * @param src The source object to query.
    * @param str The string to match against the source content.
    * @param len The length of the string to match.
    * @return The length of the matched string if it matches; otherwise, 0.
    * @details This function attempts to match the specified string against the source content at
    * the current position. If the string matches, it returns the length of the matched string;
    * otherwise, it returns 0.
    */
   usize (*match_length)(anvl_source, const char *, usize);
   /**
    * @brief Match an operator string against the source content at the current position.
    * @param src The source object to query.
    * @param op The operator string to match against the source content.
    * @param len The length of the operator string to match.
    * @return The length of the matched operator string if it matches; otherwise, 0.
    * @details This function attempts to match the specified operator string against the source
    * content at the current position. If the operator string matches, it returns the length of the
    * matched operator string; otherwise, it returns 0.
    */
   usize (*match_operator)(anvl_source, const char *, usize);

   // Character classification
   /**
    * @brief Check if a character is an alphabetic character (A-Z, a-z).
    * @param c The character to check.
    * @return true if the character is alphabetic; false otherwise.
    */
   bool (*is_alpha)(char);
   /**
    * @brief Check if a character is a digit (0-9).
    * @param c The character to check.
    * @return true if the character is a digit; false otherwise.
    */
   bool (*is_digit)(char);
   /**
    * @brief Check if a character is a hexadecimal digit (0-9, A-F, a-f).
    * @param c The character to check.
    * @return true if the character is a hexadecimal digit; false otherwise.
    */
   bool (*is_hex_digit)(char);
   /**
    * @brief Check if a character is a valid identifier start character (A-Z, a-z, _).
    * @param c The character to check.
    * @return true if the character is a valid identifier start character; false otherwise.
    */
   bool (*is_identifier_start)(char);
   /**
    * @brief Check if a character is a valid identifier part character (A-Z, a-z, 0-9, _).
    * @param c The character to check.
    * @return true if the character is a valid identifier part character; false otherwise.
    */
   bool (*is_identifier_part)(char);

   // Consume
   /**
    * @brief Consume a specified number of characters from the source object.
    * @param src The source object to consume from.
    * @param count The number of characters to consume.
    * @return The number of characters actually consumed.
    * @details This function advances the current position in the specified source object by the
    * specified count. It returns the number of characters actually consumed, which may be less than
    * the requested count if the end of the source is reached.
    */
   usize (*consume)(anvl_source, usize);

   // Data access (for scanning without consuming)
   /**
    * @brief Get a pointer to the source content buffer.
    * @param src The source object to query.
    * @return A pointer to the source content buffer, or NULL if the source is NULL.
    * @details This function retrieves a pointer to the raw content buffer of the specified source
    * object. The buffer contains the entire source content, and the caller should not modify it.
    */
   const char *(*data)(anvl_source);
   /**
    * @brief Get a pointer to the current position in the source content buffer.
    * @param src The source object to query.
    * @return A pointer to the current position in the source content buffer, or NULL if the source
    * is NULL.
    * @details This function is equivalent to Source.data(src) + Source.position(src).
    */
   const char *(*at)(anvl_source);
   /**
    * @brief Get the length of the source content buffer.
    * @param src The source object to query.
    * @return The length of the source content buffer in bytes, or 0 if the source is NULL.
    */
   usize (*length)(anvl_source);

   // Whitespace & comments
   /**
    * @brief Skip whitespace and comments in the source content.
    * @param src The source object to operate on.
    * @return The number of characters skipped.
    */
   usize (*skip_whitespace_and_comments)(anvl_source);

   // Shebang
   /**
    * @brief Check if the source content begins with a shebang (#!) directive.
    * @param src The source object to query.
    * @return true if the source content begins with a shebang; false otherwise.
    */
   bool (*is_shebang)(anvl_source);

   // Dialect parsing
   /**
    * @brief Parse the dialect from the source content, considering an optional shebang.
    * @param src The source object to operate on.
    * @param default_dialect The default dialect to use if no shebang is present.
    * @return The parsed dialect, or the default dialect if no shebang is present.
    * @details This function checks the source content for a shebang (#!) directive that
    * specifies the dialect. If a shebang is present, it parses the dialect from it. If no shebang
    * is present, it returns the provided default dialect. The function also updates the source's
    * dialect field accordingly.
    */
   anvl_dialect (*parse_dialect)(anvl_source, anvl_dialect);

   // Position management
   /**
    * @brief Set the current position, line, and column in the source object.
    * @param src The source object to operate on.
    * @param pos The new position to set.
    * @param line The new line number to set.
    * @param col The new column number to set.
    */
   void (*set_position)(anvl_source, usize, usize, usize);
   /**
    * @brief Reset the source object to its initial state, clearing any errors and resetting
    * position.
    * @param src The source object to reset.
    * @details This function resets the specified source object to its initial state. It clears any
    * recorded errors, resets the position to the beginning of the source content, and sets the
    * line and column numbers to 1. The source content remains unchanged.
    */
   void (*reset)(anvl_source);

   // Slice interrogation
   /**
    * @brief Get the length of a slice.
    * @param slice The slice to query.
    * @return The length of the slice in bytes, or 0 if the slice is invalid.
    */
   usize (*slice_length)(anvl_slice);
   /**
    * @brief Check if a slice is empty.
    * @param slice The slice to query.
    * @return `true` if the slice is empty; otherwise `false`.
    */
   bool (*slice_is_empty)(anvl_slice);
   /**
    * @brief Extract a substring from the source content into a caller-supplied buffer.
    * @param slice The slice representing the substring to extract.
    * @param out_buffer The caller-supplied buffer to receive the substring.
    * @return The length of the extracted substring, or 0 if the slice is invalid.
    * @details This function copies the content of the specified slice from the source content into
    * the provided output buffer. The caller is responsible for ensuring that the output buffer is
    * large enough to hold the substring. The slice should be a valid slice obtained from the
    * source content.
    */
   usize (*substring)(anvl_slice, char *);
} anvl_source_i;
/**
 * @defgroup Global Source Interface Instance
 * @brief Source interface instance provides access to the source interface functions for creating,
 * disposing, and manipulating source objects. It is used throughout the Anvil project to interact
 * with source content.
 * @{
 */
extern const anvl_source_i Source;
/** @} */
