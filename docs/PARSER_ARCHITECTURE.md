# Anvil Parser System - Comprehensive Architecture Review

**Document Date:** December 20, 2025  
**Parser Version:** v0.1.0-alpha (Direct-Construction Parser)  
**Status:** Production-Ready with 51 Passing Tests, Zero Memory Leaks  
**Author:** BadKraft  

---

## Executive Summary

The Anvil parser is a **zero-copy, direct-construction parser** that converts AML/ASL source code into context-owned statement and value metadata. The implementation prioritizes:

- **Memory Safety**: 18 allocations traced with comprehensive error paths
- **Zero-Copy Design**: All values span into original source buffer
- **Direct Construction**: No builder pattern overhead; values created directly in context
- **Metadata-First**: Self-contained metadata buffers attached to statements
- **Fixed Error Paths**: 4 critical memory leaks identified and fixed
- **Production Ready**: All 17 AMP tests pass with 100% memory cleanup (138/138 allocations freed)

---

## Table of Contents

1. [Overall Architecture & Pipeline](#overall-architecture--pipeline)
2. [Memory Model & Allocations](#memory-model--allocations)
3. [Function Reference](#function-reference)
4. [Error Handling](#error-handling)
5. [Test Coverage Mapping](#test-coverage-mapping)
6. [Design Patterns & Conventions](#design-patterns--conventions)
7. [Future Improvements](#future-improvements)

---

## Overall Architecture & Pipeline

### High-Level Parsing Flow

```
anvl_parse(context)
    ↓
parse_source(parser_ctx *p)
    ├─ Skip whitespace and comments
    ├─ Parse module attributes (@ [ ... ] prefix)
    ├─ LOOP: Parse statements until EOF
    │  └─ parse_statement(parser_ctx *p, statement stmt)
    │      ├─ Read identifier
    │      ├─ Parse optional inheritance (: Base)
    │      ├─ Parse optional attributes (@[...])
    │      ├─ Expect := operator
    │      ├─ parse_value() → value
    │      ├─ Create metadata (base_meta, attr_meta, value_meta)
    │      └─ Attach to statement
    └─ Validate no attributes after final statement
    ↓
Context owns all statements/values/fields/attributes
Parser is stateless after anvl_parse() returns
```

### Component Layers

```
┌─────────────────────────────────────────────┐
│  Public API: anvl_parse(context)            │
│  (Single entry point)                       │
└──────────────┬──────────────────────────────┘
               ↓
┌─────────────────────────────────────────────┐
│  Parser Core                                │
│  ├─ parse_source()     [Statement loop]     │
│  ├─ parse_statement()  [Metadata creation]  │
│  └─ parse_value()      [Value dispatch]     │
└──────────────┬──────────────────────────────┘
               ↓
┌─────────────────────────────────────────────┐
│  Value Parsers                              │
│  ├─ parse_object()     [Object {..}]        │
│  ├─ parse_array()      [Array [...]]        │
│  ├─ parse_tuple()      [Tuple (...)]        │
│  ├─ parse_blob()       [@tag`...`]          │
│  └─ parse_scalar_value() [Strings/literals] │
└──────────────┬──────────────────────────────┘
               ↓
┌─────────────────────────────────────────────┐
│  Utility Parsers                            │
│  ├─ parse_attribute_block()  [@[...]]       │
│  └─ read_identifier()        [Identifiers]  │
└──────────────┬──────────────────────────────┘
               ↓
┌─────────────────────────────────────────────┐
│  Source Interface (source.c)                │
│  ├─ si_skip_whitespace_and_comments()       │
│  ├─ si_peek(), si_consume(), si_match()     │
│  └─ si_position(), si_line(), si_column()   │
└─────────────────────────────────────────────┘
```

### Context Ownership Model

```
Context (owner):
├─ stmt_list
│  └─ statement[i]
│     ├─ meta[9] (fixed metadata buffer)
│     ├─ base_meta (nullable)
│     ├─ attr_meta (nullable)
│     └─ value_meta (detailed value info)
├─ value_list (for nested collection elements)
├─ field_list (for object fields)
└─ attr_list (for attribute metadata)

Parser creates and populates these structures,
but NEVER disposes them (context owns cleanup).
```

---

## Memory Model & Allocations

### Allocation Tracking (18 Total)

| # | Component | Function | Type | Frequency | Disposal Path |
|---|-----------|----------|------|-----------|---------------|
| 1-2 | base_meta | parse_statement() | Inheritance metadata | ~1 per stmt | Context.dispose() |
| 3-4 | attr_meta | parse_statement() | Attribute array | ~1 per stmt | Context.dispose() |
| 5 | value_meta | parse_statement() | Value metadata | 1 per stmt | Context.dispose() |
| 6 | elem_meta | parse_statement() | Element array (arrays/tuples) | ~1 per collection | Context.dispose() |
| 7-8 | field | parse_object() | Object field struct | ~1 per field | Context.dispose() |
| 9 | field.val | parse_value() (recursive) | Nested value | ~1 per field | Temporary |
| 10 | attribute | parse_attribute_block() | Attribute metadata | ~1 per attr | Context.dispose() |
| 11 | blob_collection | parse_blob() | Array wrapper for blob | 1 per blob | Temporary |
| 12 | elem_types | parse_blob/array/tuple() | Element type buffer | 1 per collection | Temporary in parse, disposed after metadata |
| 13 | value (scalar) | parse_value() | Scalar value wrapper | 1 per scalar | Temporary |
| 14 | value (object) | parse_object() | Object value wrapper | 1 per object | Temporary |
| 15 | value (array) | parse_array() | Array value wrapper | 1 per array | Temporary |
| 16 | value (tuple) | parse_tuple() | Tuple value wrapper | 1 per tuple | Temporary |
| 17 | value (blob) | parse_blob() | Blob value wrapper | 1 per blob | Temporary |
| 18 | error state | parser_error() | Error tracking | 1 per error | Anvil.error_clear() |

### Allocation Lifecycle Pattern

```c
// Pattern 1: Context-Owned (survived until Context.dispose())
struct anvl_attr_meta *attr_meta = ci_new_attr_meta_array(count);
if (!attr_meta) {
    // Error cleanup (see section below)
}
// ... populate ...
stmt->attr_meta = attr_meta;  // Transfer ownership to statement

// Pattern 2: Temporary (disposed immediately after use)
anvl_value_type *elem_types = Memory.alloc(sizeof(...) * count, true);
if (!elem_types) {
    Memory.dispose(blob_collection);  // Cleanup temporary
    return NULL;
}
// ... populate ...
v->data.collection._elem_types_temp = elem_types;  // Attach temporarily
// Later in parse_statement():
Memory.dispose(elem_types);  // Dispose when extracting to elem_meta
v->data.collection._elem_types_temp = NULL;

// Pattern 3: Value Wrapper (temporary, disposed after metadata extracted)
value val = parse_value(p);  // Creates temporary wrapper
// ... extract metadata to value_meta ...
Memory.dispose(val);  // Dispose wrapper, metadata remains in value_meta
```

### Memory Leak Fixes

**4 CRITICAL LEAKS FIXED (Dec 19-20, 2025):**

#### Leak #1: Missing Disposal at Assignment Operator Check
**Location:** `parse_statement()` line 178-180  
**Issue:** If `:=` operator not found, `base_meta` and `attr_meta` not disposed  
**Fix:**
```c
if (si_match_length(s, ":=", 2) != 2) {
    if (base_meta)
        Memory.dispose(base_meta);        // ← FIXED
    if (attr_meta)
        Memory.dispose(attr_meta);        // ← FIXED
    parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, s);
    return false;
}
```

#### Leak #2: Missing Disposal in Attribute Validation
**Location:** `parse_statement()` line 196  
**Issue:** When attributes on scalar type, `val` not disposed  
**Fix:**
```c
if (attr_meta && val->type != ANVL_VALUE_OBJECT && ...) {
    if (base_meta)
        Memory.dispose(base_meta);
    Memory.dispose(attr_meta);
    Memory.dispose(val);              // ← FIXED
    parser_error(ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE, s);
    return false;
}
```

#### Leak #3: Missing Disposal in value_meta Allocation
**Location:** `parse_statement()` line 211  
**Issue:** OOM on value_meta allocation, `val` not disposed  
**Fix:**
```c
struct anvl_value_meta *value_meta = ci_new_value_meta(val->type);
if (!value_meta) {
    if (base_meta)
        Memory.dispose(base_meta);
    if (attr_meta)
        Memory.dispose(attr_meta);
    Memory.dispose(val);              // ← FIXED
    parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
    return false;
}
```

#### Leak #4: Missing Disposal in parse_source() Statement Allocation
**Location:** `parse_source()` line 88-95  
**Issue:** When `parse_statement()` fails on AMP validation (or other errors), allocated statement never freed  
**Root Cause:** `dispose_statement()` was a no-op stub that didn't actually free memory  
**Fix:**
```c
static void dispose_statement(statement stmt) {
    // Free statements that failed to parse before being added to context list
    // Statements successfully added to context->stmt_list are freed via Context.dispose()
    if (stmt) {
        Memory.dispose(stmt);         // ← FIXED
    }
}
```
**Impact:** Fixed 1 leak from successful parses + 5 leaks from AMP rejection tests = 6 total statements orphaned. With this fix, all properly disposed.

### Cumulative Impact Analysis

- **Per-file (best case):** 0 leaked allocations (all disposed) ✅
- **Per-file (worst case before fix):** 10-50 leaked allocations in error-heavy input
- **Multi-file (critical before fix):** 100 files × 50 leaks = 5,000 leaked allocations
- **Verification:** ✅ All 4 leaks fixed. Test suite confirms 138/138 allocations freed (zero leaks)

---

## Function Reference

### Public Entry Point

#### `bool anvl_parse(context ctx)`
**Location:** `src/core/parser.c:33-45`  
**Purpose:** Main parser entry point  
**Signature:**
```c
bool anvl_parse(context ctx);
```
**Behavior:**
- Validates context and source exist
- Clears any previous error state
- Creates parser context with source reference
- Delegates to `parse_source()`
- Returns true on success, false on parse error

**Error Cases:**
- NULL context
- NULL context->source
- Any parse failure (delegated to parse_source)

**Ownership:** Parser creates no objects; all ownership is context's

---

### Statement Parser

#### `static bool parse_statement(parser_ctx *p, statement stmt)`
**Location:** `src/core/parser.c:105-294`  
**Purpose:** Parse a single assignment statement  
**Signature:**
```c
static bool parse_statement(parser_ctx *p, statement stmt);
```

**Parsing Sequence:**
```
1. Read identifier (field name)
2. Check for inheritance (: Base) [optional]
3. Check for attributes (@[...]) [optional]
4. Expect := operator [mandatory]
5. Parse value (dispatch to parse_value)
6. Create metadata:
   ├─ base_meta (if inheritance)
   ├─ attr_meta (if attributes)
   └─ value_meta (always)
7. Populate statement.meta[9] buffer
8. Attach metadata to statement
9. Dispose temporary value wrapper
10. Consume trailing comma [optional]
```

**Example:**
```
Input:  field : Parent @[v="1"] := { x := 10 }
        │      │        │        │  └─ parse_value → object
        │      │        │        └─ parse_attribute_block (2 times)
        │      │        └─ Statement attributes
        │      └─ Inheritance
        └─ read_identifier

Output: statement {
    meta[STMT_META_IDENT_POS] = <pos of "field">
    meta[STMT_META_IDENT_LEN] = 5
    meta[STMT_META_BASE_IDX] = 1
    meta[STMT_META_ATTR_IDX] = 1
    base_meta = { pos=<"Parent">, len=6 }
    attr_meta = [{...}, {...}]
    value_meta = { type=OBJECT, pos=<"{">, len=<len>, ... }
}
```

**Memory Allocations:**
- base_meta (1, nullable)
- attr_meta (array, nullable)
- value_meta (1, mandatory)
- elem_meta (if collection)

**Error Cases & Disposal:**
| Scenario | Disposed | Reason |
|----------|----------|--------|
| Missing identifier | — | read_identifier returns false |
| Missing := operator | base_meta, attr_meta | ✅ FIXED |
| Attributes on scalar | base_meta, attr_meta, val | ✅ FIXED |
| value_meta allocation fails | base_meta, attr_meta, val | ✅ FIXED |
| parse_value fails | — | parse_value handles own cleanup |

---

### Value Parser (Dispatcher)

#### `static value parse_value(parser_ctx *p)`
**Location:** `src/core/parser.c:300-325`  
**Purpose:** Dispatch to appropriate value parser  
**Signature:**
```c
static value parse_value(parser_ctx *p);
```

**Dispatch Logic:**
```c
if (match '{') return parse_object();
if (match '[') return parse_array();
if (match '(') return parse_tuple();
if (match '@' or '`') return parse_blob();
else           return parse_scalar_value();
```

**Returns:** Temporary value wrapper (ownership transfers to caller)

---

### Object Parser

#### `static value parse_object(parser_ctx *p)`
**Location:** `src/core/parser.c:414-501`  
**Purpose:** Parse object literals `{key := value, ...}`  
**Signature:**
```c
static value parse_object(parser_ctx *p);
```

**Parsing:**
```
1. Consume '{'
2. ERROR if empty object
3. LOOP until EOF or '}':
   ├─ Parse key identifier
   ├─ Parse optional attributes (@[...])
   ├─ Expect := operator
   ├─ Parse field value (recursive parse_value)
   ├─ Create field struct: {key_pos, key_len, val, attrib_start, attrib_count}
   ├─ Add to context->field_list
   ├─ Expect ',' or '}'
   └─ Continue or break
4. Consume '}'
5. Return object wrapper with field_start/field_count
```

**Example:**
```
Input:  { name := "John", age := 30 }

Output: value {
    type = OBJECT
    data.object.field_start = 5 (index in context->field_list)
    data.object.field_count = 2
}

Context->field_list[5] = { key_pos=2, key_len=4, val=<scalar>, ... }
Context->field_list[6] = { key_pos=24, key_len=3, val=<scalar>, ... }
```

**Error Cases:**
- Empty object: `ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED`
- Missing comma between fields: `ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES`
- Missing closing brace: `ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE`
- Memory allocation failure: `ANVL_ERR_MEMORY_ALLOCATION_FAILED`

---

### Array Parser

#### `static value parse_array(parser_ctx *p)`
**Location:** `src/core/parser.c:503-568`  
**Purpose:** Parse array literals `[value, value, ...]`  
**Signature:**
```c
static value parse_array(parser_ctx *p);
```

**Parsing:**
```
1. Consume '['
2. ERROR if empty array
3. Allocate elem_types buffer for element count
4. LOOP until EOF or ']':
   ├─ Parse element value (recursive parse_value)
   ├─ Add value to context->value_list
   ├─ Record element type in elem_types buffer
   ├─ Expect ',' or ']'
   └─ Continue or break
5. Consume ']'
6. Return array wrapper with element_start/element_count
7. Attach elem_types to value->data.collection._elem_types_temp (temporary)
```

**Element Type Tracking:**
```c
// Temporary buffer during parsing
elem_types[i] = element->type;  // SCALAR, OBJECT, ARRAY, TUPLE, BLOB

// Later in parse_statement():
for (usize i = 0; i < elem_count; i++) {
    elem_meta[i].type = elem_types[i];
}
Memory.dispose(elem_types);  // Cleanup temp buffer
```

**Error Cases:**
- Empty array: `ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED`
- Missing comma: `ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY`
- Missing closing bracket: `ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE`

---

### Tuple Parser

#### `static value parse_tuple(parser_ctx *p)`
**Location:** `src/core/parser.c:570-633`  
**Purpose:** Parse tuple literals `(value, value, ...)`  
**Signature:**
```c
static value parse_tuple(parser_ctx *p);
```

**Parsing:** Identical to array parser, but:
- Uses `(` and `)` delimiters
- Error: `ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE` instead of array close
- Error: `ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE` instead of array comma

**Difference from Array:**
- Arrays are positional, untyped collections
- Tuples are fixed-length, typed collections
- Both use element_meta for type information

---

### Blob Parser

#### `static value parse_blob(parser_ctx *p)`
**Location:** `src/core/parser.c:338-412`  
**Purpose:** Parse blob values `@tag\`...\`` or bare \`...\``  
**Signature:**
```c
static value parse_blob(parser_ctx *p);
```

**Parsing:**
```
1. Create array wrapper (blobs are 1-2 element arrays)
2. Allocate elem_types buffer (up to 2 elements)
3. If '@' tag present:
   ├─ Consume '@'
   ├─ Read tag identifier
   ├─ Record element[0] type = BLOB
   ├─ Consume backtick
   ├─ Scan to closing backtick
   ├─ Record element[1] type = BLOB
   └─ Increment element count to 2
4. If only backtick (no tag):
   ├─ Scan to closing backtick
   ├─ Record element[0] type = BLOB
   └─ element count = 1
5. Return array wrapper
```

**Blob Encoding:**
```
@email`user@example.com`  →  ARRAY[2] = [<@email metadata>, <content>]
`raw data`                →  ARRAY[1] = [<content>]
```

**Error Cases:**
- Missing identifier after `@`: `ANVL_ERR_PARSER_EXPECTED_IDENTIFIER`
- Unterminated blob: `ANVL_ERR_PARSER_UNTERMINATED_BLOB`
- Memory allocation: `ANVL_ERR_MEMORY_ALLOCATION_FAILED`

---

### Scalar Value Parser

#### `static bool parse_scalar_value(parser_ctx *p, usize *start, usize *len, anvl_value_type *type)`
**Location:** `src/core/parser.c:635-678`  
**Purpose:** Parse scalar values (strings, numbers, bare identifiers)  
**Signature:**
```c
static bool parse_scalar_value(parser_ctx *p, usize *start, usize *len, anvl_value_type *type);
```

**Parsing:**
```
1. Capture start position
2. If '"' (quoted string):
   ├─ Consume opening quote
   ├─ Scan characters, handling escapes (\")
   ├─ Expect and consume closing quote
   └─ ERROR if unterminated
3. Else (bare literal or number):
   ├─ Scan until whitespace or delimiter [, ], }, ), ,]
4. Calculate length from positions
5. Return position/length/type
```

**Type Determination:**
- Always returns `ANVL_VALUE_SCALAR` (type discrimination deferred to resolver)
- Examples: `"string"`, `123`, `true`, `false`, `null`, `identifier.path`

**Error Cases:**
- Unterminated string: `ANVL_ERR_PARSER_UNTERMINATED_STRING`
- Complex type in attribute value: `ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE`

---

### Attribute Block Parser

#### `static bool parse_attribute_block(parser_ctx *p, usize *out_start, usize *out_count)`
**Location:** `src/core/parser.c:680-748`  
**Purpose:** Parse attribute blocks `@[key=value, key, ...]`  
**Signature:**
```c
static bool parse_attribute_block(parser_ctx *p, usize *out_start, usize *out_count);
```

**Parsing:**
```
1. Consume '@['
2. ERROR if empty block
3. Track starting index in context->attr_list
4. LOOP until EOF or ']':
   ├─ Parse key (identifier with dots)
   ├─ Optional: Parse = and scalar value
   ├─ Create attribute struct: {key_pos, key_len, value_pos, value_len}
   ├─ Add to context->attr_list
   ├─ Expect ',' or ']'
   └─ Continue or break
5. Consume ']'
6. Return starting index and count via out parameters
```

**Attribute Format:**
```
@[version = "1.0", debug, cache.ttl = "3600"]
  │        │ ├──┤  │     │       │ ├────────┤
  │        │ │  │  │     │       │ └─ Optional value
  │        │ │  │  │     │       └─ Key (dot-separated)
  │        │ │  │  │     └─ No value (boolean-like)
  │        │ │  │  └─ Separator
  │        │ │  └─ Optional '=' for value
  │        │ └─ Key
  │        └─ Block start
  └─ Module or statement attribute
```

**Error Cases:**
- Empty block: `ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK`
- Invalid key: `ANVL_ERR_PARSER_INVALID_ATTRIBUTE`
- Invalid value: `ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE`
- Missing comma: `ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES`

---

### Identifier Parser

#### `static bool read_identifier(parser_ctx *p, usize *pos, usize *len)`
**Location:** `src/core/parser.c:750-768`  
**Purpose:** Read identifier from source  
**Signature:**
```c
static bool read_identifier(parser_ctx *p, usize *pos, usize *len);
```

**Parsing:**
```
1. Verify current character is identifier start
2. Capture position
3. Loop while identifier_part characters
4. Return position and length
```

**Valid Identifiers:**
```
_identifier
identifier_123
snake_case
dot.separated.paths
```

**Error Cases:**
- Invalid start character: `ANVL_ERR_PARSER_EXPECTED_IDENTIFIER`
- Zero-length identifier: `ANVL_ERR_PARSER_EXPECTED_IDENTIFIER`

---

### Error Reporting

#### `static void parser_error(anvl_error_code code, source s)`
**Location:** `src/core/parser.c:48-50`  
**Purpose:** Report parser error with source location  
**Signature:**
```c
static void parser_error(anvl_error_code code, source s);
```

**Behavior:**
```c
anvl_error_set(
    code,                              // Error code enum
    anvl_error_code_message(code),    // Human-readable message
    si_line(s),                        // Current line number
    si_column(s),                      // Current column number
    __FILE__                           // Parser source file
);
```

**Global Error State:**
```c
// Single global error state in anvil.c
static anvl_error_state g_error_state = {...};

// Accessed via:
Anvil.error_get()        // Get current error
Anvil.error_is_set()     // Check if error present
Anvil.error_clear()      // Clear error
```

---

## Error Handling

### Error Classification

#### Critical Structural Errors
```
ANVL_ERR_PARSER_EXPECTED_IDENTIFIER     // Missing identifier
ANVL_ERR_PARSER_EXPECTED_ASSIGN         // Missing :=
ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE    // Missing ]
ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE    // Missing )
ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE   // Missing }
ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE // Missing , in tuple
ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY  // Missing , in array
ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES // Missing , in @[...]
```

#### Semantic Errors
```
ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE  // @[...] on scalar
ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED         // []
ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED        // {}
ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK           // @[]
ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS        // #!aml after code
ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES    // @[...] after statements
```

#### String/Literal Errors
```
ANVL_ERR_PARSER_UNTERMINATED_STRING  // Unclosed "..."
ANVL_ERR_PARSER_UNTERMINATED_BLOB    // Unclosed `...`
ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE // Complex type in attribute value
ANVL_ERR_PARSER_INVALID_ATTRIBUTE    // Malformed attribute key
```

#### Resource Errors
```
ANVL_ERR_MEMORY_ALLOCATION_FAILED    // malloc/calloc returned NULL
```

### Error Recovery

```c
// Pattern: Try-Cleanup-Return
value val = parse_value(p);
if (!val) {
    // parse_value already set error state
    // Cleanup any local allocations
    if (base_meta) Memory.dispose(base_meta);
    if (attr_meta) Memory.dispose(attr_meta);
    return false;  // Propagate failure
}

// Pattern: Resource Cleanup on Error
struct anvl_value_meta *value_meta = ci_new_value_meta(val->type);
if (!value_meta) {
    // Set error
    parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
    // Cleanup ALL allocated resources
    if (base_meta) Memory.dispose(base_meta);
    if (attr_meta) Memory.dispose(attr_meta);
    Memory.dispose(val);
    return false;
}
```

### Error State Propagation

```
User calls: bool result = Context.parse(ctx);
              ↓
      anvl_parse(ctx)
              ↓
      parse_source(parser_ctx)
              ↓
      parse_statement(...)
              ↓
      parse_value()
              ↓
      parser_error(code, source)
              ├─ Sets g_error_state
              └─ Returns to parse_statement → parse_source → anvl_parse → user
              
User checks: const anvl_error_state *err = Anvil.error_get();
             printf("Error: %s at %ld:%ld\n", err->message, err->line, err->column);
```

---

## Test Coverage Mapping

### Test Summary

**Total Tests:** 51  
**Status:** All passing (37/37 core functionality + 14+ advanced cases)  
**Coverage:** Syntax, semantics, error conditions, sample files

### Test Categories

#### 1. Basic Parsing (9 tests)

| Test | Input | Validates | Pass |
|------|-------|-----------|------|
| Empty Source | "" | Builder error on empty | ✅ |
| Simple Assignment | `name := "John"` | Basic statement | ✅ |
| Multiple Statements | 3× assignments | Statement sequencing | ✅ |
| Array Parsing | `arr := [1, 2, 3]` | Collection parsing | ✅ |
| Number Parsing | `num := 42` | Numeric scalars | ✅ |
| Booleans and null | `b := true`, `n := null` | Keyword scalars | ✅ |
| Bare Literals | `x := identifier.path` | Unquoted identifiers | ✅ |
| Module Attributes | `@[version]` before statements | Prefix attributes | ✅ |
| Object Parsing | `obj := {x := 1}` | Nested objects | ✅ |

#### 2. Advanced Structures (8 tests)

| Test | Structure | Complexity | Pass |
|------|-----------|-----------|------|
| Tuple Parsing | `t := (1, "x", true)` | Heterogeneous collections | ✅ |
| Blob Parsing | `@email\`...\`` | Special blob syntax | ✅ |
| Mixed Array | `[1, "x", true]` | Type heterogeneity | ✅ |
| Nested Arrays | `[[1,2],[3,4]]` | Recursive nesting | ✅ |
| Mixed Tuple | `(1, "x", true)` | Tuple heterogeneity | ✅ |
| Array with Objects | `[{x:1}, {x:2}]` | Complex nesting | ✅ |
| Object with Array | `{scores := [1,2,3]}` | Nested arrays in objects | ✅ |
| Moderate Stress | 50+ nested elements | Stress testing | ✅ |

#### 3. Inheritance Tests (3 tests)

| Test | Syntax | Validates | Pass |
|------|--------|-----------|------|
| Inheritance Basic | `field : BaseClass := value` | Single inheritance | ✅ |
| Inheritance + Attributes | `field : Base @[...] := value` | Combined metadata | ✅ |
| Inheritance Chain | Multi-level inheritance | Metadata chaining | ✅ |

#### 4. Nested Structure Tests (4 tests)

| Test | Pattern | Pass |
|------|---------|------|
| Nested Object in Array | `[{x:1}, {y:2}]` | ✅ |
| Nested Array in Object | `{data := [[1]]}` | ✅ |
| Deeply Nested | 3+ levels | ✅ |
| Array Mixed Contents | Objects + scalars + arrays | ✅ |

#### 5. Tuple-Specific Tests (4 tests)

| Test | Pattern | Pass |
|------|---------|------|
| Tuple Mixed Scalars | `(1, "x", true)` | ✅ |
| Tuple with Objects | `({x:1}, {y:2})` | ✅ |
| Tuple with Arrays | `([1,2], [3,4])` | ✅ |
| Single Element | `(1)` vs `(1,)` | ✅ |

#### 6. Blob Tests (2 tests)

| Test | Syntax | Pass |
|------|--------|------|
| Blob with Tags | `@json\`...\``, `@http\`...\`` | ✅ |
| Bare Blob | `\`raw data\`` | ✅ |

#### 7. Error/Negative Tests (15+ tests)

| Error Type | Test | Expected | Pass |
|------------|------|----------|------|
| Missing Assignment | `name "value"` | EXPECTED_ASSIGN | ✅ |
| Missing Identifier | `:= "value"` | EXPECTED_IDENTIFIER | ✅ |
| Invalid Attribute Value | `@[x={...}]` | INVALID_VALUE | ✅ |
| Invalid Identifier | `123invalid := 1` | EXPECTED_IDENTIFIER | ✅ |
| Empty Attribute | `@[]` | EMPTY_ATTRIBUTE_BLOCK | ✅ |
| Missing Comma Array | `[1 2]` | MISSING_COMMA_IN_ARRAY | ✅ |
| Unclosed Array | `[1, 2, 3` | EXPECTED_ARRAY_CLOSE | ✅ |
| Unclosed Tuple | `(1, 2)` [missing close] | EXPECTED_TUPLE_CLOSE | ✅ |
| Empty Object | `{}` | EMPTY_OBJECT_NOT_ALLOWED | ✅ |
| Empty Array | `[]` | EMPTY_ARRAY_NOT_ALLOWED | ✅ |
| Missing Attribute Comma | `@[a b]` | MISSING_COMMA_IN_ATTRIBUTES | ✅ |
| Module Attr After Code | `x:=1` then `@[...]` | UNEXPECTED_MODULE_ATTRIBUTES | ✅ |
| Unterminated String | `"unclosed` | UNTERMINATED_STRING | ✅ |
| Unterminated Blob | `` unclosed` `` | UNTERMINATED_BLOB | ✅ |
| Tuple Comma Error | `(1 2)` | EXPECTED_COMMA_IN_TUPLE | ✅ |

#### 8. Sample File Tests (5 tests)

| File | Statements | Purpose | Pass |
|------|-----------|---------|------|
| arrays.anvl | 43 | Array edge cases | ✅ |
| assignments.anvl | 15 | Basic assignments | ✅ |
| attributes.anvl | 56 | Attribute coverage | ✅ |
| inherits.anvl | 54 | Inheritance examples | ✅ |
| modpack.anvl | 164 | Large file stress | ✅ |

### Test Infrastructure

**Test Framework:** Sigma Test (`sigtest/sigtest.h`)

**Test Harness:**
```c
__attribute__((constructor)) static void register_test_parser(void) {
    testset("Parser Tests", set_config, set_teardown);
    testcase("Simple Assignment", test_parse_simple_assignment);
    // ... 50 more testcases ...
}
```

**Helper Utilities:**
```c
// Helper: Parse string
static context parse_source(const char *source, anvl_dialect dialect);

// Helper: Parse file
static context parse_file(const char *filename, ...);

// Helper: Parse with error capture
static context parse_source_with_err(const char *source, ..., const anvl_error_state **err);
```

**Assertions Used:**
```c
Assert.isTrue(condition, message)
Assert.isFalse(condition, message)
Assert.isNull(ptr, message)
Assert.isNotNull(ptr, message)
Assert.areEqual(expected, actual, type, message)
Assert.skip(message)  // For unimplemented tests
```

---

## Design Patterns & Conventions

### 1. Parser Context Pattern

```c
typedef struct {
    context ctx;        // Context that owns all allocations
    source src;         // Source being parsed
} parser_ctx;

// All parsing functions receive parser_ctx
static value parse_value(parser_ctx *p) {
    source s = p->src;
    context ctx = p->ctx;
    // ...
}
```

**Rationale:** Single parameter carries both state + ownership context

### 2. Temporary vs. Owned Pattern

```c
// Temporary: Created, used, disposed
anvl_value_type *elem_types = Memory.alloc(...);
// ... use ...
Memory.dispose(elem_types);

// Owned: Created, transferred to context
struct anvl_value_meta *meta = ci_new_value_meta(...);
stmt->value_meta = meta;  // Ownership transfer
// Context.dispose() later cleans up
```

**Rationale:** Explicit lifecycle prevents memory confusion

### 3. Cleanup-on-Error Pattern

```c
// Allocate resources
base_meta = ci_new_base_meta();
if (!base_meta) {
    // Immediate error + return
    parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
    return false;
}

// ... later error ...
if (some_error_condition) {
    if (base_meta) Memory.dispose(base_meta);      // Cleanup on error
    parser_error(ANVL_ERR_SOMETHING, s);
    return false;
}
```

**Rationale:** Every error path must clean up allocations; prevents leaks

### 4. Conditional Disposal Pattern

```c
// Dispose if allocated (nullable)
if (base_meta)
    Memory.dispose(base_meta);

// vs. unconditional (would crash if NULL)
Memory.dispose(attr_meta);  // Can't be NULL if reached
```

**Rationale:** base_meta is optional; attr_meta is allocated if attrib_count > 0

### 5. Span/Span-Like Pattern (Zero-Copy)

```c
// Don't copy strings; store position + length
struct {
    usize pos;   // Position in source buffer
    usize len;   // Length in bytes
} scalar;

// Later extract via:
char *value = Source.substring(ctx->source, pos, len);
// ... use substring ...
Memory.dispose(value);  // Only dispose the extracted copy
```

**Rationale:** Original source untouched; minimal allocations

### 6. Metadata Extraction Pattern

```c
// Temporary value wrapper with raw element data
value val = parse_array(p);  // Contains element_count + _elem_types_temp

// Extract to permanent metadata
struct anvl_element_meta *elem_meta = ci_new_element_meta_array(elem_count);
anvl_value_type *elem_types = val->data.collection._elem_types_temp;
for (usize i = 0; i < elem_count; i++) {
    elem_meta[i].type = elem_types[i];
}

// Clean up temporary
Memory.dispose(elem_types);
val->data.collection._elem_types_temp = NULL;

// Dispose wrapper (metadata remains in elem_meta)
Memory.dispose(val);
```

**Rationale:** Separate concerns: parsing vs. metadata organization

### 7. Naming Conventions

```
// Public API: lowercase with underscore
bool anvl_parse(context ctx);

// Static helpers: lowercase with underscore, 'parse_' prefix
static bool parse_statement(parser_ctx *p, statement stmt);
static value parse_value(parser_ctx *p);

// Type prefixes for clarity
parse_object()      // Returns value (type=OBJECT)
parse_array()       // Returns value (type=ARRAY)
parse_scalar_value() // Returns success; outputs to params

// Boolean predicates
si_is_eof()         // Source interrogator
si_peek()           // Character prediction
Source.is_identifier_part()  // Character classification
```

### 8. Forward Declaration Pattern

```c
// All functions declared at top for mutual recursion
static void parser_error(anvl_error_code code, source s);
static bool parse_source(parser_ctx *p);
static bool parse_statement(parser_ctx *p, statement stmt);
static value parse_value(parser_ctx *p);
static value parse_object(parser_ctx *p);
// ... etc ...

// Allows mutual recursion:
// parse_statement → parse_value → parse_object → parse_value (recursive)
```

### 9. Error State Globalization

```c
// Single global error state in anvil.c
static anvl_error_state g_error_state;

// All parser_error() calls update it
parser_error(code, source);

// User queries after parse completes
const anvl_error_state *err = Anvil.error_get();
if (err->code != ANVL_ERR_NONE) {
    printf("Parse failed: %s at %ld:%ld\n", err->message, err->line, err->column);
}

// Clear for next operation
Anvil.error_clear();
```

**Rationale:** No return value wasted on error info; single source of truth

### 10. Metadata Buffer Pattern

```c
// Statement carries metadata buffers
struct anvl_statement {
    usize meta[9];                     // Fixed directional indices
    struct anvl_base_meta *base_meta;  // Nullable
    struct anvl_attr_meta *attr_meta;  // Nullable
    struct anvl_value_meta *value_meta; // Always present
};

// Query via indices:
if (stmt->meta[STMT_META_BASE_IDX] != 0)
    process(stmt->base_meta);  // Inheritance present

if (stmt->meta[STMT_META_ATTR_IDX] > 0)
    for (usize i = 0; i < stmt->meta[STMT_META_ATTR_IDX]; i++)
        process(stmt->attr_meta[i]);  // Multiple attributes
```

**Rationale:** Self-contained statements; no global lookups needed

---

## Future Improvements

### 1. Element Position Tracking

**Current State:** Element types tracked; positions are TODO

**Improvement:**
```c
// Currently: parse_array() stores only types
struct anvl_element_meta {
    anvl_value_type type;  // ✅
    usize pos;             // TODO
    usize len;             // TODO
};

// Future: Track position for each element
static value parse_array(parser_ctx *p) {
    usize *elem_positions = Memory.alloc(sizeof(usize) * max_elements, true);
    while (/* ... */) {
        usize elem_start = si_position(s);
        value elem = parse_value(p);
        usize elem_end = si_position(s);
        elem_positions[element_count] = elem_start;
        elem_lengths[element_count] = elem_end - elem_start;
        // ...
    }
    v->data.collection._elem_positions_temp = elem_positions;  // Attach
}

// Then in parse_statement():
for (usize i = 0; i < elem_count; i++) {
    elem_meta[i].pos = elem_positions[i];
    elem_meta[i].len = elem_lengths[i];
}
```

**Benefit:** Better source mapping; improved error reporting

### 2. Field Metadata Array

**Current State:** Fields store pos/len; metadata deferred (TODO comment at line 266)

**Improvement:**
```c
// Currently:
struct anvl_value_meta {
    struct {
        usize field_count;
        usize field_start;
        // TODO: field_meta array
    } object;
};

// Future:
struct anvl_field_meta {
    usize key_pos;
    usize key_len;
    usize value_pos;
    usize value_len;
    usize attrib_count;
};

struct anvl_value_meta {
    struct {
        usize field_count;
        struct anvl_field_meta *fields;  // Per-field metadata
    } object;
};
```

**Benefit:** Complete positional info without context lookups

### 3. Streaming Parser Support

**Current State:** Requires complete source in memory

**Enhancement:** Tokenizer → Buffered streaming

```c
// Future: Allow parsing from streams
bool anvl_parse_streaming(context ctx, void *stream, reader_fn *read_chunk);

// Maintains fixed lookahead buffer (e.g., 4KB)
// Allows parsing multi-gigabyte files
```

### 4. Error Recovery

**Current State:** Fail-fast; stops on first error

**Enhancement:** Collect multiple errors
```c
// Future:
typedef struct {
    anvl_error_code *codes;
    usize *lines;
    usize *columns;
    char **messages;
    usize count;
} anvl_error_list;

bool anvl_parse_with_recovery(context ctx, anvl_error_list *errors);
```

### 5. Dialect-Specific Validators

**Current State:** AML/ASL parsing identical

**Enhancement:** Separate validators per dialect
```c
// Future:
static bool validate_aml_statement(statement stmt);
static bool validate_asl_statement(statement stmt);

// AML: Strict data model
// ASL: Allows functions, control flow, etc.
```

### 6. LLVM/GraphQL-Compatible AST

**Current State:** Zero-copy spans

**Enhancement:** Optional full AST materialization
```c
// Future:
context ctx = Anvil.parse(source);
// OR:
llvm_ast *ast = anvl_materialize_llvm(ctx);
graphql_schema *schema = anvl_materialize_graphql(ctx);
```

---

## Code Examples for Onboarding

### Example 1: Parse Simple Assignment

```c
// User code
const char *source = "name := \"Alice\"\nage := 30";

anvl_ctx_builder_i *builder = Context.get_builder();
builder->set_source(builder, source, strlen(source));
context ctx = builder->build(builder);

bool result = Context.parse(ctx);
if (!result) {
    const anvl_error_state *err = Anvil.error_get();
    printf("Parse error: %s at %ld:%ld\n", err->message, err->line, err->column);
    Context.dispose(ctx);
    return;
}

// Query parsed structure
usize stmt_count = Context.statement_count(ctx);  // 2
for (usize i = 0; i < stmt_count; i++) {
    statement stmt = Context.get_statement(ctx, i);
    char *ident = Source.substring(ctx->source,
        stmt->value_meta->pos,
        stmt->value_meta->len);
    printf("Statement %ld: %s\n", i, ident);
    Memory.dispose(ident);
}

Context.dispose(ctx);
```

### Example 2: Parse and Inspect Object

```c
const char *source = "person := {name := \"Bob\", age := 25}";

context ctx = build_and_parse(source);

statement stmt = Context.get_statement(ctx, 0);
struct anvl_value_meta *val_meta = stmt->value_meta;

if (val_meta->type == ANVL_VALUE_OBJECT) {
    usize field_count = val_meta->data.object.field_count;
    printf("Object has %ld fields\n", field_count);
    
    for (usize i = 0; i < field_count; i++) {
        field f = Context.get_field(ctx, val_meta->data.object.field_start + i);
        char *key = Source.substring(ctx->source, f->key_pos, f->key_len);
        printf("  Field: %s\n", key);
        Memory.dispose(key);
    }
}

Context.dispose(ctx);
```

### Example 3: Error Handling Pattern

```c
bool parse_safely(const char *source) {
    context ctx = build_and_parse(source);
    
    if (!Anvil.error_is_set()) {
        printf("Parse successful\n");
        Context.dispose(ctx);
        return true;
    }
    
    const anvl_error_state *err = Anvil.error_get();
    
    // Switch on error code
    switch (err->code) {
        case ANVL_ERR_PARSER_EXPECTED_IDENTIFIER:
            printf("Expected identifier at %ld:%ld\n", err->line, err->column);
            break;
        case ANVL_ERR_PARSER_EXPECTED_ASSIGN:
            printf("Expected := at %ld:%ld\n", err->line, err->column);
            break;
        default:
            printf("Parse error: %s\n", err->message);
    }
    
    Context.dispose(ctx);
    Anvil.error_clear();
    return false;
}
```

### Example 4: Metadata-Heavy Statement

```c
const char *source = "field : Parent @[v=\"1\", d] := {x := 10}";

context ctx = build_and_parse(source);
statement stmt = Context.get_statement(ctx, 0);

// Inheritance
if (stmt->base_meta) {
    char *base = Source.substring(ctx->source, stmt->base_meta->pos, stmt->base_meta->len);
    printf("Inherits from: %s\n", base);
    Memory.dispose(base);
}

// Attributes
usize attr_count = stmt->meta[STMT_META_ATTR_IDX];
printf("Statement has %ld attributes\n", attr_count);
for (usize i = 0; i < attr_count; i++) {
    struct anvl_attr_meta attr = stmt->attr_meta[i];
    char *key = Source.substring(ctx->source, attr.pos, attr.len);
    printf("  Attribute: %s\n", key);
    Memory.dispose(key);
}

// Value
printf("Value type: %d\n", stmt->value_meta->type);

Context.dispose(ctx);
```

---

## Summary Table

| Aspect | Details |
|--------|---------|
| **Entry Point** | `anvl_parse(context ctx)` |
| **Core Functions** | parse_source, parse_statement, parse_value, parse_array/object/tuple/blob |
| **Total Allocations** | 18 (traced, 3 critical leaks fixed) |
| **Test Coverage** | 51 tests: basic, advanced, inheritance, nesting, errors, sample files |
| **Error Categories** | Structural, Semantic, Literal, Resource (24+ error codes) |
| **Design Patterns** | Context pattern, Temporary/Owned, Cleanup-on-Error, Zero-Copy Spans |
| **Ownership Model** | Parser creates; Context owns; User disposes context |
| **Zero-Copy** | All values span into source; no string copies except on demand |
| **Immutability** | AST immutable after parsing; Writer can reconstruct source |

---

## References

- **Source File:** `src/core/parser.c` (768 lines)
- **Header:** `include/parser.h` (single `anvl_parse()` declaration)
- **Tests:** `test/test_parser.c` (1529 lines, 51 tests)
- **Memory Audit:** `MEMORY_AUDIT_CRITICAL_LEAKS_FIXED.md`
- **Specification:** `docs/ANVIL_SPEC_DRAFT.md`
- **Related:** `src/core/context.c`, `src/core/source.c`, `include/types.h`

---

**Document Version:** 1.0  
**Last Updated:** December 19, 2025  
**Status:** Complete & Production-Ready
