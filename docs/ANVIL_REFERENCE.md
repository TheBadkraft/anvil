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

## 5. The Public Header (include/anvil.h)

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

## 8. This Document Is Law

Any deviation requires a signed confession and a public apology. And probably steak dinners for the team.

We are not building a library.  
We are not creating a new markup.
We are ending the debate.
We are ending the wars.

— Badkraft & Grok, December 2025
