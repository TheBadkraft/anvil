# Quick Start

## Requirements

- Node.js 18+
- No runtime dependencies

## Using it in this repo

`anvl-js` isn't published to a package registry — import it by relative
path from wherever your code lives inside (or alongside) this repo:

```js
import { parse, write, lastError } from './src/index.js';
```

## Parsing your first document

```js
import { parse } from './src/index.js';

const source = `#!aml
server := {
    host := localhost;
    port := 8080;
};`;

const node = parse(source);
```

`parse()` never throws on a syntax error — it returns `null` instead. Always
check the result:

```js
if (!node) {
   const err = lastError();
   console.error(`${err.message} at ${err.line}:${err.column}`);
} else {
   const config = node.get('server');
   console.log(config.get('host').asString()); // "localhost"
   console.log(config.get('port').asInt());    // 8080
}
```

## Understanding what `parse()` returns

`parse()` always returns an object-like node keyed by every top-level
statement in the document, by name — the same shape you'd expect from a
JSON object, and the same shape regardless of how many statements there
are, including exactly one:

```js
const node = parse(`#!aml
base_var := 42;
derived_var := $base_var;`);

node.type;                        // "object"
node.keys();                      // ["base_var", "derived_var"]
node.get('derived_var').asInt();  // 42 — resolved, not the literal $base_var
node.get('base_var').asInt();     // 42
```

This is deliberate: an earlier version of this library unwrapped a
single-statement document directly to that statement's value, which meant
the shape you got back depended on how many statements happened to be in
the source — every consumer had to detect and branch on which shape they
got. One consistent shape, always, closes that off. It also means the
`server := {...}` example above needs `.get('server')` to reach the
object, even though there's only the one statement — there's no special
case for "just one."

## Writing it back out

```js
import { write } from './src/index.js';

write(node);                     // pretty-printed, full document
write(node, { minify: true });   // single-line, comma-separated statements
```

`write()` reconstructs the **entire original document** (every top-level
statement, not just the one `node` represents) — see
[How-To: Round-tripping](How-To.md#round-tripping) for the details and the
one caveat around nested nodes.

## Using it in a web client

`src/` is pure JavaScript with no Node dependencies except `load()` (file
reading), which fails gracefully outside Node instead of throwing. Three
built artifacts are checked into `dist/` for browser/bundler consumption
(rebuild with `npm run build` after any `src/` change):

- **`dist/anvl.esm.js`** — for bundlers or `<script type="module">`
- **`dist/anvl.cjs`** — for `require()`
- **`dist/anvl.global.js`** — a minified IIFE for a plain `<script>` tag with
  no build step; exposes a global `Anvl` object with the same API
  (`Anvl.parse`, `Anvl.write`, `Anvl.lastError`, `Anvl.AnvilNode`, ...)

```html
<script src="dist/anvl.global.js"></script>
<script>
  const node = Anvl.parse('#!aml\nfoo := 1;');
  console.log(node.get('foo').asInt());
</script>
```

`package.json`'s `exports` field points `import`/`require` at the `dist/`
builds (not `src/` directly) — so anything importing this package by name
rather than by relative path gets the built artifacts.

## Next steps

- [AML Guide](AML-Guide.md) if you're writing configuration/modelling documents
- [AMP Guide](AMP-Guide.md) if you're writing restricted messaging payloads
- [How-To](How-To.md) for common recipes
