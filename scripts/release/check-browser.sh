#!/usr/bin/env bash
# check-browser.sh <extracted-wasm-package-dir> [wasm-binding-repo]
#
# A real-browser check of the *packaged* anvil-wasm: the binding's own test-browser/ (taken from the
# binding commit recorded in the package's BUILD_INFO.json) is laid over the extracted package's lib/
# and dist/, and run-browser.sh runs it in Firefox and Chromium, in glue mode (page.html, lib/index.js
# over dist/anvil.js) and bundle mode (bundle.html, dist/anvil.browser.js through a <script> tag).
#   Firefox:  TMPDIR unset (the snap profile must live under $HOME).
#   Chromium: Playwright's Chrome for Testing, TMPDIR=/tmp/<short>. CHROMIUM=path overrides the lookup,
#             which otherwise searches $PLAYWRIGHT_BROWSERS_PATH and ~/.cache/ms-playwright.
set -u
pkg=$(cd "${1:?extracted package dir}" && pwd); repo=${2:-$HOME/Projects/repos/anvil.wasm}
for f in dist/anvil.js dist/anvil.wasm dist/anvil.browser.js lib/index.js; do
  [ -f "$pkg/$f" ] || { echo "FAIL browser: package has no $f"; exit 1; }
done
commit=$(node -p "require('$pkg/BUILD_INFO.json').binding_commit")
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
git -C "$repo" archive "$commit" test-browser | tar -x -C "$work" || { echo "FAIL browser: no test-browser/ at $commit in $repo"; exit 1; }
cp -r "$pkg/lib" "$pkg/dist" "$work/"      # only the packaged files: nothing from the build tree
chromium=${CHROMIUM:-$(find "${PLAYWRIGHT_BROWSERS_PATH:-$HOME/.cache/ms-playwright}" -type f \( -name chrome -o -name chromium \) -perm -u+x 2>/dev/null | head -1)}
rc=0
run() { # <label> <browser> <mode> [env prefix...]
  local label=$1 b=$2 mode=$3; shift 3
  if out=$(cd "$work" && "$@" test-browser/run-browser.sh "$b" "$mode" 2>&1); then
    echo "PASS browser: $label ($(echo "$out" | sed -n 's/^agent: //p' | grep -oE '(Firefox|Chrome)/[0-9.]+' | head -1), $(echo "$out" | grep -c '^PASS') checks)"
  else echo "FAIL browser: $label"; echo "$out" | sed 's/^/   /'; rc=1; fi
}
for mode in glue bundle; do run "firefox $mode" firefox "$mode" env -u TMPDIR; done
if [ -n "$chromium" ]; then
  mkdir -p /tmp/anvl-rb; for mode in glue bundle; do run "chromium $mode" "$chromium" "$mode" env TMPDIR=/tmp/anvl-rb; done
else echo "FAIL browser: no Chromium found (install Playwright's: npx playwright install chromium)"; rc=1; fi
exit $rc
