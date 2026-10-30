#!/bin/sh
# conformance/drift-tables.txt must be exactly what scripts/gen-drift-tables.sh extracts from the headers now, and
# the extracted error-code table must agree with the code-name table in src/core/errors.c (every code but
# ANVL_ERR_NONE has an entry there; the name string itself may differ, e.g. 1003 is named ANVL_ERR_FILE_INVALID_PATH). Usage: check-drift-tables.sh [write]
set -u
cd "$(dirname "$0")/.." || exit 2
tmp=$(mktemp) || exit 2
trap 'rm -f "$tmp"' EXIT INT TERM
sh scripts/gen-drift-tables.sh > "$tmp" || { echo "FAIL gen-drift-tables"; exit 1; }
if [ "${1:-}" = write ]; then cp "$tmp" conformance/drift-tables.txt; echo "drift-tables: wrote conformance/drift-tables.txt ($(wc -l < "$tmp") lines)"; fi
cmp -s "$tmp" conformance/drift-tables.txt || { echo "FAIL conformance/drift-tables.txt is stale (headers changed): run make update-conformance"; diff conformance/drift-tables.txt "$tmp" | head; exit 1; }
bad=0
for name in $(awk '/^table enum:anvl_err_code$/{f=1;next} /^table/{f=0} f && $1 != "ANVL_ERR_NONE" {print $1}' "$tmp"); do
   tr '\n' ' ' < src/core/errors.c | grep -q "\\[$name\\] *= *\"[A-Z_]*\"" || { echo "FAIL $name has no entry in the error name table (src/core/errors.c)"; bad=1; }
done
[ $bad -eq 0 ] || exit 1
echo "check-drift-tables OK: $(grep -c '^table ' "$tmp") tables, $(wc -l < "$tmp") lines"
