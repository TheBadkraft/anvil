#!/usr/bin/env bash
# Download the published tarballs and run the grammar+version checks on each.
# This is the genuine RED for BR-2610-anvl-distribution-001: against the stale 0.8.0-rc files it must fail.
#   audit-published.sh <version> [base-url]    e.g. audit-published.sh 0.8.0-rc
set -u
ver=${1:?version}; base=${2:-https://anvldata.com/assets/downloads}
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
declare -A name=( [core]="anvil-v$ver-linux-x86_64" [node]="anvil-node-v$ver-linux-x86_64"
                  [wasm]="anvil-wasm-v$ver" [net]="anvil-net-v$ver-linux-x64" [py]="anvil-py-v$ver-linux-x64" )
fail=0
for kind in core node wasm net py; do
  f=${name[$kind]}.tar.gz
  echo "== $kind ($f)"
  curl -fsS -o "$work/$f" "$base/$f" || { echo "FAIL download"; fail=1; continue; }
  echo "sha256 $(sha256sum "$work/$f" | cut -d' ' -f1)"
  mkdir -p "$work/$kind" && tar -xzf "$work/$f" -C "$work/$kind"
  root=$(find "$work/$kind" -mindepth 1 -maxdepth 1 -type d | head -1)
  LEGACY_VERSION_FORMAT=1 "$here/check-grammar.sh" "$kind" "$root" "$ver" || fail=1
done
exit $fail
