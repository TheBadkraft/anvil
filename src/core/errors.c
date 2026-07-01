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
 * errors.c - Error handling implementation for Anvil
 * ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-14
 * File: src/core/errors.c
 * ------------------------------------------------------------------
 */

#include "errors.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Global Error State                                                 */
/* ------------------------------------------------------------------ */
static anvl_error_state global_error_state = {
    .code = ANVL_ERR_NONE,
    .message = NULL,
    .line = 0,
    .column = 0,
    .file = NULL};

/* ------------------------------------------------------------------ */
/* Error Code Messages                                               */
/* ------------------------------------------------------------------ */
static const char *error_messages[] = {
    [ANVL_ERR_NONE] = "No error",
    [ANVL_ERR_BUILDER_NO_SOURCE] = "Builder has no source to build context from",

    // Parser Errors (200x-400x)
    [ANVL_ERR_PARSER_EXPECTED_IDENTIFIER] = "Expected identifier",
    [ANVL_ERR_PARSER_EXPECTED_ASSIGN] = "Expected ':=' after identifier",
    [ANVL_ERR_PARSER_EXPECTED_VALUE] = "Expected content",
    [ANVL_ERR_PARSER_MULTIPLE_SHEBANG] = "Multiple shebangs not allowed",
    [ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS] = "Shebang must be first non-whitespace line",
    [ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE] = "Attribute content cannot be object, array, tuple, or blob",
    [ANVL_ERR_PARSER_INVALID_IDENTIFIER] = "Invalid identifier format",
    [ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK] = "Attribute blocks cannot be empty",

    [ANVL_ERR_PARSER_EXPECTED_OBJECT_FIELD] = "Expected object key",
    [ANVL_ERR_PARSER_EXPECTED_OBJECT_VALUE] = "Expected object content after ':='",
    [ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE] = "Expected '}' to close object",
    [ANVL_ERR_PARSER_TRAILING_COMMA_IN_OBJECT] = "Trailing comma in object",
    [ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY] = "Missing ',' in array",
    [ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE] = "Expected ']' to close array",
    [ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE] = "Expected ')' to close tuple content",
    [ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED] = "Empty objects are not allowed",
    [ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED] = "Empty arrays are not allowed",
    [ANVL_ERR_PARSER_EMPTY_TUPLE_NOT_ALLOWED] = "Empty tuples are not allowed",
    [ANVL_ERR_PARSER_TUPLE_TOO_FEW_ELEMENTS] = "Tuples must have at least 2 elements",
    [ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES] = "Missing ',' in attribute block",
    [ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES] = "Module attributes must be prior to statements",
    [ANVL_ERR_PARSER_UNEXPECTED_TOKEN] = "Unexpected token",

    [ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT] = "Duplicate identifier in document or object",
    [ANVL_ERR_PARSER_INVALID_KEY_IN_OBJECT] = "Invalid key as identifier",
    [ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD] = "Identifier cannot be a keyword",
    [ANVL_ERR_PARSER_ATTRIBUTE_IS_KEYWORD] = "Attribute name cannot be a keyword",
    [ANVL_ERR_PARSER_TUPLE_TOO_SHORT] = "Tuple requires minimum 2 values",
    [ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE] = "Missing ',' in tuple",
    [ANVL_ERR_PARSER_EMPTY_TUPLE_ELEMENT] = "Missing element in tuple",
    [ANVL_ERR_PARSER_ROCKET_OP_NOT_VALID] = "Rocket operator '=>' is not valid",
    [ANVL_ERR_PARSER_ARRAY_CANNOT_BE_EMPTY] = "Arrays cannot be empty",
    [ANVL_ERR_PARSER_ASSIGNMENT_NOT_ALLOWED_HERE] = "Assignment not valid here",
    [ANVL_ERR_PARSER_INVALID_ATTRIBUTE_BLOCK] = "Invalid attribute block; must be `@[...]`",
    [ANVL_ERR_PARSER_INVALID_ATTRIBUTE] = "The attribute identifier is invalid",
    [ANVL_ERR_PARSER_DUPLICATE_ATTRIBUTE_KEY] = "Duplicate attribute identifier",
    [ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE] = "Attributes are not allowed on scalar, array, or tuple types",
    [ANVL_ERR_ANON_BLOCK_REDECLARATION] = "Anonymous block name conflicts with an existing declaration",
    [ANVL_ERR_PARSER_UNTERMINATED_COMMENT] = "Unterminated block comment",

    [ANVL_ERR_RESOLVER_CYCLE_DETECTED] = "Cycle detected in inheritance chain",
    [ANVL_ERR_RESOLVER_MISSING_BASE] = "Base identifier not found in document",
    [ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS] = "Cannot inherit from anonymous object (use ':=' for inheritable declarations)",

    [ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED] = "vars block is already defined",
    [ANVL_ERR_VARS_NOT_FIRST] = "vars block must appear before all statements",
    [ANVL_ERR_VARS_DUPLICATE_KEY] = "Duplicate key in vars block",
    [ANVL_ERR_VARS_CIRCULAR_REF] = "Circular reference detected in vars block",
    [ANVL_ERR_VARS_KEY_NOT_FOUND] = "vars key not found",
   [ANVL_ERR_VARS_UNTERMINATED_INTERP] = "Unterminated interpolated string: missing closing quote or '}'",
    [ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF] = "Unterminated braced VarRef: '${' missing closing '}'",

    [ANVL_ERR_IMPORT_NOT_FIRST] = "import declarations must precede statements",
    [ANVL_ERR_IMPORT_AMP_FORBIDDEN] = "import is not allowed in AMP dialect",
    [ANVL_ERR_IMPORT_DUPLICATE_ALIAS] = "Duplicate import alias",
    [ANVL_ERR_IMPORT_FILE_NOT_FOUND] = "Imported file not found",
    [ANVL_ERR_IMPORT_CYCLIC] = "Cyclic import detected",

    [ANVL_ERR_USING_MODULE_NOT_FOUND] = "using: module not found",
    [ANVL_ERR_USING_IN_AMP] = "using is not allowed in AMP dialect",
    [ANVL_ERR_USING_AFTER_STATEMENTS] = "using declarations must precede statements",

    [ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR] = "AMP array/tuple elements must be scalar or blob values",

    [ANVL_ERR_SCHEMA_ATTR_MISSING] = "Schema document is missing the required @[schema] module attribute",
    [ANVL_ERR_SCHEMA_TYPE_UNRESOLVED] = "Schema field references a type name that is not defined in the schema",
    [ANVL_ERR_SCHEMA_BASE_UNKNOWN] = "Schema statement base is not a virtual base ('enum', 'flags') or a known schema type",
    [ANVL_ERR_SCHEMA_VALIDATION_REQUIRED] = "Required field is missing in the data document",
    [ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH] = "Data field value type does not match the declared schema field type",
    [ANVL_ERR_SCHEMA_VALIDATION_UNKNOWN_FIELD] = "Data document contains a field not declared in the schema type",

    [ANVL_ERR_PARSER_UNEXPECTED_CHAR] = "Unexpected character",
    [ANVL_ERR_PARSER_UNTERMINATED_STRING] = "Unterminated string literal",
    [ANVL_ERR_PARSER_UNTERMINATED_BLOB] = "Unterminated blob literal",
    [ANVL_ERR_PARSER_IO_ERROR] = "I/O error during file reading",
    [ANVL_ERR_PARSER_EXPECTED_BACKTICK] = "Expected freeform literal enclosure",
    [ANVL_ERR_PARSER_UNTERMINATED_FREEFORM] = "Unterminated freeform literal",
    [ANVL_ERR_PARSER_INVALID_HEX_LITERAL] = "Invalid hex number format",
    [ANVL_ERR_PARSER_INVALID_EXPONENT] = "Invalid exponent format",
    [ANVL_ERR_PARSER_INVALID_NUMBER] = "Invalid number format",

    [ANVL_ERR_ASL_PARSE_ERROR] = "ASL parse error",
    [ANVL_ERR_ASL_RUNTIME_ERROR] = "ASL runtime error",
    [ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED] = "ASL call depth exceeded",
    [ANVL_ERR_ASL_BREAK_OUTSIDE_LOOP] = "break outside loop",
    [ANVL_ERR_ASL_CONTINUE_OUTSIDE_LOOP] = "continue outside loop",

    [ANVL_ERR_IO_FILE_NOT_FOUND] = "File not found",
    [ANVL_ERR_IO_READ_FAILED] = "Failed to read file",
    [ANVL_ERR_MEMORY_ALLOCATION_FAILED] = "Memory allocation failed",
    [ANVL_ERR_INVALID_OPERATION] = "Invalid operation",
    [ANVL_ERR_INVALID_ARGUMENT] = "Invalid argument",
};

static const char *error_names[] = {
    [ANVL_ERR_NONE] = "ANVL_ERR_NONE",
    [ANVL_ERR_BUILDER_NO_SOURCE] = "ANVL_ERR_BUILDER_NO_SOURCE",

    [ANVL_ERR_PARSER_EXPECTED_IDENTIFIER] = "ANVL_ERR_PARSER_EXPECTED_IDENTIFIER",
    [ANVL_ERR_PARSER_EXPECTED_ASSIGN] = "ANVL_ERR_PARSER_EXPECTED_ASSIGN",
    [ANVL_ERR_PARSER_EXPECTED_VALUE] = "ANVL_ERR_PARSER_EXPECTED_VALUE",
    [ANVL_ERR_PARSER_MULTIPLE_SHEBANG] = "ANVL_ERR_PARSER_MULTIPLE_SHEBANG",
    [ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS] = "ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS",
    [ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE] = "ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE",
    [ANVL_ERR_PARSER_INVALID_IDENTIFIER] = "ANVL_ERR_PARSER_INVALID_IDENTIFIER",
    [ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK] = "ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK",

    [ANVL_ERR_PARSER_EXPECTED_OBJECT_FIELD] = "ANVL_ERR_PARSER_EXPECTED_OBJECT_FIELD",
    [ANVL_ERR_PARSER_EXPECTED_OBJECT_VALUE] = "ANVL_ERR_PARSER_EXPECTED_OBJECT_VALUE",
    [ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE] = "ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE",
    [ANVL_ERR_PARSER_TRAILING_COMMA_IN_OBJECT] = "ANVL_ERR_PARSER_TRAILING_COMMA_IN_OBJECT",
    [ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY] = "ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY",
    [ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE] = "ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE",
    [ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE] = "ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE",
    [ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED] = "ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED",
    [ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED] = "ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED",
    [ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES] = "ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES",
    [ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES] = "ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES",
    [ANVL_ERR_PARSER_UNEXPECTED_TOKEN] = "ANVL_ERR_PARSER_UNEXPECTED_TOKEN",

    [ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT] = "ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT",
    [ANVL_ERR_PARSER_INVALID_KEY_IN_OBJECT] = "ANVL_ERR_PARSER_INVALID_KEY_IN_OBJECT",
    [ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD] = "ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD",
    [ANVL_ERR_PARSER_ATTRIBUTE_IS_KEYWORD] = "ANVL_ERR_PARSER_ATTRIBUTE_IS_KEYWORD",
    [ANVL_ERR_PARSER_TUPLE_TOO_SHORT] = "ANVL_ERR_PARSER_TUPLE_TOO_SHORT",
    [ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE] = "ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE",
    [ANVL_ERR_PARSER_EMPTY_TUPLE_ELEMENT] = "ANVL_ERR_PARSER_EMPTY_TUPLE_ELEMENT",
    [ANVL_ERR_PARSER_ROCKET_OP_NOT_VALID] = "ANVL_ERR_PARSER_ROCKET_OP_NOT_VALID",
    [ANVL_ERR_PARSER_ARRAY_CANNOT_BE_EMPTY] = "ANVL_ERR_PARSER_ARRAY_CANNOT_BE_EMPTY",
    [ANVL_ERR_PARSER_ASSIGNMENT_NOT_ALLOWED_HERE] = "ANVL_ERR_PARSER_ASSIGNMENT_NOT_ALLOWED_HERE",
    [ANVL_ERR_PARSER_INVALID_ATTRIBUTE_BLOCK] = "ANVL_ERR_PARSER_INVALID_ATTRIBUTE_BLOCK",
    [ANVL_ERR_PARSER_INVALID_ATTRIBUTE] = "ANVL_ERR_PARSER_INVALID_ATTRIBUTE",
    [ANVL_ERR_PARSER_DUPLICATE_ATTRIBUTE_KEY] = "ANVL_ERR_PARSER_DUPLICATE_ATTRIBUTE_KEY",
    [ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE] = "ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE",
    [ANVL_ERR_ANON_BLOCK_REDECLARATION] = "ANVL_ERR_ANON_BLOCK_REDECLARATION",

    [ANVL_ERR_RESOLVER_CYCLE_DETECTED] = "ANVL_ERR_RESOLVER_CYCLE_DETECTED",
    [ANVL_ERR_RESOLVER_MISSING_BASE] = "ANVL_ERR_RESOLVER_MISSING_BASE",
    [ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS] = "ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS",

    [ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED] = "ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED",
    [ANVL_ERR_VARS_NOT_FIRST] = "ANVL_ERR_VARS_NOT_FIRST",
    [ANVL_ERR_VARS_DUPLICATE_KEY] = "ANVL_ERR_VARS_DUPLICATE_KEY",
    [ANVL_ERR_VARS_CIRCULAR_REF] = "ANVL_ERR_VARS_CIRCULAR_REF",
    [ANVL_ERR_VARS_KEY_NOT_FOUND] = "ANVL_ERR_VARS_KEY_NOT_FOUND",
   [ANVL_ERR_VARS_UNTERMINATED_INTERP] = "ANVL_ERR_VARS_UNTERMINATED_INTERP",
    [ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF] = "ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF",

    [ANVL_ERR_IMPORT_NOT_FIRST] = "ANVL_ERR_IMPORT_NOT_FIRST",
    [ANVL_ERR_IMPORT_AMP_FORBIDDEN] = "ANVL_ERR_IMPORT_AMP_FORBIDDEN",
    [ANVL_ERR_IMPORT_DUPLICATE_ALIAS] = "ANVL_ERR_IMPORT_DUPLICATE_ALIAS",
    [ANVL_ERR_IMPORT_FILE_NOT_FOUND] = "ANVL_ERR_IMPORT_FILE_NOT_FOUND",
    [ANVL_ERR_IMPORT_CYCLIC] = "ANVL_ERR_IMPORT_CYCLIC",

    [ANVL_ERR_USING_MODULE_NOT_FOUND] = "ANVL_ERR_USING_MODULE_NOT_FOUND",
    [ANVL_ERR_USING_IN_AMP] = "ANVL_ERR_USING_IN_AMP",
    [ANVL_ERR_USING_AFTER_STATEMENTS] = "ANVL_ERR_USING_AFTER_STATEMENTS",

    [ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR] = "ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR",

    [ANVL_ERR_SCHEMA_ATTR_MISSING] = "ANVL_ERR_SCHEMA_ATTR_MISSING",
    [ANVL_ERR_SCHEMA_TYPE_UNRESOLVED] = "ANVL_ERR_SCHEMA_TYPE_UNRESOLVED",
    [ANVL_ERR_SCHEMA_BASE_UNKNOWN] = "ANVL_ERR_SCHEMA_BASE_UNKNOWN",
    [ANVL_ERR_SCHEMA_VALIDATION_REQUIRED] = "ANVL_ERR_SCHEMA_VALIDATION_REQUIRED",
    [ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH] = "ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH",
    [ANVL_ERR_SCHEMA_VALIDATION_UNKNOWN_FIELD] = "ANVL_ERR_SCHEMA_VALIDATION_UNKNOWN_FIELD",

    [ANVL_ERR_PARSER_UNEXPECTED_CHAR] = "ANVL_ERR_PARSER_UNEXPECTED_CHAR",
    [ANVL_ERR_PARSER_UNTERMINATED_STRING] = "ANVL_ERR_PARSER_UNTERMINATED_STRING",
    [ANVL_ERR_PARSER_UNTERMINATED_BLOB] = "ANVL_ERR_PARSER_UNTERMINATED_BLOB",
    [ANVL_ERR_PARSER_IO_ERROR] = "ANVL_ERR_PARSER_IO_ERROR",
    [ANVL_ERR_PARSER_EXPECTED_BACKTICK] = "ANVL_ERR_PARSER_EXPECTED_BACKTICK",
    [ANVL_ERR_PARSER_UNTERMINATED_FREEFORM] = "ANVL_ERR_PARSER_UNTERMINATED_FREEFORM",
    [ANVL_ERR_PARSER_INVALID_HEX_LITERAL] = "ANVL_ERR_PARSER_INVALID_HEX_LITERAL",
    [ANVL_ERR_PARSER_INVALID_EXPONENT] = "ANVL_ERR_PARSER_INVALID_EXPONENT",
    [ANVL_ERR_PARSER_INVALID_NUMBER] = "ANVL_ERR_PARSER_INVALID_NUMBER",

    [ANVL_ERR_ASL_PARSE_ERROR] = "ANVL_ERR_ASL_PARSE_ERROR",
    [ANVL_ERR_ASL_RUNTIME_ERROR] = "ANVL_ERR_ASL_RUNTIME_ERROR",
    [ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED] = "ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED",
    [ANVL_ERR_ASL_BREAK_OUTSIDE_LOOP] = "ANVL_ERR_ASL_BREAK_OUTSIDE_LOOP",
    [ANVL_ERR_ASL_CONTINUE_OUTSIDE_LOOP] = "ANVL_ERR_ASL_CONTINUE_OUTSIDE_LOOP",

    [ANVL_ERR_IO_FILE_NOT_FOUND] = "ANVL_ERR_IO_FILE_NOT_FOUND",
    [ANVL_ERR_IO_READ_FAILED] = "ANVL_ERR_IO_READ_FAILED",
    [ANVL_ERR_MEMORY_ALLOCATION_FAILED] = "ANVL_ERR_MEMORY_ALLOCATION_FAILED",
    [ANVL_ERR_INVALID_OPERATION] = "ANVL_ERR_INVALID_OPERATION",
    [ANVL_ERR_INVALID_ARGUMENT] = "ANVL_ERR_INVALID_ARGUMENT",
};

/* ------------------------------------------------------------------ */
/* Error State Management                                            */
/* ------------------------------------------------------------------ */
void anvl_error_set(anvl_error_code code, const char *message, usize line, usize column, const char *file) {
   global_error_state.code = code;
   global_error_state.message = message;
   global_error_state.line = line;
   global_error_state.column = column;
   global_error_state.file = file;
}

void anvl_error_clear(void) {
   global_error_state.code = ANVL_ERR_NONE;
   global_error_state.message = NULL;
   global_error_state.line = 0;
   global_error_state.column = 0;
   global_error_state.file = NULL;
}

bool anvl_error_is_set(void) {
   return global_error_state.code != ANVL_ERR_NONE;
}

const anvl_error_state *anvl_error_get(void) {
   return &global_error_state;
}

/* ------------------------------------------------------------------ */
/* Error Code Utilities                                              */
/* ------------------------------------------------------------------ */
const char *anvl_error_code_message(anvl_error_code code) {
   if (code >= sizeof(error_messages) / sizeof(error_messages[0])) {
      return "Unknown error";
   }
   return error_messages[code] ? error_messages[code] : "Unknown error";
}

const char *anvl_error_code_name(anvl_error_code code) {
   if (code >= sizeof(error_names) / sizeof(error_names[0])) {
      return "ANVL_ERR_UNKNOWN";
   }
   return error_names[code] ? error_names[code] : "ANVL_ERR_UNKNOWN";
}