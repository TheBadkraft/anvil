#!/bin/sh
# check-exports.sh <libanvil.so> <include dir>
#
# Fails when a function (or vtable object) declared in a public header is missing from the shared
# object's dynamic symbol table, or when an internal symbol leaked into it. Exists because
# anvil_schema.h / anvil_type_registry.h once shipped without the visibility pragma and nothing
# noticed (BR-2610-anvl-visibility-001).
set -eu
so="$1"; inc="$2"
[ -f "$so" ] || { echo "FAIL: $so not found"; exit 1; }

exported=$(nm -D --defined-only "$so" | awk '{print $3}' | sort -u)

# A declaration starts in column 0: "<return type> anvil_name(" , possibly continued on later
# lines, or "extern const anvil_x_i Name;" for the vtable objects. Comments and the indented
# function-pointer members of the vtable structs never match.
# (anvil.h is not scanned: it only declares the legacy internal `Anvl` vtable, which must stay
# hidden; the public ABI lives in the anvil_*.h headers.)
declared=$(for h in "$inc"/anvil_*.h; do
   [ -f "$h" ] || continue
   grep -hoE '^[A-Za-z_][A-Za-z0-9_ ]*[ *]+anvil_[a-z0-9_]+\(' "$h" | grep -oE 'anvil_[a-z0-9_]+\($' | tr -d '('
   grep -hoE '^extern const [A-Za-z0-9_]+ [A-Za-z0-9_]+;' "$h" | awk '{gsub(";","",$4); print $4}'
done | sort -u)

missing=""
for sym in $declared; do
   echo "$exported" | grep -qx "$sym" || missing="$missing $sym"
done
if [ -n "$missing" ]; then
   echo "FAIL: declared in a public header but not exported from $so:"
   for sym in $missing; do echo "   $sym"; done
   exit 1
fi

# Internal symbols must stay hidden (the guarantee from -fvisibility=hidden).
leaked=$(echo "$exported" | grep -E '^(anvl_|mod_|doc_|List$|Map$|Math$|Time$|String$|Query$|Source$|Registry$|Collections$|Iterator$|Files$|FArray$|SparseIterator$|Allocator$|sigma)' || true)
if [ -n "$leaked" ]; then
   echo "FAIL: internal symbols exported from $so:"; echo "$leaked" | sed 's/^/   /'
   exit 1
fi
echo "exports OK: $(echo "$declared" | wc -w) public symbols exported, no internal symbols leaked"
