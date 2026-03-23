# Anvil.Net — Users Guide
**v0.4.3 — AMP scalar arrays/tuples, `@[attr]` on ASL function declarations, `FunctionNode()` attribute discovery**

---

## Overview

Anvil.Net exposes a single consumer-facing type: **`AnvilNode`**.  
It represents any value in the document — the document root, a statement, an object field,
an array/tuple element, or a scalar — and implements `IEnumerable<AnvilNode>` so LINQ works
natively on collections.

One type. One static entry point. Full LINQ. Zero friction.

---

## Entry Points

```csharp
// From a file path (dialect detected from shebang or extension)
AnvilNode root = Anvil.Load("server.aml");

// From a raw string
AnvilNode root = Anvil.Parse(sourceString);

// With an explicit dialect hint (overridden by shebang if present)
AnvilNode root = Anvil.Parse(sourceString, AnvilDialect.Aml);

// From a directory — every .aml/.asl/.anvl file, top-level only
AnvilNode root = Anvil.LoadDirectory("/path/to/data");

// Recursive scan of subdirectories
AnvilNode root = Anvil.LoadDirectory("/path/to/data", recursive: true);
```

If parsing fails, these methods return `null` and `Anvil.LastError` contains the details:

```csharp
AnvilNode? root = Anvil.Load("server.aml");
if (root is null)
{
    var err = Anvil.LastError;
    Console.Error.WriteLine($"Parse error at {err!.Line}:{err.Column} — {err.Message}");
    return;
}
```

---

## Navigation

### Indexer: objects and top-level statements

```csharp
AnvilNode server = root["server"];
string name      = root["server"]["name"].AsString();
int    port      = root["server"]["port"].AsInt();
```

### Indexer: arrays and tuples

```csharp
AnvilNode first = root["tags"][0];
AnvilNode x     = root["spawn"][0];
```

### Null-safe chaining

```csharp
string motd = root["optional"]?.AsString() ?? "default";
```

---

## Safe Navigation

Use `TryGet` when a key or index may not be present and you prefer `null` over an exception:

```csharp
// Returns null if "optional" does not exist — never throws
AnvilNode? opt = root.TryGet("optional");

// Safe indexing into arrays
AnvilNode? third = root["tags"].TryGet(2);

// Chain safely
string? value = root.TryGet("config")?.TryGet("debug")?.AsString();
```

Use `GetX(key, default)` extension methods when you want a typed value with a fallback:

```csharp
string host    = root.GetString("host",    "localhost");
int    port    = root.GetInt   ("port",    25565);
bool   debug   = root.GetBool  ("debug",   false);
double timeout = root.GetDouble("timeout", 5.0);
```

These are equivalent to `TryGet(key)?.AsX() ?? defaultValue` — zero allocation, never throw.

---

## Scalar Access

```csharp
string  s = node.AsString();    // "stone"
long    l = node.AsLong();      // 25565
double  d = node.AsDouble();    // 3.14
bool    b = node.AsBoolean();   // true
int     i = node.AsInt();       // convenience — casts AsLong()
float   f = node.AsFloat();     // convenience — casts AsDouble()
```

All throw `InvalidOperationException` on type mismatch (fail-fast — the same philosophy as the parser).

### String value convention

By convention, scalar string values should be written **without quotes** unless:
- the value contains **spaces, newlines, or significant whitespace**, or
- the quoted form is semantically clearer in context (e.g. a message or display text)

```aml
# preferred — no quotes needed
mode     := survival
version  := 1.0.0
texture  := terrain.png
enabled  := true
count    := 42

# quoted — spaces or display context require them
motd     := "Welcome to the server!"
message  := "Hello, world"
```

`AsString()` returns the raw source slice; for quoted values this **includes** the surrounding
`"` characters. Use `.Trim('"')` or the `GetString(key)` extension method (which trims
automatically) when you need the bare text from a quoted value.

---

## Zero-Copy Span Access

For performance-sensitive paths where you want to inspect scalar content without allocating a
new `string`, use the span accessors:

```csharp
// ReadOnlySpan<char> — valid only for the lifetime of the root node
ReadOnlySpan<char>   span = root["port"].AsSpan();

// ReadOnlyMemory<char> — safe to store or pass across method boundaries
ReadOnlyMemory<char> mem  = root["port"].AsMemory();

// Common use: avoid allocation when comparing or parsing
bool isDefault = root["host"].AsSpan().SequenceEqual("localhost");
long port      = long.Parse(root["port"].AsSpan());
```

Both methods throw `InvalidOperationException` on non-scalar nodes.

---

## vars Block

A `vars` block declares named, reusable values at document root. It must appear before
any top-level statements.

```aml
#!aml
vars {
    atlas     := terrain.png
    base_path := assets/textures
    scale     := 1.5
}

block := {
    texture := .atlas          // VarRef — resolved at runtime
    path    := .base_path
}
```

### VarRef (`.identifier`)

A dot-prefixed identifier in any value position resolves transparently to the `vars` entry
with that name. No special call is needed — the runtime resolves it on first access:

```csharp
AnvilNode root = Anvil.Load("blocks.aml")!;

// .atlas resolves to terrain.png — transparent to the consumer
string tex = root["block"]["texture"].AsString();  // "terrain.png"
```

VarRef chains are allowed (`a := .b`, `b := scalar`). Circular chains are detected at
load time and throw `AnvilVarsException`.

### Introspecting vars

```csharp
// Check whether a vars key exists
bool hasAtlas = root.HasVar("atlas");   // true

// Get the vars node directly
AnvilNode? atlasNode = root.Var("atlas");
string     atlas     = atlasNode!.AsString();  // "terrain.png"
```

`HasVar` and `Var` are available on the root node only.

---

## Imports

An `import` declaration loads an external AML file into the current document at parse time.
Imports must appear before all top-level statements.

```aml
#!aml
import "blocks.aml"
import "shared/items.aml" as items

// statements follow
Hero := { class := warrior, health := 100 }
```

### Syntax

```aml
import "relative/path/to/file.aml"           // auto-alias: stem of filename
import "relative/path/to/file.aml" as alias   // explicit alias
```

The path is resolved relative to the **owning file's directory**. `import` is not available
when parsing from an in-memory string (`Anvil.Parse(string)`) — use `Anvil.Load(path)`
or `Anvil.LoadFile(path)` instead.

### Alias rules

| Form | File | Alias |
|---|---|---|
| `import "blocks.aml"` | `blocks.aml` | `blocks` |
| `import "shared-blocks.aml"` | `shared-blocks.aml` | `shared_blocks` |
| `import "path/to/items.aml" as items` | `items.aml` | `items` |

Stem aliases are derived by taking the filename without extension, lowercasing, and
replacing hyphens/spaces with underscores. Explicit `as alias` overrides this.

### Cross-source statement lookup

Statements from imported files are accessed with dot-qualified keys:

```aml
// blocks.aml
Weapon := { type := sword, damage := 10 }
```

```aml
// root.aml
import "blocks.aml"

Hero := { class := warrior, health := 100 }
```

```csharp
AnvilNode root = Anvil.Load("root.aml")!;

// Local statement
string cls  = root["Hero"]["class"].AsString();    // "warrior"

// Cross-source statement (alias = "blocks")
string type = root["blocks.Weapon"]["type"].AsString();  // "sword"
long   dmg  = root["blocks.Weapon"]["damage"].AsLong();  // 10
```

The qualified key form is `"alias.StatementKey"`. `TryGet` works the same way:

```csharp
AnvilNode? weapon = root.TryGet("blocks.Weapon");   // null if alias or key absent
```

### Cross-source VarRef resolution

A field value can reference a `vars` entry from an imported file using the
`alias.varKey` dot notation:

```aml
// config.aml
vars { main_color := red, font_size := 14 }
```

```aml
// ui.aml
import "config.aml" as cfg
vars { label := widget }

Widget := {
    color := .cfg.main_color   // resolved from imported vars
    label := .label            // resolved from local vars
}
```

```csharp
AnvilNode root = Anvil.Load("ui.aml")!;
string color = root["Widget"]["color"].AsString();   // "red"
string label = root["Widget"]["label"].AsString();   // "widget"
```

### Cross-source inheritance

A local statement can inherit from a base type defined in an imported file:

```aml
// types.aml
Character := { health := 100, level := 1 }
```

```aml
// root.aml
import "types.aml"

Hero:types.Character := { weapon := sword, name := Aria }
```

```csharp
AnvilNode root = Anvil.Load("root.aml")!;

// Own fields
string weapon = root["Hero"]["weapon"].AsString();  // "sword"
// Inherited from types.Character
long   health = root["Hero"]["health"].AsLong();    // 100
long   level  = root["Hero"]["level"].AsLong();     // 1
```

### Multiple imports and transitive graphs

Imports are resolved transitively. If `a.aml` imports `b.aml` and `b.aml` imports
`c.aml`, all three are available to `a.aml` through their respective aliases.
Diamond imports (same file reachable via multiple paths) are deduplicated; the
file is loaded only once.

Cycles (`a → b → a`) are detected at load time and reported as an error.

### Import error codes

| Code | Name | When |
|---|---|---|
| `4201` | `ImportNotAllowedInMemory` | `import` used with `Anvil.Parse(string)` |
| `4202` | `ImportAmpForbidden` | `import` inside an `#!amp` dialect file |
| `4203` | `ImportNotFirst` | `import` appears after statements |
| `4204` | `ImportFileNotFound` | imported path does not exist on disk |
| `4205` | `ImportDuplicateAlias` | two imports share the same alias |
| `4206` | `ImportCyclicDependency` | import graph contains a cycle |
| `4207` | `ImportNamespaceCollision` | stem-alias conflicts with another import |

---

## LoadDirectory

`Anvil.LoadDirectory` scans a directory for Anvil files and returns a synthetic root
node that proxies all loaded files. This is a flat sibling model — every file becomes
an independently addressable import under its stem alias.

```csharp
// Load all .aml/.asl/.anvl files in one directory
AnvilNode root = Anvil.LoadDirectory("data/blocks")!;

// Load recursively — subdirectory files are included
AnvilNode root = Anvil.LoadDirectory("data", recursive: true)!;

// Access statements by "stem.Key" — same as cross-source import syntax
string category = root["stone_types.Granite"]["category"].AsString();
//                     ^^^^^^^^^^^^  ^^^^^^^
//                     stem alias    statement key
//                     (stone-types.aml → "stone_types")
```

### Stem alias rules

The alias is derived from the filename (no extension), lowercased, with hyphens and
spaces replaced by underscores — the same rule as import auto-aliasing:

| File | Stem alias |
|---|---|
| `blocks.aml` | `blocks` |
| `stone-types.aml` | `stone_types` |
| `My Weapons.asl` | `my_weapons` |

### Behaviour details

| Condition | Result |
|---|---|
| Directory not found | `null` + `FileNotFound` (code `6001`) |
| Empty directory (zero Anvil files) | non-null empty root, no error |
| Stem alias collision (two files produce the same alias) | `null` + `ImportDuplicateAlias` (code `4205`) |
| `.amp` files in the directory | silently skipped — AMP may not be imported |
| Same canonical path seen twice | deduplicated, loaded once |

Directory loads are intentionally flat: import declarations *inside* each loaded file
are not expanded. Each file’s own statements and vars are fully accessible; transitive
cross-file dependencies should be wired via the normal `import` syntax inside those
files.

```csharp
// Error handling
AnvilNode? root = Anvil.LoadDirectory("missing");
if (root is null)
{
    var err = Anvil.LastError;
    Console.Error.WriteLine($"{err!.Code} — {err.Message}");
}
```

---

## Interpolated Strings

An interpolated string (`$"..."`) embeds `vars` references inline. Curly-brace segments
are resolved at `AsString()` call time:

```aml
#!aml
vars {
    player_name := Atlas
    base_path   := assets/textures
}

greeting := $"Welcome, {player_name}!"
tex_path := $"Full path: {base_path}/terrain.png"
```

```csharp
AnvilNode root = Anvil.Parse(aml)!;

string g = root["greeting"].AsString();  // "Welcome, Atlas!"
string p = root["tex_path"].AsString();  // "Full path: assets/textures/terrain.png"
```

Escape literal braces with `{{` and `}}`:

```aml
note := $"Use {{braces}} for sets"   // → "Use {braces} for sets"
```

A missing `vars` key inside `$""` throws `AnvilVarsException` at `AsString()` call time.

---

## Collections (IEnumerable)

Arrays and tuples implement `IEnumerable<AnvilNode>`, so standard LINQ applies:

```csharp
// Array of strings
List<string> rules = root["world"]["rules"]
    .Select(r => r.AsString())
    .ToList();

// Array of objects — get a field from each
IEnumerable<int> damages = root["weapons"]
    .Select(w => w["damage"].AsInt());

// Count, Any, First — all work
int count  = root["tags"].Count();
bool hasPvp = root["world"]["rules"].Any(r => r.AsString() == "pvp");
string first = root["tags"].First().AsString();
```

---

## Attributes

```csharp
// Flag attribute (no value)
bool experimental = root.HasAttribute("experimental");

// Key=value attribute
long seed    = root["world"].Attribute("seed")!.AsLong();
string modId = root.Attribute("mod")!.AsString();

// Enumerate all attributes on a node
foreach (AnvilNode attr in root.Attributes)
    Console.WriteLine($"@{attr.Identifier} = {attr.AsString()}");
```

---

## Blobs

```csharp
AnvilNode readme = root["readme"];
Console.WriteLine(readme.Type);        // AnvilValueType.Blob
Console.WriteLine(readme.AsString());  // raw content between backticks
Console.WriteLine(readme.BlobTag);     // "md", "sql", etc. (null if untagged)
```

---

## Type Checking

```csharp
if (node.Type == AnvilValueType.Object)  { ... }
if (node.Type == AnvilValueType.Array)   { ... }
if (node.Type == AnvilValueType.Tuple)   { ... }
if (node.Type == AnvilValueType.Scalar)  { ... }
if (node.Type == AnvilValueType.Blob)    { ... }
```

Or use `switch` expression:

```csharp
string display = node.Type switch
{
    AnvilValueType.Scalar => node.AsString(),
    AnvilValueType.Array  => $"[{node.Count()} elements]",
    AnvilValueType.Object => $"{{{node.Count()} fields}}",
    _                     => node.Type.ToString(),
};
```

---

## Tuple Deconstruction

Extension methods support structured access on positional containers:

```csharp
// Two-element
var (x, y) = root["point"].AsValues<int, int>();

// Three-element
var (r, g, b) = root["color"].AsValues<int, int, int>();
```

---

## Inheritance

AML supports single-inheritance between statements:

```aml
#!aml
BaseBlock := {
    name      := "Base"
    hardness  := 5
    dropChance := 0.5
}

StoneBlock:BaseBlock := {
    name     := "Stone"
    hardness := 8
    // inherits dropChance from BaseBlock
}
```

Inheritance is resolved **transparently** — no explicit call needed:

```csharp
AnvilNode root = Anvil.Load("blocks.aml")!;

// Inherited field — just works
double chance = root["StoneBlock"]["dropChance"].AsDouble(); // 0.5 from BaseBlock
long   hard   = root["StoneBlock"]["hardness"].AsLong();     // 8 (overridden)
```

### Querying the inheritance chain

```csharp
AnvilNode stone = root["StoneBlock"];

string? parent = stone.BaseIdentifier;    // "BaseBlock"
bool    hasBase = stone.HasBase;           // true
bool    isBase  = stone.Is("BaseBlock");   // true
bool    isOther = stone.Is("OtherBlock");  // false

// Works for deeper chains too: Leaf.Is("Root") on Leaf:Mid:Root returns true
```

### Opt-in pre-warm (`Resolve()`)

By default, each derived statement’s field list is merged on first access and cached for
subsequent accesses. If you know you’ll read the entire document (e.g. bulk serialization),
call `Resolve()` to pay the merge cost all at once:

```csharp
// Pre-warm all merges upfront
AnvilNode root = Anvil.Load("blocks.aml")!.Resolve();

// Or use the convenience entry point
AnvilNode root = Anvil.LoadResolved("blocks.aml")!;
```

`Resolve()` returns `this` — the same root node. Inheritance was already transparent;
`Resolve()` is purely an allocation-timing choice.

### Error handling

- **Cycle** (`A:B`, `B:A`) — `AnvilResolverException` thrown at `Load`/`Parse` time.
- **Missing base** — `AnvilResolverException` thrown at first field access on the affected
  statement.

---

## POCO Deserialization

`AnvilSerializer` maps an Anvil document directly to a plain C# class:

```aml
#!aml
host       := "play.example.com"
port       := 25565
debug      := false
max_players := 100
tags       := ["survival", "pvp"]
```

```csharp
public class ServerConfig
{
    public string   Host       { get; set; } = "";
    public int      Port       { get; set; }
    public bool     Debug      { get; set; }

    [AnvilMember("max_players")]  // explicit key override
    public int      MaxPlayers { get; set; }

    public string[] Tags       { get; set; } = [];
}

// One-liner
ServerConfig cfg = AnvilSerializer.Load<ServerConfig>("server.aml")!;

// Or from a parsed node
AnvilNode root = Anvil.Load("server.aml")!;
ServerConfig cfg = AnvilSerializer.Deserialize<ServerConfig>(root);
```

**Key resolution** (per property, in priority order):
1. `[AnvilMember("key")]` — uses that literal key
2. No attribute — property name lowercased (`MaxPlayers` → `"maxplayers"`)

**Supported types**: `string`, `bool`, `long`, `int`, `short`, `byte`, `double`, `float`,
`Nullable<T>` of any primitive, `T[]`, `List<T>` / `IList<T>` / `IEnumerable<T>`,
nested POCO classes (resolved recursively), and **`enum` types** (see below).

Missing keys silently leave the property at their default value — no exception.

### Enum deserialization

**Part 1 — integer scalar (no attribute required)**

An AML integer scalar is automatically coerced to any `enum`-typed property:

```aml
#!aml
side := 1
```

```csharp
[Flags] public enum ModSide { Server = 1, Client = 2, Both = 3 }

public class ModManifest {
    public ModSide Side { get; init; } = ModSide.Both;
}

var m = AnvilSerializer.Parse<ModManifest>(aml)!;
// m.Side == ModSide.Server  ✓
```

**Part 2 — string by name (`CoerceAs`)**

Add a second argument to `[AnvilMember]` to enable string-by-name resolution. Matching
is case-insensitive. Unrecognised names throw `InvalidOperationException`.

```aml
#!aml
side := Server          // bare identifier — no quotes needed
```

```csharp
[AnvilMember("side", typeof(ModSide))]
public ModSide Side { get; init; } = ModSide.Both;

// m.Side == ModSide.Server  ✓  ("server" also works)
```

---

## POCO Serialization — Write Side

`AnvilSerializer` can serialize any POCO back to an AML string, file, or `TextWriter`.

```csharp
public class ServerConfig
{
    public string Host    { get; set; } = "";
    public int    Port    { get; set; }
    public bool   Debug   { get; set; }
    public string[] Tags  { get; set; } = [];
}

var cfg = new ServerConfig { Host = "play.example.com", Port = 25565, Debug = false,
                              Tags = ["survival", "pvp"] };

// To string
string aml  = AnvilSerializer.Serialize(cfg);

// To file
AnvilSerializer.Save(cfg, "server.aml");

// To any TextWriter
AnvilSerializer.Serialize(cfg, Console.Out);
```

The emitted form uses the same key-resolution rules as the read side (lowercased property
name or `[AnvilMember]` override). String values only receive quotes when required for
unambiguous parsing (see [String quoting rules](#string-quoting-rules) below).

### Inheritance — write side

Pass `baseType` to emit a multi-statement inheritance chain:

```csharp
public class BaseBlock  { public string Name { get; set; } = ""; public int Hardness { get; set; } }
public class StoneBlock : BaseBlock { public int Extra { get; set; } }

var stone = new StoneBlock { Name = "Stone", Hardness = 8, Extra = 1 };
string aml = AnvilSerializer.Serialize(stone, baseType: typeof(BaseBlock));
// → baseBlock := { name := Stone, hardness := 8 }
//   stoneBlock:baseBlock := { extra := 1 }
```

### Writer options

```csharp
AnvilSerializer.Serialize(cfg, options: new AnvilWriterOptions
{
    IncludeShebang   = true,       // emit #!aml header (default true)
    IndentWidth      = 2,          // spaces per indent level (default 4)
    InlineThreshold  = 3,          // max fields/elements on one line (default 2)
    PreserveVars     = false,      // materialise VarRefs (default true)
    Minify           = false,      // compact single-line output (default false)
    Dialect          = AnvilDialect.Aml,  // shebang dialect (default Aml)
});
```

---

## Serialization Attributes

### `[AnvilMember]` — key override + enum coercion

Already documented in the deserialization section; applies to both directions.

### `[AnvilVar]` — bind to a `vars` entry

Already documented in the deserialization section.

### `[AnvilBase]` — AML statement name

Already documented in the inheritance section; also controls the emitted statement
identifier on the write path.

### `[AnvilQuoted]` — force double-quote wrapping

Forces the serializer to emit the string value in double quotes regardless of whether
quotes are structurally required.

```csharp
public class Config
{
    [AnvilQuoted]
    public string Mode { get; set; } = "survival";
    // → mode := "survival"  (always quoted)

    public string Version { get; set; } = "1.0.0";
    // → version := 1.0.0   (no quotes needed)
}
```

### `[AnvilArray]` — custom collection as `[ … ]`

Standard `T[]`, `List<T>`, and `IList<T>` are handled automatically.  Use `[AnvilArray]`
for any other sequence type (`IEnumerable<T>`, `HashSet<T>`, etc.) or to force block
layout.

```csharp
[AnvilArray]
public IEnumerable<string> Tags { get; set; } = [];
// → tags := [ alpha, beta, gamma ]

[AnvilArray(Block = true)]
public HashSet<int> Ports { get; set; } = [];
// → ports := [
//       8080,
//       8443,
//   ]
```

### `[AnvilMap]` — `Dictionary<string, V>` as `{ k := v }`

```csharp
[AnvilMap]
public Dictionary<string, int> Scores { get; set; } = new();
// → scores := { alice := 300, bob := 200 }
```

During deserialization the AML object's child fields become dictionary entries — keys
become strings, values are coerced to `V`.

### `[AnvilTuple]` — POCO property as `( v1, v2, … )`

```csharp
[AnvilTuple]
public SpawnPoint Spawn { get; set; }
// → spawn := (100, 64, 0)   (properties in declaration order)

[AnvilTuple("Y", "X", "Z")]
public SpawnPoint Spawn { get; set; }
// → spawn := (64, 100, 0)   (Y first, then X, then Z)
```

### C# `ValueTuple` — automatic

`ValueTuple<T1, T2, …>` properties are detected and serialized as AML tuples
automatically — no attribute required.

```csharp
public (string Name, int Health) Player { get; set; } = ("Notch", 100);
// → player := (Notch, 100)
```

---

## String Quoting Rules

The serializer emits string values **without** quotes unless at least one of the
following is true:

| Condition | Example value | Emitted form |
|---|---|---|
| Empty string | `""` | `""` |
| Equals `true`, `false`, or `null` (would be misread as boolean/null) | `"true"` | `"true"` |
| Contains structural characters: whitespace, `,`, `{`, `}`, `[`, `]`, `(`, `)`, `;` | `"a b"` | `"a b"` |
| Starts with `.`, `$`, or `` ` `` (AML sigils) | `".ref"` | `".ref"` |
| `[AnvilQuoted]` applied | any | always quoted |

All other strings — including numeric-looking strings like `"8080"` or `"-443"` — are
emitted bare. `AsString().Trim('"')` strips quotes on the read side so round-trips are
transparent.

---

## AnvilWriter

`AnvilWriter` renders a parsed `AnvilNode` document back to AML text.
It is the underlying engine that `AnvilSerializer` delegates to, and can also be used
directly when you already have a parsed node tree.

```csharp
AnvilNode root = Anvil.Load("server.aml")!;

// To string
string aml = AnvilWriter.ToString(root);

// To any TextWriter
AnvilWriter.Write(root, Console.Out);

// To a file
AnvilWriter.Save(root, "out.aml");
```

### AnvilWriterOptions

```csharp
var opts = new AnvilWriterOptions
{
    Dialect          = AnvilDialect.Aml, // #!aml shebang; Aml (default), Asl, Amp, Aurora
    IncludeShebang   = true,             // prepend the dialect shebang line (default true)
    IndentWidth      = 4,                // spaces per indent level (default 4)
    AlignAssignments = false,            // pad := so columns align within a block
    InlineThreshold  = 2,               // max fields/elements per inline { }/[ ] (default 2)
    PreserveVars     = true,             // emit vars block + .ref tokens (default true)
    Minify           = false,            // compact single-line output (default false)
};

string pretty  = AnvilWriter.ToString(root, opts);
string compact = AnvilWriter.ToString(root, AnvilWriterOptions.Minified);
```

### Shebang and dialect

| `Dialect` value | Emitted shebang |
|---|---|
| `Aml` | `#!aml` |
| `Asl` | `#!asl` |
| `Amp` | `#!amp` |
| `Aurora` | *(none)* |

When `Dialect = Amp`, attempting to write an `Object`, `Array`, or `Tuple` node throws
`InvalidOperationException` — AMP only supports scalars and blobs.

### Minification

Minified output collapses the entire document onto a single content line, comma-separating
statements, with no spaces around `:=` or inside delimiters.

```csharp
const string aml = """
    #!aml
    name   := engine
    config := { port := 8080, debug := false }
    tags   := [ alpha, beta ]
    """;

string mini = AnvilWriter.ToString(Anvil.Parse(aml)!, AnvilWriterOptions.Minified);
// → #!aml
//   name:=engine,config:={port:=8080,debug:=false},tags:=[alpha,beta]
```

Minification always suppresses the `vars` block (VarRefs are materialised to their values)
and module-level attributes.

---

## AMP Messaging

AMP (*Anvil Messaging Protocol*) is the `#!amp` dialect — a restricted, human-readable
transport format designed for high-throughput message payloads.

### Allowed value types

| Type | Example |
|------|---------|
| Integer | `42`, `-7`, `0xFF` |
| Float | `3.14`, `-0.5e10` |
| Boolean | `true`, `false` |
| String | `"hello"`, `world` (bare) |
| Null | `null` |
| Scalar array | `[1, 2, 3]`, `["a", "b"]` |
| Scalar tuple | `(128, 64, -37)`, `("join", 2, true)` |
| Tagged blob | `` @utf8`{"id":42}` `` |
| Untagged blob | `` `raw bytes` `` |

**Forever forbidden:** objects `{}`, nested arrays/tuples, `@[attr]` syntax, `using`,
`import`, inheritance (`: Base`).

### Quick example

```amp
#!amp
type    := "player.join"
version := 2
active  := true
ids     := [101, 204, 387]
coords  := (128, 64, -37)
header  := ("player.join", 2, true)
data    := @nbt`<packed binary>`
```

### Blob syntax

The blob tag is a machine-readable content-type label — the parser does not interpret it,
but the host reads `node.BlobTag`:

```amp
payload := @utf8`{"id":42,"name":"player"}`
notes   := @md`**bold** note`
raw     := `untagged content`
```

```csharp
AnvilNode root = Anvil.Parse(src)!;
string content = root["payload"].AsString();   // {"id":42,"name":"player"}
string? tag    = root["payload"].BlobTag;      // "utf8"
```

### Scalar arrays and tuples

Array elements and tuple elements must all be scalars. Nesting is forbidden — a nested
array or tuple inside another produces `AmpArrayElementNotScalar` (4401).

```csharp
var ids    = root["ids"].ToList();             // IList<AnvilNode>
long first = ids[0].AsLong();                  // 101

var coords = root["coords"].ToList();
long x = coords[0].AsLong();  // 128
long y = coords[1].AsLong();  // 64
long z = coords[2].AsLong();  // -37
```

### Writing AMP

Pass `Dialect = AnvilDialect.Amp` to `AnvilWriterOptions`. Object nodes throw
`InvalidOperationException`; scalar arrays, tuples, and blobs write cleanly.

```csharp
var opts = new AnvilWriterOptions { Dialect = AnvilDialect.Amp };
string text = AnvilWriter.ToString(root, opts);
```

### `BlobEncoding` note for host consumers

When a blob node is parsed in AMP mode, `AnvilValue.ScalarLength` carries a *packed
descriptor* rather than a raw byte length:

```
upper 8 bits  = tag length
lower 56 bits = content length
```

Use `BlobEncoding.ContentLength(node._scalarLen)` / `BlobEncoding.TagLength(…)` when
accessing raw span data on the AMP path. `AsString()` and `BlobTag` handle this
transparently.

---

## Validation

The `Anvil.Validate*()` family provides structured, non-throwing validation that
collects all failures across a document set in a single pass — unlike `Anvil.Load()`,
which stops at the first error and records it in the thread-local `AnvilError` slot.

### Single-document validation

```csharp
// Validate a file
AnvilValidationResult r = Anvil.Validate("config/blocks.aml");
if (!r.IsValid) {
    foreach (var err in r.Errors)
        Console.Error.WriteLine(err); // [Code] Message (line N, col C) in file
}

// Validate an in-memory string (File will be null in any errors)
AnvilValidationResult r2 = Anvil.ValidateSource("#!aml\nFoo");  // missing :=
Console.WriteLine(r2.IsValid);          // false
Console.WriteLine(r2.Errors[0].Code);   // ExpectedAssign
Console.WriteLine(r2.Errors[0].File);   // null — in-memory source
```

### Multi-document validation

```csharp
// Validate an entire directory of .aml files in one call.
// ValidateAll returns one AnvilValidationError per failing document.
var paths = Directory.GetFiles("data/", "*.aml");
AnvilValidationResult all = Anvil.ValidateAll(paths);

if (!all.IsValid) {
    Console.Error.WriteLine($"{all.Errors.Count} file(s) failed:");
    foreach (var err in all.Errors)
        Console.Error.WriteLine($"  {err.File}: [{err.Code}] {err.Message}");
}
```

This is the key advantage over `Load()`: a batch of 50 files returns up to 50
errors instead of failing silently on the first bad document.

### ThrowIfInvalid

```csharp
// Throw an AnvilValidationException if anything failed.
// Useful when validation is a precondition and you want crash-fast semantics.
Anvil.ValidateAll(configPaths).ThrowIfInvalid();
```

`AnvilValidationException.Result` carries the full `AnvilValidationResult` so
the catch site can inspect individual errors if needed.

### AnvilValidationError fields

| Property | Type | Description |
|---|---|---|
| `Code` | `AnvilErrorCode` | Error category (same enum as `AnvilError.LastError`) |
| `Message` | `string` | Human-readable description |
| `Line` | `int` | 1-based line number |
| `Column` | `int` | 1-based column number |
| `File` | `string?` | Source file path; `null` for in-memory (`ValidateSource`) |

---

## ASL Scripting

ASL (*Anvil Scripting Language*) is the `#!asl` dialect — it extends AML with named
functions, local variables, control flow, and host-module integration. An ASL document
can also contain plain AML statements and a `vars` block; those are fully accessible to
its functions via `$identifier` references.

### Quick example

```asl
#!asl
using Calc

vars {
    base_damage := 10
}

// Multiply the scalar $damage (from AML root or vars) by a factor passed as argument
scale_damage := (factor) => {
    result = Calc.mul($base_damage, factor)
    return result
}
```

### Language syntax

#### Function declaration

All top-level functions use lambda syntax. The function name is an AML-level identifier;
the body uses C-style statements.

```asl
name := (param1, param2) => {
    // body
    return expr;
}

// No parameters
name := () => {
    return 42;
}
```

#### Function attributes

Any function declaration may be annotated with `@[attr]` immediately before `:=`.
Attributes follow the same rules as AML node attributes: flag attributes (no value) and
key=value attributes may be mixed freely.

```asl
// Flag attributes only
on_load @[hook, once] := () => {
    Logger.info("loaded")
    return 1
}

// Key=value attribute
on_tick @[hook, priority=10] := (tick) => {
    return tick % 20
}

// Mixed — flag + key=value
on_start @[hook, once, priority=100] := () => {
    Logger.info("starting")
    return 1
}
```

Attributes have no effect at runtime unless the host reads them; they are purely
declarative metadata.

#### Assignment and compound assignment

```asl
x = 10;             // simple assignment (no type annotation)
x = x + 1;         // expression assignment
x++;                // post-increment (equivalent to x = x + 1)
x--;                // post-decrement
x += 5;             // compound add
x -= 2;             // compound subtract
x *= 3;             // compound multiply
x /= 4;             // compound divide
```

Semicolons are optional — the parser accepts bare statements as well.

#### Conditionals

```asl
if (x > 10) {
    result = "high";
}
else if (x > 5) {
    result = "mid";
}
else {
    result = "low";
}
```

#### For loops

```asl
// C-style: init; condition; step — all optional
for (i = 0; i < n; i++) {
    sum += i;
}

// Condition-only (while equivalent)
for (; n != 1;) {
    if (n % 2 == 0) { n = n / 2; }
    else             { n = n * 3 + 1; }
    steps++;
}
```

`break` exits the innermost loop; `continue` skips to the next iteration.

#### return

```asl
return expr;     // return a value
return;          // return null (void)
```

#### List and Map literals

```asl
items = [1, 2, 3];              // List
first = items[0];

config = { key := "val", n := 42 };   // Map
v = config["key"];
```

#### Operators

| Category | Operators |
|---|---|
| Arithmetic | `+` `-` `*` `/` `%` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Logical | `&&` `\|\|` `!` |
| Increment/decrement | `++` `--` (statement form only) |
| Compound assignment | `+=` `-=` `*=` `/=` |

#### Module calls

```asl
using sys_math

result = sys_math.sqrt(144);    // returns 12.0
upper  = sys_str.upper("hello");
```

### Registering host modules

Before evaluating any ASL document that uses a module, register it with
`AnvilModuleRegistry.Register`:

```csharp
public sealed class CalcModule : IAnvilCallableModule
{
    public string Name => "Calc";

    public AslValue Invoke(string method, AslValue[] args) => method switch
    {
        "add" => AslValue.FromInt(args[0].LongValue + args[1].LongValue),
        "mul" => AslValue.FromInt(args[0].LongValue * args[1].LongValue),
        "sqrt" => AslValue.FromFloat(Math.Sqrt(args[0].ToDouble())),
        _     => AslValue.Null,
    };
}

// Register once, globally (thread-safe)
AnvilModuleRegistry.Register(new CalcModule());
```

### Executing functions

Obtain an `AnvilScriptEngine` from any `AnvilNode` and call functions by name:

```csharp
AnvilNode root = Anvil.Load("combat.asl")!;
AnvilScriptEngine engine = root.GetScriptEngine();

// Execute a function — returns AslValue? (null on error)
AslValue? result = engine.Execute("scale_damage", AslValue.FromInt(3));

if (result is null)
{
    Console.Error.WriteLine(Anvil.LastError);  // e.g. AslFunctionNotFound
    return;
}

Console.WriteLine(result.Value.LongValue);  // 30 (base_damage 10 × factor 3)

// Introspect before calling
bool has = engine.HasFunction("scale_damage");   // true
var names = engine.FunctionNames;                // ["scale_damage"]
```

### Attribute-based function discovery

`FunctionNode(name)` returns the full `AnvilNode` for a named function, giving the host
access to its attributes. This enables declarative hook registration without naming
conventions or reflection.

```csharp
foreach (var name in engine.FunctionNames)
{
    AnvilNode? node = engine.FunctionNode(name);
    if (node is null || !node.HasAttribute("hook"))
        continue;

    // Read optional key=value attribute (returns null if absent)
    string? pv       = node.Attribute("priority");
    int     priority = pv is not null && int.TryParse(pv, out var p) ? p : 0;
    bool    once     = node.HasAttribute("once");

    Console.WriteLine($"{name}: priority={priority}, once={once}");
}
```

| Member | Returns | Purpose |
|--------|---------|--------|
| `engine.FunctionNode(name)` | `AnvilNode?` | Full AST node; `null` if name unknown |
| `node.HasAttribute(key)` | `bool` | Presence check (flag or key=value) |
| `node.Attribute(key)` | `string?` | Value of a key=value attribute; `null` if absent |
| `node.Attributes` | `IEnumerable<AnvilNode>` | All attributes on the node |

See `use-cases/USECASE-infinicraft_mod/` for a worked example of a game mod loader
that uses `@[hook, once, priority=N]` for lifecycle auto-registration.

### `AslValue`

`AslValue` is a flat readonly struct — no boxing for numeric and boolean results.

| Factory | Kind | Meaningful field |
|---|---|---|
| `AslValue.Null` | `Null` | — |
| `AslValue.FromInt(long)` | `Int` | `LongValue` |
| `AslValue.FromFloat(double)` | `Float` | `DoubleValue` |
| `AslValue.FromString(string)` | `String` | `StringValue` |
| `AslValue.FromBool(bool)` | `Bool` | `BoolValue` |

```csharp
bool ok = value.Kind == AslValueKind.Int;
long n  = value.LongValue;

// Truthiness (null, 0, 0.0, "" → false; everything else → true)
bool b = value.Truthy;

// Numeric coercion
double d = value.ToDouble();   // Int or Float → double (0.0 for other kinds)
```

### `using` declarations and `IAnvilModule`

A `using <Name>` declaration at the top of an ASL file declares a dependency on a
host-provided module. The runtime resolves it via `AnvilModuleRegistry`.

```asl
#!asl
using Math
using Logger
```

- Implement `IAnvilModule` to register a module by name.
- Implement `IAnvilCallableModule` (extends `IAnvilModule`) to make it invocable from
  scripts. `Invoke(method, args)` should return `AslValue.Null` for unknown methods
  rather than throwing.
- Modules that only implement `IAnvilModule` (not callable) can still be registered and
  satisfy `using` checks at load time, but calling them throws at runtime.

```csharp
// Minimal read-only module (satisfies 'using', not callable)
public sealed class ConfigModule : IAnvilModule
{
    public string Name => "Config";
}

// Callable module
public sealed class MathModule : IAnvilCallableModule
{
    public string Name => "Math";
    public AslValue Invoke(string method, AslValue[] args) => method switch
    {
        "abs"  => AslValue.FromInt(Math.Abs(args[0].LongValue)),
        "max"  => AslValue.FromInt(Math.Max(args[0].LongValue, args[1].LongValue)),
        _      => AslValue.Null,
    };
}
```

### `$key` external lookup

Inside a function body, `$name` resolves in this order:

1. **Top-level AML statement** in the co-located root document, by name.
2. **`vars` block** entry in the same document.

```asl
#!asl
vars { multiplier := 5 }

base := 10          // AML statement in the same document

double_base := () => {
    b = $base          // resolves to AML statement "base" (10)
    m = $multiplier    // resolves to vars entry (5)
    return b + m
}
```

### Recursion guard

Inter-function recursion is limited to **`AnvilScriptEngine.MaxCallDepth` = 64** levels.
Exceeding this limit sets `AslCallDepthExceeded` and returns `null`.

```csharp
if (result is null && Anvil.LastError?.Code == AnvilErrorCode.AslCallDepthExceeded)
    Console.Error.WriteLine("Script hit recursion limit");
```

### Thread safety

`AnvilScriptEngine` instances are **not thread-safe**. Create one engine per call-site
or per thread when concurrent execution is required. `AnvilModuleRegistry` is
thread-safe.

### ASL error codes

| Code | Name | When |
|---|---|---|
| `4302` | `AslInStrictAml` | ASL control-flow keyword used in a strict `.aml` source |
| `4305` | `UsingModuleNotFound` | `using <Name>` references a module not in the registry |
| `5501` | `AslRuntimeError` | General evaluation/runtime failure |
| `5502` | `AslFunctionNotFound` | `Execute(name, …)` called with an unknown function name |
| `5503` | `AslCallDepthExceeded` | Recursion depth exceeded `MaxCallDepth` (64) |

```aml
#!aml
@[version=2, experimental]

server @[core] := {
    name := "Anvil Survival"
    port := 25565
    motd := "Forged in fire."
}

world @[seed=1337] := {
    spawn @[respawn] := (0, 64, 0)
    rules @[hardcore] := [
        "pvp", "keepInventory=false", "naturalRegeneration=true"
    ]
}

readme := @md`# Anvil Server — Forged in fire. Built to last.`

player := ("Notch", 100, true)
```

```csharp
public sealed class AnvilServer
{
    private readonly AnvilNode _root;

    private AnvilServer(AnvilNode root) => _root = root;

    public static AnvilServer? Load(string path)
    {
        var root = Anvil.Load(path);
        return root is null ? null : new AnvilServer(root);
    }

    public string ServerName() => _root["server"]["name"].AsString();
    public int    Port()       => _root["server"]["port"].AsInt();
    public string Motd()       => _root["server"]["motd"].AsString();

    public long WorldSeed() => _root["world"].Attribute("seed")!.AsLong();

    public (int X, int Y, int Z) Spawn()
    {
        var s = _root["world"]["spawn"];
        return (s[0].AsInt(), s[1].AsInt(), s[2].AsInt());
    }

    public IReadOnlyList<string> GameRules()
        => _root["world"]["rules"].Select(r => r.AsString()).ToList();

    public string ReadmeMarkdown() => _root["readme"].AsString();

    public bool IsHardcore()     => _root["world"]["rules"].HasAttribute("hardcore");
    public bool IsExperimental() => _root.HasAttribute("experimental");

    public (string Name, int Health, bool Founder) Player()
    {
        var p = _root["player"];
        return (p[0].AsString(), p[1].AsInt(), p[2].AsBoolean());
    }
}
```

Usage:

```csharp
var server = AnvilServer.Load("server.aml")!;

Console.WriteLine($"Starting {server.ServerName()}");
Console.WriteLine($"Port: {server.Port()}");
Console.WriteLine($"MOTD: {server.Motd()}");

var (x, y, z) = server.Spawn();
Console.WriteLine($"Spawn: {x}, {y}, {z}");

Console.WriteLine("Game rules:");
foreach (var rule in server.GameRules())
    Console.WriteLine($"  {rule}");

if (server.IsHardcore())     Console.WriteLine("Hardcore mode enabled");
if (server.IsExperimental()) Console.WriteLine("Warning: experimental build");
```

---

## Schema Validation

> Introduced in **v0.4.5-alpha**.

Schema validation lets you declare the structure of your data documents up front
and validate real documents against those declarations at runtime.  The entire
pipeline is driven through the `AnvilSchema` class.

### Schema Documents

A schema document is any AML file that carries the `@[schema]` module attribute.
The `.asch` extension (new in v0.4.5) is automatically treated as AML dialect —
no shebang required when loading from a file path.

Three statement forms are recognised inside a schema document:

| Form | Kind | Example |
|---|---|---|
| `` ident : enum := (v1, v2, …) `` | `Enum` | `ModSide : enum := (client, server, both)` |
| `` ident : flags := (f1, f2, …) `` | `Flags` | `Perms : flags := (read, write, execute)` |
| `` ident := { field := default; … } `` | `Object` | `BlockConfig := { id := ""; hardness := 0.0 }` |

Full example:

```aml
#!aml
@[schema]
ModSide : enum := (client, server, both)

BlockConfig := {
  id               := ""
  name @[required] := ""
  hardness         := 1.0
  side @[type=ModSide] := client
}
```

### Data Documents

A data document is a regular AML file whose top-level statements optionally
reference a schema type as a base identifier:

```aml
#!aml
stone : BlockConfig := {
  id       := stone
  name     := cobblestone
  hardness := 1.5
  side     := server
}
```

Statements with no base (`stone := { … }`) are passed through without validation.

### Loading a Schema

```csharp
// From file
var schema = AnvilSchema.Load("mods.asch");
if (schema is null) {
    Console.Error.WriteLine(Anvil.LastError?.Message);
    return;
}

// From in-memory string
var schema = AnvilSchema.Parse("""
    #!aml
    @[schema]
    BlockConfig := {
      id       := ""
      hardness := 1.0
    }
    """);
```

### Validating a Document

```csharp
// Validate a file
var result = schema.Validate("data/stone.aml");
if (!result.IsValid)
    foreach (var err in result.Errors)
        Console.Error.WriteLine(err);            // [Code] Message (line, col) in file

// Validate an already-loaded node
var dataNode = Anvil.Load("data/stone.aml")!;
var result2  = schema.Validate(dataNode, filePath: "data/stone.aml");
result2.ThrowIfInvalid();   // throws AnvilValidationException when invalid

// Validate many files at once (all errors collected)
var result3 = schema.ValidateAll(Directory.EnumerateFiles("data/", "*.aml"));
```

### Validation Rules (v1)

- **Object types** — every field declared in the schema type must be present in
  the data statement. Fields are checked for a matching `AnvilValueType`
  (Scalar, Object, Array, Tuple).
- **Extra fields** — extra fields in the data statement are silently ignored
  (no strict mode in v1).
- **Enum / Flags types** — value presence is recorded in `SchemaType.Values`;
  field-level validation deferred to v0.5.x.
- **Untyped statements** — statements with no base pass through unconditionally.

### Inspecting the Ruleset

```csharp
foreach (var (name, type) in schema.Types)
{
    Console.WriteLine($"{name} ({type.Kind})");
    if (type.Kind == SchemaTypeKind.Object)
        foreach (var field in type.Fields)
            Console.WriteLine($"  {field.Name} : {field.ExpectedType}");
    else
        Console.WriteLine($"  values: {string.Join(", ", type.Values)}");
}
```

### Error Codes

| Code | Value | Meaning |
|---|---|---|
| `SchemaAttrMissing` | 4601 | Document missing `@[schema]` module attribute |
| `SchemaBaseUnknown` | 4603 | Unknown base identifier in schema type declaration |
| `SchemaValidationRequired` | 4604 | Required field absent in data statement |
| `SchemaValidationTypeMismatch` | 4605 | Field value type mismatch |

---

## POCO Code Generation

`AnvilCodeGen` converts a loaded `AnvilSchema` into a complete C# source file — no
Roslyn dependency, pure `StringBuilder`.  Each schema type maps to a C# declaration:

| Schema kind | C# output |
|---|---|
| `Enum` | `public enum TypeName { Val1, Val2, … }` |
| `Flags` | `[Flags] public enum TypeName { None = 0, Val1 = 1, Val2 = 2, … }` |
| `Object` | `public sealed class TypeName { … }` (or `record` when `UseRecords = true`) |

All AML identifiers are PascalCased in the output (`my_field` → `MyField`).

### Field type mapping

| `AnvilValueType` | C# type | Default initialiser |
|---|---|---|
| `Scalar` | `string` | `= string.Empty` |
| `Array` / `Tuple` | `string[]` | `= []` |
| `Object` | `object?` | *(none)* |
| other | `object?` | *(none)* |

### Usage

```csharp
// From a pre-loaded schema
var schema = AnvilSchema.Load("mods.asch")!;
var source = AnvilCodeGen.GenerateSource(schema);
File.WriteAllText("Config.g.cs", source);

// With options
var source = AnvilCodeGen.GenerateSource(schema, new CodeGenOptions {
    Namespace      = "MyMod.Config",
    UseRecords     = true,
    NullableEnabled = true,   // default — emits #nullable enable
    Indent          = "    ", // default — 4-space indent
});

// One-shot from file path — returns null on load failure
var source = AnvilCodeGen.GenerateSource("mods.asch");
if (source is null)
    Console.Error.WriteLine(Anvil.LastError);
```

### Example output

Given this schema:

```aml
#!aml
@[schema]
ModSide     : enum := (client, server, both)
BlockConfig := {
  id       := ""
  hardness := 1.0
}
```

`AnvilCodeGen.GenerateSource(schema, new CodeGenOptions { Namespace = "MyMod" })` produces:

```csharp
#nullable enable

namespace MyMod;

public enum ModSide
{
    Client,
    Server,
    Both,
}

public sealed class BlockConfig
{
    public string Id { get; set; } = string.Empty;
    public string Hardness { get; set; } = string.Empty;
}
```

### `CodeGenOptions` members

| Property | Type | Default | Description |
|---|---|---|---|
| `Namespace` | `string?` | `null` | File-scoped namespace declaration; omitted when `null` |
| `NullableEnabled` | `bool` | `true` | Emits `#nullable enable` at file top |
| `UseRecords` | `bool` | `false` | Emits `sealed record` instead of `sealed class` for Object types |
| `Indent` | `string` | `"    "` | One level of indentation (4 spaces) |

`CodeGenOptions.Default` is a shared instance with all defaults applied.

---
