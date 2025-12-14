# Anvil Language Specification (v0.2.0-draft)

**Anvil** — A next-gen language paradigm with two dialects:  
- **AML** – Anvil Modelling Language (declarative, data-first)  
- **ASL** – Anvil Scripting Language (imperative, logic-first)

## 1. Core Principles

| Principle            | Description                                    |
|----------------------|------------------------------------------------|
| Human-first syntax   | Readable by non-programmers                    |
| Zero-boilerplate     | No classes, no types, no imports               |
| Dual dialects        | Model data with AML, script behavior with ASL |
| Seamless interop     | AML objects → ASL functions                    |
| Extensible           | @[key=value,…] for custom semantics            |

## 2. AML – Anvil Modelling Language

**Purpose:** Define immutable, hierarchical data models for any data-centric system (game content, configuration, UI layouts, asset pipelines, etc.)

### 2.1 Syntax

```
#!aml

@[context=attributes]?          # optional context-level attributes (first non-whitespace after shebang)
<identifier> @[attributes]? := value
```

### 2.2 Grammar (high-level)

```
document          → shebang? module_attributes? vars_block? statement*
module_attributes → "@[" attr_list "]"
vars_block        → "vars" WS "{" statement* "}"
statement         → ID (":" ID)? "@[" attr_list "]"? ":=" value
value             → literal | array | object | blob | bare_ref
bare_ref          → "." ID ( "." | ":" ID )*
```

### 2.3 Literals

| Type                | Syntax                                 | Example                                              | Notes                                      |
|---------------------|----------------------------------------|------------------------------------------------------|--------------------------------------------|
| Number              | [-]digits[.digits][eE[+|-]digits]?     | 50.0, -10, 1.2e-5                                    | Full scientific notation                  |
| Boolean             | true / false                           | true                                                 |                                            |
| Null                | null                                   | null                                                 |                                            |
| String              | "..." or '...'                         | "iron"                                               |                                            |
| Interpolated String | $"..." or $'...'                       | title := $"Welcome, {player.name}!"                  | C#-style, consumer resolves                |
| Hex color           | #rrggbb                                | #ffaa00                                              |                                            |
| Array               | [v1, v2, ...]                          | [wood, stone]                                        | Empty arrays disallowed                    |
| Object              | {k := v, ...}                          | {x := 1, y := 2}                                     | Empty objects disallowed                   |
| Blob (raw)          | `...`                                  | @md`**bold**`                                        | Backtick-delimited, optional @attribute    |
| Interpolated Blob   | $`...`                                 | desc := $`**{player.name}** joined!`                 | Backtick only, multi-line capable          |
| Bare Reference      | .identifier (dots/colons allowed)      | .max_health, .minecraft:stone                        | Resolves from vars block                   |

### 2.4 Attributes

```
@[type=block, mod=vanilla, debug=true]
```

- Optional on context level, top-level statements, objects, arrays, or before blobs  
- Same syntax everywhere: `@[key, key=value, …]`  
- Keys are identifiers; values are simple literals only (no objects/arrays/blobs)  
- Pure metadata – parser never interprets  
- Consumer reads attributes at runtime

### 2.5 Module-Level Attributes

- Appear immediately after the shebang (or at file start if no shebang)  
- Only one context-level attribute block allowed  
- Follows identical syntax and rules as statement-level attributes

Example:

```
#!aml
@[mod=main, author=badkraft]

vars {
    max_health = 100
}
```

### 2.6 vars Block (Module-scoped constants)

```
vars {
    max_health = 100
    default_material = .minecraft:stone
    greeting = $"Hello {player_name}!"
}
```

- Must be the first non-shebang, non-context-attribute statement  
- Flat key → value map  
- Uses `=` (not `:=`)  
- Values may be any literal type, including bare references  
- Visible to all subsequent statements and interpolations in the same context

### 2.7 Deferred Resolution

Every `Statement` carries:

```java
boolean needsResolution = false;
```

Set to `true` when the value is:
- a Bare Reference (starts with `.`)  
- an interpolated string/blob containing `${...}`

Parser never resolves – consumer replaces values from `context.vars` at load time.

## 3. Inheritance

### 3.1 Syntax

```
ChildName : ParentName @[attributes]? {
    field = override_value
    new_field = 42
}
```

- Colon (`:`) denotes inheritance – no keywords  
- Optional whitespace around colon permitted  
- Parent fields + attributes are shallow-merged  
- Child overrides silently win  
- Parent must be declared earlier in the same file (for now)

Example:

```
BaseEntity : @living {
    health = 20
    speed  = 3
}

Player : BaseEntity @visible {
    health = 100
    name   = $"Player_{id}"
}
```

Result: `Player` has `health=100`, `speed=3`, `@living` and `@visible`.

## 4. File Conventions

| Extension | Dialect | Shebang   |
|-----------|---------|-----------|
| .aml      | AML     | #!aml     |
| .asl      | ASL     | #!asl     |

Shebang overrides extension.

## 5. Roadmap & Implementation Checklist

| Feature                                    | Spec | Implemented |
|--------------------------------------------|------|-------------|
| Rename Aurora → Anvil                      | Yes  | Yes         |
| All base value types (num, bool, str, blob, null, obj, arr, tuple) | Yes  | Yes         |
| Bare reference values (`.key`)             | Yes  |             |
| `vars` block – top, flat, context-scoped    | Yes  |             |
| Interpolation `$"...${key}..."` + blob     | Yes  |             |
| `Statement.needsResolution` flag           | Yes  |             |
| Module-level attributes `@[ … ]`           | Yes  | Yes         |
| Inheritance `Child : Parent {}` merge      | Yes  |             |
| Multi-line embeds & escaping               |      |             |
| Module namespacing (`mod:main`)            |      |             |
| Parser error diagnostics & location spans  |      |             |

This is the complete, current, living Anvil specification — fully aligned with the canonical parser implementation and intentionally agnostic to any single use case.
