#!/usr/bin/env bash
# check-install.sh <node|wasm> <tarball> <release-version>
# `npm install <tarball>` in an empty directory with no sibling checkouts, then require() works and
# (surface) Schema is exposed. A wasm package that deliberately doesn't carry schema must say so in
# its README ("does not carry schema"); otherwise Schema is required.
set -eu
kind=${1:?node|wasm}; tgz=$(readlink -f "${2:?tarball}"); ver=${3:?version}
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT; cd "$work"
npm init -y >/dev/null 2>&1
npm install --no-audit --no-fund --ignore-scripts=false "$tgz" >npm.log 2>&1 || { cat npm.log; echo "FAIL npm install"; exit 1; }
pkgname=anvil-$kind
installed=$(node -p "require('./node_modules/$pkgname/package.json').version")
[ "$installed" = "$ver" ] || { echo "FAIL package.json version $installed != $ver"; exit 1; }
if [ "$kind" = wasm ]; then
  # The browser bundle ships, is listed in `files`, and loads in a no-require sandbox with the same keys.
  node -e "const p=require('./node_modules/$pkgname/package.json'); process.exit(p.files.includes('dist/anvil.browser.js')?0:1)" \
    || { echo "FAIL package.json files does not list dist/anvil.browser.js"; exit 1; }
  [ -f "node_modules/$pkgname/dist/anvil.browser.js" ] || { echo "FAIL installed package has no dist/anvil.browser.js"; exit 1; }
  bout=$(node "$(dirname "$0")/probes/probe-bundle.js" "node_modules/$pkgname" 2>&1)
  { echo "$bout" | grep -qx 'bundle=ok' && echo "$bout" | grep -qx 'bundle_keys=same' && echo "$bout" | grep -q '^bundle_version=[0-9]'; } \
    || { echo "FAIL bundle: $bout"; exit 1; }
  echo "PASS bundle: dist/anvil.browser.js loads in a no-require sandbox, same keys, ready() and getVersion() work"
fi
node - "$pkgname" "$kind" <<'JS'
const [name, kind] = process.argv.slice(2);
const fs = require('fs');
(async () => {
  const m = require(name);
  if (kind === 'wasm') await m.ready();
  if (typeof m.Schema === 'function') { console.log('PASS surface: Schema is a function'); return; }
  const readme = fs.readFileSync(`node_modules/${name}/README.md`, 'utf8');
  if (kind === 'wasm' && /does not carry schema/i.test(readme)) { console.log('PASS surface: README states no schema'); return; }
  console.error('FAIL surface: Schema is not a function'); process.exit(1);
})().catch(e => { console.error('FAIL require:', e.message); process.exit(1); });
JS
echo "PASS install: $pkgname $installed from $(basename "$tgz")"
