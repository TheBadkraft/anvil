# Anvil — Public API Bindings Reference
**v0.5.4-alpha · For binding authors and N-API implementors**

> Pinned maintenance note:
> Official bindings live under `bindings/*/` in this repository.
> Maintenance policy: `docs/maintainers/bindings-maintenance.md`
> Sign-off checklist: `docs/maintainers/bindings-signoff-checklist.md`
> Handoff manifest: `make -C bindings contract` → `bindings/.handoff/binding-handoff.json`

This document defines the complete public API surface exposed by the Anvil C library,
the N-API contract between the C layer and JavaScript, and JavaScript usage examples.
Additional language paradigms (Python, .NET, Java) will be added as bindings are built.

---

## 1. C Public API

All public interfaces follow the singleton vtable convention:

```c
extern const anvl_X_i X;   // e.g. extern const anvl_context_i Context;
X.method(args);             // e.g. Context.parse(ctx);
```

Internal functions are `static`. Nothing outside a `.c` file is accessible except
through the declared vtable singletons and the `extern` type definitions in headers.

### 1.1 `Anvil` — Global entry point

```c
// include/anvil.h
extern const anvl_i Anvil;

typedef struct anvl_i {
    root  (*load)       (const char *filepath);
    root  (*read)       (const char *source, usize len);
    void  (*dispose)    (root r);
    void  (*cleanup)    (void);
    bool  (*error_is_set)(void);
    const anvl_error_state *(*error_get)(void);
    void  (*error_clear)(void);
    const char *(*get_version)(void);
} anvl_i;
```

| Method | Description |
|--------|-------------|
| `load(filepath)` | Load and parse a file; dialect from shebang |
| `read(source, len)` | Parse in-memory bytes; dialect from shebang |
| `dispose(root)` | Release a root handle |
| `cleanup()` | Reset global builder state |
| `error_is_set()` | True if a parse error occurred |
| `error_get()` | Returns current error state (code, message, line, column) |
| `error_clear()` | Reset error state |
| `get_version()` | Return runtime version string (`major.minor.patch+build-tag`) |

### 1.2 `Context` — Document context

```c
// include/context.h
extern const anvl_context_i Context;

typedef struct anvl_context_i {
    ctx_builder          (*get_builder)(void);
    anvl_dialect        (*dialect)         (context self);
    usize               (*statement_count) (context self);
    statement           (*get_statement)   (context self, usize index);
    bool                (*add_statement)   (context self, statement stmt);
    usize               (*attribute_count) (context self);
    attribute           (*get_attribute)   (context self, usize index);
    bool                (*add_attribute)   (context self, attribute attr);
    void                (*dispose)         (context self);
    bool                (*parse)           (context self);
    // Query path primitives
    statement           (*get_statement_by_name)(context self, const char *name, usize len);
    usize               (*field_count)     (context self, statement stmt);
    field               (*get_field)       (context self, statement stmt, usize index);
    field               (*get_field_by_name)(context self, statement stmt, const char *name, usize len);
    usize               (*element_count)   (context self, statement stmt);
    struct anvl_element_meta *(*get_element)(context self, statement stmt, usize index);
} anvl_context_i;
```

**Context builder:**
```c
extern anvl_ctx_builder_i CtxBuilder;

typedef struct anvl_ctx_builder_i {
    anvl_dialect dialect;
    source       source;
    void    (*set_dialect)(struct anvl_ctx_builder_i *self, anvl_dialect dialect);
    void    (*set_source) (struct anvl_ctx_builder_i *self, const char *source, usize len);
    bool    (*load_file)  (struct anvl_ctx_builder_i *self, const char *filepath);
    context (*build)      (struct anvl_ctx_builder_i *self);
    void    (*dispose)    (struct anvl_ctx_builder_i *self);
} anvl_ctx_builder_i;
```

### 1.3 `Statement` — Top-level statement

```c
extern const anvl_statement_i Statement;

typedef struct anvl_statement_i {
    void          (*identifier)(statement self, source src, char *out_identifier);
    const char   *(*base)      (statement self, source src);
    anvl_stmt_type(*type)      (statement self);
    anvl_value_type(*value_type)(statement self);
    usize         (*length)    (statement self);
} anvl_statement_i;
```

**Statement type enum:**
```c
typedef enum {
    ANVL_STMT_ASSN,    // key := value
    ANVL_STMT_FUNC,    // function (AnvilScript)
    ANVL_STMT_MSSG,    // message (AMP, future)
    ANVL_ANON_OBJECT   // key { … } — read-only anonymous block
} anvl_stmt_type;
```

**Direct struct access** (for binding authors):
```c
struct anvl_statement {
    usize meta[9];                     // indexed by STMT_META_* constants
    struct anvl_base_meta  *base_meta; // NULL if no inheritance
    struct anvl_attr_meta  *attr_meta; // NULL if no attributes
    struct anvl_value_meta *value_meta;
};

// Metadata index constants
#define STMT_META_TYPE      0   // anvl_stmt_type
#define STMT_META_IDENT_POS 2   // identifier byte offset in source
#define STMT_META_IDENT_LEN 3   // identifier byte length
#define STMT_META_ATTR_IDX  5   // attribute count
#define STMT_META_VALUE_IDX 7   // 1 if value_meta is present
```

### 1.4 `Source` — Raw source buffer

```c
extern const anvl_source_i Source;

// Selected methods:
const char   *Source.data     (source s);          // pointer to raw bytes
usize         Source.length   (source s);          // total byte count
anvl_dialect  Source.dialect  (source s);          // detected dialect
```

### 1.5 Value types

```c
typedef enum {
    ANVL_VALUE_SCALAR,         // bare word, number, quoted string
    ANVL_VALUE_OBJECT,         // { key := val, … }
    ANVL_VALUE_ARRAY,          // [ elem, … ]
    ANVL_VALUE_TUPLE,          // ( elem, … )
    ANVL_VALUE_BLOB,           // @tag`…` or `…`
    ANVL_VALUE_VARREF,         // $identifier
    ANVL_VALUE_INTERP_STRING,  // $"…{ref}…"
} anvl_value_type;
```

**Scalar value access** (quote-stripped since v0.5.0):
```c
// value_meta->pos/len points INSIDE the quotes for "quoted" strings.
// For unquoted scalars, points to the token directly.
struct anvl_value_meta {
    anvl_value_type type;
    usize pos;   // byte offset into source (after opening quote if quoted)
    usize len;   // byte length (excludes both quotes if quoted)
    union {
        struct { usize field_count; usize field_start; } object;
        struct { usize element_count; struct anvl_element_meta *elements; } collection;
        struct { usize segment_count; struct anvl_interp_segment *segments; } interp_string;
    } data;
};

struct anvl_element_meta {
    anvl_value_type type;
    usize pos;   // quote-stripped for SCALAR elements
    usize len;
};
```

### 1.6 Dialects

```c
typedef enum {
    ANVL_DIALECT_AML,    // #!aml — declarative data
    ANVL_DIALECT_ASL,    // #!asl — scripting (implemented alpha)
    ANVL_DIALECT_AMP,    // #!amp — messaging
    ANVL_DIALECT_AURORA  // generic
} anvl_dialect;
```

### 1.7 Error codes (selected)

| Code | Constant | Meaning |
|------|----------|---------|
| 0    | `ANVL_ERR_NONE` | No error |
| 4001–4099 | Parser errors | Expected identifier, assign, etc. |
| 4201–4207 | Import errors | Path not found, cycle, etc. |
| 4305–4307 | Using errors | AMP forbidden, after statements, etc. |
| 4401 | `ANVL_ERR_AMP_ARRAY_ELEMENT_NOT_SCALAR` | Nested value in AMP collection |
| 4601–4606 | Schema errors | Missing attr, type mismatch, required field, etc. |

---

## 2. Resolver API (`anvil.resolver.o`)

```c
// include/resolver.h

anvl_node_state_t *anvl_resolver_build_state  (context ctx, source src);
const anvl_field_list_t *anvl_node_state_get_merged_fields (anvl_node_state_t *state, usize stmt_idx);
const anvl_field_list_t *anvl_node_state_get_own_fields    (anvl_node_state_t *state, usize stmt_idx);
const anvl_field_list_t *anvl_node_state_get_merged_fields_custom(anvl_node_state_t *state, usize stmt_idx, anvl_merge_policy_fn policy, void *userdata);
usize                    anvl_node_state_get_base_index    (anvl_node_state_t *state, usize stmt_idx);
bool                     anvl_node_state_warm_all          (anvl_node_state_t *state);
void                     anvl_node_state_dispose           (anvl_node_state_t *state);

typedef field (*anvl_merge_policy_fn)(context ctx, source src,
                                      field base_field, field derived_field,
                                      void *userdata);
```

`build_state` returns `NULL` with no error when no inheritance exists (fast-path).
`build_state` returns `NULL` with `ANVL_ERR_RESOLVER_CYCLE_DETECTED` on cycles.

---

## 3. Schema API (`anvil.schema.o`)

```c
// include/schema.h
extern const anvl_schema_i Schema;

typedef struct anvl_schema_i {
    anvl_schema_ruleset_t *(*resolve)     (context schema_ctx);
    anvl_schema_result_t  *(*validate)    (anvl_schema_ruleset_t *rules, context data_ctx, const char *file_path);
    anvl_schema_type_t    *(*get_type)    (anvl_schema_ruleset_t *rules, const char *name);
    void                   (*ruleset_free)(anvl_schema_ruleset_t *rules);
    void                   (*result_free) (anvl_schema_result_t  *result);
} anvl_schema_i;
```

---

## 4. N-API Contract (`n-anvl-binder`)

The N-API addon (`n-anvl-binder.node`) exposes a minimal surface.
The JS library (`anvil.js`) builds `AnvilNode` objects on top of this.

### 4.1 Exported functions

```js
const binder = require('n-anvl-binder');

// Parse source string → plain JS object tree, or null on error
binder.parse(source: string, path?: string): AnvilTree | null

// Parse file → plain JS object tree, or null on error
binder.load(filePath: string): AnvilTree | null

// Last error from parse/load
binder.lastError(): { message: string, line: number, column: number, path: string | null } | null
```

### 4.2 `AnvilTree` — the plain JS object returned by the binder

```ts
// Root object (top-level document)
interface AnvilTree {
    dialect: 'aml' | 'amp' | 'asl';
    attrs:   Record<string, string | null>;  // module-level @[attrs]
    stmts:   AnvilStmt[];
}

interface AnvilStmt {
    name:    string;              // identifier
    base:    string | null;       // inheritance base name, if any
    type:    'assign' | 'anon';   // ANVL_STMT_ASSN or ANVL_ANON_OBJECT
    attrs:   Record<string, string | null>;  // statement-level @[attrs]
    value:   AnvilValue;
    // Populated by resolver (when anvil.resolver.o is linked):
    merged?: AnvilField[];        // merged fields including inherited
}

type AnvilValue =
    | { kind: 'scalar';  text: string }          // quote-stripped content
    | { kind: 'object';  fields: AnvilField[] }
    | { kind: 'array';   elements: AnvilValue[] }
    | { kind: 'tuple';   elements: AnvilValue[] }
    | { kind: 'blob';    tag: string | null; content: string }
    | { kind: 'varref';  path: string }           // $identifier
    | { kind: 'interp';  segments: AnvilSegment[] }

interface AnvilField {
    key:   string;
    attrs: Record<string, string | null>;
    value: AnvilValue;
}

interface AnvilSegment {
    isRef: boolean;
    text:  string;  // literal span or identifier name
}
```

### 4.3 N-API implementation sketch

```c
// n-anvl-binder/src/binder.c
#include <node_api.h>
#include "anvil.h"
#include "context.h"

// Build a JS AnvilValue from a C value_meta + source
static napi_value value_to_js(napi_env env, context ctx,
                               struct anvl_value_meta *vm);

// Registered as module.exports.parse
static napi_value napi_parse(napi_env env, napi_callback_info info) {
    // 1. Extract source string arg from JS
    // 2. CtxBuilder.set_source / build / Context.parse
    // 3. Build AnvilTree JS object from ctx->stmt_list
    // 4. Context.dispose(ctx)
    // 5. Return JS object or null
}

NAPI_MODULE_INIT() {
    napi_value parse_fn, load_fn, last_error_fn;
    napi_create_function(env, "parse", NAPI_AUTO_LENGTH, napi_parse, NULL, &parse_fn);
    napi_create_function(env, "load",  NAPI_AUTO_LENGTH, napi_load,  NULL, &load_fn);
    napi_create_function(env, "lastError", NAPI_AUTO_LENGTH, napi_last_error, NULL, &last_error_fn);
    napi_set_named_property(env, exports, "parse",     parse_fn);
    napi_set_named_property(env, exports, "load",      load_fn);
    napi_set_named_property(env, exports, "lastError", last_error_fn);
    return exports;
}
```

### 4.4 `binding.gyp`

```json
{
  "targets": [{
    "target_name": "n-anvl-binder",
    "sources": [
      "src/binder.c",
    "../anvil/src/core/anvil.c",
      "../anvil/src/core/context.c",
      "../anvil/src/core/errors.c",
      "../anvil/src/core/memory.c",
      "../anvil/src/core/parser.c",
      "../anvil/src/core/strings.c",
      "../anvil/src/utilities/utils.c",
      "../anvil/src/resolver/resolver.c",
      "../anvil/src/vars/vars.c"
    ],
    "include_dirs": [
      "../anvil/include"
    ],
        "cflags": ["-std=c2x", "-Wall", "-Wextra"]
  }]
}
```

---

## 5. JavaScript API (`anvil.js`)

The JS library wraps `AnvilTree` from the binder in `AnvilNode` — the same class FlyWire already uses.

### 5.1 Public module API

```js
const anvl = require('anvil.js');  // or @quantumoverride/anvl

// Parse source string
const root = anvl.parse(source);       // → AnvilNode | null
const root = anvl.load('config.anvl'); // → AnvilNode | null
const err  = anvl.lastError();         // → { message, line, column, path } | null

// Serialize back to ANVL text
const text = anvl.write(node, { minify: false });

// Deserialize to typed JS object (schema-driven)
const obj = anvl.deserialize(node, MyClass);
```

### 5.2 `AnvilNode` methods

```js
// Field access
node.get('fieldName')           // → AnvilNode | null
node.require('fieldName')       // → AnvilNode  (throws KeyError if absent)
node.has('fieldName')           // → boolean

// Element access (arrays and tuples)
node.at(0)                      // → AnvilNode | null

// Scalar coercion
node.asString()                 // → string  (raw content, already quote-stripped)
node.asInt()                    // → number  (integer)
node.asFloat()                  // → number  (float)
node.asBool()                   // → boolean

// With default (no throw on miss)
node.getString('key', '')
node.getInt('key', 0)
node.getFloat('key', 0.0)
node.getBool('key', false)

// Iteration
for (const child of node)       // Object: yields field values in order
                                 // Array/Tuple: yields elements
node.keys()                     // → Iterable<string>  (field names)
node.entries()                  // → Iterable<[string, AnvilNode]>

// Inspection
node.type                       // AnvilValueType enum string
node.kind                       // ScalarKind (Int, Float, Bool, Identifier, String, …)
node.attrs                      // Map<string, string | null>
node.hasAttr('key')             // → boolean
node.attr('key')                // → string | null
```

### 5.3 Usage examples

**Parse a config file:**
```js
const anvl = require('anvil.js');

const root = anvl.load('server.anvl');
if (!root) {
    const e = anvl.lastError();
    throw new Error(`${e.path}:${e.line}: ${e.message}`);
}

const host  = root.get('host').asString();   // "localhost"
const port  = root.get('port').asInt();      // 8080
const debug = root.get('debug').asBool();    // false
```

**Walk an object:**
```js
const config = root.get('server');
for (const [key, val] of config.entries()) {
    console.log(key, val.asString());
}
```

**Array iteration:**
```js
const tags = root.get('tags');
for (const tag of tags) {
    console.log(tag.asString());
}
// or positional:
console.log(tags.at(0).asString());
```

**Inheritance (resolver-wired):**
```js
// schema:
// Base := { hp := 100, speed := 5 }
// Hero : Base := { name := "Alice" }
const hero = root.get('Hero');
hero.get('hp').asInt();    // 100 (inherited from Base)
hero.get('name').asString(); // "Alice"
```

**Schema validation:**
```js
// FlyWire: parse a .meta.asch schema and validate a data document
const schema = anvl.load('vehicle.asch');
const data   = anvl.load('vehicle.anvl');

const rules  = anvl.schema.resolve(schema);
const result = anvl.schema.validate(rules, data);
if (!result.isValid) {
    for (const err of result.errors)
        console.error(`[${err.code}] ${err.message}`);
}
```

---

## 6. Parity Notes (anvil.js POC → native binding)

| Feature | POC behavior | Native behavior | Action |
|---------|-------------|-----------------|--------|
| `"quoted"` strings | Quotes stripped | Quotes stripped (v0.5.0) | ✅ Same |
| `'single-quoted'` | Quotes stripped | Stored as-is (bare scalar) | **Reserved** — not supported |
| Hyphen in keys (`my-field`) | Supported | Supported (v0.5.0 fix) | ✅ Same |
| `1.0.0` version bare literals | Supported | Supported | ✅ Same |
| `#FF5733` hex colour | Supported | Supported | ✅ Same |
| `192.168.1.1` IP address | Supported | Supported | ✅ Same |
| `x := 1; y := 2` semicolons | Supported | Supported | ✅ Same |
| Inheritance resolution | JS (`_resolver.js`) | C (`anvil.resolver.o`) | ✅ Same result |

---

*JavaScript binding repo: `anvil.js` (next)  
C reference: [Anvil C Users Guide](Anvil%20C%20Users%20Guide.md)*
