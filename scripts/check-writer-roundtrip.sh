#!/bin/sh
# The round-trip property of the writer corpus: every writer script in conformance/cases/writer that finishes with
# output must produce a document the native reader loads with status ok. An `include "p";` in the output is satisfied
# by an empty file p. conformance/writer-roundtrip-known.txt lists known failures (each tied to a ticket). Usage: check-writer-roundtrip.sh <anvil-dump>
set -u
dump=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/conformance/cases/writer" || exit 2
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
known="$root/conformance/writer-roundtrip-known.txt"
n=0; bad=0; skipped=0; known_n=0
for s in $(find . -type f -name '*.wscript' | sed 's|^\./||' | LC_ALL=C sort); do
   if ! grep -q '^output bytes=' "$s.dump"; then skipped=$((skipped + 1)); continue; fi
   n=$((n + 1))
   rm -rf "$tmp/w"; mkdir "$tmp/w"
   "$dump" --writer-output "$s" > "$tmp/w/doc.anvl" 2> "$tmp/err"
   if [ $? -ne 0 ] || [ -s "$tmp/err" ]; then echo "FAIL $s: --writer-output failed: $(head -c 200 "$tmp/err")"; bad=$((bad + 1)); continue; fi
   # satisfy includes
   sed -n 's/^include "\(.*\)";$/\1/p' "$tmp/w/doc.anvl" | while IFS= read -r inc; do
      mkdir -p "$tmp/w/$(dirname "$inc")" && : > "$tmp/w/$inc"
   done
   (cd "$tmp/w" && "$dump" doc.anvl) > "$tmp/dump" 2>&1
   is_known=$(grep -v '^#' "$known" | awk -v c="$s" '$1 == c { print $2 }')
   if sed -n '4p' "$tmp/dump" | grep -qx 'status ok'; then
      [ -n "$is_known" ] && { echo "FAIL $s: loads now; remove it from conformance/writer-roundtrip-known.txt ($is_known)"; bad=$((bad + 1)); }
   elif [ -n "$is_known" ]; then
      known_n=$((known_n + 1))
   else
      echo "FAIL $s: the native reader rejects the writer's output:"; grep '^error' "$tmp/dump" | head -2; bad=$((bad + 1))
   fi
done
if [ $bad -ne 0 ]; then echo "check-writer-roundtrip FAILED: $bad of $n outputs"; exit 1; fi
echo "check-writer-roundtrip OK: $n outputs checked: $((n - known_n)) load through the reader, $known_n known failure(s) ($skipped scripts end in an error or without output)"
