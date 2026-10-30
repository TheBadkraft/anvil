#!/usr/bin/env bash
# verify-staged.sh <staging-dir> <release-version> <build> [published-downloads-dir]
# Extracts every staged tarball (core, py, net, anvil.js) and reruns the whole check set on the *extracted files*, then the
# real-browser check of anvil.js dist/ and the FlyWire guard (when the v0.11 node/wasm tarballs are at hand). On success writes
# SHA256SUMS-v<version> (this release only) and the cumulative SHA256SUMS: the published downloads' SHA256SUMS (which names
# every earlier tarball, each verified against the file on disk) plus this release.
# anvil.node and anvil.wasm are frozen and not released: nothing here builds or checks them.
set -u
dir=$(cd "${1:?staging dir}" && pwd); ver=${2:?version}; build=${3:?build}
pub=${4:-$HOME/Projects/repos/anvil/site/assets/downloads}
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
fail=0
run() { echo "-- $*"; "$@" || { echo "FAILED: $*"; fail=1; }; }
x() { mkdir -p "$work/$1" && tar -xzf "$dir/$2" -C "$work/$1" && find "$work/$1" -mindepth 1 -maxdepth 1 -type d | head -1; }

core=$(x core anvil-v$ver-linux-x86_64.tar.gz)
run "$here/check-grammar.sh" core "$core" "$ver" "$build"
run "$here/check-readme.sh" "$core"
run sh "$here/../check-exports.sh" "$core/lib/libanvil.so" "$core/include"
for k in py net; do
  p=$(x $k anvil-$k-v$ver-linux-x64.tar.gz)
  run "$here/check-grammar.sh" $k "$p" "$ver" "$build"
  run "$here/check-readme.sh" "$p"
  so=$(find "$p" -name libanvil.so | head -1)
  run sh "$here/../check-exports.sh" "$so" "$core/include"
  cmp -s "$so" "$core/lib/libanvil.so" && echo "note: $k libanvil.so is byte-identical to core's" || echo "note: $k libanvil.so differs from core's (built separately; version and exports checked above)"
done
js=$(x js anvil.js-v$ver.tar.gz)
run "$here/check-grammar.sh" js "$js" "$ver" "$build"
run "$here/check-readme.sh" "$js"
run "$here/check-js-package.sh" "$dir/anvil.js-v$ver.tar.gz" "${JS_REPO:-$HOME/Projects/repos/anvil.js}" "${CORE_REPO:-$HOME/Projects/repos/anvil}" "v$ver"
run "$here/check-browser-js.sh" "$js" "${JS_REPO:-$HOME/Projects/repos/anvil.js}"
if [ $fail -eq 0 ]; then
  ( cd "$dir"; sha256sum anvil*-v$ver*.tar.gz anvil.js-v$ver.tar.gz 2>/dev/null | sort -k2 -u > SHA256SUMS-v$ver
    if [ -f "$pub/SHA256SUMS" ]; then ( cd "$pub" && sha256sum -c SHA256SUMS >/dev/null ) || { echo "FAIL published SHA256SUMS does not match the published files"; exit 1; }; cat "$pub/SHA256SUMS" SHA256SUMS-v$ver > SHA256SUMS; else cp SHA256SUMS-v$ver SHA256SUMS; fi
    sha256sum -c SHA256SUMS-v$ver ) || fail=1
else echo "NOT writing SHA256SUMS: a check failed"; fi
exit $fail
