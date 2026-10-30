#!/usr/bin/env bash
# verify-staged.sh <staging-dir> <release-version> <build>
# Extracts every staged tarball and reruns the whole check set on the *extracted files*, then the
# FlyWire guard, the real-browser check of the wasm package and the checksum file. Writes SHA256SUMS when it passes everything else.
set -u
dir=$(cd "${1:?staging dir}" && pwd); ver=${2:?version}; build=${3:?build}
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
fail=0
run() { echo "-- $*"; "$@" || { echo "FAILED: $*"; fail=1; }; }
x() { mkdir -p "$work/$1" && tar -xzf "$dir/$2" -C "$work/$1" && find "$work/$1" -mindepth 1 -maxdepth 1 -type d | head -1; }

core=$(x core anvil-v$ver-linux-x86_64.tar.gz)
run "$here/check-grammar.sh" core "$core" "$ver" "$build"
run "$here/check-readme.sh" "$core"
run sh "$here/../check-exports.sh" "$core/lib/libanvil.so" "$core/include"
run sh "$here/../check-cxx.sh" "$core/include" "$core/lib" "$here/../../test/infra/cxx"
for k in py net; do
  p=$(x $k anvil-$k-v$ver-linux-x64.tar.gz)
  run "$here/check-grammar.sh" $k "$p" "$ver" "$build"
  run "$here/check-readme.sh" "$p"
  so=$(find "$p" -name libanvil.so | head -1)
  run sh "$here/../check-exports.sh" "$so" "$core/include"
  cmp -s "$so" "$core/lib/libanvil.so" && echo "note: $k libanvil.so is byte-identical to core's" || echo "note: $k libanvil.so differs from core's (built separately; version and exports checked above)"
done
node=$(x node anvil-node-v$ver-linux-x86_64.tar.gz); run "$here/check-grammar.sh" node "$node" "$ver" "$build"; run "$here/check-readme.sh" "$node"
wasm=$(x wasm anvil-wasm-v$ver.tar.gz);              run "$here/check-grammar.sh" wasm "$wasm" "$ver" "$build"; run "$here/check-readme.sh" "$wasm"
run "$here/check-browser.sh" "$wasm" "${WASM_REPO:-$HOME/Projects/repos/anvil.wasm}"
run "$here/check-install.sh" node "$dir/anvil-node-v$ver-linux-x86_64.tar.gz" "$ver"
run "$here/check-install.sh" wasm "$dir/anvil-wasm-v$ver.tar.gz" "$ver"
run "$here/check-downstream.sh" "$dir/anvil-node-v$ver-linux-x86_64.tar.gz" "$dir/anvil-wasm-v$ver.tar.gz"
if [ $fail -eq 0 ]; then "$here/checksums.sh" write "$dir" && "$here/checksums.sh" verify "$dir"; else echo "NOT writing SHA256SUMS: a check failed"; fi
exit $fail
