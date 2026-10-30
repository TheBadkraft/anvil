#!/bin/sh
# The writer corpus generator (conformance/seed/gen_writer.py) writes scripts only: the expected runs (.wscript.dump)
# are native's, produced by update-conformance. This checks that (1) the generator emits exactly the committed scripts
# and no dump, and (2) its optional model (--check-model) agrees with native on every case.
# Usage: check-writer-gen.sh <anvil-dump>
set -u
dump=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
bad=0
mkdir -p "$tmp/root/conformance/cases" && ln -s "$root/src" "$tmp/root/src"
python3 "$root/conformance/seed/gen_writer.py" "$tmp/root" > "$tmp/gen.out" 2>&1 || { echo "FAIL gen_writer.py: $(tail -2 "$tmp/gen.out")"; bad=$((bad + 1)); }
gen="$tmp/root/conformance/cases/writer"
if [ -n "$(find "$gen" -name '*.dump' 2>/dev/null | head -1)" ]; then echo "FAIL gen_writer.py wrote .dump files (native owns them)"; bad=$((bad + 1)); fi
(cd "$gen" 2>/dev/null && find . -name '*.wscript' | LC_ALL=C sort) > "$tmp/gen.list"
(cd "$root/conformance/cases/writer" && find . -name '*.wscript' | LC_ALL=C sort) > "$tmp/committed.list"
cmp -s "$tmp/gen.list" "$tmp/committed.list" || { echo "FAIL generated script set differs from the committed one"; diff "$tmp/committed.list" "$tmp/gen.list" | head -5; bad=$((bad + 1)); }
if ! diff -rq -x '*.dump' "$gen" "$root/conformance/cases/writer" > "$tmp/diff" 2>&1; then echo "FAIL generated scripts differ from the committed ones:"; head -5 "$tmp/diff"; bad=$((bad + 1)); fi
python3 "$root/conformance/seed/gen_writer.py" "$tmp/root" --force --check-model "$dump" > "$tmp/model.out" 2>&1 || { echo "FAIL model cross-check:"; tail -3 "$tmp/model.out"; bad=$((bad + 1)); }
if [ $bad -ne 0 ]; then echo "check-writer-gen FAILED: $bad problem(s)"; exit 1; fi
echo "check-writer-gen OK: $(wc -l < "$tmp/gen.list") scripts regenerate identically, no dumps written, the model agrees with native"
