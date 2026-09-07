# anvl-js

**ANVL** — *Attributed · Node · Variadic · Language*

A JavaScript parser and serializer for ANVL — the AML (declarative
modelling) and AMP (restricted messaging) dialects. Pure JavaScript, no
native dependencies, full round-trip capability (parse → `AnvilNode` tree →
serialize).

> **Status:** alpha software. The AML/AMP grammar and API are stable
> enough to build against, but the API surface may still change before a
> 1.0 release.

## Quick start

```js
import { parse, write } from './src/index.js';

const source = `#!aml
server := {
    host := localhost;
    port := 8080;
};`;

const node = parse(source);
const config = node.get('server');
config.get('host').asString(); // "localhost"
config.get('port').asInt();    // 8080

write(node); // re-serializes the full document back to ANVL source text
```

If `parse()` fails, it returns `null` instead of throwing — check
[`lastError()`](wiki/API-Reference.md#lasterror) for details.

## Documentation

Full docs live in [`wiki/`](wiki/Home.md):

- **[Quick Start](wiki/Quick-Start.md)** — install, first parse, first write
- **[AML Guide](wiki/AML-Guide.md)** — objects, arrays, tuples, attributes, inheritance, `$` VarRefs, blobs
- **[AMP Guide](wiki/AMP-Guide.md)** — the restricted messaging dialect and what it forbids
- **[API Reference](wiki/API-Reference.md)** — every exported function and `AnvilNode` method
- **[How-To](wiki/How-To.md)** — task-based recipes (error handling, minified output, round-tripping, iterating collections)
- **[Error Codes](wiki/Error-Codes.md)** — full error code reference

## Testing

```bash
npm test          # run once
npm run test:watch
```

## License

MIT
