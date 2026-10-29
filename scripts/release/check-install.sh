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
