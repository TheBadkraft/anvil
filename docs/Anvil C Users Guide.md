# Anvil — C Users Guide
**v0.5.5-alpha — E3 query path primitives, O(1) field-by-name lookup**

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
#include <anvil/anvil.h>
#include <anvil/context.h>
```

---

## Entry Points

```c
// 1. Get a builder
anvl_ctx_builder_i *b = Context.get_builder();

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
    const char    *id = Statement.identifier(stmt, ctx->source);
    printf("stmt[%zu]: %s  type=%d\n", i, id, t);
}
```

`Context.get_statement` returns `NULL` for out-of-range indices. Statement handles are
valid for the lifetime of the owning `context`.

### Statement types

| `anvl_stmt_type` | Meaning |
|---|---|
| `ANVL_STMT_ASSIGNMENT` | `key := value` |
| `ANVL_STMT_VARS` | `vars { … }` block |
| `ANVL_STMT_IMPORT` | `import "path"` declaration |

### Statement interface members

| Call | Returns | Purpose |
|---|---|---|
| `Statement.identifier(stmt, src)` | `const char *` | NUL-terminated identifier; pointer into source |
| `Statement.base(stmt, src)` | `const char *` | Inheritance base identifier; `NULL` if none |
| `Statement.type(stmt)` | `anvl_stmt_type` | Statement category |
| `Statement.value_type(stmt)` | `anvl_value_type` | Type of the statement's value |
| `Statement.length(stmt)` | `usize` | Source span length |

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

> **Performance**: O(1) after the first call. A FNV-1a hash map is built lazily on the
> first `get_field_by_name` call for a given statement and cached in `value_meta->name_index`
> for the lifetime of the context. `Context.dispose` releases all cached maps.

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

Scalar values are always zero-copy spans into the source buffer:

```c
// Statement-level scalar
statement s   = Context.get_statement(ctx, 0);
usize     pos = s->value_meta->data.scalar.pos;  // byte offset in source
usize     len = s->value_meta->data.scalar.len;  // byte length
const char *raw = Source.data(ctx->source);

// Copy to NUL-terminated string (caller owns)
char *val = strndup(raw + pos, len);

// Or parse in-place without allocation
long port = strtol(raw + pos, NULL, 10);
double f  = strtod(raw + pos, NULL);
bool  b   = (len == 4 && memcmp(raw + pos, "true", 4) == 0);
```

String values retain their surrounding `"` delimiters in the source span if quoted.
Strip them by adjusting `pos + 1` / `len - 2` when you need bare text.

---

## Source Interface

`Source` exposes the raw source buffer and character-level utilities:

```c
const char *raw  = Source.data(ctx->source);     // pointer to full source bytes
usize       slen = Source.length(ctx->source);   // total source byte length
anvl_dialect d   = Source.dialect(ctx->source);  // detected dialect
```

Substrings (allocates, caller owns):
```c
char *sub = Source.substring(ctx->source, pos, len);
// ... use sub ...
free(sub);
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

anvl_ctx_builder_i *b = Context.get_builder();
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

#include <anvil/anvil.h>
#include <anvil/context.h>
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
    anvl_ctx_builder_i *b = Context.get_builder();
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
        printf("%s :=\n", Statement.identifier(stmt, ctx->source));
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
    anvl_ctx_builder_i *b = Context.get_builder();
    if (!b->load_file(b, path)) return false;
    context ctx = b->build(b);
    if (!Context.parse(ctx)) { Context.dispose(ctx); return false; }

    const char *raw = Source.data(ctx->source);
    usize n = Context.statement_count(ctx);
    for (usize i = 0; i < n; i++) {
        statement s = Context.get_statement(ctx, i);
        const char *key = Statement.identifier(s, ctx->source);

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

## Named Field Lookup (O(1))

For point-in-time lookups on object statements, skip the index loop entirely:

```c
statement hero = Context.get_statement(ctx, 0);

field name_f   = Context.get_field_by_name(ctx, hero, "name",   4);
field health_f = Context.get_field_by_name(ctx, hero, "health", 6);
field weapon_f = Context.get_field_by_name(ctx, hero, "weapon", 6);

if (name_f)
    printf("name: %.*s\n", (int)name_f->val->data.scalar.len,
                             raw + name_f->val->data.scalar.pos);
```

The first call to `get_field_by_name` on a statement builds a FNV-1a hash map and caches
it in `value_meta->name_index`. All subsequent calls (including `get_field` by index)
benefit and the map is released automatically by `Context.dispose`.

---

## Linking

### Dynamic (`.so`)

```makefile
CFLAGS  += -I/usr/local/include/anvil
LDFLAGS += -L/usr/local/lib -lanvil
```

### Static / relocatable object (`.o`)

```sh
ld -r myapp.o /usr/local/packages/anvil.o -o myapp_bundle.o
# or link directly:
gcc main.o /usr/local/packages/anvil.o -o myapp
```

`anvil.o` is a fat relocatable object that bundles `sigma.memory`, `sigma.core.module`,
`sigma.core.text`, and `sigma.collections` — no additional `-l` flags needed for those
dependencies.

---

## Thread Safety

- **`context`** handles are **not thread-safe**. Each thread must own its own `context`.
- **`CtxBuilder`** is not thread-safe. Use `Context.get_builder()` (which returns a
  pointer to the global) — or maintain a per-thread builder.
- **`Anvil.error_get()`** is thread-local — safe to read from any thread without locking.
- **Module registration** (`sigma.core` module system) is handled at program startup via
  `__attribute__((constructor))` in `anvil_impl.c` — no explicit init call needed.

---

## API Quick Reference

### `Context` vtable

| Slot | Signature | Purpose |
|---|---|---|
| `get_builder` | `() → anvl_ctx_builder_i *` | Obtain the builder |
| `dialect` | `(ctx) → anvl_dialect` | Source dialect |
| `statement_count` | `(ctx) → usize` | Number of top-level statements |
| `get_statement` | `(ctx, i) → statement` | Statement by index |
| `attribute_count` | `(ctx) → usize` | Number of module-level attributes |
| `get_attribute` | `(ctx, i) → attribute` | Attribute by index |
| `parse` | `(ctx) → bool` | Run the parser |
| `dispose` | `(ctx)` | Release context and all arena memory |
| `field_count` | `(ctx, stmt) → usize` | Fields in an object statement |
| `get_field` | `(ctx, stmt, i) → field` | Field by index |
| `get_field_by_name` | `(ctx, stmt, name, len) → field` | Field by key — O(1) cached |
| `element_count` | `(ctx, stmt) → usize` | Elements in an array/tuple statement |
| `get_element` | `(ctx, stmt, i) → anvl_element_meta *` | Element by index |

### `Statement` vtable

| Slot | Signature | Purpose |
|---|---|---|
| `identifier` | `(stmt, src) → const char *` | Identifier string (NUL-terminated) |
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
| `substring` | `(src, pos, len) → char *` | Heap-allocated substring (caller frees) |

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
