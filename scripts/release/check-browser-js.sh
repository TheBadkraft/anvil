#!/usr/bin/env bash
# check-browser-js.sh <extracted-anvil.js-package-dir> [js-repo]
#
# A real-browser check of the *packaged* anvil.js dist/ builds (esm and global) in Chromium and Firefox, through anvil.wasm's
# contained harness only (test-browser/run-browser.sh -> contain.sh: memory- and time-capped systemd scope, one run at a time,
# state only under /tmp/anvl-browser-test). The binding's own test-browser/ (from the commit in BUILD_INFO.json) is laid over
# a scratch copy of the extracted package's dist/, node/, src/ and package.json. This script never starts a browser itself.
#   1. prove-harness.sh first (no browser started); abort if it fails.
#   2. install-browsers.sh only if /tmp/anvl-browser-test/browsers is missing.
#   3. exit 75 (lock held) is a FAIL; a pass must name Firefox or HeadlessChrome in its agent line.
set -u
pkg=$(cd "${1:?extracted package dir}" && pwd); repo=${2:-$HOME/Projects/repos/anvil.js}
wasm=${ANVIL_WASM_ROOT:-$HOME/Projects/repos/anvil.wasm}
for f in dist/anvil.mjs dist/anvil.global.js node/index.js src/index.js BUILD_INFO.json; do
  [ -f "$pkg/$f" ] || { echo "FAIL browser: package has no $f"; exit 1; }
done
commit=$(node -p "require('$pkg/BUILD_INFO.json').binding_commit")
work=$(mktemp -d /tmp/anvl-release-browser.XXXXXX); trap 'rm -rf "$work"' EXIT
git -C "$repo" archive "$commit" test-browser | tar -x -C "$work" || { echo "FAIL browser: no test-browser/ at $commit in $repo"; exit 1; }
cp -r "$pkg/dist" "$pkg/node" "$pkg/src" "$pkg/package.json" "$work/"
# Release overlay: a FlyWire-shaped check (FlyWire's own assets schema and its types, read-only from the FlyWire checkout)
# is inserted into the scratch copy of checks.js. The builder emits a row document and the schema validates it in the browser.
fw=${FLYWIRE_REPO:-$HOME/Projects/repos/flywire}
if [ -f "$fw/schemas/assets.meta.anvl" ] && [ -f "$fw/schema-gen/postgres.types.anvl" ]; then
  node - "$fw" "$work/test-browser/checks.js" <<'JS' || { echo "FAIL browser: could not add the FlyWire-shaped check"; exit 1; }
const fs = require('fs');
const [fw, target] = process.argv.slice(2);
const types = JSON.stringify(fs.readFileSync(fw + '/schema-gen/postgres.types.anvl', 'utf8'));
const schema = JSON.stringify(fs.readFileSync(fw + '/schemas/assets.meta.anvl', 'utf8'));
const snippet = `   check('FlyWire-shaped: assets schema (types through setIo), builder row, schema verdicts', () => {
      const files = { './schema-gen/postgres.types.anvl': ${types} };
      anvl.setIo({ read: (p) => (p in files ? files[p] : null) });
      try {
         const schema = anvl.Schema.load(${schema});
         eq(schema === null, false, 'schema loads');
         eq(anvl.Schema.load('#!aml\\nx := 1;\\n'), null, 'no @[schema]'); eq(anvl.lastError().code, anvl.SchemaErrorCode.ATTRIBUTE_MISSING, 'schema code 11');
         eq(anvl.TypeRegistry.load('#!aml\\nx := 1;\\n'), null, 'no @[types]'); eq(anvl.lastError().code, anvl.TypeErrorCode.ATTRIBUTE_MISSING, 'types code 1');
         eq(schema.fields.length, 12, 'twelve fields');
         const row = (vin, status) => { const b = anvl.AnvilBuilder.create(anvl.AnvilDialect.AML);
            b.add('asset_id', b.value(7)); b.add('asset_type', b.value(2)); b.add('vin_serial', b.string(vin)); b.add('make', b.string('Ford'));
            b.add('model', b.string('F150')); b.add('year', b.value(2020)); b.add('fleet_id', b.string('f-1')); b.add('status', b.string(status)); return b.emit(); };
         eq(schema.validate(row('1HGCM82633A004352', 'active')).valid, true, 'a good row is valid');
         const bad = schema.validate(row('1HGCM82633A004352', 'lost'));
         eq(bad.valid, false, 'a status outside the values is invalid'); eq(bad.violations[0].field, 'status', 'violation names status');
         eq(anvl.parse(row('1HGCM82633A004352', 'active')).get('year').asInt(), 2020, 'builder bytes read back');
      } finally { anvl.setIo(null); }
   });
`;
let s = fs.readFileSync(target, 'utf8');
const mark = '   const descriptions = {};';
if (!s.includes(mark)) throw new Error('no insertion point in checks.js');
fs.writeFileSync(target, s.replace(mark, snippet + mark));
JS
else
  echo "FAIL browser: FlyWire files not found in $fw"; exit 1
fi
cd "$work"
"$wasm/test-browser/prove-harness.sh" >"$work/prove.log" 2>&1 || { tail -20 "$work/prove.log"; echo "FAIL browser: prove-harness.sh failed; no browser was started"; exit 1; }
echo "PASS browser: harness proof (no browser started)"
if [ ! -d /tmp/anvl-browser-test/browsers ]; then
  "$wasm/test-browser/install-browsers.sh" >"$work/install.log" 2>&1 || { tail -20 "$work/install.log"; echo "FAIL browser: install-browsers.sh failed"; exit 1; }
fi
rc=0
for b in chromium firefox; do for mode in esm global; do
  # The harness self-check (exit 99, or 75 for the lock) can trip while the previous scope is still being torn down. A run
  # that exits 99/75 is retried ONCE, only after no Playwright browser process is left; the retry is printed, never silent.
  out=$(ANVIL_WASM_ROOT="$wasm" test-browser/run-browser.sh $b $mode 2>&1); code=$?
  if [ $code -eq 99 ] || [ $code -eq 75 ]; then
    for _ in $(seq 1 30); do pgrep -f /tmp/anvl-browser-test/browsers >/dev/null || break; sleep 1; done
    if pgrep -f /tmp/anvl-browser-test/browsers >/dev/null; then echo "FAIL browser: $b $mode: a browser process is still alive after 30 s"; rc=1; continue; fi
    echo "note browser: $b $mode: harness exit $code (self-check/lock race); no browser left; running it once more"
    out=$(ANVIL_WASM_ROOT="$wasm" test-browser/run-browser.sh $b $mode 2>&1); code=$?
  fi
  agent=$(printf '%s\n' "$out" | sed -n 's/^agent: //p' | grep -oE '(Firefox|HeadlessChrome)/[0-9.]+' | head -1)
  if [ $code -eq 0 ] && [ -n "$agent" ]; then
    echo "PASS browser: $b $mode ($agent, $(printf '%s\n' "$out" | grep -c '^PASS') checks)"
  else
    [ $code -eq 75 ] && echo "FAIL browser: $b $mode: harness lock held by another run (exit 75)" || echo "FAIL browser: $b $mode (exit $code, agent '${agent:-none}')"
    printf '%s\n' "$out" | sed 's/^/   /' | tail -25; rc=1
  fi
done; done
exit $rc
