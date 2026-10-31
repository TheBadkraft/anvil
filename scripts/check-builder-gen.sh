#!/bin/sh
# The builder corpus generator (conformance/seed/gen_builder.py) writes scripts only: the expected runs
# (.bscript.dump) are native's, produced by update-conformance. This checks that the generator emits exactly the
# committed scripts and no dump. Usage: check-builder-gen.sh
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
bad=0
mkdir -p "$tmp/root/conformance/cases"
python3 "$root/conformance/seed/gen_builder.py" "$tmp/root" > "$tmp/gen.out" 2>&1 || { echo "FAIL gen_builder.py: $(tail -2 "$tmp/gen.out")"; bad=$((bad + 1)); }
gen="$tmp/root/conformance/cases/builder"
if [ -n "$(find "$gen" -name '*.dump' 2>/dev/null | head -1)" ]; then echo "FAIL gen_builder.py wrote .dump files (native owns them)"; bad=$((bad + 1)); fi
(cd "$gen" 2>/dev/null && find . -name '*.bscript' | LC_ALL=C sort) > "$tmp/gen.list"
(cd "$root/conformance/cases/builder" && find . -name '*.bscript' | LC_ALL=C sort) > "$tmp/committed.list"
cmp -s "$tmp/gen.list" "$tmp/committed.list" || { echo "FAIL generated script set differs from the committed one"; diff "$tmp/committed.list" "$tmp/gen.list" | head -5; bad=$((bad + 1)); }
if ! diff -rq -x '*.dump' "$gen" "$root/conformance/cases/builder" > "$tmp/diff" 2>&1; then echo "FAIL generated scripts differ from the committed ones:"; head -5 "$tmp/diff"; bad=$((bad + 1)); fi
if [ $bad -ne 0 ]; then echo "check-builder-gen FAILED: $bad problem(s)"; exit 1; fi
echo "check-builder-gen OK: $(wc -l < "$tmp/gen.list") scripts regenerate identically, no dumps written"
