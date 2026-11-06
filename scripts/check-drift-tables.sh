#!/bin/sh
# conformance/drift-tables.txt must be exactly what scripts/gen-drift-tables.sh extracts from the headers now, and
# the extracted error-code table must agree with the code-name table in src/core/errors.c (every code but
# ANVL_ERR_NONE has an entry there; and the name string is exactly the identifier). Usage: check-drift-tables.sh [write]
set -u
cd "$(dirname "$0")/.." || exit 2
tmp=$(mktemp) || exit 2
trap 'rm -f "$tmp"' EXIT INT TERM
sh scripts/gen-drift-tables.sh > "$tmp" || { echo "FAIL gen-drift-tables"; exit 1; }
# A code in the committed table that the headers no longer declare with the same number was changed or removed.
# The codes are stable public ABI: this is refused, also in write mode, and the table is never regenerated over it.
codes() { awk '/^table enum:anvl_err_code$/{f=1;next} /^table/{f=0} f' "$1" | LC_ALL=C sort; }
codes conformance/drift-tables.txt > "$tmp.old"
codes "$tmp" > "$tmp.new"
lost=$(LC_ALL=C comm -23 "$tmp.old" "$tmp.new")
rm -f "$tmp.old" "$tmp.new"
if [ -n "$lost" ]; then
   echo "FAIL a pinned error code changed or was removed: pinned codes are stable ABI and never change (test/unit/error_code_pins.h, include/anvil_error_codes.h). Do NOT regenerate conformance/drift-tables.txt; revert the header change. Changed or removed:"
   echo "$lost" | head
   exit 1
fi
if [ "${1:-}" = write ]; then cp "$tmp" conformance/drift-tables.txt; echo "drift-tables: wrote conformance/drift-tables.txt ($(wc -l < "$tmp") lines)"; fi
cmp -s "$tmp" conformance/drift-tables.txt || { echo "FAIL conformance/drift-tables.txt is stale (headers changed): run make update-conformance (a NEW error code must also be appended by hand to test/unit/error_code_pins.h)"; diff conformance/drift-tables.txt "$tmp" | head; exit 1; }
bad=0
for name in $(awk '/^table enum:anvl_err_code$/{f=1;next} /^table/{f=0} f && $1 != "ANVL_ERR_NONE" {print $1}' "$tmp"); do
   tr '\n' ' ' < src/core/errors.c | grep -q "\\[$name\\] *= *\"$name\"" || { echo "FAIL $name has no entry in the error name table (src/core/errors.c)"; bad=1; }
done
[ $bad -eq 0 ] || exit 1
echo "check-drift-tables OK: $(grep -c '^table ' "$tmp") tables, $(wc -l < "$tmp") lines"
