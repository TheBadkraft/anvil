# THE ANVIL C REFERENCE DOCUMENT  
**Version 1.0 – December 2025 – This document is the single source of truth. Everything else is legacy.**

## 1. What Anvil Is (and Is Not)

> “Type doesn't matter in **AML** … so `default_config := null // wrong type!` … so what … you can make it whatever you want … document your template with a comment.” 
>
> ~Badkraft

| Is                                                                                 | Is Not                                                                                 |
|------------------------------------------------------------------------------------|--------------------------------|
| Attributed Node Variadic Language (ANVL)                                           | “Yet another config format” |
| Dual dialect: AML (declarative, data-first) + ASL (imperative, scripting-first)    | Interpreter for AML            |
| Zero-boilerplate, human-first syntax                                                 | Typed language                 |
| Zero-copy parsing, immutable AST, round-tripping                                    | Mutable AST by default         |
| Resolver is 100 % pluggable – consumer owns `$var`, `$func()`, inheritance, interpolation | Magic resolution               |
| Default file extension `.anvl` (shebang overrides)                                 | Tied to any file extension     |
| Designed to become the build configuration language of the future (AnvilBuild)     | Just a library                 |

## 2. Core Principles (never broken)

1. Pure ISO C – C23 by default, C11 fallback only when forced  
2. No macros except literal constants (`#define ANVIL_VERSION "1.0"`)  
3. Public API is a single global `const struct anvil_interface Anvil`  
4. All types are opaque handles (`anvil_root`, `anvil_value`, …)  
5. Zero-copy – everything is `anvil_span` into the original source buffer  
6. AST is immutable after parsing  
7. Resolver is pluggable, fail-soft, zero hidden magic  
8. Writer emits byte-for-byte identical source (comments, whitespace, order)  
9. No platform-specific code in the core – wrappers own the sugar  

## 3. Directory Layout (final, forever)

```
anvil/
├── bin/
├── build/
├── include/
│   └── anvil.h                  ← only public header
├── src/
│   ├── cli/
│   ├── core/
│   │   ├── anvil.c  
│   │   ├── parser.c
│   │   ├── types.c
│   │   ├── resolver.c
│   │   └── writer.c
│   ├── include/
│   ├── samples/				← 6 core .anvl files
│   ├── test/
├── tests/                      ← `stest` harness
├── samples/
└── README.md                   ← this document
```

## 4. Build System (today)

```make
.SILENT:

# -----------------------------------------------------------------------------
# Tools & flags
CC      = cc
CFLAGS  = -std=c2x -Wall -Wextra -Werror -O3 -march=native -fPIC -g
AR      = ar
ARFLAGS = rcs

# -----------------------------------------------------------------------------
# Directories
BUILD       = build
TEST_BUILD  = $(BUILD)/test
BIN         = bin

SRC    		= src
INC         = -I$(SRC)/include
SRC_TESTS   = $(SRC)/test
TEST_INC    = -I$(SRC_TESTS)/include $(INC)

# -----------------------------------------------------------------------------
# Core source objects
CORE_OBJS = $(BUILD)/anvil.o     \
            $(BUILD)/parser.o    \
            $(BUILD)/types.o     \
            $(BUILD)/resolver.o  \
            $(BUILD)/writer.o

$(BUILD) $(BIN) $(TEST_BUILD):
	@mkdir -p $@

dirs: $(BUILD) $(BIN) $(TEST_BUILD)

# -----------------------------------------------------------------------------
# Core library (static)
$(BIN)/libanvil.a: $(CORE_OBJS) | $(BIN)
	$(AR) $(ARFLAGS) $@ $^

$(BUILD)/%.o: $(SRC)/core/%.c | dirs
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# -----------------------------------------------------------------------------
# CLI executable
$(BIN)/anvil: $(SRC)/cli/main.c $(BIN)/libanvil.a | $(BIN)
	$(CC) $(CFLAGS) $(INC) $< -L$(BIN) -lanvil -o $@

$(BIN)/test_%: $(TEST_BUILD)/test_%.o $(BIN)/libanvil.a $(TEST_BUILD)/utils.o | $(BIN)
	$(CC) $(CFLAGS) $(TEST_INC) $^ -L$(BIN) -lanvil -lstest -o $@

$(TEST_BUILD)/test_%.o: $(SRC_TESTS)/test_%.c | $(TEST_BUILD)
	$(CC) $(CFLAGS) $(TEST_INC) -c $< -o $@

clean:
	rm -f $(BUILD)/*.o $(BUILD)/*.a $(BUILD)/*.so
	rm -f $(TEST_BUILD)/*.o
	rm -f $(BIN)/*

.PHONY: all dirs welcome test clean
```
**NOTE**: We will be updating the build to our Bash Build System immediately.

## 5. The Public Header (include/anvil.h)

> **Design sketch — December 2025.** The types and interface shape here (`anvil_root`, `anvil_node`, `anvil_object`, …) reflect the original design intent. The canonical, implemented C API is documented in §9.

```c
// include/anvil.h
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Opaque handles */
typedef struct anvil_root_t*   anvil_root;
typedef struct anvil_node_t*   anvil_node;
typedef struct anvil_value_t*  anvil_value;
typedef struct anvil_object_t* anvil_object;
typedef struct anvil_array_t*  anvil_array;
typedef struct anvil_tuple_t*  anvil_tuple;

/* Dialect */
typedef enum {
    ANVIL_DIALECT_AML,
    ANVIL_DIALECT_ASL
} anvil_dialect;

/* Zero-copy string view */
typedef struct {
    const char* start;
    size_t      len;
} anvil_span;

/* Resolver – 100 % pluggable */
typedef anvil_value (*anvil_resolve_var_fn)(anvil_span name, void* user);
typedef anvil_value (*anvil_resolve_call_fn)(anvil_span name,
                                            anvil_value* args,
                                            size_t argc,
                                            void* user);

typedef struct {
    anvil_resolve_var_fn  var;
    anvil_resolve_call_fn call;
    void*                 user;
} anvil_resolver;

/* Example interface ... will be broken up as necessary, respecting objects, concerns, etc. */
typedef struct anvil_interface anvil_interface;

struct anvil_interface {
    /* Creation */
    anvil_root (*parse_memory)(const char* source, size_t len, anvil_dialect dialect);
    anvil_root (*parse_file)(const char* path);  // auto-detects dialect

    void       (*root_destroy)(anvil_root root);
};

/* The Anvil interface */
extern const anvil_interface Anvil;

//  other APIs to be implemented ...
    /* Root */
    size_t     (*root_node_count)(anvil_root root);
    anvil_node (*root_node_at)(anvil_root root, size_t index);
    anvil_node (*root_node_named)(anvil_root root, const char* name, size_t name_len);

    /* Node */
    anvil_span (*node_identifier)(anvil_node node);
    anvil_value (*node_value)(anvil_node node);
    size_t     (*node_attribute_count)(anvil_node node);
    bool       (*node_has_attribute)(anvil_node node, const char* key, size_t key_len);
    anvil_value (*node_attribute)(anvil_node node, const char* key, size_t key_len);

    /* Value type queries */
    bool       (*value_is_long)(anvil_value v);
    bool       (*value_is_double)(anvil_value v);
    bool       (*value_is_string)(anvil_value v);
    bool       (*value_is_bool)(anvil_value v);
    bool       (*value_is_null)(anvil_value v);
    bool       (*value_is_object)(anvil_value v);
    bool       (*value_is_array)(anvil_value v);
    bool       (*value_is_tuple)(anvil_value v);
    bool       (*value_is_blob)(anvil_value v);
    bool       (*value_needs_resolution)(anvil_value v);   // $var, $func(), $"…"

    /* Value extraction – zero-copy */
    int64_t    (*value_as_long)(anvil_value v);
    double     (*value_as_double)(anvil_value v);
    bool       (*value_as_bool)(anvil_value v);
    anvil_span (*value_as_string)(anvil_value v);
    anvil_span (*value_as_blob)(anvil_value v);
    anvil_object (*value_as_object)(anvil_value v);
    anvil_array  (*value_as_array)(anvil_value v);
    anvil_tuple  (*value_as_tuple)(anvil_value v);

    /* Containers */
    size_t      (*object_field_count)(anvil_object obj);
    anvil_value (*object_field_value)(anvil_object obj, const char* key, size_t key_len);
    anvil_value (*object_field_at)(anvil_object obj, size_t index);

    size_t      (*array_length)(anvil_array arr);
    anvil_value (*array_at)(anvil_array arr, size_t index);

    size_t      (*tuple_length)(anvil_tuple tup);
    anvil_value (*tuple_at)(anvil_tuple tup, size_t index);

    /* Resolution & writing */
    void        (*resolve)(anvil_root root, const anvil_resolver* resolver);
    bool        (*write_to_file)(anvil_root root, const char* path);
    bool        (*write_to_buffer)(anvil_root root, char* buf, size_t* out_len);
```

## 6. Dialect Rules (parser behaviour)

| Construct                     | AML                                  | ASL                                  |
|-------------------------------|--------------------------------------|--------------------------------------|
| `(a, b) => { … }`             | illegal                              | function statement (body stored as blob with synthetic @asl attribute) |
| `$ident(...)`                 | illegal                              | deferred call expression             |
| `$var` in string / blob       | interpolation (sets needs_resolution) | same + nested calls allowed          |
| `$.path`                     | bare reference                        | same                                 |
| Dialect detection             | shebang or .anvl extension → AML     | shebang #!asl → ASL                  |

## 7. Future (already decided)

- AnvilBuild – the first real build system using `.anvl` instead of CMakeLists.txt  
- ASL VM – lightweight stack-based interpreter, fully pluggable opcodes  
- Bindings for Java, C#, Python, Rust, Zig, Odin, Hare, etc. – all consume exactly the interface above  

## 9. The Implemented C API

> **This is the canonical reference for the Anvil C library as shipped.** §5 is a historical design sketch; the types and interface names here are what the code actually exports.

### 9.1 Overview

Anvil's public API is structured as a set of global `const struct` vtables. Each vtable groups related operations and is accessed as a namespace:

| Global | Header | Purpose |
|--------|--------|---------|
| `Anvil` | `anvil.h` | Top-level load / dispose / error |
| `Context` | `context.h` | Parse context lifecycle + navigation |
| `Source` | `context.h` | Zero-copy source interrogation |
| `Statement` | `context.h` | Statement metadata queries |

All parse-time allocations are owned by a per-context bump arena (`ctx->arena`). Handles (`context`, `statement`, `field`, `value`, `attribute`) are opaque pointers into that arena — **do not `free()` them directly**. Call `Context.dispose(ctx)` to release everything at once.

### 9.2 Lifecycle

```c
// 1. Get a builder
anvl_ctx_builder_i *b = Context.get_builder();

// 2. Configure: string source
b->set_source(b, src, len);                // parse from memory
// … or file:
b->load_file(b, "/path/to/file.anvl");    // parse from file, detects dialect

// 3. Build the context
context ctx = b->build(b);

// 4. Parse
if (!Context.parse(ctx)) {
    const anvl_error_state *e = Anvil.error_get();
    // handle error …
}

// 5. Use …

// 6. Release everything
Context.dispose(ctx);
```

The builder (`anvl_ctx_builder_i`) is stack-allocated and reusable within a single call sequence.

### 9.3 Statement Navigation

After a successful `Context.parse()`, all top-level statements are available via index:

```c
usize n = Context.statement_count(ctx);
for (usize i = 0; i < n; i++) {
    statement stmt = Context.get_statement(ctx, i);
    anvl_stmt_type t = Statement.type(stmt);
    const char *id   = Statement.identifier(stmt, ctx->source);
    // …
}
```

`Context.get_statement` returns `NULL` for out-of-range indices.

### 9.4 Query Path Primitives (E3)

**Philosophy**: primitives, not policy. No single traversal algorithm suits every consumer. Anvil core exposes minimal iteration primitives; the consumer builds whatever traversal strategy fits — depth-first, jq-style, jsonpath, simple `node["field"]` chaining, etc. An `anvil.api` package (post-v1.0, out-of-core) will provide opinionated helpers on top.

**Access model**:

| Node kind | Index access | Named access |
|-----------|--------------|--------------|
| Statement | `Context.get_statement(ctx, i)` | — |
| Object field | `Context.get_field(ctx, stmt, i)` | `Context.get_field_by_name(ctx, stmt, name, len)` |
| Array element | `Context.get_element(ctx, stmt, i)` | — |
| Tuple element | `Context.get_element(ctx, stmt, i)` | — |

#### Object Field Traversal

```c
usize Context.field_count(context self, statement stmt);
```
Returns the number of key-value fields in an object-valued statement. Returns `0` if `self` or `stmt` is `NULL`, if `stmt->value_meta` is `NULL`, or if the statement's value type is not `ANVL_VALUE_OBJECT`.

```c
field Context.get_field(context self, statement stmt, usize index);
```
Returns the `index`-th field (0-based) of an object-valued statement. Returns `NULL` for `NULL` arguments, non-object value type, or `index >= field_count`. Fields are ordered as they appear in source.

```c
field Context.get_field_by_name(context self, statement stmt,
                                const char *name, usize len);
```
Returns the field whose key matches `name` (byte-exact, `len` bytes). Returns `NULL` for `NULL` arguments, non-object value type, or if no field with that key exists. The search is case-sensitive.

> **Performance note**: `get_field_by_name` is currently O(n) — a linear `memcmp` scan over the object's fields against the raw source buffer. It will be upgraded to O(1) once `Map` (`FR-2603-sigma-collections-002`) lands in `sigma.collections`.

#### Collection Element Traversal

```c
usize Context.element_count(context self, statement stmt);
```
Returns the number of elements in an array- or tuple-valued statement. Returns `0` if `self` or `stmt` is `NULL`, if `stmt->value_meta` is `NULL`, or if the value type is neither `ANVL_VALUE_ARRAY` nor `ANVL_VALUE_TUPLE`.

```c
struct anvl_element_meta *Context.get_element(context self, statement stmt, usize index);
```
Returns a pointer to the metadata for the `index`-th element (0-based). Returns `NULL` for `NULL` arguments, non-collection value type, or `index >= element_count`. The pointer is into the arena — do not `free()` it.

#### The `field` Structure

```c
struct anvl_field {
    usize key_pos;      // byte offset of the key in source
    usize key_len;      // byte length of the key
    usize attrib_start; // start index in context->attr_list (field-level attributes)
    usize attrib_count; // number of field-level attributes
    value val;          // the field's value (recursive)
};
```

Key span extraction (zero-copy):

```c
const char *raw = Source.data(ctx->source);
// key text is raw[f->key_pos .. f->key_pos + f->key_len]
printf("%.*s", (int)f->key_len, raw + f->key_pos);
```

The `val` pointer contains the field's `type` (`anvl_value_type`) and, for nested objects, its own field list indices — enabling recursive descent.

#### The `anvl_element_meta` Structure

```c
struct anvl_element_meta {
    anvl_value_type type; // type of this element (scalar, object, array, …)
    usize pos;            // byte offset of the element value in source
    usize len;            // byte length of the element value in source
};
```

Element values are always scalars in the current parser (objects and arrays as elements are stored by reference via their own `statement` or `value` structures).

#### Worked Example — Depth-First Object Walk

```c
// Source: config := { server := { host := localhost, port := 8080 }, timeout := 30 }

context ctx = /* … build + parse … */;
statement root = Context.get_statement(ctx, 0);

const char *raw = Source.data(ctx->source);

usize fc = Context.field_count(ctx, root);
for (usize i = 0; i < fc; i++) {
    field f = Context.get_field(ctx, root, i);
    printf("field: %.*s\n", (int)f->key_len, raw + f->key_pos);

    if (f->val && f->val->type == ANVL_VALUE_OBJECT) {
        // recurse — wrap in a synthetic stmt or walk field_list directly
        // (anvil.api will provide a helper for this; in core, access raw)
        usize inner_start = f->val->data.object.field_start;
        usize inner_count = f->val->data.object.field_count;
        for (usize j = 0; j < inner_count; j++) {
            field inner = ctx->field_list.fields[inner_start + j];
            printf("  field: %.*s\n", (int)inner->key_len, raw + inner->key_pos);
        }
    }
}
Context.dispose(ctx);
```

> **Note**: the inner-field direct access (`ctx->field_list.fields[…]`) is intentional for core consumers. `anvil.api` (post-v1.0) will wrap this in a cleaner recursive API.

### 9.5 anvil.api (deferred — post-v1.0)

Higher-level path helpers — `node["field"]["subfield"]`, jq-style selectors, jsonpath-style queries — will live in a separate `anvil.api` C package that sits on top of the primitives above. This keeps Anvil core free of policy. `anvil.api` is explicitly out of scope for v1.0.

---

## 8. This Document Is Law

Any deviation requires a signed confession and a public apology. And probably steak dinners for the team.

We are not building a library.  
We are not creating a new markup.
We are ending the debate.
We are ending the wars.

— Badkraft & Grok, December 2025
