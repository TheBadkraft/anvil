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
for f in package.json BUILD_INFO.json README.md LICENSE.txt dist/anvil.mjs dist/anvil.global.js node/index.js node/index.mjs src/index.js src/build-info.js; do
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
import anvl, { parse, parseRawValue, lastError, getVersion, implementation, AnvilWriter, AnvilDialect, WriterErrorCode, AnvilWriterError } from 'anvil.js';
const names = { parse, parseRawValue, lastError, getVersion, AnvilWriter, AnvilDialect, WriterErrorCode, AnvilWriterError };
for (const [k, v] of Object.entries(names)) if (typeof v === 'undefined') { console.error('missing named export ' + k); process.exit(1); }
if (implementation !== 'anvil.js' || anvl.implementation !== 'anvil.js' || anvl.AnvilWriter !== AnvilWriter) process.exit(1);
console.log('PASS js: ESM named imports (parse, AnvilWriter, AnvilDialect, WriterErrorCode, AnvilWriterError, ...) and the default import work');
JS
node "$work/proj/esm.mjs" || fail "ESM named imports from the installed package"
node -e "const e=require('anvil.js/package.json').exports['.']; if(!e.import||!/index\.mjs$/.test(e.import)||!e.require) process.exit(1)" || fail "package.json exports lacks the import (node/index.mjs) and require conditions"
echo "PASS js: package.json exports carry the import and require conditions"
node - <<'JS' || fail "lastError code / K&C writer"
const assert = require('assert');
const a = require('anvil.js');
assert.strictEqual(a.parse('#!aml\no := { x := 1; x := 2; };\n'), null);
const e = a.lastError();
assert.strictEqual(e.code, 4001, 'lastError().code'); assert.strictEqual(typeof e.name, 'string'); assert.ok(e.line >= 1 && e.column >= 1);
console.log(`PASS js: lastError() for a duplicate key is code ${e.code} (${e.name})`);
const w = a.AnvilWriter.create(a.AnvilDialect.AMP);
const text = w.statement('utid').blob('utid', 'u-1').statement('seq').value(9007199254740991)
  .statement('payload').beginTuple().value(1).string('a"b\\c\n').bare('ok').blob('t', 'x := 1;').endTuple().finish();
assert.strictEqual(text, '#!amp\n\nutid := @utid`u-1`;\nseq := 9007199254740991;\npayload := (1, "a\\"b\\\\c\\n", ok, @t`x := 1;`);\n');
assert.strictEqual(a.parse(text).get('seq').asInt(), 9007199254740991);
let code = -1; try { a.AnvilWriter.create(a.AnvilDialect.AMP).statement('x').beginObject(); } catch (er) { code = er.code; }
assert.strictEqual(code, a.WriterErrorCode.DIALECT);
console.log('PASS js: the writer produces the K&C envelope byte for byte from the installed package, and round-trips');
JS
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
# the writer corpus through the installed copy (the driver comes from the binding commit; the corpus from the core tag)
git -C "$repo" archive "$commit" test/writer-c-api-only.json scripts/writer-conformance.js | tar -x -C "$work/conf" || fail "no writer driver at $commit"
git -C "$repo" archive "$commit" js | tar -x -C "$work/conf"
wtotal=$(find "$work/conf/vendor/anvil/conformance/cases/writer" -type f -name '*.wscript' | wc -l)
wout=$(cd "$work/conf" && node scripts/writer-conformance.js 2>&1) || { echo "$wout" | tail -8; fail "writer corpus through the installed copy"; }
echo "$wout" | grep -q "^writer corpus: .* = $wtotal; 0 differ, 0 unlisted, 0 bad list entries" || { echo "$wout" | tail -3; fail "writer corpus count != $wtotal"; }
echo "PASS js: writer corpus through the installed copy: $(echo "$wout" | tail -1 | sed 's/^writer corpus: //')"
