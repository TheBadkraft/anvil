# Anvil — C Users Guide
**v0.5.4-alpha — standalone build, parser parity, vars, resolver, schema**

---

## Overview

The Anvil C library (`libanvil` / `anvil.o`) exposes a vtable-based interface. Every
subsystem is accessed through a global interface struct — `Anvil`, `Context`, `Statement`,
`Source` — whose function-pointer members are the public API surface. There are no loose
`anvl_*` functions to hunt for; each logical group lives in one place.

**Central type**: `context` — an opaque handle that owns a parsed document. All
parse-time allocations live in a per-context bump arena. Do not `free()` any handle
returned by the library; call `Context.dispose(ctx)` to release everything at once.

```c
#include "anvil.h"
#include "context.h"
```

---

## Entry Points

```c
// 1. Get a builder
ctx_builder b = Context.get_builder();

// 2. Supply a source — choose one:
b->set_source(b, src, len);           // parse from in-memory string (bytes + length)
b->load_file(b, "server.aml");        // load from file — dialect detected from shebang/ext

// 3. Build the context
context ctx = b->build(b);
if (!ctx) { /* allocation failure */ }

// 4. Parse
if (!Context.parse(ctx)) {
    const anvl_error_state *e = Anvil.error_get();
    fprintf(stderr, "parse error: %s\n", e->message);
    Context.dispose(ctx);
    return;
}

// 5. Use … (see sections below)

// 6. Release everything
Context.dispose(ctx);
```

`CtxBuilder` is a global default builder instance. `Context.get_builder()` returns a
pointer to it (stack-usable within a single call sequence). The builder is **not**
thread-safe — create one per thread if needed.

### Error handling

Errors are stored in a thread-local slot and accessed via `Anvil.error_get()`:

```c
const anvl_error_state *e = Anvil.error_get();
if (e && e->code != ANVL_ERR_NONE) {
    fprintf(stderr, "[%u] %s\n", e->code, e->message);
    Anvil.error_clear();
}
```

---

## Statement Navigation

After a successful `Context.parse()`, all top-level statements are available by index:

```c
usize n = Context.statement_count(ctx);
for (usize i = 0; i < n; i++) {
    statement stmt  = Context.get_statement(ctx, i);
    anvl_stmt_type t = Statement.type(stmt);
    char id[256] = {0};
    Statement.identifier(stmt, ctx->source, id);
    printf("stmt[%zu]: %s  type=%d\n", i, id, t);
}
```

`Context.get_statement` returns `NULL` for out-of-range indices. Statement handles are
valid for the lifetime of the owning `context`.

### Statement types

| `anvl_stmt_type`    | Meaning |
|---|---|
| `ANVL_STMT_ASSN`    | `key := value` (standard assignment) |
| `ANVL_ANON_OBJECT`  | `key { … }` (anonymous block, no `:=`) |
| `ANVL_STMT_FUNC`    | Function statement (AnvilScript) |
| `ANVL_STMT_MSSG`    | Message statement (AMP, future) |

### Statement interface members

| Call | Returns | Purpose |
|---|---|---|
| `Statement.identifier(stmt, src, out_buf)` | `void` | Writes NUL-terminated identifier into caller buffer |
| `Statement.base(stmt, src)` | `const char *` | Inheritance base identifier; `NULL` if none |
| `Statement.type(stmt)` | `anvl_stmt_type` | Statement category |
| `Statement.value_type(stmt)` | `anvl_value_type` | Type of the statement's value |
| `Statement.length(stmt)` | `usize` | Source span length |

`Statement.identifier` requires a caller-owned output buffer.

---

## Query Path Primitives (E3)

**Philosophy**: primitives, not policy. Anvil core provides minimal iteration building
blocks. Consumers compose whatever traversal fits — sequential scan, named lookup,
depth-first recursion, etc. A higher-level `anvil.api` package is planned for post-v1.0.

### Object Field Traversal

```c
usize Context.field_count(context self, statement stmt);
```
Number of key-value fields in an object-valued statement. Returns `0` if the statement's
value type is not `ANVL_VALUE_OBJECT`.

```c
field Context.get_field(context self, statement stmt, usize index);
```
Returns the `index`-th field (0-based), in source order. Returns `NULL` for out-of-range
or non-object statements.

```c
field Context.get_field_by_name(context self, statement stmt,
                                const char *name, usize len);
```
Returns the field whose key matches `name` (byte-exact, `len` bytes, case-sensitive, no
NUL required). Returns `NULL` if not found.

> **Performance note**: O(n) linear scan. The sigma.collections hash map is not yet
> linked in the standalone build. A future update will restore O(1) lookup when
> `sigma.collections` is available.

### Collection Element Traversal

```c
usize Context.element_count(context self, statement stmt);
```
Number of elements in an array- or tuple-valued statement. Returns `0` for other value types.

```c
struct anvl_element_meta *Context.get_element(context self, statement stmt, usize index);
```
Returns a pointer into the arena for the `index`-th element. Do not `free()` it.

### The `field` Structure

```c
struct anvl_field {
    usize key_pos;      // byte offset of the key in source
    usize key_len;      // byte length of the key
    usize attrib_start; // index into ctx->attr_list (field-level attributes)
    usize attrib_count; // number of field-level attributes
    value val;          // the field's value
};
```

Zero-copy key access:

```c
const char *raw = Source.data(ctx->source);
// key text: raw[f->key_pos .. f->key_pos + f->key_len]
printf("%.*s", (int)f->key_len, raw + f->key_pos);
```

### The `anvl_element_meta` Structure

```c
struct anvl_element_meta {
    anvl_value_type type; // type of this element
    usize pos;            // byte offset of the element value in source
    usize len;            // byte length of the element value in source
};
```

### Field Value Types

A `field->val` is a `value` pointer (`struct anvl_value *`). Dispatch on `val->type`:

```c
typedef enum {
    ANVL_VALUE_SCALAR,        // bare word, number, or string literal
    ANVL_VALUE_OBJECT,        // { key := val, … }
    ANVL_VALUE_ARRAY,         // [ elem, … ]
    ANVL_VALUE_TUPLE,         // ( elem, … )
    ANVL_VALUE_BLOB,          // ![ raw bytes ]
    ANVL_VALUE_VARREF,        // $identifier — unresolved variable reference
    ANVL_VALUE_INTERP_STRING, // $"…{ref}…" — interpolated string
} anvl_value_type;
```

```c
const char *raw = Source.data(ctx->source);

void handle_value(context ctx, value v) {
    switch (v->type) {
        case ANVL_VALUE_SCALAR:
            // zero-copy span: raw[v->data.scalar.pos .. +v->data.scalar.len]
            printf("%.*s", (int)v->data.scalar.len, raw + v->data.scalar.pos);
            break;
        case ANVL_VALUE_OBJECT: {
            usize start = v->data.object.field_start;
            usize count = v->data.object.field_count;
            for (usize i = 0; i < count; i++) {
                field f = ctx->field_list.fields[start + i];
                handle_value(ctx, f->val);
            }
            break;
        }
        case ANVL_VALUE_ARRAY:
        case ANVL_VALUE_TUPLE:
            // v->data.collection.element_count available;
            // for statement-level arrays use Context.get_element instead
            break;
        case ANVL_VALUE_VARREF:
            // identifier span (excludes '$'): raw[v->data.scalar.pos .. +v->data.scalar.len]
            break;
        case ANVL_VALUE_BLOB:
        case ANVL_VALUE_INTERP_STRING:
            // pos/len in source; interp segment list in value_meta (top-level stmts only)
            break;
    }
}
```

> **Collection fields**: when an object field's value is itself an array or tuple,
> `v->data.collection.element_count` gives the element count. Direct element access goes
> through `ctx->field_list` / element pool indices rather than `Context.get_element`,
> which is scoped to statement-level collections.

---

## Null-Safe Navigation Pattern

There is no `TryGet` in C — check return values directly:

```c
statement stmt = Context.get_statement(ctx, 0);
if (!stmt) return;

field host = Context.get_field_by_name(ctx, stmt, "host", 4);
if (!host) { /* key absent */ return; }

// scalar value
const char *raw = Source.data(ctx->source);
printf("host: %.*s\n", (int)host->val->data.scalar.len,
                        raw + host->val->data.scalar.pos);
```

Chain through nested objects by descending into sub-fields directly:

```c
// config := { server := { host := localhost, port := 8080 } }
statement config = Context.get_statement(ctx, 0);
field server_f   = Context.get_field_by_name(ctx, config, "server", 6);
if (!server_f || !server_f->val || server_f->val->type != ANVL_VALUE_OBJECT) return;

// drop into the nested object via field_list directly
usize start = server_f->val->data.object.field_start;
usize count = server_f->val->data.object.field_count;
for (usize i = 0; i < count; i++) {
    field f = ctx->field_list.fields[start + i];
    printf("  %.*s\n", (int)f->key_len, raw + f->key_pos);
}
```

---

## Scalar Access

Scalar values are always zero-copy spans into the source buffer.
**Since v0.5.0 the parser strips surrounding quotes** — `pos/len` points directly
at the content, not the `"` delimiters:

```c
// Statement-level scalar
statement s   = Context.get_statement(ctx, 0);
usize     pos = s->value_meta->data.scalar.pos;  // byte offset in source (after opening quote)
usize     len = s->value_meta->data.scalar.len;  // byte length (excludes both quotes)
const char *raw = Source.data(ctx->source);

// For  name := "Alice"  → raw[pos..pos+len] is "Alice" (5 bytes, no quotes)

// Copy to NUL-terminated string (caller owns)
char *val = strndup(raw + pos, len);

// Or parse in-place without allocation
long   port = strtol(raw + pos, NULL, 10);
double f    = strtod(raw + pos, NULL);
bool   b    = (len == 4 && memcmp(raw + pos, "true", 4) == 0);
```

Unquoted scalars (bare words, numbers, booleans) are unchanged — `pos/len` spans
the token directly, as before.

---

## Source Interface

`Source` exposes the raw source buffer and character-level utilities:

```c
const char *raw  = Source.data(ctx->source);     // pointer to full source bytes
usize       slen = Source.length(ctx->source);   // total source byte length
anvl_dialect d   = Source.dialect(ctx->source);  // detected dialect
```

Substrings (caller-supplied buffer):
```c
char sub[128];
Source.substring(ctx->source, pos, len, sub);
// ... use sub ...
```

---

## Attributes

Attributes on top-level statements are stored in `ctx->attr_list`:

```c
usize n = Context.attribute_count(ctx);
for (usize i = 0; i < n; i++) {
    attribute a   = Context.get_attribute(ctx, i);
    const char *raw = Source.data(ctx->source);

    // key span: raw[a->key_pos .. +a->key_len]
    printf("@%.*s", (int)a->key_len, raw + a->key_pos);

    if (a->has_value)
        printf("=%.*s", (int)a->value_len, raw + a->value_pos);
    printf("\n");
}
```

Field-level attributes follow the same layout via `f->attrib_start` and `f->attrib_count`
as indices into `ctx->attr_list`.

---

## Collections (Arrays and Tuples)

```c
// tags := [ alpha, beta, gamma ]
statement stmt = Context.get_statement(ctx, 0);

if (Statement.value_type(stmt) == ANVL_VALUE_ARRAY ||
    Statement.value_type(stmt) == ANVL_VALUE_TUPLE)
{
    usize n = Context.element_count(ctx, stmt);
    for (usize i = 0; i < n; i++) {
        struct anvl_element_meta *el = Context.get_element(ctx, stmt, i);
        printf("[%zu]: %.*s\n", i, (int)el->len, raw + el->pos);
    }
}
```

---

## Dialect Detection

```c
anvl_dialect d = Source.dialect(ctx->source);
switch (d) {
    case ANVL_DIALECT_AML: /* config/data files */ break;
    case ANVL_DIALECT_ASL: /* scripting files   */ break;
    case ANVL_DIALECT_AMP: /* messaging payloads*/ break;
    default: break;
}
```

`b->load_file` detects dialect from the shebang line or file extension automatically.
`b->set_dialect` forces an override before `build()`.

---

## AMP Messaging

AMP (`#!amp`) is the restricted messaging dialect. It allows only scalar values, scalar
arrays/tuples, and blobs — objects are forbidden.

```c
// #!amp
// type    := "player.join"
// version := 2
// ids     := [101, 204, 387]

ctx_builder b = Context.get_builder();
b->set_source(b, amp_src, amp_len);
context ctx = b->build(b);
Context.parse(ctx);

statement type_s = Context.get_statement(ctx, 0);  // type
statement ver_s  = Context.get_statement(ctx, 1);  // version
statement ids_s  = Context.get_statement(ctx, 2);  // ids

// scalar: raw span
const char *raw = Source.data(ctx->source);
usize p = type_s->value_meta->data.scalar.pos;
usize l = type_s->value_meta->data.scalar.len;
printf("type: %.*s\n", (int)l, raw + p);

// array
usize n = Context.element_count(ctx, ids_s);
for (usize i = 0; i < n; i++) {
    struct anvl_element_meta *el = Context.get_element(ctx, ids_s, i);
    long id = strtol(raw + el->pos, NULL, 10);
    printf("id[%zu] = %ld\n", i, id);
}

Context.dispose(ctx);
```

---

## Worked Example — Depth-First Object Walk

```c
// Source: config := { server := { host := localhost, port := 8080 }, timeout := 30 }

#include "anvil.h"
#include "context.h"
#include <stdio.h>

static void walk_fields(context ctx, usize field_start, usize field_count,
                        const char *raw, int depth)
{
    for (usize i = 0; i < field_count; i++) {
        field f = ctx->field_list.fields[field_start + i];
        printf("%*s%.*s", depth * 2, "", (int)f->key_len, raw + f->key_pos);

        if (f->val && f->val->type == ANVL_VALUE_OBJECT) {
            printf(" := {\n");
            walk_fields(ctx, f->val->data.object.field_start,
                             f->val->data.object.field_count, raw, depth + 1);
            printf("%*s}\n", depth * 2, "");
        } else if (f->val) {
            printf(" := %.*s\n",
                   (int)f->val->data.scalar.len,
                   raw + f->val->data.scalar.pos);
        }
    }
}

int main(void) {
    ctx_builder b = Context.get_builder();
    const char *src = "config := { server := { host := localhost, port := 8080 }, timeout := 30 }";
    b->set_source(b, src, strlen(src));
    context ctx = b->build(b);
    if (!Context.parse(ctx)) {
        fprintf(stderr, "%s\n", Anvil.error_get()->message);
        Context.dispose(ctx);
        return 1;
    }

    const char *raw = Source.data(ctx->source);
    usize n = Context.statement_count(ctx);
    for (usize i = 0; i < n; i++) {
        statement stmt = Context.get_statement(ctx, i);
        char ident[256] = {0};
        Statement.identifier(stmt, ctx->source, ident);
        printf("%s :=\n", ident);
        walk_fields(ctx, stmt->value_meta->data.object.field_start,
                         stmt->value_meta->data.object.field_count, raw, 1);
    }

    Context.dispose(ctx);
    return 0;
}
```

Output:
```
config :=
  server := {
    host := localhost
    port := 8080
  }
  timeout := 30
```

---

## Worked Example — Typed Config Reader

```c
// server.aml:
// #!aml
// host    := localhost
// port    := 8080
// debug   := false
// tags    := [survival, pvp]

typedef struct {
    char  host[256];
    long  port;
    bool  debug;
    char *tags[16];
    usize tag_count;
} server_config;

static bool load_config(const char *path, server_config *out) {
    ctx_builder b = Context.get_builder();
    if (!b->load_file(b, path)) return false;
    context ctx = b->build(b);
    if (!Context.parse(ctx)) { Context.dispose(ctx); return false; }

    const char *raw = Source.data(ctx->source);
    usize n = Context.statement_count(ctx);
    for (usize i = 0; i < n; i++) {
        statement s = Context.get_statement(ctx, i);
        char key[128] = {0};
        Statement.identifier(s, ctx->source, key);

        if (strcmp(key, "host") == 0) {
            usize p = s->value_meta->data.scalar.pos;
            usize l = s->value_meta->data.scalar.len;
            snprintf(out->host, sizeof(out->host), "%.*s", (int)l, raw + p);

        } else if (strcmp(key, "port") == 0) {
            usize p = s->value_meta->data.scalar.pos;
            out->port = strtol(raw + p, NULL, 10);

        } else if (strcmp(key, "debug") == 0) {
            usize p = s->value_meta->data.scalar.pos;
            usize l = s->value_meta->data.scalar.len;
            out->debug = (l == 4 && memcmp(raw + p, "true", 4) == 0);

        } else if (strcmp(key, "tags") == 0) {
            usize cnt = Context.element_count(ctx, s);
            out->tag_count = cnt < 16 ? cnt : 16;
            for (usize j = 0; j < out->tag_count; j++) {
                struct anvl_element_meta *el = Context.get_element(ctx, s, j);
                out->tags[j] = strndup(raw + el->pos, el->len);  // caller frees
            }
        }
    }

    Context.dispose(ctx);
    return true;
}
```

---

## Using Declarations

`using` imports an AnvilScript module into the document and escalates the dialect from
AML → ASL. It must appear before `vars` and all statements.

```c
// Source: #!aml\nusing sys_math\nx := 1
context ctx = /* parse … */;

// How many usings were declared?
usize n = ctx->using_list.count;
for (usize i = 0; i < n; i++) {
    struct anvl_using_decl *u = &ctx->using_list.decls[i];
    const char *raw = Source.data(ctx->source);
    printf("using: %.*s\n", (int)u->name_len, raw + u->name_pos);
}

// Dialect is now ASL after a using
// Source.dialect(ctx->source) == ANVL_DIALECT_ASL
```

`using` in `#!amp` produces error `ANVL_ERR_USING_IN_AMP` (4306).
`using` after vars/statements produces error `ANVL_ERR_USING_AFTER_STATEMENTS` (4307).

---

## Anonymous Blocks

An anonymous block is a top-level read-only object declared without `:=`.
Optional `@[attrs]` may appear between the identifier and `{`:

```anvl
// Plain anonymous block
defaults { hp := 100, speed := 5 }

// Decorated anonymous block
defaults @[tier=1, immutable] { hp := 100, speed := 5 }
```

```c
statement stmt = Context.get_statement(ctx, 0);
// stmt->meta[STMT_META_TYPE] == ANVL_ANON_OBJECT

// Attributes (if any)
usize attr_count = stmt->meta[STMT_META_ATTR_IDX];
if (attr_count > 0 && stmt->attr_meta) {
    const char *raw = Source.data(ctx->source);
    for (usize i = 0; i < attr_count; i++) {
        printf("@%.*s\n", (int)stmt->attr_meta[i].len,
                           raw + stmt->attr_meta[i].pos);
    }
}
```

Inheritance from an anonymous block is rejected at resolve time
(`ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS`).

---

## Resolver API (`anvil.resolver.o`)

The resolver handles single-inheritance — `derived : base := { … }` — using
Kahn's topological sort for cycle detection and lazy per-statement merge cache.

```c
#include "resolver.h"

// Build resolver state from a parsed context
anvl_node_state_t *state = anvl_resolver_build_state(ctx, ctx->source);
if (!state) {
    if (Anvil.error_is_set()) {
        // ANVL_ERR_RESOLVER_CYCLE_DETECTED or ANVL_ERR_MEMORY_ALLOCATION_FAILED
        fprintf(stderr, "resolver: %s\n", Anvil.error_get()->message);
    }
    // NULL with no error = fast-path (no inheritance in this document)
    return;
}

// Get merged fields for a statement (lazy, cached)
// stmtIdx = index in ctx->stmt_list
const anvl_field_list_t *merged = anvl_node_state_get_merged_fields(state, stmtIdx);
if (!merged) {
    // ANVL_ERR_RESOLVER_MISSING_BASE or ANVL_ERR_CANNOT_INHERIT_FROM_ANONYMOUS
    return;
}

const char *raw = Source.data(ctx->source);
for (usize i = 0; i < merged->count; i++) {
    field f = merged->fields[i];
    printf("  %.*s\n", (int)f->key_len, raw + f->key_pos);
}

// Get own (unmerged) fields only
const anvl_field_list_t *own = anvl_node_state_get_own_fields(state, stmtIdx);

// Get base statement index
usize baseIdx = anvl_node_state_get_base_index(state, stmtIdx);
// Returns (usize)-1 if no base

// Pre-warm all merges (optional)
anvl_node_state_warm_all(state);

// Always dispose when done
anvl_node_state_dispose(state);
```

**Custom merge policy** — control how fields are merged per field pair:

```c
field my_policy(context ctx, source src,
                field base_field, field derived_field, void *userdata) {
    // Return derived_field to override base (default behaviour)
    // Return base_field to keep base
    // Return NULL to exclude the field entirely
    return derived_field ? derived_field : base_field;
}

const anvl_field_list_t *merged =
    anvl_node_state_get_merged_fields_custom(state, stmtIdx, my_policy, NULL);
```

---

## Vars API (`anvil.vars.o`)

`Vars` is an explicit post-parse layer for var-ref resolution and interpolation
materialization. Parser output remains unresolved until you call this API.

```c
#include "vars.h"

anvl_vars_state_t *vs = Vars.build(ctx);
if (!vs) {
    // ANVL_ERR_VARS_CIRCULAR_REF or allocation failure
    fprintf(stderr, "vars: %s\n", Anvil.error_get()->message);
    return;
}

// Resolve a flat key
usize rpos, rlen;
anvl_value_type rtype;
if (Vars.resolve(vs, "atlas", 5, &rpos, &rlen, &rtype)) {
    const char *raw = Source.data(ctx->source);
    printf("atlas = %.*s\n", (int)rlen, raw + rpos);
}

// Resolve a dotted path (single-level supported today)
if (Vars.resolve_path(vs, ctx, "changelog.version", 17, &rpos, &rlen, &rtype)) {
    const char *raw = Source.data(ctx->source);
    printf("changelog.version = %.*s\n", (int)rlen, raw + rpos);
}

// Materialize interpolated string from statement value_meta
statement s = Context.get_statement(ctx, 0);
char *expanded = Vars.materialise_interp(vs, ctx, s->value_meta);
if (expanded) {
    printf("interp = %s\n", expanded);
    Allocator.dispose(expanded);
}

Vars.dispose(vs);
```

Notes:
- `Vars.resolve` and `Vars.resolve_path` set `ANVL_ERR_VARS_KEY_NOT_FOUND` for missing references.
- `Vars.resolve_path` currently supports one dot segment (`a.b`); deeper traversal (`a.b.c`) is still WIP.
- `Vars.materialise_interp` expects `ANVL_VALUE_INTERP_STRING` metadata and returns heap memory owned by caller.

---

## Schema API (`anvil.schema.o`)

Schema documents use `@[schema]` as a module-level attribute to declare types.
The schema module resolves type rules and validates data documents against them.

```c
#include "schema.h"

// 1. Parse the schema document (.asch or any .anvl with @[schema])
context schema_ctx = /* parse "vehicle.asch" … */;

// 2. Resolve type rules from the schema
anvl_schema_ruleset_t *rules = Schema.resolve(schema_ctx);
if (!rules) {
    // ANVL_ERR_SCHEMA_ATTR_MISSING (4601) — missing @[schema]
    // ANVL_ERR_SCHEMA_BASE_UNKNOWN (4603) — unrecognised base in schema
    return;
}

// 3. Look up a type by name
anvl_schema_type_t *t = Schema.get_type(rules, "VehicleRecord");
if (t) {
    printf("kind: %d  fields: %d\n", t->kind, t->field_count);
    // ANVL_SCHEMA_OBJECT=0, ANVL_SCHEMA_ENUM=1, ANVL_SCHEMA_FLAGS=2
    for (int i = 0; i < t->field_count; i++)
        printf("  field: %s\n", t->fields[i].name);
}

// 4. Validate a data document against the ruleset
context data_ctx = /* parse data document … */;
anvl_schema_result_t *result = Schema.validate(rules, data_ctx, "vehicle.anvl");
if (!result->is_valid) {
    for (int i = 0; i < result->error_count; i++)
        fprintf(stderr, "[%d] %s\n", result->errors[i].code,
                                     result->errors[i].message);
}

// 5. Free (always, even on is_valid=true)
Schema.result_free(result);
Schema.ruleset_free(rules);
```

**Schema type kinds:**

| Kind | Declaration | Description |
|---|---|---|
| `ANVL_SCHEMA_OBJECT` | `Type := { field value }` | Structured type with named fields |
| `ANVL_SCHEMA_ENUM` | `Type : enum := (a, b, c)` | Closed set of scalar values |
| `ANVL_SCHEMA_FLAGS` | `Type : flags := (r, w, x)` | Composable flag set |

**Validation error codes:**

| Code | Constant | Meaning |
|---|---|---|
| 4601 | `ANVL_ERR_SCHEMA_ATTR_MISSING` | Document lacks `@[schema]` |
| 4603 | `ANVL_ERR_SCHEMA_BASE_UNKNOWN` | Schema base not `enum`/`flags` |
| 4604 | `ANVL_ERR_SCHEMA_VALIDATION_REQUIRED` | Required field absent in data |
| 4605 | `ANVL_ERR_SCHEMA_VALIDATION_TYPE_MISMATCH` | Field type does not match declared type |

---

### FlyWire Pipeline (Current C Support)

For FlyWire, the current C schema stack already supports the core validation flow:

1. Parse and resolve one `.asch` schema into a reusable `anvl_schema_ruleset_t`.
2. Parse each data document (`.aml` / `.anvl`) and validate against that ruleset.
3. Collect per-file errors from `anvl_schema_result_t` and surface them in your pipeline output.

```c
static bool validate_one(anvl_schema_ruleset_t *rules, const char *file_path) {
    ctx_builder b = Context.get_builder();
    if (!b->load_file(b, file_path))
        return false;

    context data_ctx = b->build(b);
    if (!data_ctx || !Context.parse(data_ctx)) {
        Context.dispose(data_ctx);
        return false;
    }

    anvl_schema_result_t *r = Schema.validate(rules, data_ctx, file_path);
    bool ok = (r && r->is_valid);

    if (r && !r->is_valid) {
        for (int i = 0; i < r->error_count; i++) {
            fprintf(stderr, "%s: [%d] %s\n",
                    file_path,
                    r->errors[i].code,
                    r->errors[i].message);
        }
    }

    Schema.result_free(r);
    Context.dispose(data_ctx);
    return ok;
}
```

Current boundary notes:
- Schema core support is shipped and tested.
- The roadmap FlyWire milestone is about formalized pipeline tooling/workflow layers, not missing base schema primitives.

---

---

## Building (Standalone — Zero External Dependencies)

As of v0.5.0 anvil embeds all sigma dependencies. No `-l` flags needed.

```sh
# Core parser only
gcc -std=c2x -I./include \
    src/core/anvil.c src/core/context.c src/core/errors.c \
    src/core/memory.c src/core/parser.c src/core/strings.c \
    src/utilities/utils.c \
    main.c -o myapp

# With resolver and vars (default lib)
gcc -std=c2x -I./include \
    src/core/anvil.c src/core/context.c src/core/errors.c \
    src/core/memory.c src/core/parser.c src/core/strings.c \
    src/utilities/utils.c \
    src/resolver/resolver.c src/vars/vars.c \
    main.c -o myapp

# With schema (add on top of default lib)
gcc -std=c2x -I./include ... src/schema/schema.c main.c -o myapp
```

All includes resolve from `./include` — no installed packages required.

---

## Thread Safety

- **`context`** handles are **not thread-safe**. Each thread must own its own `context`.
- **`CtxBuilder`** is not thread-safe. Use `Context.get_builder()` once per sequence
  or maintain a per-thread builder.
- **`Anvil.error_get()`** is thread-local — safe to read from any thread without locking.
- No global init call is required — the standalone build has no module registration.

---

## API Quick Reference

### `Context` vtable

| Slot | Signature | Purpose |
|---|---|---|
| `get_builder` | `() → ctx_builder` | Obtain the builder |
| `dialect` | `(ctx) → anvl_dialect` | Source dialect |
| `statement_count` | `(ctx) → usize` | Number of top-level statements |
| `get_statement` | `(ctx, i) → statement` | Statement by index |
| `attribute_count` | `(ctx) → usize` | Number of module-level attributes |
| `get_attribute` | `(ctx, i) → attribute` | Attribute by index |
| `parse` | `(ctx) → bool` | Run the parser |
| `dispose` | `(ctx)` | Release context and all arena memory |
| `field_count` | `(ctx, stmt) → usize` | Fields in an object statement |
| `get_field` | `(ctx, stmt, i) → field` | Field by index |
| `get_field_by_name` | `(ctx, stmt, name, len) → field` | Field by key — linear scan |
| `element_count` | `(ctx, stmt) → usize` | Elements in an array/tuple statement |
| `get_element` | `(ctx, stmt, i) → anvl_element_meta *` | Element by index |
| `value_element_count` | `(ctx, val) → usize` | Elements in nested array/tuple field values |
| `get_value_element` | `(ctx, val, i) → anvl_element_meta *` | Nested element by index |
| `value_field_count` | `(ctx, val) → usize` | Fields in nested object field values |
| `get_value_field` | `(ctx, val, i) → field` | Nested field by index |
| `get_value_field_by_name` | `(ctx, val, name, len) → field` | Nested field by key |
| `stmt_attr_count` | `(ctx, stmt) → usize` | Statement attribute count |
| `get_stmt_attr` | `(ctx, stmt, i) → anvl_attr_meta *` | Statement attribute by index |
| `get_stmt_attr_by_name` | `(ctx, stmt, name, len) → anvl_attr_meta *` | Statement attribute by key |
| `field_attr_count` | `(ctx, field) → usize` | Field attribute count |
| `get_field_attr` | `(ctx, field, i) → attribute` | Field attribute by index |
| `get_field_attr_by_name` | `(ctx, field, name, len) → attribute` | Field attribute by key |

### `Statement` vtable

| Slot | Signature | Purpose |
|---|---|---|
| `identifier` | `(stmt, src, out_buf) → void` | Writes identifier string to caller buffer |
| `base` | `(stmt, src) → const char *` | Base/parent identifier; `NULL` if none |
| `type` | `(stmt) → anvl_stmt_type` | Statement category |
| `value_type` | `(stmt) → anvl_value_type` | Value type |
| `length` | `(stmt) → usize` | Source span length |

### `Source` vtable (selected)

| Slot | Signature | Purpose |
|---|---|---|
| `data` | `(src) → const char *` | Pointer to raw source bytes |
| `length` | `(src) → usize` | Total source byte length |
| `dialect` | `(src) → anvl_dialect` | Detected dialect |
| `substring` | `(src, pos, len, out_buf) → void` | Copy span into caller buffer |

### `anvl_value_type` enum

| Value | Meaning |
|---|---|
| `ANVL_VALUE_SCALAR` | Bare word, number, or string literal |
| `ANVL_VALUE_OBJECT` | `{ key := val, … }` |
| `ANVL_VALUE_ARRAY` | `[ elem, … ]` |
| `ANVL_VALUE_TUPLE` | `( elem, … )` |
| `ANVL_VALUE_BLOB` | `` ![ raw bytes ] `` |
| `ANVL_VALUE_VARREF` | `$identifier` — unresolved variable reference |
| `ANVL_VALUE_INTERP_STRING` | `$"…{ref}…"` — interpolated string |

---

*For the higher-level .NET API, see [Anvil.Net Users Guide.md](Anvil.Net%20Users%20Guide.md).  
For the full internal specification, see [reference.md](reference.md).*
