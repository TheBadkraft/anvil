#!/bin/sh
# The round-trip property of the builder corpus: every successful emit and write of every builder script in
# conformance/cases/builder must be a document the native reader loads with status ok. An `include "p";` in the output
# is satisfied by an empty file p. conformance/builder-roundtrip-known.txt lists known failures as
# "<script> <emit-number> <ticket>" lines. Usage: check-builder-roundtrip.sh <anvil-dump>
set -u
dump=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/conformance/cases/builder" || exit 2
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
known="$root/conformance/builder-roundtrip-known.txt"
n=0; bad=0; known_n=0; scripts=0
for s in $(find . -type f -name '*.bscript' | sed 's|^\./||' | LC_ALL=C sort); do
   rm -rf "$tmp/e"; mkdir "$tmp/e"
   "$dump" --builder-script "$s" --emit-dir "$tmp/e" > /dev/null 2> "$tmp/err"
   if [ $? -ne 0 ] || [ -s "$tmp/err" ]; then echo "FAIL $s: --builder-script failed: $(head -c 200 "$tmp/err")"; bad=$((bad + 1)); continue; fi
   scripts=$((scripts + 1))
   for f in "$tmp"/e/emit-*.anvl; do
      [ -f "$f" ] || continue
      k=$(basename "$f" .anvl); k=${k#emit-}
      n=$((n + 1))
      rm -rf "$tmp/w"; mkdir "$tmp/w"; cp "$f" "$tmp/w/doc.anvl"
      sed -n 's/^include "\(.*\)";$/\1/p' "$tmp/w/doc.anvl" | while IFS= read -r inc; do
         mkdir -p "$tmp/w/$(dirname "$inc")" && : > "$tmp/w/$inc"
      done
      (cd "$tmp/w" && "$dump" doc.anvl) > "$tmp/dump" 2>&1
      is_known=$(grep -v '^#' "$known" 2>/dev/null | awk -v c="$s" -v k="$k" '$1 == c && $2 == k { print $3 }')
      if sed -n '4p' "$tmp/dump" | grep -qx 'status ok'; then
         [ -n "$is_known" ] && { echo "FAIL $s emit $k: loads now; remove it from conformance/builder-roundtrip-known.txt ($is_known)"; bad=$((bad + 1)); }
      elif [ -n "$is_known" ]; then
         known_n=$((known_n + 1))
      else
         echo "FAIL $s emit $k: the native reader rejects the builder's output:"; grep '^error' "$tmp/dump" | head -2; bad=$((bad + 1))
      fi
   done
done
[ $n -gt 0 ] || { echo "FAIL no builder script produced an output"; bad=$((bad + 1)); }
if [ $bad -ne 0 ]; then echo "check-builder-roundtrip FAILED: $bad of $n outputs"; exit 1; fi
echo "check-builder-roundtrip OK: $n outputs of $scripts scripts: $((n - known_n)) load through the reader, $known_n known failure(s)"
