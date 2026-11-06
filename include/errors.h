/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium,    *
 * is strictly prohibited without express written permission from the      *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * errors.h - Error handling for Anvil                                     *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-13                                                     *
 * File: include/errors.h                                                  *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * Error codes and state management for Anvil parser and operations        *
 * *********************************************************************** */
#pragma once

#include "anvil_error_codes.h"
#include "types.h"
// -----------------------------------------------------------------
#include <sigma/types.h>
#include <sigma/list.h>
#include <stdbool.h>

/**
 * @brief Anvil error codes
 */
typedef enum {
   // Vars Errors (41xx)
   ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED = 4101,
   ANVL_ERR_VARS_NOT_FIRST = 4102,
   ANVL_ERR_VARS_DUPLICATE_KEY = 4103,
   ANVL_ERR_VARS_CIRCULAR_REF = 4104,
   ANVL_ERR_VARS_KEY_NOT_FOUND = 4105,

   // Include Errors (42xx)
   ANVL_ERR_INCLUDE_NOT_FIRST = 4201,
   ANVL_ERR_INCLUDE_AMP_FORBIDDEN = 4202,
   ANVL_ERR_INCLUDE_DUPLICATE_ALIAS = 4203,
   ANVL_ERR_INCLUDE_FILE_NOT_FOUND = 4204,
   ANVL_ERR_INCLUDE_CYCLIC = 4206,

   // Import Errors (43xx) -- reserved ahead of AnvlScript's own foreign-code-bridge feature
   // (not yet implemented), same status the AML `include` mechanism's codes had before it
   // shipped. Formerly reserved under "Using Errors" for the same not-yet-built construct,
   // renamed alongside the keyword itself -- see FR-2609-anvl-public-api-002.
   ANVL_ERR_IMPORT_MODULE_NOT_FOUND = 4305,
   ANVL_ERR_IMPORT_IN_AMP = 4306,
   ANVL_ERR_IMPORT_AFTER_STATEMENTS = 4307,

   // AMP Dialect Errors (44xx)
   ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR = 4401,

   // Serializer Errors (45xx)
   ANVL_ERR_SERIALIZER_AMP_OBJECT_NOT_ALLOWED = 4501,

   // Schema Errors (46xx)
   ANVL_ERR_SCHEMA_ATTR_MISSING = 4601,    // document lacks the required @[schema] module attribute
   ANVL_ERR_SCHEMA_TYPE_UNRESOLVED = 4602, // schema field references an undefined type name
   ANVL_ERR_SCHEMA_BASE_UNKNOWN =
      4603, // schema statement base is not 'enum', 'flags', or a known type
   ANVL_ERR_SCHEMA_VALIDATION_REQUIRED = 4604, // required field absent in data document
   ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH =
      4605, // data field value type does not match schema field type
   ANVL_ERR_SCHEMA_VALIDATION_UNKNOWN_FIELD =
      4606, // data field not declared in schema type (strict mode)

   // AnvlScript Function Errors (47xx)
   ANVL_ERR_ASL_DUPLICATE_PARAMETER = 4701, // two parameters in the same function declaration
                                             // share a name -- notes/anvilscript-design.md's
                                             // "the ANVL parser... validates parameter
                                             // uniqueness"
   ANVL_ERR_ASL_VARARG_NOT_LAST = 4702, // a '[]' parameter-array marker appeared on a
                                         // parameter that isn't the last one -- "a final
                                         // parameter may be marked as a parameter-array",
                                         // notes/anvilscript-design.md
   ANVL_ERR_ASL_UNTERMINATED_BODY = 4703, // source ran out before the function body's
                                           // matching '}' was found

   // Lexer/Scanner Errors (50xx)
   ANVL_ERR_PARSER_UNEXPECTED_CHAR = 5001,
   ANVL_ERR_PARSER_UNTERMINATED_STRING = 5002,
   ANVL_ERR_PARSER_UNTERMINATED_BLOB = 5003,
   ANVL_ERR_PARSER_IO_ERROR = 5004,
   ANVL_ERR_PARSER_EXPECTED_BACKTICK = 5005,
   ANVL_ERR_PARSER_UNTERMINATED_FREEFORM = 5006,
   ANVL_ERR_PARSER_INVALID_HEX_LITERAL = 5007,
   ANVL_ERR_PARSER_INVALID_EXPONENT = 5008,
   ANVL_ERR_PARSER_INVALID_NUMBER = 5009,
   // '$' not immediately followed by a syntactically well-formed reference (identifier-shaped
   // target name) — missing entirely, or whitespace between '$' and it. Purely syntactic: the
   // parser never checks whether the reference's target actually exists as a declared
   // identifier (an unmatched reference is a Resolver-phase concern, resolved to `null`, not a
   // parse error) — "identifier" here would wrongly suggest that check happens at parse time.
   ANVL_ERR_VARS_INVALID_VARREF = 5010,
   ANVL_ERR_VARS_UNTERMINATED_INTERP = 5011,        // $"…{ref" missing closing '}'
   ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF = 5012, // '${' missing closing '}'

   // ASL Errors (51xx)
   ANVL_ERR_ASL_PARSE_ERROR = 5101,           // ASL script parse failure
   ANVL_ERR_ASL_RUNTIME_ERROR = 5102,         // ASL runtime failure
   ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED = 5103,   // call depth limit exceeded
   ANVL_ERR_ASL_BREAK_OUTSIDE_LOOP = 5104,    // break outside loop
   ANVL_ERR_ASL_CONTINUE_OUTSIDE_LOOP = 5105, // continue outside loop
   ANVL_ERR_ASL_NESTING_TOO_DEEP = 5106,      // expression nesting past ASL_MAX_NESTING_DEPTH (parse time)

   // Memory Errors (70xx)
   ANVL_ERR_MEMORY_ALLOC_FAILED = 7001,
   ANVL_ERR_ARENA_NOT_INITIALIZED = 7002,

   // Generic Errors (90xx)
   ANVL_ERR_INVALID_OPERATION = 9001,
   ANVL_ERR_INVALID_ARGUMENT = 9002,
   ANVL_ERR_SOURCE_NOT_FOUND = 9003,
   ANVL_ERR_CONTEXT_INVALID = 9004,
   ANVL_ERR_SOURCE_ALREADY_LOADED = 9005, // a document with identical content is already loaded
   ANVL_ERR_NOT_IMPLEMENTED = 9999,
} anvl_err_code;

/* -----------------------------------------------------------------
 * Error State Structure
 * ----------------------------------------------------------------- */
typedef struct anvl_err_state_t {
   anvl_err_code code;
   const char *message;
   usize line;
   usize column;
   const char *file; // owned copy of the path of the document the error is in; NULL when unknown
   char *detail; // owned: set when the message is built for this one error (e.g. it names an
                 // identifier); `message` then points at it. NULL for a table message.
} anvl_err_state_t;

/* ----------------------------------------------------------------- */
/* Error State Management                                            */
/* ----------------------------------------------------------------- */
anvl_result anvl_error_set(list, anvl_err_code, usize, usize, const char *, anvl_err_code *);
/// Like anvl_error_set, but with a message built for this error (copied), used instead of the
/// code's table message. First-error-wins applies the same way.
anvl_result anvl_error_set_message(list, anvl_err_code, usize, usize, const char *, const char *,
                                   anvl_err_code *);
/// Frees an error and the message it owns.
void anvl_error_dispose(anvl_error);
void anvl_error_clear(list);
bool anvl_error_is_set(list);
anvl_error anvl_error_get(list, usize);

/* ----------------------------------------------------------------- */
/* Error Code Utilities                                              */
/* ----------------------------------------------------------------- */
const char *anvl_error_code_message(anvl_err_code);
const char *anvl_error_code_name(anvl_err_code);