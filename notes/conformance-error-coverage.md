# Conformance error-code coverage (FR-2610-anvl-conformance-002)

`include/errors.h` declares 106 internal error codes (`anvl_err_code`). The conformance corpus (`conformance/cases/`)
reaches **41** of them. The other 65 cannot be produced by any input to the reader as built (`WITH_ASL=0`). This was
established three ways: (1) a search of `src/` for a site that records each code finds none for most of them (the
rest are listed with their reason below); (2) 200000 generated and mutated documents and fragments
(`conformance/fuzz` generator, seed 7, one 200k chunk under the memory cap) reached no code outside the 40 the corpus held
before this ticket, plus 3009; (3) targeted inputs for every code that does have a recording site (`coverage/errors/`,
`coverage/includes/`), which found 9002.

The codes added by this ticket: **3009** `ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE` (an unclosed tuple that ends after a
comma: `coverage/errors/tuple_unclosed_after_comma.anvl`) and **9002** `ANVL_ERR_INVALID_ARGUMENT` (an include whose
joined path is 1024 bytes or more: `coverage/includes/path_len_1006.anvl`).

Many of the unreachable codes are names for mistakes that native reports under a different code or accepts silently.
Examples, each a corpus case: a duplicate attribute key (`errors/attr_duplicate_key.anvl`), a keyword as an attribute name
(`errors/attr_keyword_key.anvl`), a duplicate object field (`errors/object_duplicate_field.anvl`), a reference to a name that does not
exist (`errors/varref_missing_target.anvl`), inheritance from a scalar (`errors/inherit_from_scalar.anvl`), an empty attribute
block (`errors/empty_attr_block.anvl`, 2007), a trailing comma in an object (4017), a second shebang (2005), a module
attribute after a statement (2001). These are candidate defects or spec questions for the owner, not for the corpus: the dumps
record what native does today, and a port must match it.

| code | name | in corpus | first case, or why it cannot be reached |
|---|---|---|---|
| 0 | `ANVL_ERR_NONE` | no | Not an error: the dump writes it when native fails before it records anything (an unreadable root file). An input is a file that exists, so the corpus cannot hold one. |
| 1001 | `ANVL_ERR_IO_FILE_READ` | no | Raised only when the root file cannot be read. An included file that fails is reported as 4204. |
| 1002 | `ANVL_ERR_IO_FILE_NOT_FOUND` | no | Raised only when the root file does not exist. An included file that does not exist is 4204. |
| 1003 | `ANVL_ERR_IO_INVALID_PATH` | no | Raised for an empty or invalid root path, never for document content. An include with such a path is 4204. |
| 2000 | `ANVL_ERR_PARSER_INITIALIZATION_FAILED` | no | Written only when the parser cannot be set up or fails without recording a cause (out of memory). |
| 2001 | `ANVL_ERR_PARSER_EXPECTED_IDENTIFIER` | yes (70 dumps) | `coverage/columns/bom_ok.anvl` |
| 2002 | `ANVL_ERR_PARSER_EXPECTED_ASSIGN` | yes (39 dumps) | `coverage/columns/before_assign__bad_utf8.anvl` |
| 2003 | `ANVL_ERR_PARSER_EXPECTED_VALUE` | yes (68 dumps) | `coverage/columns/mb_ident_value.anvl` |
| 2004 | `ANVL_ERR_PARSER_MULTIPLE_SHEBANG` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 2005 | `ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS` | yes (12 dumps) | `coverage/errors/second_shebang.anvl` |
| 2006 | `ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 2007 | `ANVL_ERR_PARSER_INVALID_IDENTIFIER` | yes (12 dumps) | `coverage/columns/mb_attr_error.anvl` |
| 2008 | `ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 2009 | `ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_ATTRIBUTE` | yes (3 dumps) | `grammar/attr_varref_value.anvl` |
| 2010 | `ANVL_ERR_PARSER_VARREF_NOT_ALLOWED_IN_FRAGMENT` | yes (2 dumps) | `unit/value_fragment_L114.frag` |
| 2011 | `ANVL_ERR_PARSER_INVALID_SHEBANG_DIALECT` | yes (18 dumps) | `coverage/errors/shebang_asl_short.anvl` |
| 2012 | `ANVL_ERR_HEADER_ASL_NOT_SUPPORTED` | yes (30 dumps) | `coverage/errors/asl_dialect_without_asl.anvl` |
| 3001 | `ANVL_ERR_PARSER_EXPECTED_OBJECT_FIELD` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 3002 | `ANVL_ERR_PARSER_EXPECTED_OBJECT_VALUE` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 3003 | `ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE` | yes (3 dumps) | `coverage/errors/object_unclosed_eof.anvl` |
| 3004 | `ANVL_ERR_PARSER_TRAILING_COMMA_IN_OBJECT` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 3007 | `ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY` | yes (9 dumps) | `coverage/columns/mb_fragment_error.frag` |
| 3008 | `ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE` | yes (2 dumps) | `coverage/errors/tuple_unclosed_in_array.anvl` |
| 3009 | `ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE` | yes (7 dumps) | `coverage/errors/tuple_unclosed.frag` |
| 3010 | `ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED` | yes (5 dumps) | `coverage/errors/empty_object.anvl` |
| 3011 | `ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 3012 | `ANVL_ERR_PARSER_EMPTY_TUPLE_NOT_ALLOWED` | yes (4 dumps) | `coverage/errors/empty_tuple.anvl` |
| 3013 | `ANVL_ERR_PARSER_TUPLE_TOO_FEW_ELEMENTS` | yes (7 dumps) | `coverage/errors/tuple_one_element.anvl` |
| 3014 | `ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 3015 | `ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 3016 | `ANVL_ERR_PARSER_EXPECTED_COLON` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 3100 | `ANVL_ERR_PARSER_UNEXPECTED_TOKEN` | yes (38 dumps) | `coverage/errors/amp_no_shebang.amp` |
| 4001 | `ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4002 | `ANVL_ERR_PARSER_INVALID_KEY_IN_OBJECT` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4003 | `ANVL_ERR_PARSER_IDENTIFIER_IS_KEYWORD` | yes (22 dumps) | `coverage/errors/amp_import_not_allowed.amp` |
| 4004 | `ANVL_ERR_PARSER_ATTRIBUTE_IS_KEYWORD` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4005 | `ANVL_ERR_PARSER_TUPLE_TOO_SHORT` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4006 | `ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE` | yes (4 dumps) | `coverage/errors/tuple_missing_comma_eof.anvl` |
| 4007 | `ANVL_ERR_PARSER_EMPTY_TUPLE_ELEMENT` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4008 | `ANVL_ERR_PARSER_ROCKET_OP_NOT_VALID` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4009 | `ANVL_ERR_PARSER_ARRAY_CANNOT_BE_EMPTY` | yes (5 dumps) | `coverage/errors/empty_array.anvl` |
| 4010 | `ANVL_ERR_PARSER_ASSIGNMENT_NOT_ALLOWED_HERE` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4011 | `ANVL_ERR_PARSER_INVALID_ATTRIBUTE_BLOCK` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4012 | `ANVL_ERR_PARSER_INVALID_ATTRIBUTE` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4013 | `ANVL_ERR_PARSER_DUPLICATE_ATTRIBUTE_KEY` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4014 | `ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4015 | `ANVL_ERR_ANON_BLOCK_REDECLARATION` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4016 | `ANVL_ERR_PARSER_UNTERMINATED_COMMENT` | yes (13 dumps) | `coverage/errors/block_comment_unterminated.anvl` |
| 4017 | `ANVL_ERR_PARSER_UNTERMINATED_STATEMENT` | yes (66 dumps) | `coverage/columns/multiline_string__bad_utf8.anvl` |
| 4018 | `ANVL_ERR_PARSER_VALUE_IS_KEYWORD` | yes (5 dumps) | `grammar/bare_import.anvl` |
| 4019 | `ANVL_ERR_PARSER_INVALID_BLOB_TAG` | yes (5 dumps) | `coverage/errors/blob_tag_invalid.anvl` |
| 4020 | `ANVL_ERR_PARSER_INHERITANCE_REQUIRES_OBJECT` | yes (2 dumps) | `fixtures/body_err_base_non_object.anvl` |
| 4021 | `ANVL_ERR_PARSER_INVALID_BARE_LITERAL` | yes (2 dumps) | `unit/body_amp_L1029.anvl` |
| 4050 | `ANVL_ERR_RESOLVER_CYCLE_DETECTED` | yes (4 dumps) | `coverage/errors/inherit_cycle.anvl` |
| 4051 | `ANVL_ERR_RESOLVER_MISSING_BASE` | yes (4 dumps) | `coverage/errors/inherit_unknown_base.anvl` |
| 4052 | `ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS` | yes (1 dumps) | `unit/resolver_L288.anvl` |
| 4053 | `ANVL_ERR_RESOLVER_DUPLICATE_IDENTIFIER` | yes (17 dumps) | `coverage/errors/document_duplicate_name.anvl` |
| 4101 | `ANVL_ERR_VARS_BLOCK_ALREADY_DEFINED` | no | For the `vars` block, an AnvilScript feature: with WITH_ASL=0 `vars` is a reserved word and `vars { ... }` is 4003 (`errors/vars_block_is_keyword.anvl`). Nothing records it. |
| 4102 | `ANVL_ERR_VARS_NOT_FIRST` | no | For the `vars` block, an AnvilScript feature: with WITH_ASL=0 `vars` is a reserved word and `vars { ... }` is 4003 (`errors/vars_block_is_keyword.anvl`). Nothing records it. |
| 4103 | `ANVL_ERR_VARS_DUPLICATE_KEY` | no | For the `vars` block, an AnvilScript feature: with WITH_ASL=0 `vars` is a reserved word and `vars { ... }` is 4003 (`errors/vars_block_is_keyword.anvl`). Nothing records it. |
| 4104 | `ANVL_ERR_VARS_CIRCULAR_REF` | no | For the `vars` block, an AnvilScript feature: with WITH_ASL=0 `vars` is a reserved word and `vars { ... }` is 4003 (`errors/vars_block_is_keyword.anvl`). Nothing records it. |
| 4105 | `ANVL_ERR_VARS_KEY_NOT_FOUND` | no | For the `vars` block, an AnvilScript feature: with WITH_ASL=0 `vars` is a reserved word and `vars { ... }` is 4003 (`errors/vars_block_is_keyword.anvl`). Nothing records it. |
| 4201 | `ANVL_ERR_INCLUDE_NOT_FIRST` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4202 | `ANVL_ERR_INCLUDE_AMP_FORBIDDEN` | yes (3 dumps) | `coverage/errors/amp_include_not_allowed.amp` |
| 4203 | `ANVL_ERR_INCLUDE_DUPLICATE_ALIAS` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4204 | `ANVL_ERR_INCLUDE_FILE_NOT_FOUND` | yes (12 dumps) | `coverage/includes/abs_missing.anvl` |
| 4206 | `ANVL_ERR_INCLUDE_CYCLIC` | yes (10 dumps) | `coverage/includes/mutual_a.anvl` |
| 4305 | `ANVL_ERR_IMPORT_MODULE_NOT_FOUND` | no | For `import`, an AnvilScript feature: with WITH_ASL=0 it is a reserved word (4003, `errors/import_in_aml.anvl`, `errors/amp_import_not_allowed.amp`). Nothing records it. |
| 4306 | `ANVL_ERR_IMPORT_IN_AMP` | no | For `import`, an AnvilScript feature: with WITH_ASL=0 it is a reserved word (4003, `errors/import_in_aml.anvl`, `errors/amp_import_not_allowed.amp`). Nothing records it. |
| 4307 | `ANVL_ERR_IMPORT_AFTER_STATEMENTS` | no | For `import`, an AnvilScript feature: with WITH_ASL=0 it is a reserved word (4003, `errors/import_in_aml.anvl`, `errors/amp_import_not_allowed.amp`). Nothing records it. |
| 4401 | `ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR` | yes (3 dumps) | `coverage/errors/amp_array_of_array.amp` |
| 4501 | `ANVL_ERR_SERIALIZER_AMP_OBJECT_NOT_ALLOWED` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 4601 | `ANVL_ERR_SCHEMA_ATTR_MISSING` | no | Schema validation code. The reader never validates a schema, and nothing in `src/` records it as a load error (schema violations have their own diagnostic codes). |
| 4602 | `ANVL_ERR_SCHEMA_TYPE_UNRESOLVED` | no | Schema validation code. The reader never validates a schema, and nothing in `src/` records it as a load error (schema violations have their own diagnostic codes). |
| 4603 | `ANVL_ERR_SCHEMA_BASE_UNKNOWN` | no | Schema validation code. The reader never validates a schema, and nothing in `src/` records it as a load error (schema violations have their own diagnostic codes). |
| 4604 | `ANVL_ERR_SCHEMA_VALIDATION_REQUIRED` | no | Schema validation code. The reader never validates a schema, and nothing in `src/` records it as a load error (schema violations have their own diagnostic codes). |
| 4605 | `ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH` | no | Schema validation code. The reader never validates a schema, and nothing in `src/` records it as a load error (schema violations have their own diagnostic codes). |
| 4606 | `ANVL_ERR_SCHEMA_VALIDATION_UNKNOWN_FIELD` | no | Schema validation code. The reader never validates a schema, and nothing in `src/` records it as a load error (schema violations have their own diagnostic codes). |
| 4701 | `ANVL_ERR_ASL_DUPLICATE_PARAMETER` | no | AnvilScript only (asl_parser.c): `#!asl` is rejected with 2012 before any script is read while WITH_ASL=0. |
| 4702 | `ANVL_ERR_ASL_VARARG_NOT_LAST` | no | AnvilScript only, as 4701. |
| 4703 | `ANVL_ERR_ASL_UNTERMINATED_BODY` | no | AnvilScript only, as 4701. |
| 5001 | `ANVL_ERR_PARSER_UNEXPECTED_CHAR` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 5002 | `ANVL_ERR_PARSER_UNTERMINATED_STRING` | yes (16 dumps) | `coverage/columns/error_at_eof_after_tab.anvl` |
| 5003 | `ANVL_ERR_PARSER_UNTERMINATED_BLOB` | yes (5 dumps) | `coverage/errors/blob_tagged_unterminated.anvl` |
| 5004 | `ANVL_ERR_PARSER_IO_ERROR` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 5005 | `ANVL_ERR_PARSER_EXPECTED_BACKTICK` | yes (6 dumps) | `grammar/blob_tag_32.anvl` |
| 5006 | `ANVL_ERR_PARSER_UNTERMINATED_FREEFORM` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 5007 | `ANVL_ERR_PARSER_INVALID_HEX_LITERAL` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 5008 | `ANVL_ERR_PARSER_INVALID_EXPONENT` | yes (21 dumps) | `coverage/columns/error_on_last_byte.anvl` |
| 5009 | `ANVL_ERR_PARSER_INVALID_NUMBER` | yes (43 dumps) | `coverage/columns/comment_then_num__bad_utf8.anvl` |
| 5010 | `ANVL_ERR_VARS_INVALID_VARREF` | yes (12 dumps) | `coverage/errors/braced_ref_unterminated.anvl` |
| 5011 | `ANVL_ERR_VARS_UNTERMINATED_INTERP` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 5012 | `ANVL_ERR_VARS_UNTERMINATED_BRACED_VARREF` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 5101 | `ANVL_ERR_ASL_PARSE_ERROR` | no | AnvilScript runtime error: declared and named, but nothing in `src/` records it; the runtime is a separate build. |
| 5102 | `ANVL_ERR_ASL_RUNTIME_ERROR` | no | AnvilScript runtime error: declared and named, but nothing in `src/` records it; the runtime is a separate build. |
| 5103 | `ANVL_ERR_ASL_CALL_DEPTH_EXCEEDED` | no | AnvilScript runtime error: declared and named, but nothing in `src/` records it; the runtime is a separate build. |
| 5104 | `ANVL_ERR_ASL_BREAK_OUTSIDE_LOOP` | no | AnvilScript runtime error: declared and named, but nothing in `src/` records it; the runtime is a separate build. |
| 5105 | `ANVL_ERR_ASL_CONTINUE_OUTSIDE_LOOP` | no | AnvilScript runtime error: declared and named, but nothing in `src/` records it; the runtime is a separate build. |
| 7001 | `ANVL_ERR_MEMORY_ALLOC_FAILED` | no | Allocation failure; not reachable from a well-formed input. |
| 7002 | `ANVL_ERR_ARENA_NOT_INITIALIZED` | no | The arena is not initialised: an internal state a loaded document never reaches. |
| 9001 | `ANVL_ERR_INVALID_OPERATION` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
| 9002 | `ANVL_ERR_INVALID_ARGUMENT` | yes (7 dumps) | `coverage/includes/path_len_1006.anvl` |
| 9003 | `ANVL_ERR_SOURCE_NOT_FOUND` | no | Internal source-registry lookup of a source that does not exist; not reachable through a load. |
| 9004 | `ANVL_ERR_CONTEXT_INVALID` | no | Internal: an invalid context passed to a source function; not reachable through a load. |
| 9005 | `ANVL_ERR_SOURCE_ALREADY_LOADED` | no | A second load of identical bytes in one context. An include that repeats a document is handled internally and resolves to the existing document (no error), and a root load always has a fresh context. |
| 9999 | `ANVL_ERR_NOT_IMPLEMENTED` | no | Declared and named in `errors.c`, but nothing in `src/` ever records it; native reports the mistake under another code or accepts it. See the cases named in the section below. |
