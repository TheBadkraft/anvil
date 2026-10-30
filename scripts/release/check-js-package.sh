#!/usr/bin/env bash
# check-js-package.sh <tarball> [js-repo] [core-repo] [core-tag]
#
# Release checks for anvil.js that need the tarball *installed*, not just extracted:
#   1. contents: the packed tree holds package.json (name anvil.js), src/, node/, dist/anvil.mjs, dist/anvil.global.js,
#      BUILD_INFO.json (implementation anvil.js), README.md, LICENSE.txt, and no test/, bench/, vendor/, scripts/, js/;
#   2. `npm install <tarball>` in an empty directory with no sibling checkouts; require('anvil.js') and a dynamic
#      import() both work, `implementation` is 'anvil.js', getVersion() equals BUILD_INFO's version+build;
#   3. the conformance corpus (taken from the core tag, vendor/ is not shipped) runs through the INSTALLED copy: the binding
#      commit named in BUILD_INFO supplies only the dump driver (js/dump.js, scripts/conformance.js); src/ and node/
#      are the installed package's. Every case must pass, and the count must equal the corpus size.
set -eu
tgz=$(readlink -f "${1:?tarball}"); repo=${2:-$HOME/Projects/repos/anvil.js}
core=${3:-$HOME/Projects/repos/anvil}; tag=${4:-}
work=$(mktemp -d /tmp/anvl-release-js.XXXXXX); trap 'rm -rf "$work"' EXIT
fail() { echo "FAIL js: $*"; exit 1; }
tar -tzf "$tgz" | sed 's#^[^/]*/##' | grep -v '^$' > "$work/list"
for f in package.json BUILD_INFO.json README.md LICENSE.txt dist/anvil.mjs dist/anvil.global.js node/index.js src/index.js src/build-info.js; do
  grep -qx "$f" "$work/list" || fail "tarball has no $f"; done
if bad=$(grep -E '^(test|tests|test-browser|bench|vendor|scripts|js|notes|node_modules|\.git)(/|$)' "$work/list"); then fail "tarball holds: $(echo "$bad" | head -3 | tr '\n' ' ')"; fi
echo "PASS js: contents (no tests, bench, vendor, scripts)"
mkdir "$work/proj" && cd "$work/proj" && npm init -y >/dev/null 2>&1
npm install --no-audit --no-fund "$tgz" >npm.log 2>&1 || { cat npm.log; fail "npm install"; }
pkg=$work/proj/node_modules/anvil.js
[ "$(node -p "require('anvil.js/package.json').name")" = anvil.js ] || fail "installed package name"
node - <<'JS' || fail "require/import"
const assert = require('assert');
const a = require('anvil.js');
const info = require('anvil.js/BUILD_INFO.json');
assert.strictEqual(a.implementation, 'anvil.js');
assert.strictEqual(a.getVersion(), `${info.native_version}+${info.build}`);
assert.strictEqual(a.parse('#!aml\nname := "David";\n').get('name').asString(), 'David');
(async () => {
  const m = await import('anvil.js');
  assert.strictEqual((m.default || m).implementation, 'anvil.js');
  assert.strictEqual(typeof (m.parse || m.default.parse), 'function');
  console.log(`PASS js: require and import work, implementation anvil.js, version ${a.getVersion()}`);
})().catch((e) => { console.error(e); process.exit(1); });
JS
cat > "$work/proj/esm.mjs" <<'JS'
import anvl from 'anvil.js';
const { parse, getVersion } = anvl;
if (anvl.implementation !== 'anvil.js' || typeof parse !== 'function' || !getVersion().includes('+')) process.exit(1);
console.log('PASS js: static ESM default import works (named imports from the CommonJS entry are not offered)');
JS
node "$work/proj/esm.mjs" || fail "static ESM import"
# conformance through the installed copy
commit=$(node -p "require('$pkg/BUILD_INFO.json').binding_commit")
mkdir "$work/conf" && cp -r "$pkg/src" "$pkg/node" "$pkg/package.json" "$pkg/BUILD_INFO.json" "$work/conf/"
git -C "$repo" archive "$commit" js scripts/conformance.js | tar -x -C "$work/conf" || fail "no driver at binding commit $commit in $repo"
ntag=${tag:-v$(node -p "require('$pkg/BUILD_INFO.json').version")}
mkdir -p "$work/conf/vendor/anvil" && git -C "$core" archive "$ntag" conformance | tar -x -C "$work/conf/vendor/anvil"
total=$(find "$work/conf/vendor/anvil/conformance/cases" -type f ! -name '*.dump' | wc -l)
out=$(cd "$work/conf" && node scripts/conformance.js 2>&1) || { echo "$out" | tail -8; fail "conformance through the installed copy"; }
echo "$out" | grep -q "^conformance (all): $total/$total passed, 0 failed" || { echo "$out"; fail "conformance count != corpus size $total"; }
echo "PASS js: conformance through the installed copy: $total/$total"
