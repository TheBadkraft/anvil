#!/usr/bin/env bash
# check-browser.sh <extracted-wasm-package-dir> [wasm-binding-repo]
#
# A real-browser check of the *packaged* anvil-wasm. The binding's own test-browser/ (taken from the
# binding commit recorded in the package's BUILD_INFO.json) is laid over a scratch copy of the extracted
# package's lib/ and dist/, and its contained harness runs the page in Chromium and Firefox, in glue mode
# (lib/index.js over dist/anvil.js) and bundle mode (dist/anvil.browser.js through a <script> tag), one
# run at a time. This script never starts a browser itself, never sets TMPDIR, and uses no system or snap
# browser: everything goes through test-browser/run-browser.sh (anvil.wasm's contained harness: a memory-
# and time-capped systemd scope, state only under /tmp/anvl-browser-test).
#   1. prove-harness.sh first (no browser started); abort if it fails.
#   2. install-browsers.sh only if /tmp/anvl-browser-test/browsers is missing.
#   3. exit 75 (the harness lock is held by another run) is a FAIL, not a retry.
#   4. a pass must name Firefox or HeadlessChrome in its agent line.
set -u
pkg=$(cd "${1:?extracted package dir}" && pwd); repo=${2:-$HOME/Projects/repos/anvil.wasm}
for f in dist/anvil.js dist/anvil.wasm dist/anvil.browser.js lib/index.js BUILD_INFO.json; do
  [ -f "$pkg/$f" ] || { echo "FAIL browser: package has no $f"; exit 1; }
done
commit=$(node -p "require('$pkg/BUILD_INFO.json').binding_commit")
work=$(mktemp -d /tmp/anvl-release-browser.XXXXXX); trap 'rm -rf "$work"' EXIT
git -C "$repo" archive "$commit" test-browser | tar -x -C "$work" || { echo "FAIL browser: no test-browser/ at $commit in $repo"; exit 1; }
[ -x "$work/test-browser/run-browser.sh" ] && [ -f "$work/test-browser/prove-harness.sh" ] \
  || { echo "FAIL browser: $commit has no contained harness (test-browser/run-browser.sh, prove-harness.sh)"; exit 1; }
cp -r "$pkg/lib" "$pkg/dist" "$work/"      # only the packaged files: nothing from the build tree
cd "$work"
test-browser/prove-harness.sh >"$work/prove.log" 2>&1 || { tail -20 "$work/prove.log"; echo "FAIL browser: prove-harness.sh failed; no browser was started"; exit 1; }
echo "PASS browser: harness proof (no browser started)"
if [ ! -d /tmp/anvl-browser-test/browsers ]; then
  test-browser/install-browsers.sh >"$work/install.log" 2>&1 || { tail -20 "$work/install.log"; echo "FAIL browser: install-browsers.sh failed"; exit 1; }
fi
rc=0
for b in chromium firefox; do for mode in glue bundle; do
  out=$(test-browser/run-browser.sh $b $mode 2>&1); code=$?
  agent=$(printf '%s\n' "$out" | sed -n 's/^agent: //p' | grep -oE '(Firefox|HeadlessChrome)/[0-9.]+' | head -1)
  if [ $code -eq 0 ] && [ -n "$agent" ]; then
    echo "PASS browser: $b $mode ($agent, $(printf '%s\n' "$out" | grep -c '^PASS') checks)"
  else
    [ $code -eq 75 ] && echo "FAIL browser: $b $mode: harness lock held by another run (exit 75)" || echo "FAIL browser: $b $mode (exit $code, agent '${agent:-none}')"
    printf '%s\n' "$out" | sed 's/^/   /' | tail -25; rc=1
  fi
done; done
exit $rc
