#!/usr/bin/env bash
# check-readme.sh <package-dir>
#   1. a shipped README must not link outside the package (`](../x)`): those paths are sibling checkouts
#      that don't exist in an installed tarball;
#   2. a README that names dist/anvil.browser.js must be shipped with it (the file is in the package);
#   3. node/wasm/js: the README's ```js blocks that `require("anvil-node"|"anvil-wasm"|"anvil.js")` run against this
#      package (installed layout, via a scratch node_modules). The ```html blocks and the service-worker
#      (`// sw.js`) block are browser-only illustrations and are skipped, as is any block with a
#      `{ ... }` placeholder; a block that fails only with a ReferenceError (an undefined placeholder such as
#      `schemaText`) is a fragment and is skipped.
set -eu
pkg=${1:?package dir}; readme=$pkg/README.md
[ -f "$readme" ] || { echo "FAIL readme: $readme missing"; exit 1; }
if bad=$(grep -nE '\]\((\./)?\.\./' "$readme"); then
  echo "FAIL readme: links outside the package:"; echo "$bad" | sed 's/^/   /'; exit 1
fi
echo "PASS readme: no ../ links"
if grep -q 'anvil\.browser\.js' "$readme"; then
  [ -f "$pkg/dist/anvil.browser.js" ] || { echo "FAIL readme: names dist/anvil.browser.js, but the package does not ship it"; exit 1; }
  echo "PASS readme: the browser bundle it names is in the package"
fi
pkg=$(cd "$pkg" && pwd)
name=$(node -p "try{require('$pkg/package.json').name}catch(e){''}" 2>/dev/null || true)
case $name in anvil-node|anvil-wasm|anvil.js) ;; *) exit 0 ;; esac
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
mkdir -p "$work/node_modules"; ln -s "$pkg" "$work/node_modules/$name"
node - "$readme" "$name" "$work" <<'JS'
const fs = require('fs'), path = require('path');
const [readme, name, work] = process.argv.slice(2);
const blocks = [...fs.readFileSync(readme, 'utf8').matchAll(/```js\n([\s\S]*?)```/g)].map((m) => m[1])
  .filter((b) => b.includes(`require('${name}')`) && !/^\/\/ sw\.js/.test(b) && !/\{ \.\.\. \}/.test(b));
(async () => {
  let n = 0, skipped = 0;
  for (const b of blocks) {
    const fn = new Function('require', 'console', `return (async () => {\n${b}\n})()`);
    const req = (id) => require(require.resolve(id, { paths: [work] }));
    try { await fn(req, { log() {} }); n++; }
    catch (e) {
      if (e instanceof ReferenceError) { skipped++; continue; }   // a fragment that names an undefined placeholder (schemaText, source)
      console.log(`FAIL readme example ${n + 1}: ${e && e.message}\n${b.split('\n').slice(0, 4).join('\n')}`); process.exit(1);
    }
  }
  console.log(`PASS readme: ${n} js example(s) run against the package (${skipped} fragments with placeholders skipped)`);
})();
JS
