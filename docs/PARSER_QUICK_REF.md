# Anvil Parser - Quick Reference Guide

**For:** Developers joining the project  
**Size:** ~5 min read  
**Updated:** December 19, 2025  

---

## At a Glance

**Parser Type:** Zero-copy, direct-construction  
**Language:** C23 (ISO C)  
**Entry Point:** `bool anvl_parse(context ctx)`  
**Test Status:** 51 tests, all passing ✅  
**Memory:** 18 allocations, 3 critical leaks fixed ✅  

---

## Quick Facts

### What It Does
- Converts AML/ASL source code → metadata-rich statement tree
- Stores all values as spans (position + length) into original source
- No builders, no intermediate representations
- All ownership transfers to context immediately

### What It Doesn't Do
- No semantic validation (type checking, inheritance resolution, etc.)
- No error recovery (stops on first error)
- No string materialization (on-demand via `Source.substring()`)
- No compilation/execution

### 30-Second Parsing Flow
```
Input:  name := "John"
        ↓
Parse identifier: "name" (pos=0, len=4)
        ↓
Expect :=  ✓
        ↓
Parse value: "John" (scalar, pos=9, len=4)
        ↓
Create metadata: {type=SCALAR, pos=9, len=4}
        ↓
Attach to statement
        ↓
Output: statement with metadata
```

---

## The 7 Main Functions

### 1. **anvl_parse(context ctx)** – Entry Point
- **What:** Main parser; only public function
- **Input:** Built context with source
- **Output:** `true` (success) / `false` (error set)
- **Calls:** parse_source()

### 2. **parse_source(parser_ctx)** – Statement Loop
- **What:** Main loop parsing statements until EOF
- **Handles:** Module attributes (@[...] prefix)
- **Calls:** parse_statement() repeatedly

### 3. **parse_statement(parser_ctx, statement)** – Core Statement
- **What:** Parse one `field := value` statement
- **Handles:** Identifiers, inheritance, attributes, values
- **Creates:** base_meta, attr_meta, value_meta
- **Calls:** read_identifier(), parse_attribute_block(), parse_value()

### 4. **parse_value(parser_ctx)** – Dispatcher
- **What:** Route to correct value parser
- **Decision:**
  - `{` → parse_object()
  - `[` → parse_array()
  - `(` → parse_tuple()
  - `@` or `` ` `` → parse_blob()
  - else → parse_scalar_value()

### 5-8. **parse_object/array/tuple/blob()** – Value Parsers
- **What:** Parse collection types
- **Returns:** Temporary value wrapper (owns element metadata)
- **Output:** Transferred to parse_statement for final metadata

### 9. **parse_attribute_block()** – Attribute Parser
- **What:** Parse @[key=value, key, ...] blocks
- **Creates:** attribute structs → context->attr_list
- **Returns:** count via output parameter

### 10. **read_identifier()** – Utility
- **What:** Read identifier from source
- **Returns:** Position + length (zero-copy)

---

## The Memory Contract

### What Parser Allocates
```
parse_statement()
├─ base_meta         [NULLABLE] inheritance info
├─ attr_meta         [NULLABLE] array of attributes
├─ value_meta        [MANDATORY] value information
├─ elem_meta         [CONDITIONAL] if collection
└─ (these get attached to statement)

parse_blob/array/tuple()
├─ elem_types        [TEMPORARY] element type buffer
└─ (disposed after extracting to elem_meta)
```

### Ownership Rules
```
RULE 1: Parser creates everything in context
        └─ Context.dispose() cleans all of it

RULE 2: Temporary objects are disposed immediately
        └─ elem_types, elem_meta, field values

RULE 3: ERROR PATH: Dispose ALL before returning false
        └─ Check: parse_statement() lines 178-194
        └ Check: No leaks; all paths covered

RULE 4: Never dispose value/statement/field after attaching to context
        └─ Context owns; you don't
```

---

## Error Handling in 10 Seconds

### Three-Layer Error System

```
LAYER 1: Parser sets error
    parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, source);
         ↓
LAYER 2: Global error state updated (anvil.c)
    g_error_state = {code, message, line, column, file};
         ↓
LAYER 3: User queries
    const anvl_error_state *err = Anvil.error_get();
    Anvil.error_clear();
```

### Common Error Codes
```
ANVL_ERR_PARSER_EXPECTED_IDENTIFIER       // Missing identifier
ANVL_ERR_PARSER_EXPECTED_ASSIGN           // Missing :=
ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE      // Missing ]
ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED  // {}
ANVL_ERR_PARSER_UNTERMINATED_STRING       // "unclosed
ANVL_ERR_MEMORY_ALLOCATION_FAILED         // malloc() returned NULL
```

### Cleanup Pattern
```c
// Allocate
base_meta = ci_new_base_meta();
if (!base_meta) {
    parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
    return false;  // ERROR: Nothing to clean
}

// Later error
if (bad_condition) {
    if (base_meta) Memory.dispose(base_meta);  // Cleanup
    parser_error(ANVL_ERR_SOMETHING, s);
    return false;  // Propagate error
}

// Success
stmt->base_meta = base_meta;  // Transfer ownership
return true;
```

---

## Reading Source Code: First Steps

### File Structure
```
src/core/parser.c         (768 lines)
├─ Lines 1-50:    Includes, types, forward declarations
├─ Lines 33-45:   PUBLIC anvl_parse()
├─ Lines 48-50:   parser_error()
├─ Lines 52-62:   dispose_*() stubs (context owns)
├─ Lines 64-101:  parse_source() [statement loop]
├─ Lines 105-294: parse_statement() [core logic ⭐]
├─ Lines 300-325: parse_value() [dispatcher]
├─ Lines 338-412: parse_blob()
├─ Lines 414-501: parse_object()
├─ Lines 503-568: parse_array()
├─ Lines 570-633: parse_tuple()
├─ Lines 635-678: parse_scalar_value()
├─ Lines 680-748: parse_attribute_block()
└─ Lines 750-768: read_identifier()
```

### Start Reading Here
1. **Entry:** `anvl_parse()` (line 33) – understand signature
2. **Main Loop:** `parse_source()` (line 64) – statement sequencing
3. **Core Logic:** `parse_statement()` (line 105) ⭐ – where magic happens
4. **One Value:** `parse_scalar_value()` (line 635) – simplest parser
5. **One Collection:** `parse_array()` (line 503) – complex but clear

### Key Lines to Understand

| Line | What | Why |
|------|------|-----|
| 105-115 | Statement parsing setup | Understand metadata lifecycle |
| 125-135 | Inheritance parsing | Single `:`syntax |
| 147-170 | Attribute parsing | Module & statement attrs |
| 178-194 | Error cleanup | Learn leak fixes |
| 187-215 | Metadata creation | All three meta types |
| 245-255 | Statement metadata buffer | How meta[9] works |
| 259-262 | Ownership transfer | stmt->base_meta = base_meta |
| 300-325 | Dispatcher logic | 5-way decision tree |

---

## Common Tasks

### Task 1: Add a New Error Code

**Where:** `include/errors.h`

```c
typedef enum {
    // ... existing codes ...
    ANVL_ERR_PARSER_MY_NEW_ERROR,  // Add here
} anvl_error_code;
```

**Then in parser.c:**
```c
if (bad_condition) {
    parser_error(ANVL_ERR_PARSER_MY_NEW_ERROR, s);
    return false;
}
```

### Task 2: Track New Metadata (e.g., element positions)

**In parse_array():**
```c
usize *elem_positions = Memory.alloc(sizeof(usize) * count, true);
while (/* ... */) {
    usize elem_start = si_position(s);
    value elem = parse_value(p);
    elem_positions[element_count++] = elem_start;
}
v->data.collection._elem_positions_temp = elem_positions;
```

**In parse_statement():**
```c
usize *elem_pos = val->data.collection._elem_positions_temp;
if (elem_pos) {
    for (usize i = 0; i < elem_count; i++) {
        elem_meta[i].pos = elem_pos[i];
    }
    Memory.dispose(elem_pos);
}
```

### Task 3: Fix a Memory Leak

**Pattern:**
1. Find the error return path
2. Check what's allocated before it
3. Dispose all allocated objects before return
4. Example: Lines 189-194 (Leak #3 fix)

```c
if (!value_meta) {
    // What's allocated? base_meta, attr_meta, val
    if (base_meta) Memory.dispose(base_meta);
    if (attr_meta) Memory.dispose(attr_meta);
    Memory.dispose(val);  // ← FIX: Was missing
    parser_error(...);
    return false;
}
```

### Task 4: Add a Test

**In test/test_parser.c:**
```c
static void test_parse_my_feature(void) {
    const char *source = "my_test := value";
    
    context ctx = parse_source(source, ANVL_DIALECT_AML);
    Assert.isNotNull(ctx, "Context should be created");
    
    // Your assertions here
    
    Context.dispose(ctx);
    teardown();
}

// Register in __attribute__((constructor)):
testcase("My Feature", test_parse_my_feature);
```

---

## Debugging Tips

### Check Error State
```c
context ctx = builder->build(builder);
Context.parse(ctx);

const anvl_error_state *err = Anvil.error_get();
if (err && err->code != ANVL_ERR_NONE) {
    printf("ERROR: %s at %ld:%ld\n", 
           err->message, err->line, err->column);
}
```

### Print Source Position
```c
printf("Current position: %ld\n", si_position(s));
printf("Line: %ld, Column: %ld\n", si_line(s), si_column(s));
printf("Next char: '%c' (EOF=%d)\n", si_peek(s), si_is_eof(s));
```

### Inspect Metadata
```c
statement stmt = Context.get_statement(ctx, 0);

printf("Identifier: pos=%ld, len=%ld\n", 
       stmt->meta[STMT_META_IDENT_POS],
       stmt->meta[STMT_META_IDENT_LEN]);

if (stmt->base_meta) {
    printf("Inherits from: pos=%ld, len=%ld\n",
           stmt->base_meta->pos, stmt->base_meta->len);
}

printf("Value type: %d\n", stmt->value_meta->type);
```

### Extract Strings (for debugging)
```c
char *ident = Source.substring(ctx->source,
    stmt->meta[STMT_META_IDENT_POS],
    stmt->meta[STMT_META_IDENT_LEN]);
printf("Identifier: %s\n", ident);
Memory.dispose(ident);
```

---

## Testing Workflow

### Run All Parser Tests
```bash
make test_parser
```

### Run Specific Test
```bash
make test_parser 2>&1 | grep "Simple Assignment"
```

### Add Assertions
```c
Assert.isTrue(condition, "message");
Assert.isFalse(condition, "message");
Assert.isNull(ptr, "message");
Assert.isNotNull(ptr, "message");
Assert.areEqual(&expected, &actual, INT, "message");
Assert.skip("reason");
```

### Check Memory Leaks
```bash
# Build with sanitizers
gcc -fsanitize=address ...

# Run test
./build/test/test_parser

# Examine output for leaks
```

---

## Dependencies

### Within Parser
- `context.c` – Context creation (ci_new_*() functions)
- `source.c` – Source interrogation (si_*() functions)
- `errors.c` – Error reporting

### External
- `sigcore/memory.h` – Allocation/disposal
- `stdio.h`, `string.h` – Standard C

### Modules NOT Needed
- `resolver.c` – Does NOT call resolver
- `writer.c` – Parser has no write capability
- `operators.c`, `symbols.c` – Pre-parsed; parser doesn't use

---

## Design Philosophy

### "Zero-Copy"
All strings stored as `{pos, len}` into original buffer.
User extracts via `Source.substring()` only if needed.
Massive memory savings for large documents.

### "Direct Construction"
No builder pattern. Values created directly in context.
Parser is stateless after `anvl_parse()` returns.
Simpler, faster, easier to debug.

### "Context Ownership"
Parser creates in context; context destroys.
User calls `Context.dispose(ctx)` once at end.
Prevents double-free, use-after-free bugs.

### "Metadata-First"
Don't store values; store metadata (pos, len, type).
Each statement is self-contained.
Resolver can query on demand without context lookups.

---

## Common Misconceptions

### ❌ "Parser copies strings"
**✅ Correct:** Parser stores positions; strings stay in source

### ❌ "I need to dispose value objects"
**✅ Correct:** Context owns everything; dispose context once

### ❌ "Parser validates semantics"
**✅ Correct:** Parser only checks syntax; resolver handles semantics

### ❌ "Error handling is complex"
**✅ Correct:** Single global error state; check after parse

### ❌ "I need to understand resolver"
**✅ Correct:** Parser works standalone; resolver is optional

---

## Links & Resources

| Resource | Location |
|----------|----------|
| Full Architecture | `docs/PARSER_ARCHITECTURE.md` |
| Memory Audit | `MEMORY_AUDIT_CRITICAL_LEAKS_FIXED.md` |
| Type Definitions | `include/types.h` |
| Spec Draft | `docs/ANVIL_SPEC_DRAFT.md` |
| Tests | `test/test_parser.c` (1529 lines) |
| Sample Files | `test/samples/*.anvl` |

---

## One-Page Cheat Sheet

```c
// PARSE
context ctx = Context.get_builder()->build(builder);
Context.parse(ctx);

// CHECK ERROR
if (Anvil.error_is_set()) {
    printf("%s\n", Anvil.error_get()->message);
}

// GET STATEMENT
statement stmt = Context.get_statement(ctx, 0);

// EXTRACT IDENTIFIER
char *ident = Source.substring(ctx->source,
    stmt->meta[STMT_META_IDENT_POS],
    stmt->meta[STMT_META_IDENT_LEN]);

// GET VALUE TYPE
anvl_value_type type = stmt->value_meta->type;

// CHECK INHERITANCE
if (stmt->base_meta) {
    char *base = Source.substring(ctx->source,
        stmt->base_meta->pos, stmt->base_meta->len);
}

// ITERATE ATTRIBUTES
for (usize i = 0; i < stmt->meta[STMT_META_ATTR_IDX]; i++) {
    struct anvl_attr_meta attr = stmt->attr_meta[i];
}

// CLEANUP
Context.dispose(ctx);
Anvil.error_clear();
```

---

**Next Steps:**
1. Read `docs/PARSER_ARCHITECTURE.md` (full docs)
2. Examine `src/core/parser.c` lines 105-294 (parse_statement)
3. Run `make test_parser` to see it work
4. Try modifying a test to understand the flow
5. Trace through with debugger if needed

**Questions?** Check the architecture doc or examine test cases for examples.

---

**Version:** 1.0 (Dec 19, 2025)  
**Status:** Ready for onboarding
