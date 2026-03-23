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
 * errors.h - Error handling for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: include/errors.h
 * ------------------------------------------------------------------
 * Description:
 * Error codes and state management for Anvil parser and operations
 * ------------------------------------------------------------------
 */
#pragma once

#include <sigma.core/types.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Error Codes                                                        */
/* ------------------------------------------------------------------ */
typedef enum {
   ANVL_ERR_NONE = 0,

   // Builder/Initialization Errors (100x)
   ANVL_ERR_BUILDER_NO_SOURCE = 1001,

   // Parser Errors (200x-400x)
   // Top Level (200x)
   ANVL_ERR_PARSER_EXPECTED_IDENTIFIER = 2001,
   ANVL_ERR_PARSER_EXPECTED_ASSIGN = 2002,
   ANVL_ERR_PARSER_EXPECTED_VALUE = 2003,
   ANVL_ERR_PARSER_MULTIPLE_SHEBANG = 2004,
   ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS = 2005,
   ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE = 2006,
   ANVL_ERR_PARSER_INVALID_IDENTIFIER = 2007,
   ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK = 2008,

   // Object Parsing (300x)
   ANVL_ERR_PARSER_EXPECTED_OBJECT_FIELD = 3001,
   ANVL_ERR_PARSER_EXPECTED_OBJECT_VALUE = 3002,
   ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE = 3003,
   ANVL_ERR_PARSER_TRAILING_COMMA_IN_OBJECT = 3004,
   ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY = 3007,
   ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE = 3008,
   ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE = 3009,
   ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED = 3010,
   ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED = 3011,
   ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES = 3012,
   ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES = 3013,
   ANVL_ERR_PARSER_EXPECTED_COLON = 3014,
   ANVL_ERR_PARSER_UNEXPECTED_TOKEN = 3100,

   // Semantic Errors (400x)
   ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT = 4001,
   ANVL_ERR_PARSER_INVALID_KEY_IN_OBJECT = 4002,
   ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD = 4003,
   ANVL_ERR_PARSER_ATTRIBUTE_IS_KEYWORD = 4004,
   ANVL_ERR_PARSER_TUPLE_TOO_SHORT = 4005,
   ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE = 4006,
   ANVL_ERR_PARSER_EMPTY_TUPLE_ELEMENT = 4007,
   ANVL_ERR_PARSER_ROCKET_OP_NOT_VALID = 4008,
   ANVL_ERR_PARSER_ARRAY_CANNOT_BE_EMPTY = 4009,
   ANVL_ERR_PARSER_ASSIGNMENT_NOT_ALLOWED_HERE = 4010,
   ANVL_ERR_PARSER_INVALID_ATTRIBUTE_BLOCK = 4011,
   ANVL_ERR_PARSER_INVALID_ATTRIBUTE = 4012,
   ANVL_ERR_PARSER_DUPLICATE_ATTRIBUTE_KEY = 4013,
   ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE = 4014,
   ANVL_ERR_ANON_BLOCK_REDECLARATION = 4015,

   // Resolver Errors (405x)
   ANVL_ERR_RESOLVER_CYCLE_DETECTED = 4050,
   ANVL_ERR_RESOLVER_MISSING_BASE = 4051,

   // Vars Errors (410x)
   ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED = 4101,
   ANVL_ERR_VARS_NOT_FIRST = 4102,
   ANVL_ERR_VARS_DUPLICATE_KEY = 4103,
   ANVL_ERR_VARS_CIRCULAR_REF = 4104,
   ANVL_ERR_VARS_KEY_NOT_FOUND = 4105,

   // Import Errors (420x)
   ANVL_ERR_IMPORT_NOT_FIRST = 4201,
   ANVL_ERR_IMPORT_AMP_FORBIDDEN = 4202,
   ANVL_ERR_IMPORT_DUPLICATE_ALIAS = 4203,
   ANVL_ERR_IMPORT_FILE_NOT_FOUND = 4204,
   ANVL_ERR_IMPORT_CYCLIC = 4206,

   // Using Errors (430x)
   ANVL_ERR_USING_MODULE_NOT_FOUND = 4305,
   ANVL_ERR_USING_IN_AMP = 4306,
   ANVL_ERR_USING_AFTER_STATEMENTS = 4307,

   // AMP Dialect Errors (440x)
   ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR = 4401,

   // Serializer Errors (450x)
   ANVL_ERR_SERIALIZER_AMP_OBJECT_NOT_ALLOWED = 4501,

   // Schema Errors (460x)
   ANVL_ERR_SCHEMA_ATTR_MISSING = 4601,             // document lacks the required @[schema] module attribute
   ANVL_ERR_SCHEMA_TYPE_UNRESOLVED = 4602,          // schema field references an undefined type name
   ANVL_ERR_SCHEMA_BASE_UNKNOWN = 4603,             // schema statement base is not 'enum', 'flags', or a known type
   ANVL_ERR_SCHEMA_VALIDATION_REQUIRED = 4604,      // required field absent in data document
   ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH = 4605, // data field value type does not match schema field type
   ANVL_ERR_SCHEMA_VALIDATION_UNKNOWN_FIELD = 4606, // data field not declared in schema type (strict mode)

   // Lexer/Scanner Errors (500x)
   ANVL_ERR_PARSER_UNEXPECTED_CHAR = 5001,
   ANVL_ERR_PARSER_UNTERMINATED_STRING = 5002,
   ANVL_ERR_PARSER_UNTERMINATED_BLOB = 5003,
   ANVL_ERR_PARSER_IO_ERROR = 5004,
   ANVL_ERR_PARSER_EXPECTED_BACKTICK = 5005,
   ANVL_ERR_PARSER_UNTERMINATED_FREEFORM = 5006,
   ANVL_ERR_PARSER_INVALID_HEX_LITERAL = 5007,
   ANVL_ERR_PARSER_INVALID_EXPONENT = 5008,
   ANVL_ERR_PARSER_INVALID_NUMBER = 5009,
   ANVL_ERR_VARS_INVALID_VARREF = 5010,           // '$' not followed by valid identifier
   ANVL_ERR_VARS_UNTERMINATED_INTERP = 5011,        // $"…{ref" missing closing '}'
   ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF = 5012, // '${' missing closing '}'

   // ASL Errors (510x)
   ANVL_ERR_ASL_PARSE_ERROR = 5101,           // ASL script parse failure
   ANVL_ERR_ASL_RUNTIME_ERROR = 5102,         // ASL runtime failure
   ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED = 5103,   // call depth limit exceeded
   ANVL_ERR_ASL_BREAK_OUTSIDE_LOOP = 5104,    // break outside loop
   ANVL_ERR_ASL_CONTINUE_OUTSIDE_LOOP = 5105, // continue outside loop

   // I/O Errors (600x)
   ANVL_ERR_IO_FILE_NOT_FOUND = 6001,
   ANVL_ERR_IO_READ_FAILED = 6002,

   // Memory Errors (700x)
   ANVL_ERR_MEMORY_ALLOCATION_FAILED = 7001,

   // Generic Errors (900x)
   ANVL_ERR_INVALID_OPERATION = 9001,
   ANVL_ERR_INVALID_ARGUMENT = 9002,
} anvl_error_code;

/* ------------------------------------------------------------------ */
/* Error State Structure                                             */
/* ------------------------------------------------------------------ */
typedef struct {
   anvl_error_code code;
   const char *message;
   usize line;
   usize column;
   const char *file;
} anvl_error_state;

/* ------------------------------------------------------------------ */
/* Error State Management                                            */
/* ------------------------------------------------------------------ */
void anvl_error_set(anvl_error_code code, const char *message, usize line, usize column, const char *file);
void anvl_error_clear(void);
bool anvl_error_is_set(void);
const anvl_error_state *anvl_error_get(void);

/* ------------------------------------------------------------------ */
/* Error Code Utilities                                              */
/* ------------------------------------------------------------------ */
const char *anvl_error_code_message(anvl_error_code code);
const char *anvl_error_code_name(anvl_error_code code);