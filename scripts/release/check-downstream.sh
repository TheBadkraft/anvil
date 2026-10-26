#!/usr/bin/env bash
# check-downstream.sh <node-tarball> <wasm-tarball> [flywire-checkout]
# FlyWire's own guard (tests/test-anvl-binding-grammar.js) against the staged tarballs. Read-only on
# the FlyWire checkout: the pieces the guard touches are copied to a scratch dir and the two
# bindings are installed there from the tarballs.
set -eu
node_tgz=$(readlink -f "${1:?node tarball}"); wasm_tgz=$(readlink -f "${2:?wasm tarball}")
fw=${3:-$HOME/Projects/repos/flywire}
[ -f "$fw/tests/test-anvl-binding-grammar.js" ] || { echo "FAIL: guard test not found in $fw" >&2; exit 1; }
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
mkdir -p "$work/tests" "$work/src"
cp "$fw/tests/test-anvl-binding-grammar.js" "$work/tests/"
cp -r "$fw/src/anvl" "$work/src/anvl"
for d in schemas schema-gen; do [ -d "$fw/$d" ] && cp -r "$fw/$d" "$work/$d"; done
cd "$work"; npm init -y >/dev/null 2>&1
npm install --no-audit --no-fund "$node_tgz" "$wasm_tgz" >npm.log 2>&1 || { cat npm.log; echo "FAIL npm install"; exit 1; }
node tests/test-anvl-binding-grammar.js
