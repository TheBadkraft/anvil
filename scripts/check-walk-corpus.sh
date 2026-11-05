#!/bin/sh
# Walks every reader input of the conformance corpus that loads clean through the public, transparent accessors, with a
# depth and a visit cap (tools/anvil-walk.c). A document that loads without errors must never send a walker round in
# circles; a reference that leads back to a container holding it is a load error (4054), so none gets here.
# anvil-dump does not follow `resolved`, which is why this check exists. Usage: check-walk-corpus.sh <anvil-walk>
# The inputs are the ones scripts/check-conformance.sh runs as documents (not .frag, .wscript, .bscript, .sscript, .dump),
# run from conformance/cases so includes resolve there.
set -u
walk=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/conformance/cases" || exit 2
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
find . -type f ! -name '*.dump' ! -name '*.frag' ! -name '*.wscript' ! -name '*.bscript' ! -name '*.sscript' | sed 's|^\./||' | LC_ALL=C sort > "$tmp/inputs"
"$walk" < "$tmp/inputs" > "$tmp/out" 2>&1
rc=$?
cat "$tmp/out"
if [ $rc -ne 0 ]; then echo "check-walk-corpus FAILED"; exit 1; fi
echo "check-walk-corpus OK"
