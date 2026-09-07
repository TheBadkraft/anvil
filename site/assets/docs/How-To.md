# How-To

Task-based recipes. See [API Reference](API-Reference.md) for full method
signatures.

## Handle parse errors

`parse()` and `load()` never throw for syntax/structural problems — always
check the return value:

```js
import { parse, lastError } from './src/index.js';

const node = parse(source);
if (!node) {
   const err = lastError();
   console.error(`Parse failed [${err.code}] at ${err.line}:${err.column}: ${err.message}`);
   return;
}
```

`lastError()` only reflects the most recent call — read it immediately
after `parse()`/`load()`, before parsing anything else.

## Read a file from disk

```js
import { load, lastError } from './src/index.js';

const node = await load('./config.anvl');
if (!node) {
   console.error(lastError());
}
```

`load()` is async and Node.js-only. It resolves `null` with an
`INVALID_PATH` error if called somewhere `node:fs/promises` isn't available
(e.g. a browser bundle).

## Round-tripping

```js
import { parse, write } from './src/index.js';

const node = parse(source);
const text = write(node);          // pretty-printed
const min  = write(node, { minify: true }); // single line, forced commas
```

`write()` reconstructs the **entire original document** — every top-level
statement — not just the value `node` happens to represent. This works
because `write()` follows a back-reference from `node` to its parsed
document.

**Caveat:** that back-reference only exists on **top-level statement
values** — the node `parse()`/`load()` gave you directly, and each of its
immediate fields via `.get(name)`, since those are top-level statements
too. Anything nested deeper than that — an object field's own field, an
array element, whatever `.get()` or `.at()` gets you from *there* — has no
document to reconstruct, and `write()` returns `""`. When in doubt, call
`write()` on the node `parse()`/`load()` gave you rather than something
you navigated further into.

## Produce minified output

```js
write(node, { minify: true });
```

Minified output always includes a comma between top-level statements, even
though the parser accepts them as optional — this keeps minified output
unambiguous to re-parse. Non-top-level separators (object fields, array/
tuple elements) already require commas regardless of minify.

## Iterate a collection

Objects, arrays, and tuples are all iterable:

```js
for (const item of node) {
   console.log(item.type, item.asString?.() ?? item.type);
}
```

For objects, prefer `.entries()` if you need the key alongside the value:

```js
for (const [key, value] of node.entries()) {
   console.log(key, '=', value.asString());
}
```

## Work with resolved `$` VarRefs

VarRefs are resolved once, statically, before you ever see the parsed
result — there's no live binding or resolver object to interact with at
runtime. By the time `parse()` returns, `$identifier` values have already
become whatever they resolved to:

```js
const node = parse(`#!aml
base    := 10;
derived := $base;`);

node.get('derived').asInt(); // 10 — already resolved, indistinguishable from a literal 10
```

An unmatched or circular reference is already `null` by the time you see
it:

```js
const node = parse('#!aml\nfoo := $missing;');
node.get('foo').isNull(); // true
```

If you need to know whether a value in your *source* was a literal or a
reference, that distinction doesn't survive parsing — check for it before
parsing, or treat "resolves to `null`" as your signal for "this reference
didn't resolve," same as the spec intends.

## Check inheritance without merging

```js
if (node.hasBase()) {
   const baseName = node.baseIdentifier();
   // field merging isn't implemented — look up and merge yourself if needed
}
```

## Distinguish AML from AMP at runtime

The dialect isn't exposed directly on `AnvilNode`. If your code needs to
branch on it, inspect the source text's shebang before calling `parse()`,
or rely on the fact that AMP-forbidden constructs will simply fail to
parse with one of the `AMP_*` error codes (see [Error Codes](Error-Codes.md)).
