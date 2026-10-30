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
