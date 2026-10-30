'use strict';
// Release probe for anvil-wasm's browser bundle: argv[2] = extracted package dir.
// Loads dist/anvil.browser.js the way a page's <script> tag does, in a vm sandbox with no `require`,
// `module`, `exports` or `process`, a stubbed `fetch` serving dist/anvil.wasm, and compares
// `window.anvl` with `require(<package>)`. Prints key=value lines for check-grammar.sh:
//   bundle=ok|missing|<error>  bundle_keys=same|<difference>  bundle_version=...  bundle_include=...  bundle_name=...
const fs = require('fs');
const path = require('path');
const vm = require('vm');

(async () => {
  const pkg = path.resolve(process.argv[2]);
  const file = path.join(pkg, 'dist', 'anvil.browser.js');
  if (!fs.existsSync(file)) { console.log('bundle=missing'); return; }
  const cjs = require(pkg);
  const wasm = fs.readFileSync(path.join(pkg, 'dist', 'anvil.wasm'));
  const sandbox = {
    document: { currentScript: { src: 'http://localhost/static/anvil.browser.js' }, baseURI: 'http://localhost/' },
    location: { href: 'http://localhost/' },
    fetch: () => Promise.resolve(new Response(wasm, { status: 200, headers: { 'content-type': 'application/wasm' } })),
    performance, Response, WebAssembly, TextDecoder, TextEncoder, URL, console, setTimeout, clearTimeout, queueMicrotask,
  };
  sandbox.window = sandbox; sandbox.self = sandbox; sandbox.globalThis = sandbox;
  vm.createContext(sandbox);
  const globals = vm.runInContext('typeof require + typeof process + typeof module + typeof exports', sandbox);
  if (globals !== 'undefinedundefinedundefinedundefined') { console.log('bundle=sandbox has CommonJS/Node globals: ' + globals); return; }
  vm.runInContext(fs.readFileSync(file, 'utf8'), sandbox, { filename: 'anvil.browser.js' });
  const a = sandbox.window.anvl;
  if (!a) { console.log('bundle=window.anvl is not defined'); return; }
  console.log('bundle=ok');
  const want = Object.keys(cjs).sort(), got = Object.keys(a).sort();
  const missing = want.filter((k) => !got.includes(k)), extra = got.filter((k) => !want.includes(k));
  console.log('bundle_keys=' + (missing.length || extra.length ? `missing [${missing}] extra [${extra}]` : 'same'));
  await a.ready();
  console.log(`bundle_version=${a.getVersion()}`);
  const inc = a.parse('#!aml\ninclude "missing.anvl";\nx := 1;\n');
  const e = a.lastError();
  console.log(`bundle_include=${inc === null ? (e && e.message) || '<null, no error>' : '<parsed without error>'}`);
  const doc = a.parse('#!aml\nname := "David";\n');
  console.log(`bundle_name=${doc ? doc.get('name').asString() : '<none>'}`);
})().catch((e) => console.log('bundle=' + String(e && e.message || e).split('\n')[0]));
