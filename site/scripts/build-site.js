// build-site.js — moved here verbatim from anvil.js/scripts/build-site.js during the site/
// relocation (anvil.js -> anvil, since Anvil.C is now the de facto reference implementation).
// NOT reworked yet -- this is a structural move only, not a content rewrite. As written, this
// script still assumes anvil.js's own shape (a `dist/` of JS bundles, a `wiki/` of anvl-js-
// specific doc pages, `README.md` at the repo root) and will fail if run as-is from this new
// location, since none of those exist here in that shape. See notes/deferred-work.md's site
// entry for the real rework this needs: which docs/bundles the *rewritten* site should pull
// from (this repo's own wiki, plus anvil.node's/anvil.net's/etc.), before this script is safe
// to run again.

import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const site = path.join(root, 'site');
const pkg = JSON.parse(await fs.readFile(path.join(root, 'package.json'), 'utf8'));

// --- 1. Bundle files. Never copy .map — esbuild's sourcemap embeds the full
// original source (sourcesContent) for every file, which would publish the
// entire private src/ tree if it ever reached a public deploy.
const distOut = path.join(site, 'assets', 'dist');
await fs.mkdir(distOut, { recursive: true });
for (const file of ['anvl.esm.js', 'anvl.cjs', 'anvl.global.js']) {
   await fs.copyFile(path.join(root, 'dist', file), path.join(distOut, file));
}

// --- 2. Docs. Explicit allowlist only — Home.md and Docs-Server.md are
// internal-only (the latter documents private Linode ops) and must never be
// copied, defense-in-depth beyond just not linking them from docs.html.
const docsOut = path.join(site, 'assets', 'docs');
await fs.mkdir(docsOut, { recursive: true });

// Strips the private design-spec cross-reference, which names other
// internal projects not meant for a public page. Exact literal replacement,
// not regex — verified against the current wording of each file below;
// re-check these if the source wiki pages are reworded.
const LITERAL_REPLACEMENTS = [
   [
      `> **Status:** interim implementation. This is the production parser for AML
> and AMP consumers until the canonical C parser's WASM build lands. See
> [\`anvl-js-parser-spec.md\`](anvl-js-parser-spec.md) for the full design spec
> this implementation was built against.`,
      `> **Status:** alpha software. The AML/AMP grammar and API are stable
> enough to build against, but the API surface may still change before a
> 1.0 release.`,
   ],
   [
      `become reachable from AMP just because it shares a code path. If you're
extending the parser, preserve that separation; see
[\`anvl-js-parser-spec.md\` §6.4](../anvl-js-parser-spec.md) for the full
rationale.`,
      `become reachable from AMP just because it shares a code path. This
separation is enforced deliberately, not just by convention.`,
   ],
];

function stripSpecReference(text) {
   let out = text;
   for (const [needle, replacement] of LITERAL_REPLACEMENTS) {
      out = out.replaceAll(needle, replacement);
   }
   return out;
}

const readme = await fs.readFile(path.join(root, 'README.md'), 'utf8');
await fs.writeFile(path.join(docsOut, 'README.md'), stripSpecReference(readme));

const allowedWikiPages = [
   'Quick-Start.md',
   'AML-Guide.md',
   'AMP-Guide.md',
   'API-Reference.md',
   'How-To.md',
   'Error-Codes.md',
];
for (const name of allowedWikiPages) {
   const text = await fs.readFile(path.join(root, 'wiki', name), 'utf8');
   await fs.writeFile(path.join(docsOut, name), stripSpecReference(text));
}

// --- 3. Version stamping. index.html/download.html read window.ANVL_VERSION
// at runtime (falling back to the literal "__VERSION__" placeholder if this
// script never ran) rather than having their HTML rewritten in place — that
// keeps this script safely re-runnable without mutating checked-in templates.
await fs.writeFile(path.join(site, 'assets', 'version.js'), `window.ANVL_VERSION = ${JSON.stringify(pkg.version)};\n`);

// --- 4. Safety net: never let a spec/internal-project reference reach the
// public docs, even if the literal replacements above go stale after a
// future wiki edit.
const forbidden = ['anvl-js-parser-spec', 'CabNet', 'FlyWire', 'Anvil UX'];
const producedFiles = [
   path.join(docsOut, 'README.md'),
   ...allowedWikiPages.map((name) => path.join(docsOut, name)),
];
for (const filePath of producedFiles) {
   const text = await fs.readFile(filePath, 'utf8');
   for (const term of forbidden) {
      if (text.includes(term)) {
         throw new Error(`Site build aborted: "${term}" found in ${filePath} — this must not reach the public site.`);
      }
   }
}

console.log(`Site build complete: version ${pkg.version}, ${allowedWikiPages.length + 1} doc pages, 3 bundle files.`);
