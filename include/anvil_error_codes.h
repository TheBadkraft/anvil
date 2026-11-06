/* anvil_error_codes.h - the specific error codes (public, stable ABI); SPDX-License-Identifier: Proprietary */
#pragma once

enum {
   ANVL_ERR_NONE = 0,

   // File I/O Errors (100x)
   ANVL_ERR_IO_FILE_READ = 1001,
   ANVL_ERR_IO_FILE_NOT_FOUND = 1002,
   ANVL_ERR_IO_INVALID_PATH = 1003,

   // Parser Errors (200x-400x)
   ANVL_ERR_PARSER_INITIALIZATION_FAILED = 2000,
   // Top Level (200x)
   ANVL_ERR_PARSER_EXPECTED_IDENTIFIER = 2001,
   ANVL_ERR_PARSER_EXPECTED_ASSIGN = 2002,
   ANVL_ERR_PARSER_EXPECTED_VALUE = 2003,
   ANVL_ERR_PARSER_MULTIPLE_SHEBANG = 2004,
   ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS = 2005,
   ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE = 2006,
   ANVL_ERR_PARSER_INVALID_IDENTIFIER = 2007,
   ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK = 2008,
   ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_ATTRIBUTE = 2009,
   ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_FRAGMENT = 2010,
   ANVL_ERR_PARSER_INVALID_SHEBANG_DIALECT = 2011,
   // Document declares '#!asl' -- a real, recognized dialect token (unlike
   // ANVL_ERR_PARSER_INVALID_SHEBANG_DIALECT's unrecognized-token case) -- but this build was
   // compiled with WITH_ASL=0. Deliberately loud/immediate: checked at header-scan time,
   // before any body content is read, so an ANVL-only build never silently parses an #!asl
   // document just because it happens to contain no function declarations.
   ANVL_ERR_HEADER_ASL_NOT_SUPPORTED = 2012,

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
   ANVL_ERR_PARSER_EMPTY_TUPLE_NOT_ALLOWED = 3012,
   ANVL_ERR_PARSER_TUPLE_TOO_FEW_ELEMENTS = 3013,
   ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES = 3014,
   ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES = 3015,
   ANVL_ERR_PARSER_EXPECTED_COLON = 3016,
   ANVL_ERR_PARSER_UNEXPECTED_TOKEN = 3100,

};

enum {
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
   ANVL_ERR_PARSER_UNTERMINATED_COMMENT = 4016,
   ANVL_ERR_PARSER_UNTERMINATED_STATEMENT = 4017,
   ANVL_ERR_PARSER_VALUE_IS_KEYWORD = 4018,
   ANVL_ERR_PARSER_INVALID_BLOB_TAG = 4019,
   ANVL_ERR_PARSER_INHERITANCE_REQUIRES_OBJECT = 4020,
   ANVL_ERR_PARSER_INVALID_BARE_LITERAL = 4021,
   // A base (`a : b := { ... };` or `a : b { ... };`) on a statement inside an object, at any depth.
   // Only a top-level statement may have a base.
   ANVL_ERR_PARSER_NESTED_BASE = 4022,

   // Resolver Errors (405x)
   ANVL_ERR_RESOLVER_CYCLE_DETECTED = 4050,
   ANVL_ERR_RESOLVER_MISSING_BASE = 4051,
   ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS = 4052,
   // Two top-level statements (in the same document, or across merged includes) declare the
   // same name — ambiguous for both '$identifier' VarRef and 'base' lookup purposes, so this
   // is a hard error regardless of where the collision originates. See notes/resolution-phase.md.
   ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER = 4053,
   // A '$' reference leads back to a container that holds it: `s1 := { a := $s1; };`, `a := [ $a ];`, two
   // statements that refer to each other, a cycle closed through an inherited field. Reported at the '$' of the
   // first reference on a cycle in source order (documents in load order). A plain chain cycle
   // (`a := $b; b := $a;`) is not this error: it stays unresolved.
   ANVL_ERR_RESOLVER_REFERENCE_CYCLE = 4054,

};
