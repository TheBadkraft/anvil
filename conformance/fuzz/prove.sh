#!/bin/sh
# Proves the parity tooling can fail. In a scratch copy (never committed) one number rule of the parser is
# altered: the exponent marker `E` stops being part of a number. Then:
#   1. make check-conformance on the altered build must be RED;
#   2. the differential fuzzer, native against itself, must find 0 differences;
#   3. the fuzzer, native against the altered build, must find a difference and minimise it.
# Usage: conformance/fuzz/prove.sh [cases]   (run from the repo root; builds in a mktemp directory, removed on exit)
set -u
cases=${1:-20000}
root=$(cd "$(dirname "$0")/../.." && pwd)
scratch=$(mktemp -d) || exit 2
trap 'rm -rf "$scratch"' EXIT INT TERM
cd "$root" || exit 2
make -s anvil-dump >/dev/null || exit 2
tar -cf - include src tools Makefile libanvil.exports scripts conformance test/fixtures | tar -xf - -C "$scratch"
cd "$scratch" || exit 2
before=$(grep -c "Source.peek(src) == 'e' || Source.peek(src) == 'E'" src/core/parser.c)
sed -i "s/Source.peek(src) == 'e' || Source.peek(src) == 'E'/Source.peek(src) == 'e'/" src/core/parser.c
after=$(grep -c "Source.peek(src) == 'e' || Source.peek(src) == 'E'" src/core/parser.c)
[ "$before" = 1 ] && [ "$after" = 0 ] || { echo "prove: the number-rule patch did not apply"; exit 2; }
make -s anvil-dump >/dev/null 2>&1 || { echo "prove: the altered build failed to compile"; exit 2; }
fail=0

echo "== 1. check-conformance against the altered build (must be RED)"
if make -s check-conformance ANVIL_DUMP="$scratch/build/tools/anvil-dump" > "$scratch/cc.log" 2>&1; then
   echo "FAIL: check-conformance stayed green on an altered build"; fail=1
else
   grep -c '^DIFF' "$scratch/cc.log" | sed 's/^/RED as required: cases differing: /'; tail -1 "$scratch/cc.log"
fi

echo "== 2. fuzz, native against itself (must be 0 differences)"
python3 "$root/conformance/fuzz/fuzz.py" --a "$root/build/tools/anvil-dump" --b "$root/build/tools/anvil-dump" --cases "$cases" --corpus "$root/conformance/cases" | tail -1
python3 "$root/conformance/fuzz/fuzz.py" --a "$root/build/tools/anvil-dump" --b "$root/build/tools/anvil-dump" --cases "$cases" --corpus "$root/conformance/cases" > /dev/null || { echo "FAIL: native differs from itself"; fail=1; }

echo "== 3. fuzz, native against the altered build (must find a difference)"
python3 "$root/conformance/fuzz/fuzz.py" --a "$root/build/tools/anvil-dump" --b "$scratch/build/tools/anvil-dump" --cases "$cases" --corpus "$root/conformance/cases" --max-diffs 3
[ $? -eq 1 ] || { echo "FAIL: the fuzzer did not find the altered rule"; fail=1; }

[ $fail -eq 0 ] && echo "prove: OK" || echo "prove: FAILED"
exit $fail
